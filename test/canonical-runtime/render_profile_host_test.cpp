#include "dusk/psp/playable_render.hpp"

#include <cstdio>
#include <cstring>

namespace playable = dusk::psp::playable;

int main() {
    constexpr playable::RenderProfileConfig safe =
        playable::render_profile_config(
            playable::RenderProfile::KnownGoodUnlit);
    constexpr playable::RenderProfileConfig diagnostic =
        playable::render_profile_config(
            playable::RenderProfile::LightingDiagnostics);
    constexpr playable::RenderProfileConfig candidate =
        playable::render_profile_config(
            playable::RenderProfile::CandidateGame);
    const bool valid =
        safe.lighting == playable::LightingMode::Off &&
        safe.fog == playable::FogMode::Off &&
        safe.shadows == playable::ShadowMode::Off &&
        diagnostic.lighting == playable::LightingMode::Off &&
        diagnostic.fog == playable::FogMode::Off &&
        diagnostic.shadows == playable::ShadowMode::Off &&
        candidate.lighting ==
            playable::LightingMode::SafeWrappedDiffuse &&
        candidate.fog == playable::FogMode::Source &&
        candidate.shadows == playable::ShadowMode::ProjectedLink &&
        std::strcmp(
            playable::render_profile_name(
                playable::RenderProfile::KnownGoodUnlit),
            "known_good_unlit") == 0;
    if (!valid) {
        std::fprintf(stderr, "render profile safety contract failed\n");
        return 1;
    }
    std::printf(
        "RENDER_PROFILE_HOST_OK default=known_good_unlit "
        "lighting=off fog=off shadows=off\n");
    return 0;
}
