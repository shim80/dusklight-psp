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

struct NewGameIntroVec3 {
    float x;
    float y;
    float z;
};

struct NewGameIntroShot {
    std::uint16_t source_message_index;
    std::uint16_t source_message_id;
    std::uint16_t source_timeline_frame;
    std::uint16_t link_body_bck_id;
    std::uint16_t rusl_model_id;
    std::uint16_t rusl_body_bck_id;
    std::uint16_t rusl_btk_id;
    std::uint16_t rusl_btp_id;
    NewGameIntroVec3 camera_eye;
    NewGameIntroVec3 camera_center;
    float camera_fov;
    float camera_near;
    float camera_far;
    NewGameIntroVec3 actor_translation;
    NewGameIntroVec3 actor_rotation_degrees;
    float link_animation_frame;
    float rusl_animation_frame;
};

class NewGameIntroRuntime {
public:
    static constexpr std::uint32_t kMaximumPhaseFrames = 270;

    void initialize(bool enabled);
    bool tick(const NewGameIntroInput& input);

    NewGameIntroPhase phase() const;
    std::uint32_t phase_frames() const;
    const NewGameIntroShot* shot() const;
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
