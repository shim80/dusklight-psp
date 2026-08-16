#ifndef DUSK_PSP_PLAYABLE_RENDER_HPP
#define DUSK_PSP_PLAYABLE_RENDER_HPP

#include "dusk/psp/playable_runtime.hpp"
#include "dusk/psp/actor_render.hpp"
#include "dusk/psp/environment_runtime.hpp"
#include "dusk/psp/opaque_order.hpp"
#include "dusk/psp/presentation_profile.hpp"
#include "dusk/psp/render_state_trace.hpp"
#include "dusk/psp/shadow_runtime.hpp"
#include "dusk/psp/startup_ui_package.hpp"

#include <cstdint>

namespace dusk::psp::playable {

enum class RenderProfile : std::uint8_t {
    KnownGoodUnlit,
    LightingDiagnostics,
    CandidateGame,
};

enum class LightingMode : std::uint8_t {
    SourceApprox,
    SafeAmbient,
    SafeWrappedDiffuse,
    SafeWrappedDiffuseRim,
    Off,
    Debug,
    WhiteAmbient,
    WhiteKey,
    SourceAmbient,
    SourceKey,
    MaterialBaseColor,
    TextureOnly,
    GuCandidate,
};

enum class FogMode : std::uint8_t {
    Source,
    Off,
};

enum class ShadowMode : std::uint8_t {
    ProjectedLink,
    Off,
    Simple,
    ProjectedPriority,
    Debug,
};

struct RenderProfileConfig {
    LightingMode lighting;
    FogMode fog;
    ShadowMode shadows;
};

constexpr RenderProfileConfig render_profile_config(RenderProfile profile) {
    return profile == RenderProfile::CandidateGame
        ? RenderProfileConfig{
              LightingMode::SafeWrappedDiffuse,
              FogMode::Source,
              ShadowMode::ProjectedLink}
        : RenderProfileConfig{
              LightingMode::Off,
              FogMode::Off,
              ShadowMode::Off};
}

constexpr const char* render_profile_name(RenderProfile profile) {
    return profile == RenderProfile::KnownGoodUnlit
        ? "known_good_unlit"
        : profile == RenderProfile::LightingDiagnostics
            ? "lighting_diagnostics"
            : "candidate_game";
}

struct RenderMetrics {
    std::uint32_t edram_total;
    std::uint32_t edram_framebuffers;
    std::uint32_t edram_depth;
    std::uint32_t edram_textures;
    std::uint32_t edram_ui;
    std::uint32_t edram_remaining;
    std::uint32_t link_draw_calls;
    std::uint32_t scene_draw_calls;
    std::uint32_t ui_draw_calls;
    std::uint32_t command_bytes;
    std::uint32_t frames;
    std::uint32_t pixel_regions_changed;
    std::uint16_t pixel_values[11];
    bool pixel_checks_valid;
    bool synchronized;
    std::uint32_t edram_room;
    std::uint32_t room_draw_calls;
    std::uint32_t room_opaque_draws;
    std::uint32_t room_alpha_test_draws;
    std::uint32_t room_alpha_blend_draws;
    std::uint32_t room_alpha_state_records;
    std::uint32_t room_alpha_state_missing_draws;
    std::uint32_t room_alpha_exact_draws;
    std::uint32_t room_blend_exact_draws;
    std::uint32_t room_cull_exact_draws;
    std::uint32_t room_pixel_regions_changed;
    std::uint16_t room_pixel_values[16];
    std::uint32_t original_actor_draw_calls;
    std::uint32_t original_actor_opaque_draws;
    std::uint32_t original_actor_alpha_test_draws;
    std::uint32_t original_actor_alpha_blend_draws;
    std::uint32_t environment_records_loaded;
    std::uint32_t environment_clear_color;
    std::uint32_t environment_fog_color;
    float environment_fog_near;
    float environment_fog_far;
    bool environment_fog_enabled;
    bool environment_lighting_enabled;
    std::uint32_t shadow_simple_draw_calls;
    std::uint32_t shadow_receiver_triangles;
    bool shadow_simple_enabled;
    std::uint32_t shadow_projected_draw_calls;
    std::uint32_t shadow_caster_vertices;
    std::uint32_t shadow_map_edram_bytes;
    std::uint32_t shadow_aux_edram_bytes;
    std::uint16_t shadow_map_width;
    std::uint16_t shadow_map_height;
    bool shadow_projected_enabled;
    bool shadow_target_restored;
    std::uint32_t source_normal_count;
    std::uint32_t runtime_normal_count;
    std::uint32_t zero_normal_count;
    std::uint32_t non_finite_normal_count;
    float normal_length_min;
    float normal_length_max;
    float normal_error_mean;
    float normal_error_max;
    float normal_debug_color_variance;
    std::uint32_t source_material_count;
    std::uint32_t runtime_material_lighting_count;
    std::uint32_t unlit_material_count;
    std::uint32_t emissive_material_count;
    std::uint32_t material_fallback_count;
    bool source_material_colors_loaded;
    bool color_channel_mapping_valid;
    bool light_transform_valid;
    bool source_approx_link_visible;
    bool safe_link_visible;
    float link_lighting_luminance_min;
    float link_lighting_luminance_mean;
    float link_lighting_luminance_max;
    bool actor_bucket_state_applied;
    std::uint32_t ge_submit_us;
    std::uint32_t ge_sync_us;
    std::uint32_t lighting_cpu_us;
    std::uint32_t shadow_cpu_us;
    std::uint32_t hud_cpu_us;
};

struct MessageOverlayRenderInput {
    const char* text;
    std::uint32_t source_message_id;
    std::uint16_t visible_characters;
    bool active;
    bool awaiting_confirm;
};

struct RealRoomRenderInput {
    Vec3 link_position;
    float link_yaw;
    Vec3 camera_eye;
    Vec3 camera_center;
    // Source vertical field of view in degrees. Zero retains the PSP default.
    float camera_fov;
    Vec3 rubies[5];
    bool ruby_active[5];
    Vec3 interaction;
    presentation::Profile presentation;
    GameplayState ui_state;
    const MessageOverlayRenderInput* message_overlay;
    bool hide_hud;
    PspLinkRootPoseMetrics root_pose;
    const environment::PspMaterialEnvironmentState* environment;
    const shadow::PspShadowSystem* shadows;
    RenderProfile render_profile;
    LightingMode lighting_mode;
    FogMode fog_mode;
    ShadowMode shadow_mode;
};

struct StaticModelRenderView {
    const void* model;
    std::uint32_t model_size;
    const void* textures;
    std::uint32_t texture_size;
    float matrix[12];
    float scale[3];
};

struct OpaqueOrderDrawRecord {
    std::uint32_t source_index;
    std::uint32_t output_slot;
    std::uint16_t actor_id;
    std::uint16_t material_id;
    std::uint16_t shape_id;
    std::uint16_t texture_id;
    std::uint8_t source;
    bool depth_test;
    bool depth_write;
    bool source_order_dependent;
};

struct OpaqueOrderRenderMetrics {
    opaque_order::Variant variant;
    opaque_order::PlanMetrics plan;
    std::uint32_t draw_count;
    std::uint32_t room_draw_count;
    std::uint32_t link_draw_count;
    std::uint32_t excluded_non_writing_draws;
    bool complete_draw_manifest;
    bool complete_state_per_draw;
    OpaqueOrderDrawRecord draws[opaque_order::kMaximumSubmissions];
};

void set_render_trace_sink(render_trace::Sink sink, void* user);
void clear_render_trace_sink();

struct StartupTitleCamera {
    Vec3 eye;
    Vec3 center;
    Vec3 up;
    float fov;
    float near_plane;
    float far_plane;
};

struct StartupNameEntryRenderInput {
    const char* heading;
    const char* name;
    std::uint8_t cursor_row;
    std::uint8_t cursor_column;
    bool lowercase;
};

bool initialize_renderer(
    const PackageView& textures,
    const PackageView& ui,
    void* command_list,
    std::uint32_t command_list_bytes,
    RenderMetrics* metrics);
bool initialize_startup_ui_renderer(
    const startup::UiPackageView& ui,
    void* command_list,
    std::uint32_t command_list_bytes,
    RenderMetrics* metrics);
bool render_startup_ui_frame(
    std::uint16_t channel,
    std::uint8_t fade_alpha,
    RenderMetrics* metrics);
bool render_startup_ui_frame_layers(
    std::uint16_t base_channel,
    std::uint16_t overlay_channel,
    std::uint8_t fade_alpha,
    RenderMetrics* metrics);
bool render_startup_name_entry_frame(
    const StartupNameEntryRenderInput& input,
    RenderMetrics* metrics);
bool initialize_startup_title_renderer(
    const startup::UiPackageView& ui,
    const PackageView& room_textures,
    const void* room_model,
    std::uint32_t room_model_size,
    void* command_list,
    std::uint32_t command_list_bytes,
    RenderMetrics* metrics);
bool render_startup_title_frame(
    const StaticModelRenderView& title_model,
    const StartupTitleCamera& camera,
    bool draw_title_model,
    std::uint16_t ui_channel,
    std::uint8_t fade_alpha,
    RenderMetrics* metrics);
bool render_playable_frame(
    const Runtime& runtime,
    const GameplayState& gameplay,
    RenderMetrics* metrics);
bool initialize_real_room_renderer(
    const PackageView& link_textures,
    const PackageView& room_textures,
    const PackageView& ui,
    const void* room_model,
    std::uint32_t room_model_size,
    void* command_list,
    std::uint32_t command_list_bytes,
    RenderMetrics* metrics);
bool replace_real_room_renderer(
    const PackageView& room_textures,
    const void* room_model,
    std::uint32_t room_model_size,
    RenderMetrics* metrics);
void deactivate_real_room_renderer(RenderMetrics* metrics);
bool render_black_transition_frame(RenderMetrics* metrics);
bool render_real_room_frame(
    const Runtime& runtime,
    const RealRoomRenderInput& input,
    RenderMetrics* metrics);
bool render_real_room_opaque_order_frame(
    const Runtime& runtime,
    const RealRoomRenderInput& input,
    opaque_order::Variant variant,
    std::uint32_t seed,
    OpaqueOrderRenderMetrics* order_metrics,
    RenderMetrics* metrics);
bool render_real_room_frame_with_models(
    const Runtime& runtime,
    const RealRoomRenderInput& input,
    const StaticModelRenderView* models,
    std::uint16_t model_count,
    RenderMetrics* metrics);
bool render_real_actor_frame(
    const Runtime& runtime,
    const RealRoomRenderInput& input,
    const actor::ActorSystem& actors,
    actor::RenderMetrics* actor_metrics,
    RenderMetrics* metrics);
bool verify_playable_frame(RenderMetrics* metrics);
bool verify_real_room_frame(RenderMetrics* metrics);
bool capture_playable_frame_5650(
    void* output, std::uint32_t capacity);
bool capture_playable_depth_16(
    void* output, std::uint32_t capacity);
void shutdown_renderer();

}  // namespace dusk::psp::playable

#endif
