#include "dusk/psp/game_context.hpp"

#include <cstring>

namespace dusk::psp::game {

bool PspGameContext::initialize(
    const char* resource_root,
    const void* manifest, std::uint32_t manifest_size,
    resources::ReadResource reader, void* reader_user) {
    processes.initialize();
    render_queue.initialize();
    environment.initialize();
    shadows.initialize();
    if (!resources.initialize(
            resource_root, manifest, manifest_size,
            reader, reader_user)) {
        render_queue.shutdown();
        processes.shutdown();
        return false;
    }
    movebg.initialize();
    if (!models.initialize(&resources, &render_queue, &movebg)) {
        movebg.shutdown();
        resources.shutdown();
        render_queue.shutdown();
        processes.shutdown();
        return false;
    }
    process::bind_process_manager(&processes);
    model::bind_model_runtime(&models);
    persistent = {3, 3, 0, 0, false};
    if (!switch_surface.initialize(&persistent) ||
        !event_context.initialize() ||
        !interaction_context.initialize() ||
        !item_context.initialize()) {
        event_context.shutdown();
        switch_surface.shutdown();
        interaction_context.shutdown();
        process::unbind_process_manager();
        model::unbind_model_runtime();
        models.shutdown();
        movebg.shutdown();
        resources.shutdown();
        render_queue.shutdown();
        processes.shutdown();
        return false;
    }
    platform_initialized = true;
    metrics.game_context_initialized = true;
    metrics.allocations_during_playing = 0;
    metrics.bytes_leaked = 0;
    return true;
}

void PspGameContext::shutdown() {
    item_context.shutdown();
    interaction_context.shutdown();
    event_context.shutdown();
    switch_surface.shutdown();
    processes.shutdown();
    process::unbind_process_manager();
    model::unbind_model_runtime();
    models.shutdown();
    movebg.shutdown();
    render_queue.shutdown();
    resources.shutdown();
    platform_initialized = false;
    collision_initialized = false;
    camera_initialized = false;
    hud_initialized = false;
    metrics.game_context_initialized = false;
    environment.initialize();
    shadows.initialize();
}

bool PspGameContext::consistent() const {
    return platform_initialized &&
           metrics.game_context_initialized &&
           resources.initialized() &&
           models.initialized() &&
           switch_surface.initialized() &&
           event_context.initialized() &&
           interaction_context.initialized() &&
           item_context.initialized() &&
           render_queue.initialized() &&
           metrics.allocations_during_playing == 0 &&
           metrics.bytes_leaked == 0 &&
           metrics.non_finite_values == 0 &&
           processes.metrics.errors == 0 &&
           render_queue.overflows() == 0;
}

}  // namespace dusk::psp::game
