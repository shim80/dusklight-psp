#include "dusk/psp/movebg_runtime.hpp"

#include <algorithm>
#include <cmath>
#include <cstring>

namespace dusk::psp::movebg {
namespace {

bool finite_matrix(const Matrix34& matrix) {
    for (const auto& row : matrix.value) {
        for (float value : row) {
            if (!std::isfinite(value)) {
                return false;
            }
        }
    }
    return true;
}

bool invert(const Matrix34& source, Matrix34* output) {
    const float a = source.value[0][0];
    const float b = source.value[0][1];
    const float c = source.value[0][2];
    const float d = source.value[1][0];
    const float e = source.value[1][1];
    const float f = source.value[1][2];
    const float g = source.value[2][0];
    const float h = source.value[2][1];
    const float i = source.value[2][2];
    const float determinant =
        a * (e * i - f * h) -
        b * (d * i - f * g) +
        c * (d * h - e * g);
    if (!std::isfinite(determinant) ||
        std::fabs(determinant) < 0.000001f) {
        return false;
    }
    const float inverse = 1.0f / determinant;
    output->value[0][0] = (e * i - f * h) * inverse;
    output->value[0][1] = (c * h - b * i) * inverse;
    output->value[0][2] = (b * f - c * e) * inverse;
    output->value[1][0] = (f * g - d * i) * inverse;
    output->value[1][1] = (a * i - c * g) * inverse;
    output->value[1][2] = (c * d - a * f) * inverse;
    output->value[2][0] = (d * h - e * g) * inverse;
    output->value[2][1] = (b * g - a * h) * inverse;
    output->value[2][2] = (a * e - b * d) * inverse;
    for (std::uint32_t row = 0; row < 3; ++row) {
        output->value[row][3] = -(
            output->value[row][0] * source.value[0][3] +
            output->value[row][1] * source.value[1][3] +
            output->value[row][2] * source.value[2][3]);
    }
    return true;
}

room::Vec3 transform_point(
    const Matrix34& matrix, const room::Vec3& point) {
    return {
        matrix.value[0][0] * point.x +
            matrix.value[0][1] * point.y +
            matrix.value[0][2] * point.z +
            matrix.value[0][3],
        matrix.value[1][0] * point.x +
            matrix.value[1][1] * point.y +
            matrix.value[1][2] * point.z +
            matrix.value[1][3],
        matrix.value[2][0] * point.x +
            matrix.value[2][1] * point.y +
            matrix.value[2][2] * point.z +
            matrix.value[2][3],
    };
}

room::Vec3 transform_vector(
    const Matrix34& matrix, const room::Vec3& vector) {
    room::Vec3 result = {
        matrix.value[0][0] * vector.x +
            matrix.value[0][1] * vector.y +
            matrix.value[0][2] * vector.z,
        matrix.value[1][0] * vector.x +
            matrix.value[1][1] * vector.y +
            matrix.value[1][2] * vector.z,
        matrix.value[2][0] * vector.x +
            matrix.value[2][1] * vector.y +
            matrix.value[2][2] * vector.z,
    };
    const float length = std::sqrt(
        result.x * result.x + result.y * result.y +
        result.z * result.z);
    if (length > 0.000001f) {
        result.x /= length;
        result.y /= length;
        result.z /= length;
    }
    return result;
}

}  // namespace

Matrix34 identity_matrix() {
    return {{{1.0f, 0.0f, 0.0f, 0.0f},
             {0.0f, 1.0f, 0.0f, 0.0f},
             {0.0f, 0.0f, 1.0f, 0.0f}}};
}

bool source_compatibility_surface_valid() {
    return PspMoveBgWorld::kCapacity == 16 &&
           sizeof(Matrix34) == sizeof(float) * 12;
}

void PspMoveBgWorld::initialize() {
    std::memset(slots_, 0, sizeof(slots_));
    metrics = {};
    initialized_ = true;
}

void PspMoveBgWorld::shutdown() {
    std::memset(slots_, 0, sizeof(slots_));
    initialized_ = false;
}

bool PspMoveBgWorld::create(
    const void* dpcl, std::uint32_t size,
    const Matrix34& matrix_value, Handle* handle) {
    Matrix34 inverse = {};
    if (!initialized_ || handle == nullptr ||
        !finite_matrix(matrix_value) ||
        !invert(matrix_value, &inverse)) {
        ++metrics.invalid_matrices;
        return false;
    }
    for (std::uint16_t index = 0; index < kCapacity; ++index) {
        Slot& slot = slots_[index];
        if (!slot.active &&
            room::initialize_collision_world(
                &slot.collision, dpcl, size)) {
            ++slot.generation;
            if (slot.generation == 0) {
                ++slot.generation;
            }
            slot.previous = matrix_value;
            slot.current = matrix_value;
            slot.inverse = inverse;
            slot.active = true;
            *handle = {index, slot.generation};
            ++metrics.creates;
            std::uint32_t active = 0;
            for (const auto& candidate : slots_) {
                active += candidate.active ? 1u : 0u;
            }
            metrics.peak_handles = std::max(
                metrics.peak_handles, active);
            ++metrics.commits;
            return true;
        }
    }
    return false;
}

bool PspMoveBgWorld::update(
    Handle handle, const Matrix34& matrix_value) {
    Slot* slot = resolve(handle);
    Matrix34 inverse = {};
    if (slot == nullptr) {
        ++metrics.calls_after_delete;
        return false;
    }
    if (!finite_matrix(matrix_value) ||
        !invert(matrix_value, &inverse)) {
        ++metrics.invalid_matrices;
        return false;
    }
    slot->previous = slot->current;
    slot->current = matrix_value;
    slot->inverse = inverse;
    ++metrics.updates;
    ++metrics.commits;
    return true;
}

bool PspMoveBgWorld::destroy(Handle handle) {
    Slot* slot = resolve(handle);
    if (slot == nullptr) {
        ++metrics.calls_after_delete;
        return false;
    }
    slot->active = false;
    ++metrics.deletes;
    return true;
}

PspMoveBgWorld::Slot* PspMoveBgWorld::resolve(Handle handle) {
    if (!initialized_ || handle.slot >= kCapacity) {
        ++metrics.stale_handles;
        return nullptr;
    }
    Slot& slot = slots_[handle.slot];
    if (!slot.active || slot.generation != handle.generation) {
        ++metrics.stale_handles;
        return nullptr;
    }
    return &slot;
}

const PspMoveBgWorld::Slot* PspMoveBgWorld::resolve(
    Handle handle) const {
    if (!initialized_ || handle.slot >= kCapacity) {
        return nullptr;
    }
    const Slot& slot = slots_[handle.slot];
    return slot.active && slot.generation == handle.generation
        ? &slot : nullptr;
}

bool PspMoveBgWorld::valid(Handle handle) const {
    return resolve(handle) != nullptr;
}

room::FloorHit PspMoveBgWorld::find_floor(
    Handle handle, const room::Vec3& world_position,
    float maximum_above, float slope_limit_cosine) {
    Slot* slot = resolve(handle);
    if (slot == nullptr) {
        return {};
    }
    const room::Vec3 local =
        transform_point(slot->inverse, world_position);
    room::FloorHit hit = room::find_floor(
        &slot->collision, local, maximum_above,
        slope_limit_cosine);
    if (hit.hit) {
        const room::Vec3 world_hit = transform_point(
            slot->current, {local.x, hit.height, local.z});
        hit.height = world_hit.y;
        hit.normal = transform_vector(slot->current, hit.normal);
    }
    ++metrics.floor_queries;
    return hit;
}

bool PspMoveBgWorld::resolve_horizontal(
    Handle handle, const room::Vec3& current,
    const room::Vec3& desired, float radius, float height,
    room::Vec3* resolved) {
    Slot* slot = resolve(handle);
    if (slot == nullptr || resolved == nullptr) {
        return false;
    }
    const room::Vec3 local_current =
        transform_point(slot->inverse, current);
    const room::Vec3 local_desired =
        transform_point(slot->inverse, desired);
    room::Vec3 local_resolved = {};
    const bool hit = room::resolve_horizontal(
        &slot->collision, local_current, local_desired,
        radius, height, &local_resolved);
    *resolved = transform_point(slot->current, local_resolved);
    ++metrics.wall_queries;
    return hit;
}

room::LineHit PspMoveBgWorld::trace_camera(
    Handle handle, const room::Vec3& start,
    const room::Vec3& end) {
    Slot* slot = resolve(handle);
    if (slot == nullptr) {
        return {};
    }
    room::LineHit hit = room::trace_camera(
        &slot->collision,
        transform_point(slot->inverse, start),
        transform_point(slot->inverse, end));
    if (hit.hit) {
        hit.position = transform_point(slot->current, hit.position);
        hit.normal = transform_vector(slot->current, hit.normal);
    }
    ++metrics.camera_queries;
    return hit;
}

bool PspMoveBgWorld::carry_point(
    Handle handle, const room::Vec3& previous_world,
    room::Vec3* current_world) {
    Slot* slot = resolve(handle);
    Matrix34 previous_inverse = {};
    if (slot == nullptr || current_world == nullptr ||
        !invert(slot->previous, &previous_inverse)) {
        return false;
    }
    *current_world = transform_point(
        slot->current,
        transform_point(previous_inverse, previous_world));
    ++metrics.carry_updates;
    return true;
}

std::uint32_t PspMoveBgWorld::collect_shadow_receivers(
    const room::Vec3& world_center,
    float radius,
    float maximum_drop,
    const room::Vec3& world_projection_direction,
    room::ShadowReceiverTriangle* output,
    std::uint32_t capacity,
    bool* overflow) const {
    if (overflow != nullptr) {
        *overflow = false;
    }
    if (!initialized_ || output == nullptr || capacity == 0) {
        return 0;
    }
    std::uint32_t count = 0;
    for (const Slot& slot : slots_) {
        if (!slot.active || count >= capacity) {
            continue;
        }
        bool slot_overflow = false;
        const room::Vec3 local_center =
            transform_point(slot.inverse, world_center);
        const room::Vec3 local_direction =
            transform_vector(
                slot.inverse, world_projection_direction);
        const std::uint32_t selected =
            room::collect_shadow_receivers(
                slot.collision,
                local_center,
                radius,
                maximum_drop,
                local_direction,
                output + count,
                capacity - count,
                &slot_overflow);
        for (std::uint32_t index = 0; index < selected; ++index) {
            room::ShadowReceiverTriangle& receiver =
                output[count + index];
            for (room::Vec3& vertex : receiver.vertices) {
                vertex = transform_point(slot.current, vertex);
            }
            receiver.normal =
                transform_vector(slot.current, receiver.normal);
        }
        count += selected;
        if (overflow != nullptr && slot_overflow) {
            *overflow = true;
        }
    }
    return count;
}

const Matrix34* PspMoveBgWorld::matrix(Handle handle) const {
    const Slot* slot = resolve(handle);
    return slot != nullptr ? &slot->current : nullptr;
}

}  // namespace dusk::psp::movebg
