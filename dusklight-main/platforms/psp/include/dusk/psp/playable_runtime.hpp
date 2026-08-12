#ifndef DUSK_PSP_PLAYABLE_RUNTIME_HPP
#define DUSK_PSP_PLAYABLE_RUNTIME_HPP

#include "dusk/psp/playable_package.hpp"

#include <cstdint>

namespace dusk::psp::playable {

constexpr std::uint32_t kPlayableJointCount = 35;
constexpr std::uint32_t kPlayableMaxVertices = 6000;
constexpr std::uint32_t kPlayableMaxRubies = 5;

enum class Locomotion : std::uint8_t {
    Idle,
    Walk,
    Run,
    TurnInPlace,
};

enum class GameMode : std::uint8_t {
    Playing,
    Paused,
    Exiting,
};

struct Vec3 {
    float x;
    float y;
    float z;
};

struct Quat {
    float x;
    float y;
    float z;
    float w;
};

struct Transform {
    Vec3 translation;
    Quat rotation;
    Vec3 scale;
};

struct Mat34 {
    float value[3][4];
};

struct SkinnedVertex {
    float u;
    float v;
    std::uint32_t color;
    float nx;
    float ny;
    float nz;
    float x;
    float y;
    float z;
};

struct Input {
    float analog_x;
    float analog_y;
    bool camera_left;
    bool camera_right;
    bool zoom_in;
    bool zoom_out;
    bool action_pressed;
    bool pause_pressed;
    bool cancel_pressed;
    bool up_pressed;
    bool down_pressed;
    bool debug_pressed;
};

struct Ruby {
    Vec3 position;
    bool active;
};

struct GameplayState {
    GameMode mode;
    Locomotion locomotion;
    Vec3 position;
    float yaw;
    float camera_yaw;
    float camera_distance;
    Ruby rubies[kPlayableMaxRubies];
    std::uint32_t rupees;
    std::uint32_t collected;
    std::uint32_t hearts;
    std::uint32_t pause_selection;
    std::uint32_t updates;
    std::uint32_t transitions;
    std::uint32_t idle_frames;
    std::uint32_t walk_frames;
    std::uint32_t run_frames;
    std::uint32_t pedestal_actions;
    bool action_prompt;
    bool debug_visible;
};

struct AnimationState {
    Transform local[kPlayableJointCount];
    Transform blend_from[kPlayableJointCount];
    Mat34 global[kPlayableJointCount];
    Mat34 skin[kPlayableJointCount];
    Locomotion current;
    Locomotion secondary;
    float clip_frame;
    float secondary_clip_frame;
    float playback_rate;
    float source_blend_ratio;
    float blend_frame;
    Vec3 source_previous_feet[2];
    float source_foot_motion_raw;
    std::uint32_t transitions;
    bool source_feet_valid;
    bool source_blend_active;
    bool matrices_finite;
};

struct PspLinkGroundingInput {
    Vec3 actor_position;
    float actor_yaw;
    float floor_y;
    Vec3 floor_normal;
    float capsule_bottom_y;
    std::uint32_t collision_generation;
    bool floor_valid;
};

struct PspLinkFootState {
    Vec3 contact;
    float floor_y;
    float correction_y;
    float penetration;
    float hover;
    bool contact_valid;
};

struct PspLinkGroundingMetrics {
    float actor_origin_y;
    float root_joint_y;
    float pelvis_y;
    float left_ankle_y;
    float right_ankle_y;
    float left_sole_min_y;
    float right_sole_min_y;
    float floor_y;
    float left_foot_penetration_max;
    float right_foot_penetration_max;
    float left_foot_hover_max;
    float right_foot_hover_max;
    std::uint32_t legs_visible_below_floor_frames;
    std::uint32_t pelvis_below_floor_frames;
    std::uint32_t collision_bottom_below_floor_frames;
    std::uint32_t corrected_frames;
    std::uint32_t source_solver_iterations;
    bool model_collision_vertical_parity;
    bool root_translation_double_applied;
    bool grounding_source_derived;
    bool frame_valid;
};

struct PspLinkIdleFidelityMetrics {
    float left_foot_slip_max;
    float right_foot_slip_max;
    float left_contact_drift_total;
    float right_contact_drift_total;
    float actor_world_drift;
    float root_world_drift;
    std::uint32_t left_planted_frames;
    std::uint32_t right_planted_frames;
    std::uint32_t loop_discontinuity_frames;
    bool feet_contact_valid;
    bool visual_glide_detected;
};

struct PspLinkGroundingState {
    PspLinkGroundingInput input;
    PspLinkFootState feet[2];
    PspLinkGroundingMetrics metrics;
    std::uint16_t sole_vertices[2][64];
    std::uint16_t sole_vertex_count[2];
    std::uint32_t last_collision_generation;
    Vec3 idle_contact_anchor[2];
    Vec3 idle_previous_contact[2];
    PspLinkIdleFidelityMetrics idle_fidelity;
    bool idle_anchor_valid[2];
    bool sole_sets_valid;
    bool enabled;
    bool debug_enabled;
};

struct PspLinkRootPoseMetrics {
    Vec3 bind_root_translation;
    Vec3 animated_root_translation;
    Vec3 root_delta;
    Vec3 final_root_translation;
    float idle_root_translation_min;
    float idle_root_translation_max;
    float idle_pelvis_translation_min;
    float idle_pelvis_translation_max;
    float walk_root_translation_min;
    float walk_root_translation_max;
    float run_root_translation_min;
    float run_root_translation_max;
    bool root_anchor_source_derived;
    bool root_horizontal_motion_removed;
    bool root_horizontal_motion_preserved;
    bool root_horizontal_motion_double_applied;
    bool idle_actor_origin_stable;
    bool idle_root_reference_valid;
    bool idle_pelvis_motion_preserved;
    bool idle_feet_grounded;
    bool walk_root_reference_valid;
    bool run_root_reference_valid;
    bool collision_model_origin_parity;
    bool idle_observed;
    bool walk_observed;
    bool run_observed;
    bool frame_valid;
};

struct PspLinkRootPoseState {
    PspLinkRootPoseMetrics metrics;
};

struct Runtime {
    PackageSet packages;
    AnimationState animation;
    PspLinkGroundingState grounding;
    PspLinkRootPoseState root_pose;
    alignas(16) SkinnedVertex vertices[2][kPlayableMaxVertices];
    std::uint32_t vertex_count;
    std::uint32_t active_buffer;
    std::uint32_t skinned_vertices;
    std::uint32_t allocations_during_update;
    std::uint32_t allocations_during_render;
    bool vertices_finite;
    bool guards_valid;
    std::uint32_t guard_before;
    std::uint32_t guard_after;
};

void reset_gameplay(GameplayState* state);
void update_gameplay(
    GameplayState* state, const Input& input, float delta_seconds);
Input replay_input(std::uint32_t update);
bool playable_mode_name_valid(const char* name);
bool gameplay_state_consistent(const GameplayState& state);
bool replay_state_complete(const GameplayState& state);
bool initialize_runtime(Runtime* runtime, const PackageSet& packages);
bool update_animation_and_skin(
    Runtime* runtime,
    Locomotion locomotion,
    float speed,
    float delta_seconds);
bool update_source_locomotion_and_skin(
    Runtime* runtime,
    float normal_speed,
    float delta_seconds);
bool update_source_animation_and_skin(
    Runtime* runtime,
    Locomotion locomotion,
    float normal_speed,
    float delta_seconds);
// Apply a source animation resource at an explicit source frame. Unlike the
// locomotion API this resolves the DPAN clip by resource ID (e.g. GETA 0x169)
// and therefore preserves event/demo animation identity.
bool apply_source_animation_resource_and_skin(
    Runtime* runtime,
    std::uint32_t resource_id,
    float source_frame);
std::uint32_t active_animation_resource_id(const Runtime& runtime);
float active_animation_source_frame(const Runtime& runtime);
float active_animation_playback_rate(const Runtime& runtime);
void set_grounding_input(
    Runtime* runtime, const PspLinkGroundingInput& input);
void clear_grounding_input(Runtime* runtime);
bool grounding_frame_valid(const Runtime& runtime);
bool root_anchor_frame_valid(const Runtime& runtime);
float source_foot_motion_raw(const Runtime& runtime);
float source_old_frame_rate_next(const Runtime& runtime);
bool idle_fidelity_frame_valid(const Runtime& runtime);
const SkinnedVertex* current_vertices(const Runtime& runtime);
const char* locomotion_name(Locomotion locomotion);

}  // namespace dusk::psp::playable

#endif
