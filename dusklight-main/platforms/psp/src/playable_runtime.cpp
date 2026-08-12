#include "dusk/psp/playable_runtime.hpp"

#include <algorithm>
#include <cmath>
#include <cstring>
#include <limits>

namespace dusk::psp::playable {
namespace {

constexpr float kPi = 3.14159265358979323846f;
constexpr float kWalkThreshold = 0.4f;
constexpr float kRunThreshold = 0.8f;
constexpr float kWalkSpeed = 2.0f;
constexpr float kRunSpeed = 4.0f;
// daAlinkHIO_basic_c0::m.mBasicInterpolation from the preserved source
// snapshot.  mDoExt_MtxCalcOldFrame consumes one source render step at a
// time and recursively blends from the pose produced by the previous step.
constexpr float kBasicInterpolationFrames = 4.0f;
constexpr float kSourceMaxSpeed = 23.0f;
constexpr float kSourceWalkChangeRate = 0.4f;
constexpr float kSourceRunChangeRate = 0.8f;
constexpr float kSourceMinimumWalkBlend = 0.7f;
constexpr float kSourceWaitPlayback = 1.0f;
constexpr float kSourceWalkPlayback = 0.75f;
constexpr float kSourceRunPlayback = 1.5f;
constexpr float kGroundingTolerance = 0.25f;
constexpr float kSourceLeg1Length = 30.0f;
constexpr float kSourceLeg2Length = 39.363499f;
constexpr std::uint16_t kBootsSubmesh = 15;
constexpr std::uint8_t kBodyPart = 1;
constexpr std::uint32_t kGuardBefore = 0xA17ECAFEu;
constexpr std::uint32_t kGuardAfter = 0x51C1B00Fu;

float length(float x, float z) {
    return std::sqrt(x * x + z * z);
}

float clamp_angle(float value) {
    while (value > kPi) {
        value -= 2.0f * kPi;
    }
    while (value < -kPi) {
        value += 2.0f * kPi;
    }
    return value;
}

Quat normalized(Quat value) {
    const float magnitude = std::sqrt(
        value.x * value.x + value.y * value.y +
        value.z * value.z + value.w * value.w);
    if (!(magnitude > 0.000001f) || !std::isfinite(magnitude)) {
        return {0.0f, 0.0f, 0.0f, 1.0f};
    }
    const float inverse = 1.0f / magnitude;
    return {
        value.x * inverse, value.y * inverse,
        value.z * inverse, value.w * inverse};
}

Transform mix(const Transform& a, const Transform& b, float amount) {
    Quat target = b.rotation;
    const float dot =
        a.rotation.x * target.x + a.rotation.y * target.y +
        a.rotation.z * target.z + a.rotation.w * target.w;
    if (dot < 0.0f) {
        target = {-target.x, -target.y, -target.z, -target.w};
    }
    return {
        {
            a.translation.x + (b.translation.x - a.translation.x) * amount,
            a.translation.y + (b.translation.y - a.translation.y) * amount,
            a.translation.z + (b.translation.z - a.translation.z) * amount,
        },
        normalized({
            a.rotation.x + (target.x - a.rotation.x) * amount,
            a.rotation.y + (target.y - a.rotation.y) * amount,
            a.rotation.z + (target.z - a.rotation.z) * amount,
            a.rotation.w + (target.w - a.rotation.w) * amount,
        }),
        {
            a.scale.x + (b.scale.x - a.scale.x) * amount,
            a.scale.y + (b.scale.y - a.scale.y) * amount,
            a.scale.z + (b.scale.z - a.scale.z) * amount,
        },
    };
}

Transform source_quat_lerp(
    const Transform& a, const Transform& b, float amount) {
    // JMAQuatLerp deliberately leaves the interpolated quaternion
    // unnormalised. mDoExt_MtxCalcAnmBlendTblOld stores that value for the
    // following old-frame morph; MTXQuat accounts for its magnitude only
    // when producing the matrix.
    Quat target = b.rotation;
    const float rotation_dot =
        a.rotation.x * target.x + a.rotation.y * target.y +
        a.rotation.z * target.z + a.rotation.w * target.w;
    if (rotation_dot < 0.0f) {
        target = {-target.x, -target.y, -target.z, -target.w};
    }
    return {
        {
            a.translation.x + (b.translation.x - a.translation.x) * amount,
            a.translation.y + (b.translation.y - a.translation.y) * amount,
            a.translation.z + (b.translation.z - a.translation.z) * amount,
        },
        {
            a.rotation.x + (target.x - a.rotation.x) * amount,
            a.rotation.y + (target.y - a.rotation.y) * amount,
            a.rotation.z + (target.z - a.rotation.z) * amount,
            a.rotation.w + (target.w - a.rotation.w) * amount,
        },
        {
            a.scale.x + (b.scale.x - a.scale.x) * amount,
            a.scale.y + (b.scale.y - a.scale.y) * amount,
            a.scale.z + (b.scale.z - a.scale.z) * amount,
        },
    };
}

Mat34 matrix(const Transform& transform) {
    const Quat q = normalized(transform.rotation);
    const float xx = q.x * q.x;
    const float yy = q.y * q.y;
    const float zz = q.z * q.z;
    const float xy = q.x * q.y;
    const float xz = q.x * q.z;
    const float yz = q.y * q.z;
    const float wx = q.w * q.x;
    const float wy = q.w * q.y;
    const float wz = q.w * q.z;
    return {{
        {
            (1.0f - 2.0f * (yy + zz)) * transform.scale.x,
            (2.0f * (xy - wz)) * transform.scale.y,
            (2.0f * (xz + wy)) * transform.scale.z,
            transform.translation.x,
        },
        {
            (2.0f * (xy + wz)) * transform.scale.x,
            (1.0f - 2.0f * (xx + zz)) * transform.scale.y,
            (2.0f * (yz - wx)) * transform.scale.z,
            transform.translation.y,
        },
        {
            (2.0f * (xz - wy)) * transform.scale.x,
            (2.0f * (yz + wx)) * transform.scale.y,
            (1.0f - 2.0f * (xx + yy)) * transform.scale.z,
            transform.translation.z,
        },
    }};
}

Mat34 multiply(const Mat34& a, const Mat34& b) {
    Mat34 result = {};
    for (std::uint32_t row = 0; row < 3; ++row) {
        for (std::uint32_t column = 0; column < 3; ++column) {
            for (std::uint32_t item = 0; item < 3; ++item) {
                result.value[row][column] +=
                    a.value[row][item] * b.value[item][column];
            }
        }
        result.value[row][3] = a.value[row][3];
        for (std::uint32_t item = 0; item < 3; ++item) {
            result.value[row][3] +=
                a.value[row][item] * b.value[item][3];
        }
    }
    return result;
}

void place_child_on_parent_x(
    AnimationState* animation, std::uint32_t parent,
    std::uint32_t child, float length) {
    const Mat34& source = animation->global[parent];
    Mat34& target = animation->global[child];
    target.value[0][3] = source.value[0][3] + source.value[0][0] * length;
    target.value[1][3] = source.value[1][3] + source.value[1][0] * length;
    target.value[2][3] = source.value[2][3] + source.value[2][0] * length;
}

void apply_source_foot_chain_lengths(AnimationState* animation) {
    // daAlink_c::setFootMatrix() is the final model callback after
    // footBgCheck(): contact correction happens first, then both chains are
    // rebuilt from the corrected joint bases with the source segment lengths.
    constexpr std::uint32_t hips[] = {18, 23};
    for (std::uint32_t hip : hips) {
        place_child_on_parent_x(animation, hip, hip + 1, 30.0f);
        place_child_on_parent_x(animation, hip + 1, hip + 2, 39.363499f);
        place_child_on_parent_x(animation, hip + 2, hip + 3, 14.18f);
    }
}

bool finite(const Mat34& value) {
    for (const auto& row : value.value) {
        for (float item : row) {
            if (!std::isfinite(item)) {
                return false;
            }
        }
    }
    return true;
}

bool finite(Vec3 value) {
    return std::isfinite(value.x) &&
           std::isfinite(value.y) &&
           std::isfinite(value.z);
}

Vec3 add(Vec3 a, Vec3 b) {
    return {a.x + b.x, a.y + b.y, a.z + b.z};
}

Vec3 subtract(Vec3 a, Vec3 b) {
    return {a.x - b.x, a.y - b.y, a.z - b.z};
}

Vec3 scale(Vec3 value, float amount) {
    return {value.x * amount, value.y * amount, value.z * amount};
}

float dot(Vec3 a, Vec3 b) {
    return a.x * b.x + a.y * b.y + a.z * b.z;
}

Vec3 cross(Vec3 a, Vec3 b) {
    return {
        a.y * b.z - a.z * b.y,
        a.z * b.x - a.x * b.z,
        a.x * b.y - a.y * b.x,
    };
}

float magnitude(Vec3 value) {
    return std::sqrt(dot(value, value));
}

Vec3 unit(Vec3 value, Vec3 fallback = {0.0f, 1.0f, 0.0f}) {
    const float value_magnitude = magnitude(value);
    if (!(value_magnitude > 0.000001f) ||
        !std::isfinite(value_magnitude)) {
        return fallback;
    }
    return scale(value, 1.0f / value_magnitude);
}

Vec3 translation(const Mat34& matrix_value) {
    return {
        matrix_value.value[0][3],
        matrix_value.value[1][3],
        matrix_value.value[2][3],
    };
}

Vec3 transform_point(const Mat34& matrix_value, Vec3 point) {
    return {
        matrix_value.value[0][0] * point.x +
            matrix_value.value[0][1] * point.y +
            matrix_value.value[0][2] * point.z +
            matrix_value.value[0][3],
        matrix_value.value[1][0] * point.x +
            matrix_value.value[1][1] * point.y +
            matrix_value.value[1][2] * point.z +
            matrix_value.value[1][3],
        matrix_value.value[2][0] * point.x +
            matrix_value.value[2][1] * point.y +
            matrix_value.value[2][2] * point.z +
            matrix_value.value[2][3],
    };
}

void update_skin_matrices(Runtime* runtime) {
    const std::uint8_t* model = runtime->packages.model.bytes;
    const std::uint32_t joint_offset = read_u32(model + 64);
    const std::uint32_t joint_stride = read_u32(model + 68);
    runtime->animation.matrices_finite = true;
    for (std::uint32_t joint = 0; joint < kPlayableJointCount; ++joint) {
        Mat34 inverse = {};
        const std::uint8_t* inverse_bytes =
            model + joint_offset + joint * joint_stride + 48;
        for (std::uint32_t row = 0; row < 3; ++row) {
            for (std::uint32_t column = 0; column < 4; ++column) {
                inverse.value[row][column] =
                    read_f32(inverse_bytes + (row * 4 + column) * 4);
            }
        }
        runtime->animation.skin[joint] =
            multiply(runtime->animation.global[joint], inverse);
        runtime->animation.matrices_finite =
            runtime->animation.matrices_finite &&
            finite(runtime->animation.global[joint]) &&
            finite(runtime->animation.skin[joint]);
    }
}

std::uint32_t leg_weight(
    const std::uint8_t* vertex, std::uint8_t first, std::uint8_t last) {
    std::uint32_t result = 0;
    for (std::uint32_t influence = 0; influence < 5; ++influence) {
        if (vertex[32 + influence] >= first &&
            vertex[32 + influence] <= last) {
            result += vertex[37 + influence];
        }
    }
    return result;
}

bool add_sole_vertex(
    PspLinkGroundingState* grounding,
    std::uint32_t side,
    std::uint16_t vertex) {
    for (std::uint16_t index = 0;
         index < grounding->sole_vertex_count[side];
         ++index) {
        if (grounding->sole_vertices[side][index] == vertex) {
            return true;
        }
    }
    if (grounding->sole_vertex_count[side] >= 64) {
        return false;
    }
    grounding->sole_vertices[side]
                            [grounding->sole_vertex_count[side]++] = vertex;
    return true;
}

bool derive_sole_sets(Runtime* runtime) {
    PspLinkGroundingState& grounding = runtime->grounding;
    const std::uint8_t* bytes = runtime->packages.model.bytes;
    const std::uint32_t vertex_offset = read_u32(bytes + 72);
    const std::uint32_t vertex_stride = read_u32(bytes + 76);
    const std::uint32_t index_offset = read_u32(bytes + 80);
    const std::uint32_t submesh_offset = read_u32(bytes + 84);
    const std::uint32_t submesh_stride = read_u32(bytes + 88);
    const std::uint32_t submesh_count = read_u32(bytes + 28);
    for (std::uint32_t submesh = 0; submesh < submesh_count; ++submesh) {
        const std::uint8_t* item =
            bytes + submesh_offset + submesh * submesh_stride;
        if (read_u16(item + 8) != kBootsSubmesh ||
            item[12] != kBodyPart) {
            continue;
        }
        const std::uint32_t first = read_u32(item);
        const std::uint32_t count = read_u32(item + 4);
        for (std::uint32_t index = 0; index < count; index += 3) {
            std::uint16_t triangle[3] = {};
            float normal_y = 0.0f;
            std::uint32_t sides[3] = {};
            for (std::uint32_t corner = 0; corner < 3; ++corner) {
                triangle[corner] = read_u16(
                    bytes + index_offset + (first + index + corner) * 2);
                const std::uint8_t* vertex =
                    bytes + vertex_offset + triangle[corner] * vertex_stride;
                normal_y += read_f32(vertex + 16);
                const std::uint32_t left =
                    leg_weight(vertex, 18, 21);
                const std::uint32_t right =
                    leg_weight(vertex, 23, 26);
                sides[corner] =
                    left == right ? 2u : left > right ? 0u : 1u;
            }
            if (normal_y / 3.0f > -0.35f ||
                sides[0] != sides[1] ||
                sides[1] != sides[2] ||
                sides[0] > 1) {
                continue;
            }
            for (std::uint16_t vertex : triangle) {
                if (!add_sole_vertex(
                        &grounding, sides[0], vertex)) {
                    return false;
                }
            }
        }
    }
    grounding.sole_sets_valid =
        grounding.sole_vertex_count[0] != 0 &&
        grounding.sole_vertex_count[1] != 0;
    return grounding.sole_sets_valid;
}

Vec3 skinned_position(
    const Runtime& runtime, std::uint16_t vertex_index) {
    const std::uint8_t* model = runtime.packages.model.bytes;
    const std::uint32_t vertex_offset = read_u32(model + 72);
    const std::uint32_t vertex_stride = read_u32(model + 76);
    const std::uint8_t* source =
        model + vertex_offset + vertex_index * vertex_stride;
    const Vec3 position = {
        read_f32(source), read_f32(source + 4), read_f32(source + 8)};
    Vec3 result = {};
    for (std::uint32_t influence = 0; influence < 5; ++influence) {
        const float weight = source[37 + influence] / 255.0f;
        if (weight == 0.0f) {
            continue;
        }
        const Vec3 transformed = transform_point(
            runtime.animation.skin[source[32 + influence]], position);
        result = add(result, scale(transformed, weight));
    }
    return result;
}

struct SoleSample {
    Vec3 point;
    float floor_y;
    float penetration;
    float hover;
};

float local_floor_y(
    const PspLinkGroundingInput& input,
    Vec3 local_normal,
    Vec3 point) {
    const float plane_y = input.floor_y - input.actor_position.y;
    return plane_y -
        (local_normal.x * point.x + local_normal.z * point.z) /
            local_normal.y;
}

SoleSample sample_sole(
    const Runtime& runtime, std::uint32_t side, Vec3 local_normal) {
    const PspLinkGroundingState& grounding = runtime.grounding;
    SoleSample result = {};
    result.point.y = INFINITY;
    for (std::uint16_t index = 0;
         index < grounding.sole_vertex_count[side];
         ++index) {
        const Vec3 point = skinned_position(
            runtime, grounding.sole_vertices[side][index]);
        const float floor =
            local_floor_y(grounding.input, local_normal, point);
        const float penetration = std::max(0.0f, floor - point.y);
        if (penetration > result.penetration ||
            (penetration == result.penetration &&
             point.y < result.point.y)) {
            result.point = point;
            result.floor_y = floor;
            result.penetration = penetration;
        }
    }
    result.hover = std::max(0.0f, result.point.y - result.floor_y);
    return result;
}

Vec3 source_foot_ground_sample(
    const Runtime& runtime, std::uint32_t side) {
    // daAlink_c::footBgCheck() samples the midpoint of two explicit model
    // offsets.  Its leg correction is the ground height at that XZ point
    // relative to the model base; it is not mesh-sole penetration.
    constexpr Vec3 foot_offsets[] = {
        {-3.0f, 13.0f, 0.0f}, {-3.0f, -13.0f, 0.0f}};
    constexpr Vec3 toe_offsets[] = {
        {10.0f, 5.0f, 0.0f}, {10.0f, -5.0f, 0.0f}};
    const std::uint32_t foot_joint = side == 0 ? 20 : 25;
    const Vec3 foot = transform_point(
        runtime.animation.global[foot_joint], foot_offsets[side]);
    const Vec3 toe = transform_point(
        runtime.animation.global[foot_joint + 1], toe_offsets[side]);
    return scale(add(foot, toe), 0.5f);
}

void rotation_between(Vec3 from, Vec3 to, float output[3][3]) {
    const Vec3 a = unit(from);
    const Vec3 b = unit(to);
    const float cosine = std::clamp(dot(a, b), -1.0f, 1.0f);
    Vec3 axis = cross(a, b);
    float sine = magnitude(axis);
    if (sine < 0.000001f) {
        if (cosine > 0.0f) {
            std::memset(output, 0, sizeof(float) * 9);
            output[0][0] = 1.0f;
            output[1][1] = 1.0f;
            output[2][2] = 1.0f;
            return;
        }
        axis = unit(cross(
            a,
            std::fabs(a.y) < 0.9f
                ? Vec3{0.0f, 1.0f, 0.0f}
                : Vec3{1.0f, 0.0f, 0.0f}));
        sine = 0.0f;
    } else {
        axis = scale(axis, 1.0f / sine);
    }
    const float one_minus_cosine = 1.0f - cosine;
    output[0][0] =
        cosine + axis.x * axis.x * one_minus_cosine;
    output[0][1] =
        axis.x * axis.y * one_minus_cosine - axis.z * sine;
    output[0][2] =
        axis.x * axis.z * one_minus_cosine + axis.y * sine;
    output[1][0] =
        axis.y * axis.x * one_minus_cosine + axis.z * sine;
    output[1][1] =
        cosine + axis.y * axis.y * one_minus_cosine;
    output[1][2] =
        axis.y * axis.z * one_minus_cosine - axis.x * sine;
    output[2][0] =
        axis.z * axis.x * one_minus_cosine - axis.y * sine;
    output[2][1] =
        axis.z * axis.y * one_minus_cosine + axis.x * sine;
    output[2][2] =
        cosine + axis.z * axis.z * one_minus_cosine;
}

Vec3 rotate(float rotation[3][3], Vec3 value) {
    return {
        rotation[0][0] * value.x +
            rotation[0][1] * value.y +
            rotation[0][2] * value.z,
        rotation[1][0] * value.x +
            rotation[1][1] * value.y +
            rotation[1][2] * value.z,
        rotation[2][0] * value.x +
            rotation[2][1] * value.y +
            rotation[2][2] * value.z,
    };
}

void rotate_chain(
    AnimationState* animation,
    std::uint32_t first,
    std::uint32_t last,
    Vec3 pivot,
    float rotation[3][3]) {
    for (std::uint32_t joint = first; joint <= last; ++joint) {
        Mat34& value = animation->global[joint];
        float basis[3][3] = {};
        for (std::uint32_t row = 0; row < 3; ++row) {
            for (std::uint32_t column = 0; column < 3; ++column) {
                for (std::uint32_t item = 0; item < 3; ++item) {
                    basis[row][column] +=
                        rotation[row][item] * value.value[item][column];
                }
            }
        }
        const Vec3 moved = add(
            pivot, rotate(rotation, subtract(translation(value), pivot)));
        for (std::uint32_t row = 0; row < 3; ++row) {
            for (std::uint32_t column = 0; column < 3; ++column) {
                value.value[row][column] = basis[row][column];
            }
        }
        value.value[0][3] = moved.x;
        value.value[1][3] = moved.y;
        value.value[2][3] = moved.z;
    }
}

void translate_chain(
    AnimationState* animation,
    std::uint32_t first,
    std::uint32_t last,
    Vec3 amount) {
    for (std::uint32_t joint = first; joint <= last; ++joint) {
        animation->global[joint].value[0][3] += amount.x;
        animation->global[joint].value[1][3] += amount.y;
        animation->global[joint].value[2][3] += amount.z;
    }
}

bool solve_source_leg(
    Runtime* runtime, std::uint32_t side, Vec3 correction) {
    const std::uint32_t hip_joint = side == 0 ? 18 : 23;
    const std::uint32_t knee_joint = hip_joint + 1;
    const std::uint32_t ankle_joint = hip_joint + 2;
    const std::uint32_t toe_joint = hip_joint + 3;
    AnimationState& animation = runtime->animation;
    const Vec3 hip = translation(animation.global[hip_joint]);
    const Vec3 old_knee = translation(animation.global[knee_joint]);
    const Vec3 old_ankle = translation(animation.global[ankle_joint]);
    const Vec3 target = add(old_ankle, correction);
    Vec3 direction = subtract(target, hip);
    float distance = magnitude(direction);
    if (!(distance > 0.000001f) || !std::isfinite(distance)) {
        return false;
    }
    const float minimum =
        std::fabs(kSourceLeg1Length - kSourceLeg2Length) + 0.001f;
    const float maximum =
        kSourceLeg1Length + kSourceLeg2Length - 0.001f;
    distance = std::clamp(distance, minimum, maximum);
    direction = unit(direction);
    const Vec3 plane_normal = unit(
        cross(subtract(old_knee, hip), subtract(old_ankle, hip)),
        {0.0f, 0.0f, side == 0 ? 1.0f : -1.0f});
    Vec3 perpendicular = unit(cross(plane_normal, direction));
    const float along =
        (kSourceLeg1Length * kSourceLeg1Length -
         kSourceLeg2Length * kSourceLeg2Length +
         distance * distance) /
        (2.0f * distance);
    const float height = std::sqrt(std::max(
        0.0f,
        kSourceLeg1Length * kSourceLeg1Length - along * along));
    Vec3 new_knee = add(
        add(hip, scale(direction, along)),
        scale(perpendicular, height));
    const Vec3 other_knee = add(
        add(hip, scale(direction, along)),
        scale(perpendicular, -height));
    if (magnitude(subtract(other_knee, old_knee)) <
        magnitude(subtract(new_knee, old_knee))) {
        new_knee = other_knee;
    }

    float hip_rotation[3][3] = {};
    rotation_between(
        subtract(old_knee, hip), subtract(new_knee, hip),
        hip_rotation);
    rotate_chain(
        &animation, hip_joint, toe_joint, hip, hip_rotation);
    translate_chain(
        &animation, knee_joint, toe_joint,
        subtract(new_knee, translation(animation.global[knee_joint])));

    const Vec3 rotated_ankle =
        translation(animation.global[ankle_joint]);
    float knee_rotation[3][3] = {};
    rotation_between(
        subtract(rotated_ankle, new_knee),
        subtract(target, new_knee),
        knee_rotation);
    rotate_chain(
        &animation, knee_joint, toe_joint, new_knee, knee_rotation);
    translate_chain(
        &animation, ankle_joint, toe_joint,
        subtract(target, translation(animation.global[ankle_joint])));
    return true;
}

Vec3 sole_centroid(
    const Runtime& runtime, std::uint32_t side) {
    Vec3 result = {};
    const std::uint16_t count =
        runtime.grounding.sole_vertex_count[side];
    for (std::uint16_t index = 0; index < count; ++index) {
        result = add(
            result,
            skinned_position(
                runtime,
                runtime.grounding.sole_vertices[side][index]));
    }
    return count == 0
        ? result
        : scale(result, 1.0f / static_cast<float>(count));
}

float horizontal_distance(Vec3 first, Vec3 second) {
    const float x = first.x - second.x;
    const float z = first.z - second.z;
    return std::sqrt(x * x + z * z);
}

void reset_idle_foot_lock(PspLinkGroundingState* grounding) {
    grounding->idle_anchor_valid[0] = false;
    grounding->idle_anchor_valid[1] = false;
    grounding->idle_previous_contact[0] = {};
    grounding->idle_previous_contact[1] = {};
}

void apply_idle_foot_lock(Runtime* runtime) {
    PspLinkGroundingState& grounding = runtime->grounding;
    PspLinkIdleFidelityMetrics& metrics =
        grounding.idle_fidelity;
    constexpr float kVisibleSlipTolerance = 0.25f;
    for (std::uint32_t side = 0; side < 2; ++side) {
        Vec3 current = sole_centroid(*runtime, side);
        if (!grounding.idle_anchor_valid[side]) {
            grounding.idle_contact_anchor[side] = current;
            grounding.idle_previous_contact[side] = current;
            grounding.idle_anchor_valid[side] = true;
        } else {
            const Vec3 anchor = grounding.idle_contact_anchor[side];
            // daAlink's setFootMatrix callback writes the ankle/foot matrices
            // after the BCK pose. Preserve that ordering for the small
            // horizontal contact correction without moving the actor or
            // pelvis. Vertical correction remains owned by setLegAngle's
            // two-bone solve above.
            const std::uint32_t ankle_joint = side == 0 ? 20 : 25;
            const std::uint32_t toe_joint = ankle_joint + 1;
            for (std::uint32_t iteration = 0;
                 iteration < 12; ++iteration) {
                const Vec3 residual = {
                    anchor.x - current.x,
                    0.0f,
                    anchor.z - current.z,
                };
                if (horizontal_distance(current, anchor) <=
                    kVisibleSlipTolerance * 0.1f) {
                    break;
                }
                translate_chain(
                    &runtime->animation,
                    ankle_joint, toe_joint, residual);
                update_skin_matrices(runtime);
                current = sole_centroid(*runtime, side);
            }
        }
        const float slip = horizontal_distance(
            current, grounding.idle_contact_anchor[side]);
        const float drift = horizontal_distance(
            current, grounding.idle_previous_contact[side]);
        float& maximum = side == 0
            ? metrics.left_foot_slip_max
            : metrics.right_foot_slip_max;
        float& total = side == 0
            ? metrics.left_contact_drift_total
            : metrics.right_contact_drift_total;
        std::uint32_t& planted = side == 0
            ? metrics.left_planted_frames
            : metrics.right_planted_frames;
        maximum = std::max(maximum, slip);
        total += drift;
        ++planted;
        grounding.idle_previous_contact[side] = current;
    }
    metrics.feet_contact_valid =
        grounding.idle_anchor_valid[0] &&
        grounding.idle_anchor_valid[1] &&
        metrics.left_foot_slip_max <= kVisibleSlipTolerance &&
        metrics.right_foot_slip_max <= kVisibleSlipTolerance;
    metrics.visual_glide_detected = !metrics.feet_contact_valid;
}

bool grounding_input_valid(const PspLinkGroundingInput& input) {
    return input.floor_valid &&
           std::isfinite(input.actor_position.x) &&
           std::isfinite(input.actor_position.y) &&
           std::isfinite(input.actor_position.z) &&
           std::isfinite(input.actor_yaw) &&
           std::isfinite(input.floor_y) &&
           std::isfinite(input.floor_normal.x) &&
           std::isfinite(input.floor_normal.y) &&
           std::isfinite(input.floor_normal.z) &&
           std::isfinite(input.capsule_bottom_y) &&
           input.floor_normal.y > 0.1f;
}

void apply_grounding(Runtime* runtime, Locomotion locomotion) {
    PspLinkGroundingState& grounding = runtime->grounding;
    if (!grounding.enabled) {
        return;
    }
    PspLinkGroundingMetrics& metrics = grounding.metrics;
    metrics.grounding_source_derived = true;
    metrics.actor_origin_y = grounding.input.actor_position.y;
    metrics.floor_y = grounding.input.floor_y;
    metrics.root_joint_y =
        grounding.input.actor_position.y +
        runtime->animation.global[0].value[1][3];
    metrics.pelvis_y =
        grounding.input.actor_position.y +
        runtime->animation.global[16].value[1][3];
    metrics.root_translation_double_applied =
        std::fabs(
            runtime->animation.local[0].translation.x -
            runtime->root_pose.metrics.animated_root_translation.x) >
                0.0001f ||
        std::fabs(
            runtime->animation.local[0].translation.z -
            runtime->root_pose.metrics.animated_root_translation.z) >
                0.0001f;
    if (!grounding.sole_sets_valid ||
        !grounding_input_valid(grounding.input)) {
        metrics.frame_valid = false;
        metrics.model_collision_vertical_parity = false;
        return;
    }
    if (grounding.last_collision_generation !=
        grounding.input.collision_generation) {
        grounding.feet[0] = {};
        grounding.feet[1] = {};
        grounding.last_collision_generation =
            grounding.input.collision_generation;
        reset_idle_foot_lock(&grounding);
    }
    grounding.feet[0].correction_y = 0.0f;
    grounding.feet[1].correction_y = 0.0f;
    const float sine = std::sin(grounding.input.actor_yaw);
    const float cosine = std::cos(grounding.input.actor_yaw);
    const Vec3 world_normal = unit(grounding.input.floor_normal);
    const Vec3 local_normal = {
        cosine * world_normal.x - sine * world_normal.z,
        world_normal.y,
        sine * world_normal.x + cosine * world_normal.z,
    };
    for (std::uint32_t side = 0; side < 2; ++side) {
        const Vec3 sample = source_foot_ground_sample(*runtime, side);
        const float correction = local_floor_y(
            grounding.input, local_normal, sample);
        if (std::fabs(correction) >= 0.1f &&
            solve_source_leg(
                runtime, side, {0.0f, correction, 0.0f})) {
            ++metrics.source_solver_iterations;
            grounding.feet[side].correction_y = correction;
            update_skin_matrices(runtime);
        }
    }
    if (locomotion == Locomotion::Idle) {
        apply_idle_foot_lock(runtime);
        for (std::uint32_t side = 0; side < 2; ++side) {
            for (std::uint32_t iteration = 0;
                 iteration < 4; ++iteration) {
                const SoleSample sample =
                    sample_sole(*runtime, side, local_normal);
                if (sample.penetration <= kGroundingTolerance) {
                    break;
                }
                if (!solve_source_leg(
                        runtime, side,
                        {0.0f, sample.penetration, 0.0f})) {
                    break;
                }
                update_skin_matrices(runtime);
            }
        }
    } else {
        reset_idle_foot_lock(&grounding);
    }
    const SoleSample left = sample_sole(*runtime, 0, local_normal);
    const SoleSample right = sample_sole(*runtime, 1, local_normal);
    grounding.feet[0] = {
        left.point, left.floor_y, grounding.feet[0].correction_y,
        left.penetration, left.hover, true};
    grounding.feet[1] = {
        right.point, right.floor_y, grounding.feet[1].correction_y,
        right.penetration, right.hover, true};
    metrics.left_ankle_y =
        grounding.input.actor_position.y +
        runtime->animation.global[20].value[1][3];
    metrics.right_ankle_y =
        grounding.input.actor_position.y +
        runtime->animation.global[25].value[1][3];
    metrics.left_sole_min_y =
        grounding.input.actor_position.y + left.point.y;
    metrics.right_sole_min_y =
        grounding.input.actor_position.y + right.point.y;
    metrics.left_foot_penetration_max = std::max(
        metrics.left_foot_penetration_max, left.penetration);
    metrics.right_foot_penetration_max = std::max(
        metrics.right_foot_penetration_max, right.penetration);
    metrics.left_foot_hover_max = std::max(
        metrics.left_foot_hover_max, left.hover);
    metrics.right_foot_hover_max = std::max(
        metrics.right_foot_hover_max, right.hover);
    // The source contract does not classify arbitrary boot-mesh vertices as
    // leg penetration.  footBgCheck samples its explicit foot/toe offsets and
    // keeps collision parity at the actor capsule/base-matrix boundary.
    if (metrics.pelvis_y < grounding.input.floor_y) {
        ++metrics.pelvis_below_floor_frames;
    }
    const float capsule_error =
        grounding.input.floor_y - grounding.input.capsule_bottom_y;
    if (capsule_error > kGroundingTolerance) {
        ++metrics.collision_bottom_below_floor_frames;
    }
    if (grounding.feet[0].correction_y != 0.0f ||
        grounding.feet[1].correction_y != 0.0f) {
        ++metrics.corrected_frames;
    }
    metrics.model_collision_vertical_parity =
        std::fabs(capsule_error) <= kGroundingTolerance;
    metrics.frame_valid =
        metrics.model_collision_vertical_parity &&
        !metrics.root_translation_double_applied &&
        metrics.pelvis_y >= grounding.input.floor_y &&
        runtime->animation.matrices_finite;
}

Transform read_transform(const std::uint8_t* bytes) {
    return {
        {read_f32(bytes), read_f32(bytes + 4), read_f32(bytes + 8)},
        {
            read_f32(bytes + 12), read_f32(bytes + 16),
            read_f32(bytes + 20), read_f32(bytes + 24),
        },
        {read_f32(bytes + 28), read_f32(bytes + 32), read_f32(bytes + 36)},
    };
}

std::uint32_t clip_for(Locomotion locomotion) {
    switch (locomotion) {
    case Locomotion::Idle: return 0;
    case Locomotion::Walk: return 1;
    case Locomotion::Run: return 2;
    case Locomotion::TurnInPlace: return 3;
    }
    return 0;
}

float clip_frame_max(
    const PackageView& package, Locomotion locomotion) {
    const std::uint32_t table = read_u32(package.bytes + 32);
    const std::uint32_t stride = read_u32(package.bytes + 36);
    const std::uint8_t* clip =
        package.bytes + table + clip_for(locomotion) * stride;
    return static_cast<float>(read_u32(clip + 12) - 1u);
}

void sample_clip(
    const PackageView& package,
    Locomotion locomotion,
    float frame,
    Transform* output) {
    const std::uint8_t* bytes = package.bytes;
    const std::uint32_t table = read_u32(bytes + 32);
    const std::uint32_t stride = read_u32(bytes + 36);
    const std::uint8_t* clip =
        bytes + table + clip_for(locomotion) * stride;
    const std::uint32_t sample_count = read_u32(clip + 12);
    const std::uint32_t offset = read_u32(clip + 24);
    const float wrapped = std::fmod(
        std::max(frame, 0.0f), static_cast<float>(sample_count - 1));
    const std::uint32_t first =
        static_cast<std::uint32_t>(std::floor(wrapped));
    const std::uint32_t second =
        (first + 1) % (sample_count - 1);
    const float amount = wrapped - static_cast<float>(first);
    for (std::uint32_t joint = 0; joint < kPlayableJointCount; ++joint) {
        const Transform a = read_transform(
            bytes + offset + (first * kPlayableJointCount + joint) * 40);
        const Transform b = read_transform(
            bytes + offset + (second * kPlayableJointCount + joint) * 40);
        output[joint] = mix(a, b, amount);
    }
}


bool sample_clip_resource(
    const PackageView& package,
    std::uint32_t resource_id,
    float frame,
    Transform* output) {
    if (package.bytes == nullptr || output == nullptr ||
        !std::isfinite(frame) || frame < 0.0f) {
        return false;
    }
    const std::uint32_t clips = read_u32(package.bytes + 16);
    const std::uint32_t table = read_u32(package.bytes + 32);
    const std::uint32_t stride = read_u32(package.bytes + 36);
    const std::uint8_t* clip = nullptr;
    for (std::uint32_t index = 0; index < clips; ++index) {
        const std::uint8_t* candidate =
            package.bytes + table + index * stride;
        if (read_u32(candidate) == resource_id) {
            clip = candidate;
            break;
        }
    }
    if (clip == nullptr || read_u32(clip + 16) != kPlayableJointCount) {
        return false;
    }
    const std::uint32_t samples = read_u32(clip + 12);
    const std::uint32_t offset = read_u32(clip + 24);
    if (samples < 2) return false;
    const float clamped = std::min(
        frame, static_cast<float>(samples - 1u));
    const std::uint32_t first =
        static_cast<std::uint32_t>(std::floor(clamped));
    const std::uint32_t second =
        first + 1u < samples ? first + 1u : first;
    const float amount = clamped - static_cast<float>(first);
    for (std::uint32_t joint = 0; joint < kPlayableJointCount; ++joint) {
        const Transform a = read_transform(
            package.bytes + offset +
            (first * kPlayableJointCount + joint) * 40);
        const Transform b = read_transform(
            package.bytes + offset +
            (second * kPlayableJointCount + joint) * 40);
        output[joint] = mix(a, b, amount);
    }
    return true;
}

Vec3 bind_root_translation(const PackageView& model) {
    const std::uint32_t joint_offset = read_u32(model.bytes + 64);
    return {
        read_f32(model.bytes + joint_offset + 8),
        read_f32(model.bytes + joint_offset + 12),
        read_f32(model.bytes + joint_offset + 16),
    };
}

bool apply_root_pose_policy(
    Runtime* runtime, Transform* pose) {
    PspLinkRootPoseMetrics& metrics = runtime->root_pose.metrics;
    const Vec3 bind = bind_root_translation(runtime->packages.model);
    const Vec3 animated = pose[0].translation;
    if (!finite(bind) || !finite(animated)) {
        metrics.frame_valid = false;
        return false;
    }
    const Vec3 delta = subtract(animated, bind);
    metrics.bind_root_translation = bind;
    metrics.animated_root_translation = animated;
    metrics.root_delta = delta;
    metrics.root_anchor_source_derived = true;
    metrics.root_horizontal_motion_removed = false;
    metrics.root_horizontal_motion_preserved = true;
    return finite(pose[0].translation);
}

void track_root_range(
    float value, float* minimum, float* maximum) {
    *minimum = std::min(*minimum, value);
    *maximum = std::max(*maximum, value);
}

void update_root_pose_metrics(
    Runtime* runtime,
    Locomotion locomotion,
    Vec3 actor_origin_before_pose) {
    PspLinkRootPoseMetrics& metrics =
        runtime->root_pose.metrics;
    const Vec3 final_root = translation(runtime->animation.global[0]);
    const float pelvis_y = runtime->animation.global[16].value[1][3];
    metrics.final_root_translation = final_root;
    const bool reference_valid =
        finite(final_root) &&
        std::fabs(
            final_root.x - metrics.animated_root_translation.x) <=
                0.0001f &&
        std::fabs(
            final_root.z - metrics.animated_root_translation.z) <=
                0.0001f;
    metrics.root_horizontal_motion_double_applied = !reference_valid;
    const bool feet_grounded =
        runtime->grounding.enabled &&
        runtime->grounding.metrics.frame_valid;
    if (locomotion == Locomotion::Idle) {
        metrics.idle_observed = true;
        track_root_range(
            final_root.y,
            &metrics.idle_root_translation_min,
            &metrics.idle_root_translation_max);
        track_root_range(
            pelvis_y,
            &metrics.idle_pelvis_translation_min,
            &metrics.idle_pelvis_translation_max);
        if (runtime->grounding.enabled) {
            const Vec3 actor =
                runtime->grounding.input.actor_position;
            metrics.idle_actor_origin_stable =
                metrics.idle_actor_origin_stable &&
                std::fabs(actor.x - actor_origin_before_pose.x) <=
                    0.0001f &&
                std::fabs(actor.y - actor_origin_before_pose.y) <=
                    0.0001f &&
                std::fabs(actor.z - actor_origin_before_pose.z) <=
                    0.0001f;
            metrics.idle_feet_grounded =
                metrics.idle_feet_grounded && feet_grounded;
        }
        metrics.idle_root_reference_valid =
            metrics.idle_root_reference_valid && reference_valid;
        metrics.idle_pelvis_motion_preserved =
            metrics.idle_pelvis_translation_max >
            metrics.idle_pelvis_translation_min;
    } else if (locomotion == Locomotion::Walk) {
        metrics.walk_observed = true;
        track_root_range(
            final_root.y,
            &metrics.walk_root_translation_min,
            &metrics.walk_root_translation_max);
        metrics.walk_root_reference_valid =
            metrics.walk_root_reference_valid && reference_valid;
    } else if (locomotion == Locomotion::Run) {
        metrics.run_observed = true;
        track_root_range(
            final_root.y,
            &metrics.run_root_translation_min,
            &metrics.run_root_translation_max);
        metrics.run_root_reference_valid =
            metrics.run_root_reference_valid && reference_valid;
    }
    metrics.collision_model_origin_parity =
        runtime->grounding.metrics.model_collision_vertical_parity;
    const bool current_reference_valid =
        locomotion == Locomotion::Idle
            ? metrics.idle_root_reference_valid
            : locomotion == Locomotion::Walk
                ? metrics.walk_root_reference_valid
                : locomotion == Locomotion::Run
                    ? metrics.run_root_reference_valid
                    : reference_valid;
    metrics.frame_valid =
        metrics.root_anchor_source_derived &&
        metrics.root_horizontal_motion_preserved &&
        !metrics.root_horizontal_motion_removed &&
        !metrics.root_horizontal_motion_double_applied &&
        current_reference_valid &&
        (!runtime->grounding.enabled ||
         (feet_grounded &&
          metrics.collision_model_origin_parity)) &&
        runtime->animation.matrices_finite;
}

bool blocked(float x, float z) {
    constexpr float obstacles[4][4] = {
        {-5.0f, -3.5f, -1.5f, 0.0f},
        {2.5f, 4.5f, -4.0f, -2.0f},
        {-1.0f, 1.0f, 2.5f, 4.5f},
        {4.5f, 6.0f, 3.5f, 5.5f},
    };
    constexpr float radius = 0.35f;
    for (const auto& obstacle : obstacles) {
        if (x + radius > obstacle[0] && x - radius < obstacle[1] &&
            z + radius > obstacle[2] && z - radius < obstacle[3]) {
            return true;
        }
    }
    return false;
}

bool finish_animation_pose(
    Runtime* runtime, Locomotion locomotion,
    const Vec3& actor_origin_before_pose, Transform* target) {
    AnimationState& state = runtime->animation;
    if (!apply_root_pose_policy(runtime, target)) {
        return false;
    }
    if (state.blend_frame > 0.0f) {
        const float amount = 1.0f / state.blend_frame;
        for (std::uint32_t joint = 0;
             joint < kPlayableJointCount; ++joint) {
            state.local[joint] = state.source_blend_active
                ? source_quat_lerp(
                      state.blend_from[joint], target[joint], amount)
                : mix(state.blend_from[joint], target[joint], amount);
            state.blend_from[joint] = state.local[joint];
        }
        state.blend_frame = std::max(
            0.0f, state.blend_frame - 1.0f);
    } else {
        std::memcpy(state.local, target, sizeof(state.local));
    }
    runtime->root_pose.metrics.animated_root_translation =
        state.local[0].translation;
    runtime->root_pose.metrics.root_delta = subtract(
        state.local[0].translation,
        runtime->root_pose.metrics.bind_root_translation);
    const std::uint8_t* model = runtime->packages.model.bytes;
    const std::uint32_t joint_offset = read_u32(model + 64);
    const std::uint32_t joint_stride = read_u32(model + 68);
    state.matrices_finite = true;
    for (std::uint32_t joint = 0;
         joint < kPlayableJointCount; ++joint) {
        const Mat34 local = matrix(state.local[joint]);
        const std::int16_t parent =
            read_s16(model + joint_offset + joint * joint_stride);
        state.global[joint] =
            parent < 0 ? local : multiply(state.global[parent], local);
        state.matrices_finite =
            state.matrices_finite && finite(state.global[joint]);
    }
    update_skin_matrices(runtime);
    apply_grounding(runtime, locomotion);
    apply_source_foot_chain_lengths(&state);
    update_skin_matrices(runtime);
    const Vec3 source_feet[2] = {
        translation(state.global[20]),
        translation(state.global[25]),
    };
    if (state.source_feet_valid) {
        const std::uint32_t lower =
            source_feet[0].y < source_feet[1].y ? 0u : 1u;
        const Vec3 delta = subtract(
            source_feet[lower], state.source_previous_feet[lower]);
        // current.angle.y and shape_angle.y share the same source value in
        // the supported Link runtime, reducing setFootSpeed to |delta.z|.
        state.source_foot_motion_raw = std::fabs(delta.z);
    } else {
        state.source_foot_motion_raw = 0.0f;
        state.source_feet_valid = true;
    }
    state.source_previous_feet[0] = source_feet[0];
    state.source_previous_feet[1] = source_feet[1];
    update_root_pose_metrics(
        runtime, locomotion, actor_origin_before_pose);
    runtime->active_buffer ^= 1u;
    SkinnedVertex* output = runtime->vertices[runtime->active_buffer];
    const std::uint32_t vertex_offset = read_u32(model + 72);
    const std::uint32_t vertex_stride = read_u32(model + 76);
    runtime->vertices_finite = true;
    for (std::uint32_t index = 0; index < runtime->vertex_count; ++index) {
        const std::uint8_t* source =
            model + vertex_offset + index * vertex_stride;
        const Vec3 position = {
            read_f32(source), read_f32(source + 4), read_f32(source + 8)};
        const Vec3 normal = {
            read_f32(source + 12),
            read_f32(source + 16), read_f32(source + 20)};
        Vec3 p = {};
        Vec3 n = {};
        for (std::uint32_t influence = 0; influence < 5; ++influence) {
            const float weight = source[37 + influence] / 255.0f;
            if (weight == 0.0f) {
                continue;
            }
            const Mat34& skin = state.skin[source[32 + influence]];
            p.x += weight * (
                skin.value[0][0] * position.x +
                skin.value[0][1] * position.y +
                skin.value[0][2] * position.z + skin.value[0][3]);
            p.y += weight * (
                skin.value[1][0] * position.x +
                skin.value[1][1] * position.y +
                skin.value[1][2] * position.z + skin.value[1][3]);
            p.z += weight * (
                skin.value[2][0] * position.x +
                skin.value[2][1] * position.y +
                skin.value[2][2] * position.z + skin.value[2][3]);
            n.x += weight * (
                skin.value[0][0] * normal.x +
                skin.value[0][1] * normal.y +
                skin.value[0][2] * normal.z);
            n.y += weight * (
                skin.value[1][0] * normal.x +
                skin.value[1][1] * normal.y +
                skin.value[1][2] * normal.z);
            n.z += weight * (
                skin.value[2][0] * normal.x +
                skin.value[2][1] * normal.y +
                skin.value[2][2] * normal.z);
        }
        const float normal_length = std::max(
            std::sqrt(n.x * n.x + n.y * n.y + n.z * n.z),
            0.000001f);
        n = {n.x / normal_length, n.y / normal_length,
             n.z / normal_length};
        output[index] = {
            read_f32(source + 24), read_f32(source + 28),
            0xffffffffu, n.x, n.y, n.z, p.x, p.y, p.z};
        runtime->vertices_finite =
            runtime->vertices_finite &&
            std::isfinite(p.x) && std::isfinite(p.y) &&
            std::isfinite(p.z) && std::isfinite(n.x) &&
            std::isfinite(n.y) && std::isfinite(n.z);
    }
    runtime->skinned_vertices += runtime->vertex_count;
    runtime->guards_valid =
        runtime->guard_before == kGuardBefore &&
        runtime->guard_after == kGuardAfter;
    return state.matrices_finite && runtime->vertices_finite &&
           runtime->guards_valid;
}

}  // namespace

std::uint32_t active_animation_resource_id(const Runtime& runtime) {
    const PackageView& package = runtime.packages.animations;
    const std::uint32_t table = read_u32(package.bytes + 32);
    const std::uint32_t stride = read_u32(package.bytes + 36);
    return read_u32(
        package.bytes + table +
        clip_for(runtime.animation.current) * stride);
}

float active_animation_source_frame(const Runtime& runtime) {
    const PackageView& package = runtime.packages.animations;
    const std::uint32_t table = read_u32(package.bytes + 32);
    const std::uint32_t stride = read_u32(package.bytes + 36);
    const std::uint8_t* clip =
        package.bytes + table +
        clip_for(runtime.animation.current) * stride;
    const std::uint32_t sample_count = read_u32(clip + 12);
    if (sample_count <= 1) {
        return 0.0f;
    }
    // The ordinary source J3D controller is observed after its execute
    // advance; the PSP pose sampler runs one checkpoint earlier. PROC_WAIT_TURN
    // selects its clip after that advance, so its selected frame is already
    // the source-observed frame.
    const float observation_offset =
        runtime.animation.current == Locomotion::TurnInPlace ? 0.0f : 1.0f;
    return std::fmod(
        std::max(runtime.animation.clip_frame + observation_offset, 0.0f),
        static_cast<float>(sample_count - 1));
}

float active_animation_playback_rate(const Runtime& runtime) {
    return runtime.animation.playback_rate;
}

void reset_gameplay(GameplayState* state) {
    if (state == nullptr) {
        return;
    }
    const std::uint32_t updates = state->updates;
    const std::uint32_t transitions = state->transitions;
    const std::uint32_t idle_frames = state->idle_frames;
    const std::uint32_t walk_frames = state->walk_frames;
    const std::uint32_t run_frames = state->run_frames;
    const std::uint32_t collected = state->collected;
    const std::uint32_t pedestal_actions = state->pedestal_actions;
    *state = {};
    state->mode = GameMode::Playing;
    state->locomotion = Locomotion::Idle;
    state->camera_distance = 5.0f;
    state->hearts = 3;
    state->updates = updates;
    state->transitions = transitions;
    state->idle_frames = idle_frames;
    state->walk_frames = walk_frames;
    state->run_frames = run_frames;
    state->collected = collected;
    state->pedestal_actions = pedestal_actions;
    constexpr Vec3 positions[kPlayableMaxRubies] = {
        {-1.0f, 0.35f, -1.0f},
        {-2.5f, 0.35f, -2.5f},
        {-4.0f, 0.35f, -4.0f},
        {4.0f, 0.35f, -8.0f},
        {0.0f, 0.35f, -0.5f},
    };
    for (std::uint32_t index = 0; index < kPlayableMaxRubies; ++index) {
        state->rubies[index] = {positions[index], true};
    }
}

void update_gameplay(
    GameplayState* state, const Input& input, float delta_seconds) {
    if (state == nullptr || state->mode == GameMode::Exiting) {
        return;
    }
    const float dt = std::clamp(delta_seconds, 0.0f, 1.0f / 15.0f);
    if (state->mode == GameMode::Paused) {
        if (input.up_pressed) {
            state->pause_selection =
                (state->pause_selection + 2) % 3;
        }
        if (input.down_pressed) {
            state->pause_selection =
                (state->pause_selection + 1) % 3;
        }
        if (input.cancel_pressed || input.pause_pressed ||
            (input.action_pressed && state->pause_selection == 0)) {
            state->mode = GameMode::Playing;
        } else if (input.action_pressed && state->pause_selection == 1) {
            reset_gameplay(state);
        } else if (input.action_pressed && state->pause_selection == 2) {
            state->mode = GameMode::Exiting;
        }
        ++state->updates;
        return;
    }
    if (input.pause_pressed) {
        state->mode = GameMode::Paused;
        state->pause_selection = 0;
        ++state->updates;
        return;
    }
    if (input.cancel_pressed) {
        state->camera_yaw = 0.0f;
    }
    if (input.debug_pressed) {
        state->debug_visible = !state->debug_visible;
    }
    state->camera_yaw +=
        (input.camera_right ? 1.0f : 0.0f) * dt * 1.8f -
        (input.camera_left ? 1.0f : 0.0f) * dt * 1.8f;
    state->camera_distance = std::clamp(
        state->camera_distance +
            (input.zoom_out ? dt * 3.0f : 0.0f) -
            (input.zoom_in ? dt * 3.0f : 0.0f),
        4.0f, 9.0f);
    float x = std::clamp(input.analog_x, -1.0f, 1.0f);
    float y = std::clamp(input.analog_y, -1.0f, 1.0f);
    const float magnitude = std::min(length(x, y), 1.0f);
    Locomotion next = Locomotion::Idle;
    if (magnitude >= kRunThreshold) {
        next = Locomotion::Run;
    } else if (magnitude >= kWalkThreshold) {
        next = Locomotion::Walk;
    }
    if (next != state->locomotion) {
        state->locomotion = next;
        ++state->transitions;
    }
    if (next == Locomotion::Idle) {
        ++state->idle_frames;
    } else if (next == Locomotion::Walk) {
        ++state->walk_frames;
    } else {
        ++state->run_frames;
    }
    if (magnitude > 0.0001f) {
        x /= magnitude;
        y /= magnitude;
        const float sine = std::sin(state->camera_yaw);
        const float cosine = std::cos(state->camera_yaw);
        const float direction_x = x * cosine - y * sine;
        const float direction_z = x * sine + y * cosine;
        const float speed =
            (next == Locomotion::Run ? kRunSpeed : kWalkSpeed) * magnitude;
        const float candidate_x = std::clamp(
            state->position.x + direction_x * speed * dt, -9.3f, 9.3f);
        const float candidate_z = std::clamp(
            state->position.z + direction_z * speed * dt, -9.3f, 9.3f);
        if (!blocked(candidate_x, state->position.z)) {
            state->position.x = candidate_x;
        }
        if (!blocked(state->position.x, candidate_z)) {
            state->position.z = candidate_z;
        }
        const float target = std::atan2(direction_x, direction_z);
        const float difference = clamp_angle(target - state->yaw);
        state->yaw += std::clamp(difference, -dt * 7.0f, dt * 7.0f);
    }
    for (Ruby& ruby : state->rubies) {
        if (ruby.active &&
            length(
                ruby.position.x - state->position.x,
                ruby.position.z - state->position.z) < 0.65f) {
            ruby.active = false;
            ++state->rupees;
            ++state->collected;
        }
    }
    state->action_prompt =
        length(state->position.x + 8.7f, state->position.z + 8.7f) < 1.25f;
    if (state->action_prompt && input.action_pressed) {
        ++state->pedestal_actions;
    }
    ++state->updates;
}

Input replay_input(std::uint32_t update) {
    Input input = {};
    if (update >= 120 && update < 300) {
        input.analog_x = -0.42f;
        input.analog_y = -0.42f;
    } else if (update >= 300 && update < 480) {
        input.analog_x = 0.9f;
        input.analog_y = -0.45f;
    } else if (update >= 480 && update < 570) {
        input.camera_right = true;
        input.analog_y = -0.55f;
    } else if (update >= 570 && update < 660) {
        input.analog_x = -0.5f;
        input.analog_y = 0.1f;
    }
    if (update == 720) input.pause_pressed = true;
    if (update == 722) input.down_pressed = true;
    if (update == 724) input.cancel_pressed = true;
    if (update == 760) input.pause_pressed = true;
    if (update == 762) input.down_pressed = true;
    if (update == 764) input.action_pressed = true;
    if (update >= 800 && update < 980) {
        input.analog_x = -0.7f;
        input.analog_y = -0.7f;
    }
    if (update == 1000) input.action_pressed = true;
    return input;
}

bool playable_mode_name_valid(const char* name) {
    return name != nullptr &&
           (std::strcmp(name, "smoke") == 0 ||
            std::strcmp(name, "replay") == 0 ||
            std::strcmp(name, "interactive") == 0);
}

bool gameplay_state_consistent(const GameplayState& state) {
    return static_cast<std::uint32_t>(state.mode) <=
               static_cast<std::uint32_t>(GameMode::Exiting) &&
           static_cast<std::uint32_t>(state.locomotion) <=
               static_cast<std::uint32_t>(Locomotion::Run) &&
           std::isfinite(state.position.x) &&
           std::isfinite(state.position.y) &&
           std::isfinite(state.position.z) &&
           std::isfinite(state.yaw) &&
           std::isfinite(state.camera_yaw) &&
           std::isfinite(state.camera_distance) &&
           state.position.x >= -9.3f && state.position.x <= 9.3f &&
           state.position.z >= -9.3f && state.position.z <= 9.3f &&
           state.camera_distance >= 4.0f &&
           state.camera_distance <= 9.0f &&
           state.rupees <= kPlayableMaxRubies &&
           state.rupees <= state.collected &&
           state.hearts == 3 &&
           state.pause_selection < 3;
}

bool replay_state_complete(const GameplayState& state) {
    return gameplay_state_consistent(state) &&
           state.transitions >= 4 &&
           state.idle_frames != 0 &&
           state.walk_frames != 0 &&
           state.run_frames != 0 &&
           state.collected >= 4 &&
           state.pedestal_actions != 0 &&
           state.mode == GameMode::Playing;
}

bool initialize_runtime(Runtime* runtime, const PackageSet& packages) {
    if (runtime == nullptr ||
        validate_package_set(packages) != PackageError::Ok) {
        return false;
    }
    std::memset(runtime, 0, sizeof(*runtime));
    runtime->packages = packages;
    runtime->vertex_count = read_u32(packages.model.bytes + 20);
    if (runtime->vertex_count == 0 ||
        runtime->vertex_count > kPlayableMaxVertices) {
        return false;
    }
    runtime->guard_before = kGuardBefore;
    runtime->guard_after = kGuardAfter;
    runtime->animation.current = Locomotion::Idle;
    runtime->animation.secondary = Locomotion::Idle;
    runtime->animation.playback_rate = kSourceWaitPlayback;
    runtime->animation.matrices_finite = true;
    PspLinkRootPoseMetrics& root_metrics =
        runtime->root_pose.metrics;
    const float maximum = std::numeric_limits<float>::max();
    root_metrics.idle_root_translation_min = maximum;
    root_metrics.idle_root_translation_max = -maximum;
    root_metrics.idle_pelvis_translation_min = maximum;
    root_metrics.idle_pelvis_translation_max = -maximum;
    root_metrics.walk_root_translation_min = maximum;
    root_metrics.walk_root_translation_max = -maximum;
    root_metrics.run_root_translation_min = maximum;
    root_metrics.run_root_translation_max = -maximum;
    root_metrics.idle_actor_origin_stable = true;
    root_metrics.idle_root_reference_valid = true;
    root_metrics.idle_feet_grounded = true;
    root_metrics.walk_root_reference_valid = true;
    root_metrics.run_root_reference_valid = true;
    if (!derive_sole_sets(runtime)) {
        return false;
    }
    sample_clip(
        packages.animations, Locomotion::Idle, 0.0f,
        runtime->animation.local);
    if (!apply_root_pose_policy(
            runtime, runtime->animation.local)) {
        return false;
    }
    std::memcpy(
        runtime->animation.blend_from,
        runtime->animation.local,
        sizeof(runtime->animation.local));
    return update_animation_and_skin(
        runtime, Locomotion::Idle, 0.0f, 0.0f);
}

bool update_animation_and_skin(
    Runtime* runtime,
    Locomotion locomotion,
    float speed,
    float delta_seconds) {
    if (runtime == nullptr) {
        return false;
    }
    const Vec3 actor_origin_before_pose =
        runtime->grounding.input.actor_position;
    AnimationState& state = runtime->animation;
    const bool locomotion_changed = locomotion != state.current;
    state.source_blend_active = false;
    if (locomotion_changed) {
        std::memcpy(
            state.blend_from, state.local, sizeof(state.local));
        state.current = locomotion;
        state.clip_frame = 0.0f;
        state.blend_frame = kBasicInterpolationFrames;
        ++state.transitions;
    }
    const float playback =
        locomotion == Locomotion::Idle ? 1.0f :
        locomotion == Locomotion::Walk ? 0.8f + speed * 0.2f :
        locomotion == Locomotion::Run ? 1.0f + speed * 0.15f :
        0.7f;
    state.playback_rate = playback;
    if (!(locomotion_changed && locomotion == Locomotion::TurnInPlace)) {
        state.clip_frame +=
            std::clamp(delta_seconds, 0.0f, 1.0f / 15.0f) *
            30.0f * playback;
    }
    Transform target[kPlayableJointCount] = {};
    sample_clip(
        runtime->packages.animations, locomotion,
        state.clip_frame, target);
    return finish_animation_pose(
        runtime, locomotion, actor_origin_before_pose, target);
}

bool update_source_locomotion_and_skin(
    Runtime* runtime, float normal_speed, float delta_seconds) {
    if (runtime == nullptr || !std::isfinite(normal_speed)) {
        return false;
    }
    AnimationState& state = runtime->animation;
    const Vec3 actor_origin_before_pose =
        runtime->grounding.input.actor_position;
    const float frame_scale =
        std::clamp(delta_seconds, 0.0f, 1.0f / 15.0f) * 30.0f;
    state.clip_frame += state.playback_rate * frame_scale;

    const float rate = std::clamp(
        std::fabs(normal_speed) / kSourceMaxSpeed, 0.0f, 1.0f);
    Locomotion primary = Locomotion::Idle;
    Locomotion secondary = Locomotion::Walk;
    float blend = 0.0f;
    float playback_a = kSourceWaitPlayback;
    float playback_b = kSourceWalkPlayback;
    if (normal_speed == 0.0f) {
        secondary = Locomotion::Idle;
        playback_b = kSourceWaitPlayback;
    } else if (rate < kSourceWalkChangeRate) {
        const float raw = rate / kSourceWalkChangeRate;
        blend = kSourceMinimumWalkBlend +
            raw * (1.0f - kSourceMinimumWalkBlend);
    } else if (rate < kSourceRunChangeRate) {
        primary = Locomotion::Walk;
        secondary = Locomotion::Run;
        blend = (rate - kSourceWalkChangeRate) /
            (kSourceRunChangeRate - kSourceWalkChangeRate);
        playback_a = kSourceWalkPlayback;
        playback_b = kSourceRunPlayback;
    } else {
        primary = Locomotion::Run;
        secondary = Locomotion::Run;
        blend = 1.0f;
        playback_a = kSourceRunPlayback;
        playback_b = kSourceRunPlayback;
    }

    const PackageView& animations = runtime->packages.animations;
    const float old_max = clip_frame_max(animations, state.current);
    const float primary_max = clip_frame_max(animations, primary);
    const bool resources_changed =
        primary != state.current || secondary != state.secondary;
    if (primary != state.current) {
        if (!state.source_blend_active) {
            // commonDoubleAnime enters with field_0x2f8c == 0 after a
            // single-animation controller and starts both source clips at 0.
            state.clip_frame = -1.0f;
        } else {
            const float observed = state.clip_frame + 1.0f;
            state.clip_frame = std::fmod(
                observed / old_max * primary_max, primary_max) - 1.0f;
        }
    }
    const float secondary_max = clip_frame_max(animations, secondary);
    state.secondary_clip_frame = std::fmod(
        state.clip_frame / primary_max * secondary_max,
        secondary_max);
    if (resources_changed) {
        std::memcpy(
            state.blend_from, state.local, sizeof(state.local));
        state.blend_frame = kBasicInterpolationFrames;
        ++state.transitions;
    }
    state.current = primary;
    state.secondary = secondary;
    state.source_blend_ratio = blend;
    state.source_blend_active = true;
    state.playback_rate = playback_a + blend *
        ((playback_b * primary_max / secondary_max) - playback_a);

    Transform primary_pose[kPlayableJointCount] = {};
    Transform secondary_pose[kPlayableJointCount] = {};
    Transform target[kPlayableJointCount] = {};
    sample_clip(
        animations, primary, state.clip_frame, primary_pose);
    sample_clip(
        animations, secondary, state.secondary_clip_frame,
        secondary_pose);
    for (std::uint32_t joint = 0;
         joint < kPlayableJointCount; ++joint) {
        target[joint] = source_quat_lerp(
            primary_pose[joint], secondary_pose[joint], blend);
    }
    const Locomotion pose_locomotion =
        normal_speed == 0.0f ? Locomotion::Idle :
        rate >= kSourceRunChangeRate ? Locomotion::Run :
        Locomotion::Walk;
    return finish_animation_pose(
        runtime, pose_locomotion, actor_origin_before_pose, target);
}

bool update_source_animation_and_skin(
    Runtime* runtime, Locomotion locomotion,
    float normal_speed, float delta_seconds) {
    if (locomotion == Locomotion::TurnInPlace) {
        return update_animation_and_skin(
            runtime, Locomotion::TurnInPlace, 0.0f, delta_seconds);
    }
    return update_source_locomotion_and_skin(
        runtime, normal_speed, delta_seconds);
}

bool apply_source_animation_resource_and_skin(
    Runtime* runtime,
    std::uint32_t resource_id,
    float source_frame) {
    if (runtime == nullptr) return false;
    Transform target[kPlayableJointCount] = {};
    if (!sample_clip_resource(
            runtime->packages.animations, resource_id,
            source_frame, target)) {
        return false;
    }
    runtime->animation.blend_frame = 0.0f;
    runtime->animation.source_blend_active = false;
    const Vec3 actor_origin = runtime->grounding.enabled
        ? runtime->grounding.input.actor_position
        : Vec3{0.0f, 0.0f, 0.0f};
    return finish_animation_pose(
        runtime, Locomotion::Idle, actor_origin, target);
}

void set_grounding_input(
    Runtime* runtime, const PspLinkGroundingInput& input) {
    if (runtime == nullptr) {
        return;
    }
    runtime->grounding.input = input;
    runtime->grounding.enabled = true;
}

void clear_grounding_input(Runtime* runtime) {
    if (runtime == nullptr) {
        return;
    }
    runtime->grounding.enabled = false;
    runtime->grounding.input = {};
    runtime->grounding.feet[0] = {};
    runtime->grounding.feet[1] = {};
}

bool grounding_frame_valid(const Runtime& runtime) {
    return runtime.grounding.enabled &&
           runtime.grounding.metrics.frame_valid;
}

bool root_anchor_frame_valid(const Runtime& runtime) {
    return runtime.root_pose.metrics.frame_valid;
}

float source_foot_motion_raw(const Runtime& runtime) {
    return runtime.animation.source_foot_motion_raw;
}

float source_old_frame_rate_next(const Runtime& runtime) {
    const float frames = runtime.animation.blend_frame;
    return frames > 1.0f ? (frames - 1.0f) / frames : 0.0f;
}

bool idle_fidelity_frame_valid(const Runtime& runtime) {
    return runtime.animation.current != Locomotion::Idle ||
           (runtime.grounding.idle_fidelity.feet_contact_valid &&
            !runtime.grounding.idle_fidelity.visual_glide_detected &&
            runtime.grounding.idle_fidelity.actor_world_drift == 0.0f);
}

const SkinnedVertex* current_vertices(const Runtime& runtime) {
    return runtime.vertices[runtime.active_buffer];
}

const char* locomotion_name(Locomotion locomotion) {
    switch (locomotion) {
    case Locomotion::Idle: return "Idle";
    case Locomotion::Walk: return "Walk";
    case Locomotion::Run: return "Run";
    case Locomotion::TurnInPlace: return "TurnInPlace";
    }
    return "Unknown";
}

}  // namespace dusk::psp::playable
