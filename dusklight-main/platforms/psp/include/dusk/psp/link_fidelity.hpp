#ifndef DUSK_PSP_LINK_FIDELITY_HPP
#define DUSK_PSP_LINK_FIDELITY_HPP

#include "dusk/psp/room_collision.hpp"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <limits>

namespace dusk::psp::link {

constexpr float kPi = 3.14159265358979323846f;
constexpr float kSourceWorldScale = 1.0f;
constexpr std::int32_t kSourceStickMinimum = 15;
constexpr std::int32_t kSourceStickMaximum = 72;
constexpr std::int32_t kSourceStickOctagon = 40;
constexpr float kSourceStickJutDivisor = 54.0f;
constexpr float kSourceMaxSpeedPerUpdate = 23.0f;
constexpr float kSourceAccelerationPerUpdate = 1.9f;
constexpr float kSourceDecelerationPerUpdate = 2.2f;
constexpr float kWalkChangeRate = 0.4f;
constexpr float kRunChangeRate = 0.8f;
constexpr std::int16_t kMaxTurnAngleS16 = 4500;
constexpr std::int16_t kMinTurnAngleS16 = 100;
constexpr std::int16_t kWaitTurnStartDistanceS16 = 0x7800;
constexpr std::int16_t kWaitTurnRateS16 = 30;
constexpr std::int16_t kWaitTurnMaximumS16 = 0x3cdf;
constexpr std::int16_t kWaitTurnMinimumS16 = 8000;
constexpr std::int16_t kTurnEventThresholdS16 = 0x0800;
constexpr float kUpdatesPerSecond = 30.0f;
constexpr float kPlayerRadius = 35.0f;
constexpr float kPlayerHeight = 160.0f;
constexpr float kCameraTargetHeight = 95.0f;
constexpr float kNormalAttentionHeight = 150.0f;

struct SourceToPspWorldTransform {
    float scale;
    bool permutes_axes;
    bool reflects_axis;
    bool reverses_winding;
};

constexpr SourceToPspWorldTransform kSourceToPspWorld = {
    kSourceWorldScale, false, false, false};

inline bool coordinate_contract_valid(
    const SourceToPspWorldTransform& transform) {
    return std::isfinite(transform.scale) &&
           transform.scale == kSourceWorldScale &&
           !transform.permutes_axes &&
           !transform.reflects_axis &&
           !transform.reverses_winding;
}

struct Vec2 {
    float x;
    float z;
};

struct Stick {
    float right;
    float forward;
    float magnitude;
};

struct ActorMatrix {
    float value[3][4];
};

inline float wrap_angle(float angle) {
    while (angle > kPi) {
        angle -= 2.0f * kPi;
    }
    while (angle < -kPi) {
        angle += 2.0f * kPi;
    }
    return angle;
}

inline float s16_to_radians(std::uint16_t angle) {
    return static_cast<float>(static_cast<std::int16_t>(angle)) *
           (2.0f * kPi / 65536.0f);
}

inline std::int16_t radians_to_s16(float radians) {
    const float wrapped = wrap_angle(radians);
    const std::int32_t value = static_cast<std::int32_t>(
        wrapped * (32768.0f / kPi));
    return value >= 32768
        ? static_cast<std::int16_t>(-32768)
        : static_cast<std::int16_t>(value);
}

inline float exact_s16_radians(std::uint16_t angle) {
    const std::int16_t wanted = static_cast<std::int16_t>(angle);
    float radians = s16_to_radians(angle);
    if (radians_to_s16(radians) != wanted) {
        radians = std::nextafter(
            radians,
            wanted < 0
                ? -std::numeric_limits<float>::infinity()
                : std::numeric_limits<float>::infinity());
    }
    return radians;
}

// cM_atan2s quantizes the smaller/larger component ratio to one of 1025
// entries before looking up the angle. The source then converts that s16
// through radians when constructing cSGlobe, so preserve both conversions.
inline std::uint16_t source_atan_table(float numerator, float denominator) {
    const std::int32_t index = std::clamp(
        static_cast<std::int32_t>(numerator / denominator * 1024.0f),
        std::int32_t{0}, std::int32_t{1024});
    return static_cast<std::uint16_t>(std::lround(
        std::atan(static_cast<float>(index) / 1024.0f) *
        (32768.0f / kPi)));
}

inline std::int16_t source_atan2_s16(float y, float x) {
    constexpr float kSourceZero =
        32.0f * std::numeric_limits<float>::epsilon();
    std::uint32_t result = 0;
    if (std::fabs(y) < kSourceZero) {
        result = x >= 0.0f ? 0u : 0x8000u;
    } else if (std::fabs(x) < kSourceZero) {
        result = y >= 0.0f ? 0x4000u : 0xc000u;
    } else if (y >= 0.0f && x >= 0.0f) {
        result = x >= y
            ? source_atan_table(y, x)
            : 0x4000u - source_atan_table(x, y);
    } else if (y >= 0.0f) {
        result = -x < y
            ? source_atan_table(-x, y) + 0x4000u
            : 0x8000u - source_atan_table(y, -x);
    } else if (x < 0.0f) {
        result = -x >= -y
            ? source_atan_table(-y, -x) + 0x8000u
            : 0xc000u - source_atan_table(-x, -y);
    } else {
        result = x < -y
            ? source_atan_table(x, -y) + 0xc000u
            : 0u - source_atan_table(-y, x);
    }
    return static_cast<std::int16_t>(result);
}

inline float source_globe_yaw(float x, float z) {
    constexpr double kSourcePi = 3.14159265358979323846;
    const float source_radians = static_cast<float>(
        static_cast<double>(source_atan2_s16(x, z)) *
        (2.0 * kSourcePi / 65536.0));
    const auto round_trip = static_cast<std::int16_t>(
        static_cast<std::int32_t>(
            static_cast<double>(source_radians) *
            (32768.0 / kSourcePi)));
    return s16_to_radians(static_cast<std::uint16_t>(round_trip));
}

inline bool source_wait_turn_required(float current, float target) {
    const std::int16_t difference = static_cast<std::int16_t>(
        radians_to_s16(target) - radians_to_s16(current));
    return std::abs(static_cast<std::int32_t>(difference)) >
           kWaitTurnStartDistanceS16;
}

inline float source_wait_turn_yaw(float current, float target) {
    std::int16_t value = radians_to_s16(current);
    const std::int16_t target_value = radians_to_s16(target);
    std::int16_t difference = static_cast<std::int16_t>(
        target_value - value);
    if (value == target_value) {
        return exact_s16_radians(static_cast<std::uint16_t>(value));
    }
    std::int16_t step = static_cast<std::int16_t>(
        difference / kWaitTurnRateS16);
    if (step > kWaitTurnMinimumS16 ||
        step < -kWaitTurnMinimumS16) {
        step = std::clamp(
            step,
            static_cast<std::int16_t>(-kWaitTurnMaximumS16),
            kWaitTurnMaximumS16);
        value = static_cast<std::int16_t>(value + step);
    } else if (difference >= 0) {
        value = static_cast<std::int16_t>(
            value + kWaitTurnMinimumS16);
        difference = static_cast<std::int16_t>(
            target_value - value);
        if (difference <= 0) {
            value = target_value;
        }
    } else {
        value = static_cast<std::int16_t>(
            value - kWaitTurnMinimumS16);
        difference = static_cast<std::int16_t>(
            target_value - value);
        if (difference >= 0) {
            value = target_value;
        }
    }
    return exact_s16_radians(static_cast<std::uint16_t>(value));
}

inline bool source_wait_turn_reached(float current, float target) {
    return radians_to_s16(current) == radians_to_s16(target);
}

inline std::int16_t source_yaw_error_s16(
    float target, float current) {
    return static_cast<std::int16_t>(
        radians_to_s16(target) - radians_to_s16(current));
}

inline float actor_to_model_orientation(float actor_yaw) {
    return wrap_angle(actor_yaw);
}

inline Vec2 forward_from_yaw(float yaw) {
    return {std::sin(yaw), std::cos(yaw)};
}

inline ActorMatrix actor_matrix(
    const room::Vec3& position, float actor_yaw) {
    const float yaw = actor_to_model_orientation(actor_yaw);
    const float sine = std::sin(yaw);
    const float cosine = std::cos(yaw);
    return {{
        {cosine, 0.0f, sine, position.x},
        {0.0f, 1.0f, 0.0f, position.y},
        {-sine, 0.0f, cosine, position.z},
    }};
}

inline room::Vec3 transform_point(
    const ActorMatrix& matrix, const room::Vec3& point) {
    return {
        matrix.value[0][0] * point.x +
            matrix.value[0][1] * point.y +
            matrix.value[0][2] * point.z + matrix.value[0][3],
        matrix.value[1][0] * point.x +
            matrix.value[1][1] * point.y +
            matrix.value[1][2] * point.z + matrix.value[1][3],
        matrix.value[2][0] * point.x +
            matrix.value[2][1] * point.y +
            matrix.value[2][2] * point.z + matrix.value[2][3],
    };
}

inline room::Vec3 camera_target(const room::Vec3& actor_origin) {
    return {
        actor_origin.x,
        actor_origin.y + kCameraTargetHeight,
        actor_origin.z,
    };
}

inline float source_camera_heading(float actor_to_camera_yaw) {
    return wrap_angle(actor_to_camera_yaw + kPi);
}

inline std::int16_t source_link_stick_angle_s16(const Stick& stick) {
    std::int16_t pad_angle = 0;
    if (stick.magnitude > 0.0f) {
        if (stick.forward == 0.0f) {
            pad_angle = stick.right > 0.0f ? 0x4000 : -0x4000;
        } else {
            constexpr float kJutPi = 3.1415926f;
            pad_angle = static_cast<std::int16_t>(
                (32768.0f / kJutPi) *
                std::atan2(stick.right, -stick.forward));
        }
    }
    return static_cast<std::int16_t>(
        static_cast<std::uint16_t>(pad_angle) + 0x8000u);
}

inline float source_move_yaw(
    float stick_angle, float actor_to_camera_yaw) {
    const std::uint16_t stick = static_cast<std::uint16_t>(
        radians_to_s16(stick_angle));
    const std::uint16_t camera = static_cast<std::uint16_t>(
        radians_to_s16(source_camera_heading(actor_to_camera_yaw)));
    return s16_to_radians(static_cast<std::uint16_t>(stick + camera));
}

inline room::Vec3 normal_attention_origin(
    const ActorMatrix& model_base_matrix) {
    // daAlink_c::setAttentionPos transforms normalOffset={0,150,0}
    // through the model base rotation, then composes current.pos and the
    // model base Y translation. The canonical flat-ground base matrix is
    // exactly the actor matrix represented here.
    return transform_point(
        model_base_matrix, {0.0f, kNormalAttentionHeight, 0.0f});
}

inline Stick normalize_stick(float analog_x, float analog_y) {
    auto raw_axis = [](float value) -> std::int32_t {
        return static_cast<std::int32_t>(std::lround(
            std::clamp(value, -1.0f, 1.0f) * 127.0f));
    };
    std::int32_t x = raw_axis(analog_x);
    std::int32_t forward = raw_axis(-analog_y);
    auto clamp_axis = [](std::int32_t value) -> std::int32_t {
        const std::int32_t sign = value < 0 ? -1 : 1;
        value = std::abs(value);
        if (value <= kSourceStickMinimum) {
            return std::int32_t{0};
        }
        value -= kSourceStickMinimum;
        return sign * (
            value * kSourceStickMaximum /
            (127 - kSourceStickMinimum));
    };
    x = clamp_axis(x);
    forward = clamp_axis(forward);
    const std::int32_t absolute_x = std::abs(x);
    const std::int32_t absolute_forward = std::abs(forward);
    const std::int32_t divisor =
        absolute_forward <= absolute_x
            ? kSourceStickOctagon * absolute_x +
                (kSourceStickMaximum - kSourceStickOctagon) *
                    absolute_forward
            : kSourceStickOctagon * absolute_forward +
                (kSourceStickMaximum - kSourceStickOctagon) *
                    absolute_x;
    if (kSourceStickOctagon * kSourceStickMaximum < divisor) {
        x = kSourceStickOctagon * kSourceStickMaximum * x / divisor;
        forward =
            kSourceStickOctagon * kSourceStickMaximum * forward /
            divisor;
    }
    float normalized_x =
        static_cast<float>(x) / kSourceStickJutDivisor;
    float normalized_forward =
        static_cast<float>(forward) / kSourceStickJutDivisor;
    float magnitude = std::sqrt(
        normalized_x * normalized_x +
        normalized_forward * normalized_forward);
    if (magnitude <= 0.0f) {
        return {};
    }
    if (magnitude > 1.0f) {
        normalized_x /= magnitude;
        normalized_forward /= magnitude;
        magnitude = 1.0f;
    }
    return {
        normalized_x,
        normalized_forward,
        magnitude,
    };
}

inline Vec2 camera_relative_world(
    const Stick& stick, float actor_to_camera_yaw) {
    const Vec2 view_forward = {
        -std::sin(actor_to_camera_yaw),
        -std::cos(actor_to_camera_yaw),
    };
    const Vec2 view_right = {
        view_forward.z,
        -view_forward.x,
    };
    return {
        view_right.x * stick.right +
            view_forward.x * stick.forward,
        view_right.z * stick.right +
            view_forward.z * stick.forward,
    };
}

inline void world_direction_to_camera_input(
    float world_x, float world_z, float actor_to_camera_yaw,
    float* analog_x, float* analog_y) {
    const Vec2 view_forward = {
        -std::sin(actor_to_camera_yaw),
        -std::cos(actor_to_camera_yaw),
    };
    const Vec2 view_right = {
        view_forward.z,
        -view_forward.x,
    };
    *analog_x = world_x * view_right.x + world_z * view_right.z;
    const float forward =
        world_x * view_forward.x + world_z * view_forward.z;
    *analog_y = -forward;
}

inline float approach_yaw(
    float current, float target, float delta_seconds) {
    const float error = wrap_angle(target - current);
    const float frame_scale =
        std::clamp(delta_seconds, 0.0f, 1.0f / 15.0f) *
        kUpdatesPerSecond;
    const float maximum =
        static_cast<float>(kMaxTurnAngleS16) *
        (2.0f * kPi / 65536.0f) * frame_scale;
    const float minimum =
        static_cast<float>(kMinTurnAngleS16) *
        (2.0f * kPi / 65536.0f) * frame_scale;
    const float step = std::min(
        std::fabs(error),
        std::clamp(std::fabs(error) / 5.0f, minimum, maximum));
    return wrap_angle(current + std::copysign(step, error));
}

inline float source_move_approach_yaw(
    float current, float target, float stick_magnitude) {
    std::int16_t value = radians_to_s16(current);
    const std::int16_t target_value = radians_to_s16(target);
    std::int16_t difference = static_cast<std::int16_t>(
        target_value - value);
    if (value == target_value) {
        return exact_s16_radians(static_cast<std::uint16_t>(value));
    }
    const float squared = std::clamp(
        stick_magnitude, 0.0f, 1.0f) *
        std::clamp(stick_magnitude, 0.0f, 1.0f);
    const std::int16_t maximum = static_cast<std::int16_t>(
        std::max(10.0f, kMaxTurnAngleS16 * squared));
    const std::int16_t minimum = static_cast<std::int16_t>(
        std::max(1.0f, kMinTurnAngleS16 * squared));
    std::int16_t step = static_cast<std::int16_t>(difference / 5);
    if (step > minimum || step < -minimum) {
        step = std::clamp(
            step, static_cast<std::int16_t>(-maximum), maximum);
        value = static_cast<std::int16_t>(value + step);
    } else if (difference >= 0) {
        value = static_cast<std::int16_t>(value + minimum);
        if (static_cast<std::int16_t>(target_value - value) <= 0) {
            value = target_value;
        }
    } else {
        value = static_cast<std::int16_t>(value - minimum);
        if (static_cast<std::int16_t>(target_value - value) >= 0) {
            value = target_value;
        }
    }
    return exact_s16_radians(static_cast<std::uint16_t>(value));
}

inline float source_speed_target(float stick_magnitude) {
    const float value = std::clamp(stick_magnitude, 0.0f, 1.0f);
    return kSourceMaxSpeedPerUpdate * kUpdatesPerSecond *
           value * value;
}

inline float source_normal_speed_target(float stick_magnitude) {
    const float value = std::clamp(stick_magnitude, 0.0f, 1.0f);
    return kSourceMaxSpeedPerUpdate * value * value;
}

inline float approach_normal_speed(
    float current, float target, float stick_magnitude,
    float delta_seconds) {
    const float frame_scale =
        std::clamp(delta_seconds, 0.0f, 1.0f / 15.0f) *
        kUpdatesPerSecond;
    if (target > current) {
        const float value =
            std::clamp(stick_magnitude, 0.0f, 1.0f);
        return std::min(
            target,
            current + kSourceAccelerationPerUpdate * value * value *
                frame_scale);
    }
    return std::max(
        target,
        current - kSourceDecelerationPerUpdate * frame_scale);
}

inline float source_move_speed_modifier(float normal_speed) {
    constexpr float kFootPositionRatio = 0.99f;
    constexpr float kMinimumWalkRate = 0.7f;
    const float speed_rate = std::fabs(normal_speed) /
        kSourceMaxSpeedPerUpdate;
    if (speed_rate < kWalkChangeRate) {
        if (std::fabs(normal_speed) < 0.001f) {
            return 0.0f;
        }
        const float blend = kMinimumWalkRate +
            (speed_rate / kWalkChangeRate) *
                (1.0f - kMinimumWalkRate);
        return 1.0f - (1.0f - kFootPositionRatio) * blend;
    }
    if (speed_rate < kRunChangeRate) {
        const float blend =
            (speed_rate - kWalkChangeRate) /
            (kRunChangeRate - kWalkChangeRate);
        return kFootPositionRatio * (1.0f - blend);
    }
    return 0.0f;
}

inline float source_effective_speed_base(float normal_speed) {
    return normal_speed *
        (1.0f - std::fabs(source_move_speed_modifier(normal_speed)));
}

inline float source_effective_speed(
    float normal_speed, float foot_speed, float old_frame_rate) {
    const float modifier = source_move_speed_modifier(normal_speed);
    const float base = normal_speed * (1.0f - std::fabs(modifier));
    const float animated = foot_speed *
        (1.0f - std::clamp(old_frame_rate, 0.0f, 1.0f)) * modifier;
    return normal_speed < 0.0f ? base - animated : base + animated;
}

inline float approach_speed(
    float current, float target, float stick_magnitude,
    float delta_seconds) {
    const float dt =
        std::clamp(delta_seconds, 0.0f, 1.0f / 15.0f);
    if (target > current) {
        const float value =
            std::clamp(stick_magnitude, 0.0f, 1.0f);
        const float acceleration =
            kSourceAccelerationPerUpdate * kUpdatesPerSecond *
            kUpdatesPerSecond * value * value;
        return std::min(target, current + acceleration * dt);
    }
    const float deceleration =
        kSourceDecelerationPerUpdate * kUpdatesPerSecond *
        kUpdatesPerSecond;
    return std::max(target, current - deceleration * dt);
}

}  // namespace dusk::psp::link

#endif
