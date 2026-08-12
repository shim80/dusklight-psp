#ifndef DUSK_PSP_GAME_CONTEXT_HPP
#define DUSK_PSP_GAME_CONTEXT_HPP

#include "dusk/psp/actor_runtime.hpp"
#include "dusk/psp/event_runtime.hpp"
#include "dusk/psp/environment_runtime.hpp"
#include "dusk/psp/item_runtime.hpp"
#include "dusk/psp/interaction_runtime.hpp"
#include "dusk/psp/model_runtime.hpp"
#include "dusk/psp/movebg_runtime.hpp"
#include "dusk/psp/original_scene_exit_bridge.hpp"
#include "dusk/psp/playable_render.hpp"
#include "dusk/psp/playable_runtime.hpp"
#include "dusk/psp/real_room_runtime.hpp"
#include "dusk/psp/render_queue.hpp"
#include "dusk/psp/resource_manager.hpp"
#include "dusk/psp/stage_runtime.hpp"
#include "dusk/psp/shadow_runtime.hpp"
#include "dusk/psp/switch_runtime.hpp"

#include <cstdint>

namespace dusk::psp::game {

struct PspGameMetrics {
    std::uint32_t frame_index;
    std::uint32_t allocations_during_loading;
    std::uint32_t allocations_during_playing;
    std::uint32_t bytes_leaked;
    std::uint32_t non_finite_values;
    bool game_context_initialized;
    bool actor_system_initialized;
};

struct PspGameContext {
    resources::PspResourceManager resources;
    render::PspRenderQueue render_queue;
    process::PspProcessManager processes;
    movebg::PspMoveBgWorld movebg;
    model::PspStaticModelRuntime models;
    stage::PspStageRuntime stage;
    environment::PspEnvironmentRuntime environment;
    shadow::PspShadowSystem shadows;
    playable::Runtime player;
    room::RealRoomRuntime room;
    actor::ActorSystem translated_actors;
    playable::RenderMetrics renderer;
    stage::RoomSwitchBank room_switches;
    stage::StageSwitchBank stage_switches;
    stage::PersistentDemoState persistent;
    switches::PspSwitchSurface switch_surface;
    events::PspEventContext event_context;
    items::PspItemContext item_context;
    interaction::PspInteractionContext interaction_context;
    PspGameMetrics metrics;
    bool platform_initialized;
    bool collision_initialized;
    bool camera_initialized;
    bool hud_initialized;

    bool initialize(
        const char* resource_root,
        const void* manifest, std::uint32_t manifest_size,
        resources::ReadResource reader, void* reader_user);
    void shutdown();
    bool consistent() const;
};

}  // namespace dusk::psp::game

#endif
