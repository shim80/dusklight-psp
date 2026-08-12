#include "dusk/psp/source_getitem_camera.hpp"

#include <algorithm>
#include <cmath>

namespace dusk::psp::camera {
namespace {
constexpr float kPi = 3.14159265358979323846f;
struct Row {
    SourceCameraVec3 center;
    SourceCameraVec3 eye;
    float fov;
    std::uint32_t timer;
    int side;
};
constexpr Row kRows[5] = {
    {{0, -27, 32}, {84, -18, 134}, 50, 12, 1},
    {{17, 10, -45}, {-45, 128, 45}, 50, 12, 0},
    {{0, -27, -62}, {-84, -18, -164}, 50, 17, -1},
    {{0, -27, -62}, {-84, -18, -164}, 50, 17, -1},
    {{-12, 10, -130}, {50, 128, -220}, 50, 17, 0},
};
constexpr float kCurve[6] = {0.0f, 0.0f, 0.1f, 0.7f, 1.0f, 1.0f};
SourceCameraVec3 add(SourceCameraVec3 a, SourceCameraVec3 b) {
    return {a.x+b.x,a.y+b.y,a.z+b.z};
}
SourceCameraVec3 sub(SourceCameraVec3 a, SourceCameraVec3 b) {
    return {a.x-b.x,a.y-b.y,a.z-b.z};
}
SourceCameraVec3 scale(SourceCameraVec3 a, float s) {
    return {a.x*s,a.y*s,a.z*s};
}
float angle_rad(std::int16_t a) {
    return static_cast<float>(a) * kPi / 32768.0f;
}
std::int16_t angle_s16(float radians) {
    return static_cast<std::int16_t>(radians * 32768.0f / kPi);
}
}  // namespace

void SourceGetItemCamera::reset() {
    input_ = {};
    view_ = {};
    target_center_ = {};
    target_eye_ = {};
    start_direction_ = {};
    target_direction_ = {};
    spline_step_ = 0;
    style_timer_ = 0;
    timer_ = 0;
    resolved_type_ = -1;
    active_ = false;
}

SourceCameraVec3 SourceGetItemCamera::rotate_y(
    SourceCameraVec3 value, std::int16_t yaw) {
    const float s = std::sin(angle_rad(yaw));
    const float c = std::cos(angle_rad(yaw));
    return {value.x*c + value.z*s, value.y, value.z*c - value.x*s};
}

SourceCameraVec3 SourceGetItemCamera::relative(
    const SourceCameraVec3&, const SourceCameraVec3& attention,
    std::int16_t yaw, SourceCameraVec3 local) {
    return add(attention, rotate_y(local, yaw));
}

SourceGetItemCamera::Globe SourceGetItemCamera::globe(SourceCameraVec3 value) {
    const float horizontal = std::sqrt(value.x*value.x + value.z*value.z);
    const float radius = std::sqrt(horizontal*horizontal + value.y*value.y);
    if (!(radius > 0.0f)) return {0.0f,0,0};
    return {
        radius,
        angle_s16(std::atan2(value.y, horizontal)),
        angle_s16(std::atan2(value.x, value.z)),
    };
}

SourceCameraVec3 SourceGetItemCamera::xyz(const Globe& value) {
    const float v = angle_rad(value.v);
    const float u = angle_rad(value.u);
    const float h = value.radius * std::cos(v);
    return {h*std::sin(u), value.radius*std::sin(v), h*std::cos(u)};
}

std::int16_t SourceGetItemCamera::mix_angle(
    std::int16_t first, std::int16_t second, float amount) {
    const std::int16_t delta = static_cast<std::int16_t>(second - first);
    return static_cast<std::int16_t>(
        first + static_cast<std::int16_t>(static_cast<float>(delta) * amount));
}

float SourceGetItemCamera::spline_value(
    std::uint32_t step, std::uint32_t duration) {
    // Exact d2DBSplinePath::Init(6,duration+1) + Step/Calc sequence used by
    // getItemEvCamera. begin() performs the initial Step; every camera frame
    // performs one further Step before Calc.
    const std::uint32_t total = duration + 1u;
    const std::uint32_t sample = std::min(step, total - 1u);
    const float stride = 4.0f / static_cast<float>(total - 1u);
    float point = stride * static_cast<float>(sample);
    std::uint32_t key = static_cast<std::uint32_t>(point);
    float t = point - static_cast<float>(key);
    if (sample == total - 1u) t = 1.0f;
    const std::uint32_t k0 = std::min<std::uint32_t>(key, 5u);
    const std::uint32_t k1 = std::min<std::uint32_t>(key + 1u, 5u);
    const std::uint32_t k2 = std::min<std::uint32_t>(key + 2u, 5u);
    const float inv = 1.0f - t;
    return inv*inv*0.5f*kCurve[k0] +
           (t*inv + 0.5f)*kCurve[k1] +
           t*t*0.5f*kCurve[k2];
}

bool SourceGetItemCamera::begin(const SourceGetItemCameraInput& input) {
    reset();
    if (input.event_type < 0 || input.event_type >= 5 ||
        !std::isfinite(input.start_fov)) return false;
    input_ = input;
    resolved_type_ = input.event_type;
    if (resolved_type_ == 2) resolved_type_ = input.wolf ? 4 : 3;
    const Row& row = kRows[resolved_type_];
    timer_ = row.timer;
    view_ = {input.start_center, input.start_eye, input.start_fov, false};
    target_center_ = relative(
        input.player_position, input.player_attention,
        input.player_yaw, row.center);
    SourceCameraVec3 eye = row.eye;
    if (row.side != 0) {
        const Globe current = globe(sub(input.start_eye, input.player_position));
        const std::int16_t relative_angle =
            static_cast<std::int16_t>(current.u - input.player_yaw);
        if ((row.side > 0 && relative_angle < 0) ||
            (row.side < 0 && relative_angle > 0)) {
            eye.x = -eye.x;
        }
        if (input.line_blocked) eye.x = -eye.x;
    }
    target_eye_ = relative(
        input.player_position, input.player_attention,
        input.player_yaw, eye);
    start_direction_ = globe(sub(input.start_eye, input.start_center));
    target_direction_ = globe(sub(target_eye_, target_center_));
    spline_step_ = 1;  // Init + initial Step at style start.
    active_ = true;
    return true;
}

bool SourceGetItemCamera::step() {
    if (!active_ || view_.finished) return false;
    if (style_timer_ >= timer_) {
        view_.finished = true;
        active_ = false;
        return true;
    }
    ++spline_step_;
    const float amount = spline_value(spline_step_ - 1u, timer_);
    view_.center = add(input_.start_center,
        scale(sub(target_center_, input_.start_center), amount));
    Globe direction = {};
    direction.radius = start_direction_.radius +
        amount * (target_direction_.radius - start_direction_.radius);
    direction.v = mix_angle(start_direction_.v, target_direction_.v, amount);
    direction.u = mix_angle(start_direction_.u, target_direction_.u, amount);
    view_.eye = add(view_.center, xyz(direction));
    view_.fov += amount * (kRows[resolved_type_].fov - view_.fov);
    ++style_timer_;
    return true;
}

}  // namespace dusk::psp::camera
