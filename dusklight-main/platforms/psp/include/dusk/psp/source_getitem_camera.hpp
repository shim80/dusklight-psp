#ifndef DUSK_PSP_SOURCE_GETITEM_CAMERA_HPP
#define DUSK_PSP_SOURCE_GETITEM_CAMERA_HPP

#include <cstdint>

namespace dusk::psp::camera {

struct SourceCameraVec3 {
    float x;
    float y;
    float z;
};

struct SourceGetItemCameraInput {
    SourceCameraVec3 start_center;
    SourceCameraVec3 start_eye;
    float start_fov;
    SourceCameraVec3 player_position;
    SourceCameraVec3 player_attention;
    std::int16_t player_yaw;
    int event_type;
    bool wolf;
    bool line_blocked;
};

struct SourceGetItemCameraView {
    SourceCameraVec3 center;
    SourceCameraVec3 eye;
    float fov;
    bool finished;
};

class SourceGetItemCamera {
public:
    bool begin(const SourceGetItemCameraInput& input);
    bool step();
    void reset();
    const SourceGetItemCameraView& view() const { return view_; }
    std::uint32_t timer() const { return timer_; }
    int resolved_type() const { return resolved_type_; }
    bool active() const { return active_; }

private:
    struct Globe {
        float radius;
        std::int16_t v;
        std::int16_t u;
    };
    static SourceCameraVec3 rotate_y(SourceCameraVec3 value, std::int16_t yaw);
    static SourceCameraVec3 relative(
        const SourceCameraVec3& position,
        const SourceCameraVec3& attention,
        std::int16_t yaw,
        SourceCameraVec3 local);
    static Globe globe(SourceCameraVec3 value);
    static SourceCameraVec3 xyz(const Globe& value);
    static float spline_value(std::uint32_t step, std::uint32_t duration);
    static std::int16_t mix_angle(
        std::int16_t first, std::int16_t second, float amount);

    SourceGetItemCameraInput input_ = {};
    SourceGetItemCameraView view_ = {};
    SourceCameraVec3 target_center_ = {};
    SourceCameraVec3 target_eye_ = {};
    Globe start_direction_ = {};
    Globe target_direction_ = {};
    std::uint32_t spline_step_ = 0;
    std::uint32_t style_timer_ = 0;
    std::uint32_t timer_ = 0;
    int resolved_type_ = -1;
    bool active_ = false;
};

}  // namespace dusk::psp::camera

#endif
