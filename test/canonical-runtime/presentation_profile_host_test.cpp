#include "dusk/psp/presentation_profile.hpp"

#include <cstdio>
#include <cstring>

namespace presentation = dusk::psp::presentation;

int main() {
    const bool valid =
        presentation::parse(nullptr) == presentation::Profile::Game &&
        presentation::parse("game") == presentation::Profile::Game &&
        presentation::parse("debug") == presentation::Profile::Debug &&
        presentation::parse("opaque_only") ==
            presentation::Profile::OpaqueOnly &&
        presentation::parse("smoke") ==
            presentation::Profile::Invalid &&
        !presentation::debug_visuals(presentation::Profile::Game) &&
        presentation::debug_visuals(presentation::Profile::Debug) &&
        presentation::opaque_only(presentation::Profile::OpaqueOnly) &&
        !presentation::opaque_only(presentation::Profile::Game) &&
        std::strcmp(
            presentation::name(presentation::Profile::Game),
            "game") == 0;
    if (!valid) {
        std::fprintf(stderr, "presentation profile contract failed\n");
        return 1;
    }
    std::printf(
        "PRESENTATION_PROFILE_HOST_OK default=game "
        "debug_visuals_default=false "
        "test_only_world_entities_visible_in_game=false\n");
    return 0;
}
