#ifndef DUSK_PSP_ACTOR_RUNTIME_HPP
#define DUSK_PSP_ACTOR_RUNTIME_HPP

#include "dusk/psp/room_collision.hpp"
#include "dusk/psp/room_package.hpp"

#include <cstdint>

namespace dusk::psp::actor {

constexpr std::uint32_t kActorCapacity = 16;
constexpr std::uint32_t kEssentialSourceActorCapacity = 16;
constexpr std::uint32_t kParticleCapacity = 96;
constexpr std::uint32_t kParticlesPerActor = 48;
constexpr std::uint16_t kGeyserProcessId = 0x0167;
constexpr std::uint32_t kGeyserNameHash = 0xc856c722;

enum class Error : std::uint32_t {
    Ok,
    MissingScene,
    SceneVersion,
    SceneInvalid,
    Capacity,
    DuplicateSourceIndex,
    UnknownType,
    ProcessMismatch,
    InvalidParameters,
    NonFiniteTransform,
    CreateFailure,
    UpdateFailure,
    InvalidTransition,
    ParticleOverflow,
    DynamicBufferOverflow,
    DrawLimit,
    MemoryBudget,
};

enum class GeyserState : std::uint8_t {
    Off,
    Warning,
    On,
    Disappear,
};

struct GeyserParameters {
    std::uint8_t behavior;
    std::uint8_t off_units;
    std::uint8_t warning_units;
    std::uint8_t on_units;
    std::uint16_t off_updates;
    std::uint16_t warning_updates;
    std::uint16_t on_updates;
    bool reactive;
};

struct Placement {
    char name[8];
    std::uint32_t name_hash;
    std::uint16_t process_id;
    std::uint32_t parameters;
    room::Vec3 position;
    std::int16_t rotation[3];
    std::uint8_t type;
    std::uint8_t room_index;
    std::uint8_t layer;
    std::uint8_t flags;
    std::uint16_t source_index;
    room::Vec3 scale;
    std::uint32_t source_chunk_hash;
};

struct Particle {
    room::Vec3 position;
    room::Vec3 velocity;
    float size;
    float alpha;
    std::uint16_t life;
    std::uint8_t owner;
    bool active;
};

struct GeyserActor {
    Placement placement;
    GeyserParameters decoded;
    GeyserState state;
    std::uint16_t timer;
    std::uint16_t disappear_target;
    float strength;
    std::uint32_t rng;
    std::uint32_t updates;
    std::uint32_t transitions;
    std::uint32_t active_frames;
    std::uint32_t effects_spawned;
    std::uint32_t player_contacts;
    bool alive;
};

// A bounded PSP lifecycle adapter for source records whose real Dusklight
// process is observed by DTRC v2 but whose behavior is not needed during the
// first 180 idle frames. This is deliberately not a Dusklight actor class.
struct EssentialSourceActor {
    Placement placement;
    std::uint32_t updates;
    bool alive;
};

struct Interaction {
    room::Vec3 horizontal_impulse;
    float vertical_impulse;
    std::uint32_t contacts;
};

struct Context {
    room::Vec3 player_position;
    bool player_heavy_boots;
    bool paused;
};

struct ActorSystem {
    alignas(64) GeyserActor actors[kActorCapacity];
    alignas(64) EssentialSourceActor
        essential_source_actors[kEssentialSourceActorCapacity];
    alignas(64) Particle particles[kParticleCapacity];
    std::uint32_t actor_count;
    std::uint32_t essential_source_actor_count;
    std::uint32_t active_count;
    std::uint32_t unsupported_count;
    std::uint32_t particle_peak;
    std::uint32_t particle_overflow;
    std::uint32_t create_calls;
    std::uint32_t update_calls;
    std::uint32_t draw_calls;
    std::uint32_t reset_calls;
    std::uint32_t destroy_calls;
    std::uint32_t allocations_during_update;
    std::uint32_t allocations_during_render;
    std::uint32_t non_finite_values;
    Error error;
};

GeyserParameters decode_geyser_parameters(std::uint32_t parameters);
const char* geyser_state_name(GeyserState state);
Error initialize_actor_system(
    ActorSystem* system, const room::PackageView& scene);
Error update_actor_system(
    ActorSystem* system, const Context& context, Interaction* interaction);
Error reset_actor_system(ActorSystem* system);
void destroy_actor_system(ActorSystem* system);
bool actor_system_consistent(const ActorSystem& system);
std::uint32_t active_particle_count(const ActorSystem& system);
Error validate_actor_budgets(
    std::uint32_t actor_main_bytes,
    std::uint32_t texture_edram_bytes,
    std::uint32_t dynamic_buffer_bytes,
    std::uint32_t actor_draw_calls,
    std::uint32_t total_draw_calls,
    std::uint32_t edram_remaining);

}  // namespace dusk::psp::actor

#endif
