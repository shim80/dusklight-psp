#include "dusk/psp/startup_intro.hpp"

namespace dusk::psp::startup {
namespace {

// Compiled from GZ2P01 Demo01_01/demo01_01.stb. The source camera stores
// seconds while the event sequence runs at 30 Hz. The wide shot samples the
// first FVB camera set at 9 s (timeline frame 270). The Rusl close-up begins
// with the second camera set and cut02 actor tracks after frame 390; frame 482
// is the first held dialogue composition represented by this bounded runtime.
constexpr NewGameIntroShot kWideShot = {
    1512, 3002, 270,
    3, 47, 18, 31, 41,
    {-16537.84375f, 230.799316f, -4335.285156f},
    {-16656.855469f, 204.427231f, -4432.334961f},
    30.128679f, 10.0f, 100000.0f,
    {-17320.0f, -62.0f, -5100.0f},
    {0.0f, -115.0f, 0.0f},
    30.0f, 30.0f,
};

constexpr NewGameIntroShot kRuslCloseupShot = {
    1514, 3004, 482,
    4, 47, 19, 31, 41,
    {-17422.097656f, -33.586063f, -5202.602051f},
    {-17379.376953f, 19.296864f, -5062.423828f},
    23.482006f, 10.0f, 100000.0f,
    {-17320.0f, -62.0f, -5100.0f},
    {0.0f, -115.0f, 0.0f},
    92.0f, 92.0f,
};

}  // namespace

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
const NewGameIntroShot* NewGameIntroRuntime::shot() const {
    if (phase_ == NewGameIntroPhase::Wide) {
        return &kWideShot;
    }
    if (phase_ == NewGameIntroPhase::Closeup) {
        return &kRuslCloseupShot;
    }
    return nullptr;
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
        return "They say it's the only time when\n"
               "our world intersects with theirs...";
    }
    if (phase_ == NewGameIntroPhase::Closeup) {
        return "That is why loneliness always\n"
               "pervades the hour of twilight...";
    }
    return "";
}

}  // namespace dusk::psp::startup
