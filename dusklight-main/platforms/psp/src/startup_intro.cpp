#include "dusk/psp/startup_intro.hpp"

namespace dusk::psp::startup {

void NewGameIntroRuntime::initialize(bool enabled) {
    phase_ = enabled
        ? NewGameIntroPhase::Wide
        : NewGameIntroPhase::Complete;
    phase_frames_ = 0;
}

void NewGameIntroRuntime::advance() {
    phase_ = phase_ == NewGameIntroPhase::Wide
        ? NewGameIntroPhase::Closeup
        : NewGameIntroPhase::Complete;
    phase_frames_ = 0;
}

bool NewGameIntroRuntime::tick(const NewGameIntroInput& input) {
    if (!active()) {
        return phase_ != NewGameIntroPhase::Inactive;
    }
    if (input.skip) {
        phase_ = NewGameIntroPhase::Complete;
        phase_frames_ = 0;
        return true;
    }
    ++phase_frames_;
    if (input.advance || phase_frames_ >= kMaximumPhaseFrames) {
        advance();
    }
    return true;
}

NewGameIntroPhase NewGameIntroRuntime::phase() const { return phase_; }
std::uint32_t NewGameIntroRuntime::phase_frames() const {
    return phase_frames_;
}
bool NewGameIntroRuntime::active() const {
    return phase_ == NewGameIntroPhase::Wide ||
           phase_ == NewGameIntroPhase::Closeup;
}
bool NewGameIntroRuntime::complete() const {
    return phase_ == NewGameIntroPhase::Complete;
}

const char* NewGameIntroRuntime::message() const {
    if (phase_ == NewGameIntroPhase::Wide) {
        return "They say it is the only time when\n"
               "our world intersects with theirs...";
    }
    if (phase_ == NewGameIntroPhase::Closeup) {
        return "That is why loneliness always\n"
               "pervades the hour of twilight...";
    }
    return "";
}

}  // namespace dusk::psp::startup
