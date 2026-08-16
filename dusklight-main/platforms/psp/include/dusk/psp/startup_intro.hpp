#ifndef DUSK_PSP_STARTUP_INTRO_HPP
#define DUSK_PSP_STARTUP_INTRO_HPP

#include <cstdint>

namespace dusk::psp::startup {

enum class NewGameIntroPhase : std::uint8_t {
    Inactive,
    Wide,
    Closeup,
    Complete,
};

struct NewGameIntroInput {
    bool advance = false;
    bool skip = false;
};

class NewGameIntroRuntime {
public:
    static constexpr std::uint32_t kMaximumPhaseFrames = 270;

    void initialize(bool enabled);
    bool tick(const NewGameIntroInput& input);

    NewGameIntroPhase phase() const;
    std::uint32_t phase_frames() const;
    bool active() const;
    bool complete() const;
    const char* message() const;

private:
    void advance();

    NewGameIntroPhase phase_ = NewGameIntroPhase::Inactive;
    std::uint32_t phase_frames_ = 0;
};

}  // namespace dusk::psp::startup

#endif
