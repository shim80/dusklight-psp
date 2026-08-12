#ifndef DUSK_PSP_ACTOR_RENDER_HPP
#define DUSK_PSP_ACTOR_RENDER_HPP

#include "dusk/psp/actor_runtime.hpp"

#include <cstdint>

namespace dusk::psp::actor {

struct RenderMetrics {
    std::uint32_t actor_draw_calls;
    std::uint32_t particle_draw_calls;
    std::uint32_t dynamic_buffer_bytes;
    std::uint32_t texture_edram_bytes;
    std::uint32_t vertices;
    bool visual_valid;
};

bool draw_actor_backend(
    const ActorSystem& system, RenderMetrics* metrics);

}  // namespace dusk::psp::actor

#endif
