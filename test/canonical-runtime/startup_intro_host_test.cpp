#include "dusk/psp/startup_intro.hpp"

#include <cassert>
#include <cstring>
#include <cstdio>

int main() {
    using dusk::psp::startup::NewGameIntroPhase;
    using dusk::psp::startup::NewGameIntroRuntime;

    NewGameIntroRuntime intro;
    intro.initialize(true);
    assert(intro.phase() == NewGameIntroPhase::Wide);
    assert(std::strstr(intro.message(), "world intersects") != nullptr);
    intro.tick({.advance = true});
    assert(intro.phase() == NewGameIntroPhase::Closeup);
    assert(std::strstr(intro.message(), "hour of twilight") != nullptr);
    intro.tick({.advance = true});
    assert(intro.complete());

    intro.initialize(true);
    intro.tick({.skip = true});
    assert(intro.complete());

    intro.initialize(true);
    for (std::uint32_t frame = 0;
         frame < NewGameIntroRuntime::kMaximumPhaseFrames; ++frame) {
        intro.tick({});
    }
    assert(intro.phase() == NewGameIntroPhase::Closeup);

    intro.initialize(false);
    assert(intro.complete());
    std::puts(
        "STARTUP_INTRO_HOST_OK phases=wide,closeup,complete "
        "advance=input_or_270_frames");
    return 0;
}
