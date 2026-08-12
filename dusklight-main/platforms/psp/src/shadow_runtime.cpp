#include "dusk/psp/shadow_runtime.hpp"

#include <cmath>
#include <cstring>

namespace dusk::psp::shadow {
namespace {

bool finite(const room::Vec3& value) {
    return std::isfinite(value.x) &&
           std::isfinite(value.y) &&
           std::isfinite(value.z);
}

bool valid(const PspSimpleShadowRequest& request) {
    return finite(request.position) &&
           finite(request.floor_normal) &&
           finite(request.environment_direction) &&
           std::isfinite(request.radius) && request.radius > 0.0f &&
           std::isfinite(request.height) && request.height >= 0.0f &&
           std::isfinite(request.alpha) && request.alpha >= 0.0f &&
           request.alpha <= 1.0f &&
           std::isfinite(request.environment_density) &&
           request.environment_density >= 0.0f &&
           request.environment_density <= 1.0f &&
           std::isfinite(request.fade) && request.fade >= 0.0f &&
           request.fade <= 1.0f;
}

}  // namespace

void PspShadowSystem::initialize() {
    std::memset(this, 0, sizeof(*this));
}

void PspShadowSystem::begin_frame(std::uint32_t room_generation) {
    generation = room_generation;
    simple_count = 0;
    projected_count = 0;
    receivers.triangle_count = 0;
    receivers.generation = room_generation;
    receivers.overflow = false;
    frame_active = room_generation != 0;
}

bool PspShadowSystem::submit_simple(
    const PspSimpleShadowRequest& request) {
    if (!frame_active || request.generation != generation) {
        ++metrics.stale_handles;
        return false;
    }
    if (!valid(request)) {
        ++metrics.non_finite_values;
        return false;
    }
    if (simple_count >= kSimpleCapacity) {
        return false;
    }
    simple[simple_count++] = request;
    ++metrics.simple_requests;
    return true;
}

bool PspShadowSystem::submit_projected(
    const PspProjectedShadowRequest& request) {
    if (!frame_active || request.generation != generation) {
        ++metrics.stale_handles;
        return false;
    }
    if (!finite(request.position) || !finite(request.direction) ||
        !std::isfinite(request.radius) || request.radius <= 0.0f ||
        !std::isfinite(request.height) || request.height <= 0.0f ||
        !std::isfinite(request.density) || request.density < 0.0f ||
        request.density > 1.0f) {
        ++metrics.non_finite_values;
        return false;
    }
    if (projected_count >= kProjectedCapacity) {
        return false;
    }
    projected[projected_count++] = request;
    ++metrics.projected_requests;
    return true;
}

bool PspShadowSystem::configure_projected_map(
    std::uint16_t width,
    std::uint16_t height,
    std::uint32_t format,
    std::uint32_t map_bytes,
    std::uint32_t auxiliary_bytes,
    std::uint32_t content_signature) {
    if (!frame_active ||
        (width != 64 && width != 96 && width != 128) ||
        width != height || map_bytes > 65536 ||
        auxiliary_bytes > 32768 ||
        map_bytes + auxiliary_bytes > 98304) {
        map.valid = false;
        return false;
    }
    map.width = width;
    map.height = height;
    map.format = format;
    map.edram_bytes = map_bytes;
    map.auxiliary_edram_bytes = auxiliary_bytes;
    const bool reusable =
        map.valid && map.generation == generation &&
        map.content_signature == content_signature;
    map.update_required = !reusable;
    if (reusable) {
        ++map.reuses;
    } else {
        ++map.updates;
    }
    map.generation = generation;
    map.content_signature = content_signature;
    map.valid = true;
    return true;
}

bool PspShadowSystem::prepare_simple(
    const room::CollisionWorld& collision) {
    if (!frame_active || receivers.generation != generation) {
        ++metrics.stale_handles;
        return false;
    }
    receivers.triangle_count = 0;
    receivers.overflow = false;
    for (std::uint32_t request = 0; request < simple_count; ++request) {
        const std::uint32_t remaining =
            PspShadowReceiverMesh::kCapacity - receivers.triangle_count;
        if (remaining == 0) {
            receivers.overflow = true;
            break;
        }
        bool overflow = false;
        const PspSimpleShadowRequest& current = simple[request];
        const std::uint32_t selected = room::collect_shadow_receivers(
            collision, current.position, current.radius,
            current.height + current.radius,
            current.environment_direction,
            receivers.triangles + receivers.triangle_count,
            remaining, &overflow);
        for (std::uint32_t index = 0; index < selected; ++index) {
            receivers.request_indices[
                receivers.triangle_count + index] =
                    static_cast<std::uint16_t>(request);
        }
        receivers.triangle_count += selected;
        receivers.overflow = receivers.overflow || overflow;
    }
    metrics.receiver_triangles += receivers.triangle_count;
    metrics.receiver_overflows += receivers.overflow ? 1u : 0u;
    return receivers.triangle_count != 0 && !receivers.overflow;
}

bool PspShadowSystem::prepare_movebg(
    const movebg::PspMoveBgWorld& world) {
    if (!frame_active || receivers.generation != generation) {
        ++metrics.stale_handles;
        return false;
    }
    for (std::uint32_t request = 0; request < simple_count; ++request) {
        const std::uint32_t remaining =
            PspShadowReceiverMesh::kCapacity - receivers.triangle_count;
        if (remaining == 0) {
            receivers.overflow = true;
            return false;
        }
        bool overflow = false;
        const PspSimpleShadowRequest& current = simple[request];
        const std::uint32_t selected =
            world.collect_shadow_receivers(
                current.position,
                current.radius,
                current.height + current.radius,
                current.environment_direction,
                receivers.triangles + receivers.triangle_count,
                remaining,
                &overflow);
        for (std::uint32_t index = 0; index < selected; ++index) {
            receivers.request_indices[
                receivers.triangle_count + index] =
                    static_cast<std::uint16_t>(request);
        }
        receivers.triangle_count += selected;
        metrics.movebg_receiver_triangles += selected;
        receivers.overflow = receivers.overflow || overflow;
    }
    return !receivers.overflow;
}

void PspShadowSystem::unload(std::uint32_t room_generation) {
    if (frame_active && generation != room_generation) {
        ++metrics.stale_handles;
        return;
    }
    generation = 0;
    simple_count = 0;
    projected_count = 0;
    receivers.triangle_count = 0;
    receivers.generation = 0;
    receivers.overflow = false;
    map.valid = false;
    map.update_required = false;
    frame_active = false;
}

bool PspShadowSystem::consistent(
    std::uint32_t room_generation) const {
    return frame_active && generation == room_generation &&
           receivers.generation == room_generation &&
           simple_count <= kSimpleCapacity &&
           projected_count <= kProjectedCapacity &&
           (!map.valid ||
            (map.edram_bytes <= 65536 &&
             map.auxiliary_edram_bytes <= 32768 &&
             map.edram_bytes + map.auxiliary_edram_bytes <= 98304)) &&
           receivers.triangle_count <= PspShadowReceiverMesh::kCapacity &&
           !receivers.overflow &&
           metrics.allocations_during_playing == 0 &&
           metrics.non_finite_values == 0;
}

}  // namespace dusk::psp::shadow
