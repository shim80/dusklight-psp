#include "dusk/psp/real_room_runtime.hpp"
#include "dusk/psp/link_fidelity.hpp"

#include <algorithm>
#include <cmath>
#include <cstring>

namespace dusk::psp::room {
namespace {

constexpr float kPi = link::kPi;
constexpr std::uint32_t kStageFSp110 = 0xba29ace1u;
constexpr float kSlopeCosine = 0.643f;
constexpr float kStepHeight = 35.0f;
// dCamera_c::chaseCamera style 45 values from the pinned local camstyle.dat.
// This is the source startup subset used before the general PSP camera path.
constexpr float kSourceCameraAttentionHeight = 142.5f;
// The style's -10 focus parameter composes with Link's half-unit source
// attention bias at the first execute checkpoint.
constexpr float kSourceCameraCenterYOffset = -10.5f;
constexpr float kSourceCameraCenterZOffset = 1.0f;
constexpr float kSourceCameraDistance = 300.0f;
// cSAngle quantization of the style's 23-degree latitude.
constexpr float kSourceCameraPitch = 22.998047f;
constexpr float kSourceStyleSettleRate = 0.75f;
constexpr float kSourceCenterHorizontalCushion = 0.5f;
constexpr float kSourceCenterVerticalCushion = 0.18f;
constexpr float kSourcePitchInitialCushion = 0.0545f;
constexpr float kSourcePitchCushionStep = 0.04f;
constexpr float kSourcePitchCushionLimit = 0.38f;
constexpr std::uint32_t kSourceDemoInputReleaseTick = 28;

void emit_checkpoint(
    RealRoomRuntime* runtime, RealRoomCheckpoint checkpoint) {
#if defined(DUSK_REAL_ROOM_CHECKPOINTS)
    if (runtime->checkpoint_observer != nullptr) {
        runtime->checkpoint_observer(
            checkpoint, runtime->state,
            runtime->checkpoint_observer_user);
    }
#else
    (void)runtime;
    (void)checkpoint;
#endif
}

float distance_xz(const Vec3& a, const Vec3& b) {
    const float dx = a.x - b.x;
    const float dz = a.z - b.z;
    return std::sqrt(dx * dx + dz * dz);
}

Vec3 desired_camera_eye(const RealRoomState& state) {
    const float horizontal =
        std::cos(state.camera_pitch) * state.camera_distance;
    return {
        state.camera_center.x +
            std::sin(state.camera_view_yaw) * horizontal,
        state.camera_center.y +
            std::sin(state.camera_pitch) * state.camera_distance,
        state.camera_center.z +
            std::cos(state.camera_view_yaw) * horizontal,
    };
}

Vec3 source_camera_center_target(const RealRoomState& state) {
    return {
        state.position.x +
            std::sin(state.yaw) * kSourceCameraCenterZOffset,
        state.position.y + kSourceCameraAttentionHeight +
            kSourceCameraCenterYOffset,
        state.position.z +
            std::cos(state.yaw) * kSourceCameraCenterZOffset,
    };
}

void update_camera(RealRoomRuntime* runtime) {
    RealRoomState& state = runtime->state;
    if (!state.camera_style_settled) {
        const Vec3 target = source_camera_center_target(state);
        state.camera_center.x +=
            (target.x - state.camera_center.x) * kSourceStyleSettleRate;
        state.camera_center.y +=
            (target.y - state.camera_center.y) * kSourceStyleSettleRate;
        state.camera_center.z +=
            (target.z - state.camera_center.z) * kSourceStyleSettleRate;
        state.camera_pitch +=
            ((kSourceCameraPitch * kPi / 180.0f) - state.camera_pitch) *
            kSourceStyleSettleRate;
        state.camera_eye = desired_camera_eye(state);
        state.camera_controlled_yaw_next = link::source_globe_yaw(
            state.camera_eye.x - state.camera_center.x,
            state.camera_eye.z - state.camera_center.z);
        state.camera_style_settled = true;
        ++state.camera_style_timer;
        return;
    }
    const Vec3 previous_eye = state.camera_eye;
    const Vec3 target = source_camera_center_target(state);
    state.camera_center.x +=
        (target.x - state.camera_center.x) *
        kSourceCenterHorizontalCushion;
    state.camera_center.y +=
        (target.y - state.camera_center.y) *
        kSourceCenterVerticalCushion;
    state.camera_center.z +=
        (target.z - state.camera_center.z) *
        kSourceCenterHorizontalCushion;
    const float prior_direction_x = previous_eye.x - state.camera_center.x;
    const float prior_direction_y = previous_eye.y - state.camera_center.y;
    const float prior_direction_z = previous_eye.z - state.camera_center.z;
    state.camera_view_yaw = link::source_globe_yaw(
        prior_direction_x, prior_direction_z);
    state.camera_distance = std::sqrt(
        prior_direction_x * prior_direction_x +
        prior_direction_y * prior_direction_y +
        prior_direction_z * prior_direction_z);
    const float pitch_cushion = std::min(
        kSourcePitchCushionLimit,
        kSourcePitchInitialCushion +
            static_cast<float>(state.camera_style_timer - 1) *
                kSourcePitchCushionStep);
    state.camera_pitch +=
        ((kSourceCameraPitch * kPi / 180.0f) - state.camera_pitch) *
        pitch_cushion;
    const Vec3 desired = desired_camera_eye(state);
    const LineHit hit =
        trace_camera(&runtime->collision, state.camera_center, desired);
    if (hit.hit) {
        const float fraction = std::max(0.08f, hit.fraction - 0.04f);
        state.camera_eye = {
            state.camera_center.x +
                (desired.x - state.camera_center.x) * fraction,
            state.camera_center.y +
                (desired.y - state.camera_center.y) * fraction,
            state.camera_center.z +
                (desired.z - state.camera_center.z) * fraction,
        };
    } else {
        state.camera_eye = desired;
    }
    state.camera_controlled_yaw_next = link::source_globe_yaw(
        state.camera_eye.x - state.camera_center.x,
        state.camera_eye.z - state.camera_center.z);
    ++state.camera_style_timer;
}

void update_collectibles(RealRoomState* state) {
    for (std::uint32_t index = 0; index < 5; ++index) {
        if (state->ruby_active[index] &&
            distance_xz(state->position, state->rubies[index]) < 85.0f) {
            state->ruby_active[index] = false;
            ++state->rupees;
            ++state->collected;
            ++state->total_collected;
        }
    }
    state->action_prompt =
        distance_xz(state->position, state->interaction) < 150.0f;
}

void set_link_procedure(
    RealRoomState* state, LinkProcedure procedure) {
    if (!state->link_procedure_valid ||
        state->link_procedure != procedure) {
        state->link_procedure = procedure;
        state->link_procedure_frames = 1;
        state->link_procedure_valid = true;
    } else {
        ++state->link_procedure_frames;
    }
}

void update_link_procedure(
    RealRoomState* state, const link::Stick& stick) {
    if (stick.magnitude <= 0.0f) {
        set_link_procedure(state, LinkProcedure::Wait);
    } else if (state->motion_phase == MotionPhase::TurnInPlace) {
        set_link_procedure(state, LinkProcedure::WaitTurn);
    } else {
        set_link_procedure(state, LinkProcedure::Move);
    }
}

}  // namespace

void construct_link_procedure_state(RealRoomState* state) {
    if (state == nullptr) {
        return;
    }
    state->link_procedure = LinkProcedure::Invalid;
    state->link_procedure_frames = 0;
    state->link_procedure_valid = false;
}

void initialize_link_wait_procedure(RealRoomState* state) {
    if (state != nullptr) {
        set_link_procedure(state, LinkProcedure::Wait);
    }
}

const char* link_procedure_symbol(LinkProcedure procedure) {
    switch (procedure) {
    case LinkProcedure::Wait: return "PROC_WAIT";
    case LinkProcedure::Move: return "PROC_MOVE";
    case LinkProcedure::WaitTurn: return "PROC_WAIT_TURN";
    case LinkProcedure::Damage: return "PROC_DAMAGE";
    case LinkProcedure::Invalid: return "PROC_MAX";
    }
    return "UNKNOWN";
}

const char* real_room_checkpoint_name(RealRoomCheckpoint checkpoint) {
    switch (checkpoint) {
    case RealRoomCheckpoint::InputUpdateEnter:
        return "input_update_enter";
    case RealRoomCheckpoint::InputUpdateExit:
        return "input_update_exit";
    case RealRoomCheckpoint::ProcedureUpdateEnter:
        return "procedure_update_enter";
    case RealRoomCheckpoint::ProcedureUpdateExit:
        return "procedure_update_exit";
    case RealRoomCheckpoint::CameraUpdateEnter:
        return "camera_update_enter";
    case RealRoomCheckpoint::CameraUpdateExit:
        return "camera_update_exit";
    case RealRoomCheckpoint::FramePresent:
        return "frame_present";
    }
    return "unknown";
}

void set_real_room_checkpoint_observer(
    RealRoomRuntime* runtime,
    void (*observer)(
        RealRoomCheckpoint checkpoint,
        const RealRoomState& state,
        void* user),
    void* user) {
#if defined(DUSK_REAL_ROOM_CHECKPOINTS)
    if (runtime != nullptr) {
        runtime->checkpoint_observer = observer;
        runtime->checkpoint_observer_user = user;
    }
#else
    (void)runtime;
    (void)observer;
    (void)user;
#endif
}

bool initialize_real_room_runtime(
    RealRoomRuntime* runtime,
    const PackageView& model,
    const PackageView& textures,
    const PackageView& collision,
    const PackageView& scene) {
    if (runtime == nullptr ||
        validate_dprm(model.bytes, model.size, &runtime->model) !=
            PackageError::Ok ||
        validate_room_dptx(
            textures.bytes, textures.size, &runtime->textures) !=
            PackageError::Ok ||
        validate_dpcl(
            collision.bytes, collision.size,
            &runtime->collision_package) != PackageError::Ok ||
        validate_dpsc(scene.bytes, scene.size, &runtime->scene) !=
            PackageError::Ok ||
        (read_u16(scene.bytes + 4) < 3 &&
         (read_u32(scene.bytes + 16) != kStageFSp110 ||
          read_u32(scene.bytes + 20) != 2 ||
          read_u32(scene.bytes + 24) != 0)) ||
        !initialize_collision_world(
            &runtime->collision, collision.bytes, collision.size)) {
        return false;
    }
    runtime->allocations_during_update = 0;
    runtime->allocations_during_render = 0;
    runtime->tracked_memory =
        model.size + textures.size + collision.size + scene.size +
        sizeof(*runtime);
    runtime->package_crc_valid = true;
    runtime->collision_valid = true;
#if defined(DUSK_REAL_ROOM_CHECKPOINTS)
    runtime->checkpoint_observer = nullptr;
    runtime->checkpoint_observer_user = nullptr;
#endif
    reset_real_room(runtime);
    const FloorHit floor =
        find_floor(&runtime->collision, runtime->state.position, 200.0f, 0.5f);
    runtime->spawn_valid =
        floor.hit &&
        std::fabs(floor.height - runtime->state.position.y) <= kStepHeight;
    return runtime->spawn_valid;
}

bool spawn_real_room(
    RealRoomRuntime* runtime, std::uint8_t start_index) {
    if (runtime == nullptr ||
        read_u16(runtime->scene.bytes + 4) < 3) {
        return false;
    }
    SceneSpawnV3 spawn = {};
    if (find_dpsc_spawn_v3(
            runtime->scene, start_index, &spawn) != PackageError::Ok ||
        !spawn.floor_valid) {
        return false;
    }
    RealRoomState& state = runtime->state;
    state.position = {
        spawn.position[0], spawn.position[1], spawn.position[2]};
    state.yaw =
        static_cast<float>(spawn.rotation[1]) *
        (2.0f * kPi / 65536.0f);
    state.target_yaw = state.yaw;
    state.wait_turn_target_yaw = state.yaw;
    state.camera_yaw = state.yaw + kPi;
    state.camera_controlled_yaw_next = state.camera_yaw;
    state.camera_view_yaw = state.camera_yaw;
    state.camera_distance = kSourceCameraDistance;
    state.camera_pitch = 0.0f;
    state.camera_center = {
        state.position.x,
        state.position.y + kSourceCameraAttentionHeight,
        state.position.z};
    state.camera_eye = desired_camera_eye(state);
    state.camera_style_timer = 0;
    state.camera_style_settled = false;
    state.locomotion = playable::Locomotion::Idle;
    state.motion_phase = MotionPhase::Idle;
    initialize_link_wait_procedure(&state);
    state.forward_alignment_dot_min = 1.0f;
    state.mode = LoadState::Playing;
    const FloorHit floor =
        find_floor(&runtime->collision, state.position, 200.0f, 0.5f);
    if (floor.hit) {
        state.camera_center.y =
            floor.height + kSourceCameraAttentionHeight;
        state.camera_eye = desired_camera_eye(state);
    }
    runtime->spawn_valid =
        floor.hit &&
        std::fabs(floor.height - state.position.y) <= kStepHeight;
    update_collectibles(&state);
    return runtime->spawn_valid;
}

void reset_real_room(RealRoomRuntime* runtime) {
    const std::uint8_t* scene = runtime->scene.bytes;
    RealRoomState& state = runtime->state;
    const std::uint32_t prior_resets = state.resets;
    const std::uint32_t prior_total_collected = state.total_collected;
    const std::uint32_t prior_actions = state.actions;
    const std::uint32_t prior_prompt_frames = state.action_prompt_frames;
    const std::uint32_t prior_pause_entries = state.pause_entries;
    const std::uint32_t prior_pause_navigation = state.pause_navigation;
    std::memset(&state, 0, sizeof(state));
    construct_link_procedure_state(&state);
    state.mode = LoadState::Playing;
    state.locomotion = playable::Locomotion::Idle;
    state.motion_phase = MotionPhase::Idle;
    initialize_link_wait_procedure(&state);
    state.forward_alignment_dot_min = 1.0f;
    state.position = {
        read_f32(scene + 40),
        read_f32(scene + 44),
        read_f32(scene + 48),
    };
    state.yaw =
        static_cast<float>(static_cast<std::int16_t>(read_u16(scene + 52))) *
        (2.0f * kPi / 65536.0f);
    state.target_yaw = state.yaw;
    state.wait_turn_target_yaw = state.yaw;
    state.camera_yaw = state.yaw + kPi;
    state.camera_controlled_yaw_next = state.camera_yaw;
    state.camera_view_yaw = state.camera_yaw;
    state.camera_distance = kSourceCameraDistance;
    state.camera_pitch = 0.0f;
    state.camera_center = {
        state.position.x,
        state.position.y + kSourceCameraAttentionHeight,
        state.position.z};
    state.camera_eye = desired_camera_eye(state);
    state.camera_style_timer = 0;
    state.camera_style_settled = false;
    const std::uint32_t ruby_offset = read_u32(scene + 124);
    for (std::uint32_t index = 0; index < 5; ++index) {
        const std::uint8_t* ruby = scene + ruby_offset + index * 16;
        state.rubies[index] = {
            read_f32(ruby), read_f32(ruby + 4), read_f32(ruby + 8)};
        state.ruby_active[index] = true;
    }
    state.interaction = {
        read_f32(scene + 144),
        read_f32(scene + 148),
        read_f32(scene + 152),
    };
    state.resets = prior_resets + 1;
    state.total_collected = prior_total_collected;
    state.actions = prior_actions;
    state.action_prompt_frames = prior_prompt_frames;
    state.pause_entries = prior_pause_entries;
    state.pause_navigation = prior_pause_navigation;
    const FloorHit floor =
        find_floor(&runtime->collision, state.position, 200.0f, 0.5f);
    if (floor.hit) {
        state.camera_center.y =
            floor.height + kSourceCameraAttentionHeight;
        state.camera_eye = desired_camera_eye(state);
    }
    update_collectibles(&state);
}

void update_real_room(
    RealRoomRuntime* runtime,
    const playable::Input& input,
    float delta_seconds) {
    RealRoomState& state = runtime->state;
    const float dt = std::clamp(delta_seconds, 0.0f, 1.0f / 15.0f);
    if (state.mode == LoadState::Exiting) {
        return;
    }
    if (state.mode == LoadState::Paused) {
        if (input.up_pressed && state.pause_selection > 0) {
            --state.pause_selection;
            ++state.pause_navigation;
        }
        if (input.down_pressed && state.pause_selection < 2) {
            ++state.pause_selection;
            ++state.pause_navigation;
        }
        if (input.cancel_pressed) {
            state.mode = LoadState::Playing;
        } else if (input.action_pressed) {
            if (state.pause_selection == 0) {
                state.mode = LoadState::Playing;
            } else if (state.pause_selection == 1) {
                reset_real_room(runtime);
            } else {
                state.mode = LoadState::Exiting;
            }
        }
        ++state.updates;
        return;
    }
    if (input.pause_pressed) {
        state.mode = LoadState::Paused;
        state.pause_selection = 0;
        ++state.pause_entries;
        ++state.updates;
        return;
    }
    if (input.debug_pressed) {
        state.debug_visible = !state.debug_visible;
    }
    // dCamera_c updates after daAlink_c in the source process order. Publish
    // the controlled direction computed by the preceding camera execute only
    // when the next actor update begins.
    state.camera_yaw = state.camera_controlled_yaw_next;
    emit_checkpoint(runtime, RealRoomCheckpoint::InputUpdateEnter);
    const FloorHit support =
        find_floor(
            &runtime->collision,
            {
                state.position.x,
                state.position.y + kStepHeight,
                state.position.z,
            },
            kStepHeight, kSlopeCosine);
    if (support.hit &&
        std::fabs(support.height - state.position.y) <= kStepHeight) {
        state.position.y = support.height;
    }
    if (input.camera_left) {
        state.camera_controlled_yaw_next -= 1.6f * dt;
        state.camera_view_yaw -= 1.6f * dt;
        state.camera_manual_frames = 60;
    }
    if (input.camera_right) {
        state.camera_controlled_yaw_next += 1.6f * dt;
        state.camera_view_yaw += 1.6f * dt;
        state.camera_manual_frames = 60;
    }
    if (input.cancel_pressed) {
        state.camera_controlled_yaw_next = state.yaw + kPi;
        state.camera_view_yaw = state.camera_controlled_yaw_next;
    }
    if (input.zoom_in) {
        state.camera_distance =
            std::max(180.0f, state.camera_distance - 220.0f * dt);
    }
    if (input.zoom_out) {
        state.camera_distance =
            std::min(700.0f, state.camera_distance + 220.0f * dt);
    }
    const link::Stick stick =
        link::normalize_stick(input.analog_x, input.analog_y);
    state.input_stick_angle =
        stick.magnitude > 0.0f
            ? link::s16_to_radians(static_cast<std::uint16_t>(
                  link::source_link_stick_angle_s16(stick)))
            : state.updates >= kSourceDemoInputReleaseTick
                ? link::s16_to_radians(0x8000u) : 0.0f;
    if (stick.magnitude <= 0.0f &&
        state.updates >= kSourceDemoInputReleaseTick) {
        state.target_yaw = link::source_move_yaw(
            state.input_stick_angle, state.camera_yaw);
    }
    const MotionPhase previous_phase = state.motion_phase;
    if (stick.magnitude > 0.0f) {
        // daAlink_c::setStickData adds the signed-16 pad angle directly to
        // dCam_getControledAngleY; the camera's internal actor-to-eye yaw is
        // not itself the movement heading.
        state.target_yaw = link::source_move_yaw(
            state.input_stick_angle, state.camera_yaw);
        const bool continuing_wait_turn =
            previous_phase == MotionPhase::TurnInPlace;
        const bool starting_wait_turn =
            !continuing_wait_turn && state.normal_speed <= 0.001f &&
            link::source_wait_turn_required(
                state.yaw, state.target_yaw);
        if (starting_wait_turn) {
            state.wait_turn_target_yaw = state.target_yaw;
            state.motion_phase = MotionPhase::TurnInPlace;
            state.normal_speed = link::approach_normal_speed(
                state.normal_speed, 0.0f, stick.magnitude, dt);
        } else if (continuing_wait_turn) {
            state.yaw = link::source_wait_turn_yaw(
                state.yaw, state.wait_turn_target_yaw);
            const bool wait_turn_reached =
                link::source_wait_turn_reached(
                    state.yaw, state.wait_turn_target_yaw);
            if (wait_turn_reached) {
                // procWaitTurn reaches its target, then checkNextAction(0)
                // executes setSpeedAndAngleNormal before procMoveInit.
                state.normal_speed = link::approach_normal_speed(
                    state.normal_speed,
                    link::source_normal_speed_target(stick.magnitude),
                    stick.magnitude, dt);
                // initOldFrameMorf(4) performs its initial decrement before
                // posMove observes the new double-animation controller.
                state.source_old_frame_rate = 0.75f;
                state.motion_phase = MotionPhase::WalkStart;
            } else {
                state.normal_speed = link::approach_normal_speed(
                    state.normal_speed, 0.0f, stick.magnitude, dt);
                state.motion_phase = MotionPhase::TurnInPlace;
            }
        } else {
            state.yaw = link::source_move_approach_yaw(
                state.yaw, state.target_yaw, stick.magnitude);
            const float target_speed =
                link::source_normal_speed_target(stick.magnitude);
            state.normal_speed = link::approach_normal_speed(
                state.normal_speed, target_speed, stick.magnitude, dt);
            const float speed_rate =
                state.normal_speed / link::kSourceMaxSpeedPerUpdate;
            if (speed_rate >= link::kRunChangeRate) {
                state.motion_phase =
                    previous_phase == MotionPhase::Run
                        ? MotionPhase::Run
                        : MotionPhase::RunStart;
            } else {
                state.motion_phase =
                    previous_phase == MotionPhase::Walk
                        ? MotionPhase::Walk
                        : MotionPhase::WalkStart;
            }
        }
    } else {
        state.normal_speed = link::approach_normal_speed(
            state.normal_speed, 0.0f, 0.0f, dt);
        state.motion_phase =
            state.normal_speed > 0.001f
                ? MotionPhase::Decelerate
                : MotionPhase::Idle;
    }
    // daAlink_c::posMove computes speedF from mNormalSpeed only after
    // setBlendMoveAnime has selected its speed modifier, then adds the
    // preceding model pose's smoothed foot displacement.
    const float speed_modifier =
        link::source_move_speed_modifier(state.normal_speed);
    if (std::fabs(speed_modifier) < 1.0f && stick.magnitude > 0.0f &&
        std::fabs(
            state.previous_stick_magnitude - stick.magnitude) < 0.2f) {
        state.source_foot_speed =
            0.3f * state.source_foot_motion_raw +
            0.7f * state.source_foot_speed;
    } else {
        state.source_foot_speed = state.source_foot_motion_raw;
    }
    state.speed = link::source_effective_speed(
        state.normal_speed, state.source_foot_speed,
        state.source_old_frame_rate);
    state.previous_stick_magnitude = stick.magnitude;
    state.motion_phase_frames =
        state.motion_phase == previous_phase
            ? state.motion_phase_frames + 1
            : 1;
    emit_checkpoint(runtime, RealRoomCheckpoint::InputUpdateExit);
    emit_checkpoint(runtime, RealRoomCheckpoint::ProcedureUpdateEnter);
    update_link_procedure(&state, stick);
    emit_checkpoint(runtime, RealRoomCheckpoint::ProcedureUpdateExit);
    const float speed_rate =
        state.normal_speed / link::kSourceMaxSpeedPerUpdate;
    if (state.motion_phase == MotionPhase::TurnInPlace) {
        state.locomotion = playable::Locomotion::TurnInPlace;
    } else if (state.normal_speed <= 0.001f) {
        state.locomotion = playable::Locomotion::Idle;
        ++state.idle_frames;
    } else {
        state.locomotion =
            speed_rate >= link::kRunChangeRate
                ? playable::Locomotion::Run
                : playable::Locomotion::Walk;
        if (state.locomotion == playable::Locomotion::Run) {
            ++state.run_frames;
        } else {
            ++state.walk_frames;
        }
        const link::Vec2 forward = link::forward_from_yaw(state.yaw);
        const Vec3 desired = {
            state.position.x + forward.x * state.speed,
            state.position.y,
            state.position.z + forward.z * state.speed,
        };
        Vec3 resolved = desired;
        resolve_horizontal(
            &runtime->collision, state.position, desired,
            link::kPlayerRadius, link::kPlayerHeight, &resolved);
        const float resolved_x = resolved.x - state.position.x;
        const float resolved_z = resolved.z - state.position.z;
        const float resolved_length =
            std::sqrt(resolved_x * resolved_x + resolved_z * resolved_z);
        if (resolved_length > 0.001f) {
            const float resolved_alignment =
                (forward.x * resolved_x + forward.z * resolved_z) /
                resolved_length;
            if (resolved_alignment < 0.995f) {
                resolved.x = state.position.x;
                resolved.z = state.position.z;
            }
        }
        const FloorHit floor =
            find_floor(
                &runtime->collision,
                {resolved.x, state.position.y + kStepHeight, resolved.z},
                kStepHeight, kSlopeCosine);
        if (floor.hit &&
            std::fabs(floor.height - state.position.y) <= kStepHeight) {
            const Vec3 previous = state.position;
            state.position = {resolved.x, floor.height, resolved.z};
            state.velocity = {
                state.position.x - previous.x,
                state.position.y - previous.y,
                state.position.z - previous.z,
            };
            const float horizontal = std::sqrt(
                state.velocity.x * state.velocity.x +
                state.velocity.z * state.velocity.z);
            if (horizontal > 0.001f &&
                state.motion_phase != MotionPhase::TurnInPlace) {
                const float alignment =
                    (forward.x * state.velocity.x +
                     forward.z * state.velocity.z) / horizontal;
                state.forward_alignment_dot_min = std::min(
                    state.forward_alignment_dot_min, alignment);
                state.forward_alignment_dot_sum += alignment;
                ++state.forward_alignment_samples;
                if (alignment < 0.0f) {
                    ++state.backward_visual_frames;
                }
            }
        } else {
            state.velocity = {};
            ++state.slope_rejections;
        }
    }
    if (state.speed <= 0.001f) {
        state.velocity = {};
    }
    if (state.camera_manual_frames > 0) {
        --state.camera_manual_frames;
    } else if (state.speed > 0.001f) {
        const float wanted = link::wrap_angle(state.yaw + kPi);
        state.camera_controlled_yaw_next = link::approach_yaw(
            state.camera_controlled_yaw_next, wanted, dt * 0.25f);
    }
    update_collectibles(&state);
    if (state.action_prompt) {
        ++state.action_prompt_frames;
    }
    if (input.action_pressed && state.action_prompt) {
        ++state.actions;
    }
    emit_checkpoint(runtime, RealRoomCheckpoint::CameraUpdateEnter);
    update_camera(runtime);
    emit_checkpoint(runtime, RealRoomCheckpoint::CameraUpdateExit);
    ++state.updates;
    emit_checkpoint(runtime, RealRoomCheckpoint::FramePresent);
}

void set_link_animation_motion(
    RealRoomRuntime* runtime,
    float foot_motion_raw,
    float old_frame_rate_next) {
    if (runtime == nullptr || !std::isfinite(foot_motion_raw) ||
        !std::isfinite(old_frame_rate_next)) {
        return;
    }
    runtime->state.source_foot_motion_raw =
        std::max(0.0f, foot_motion_raw);
    runtime->state.source_old_frame_rate =
        std::clamp(old_frame_rate_next, 0.0f, 1.0f);
}

playable::Input real_room_replay_input(
    const RealRoomRuntime& runtime, std::uint32_t update) {
    playable::Input input = {};
    if (update < 45) {
        return input;
    }
    if (update == 700) {
        input.pause_pressed = true;
        return input;
    }
    if (update == 710) {
        input.down_pressed = true;
        return input;
    }
    if (update == 720) {
        input.up_pressed = true;
        return input;
    }
    if (update == 730) {
        input.action_pressed = true;
        return input;
    }
    if (update == 900) {
        input.pause_pressed = true;
        return input;
    }
    if (update == 910) {
        input.down_pressed = true;
        return input;
    }
    if (update == 920) {
        input.action_pressed = true;
        return input;
    }
    if (update > 920 && update < 980) {
        return input;
    }
    if (update > 1100 && update < 1140) {
        input.camera_left = true;
    }
    if (update > 1160 && update < 1200) {
        input.zoom_in = true;
    }
    if (update == 105 || update == 1250) {
        input.action_pressed = true;
    }
    const RealRoomState& state = runtime.state;
    Vec3 target = state.interaction;
    bool found = false;
    if (update < 130) {
        found = true;
    } else {
        for (std::uint32_t index = 0; index < 5; ++index) {
            if (state.ruby_active[index]) {
                target = state.rubies[index];
                found = true;
                break;
            }
        }
    }
    if (!found && update < 850) {
        target = {
            read_f32(runtime.scene.bytes + 96) - 100.0f,
            state.position.y,
            read_f32(runtime.scene.bytes + 104) - 100.0f,
        };
    }
    const float dx = target.x - state.position.x;
    const float dz = target.z - state.position.z;
    const float length = std::sqrt(dx * dx + dz * dz);
    if (length > 20.0f) {
        link::world_direction_to_camera_input(
            dx / length, dz / length, state.camera_yaw,
            &input.analog_x, &input.analog_y);
    }
    return input;
}

bool real_room_state_consistent(const RealRoomRuntime& runtime) {
    const RealRoomState& state = runtime.state;
    return std::isfinite(state.position.x) &&
           std::isfinite(state.position.y) &&
           std::isfinite(state.position.z) &&
           std::isfinite(state.camera_eye.x) &&
           std::isfinite(state.camera_eye.y) &&
           std::isfinite(state.camera_eye.z) &&
           std::isfinite(state.normal_speed) &&
           std::isfinite(state.speed) &&
           std::isfinite(state.yaw) &&
           std::isfinite(state.target_yaw) &&
           state.backward_visual_frames == 0 &&
           state.rupees <= 5 &&
           runtime.allocations_during_update == 0 &&
           runtime.allocations_during_render == 0;
}

bool real_room_replay_complete(const RealRoomRuntime& runtime) {
    const RealRoomState& state = runtime.state;
    return state.idle_frames > 0 &&
           state.walk_frames + state.run_frames > 0 &&
           state.total_collected >= 4 &&
           state.collected >= 1 &&
           state.resets >= 2 &&
           state.actions >= 1 &&
           state.action_prompt_frames > 0 &&
           state.pause_entries >= 2 &&
           state.pause_navigation >= 3 &&
           runtime.collision.floor_queries > 100 &&
           runtime.collision.camera_queries > 100 &&
           runtime.collision.camera_hits > 0 &&
           runtime.collision.wall_hits > 0 &&
           state.mode != LoadState::Exiting &&
           real_room_state_consistent(runtime);
}

}  // namespace dusk::psp::room
