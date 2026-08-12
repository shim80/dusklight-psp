#include "dusk/psp/actor_runtime.hpp"

#include <algorithm>
#include <cmath>
#include <cstring>

namespace dusk::psp::actor {
namespace {

constexpr float kPi = 3.14159265358979323846f;

bool finite_vec(const room::Vec3& value) {
    return std::isfinite(value.x) &&
           std::isfinite(value.y) &&
           std::isfinite(value.z);
}

bool essential_source_process(std::uint16_t process_id) {
    switch (process_id) {
    case 0x02b8:  // fpcNm_KYTAG14_e / Savmem
    case 0x0053:  // fpcNm_Obj_Digpl_e / Digpl
    case 0x02cf:  // fpcNm_TAG_CAMERA_e / CamChg
    case 0x02d1:  // fpcNm_TAG_EVENT_e / TagEv
    case 0x0225:  // fpcNm_SWC00_e / SwAreaS, SwAreaC
    case 0x01aa:  // fpcNm_Tag_AttackItem_e / atkItem
        return true;
    default:
        return false;
    }
}

float chase(float value, float target, float step) {
    if (value < target) return std::min(value + step, target);
    if (value > target) return std::max(value - step, target);
    return value;
}

float random_unit(std::uint32_t* state) {
    *state = *state * 1664525u + 1013904223u;
    return static_cast<float>((*state >> 8) & 0xffffu) / 65535.0f;
}

room::Vec3 geyser_direction(const GeyserActor& actor) {
    if (actor.placement.type == 0) {
        return {0.0f, 1.0f, 0.0f};
    }
    const float yaw =
        static_cast<float>(actor.placement.rotation[1]) *
        (2.0f * kPi / 65536.0f);
    return {std::sin(yaw), 0.0f, std::cos(yaw)};
}

float point_segment_distance(
    const room::Vec3& point,
    const room::Vec3& start,
    const room::Vec3& direction,
    float length) {
    const room::Vec3 delta = {
        point.x - start.x, point.y - start.y, point.z - start.z};
    const float projected = std::clamp(
        delta.x * direction.x +
        delta.y * direction.y +
        delta.z * direction.z,
        -50.0f, length);
    const room::Vec3 nearest = {
        start.x + direction.x * projected,
        start.y + direction.y * projected,
        start.z + direction.z * projected};
    const float dx = point.x - nearest.x;
    const float dy = point.y - nearest.y;
    const float dz = point.z - nearest.z;
    return std::sqrt(dx * dx + dy * dy + dz * dz);
}

void transition(
    GeyserActor* actor, GeyserState state, std::uint16_t timer) {
    actor->state = state;
    actor->timer = timer;
    ++actor->transitions;
}

bool emit_particle(ActorSystem* system, std::uint32_t actor_index) {
    std::uint32_t owner_count = 0;
    for (const Particle& particle : system->particles) {
        owner_count += particle.active && particle.owner == actor_index;
    }
    if (owner_count >= kParticlesPerActor) {
        ++system->particle_overflow;
        return false;
    }
    for (Particle& particle : system->particles) {
        if (particle.active) continue;
        GeyserActor& actor = system->actors[actor_index];
        const room::Vec3 direction = geyser_direction(actor);
        const float side_x = random_unit(&actor.rng) * 2.0f - 1.0f;
        const float side_z = random_unit(&actor.rng) * 2.0f - 1.0f;
        particle.position = {
            actor.placement.position.x + side_x * 32.0f,
            actor.placement.position.y,
            actor.placement.position.z + side_z * 32.0f};
        particle.velocity = {
            direction.x * (70.0f + random_unit(&actor.rng) * 55.0f),
            direction.y * (70.0f + random_unit(&actor.rng) * 55.0f) + 8.0f,
            direction.z * (70.0f + random_unit(&actor.rng) * 55.0f)};
        particle.size = 12.0f + random_unit(&actor.rng) * 16.0f;
        particle.alpha = 1.0f;
        particle.life = 28 + static_cast<std::uint16_t>(
            random_unit(&actor.rng) * 20.0f);
        particle.owner = static_cast<std::uint8_t>(actor_index);
        particle.active = true;
        ++actor.effects_spawned;
        system->particle_peak = std::max(
            system->particle_peak, active_particle_count(*system));
        return true;
    }
    ++system->particle_overflow;
    return false;
}

void update_particles(ActorSystem* system) {
    for (Particle& particle : system->particles) {
        if (!particle.active) continue;
        if (particle.life == 0) {
            particle.active = false;
            continue;
        }
        particle.position.x += particle.velocity.x / 30.0f;
        particle.position.y += particle.velocity.y / 30.0f;
        particle.position.z += particle.velocity.z / 30.0f;
        particle.velocity.x *= 0.97f;
        particle.velocity.y *= 0.97f;
        particle.velocity.z *= 0.97f;
        particle.alpha = static_cast<float>(particle.life) / 48.0f;
        --particle.life;
    }
}

void update_reactive(
    GeyserActor* actor, const Context& context, float distance) {
    switch (actor->state) {
    case GeyserState::Off:
        actor->strength = chase(
            actor->strength, 0.0f, 0.1f * actor->placement.scale.y);
        if (distance < 1200.0f) {
            transition(actor, GeyserState::Warning, 0);
        }
        break;
    case GeyserState::Warning:
        actor->strength = chase(
            actor->strength, 0.0f, 0.1f * actor->placement.scale.y);
        if (distance < 600.0f) {
            transition(actor, GeyserState::On, 0);
        } else if (distance > 1200.0f) {
            transition(actor, GeyserState::Off, 0);
        }
        break;
    case GeyserState::On:
        actor->strength = chase(
            actor->strength, actor->placement.scale.y,
            0.05f * actor->placement.scale.y);
        ++actor->active_frames;
        if (distance > 1200.0f) {
            actor->disappear_target = 0;
            transition(actor, GeyserState::Disappear, 10);
        } else if (distance > 600.0f) {
            actor->disappear_target = 1;
            transition(actor, GeyserState::Disappear, 10);
        }
        break;
    case GeyserState::Disappear:
        if (actor->timer > 0) --actor->timer;
        actor->strength = chase(
            actor->strength, 0.0f, 0.1f * actor->placement.scale.y);
        if (actor->timer == 0) {
            transition(
                actor,
                actor->disappear_target == 1
                    ? GeyserState::Warning : GeyserState::Off,
                0);
        }
        break;
    }
    (void)context;
}

void update_periodic(GeyserActor* actor) {
    if (actor->timer > 0) --actor->timer;
    switch (actor->state) {
    case GeyserState::Off:
        actor->strength = chase(
            actor->strength, 0.0f, 0.1f * actor->placement.scale.y);
        if (actor->timer == 0) {
            transition(
                actor, GeyserState::Warning,
                actor->decoded.warning_updates);
        }
        break;
    case GeyserState::Warning:
        actor->strength = chase(
            actor->strength, 0.0f, 0.1f * actor->placement.scale.y);
        if (actor->timer == 0) {
            transition(actor, GeyserState::On, actor->decoded.on_updates);
        }
        break;
    case GeyserState::On:
        actor->strength = chase(
            actor->strength, actor->placement.scale.y,
            0.05f * actor->placement.scale.y);
        ++actor->active_frames;
        if (actor->timer == 0) {
            transition(actor, GeyserState::Off, actor->decoded.off_updates);
        }
        break;
    case GeyserState::Disappear:
        break;
    }
}

}  // namespace

GeyserParameters decode_geyser_parameters(std::uint32_t parameters) {
    GeyserParameters result = {};
    result.behavior = static_cast<std::uint8_t>(parameters);
    result.off_units = static_cast<std::uint8_t>(parameters >> 8);
    result.warning_units = static_cast<std::uint8_t>(parameters >> 16);
    result.on_units = static_cast<std::uint8_t>(parameters >> 24);
    result.reactive = result.behavior == 1;
    const auto updates = [](std::uint8_t value, std::uint8_t fallback) {
        return static_cast<std::uint16_t>(
            (value == 0xff ? fallback : value) * 15u);
    };
    result.off_updates = updates(result.off_units, 10);
    result.warning_updates = updates(result.warning_units, 4);
    result.on_updates = updates(result.on_units, 6);
    return result;
}

const char* geyser_state_name(GeyserState state) {
    static constexpr const char* names[] = {
        "off", "warning", "on", "disappear"};
    return names[static_cast<std::uint32_t>(state)];
}

Error initialize_actor_system(
    ActorSystem* system, const room::PackageView& scene) {
    if (system == nullptr || scene.bytes == nullptr) {
        return Error::MissingScene;
    }
    room::PackageView checked = {};
    if (room::validate_dpsc(scene.bytes, scene.size, &checked) !=
        room::PackageError::Ok) {
        return Error::SceneInvalid;
    }
    const std::uint16_t scene_version =
        room::read_u16(scene.bytes + 4);
    if (scene_version < 2 || scene_version > 4) {
        return Error::SceneVersion;
    }
    std::memset(system, 0, sizeof(*system));
    const std::uint32_t count = room::read_u32(scene.bytes + 136);
    const std::uint32_t offset = room::read_u32(scene.bytes + 140);
    const std::uint32_t stride = room::read_u32(scene.bytes + 168);
    bool source_indices[1024] = {};
    for (std::uint32_t index = 0; index < count; ++index) {
        const std::uint8_t* source = scene.bytes + offset + index * stride;
        const std::uint16_t source_index = room::read_u16(source + 42);
        if (source_index >= 1024 || source_indices[source_index]) {
            system->error = Error::DuplicateSourceIndex;
            return system->error;
        }
        source_indices[source_index] = true;
        const std::uint16_t process_id = room::read_u16(source + 12);
        if (source[41] != 0 && essential_source_process(process_id)) {
            if (system->essential_source_actor_count >=
                kEssentialSourceActorCapacity) {
                system->error = Error::Capacity;
                return system->error;
            }
            EssentialSourceActor& actor =
                system->essential_source_actors[
                    system->essential_source_actor_count++];
            std::memcpy(actor.placement.name, source, 8);
            actor.placement.name_hash = room::read_u32(source + 8);
            actor.placement.process_id = process_id;
            actor.placement.parameters = room::read_u32(source + 16);
            actor.placement.position = {
                room::read_f32(source + 20),
                room::read_f32(source + 24),
                room::read_f32(source + 28)};
            actor.placement.rotation[0] =
                static_cast<std::int16_t>(room::read_u16(source + 32));
            actor.placement.rotation[1] =
                static_cast<std::int16_t>(room::read_u16(source + 34));
            actor.placement.rotation[2] =
                static_cast<std::int16_t>(room::read_u16(source + 36));
            actor.placement.type = source[38];
            actor.placement.room_index = source[39];
            actor.placement.layer = source[40];
            actor.placement.flags = source[41];
            actor.placement.source_index = source_index;
            actor.placement.scale = {
                room::read_f32(source + 44),
                room::read_f32(source + 48),
                room::read_f32(source + 52)};
            actor.placement.source_chunk_hash =
                room::read_u32(source + 56);
            if (!finite_vec(actor.placement.position) ||
                !finite_vec(actor.placement.scale)) {
                system->error = Error::NonFiniteTransform;
                return system->error;
            }
            actor.alive = true;
            ++system->active_count;
            ++system->create_calls;
            continue;
        }
        if (std::memcmp(source, "geyser\0\0", 8) != 0) {
            ++system->unsupported_count;
            continue;
        }
        if (system->actor_count >= kActorCapacity) {
            system->error = Error::Capacity;
            return system->error;
        }
        GeyserActor& actor = system->actors[system->actor_count];
        std::memcpy(actor.placement.name, source, 8);
        actor.placement.name_hash = room::read_u32(source + 8);
        actor.placement.process_id = room::read_u16(source + 12);
        actor.placement.parameters = room::read_u32(source + 16);
        actor.placement.position = {
            room::read_f32(source + 20),
            room::read_f32(source + 24),
            room::read_f32(source + 28)};
        actor.placement.rotation[0] =
            static_cast<std::int16_t>(room::read_u16(source + 32));
        actor.placement.rotation[1] =
            static_cast<std::int16_t>(room::read_u16(source + 34));
        actor.placement.rotation[2] =
            static_cast<std::int16_t>(room::read_u16(source + 36));
        actor.placement.type = source[38];
        actor.placement.room_index = source[39];
        actor.placement.layer = source[40];
        actor.placement.flags = source[41];
        actor.placement.source_index = source_index;
        actor.placement.scale = {
            room::read_f32(source + 44),
            room::read_f32(source + 48),
            room::read_f32(source + 52)};
        actor.placement.source_chunk_hash = room::read_u32(source + 56);
        if (actor.placement.process_id != kGeyserProcessId) {
            system->error = Error::ProcessMismatch;
            return system->error;
        }
        actor.decoded =
            decode_geyser_parameters(actor.placement.parameters);
        if (actor.decoded.behavior != 0 &&
            actor.decoded.behavior != 1 &&
            actor.decoded.behavior != 0xff) {
            system->error = Error::InvalidParameters;
            return system->error;
        }
        if (!finite_vec(actor.placement.position) ||
            !finite_vec(actor.placement.scale)) {
            system->error = Error::NonFiniteTransform;
            return system->error;
        }
        actor.state = GeyserState::Off;
        actor.timer =
            actor.decoded.reactive ? 0 : actor.decoded.off_updates;
        actor.rng = 0x47595352u ^ source_index;
        actor.alive = true;
        ++system->actor_count;
        ++system->active_count;
        ++system->create_calls;
    }
    system->error =
        scene_version >= 3 ||
        (system->actor_count == 2 && system->unsupported_count == 1)
            ? Error::Ok : Error::CreateFailure;
    return system->error;
}

Error update_actor_system(
    ActorSystem* system, const Context& context, Interaction* interaction) {
    if (system == nullptr || interaction == nullptr ||
        !finite_vec(context.player_position)) {
        return Error::UpdateFailure;
    }
    std::memset(interaction, 0, sizeof(*interaction));
    if (context.paused) {
        return Error::Ok;
    }
    ++system->update_calls;
    update_particles(system);
    for (std::uint32_t index = 0;
         index < system->essential_source_actor_count; ++index) {
        if (system->essential_source_actors[index].alive) {
            ++system->essential_source_actors[index].updates;
        }
    }
    for (std::uint32_t index = 0; index < system->actor_count; ++index) {
        GeyserActor& actor = system->actors[index];
        if (!actor.alive) continue;
        const room::Vec3 direction = geyser_direction(actor);
        const float source_length =
            500.0f * (actor.placement.type == 0 ? 1.0f : 0.8f);
        const float distance = point_segment_distance(
            context.player_position, actor.placement.position,
            direction, actor.decoded.reactive ? 1200.0f : source_length);
        if (actor.decoded.reactive) {
            update_reactive(&actor, context, distance);
        } else {
            update_periodic(&actor);
        }
        if ((actor.state == GeyserState::Warning ||
             actor.state == GeyserState::On) &&
            actor.updates % (actor.state == GeyserState::On ? 2 : 6) == 0) {
            emit_particle(system, index);
        }
        if (actor.state == GeyserState::On &&
            actor.strength > 0.2f &&
            !context.player_heavy_boots) {
            const float radius =
                (actor.placement.type == 0 ? 70.0f : 30.0f) *
                actor.placement.scale.x;
            const float active_distance = point_segment_distance(
                context.player_position, actor.placement.position,
                direction, source_length * actor.strength);
            if (active_distance <= radius + 35.0f) {
                interaction->horizontal_impulse.x +=
                    direction.x * 7.0f * actor.strength;
                interaction->horizontal_impulse.z +=
                    direction.z * 7.0f * actor.strength;
                interaction->vertical_impulse +=
                    direction.y * 7.0f * actor.strength;
                ++interaction->contacts;
                ++actor.player_contacts;
            }
        }
        ++actor.updates;
        if (!std::isfinite(actor.strength)) {
            ++system->non_finite_values;
            system->error = Error::UpdateFailure;
            return system->error;
        }
    }
    return Error::Ok;
}

Error reset_actor_system(ActorSystem* system) {
    if (system == nullptr) return Error::CreateFailure;
    const std::uint32_t count = system->actor_count;
    for (std::uint32_t index = 0; index < count; ++index) {
        GeyserActor& actor = system->actors[index];
        actor.state = GeyserState::Off;
        actor.timer =
            actor.decoded.reactive ? 0 : actor.decoded.off_updates;
        actor.disappear_target = 0;
        actor.strength = 0.0f;
        actor.rng = 0x47595352u ^ actor.placement.source_index;
        actor.alive = true;
    }
    std::memset(system->particles, 0, sizeof(system->particles));
    for (std::uint32_t index = 0;
         index < system->essential_source_actor_count; ++index) {
        system->essential_source_actors[index].alive = true;
        system->essential_source_actors[index].updates = 0;
    }
    system->active_count =
        count + system->essential_source_actor_count;
    system->particle_peak = 0;
    system->particle_overflow = 0;
    ++system->reset_calls;
    return Error::Ok;
}

void destroy_actor_system(ActorSystem* system) {
    if (system == nullptr) return;
    for (std::uint32_t index = 0; index < system->actor_count; ++index) {
        if (system->actors[index].alive) {
            system->actors[index].alive = false;
            ++system->destroy_calls;
        }
    }
    for (std::uint32_t index = 0;
         index < system->essential_source_actor_count; ++index) {
        if (system->essential_source_actors[index].alive) {
            system->essential_source_actors[index].alive = false;
            ++system->destroy_calls;
        }
    }
    system->active_count = 0;
    std::memset(system->particles, 0, sizeof(system->particles));
}

bool actor_system_consistent(const ActorSystem& system) {
    if (system.actor_count > kActorCapacity ||
        system.essential_source_actor_count >
            kEssentialSourceActorCapacity ||
        system.active_count >
            system.actor_count + system.essential_source_actor_count ||
        system.particle_overflow != 0 ||
        system.allocations_during_update != 0 ||
        system.allocations_during_render != 0 ||
        system.non_finite_values != 0) {
        return false;
    }
    for (std::uint32_t index = 0;
         index < system.essential_source_actor_count; ++index) {
        const EssentialSourceActor& actor =
            system.essential_source_actors[index];
        if (!finite_vec(actor.placement.position) ||
            !finite_vec(actor.placement.scale) ||
            !essential_source_process(actor.placement.process_id)) {
            return false;
        }
    }
    for (std::uint32_t index = 0; index < system.actor_count; ++index) {
        const GeyserActor& actor = system.actors[index];
        if (!finite_vec(actor.placement.position) ||
            !finite_vec(actor.placement.scale) ||
            !std::isfinite(actor.strength) ||
            static_cast<std::uint32_t>(actor.state) >
                static_cast<std::uint32_t>(GeyserState::Disappear)) {
            return false;
        }
    }
    return true;
}

std::uint32_t active_particle_count(const ActorSystem& system) {
    std::uint32_t count = 0;
    for (const Particle& particle : system.particles) {
        count += particle.active;
    }
    return count;
}

Error validate_actor_budgets(
    std::uint32_t actor_main_bytes,
    std::uint32_t texture_edram_bytes,
    std::uint32_t dynamic_buffer_bytes,
    std::uint32_t actor_draw_calls,
    std::uint32_t total_draw_calls,
    std::uint32_t edram_remaining) {
    if (actor_main_bytes > 1048576) return Error::MemoryBudget;
    if (texture_edram_bytes > 65536 || edram_remaining < 400000) {
        return Error::MemoryBudget;
    }
    if (dynamic_buffer_bytes > 131072) {
        return Error::DynamicBufferOverflow;
    }
    if (actor_draw_calls > 8 || total_draw_calls > 80) {
        return Error::DrawLimit;
    }
    return Error::Ok;
}

}  // namespace dusk::psp::actor
