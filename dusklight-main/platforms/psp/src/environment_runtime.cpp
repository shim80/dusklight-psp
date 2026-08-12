#include "dusk/psp/environment_runtime.hpp"

#include <algorithm>
#include <cmath>
#include <cstring>

namespace dusk::psp::environment {
namespace {

std::uint8_t channel(std::uint32_t color, std::uint32_t shift) {
    return static_cast<std::uint8_t>((color >> shift) & 0xffu);
}

std::uint32_t blend_color(
    std::uint32_t from, std::uint32_t to, float amount) {
    std::uint32_t result = 0;
    for (std::uint32_t shift = 0; shift < 32; shift += 8) {
        const float value =
            static_cast<float>(channel(from, shift)) +
            (static_cast<float>(channel(to, shift)) -
             static_cast<float>(channel(from, shift))) * amount;
        result |= static_cast<std::uint32_t>(
            std::clamp(value, 0.0f, 255.0f)) << shift;
    }
    return result;
}

float blend(float from, float to, float amount) {
    return from + (to - from) * amount;
}

bool finite_record(const room::EnvironmentRecordV4& record) {
    for (std::uint32_t component = 0; component < 3; ++component) {
        if (!std::isfinite(record.key_light_direction[component]) ||
            !std::isfinite(record.local_light_position[component]) ||
            !std::isfinite(record.shadow_direction[component])) {
            return false;
        }
    }
    return std::isfinite(record.local_light_power) &&
           std::isfinite(record.fog_near) &&
           std::isfinite(record.fog_far) &&
           std::isfinite(record.shadow_density) &&
           std::isfinite(record.transition_rate);
}

void material_from_record(
    const room::EnvironmentRecordV4& record,
    PspMaterialEnvironmentState* material) {
    material->ambient = record.ambient_actor;
    material->diffuse = record.key_light_color;
    material->key_light_color = record.key_light_color;
    material->local_light_color = record.local_light_color;
    material->local_light_power = record.local_light_power;
    material->local_light_enabled =
        record.local_light_count != 0 && record.local_light_power > 0.0f;
    material->fog_enabled =
        record.fog_far > record.fog_near && (record.flags & 2u) != 0;
    material->fog_near = record.fog_near;
    material->fog_far = record.fog_far;
    material->fog_color = record.fog_color;
    material->clear_color = record.clear_color;
    material->shadow_factor = record.shadow_density;
    material->emissive = 0xff000000u;
    material->alpha = 255;
    for (std::uint32_t component = 0; component < 3; ++component) {
        material->key_light_direction[component] =
            record.key_light_direction[component];
        material->local_light_position[component] =
            record.local_light_position[component];
        material->shadow_direction[component] =
            record.shadow_direction[component];
    }
}

}  // namespace

void PspEnvironmentRuntime::initialize() {
    std::memset(this, 0, sizeof(*this));
    material.alpha = 255;
}

Error PspEnvironmentRuntime::load(
    const room::PackageView& scene,
    std::uint32_t expected_stage_hash,
    std::uint32_t expected_room,
    std::uint32_t room_generation) {
    room::EnvironmentRecordV4 record = {};
    if (room::read_dpsc_environment_v4(scene, &record) !=
        room::PackageError::Ok) {
        return Error::InvalidPackage;
    }
    if (record.stage_hash != expected_stage_hash) {
        return Error::StageMismatch;
    }
    if (record.room_index != expected_room) {
        return Error::RoomMismatch;
    }
    if (!finite_record(record)) {
        ++metrics.non_finite_values;
        return Error::NonFinite;
    }
    target = record;
    generation = room_generation;
    loaded = true;
    ++metrics.records_loaded;
    metrics.source_derived = true;
    return Error::Ok;
}

Error PspEnvironmentRuntime::activate(std::uint32_t room_generation) {
    if (!loaded || generation != room_generation) {
        ++metrics.stale_generations_rejected;
        return Error::Generation;
    }
    if (!active_valid || target.transition_rate <= 0.0f) {
        active = target;
        material_from_record(active, &material);
        active_valid = true;
        transitioning = false;
        transition = 1.0f;
        transition_duration = 0.0f;
        metrics.fog_enabled = material.fog_enabled;
        metrics.local_lights_used =
            material.local_light_enabled ? 1u : 0u;
        return Error::Ok;
    }
    transition = 0.0f;
    transition_duration =
        std::clamp(target.transition_rate, 1.0f / 60.0f, 4.0f);
    transitioning = true;
    ++metrics.transitions_started;
    return Error::Ok;
}

Error PspEnvironmentRuntime::update(
    float delta_seconds, std::uint32_t room_generation) {
    if (!active_valid || generation != room_generation) {
        ++metrics.stale_generations_rejected;
        return Error::Generation;
    }
    if (!std::isfinite(delta_seconds) || delta_seconds < 0.0f) {
        ++metrics.non_finite_values;
        return Error::NonFinite;
    }
    if (!transitioning) {
        return Error::Ok;
    }
    transition = std::clamp(
        transition + delta_seconds / transition_duration, 0.0f, 1.0f);
    material.ambient =
        blend_color(active.ambient_actor, target.ambient_actor, transition);
    material.diffuse =
        blend_color(active.key_light_color, target.key_light_color, transition);
    material.key_light_color = material.diffuse;
    material.fog_color =
        blend_color(active.fog_color, target.fog_color, transition);
    material.clear_color =
        blend_color(active.clear_color, target.clear_color, transition);
    material.fog_near =
        blend(active.fog_near, target.fog_near, transition);
    material.fog_far =
        blend(active.fog_far, target.fog_far, transition);
    material.shadow_factor = blend(
        active.shadow_density, target.shadow_density, transition);
    for (std::uint32_t component = 0; component < 3; ++component) {
        material.key_light_direction[component] = blend(
            active.key_light_direction[component],
            target.key_light_direction[component], transition);
        material.shadow_direction[component] = blend(
            active.shadow_direction[component],
            target.shadow_direction[component], transition);
    }
    if (transition >= 1.0f) {
        active = target;
        material_from_record(active, &material);
        transitioning = false;
        ++metrics.transitions_completed;
    }
    metrics.fog_enabled = material.fog_enabled;
    metrics.local_lights_used =
        material.local_light_enabled ? 1u : 0u;
    return Error::Ok;
}

void PspEnvironmentRuntime::unload(std::uint32_t room_generation) {
    if (generation != room_generation) {
        ++metrics.stale_generations_rejected;
        return;
    }
    active = {};
    target = {};
    material = {};
    material.alpha = 255;
    generation = 0;
    transition = 0.0f;
    transition_duration = 0.0f;
    loaded = false;
    active_valid = false;
    transitioning = false;
    metrics.fog_enabled = false;
    metrics.local_lights_used = 0;
}

bool PspEnvironmentRuntime::consistent(
    std::uint32_t room_generation) const {
    return loaded && active_valid && generation == room_generation &&
           metrics.records_loaded != 0 &&
           metrics.source_derived &&
           metrics.allocations_during_playing == 0 &&
           metrics.non_finite_values == 0 &&
           finite_record(active) &&
           material.fog_far > material.fog_near &&
           material.shadow_factor >= 0.0f &&
           material.shadow_factor <= 1.0f;
}

}  // namespace dusk::psp::environment
