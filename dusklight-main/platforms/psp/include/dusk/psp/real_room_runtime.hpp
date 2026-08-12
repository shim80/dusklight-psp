#ifndef DUSK_PSP_REAL_ROOM_RUNTIME_HPP
#define DUSK_PSP_REAL_ROOM_RUNTIME_HPP

#include "dusk/psp/playable_runtime.hpp"
#include "dusk/psp/room_collision.hpp"

#include <cstdint>

namespace dusk::psp::room {

enum class LoadState : std::uint8_t {
    Boot,
    LoadingRoom,
    Playing,
    Paused,
    UnloadingRoom,
    Exiting,
};

enum class MotionPhase : std::uint8_t {
    Idle,
    WalkStart,
    Walk,
    RunStart,
    Run,
    Decelerate,
    TurnInPlace,
};

// Source-compatible subset of daAlink_c::daAlink_PROC used by the
// already-supported PSP Link runtime. Values come from the exact local
// Dusklight snapshot; this is runtime state, not trace-emitter metadata.
enum class LinkProcedure : std::uint16_t {
    Wait = 0x003,
    Move = 0x004,
    WaitTurn = 0x008,
    Damage = 0x032,
    Invalid = 0xffff,
};

enum class RealRoomCheckpoint : std::uint8_t {
    InputUpdateEnter,
    InputUpdateExit,
    ProcedureUpdateEnter,
    ProcedureUpdateExit,
    CameraUpdateEnter,
    CameraUpdateExit,
    FramePresent,
};

struct RealRoomState {
    LoadState mode;
    playable::Locomotion locomotion;
    Vec3 position;
    float yaw;
    float target_yaw;
    float wait_turn_target_yaw;
    float input_stick_angle;
    // Source daAlink_c::mNormalSpeed, in world units per actor update.
    float normal_speed;
    // Source fopAc_ac_c::speedF after the locomotion blend contribution.
    float speed;
    // Source setFootSpeed/posMove state, supplied by the preceding model pose.
    float source_foot_motion_raw;
    float source_foot_speed;
    float source_old_frame_rate;
    float previous_stick_magnitude;
    Vec3 velocity;
    MotionPhase motion_phase;
    std::uint32_t motion_phase_frames;
    LinkProcedure link_procedure;
    std::uint32_t link_procedure_frames;
    bool link_procedure_valid;
    float camera_yaw;
    float camera_controlled_yaw_next;
    float camera_view_yaw;
    float camera_distance;
    float camera_pitch;
    Vec3 camera_eye;
    Vec3 camera_center;
    std::uint32_t camera_style_timer;
    bool camera_style_settled;
    Vec3 rubies[5];
    bool ruby_active[5];
    Vec3 interaction;
    std::uint32_t rupees;
    std::uint32_t collected;
    std::uint32_t total_collected;
    std::uint32_t pause_selection;
    std::uint32_t updates;
    std::uint32_t idle_frames;
    std::uint32_t walk_frames;
    std::uint32_t run_frames;
    std::uint32_t resets;
    std::uint32_t actions;
    std::uint32_t action_prompt_frames;
    std::uint32_t pause_entries;
    std::uint32_t pause_navigation;
    std::uint32_t slope_rejections;
    std::uint32_t backward_visual_frames;
    float forward_alignment_dot_min;
    float forward_alignment_dot_sum;
    std::uint32_t forward_alignment_samples;
    std::uint32_t camera_manual_frames;
    bool action_prompt;
    bool debug_visible;
};

void construct_link_procedure_state(RealRoomState* state);
void initialize_link_wait_procedure(RealRoomState* state);
const char* link_procedure_symbol(LinkProcedure procedure);

struct RealRoomRuntime {
    PackageView model;
    PackageView textures;
    PackageView collision_package;
    PackageView scene;
    CollisionWorld collision;
    RealRoomState state;
    std::uint32_t allocations_during_update;
    std::uint32_t allocations_during_render;
    std::uint32_t tracked_memory;
    bool package_crc_valid;
    bool spawn_valid;
    bool collision_valid;
#if defined(DUSK_REAL_ROOM_CHECKPOINTS)
    void (*checkpoint_observer)(
        RealRoomCheckpoint checkpoint,
        const RealRoomState& state,
        void* user);
    void* checkpoint_observer_user;
#endif
};

const char* real_room_checkpoint_name(RealRoomCheckpoint checkpoint);
void set_real_room_checkpoint_observer(
    RealRoomRuntime* runtime,
    void (*observer)(
        RealRoomCheckpoint checkpoint,
        const RealRoomState& state,
        void* user),
    void* user);

bool initialize_real_room_runtime(
    RealRoomRuntime* runtime,
    const PackageView& model,
    const PackageView& textures,
    const PackageView& collision,
    const PackageView& scene);
bool spawn_real_room(
    RealRoomRuntime* runtime, std::uint8_t start_index);
void reset_real_room(RealRoomRuntime* runtime);
void update_real_room(
    RealRoomRuntime* runtime,
    const playable::Input& input,
    float delta_seconds);
void set_link_animation_motion(
    RealRoomRuntime* runtime,
    float foot_motion_raw,
    float old_frame_rate_next);
playable::Input real_room_replay_input(
    const RealRoomRuntime& runtime, std::uint32_t update);
bool real_room_state_consistent(const RealRoomRuntime& runtime);
bool real_room_replay_complete(const RealRoomRuntime& runtime);

}  // namespace dusk::psp::room

#endif
