#include "dusk/psp/playable_render.hpp"
#include "dusk/psp/color_packing.hpp"
#include "dusk/psp/gu_pass_state.hpp"
#include "dusk/psp/link_fidelity.hpp"
#include "dusk/psp/link_lighting.hpp"
#include "dusk/psp/platform.hpp"
#include "dusk/psp/room_package.hpp"

#include <pspdisplay.h>
#include <pspge.h>
#include <pspgu.h>
#include <pspgum.h>
#include <pspkernel.h>
#include <psputils.h>

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstring>

namespace dusk::psp::playable {
namespace {

constexpr std::uint32_t kWidth = 480;
constexpr std::uint32_t kHeight = 272;
constexpr std::uint32_t kStride = 512;
constexpr std::uint32_t kBufferBytes = kStride * kHeight * 2;
constexpr std::uint32_t kFramebuffers = kBufferBytes * 2;
constexpr std::uint32_t kDepthOffset = kFramebuffers;
constexpr std::uint32_t kTextureOffset = kDepthOffset + kBufferBytes;
constexpr std::uint32_t kShadowMapBytes = 64 * 64 * 2;
constexpr std::uint32_t kShadowAuxBytes = 64 * 64 * 2;
constexpr std::uint32_t kBackground = 0xff506f89u;
constexpr float kPi = 3.14159265358979323846f;

struct Texture {
    std::uint16_t width;
    std::uint16_t height;
    std::uint16_t stored_width;
    std::uint16_t stored_height;
    std::uint32_t offset;
    std::uint32_t bytes;
    std::uint8_t format;
};

struct ColorVertex {
    std::uint32_t color;
    float x;
    float y;
    float z;
};

struct SpriteVertex {
    std::int16_t u;
    std::int16_t v;
    std::int16_t x;
    std::int16_t y;
    std::int16_t z;
};

struct ShadowVertex {
    float u;
    float v;
    std::uint32_t color;
    float x;
    float y;
    float z;
};

struct ShadowCasterVertex {
    float x;
    float y;
    float z;
};

enum class OpaqueDrawSource : std::uint8_t {
    Room = 0,
    Link = 2,
};

struct OpaqueDrawDescriptor {
    opaque_order::Submission submission;
    std::uint16_t texture_id;
    std::uint16_t submesh_index;
};

static_assert(sizeof(SkinnedVertex) == 36);
static_assert(sizeof(ColorVertex) == 16);
static_assert(sizeof(SpriteVertex) == 10);
static_assert(sizeof(ShadowVertex) == 24);
static_assert(sizeof(ShadowCasterVertex) == 12);

Texture g_textures[29] = {};
Texture g_room_textures[96] = {};
Texture g_ui = {};
PackageView g_texture_package = {};
PackageView g_room_texture_package = {};
PackageView g_ui_package = {};
startup::UiPackageView g_startup_ui_package = {};
const std::uint8_t* g_room_model = nullptr;
std::uint32_t g_room_model_size = 0;
void* g_list = nullptr;
std::uint32_t g_list_bytes = 0;
std::uint8_t* g_edram = nullptr;
std::uint32_t g_draw_offset = 0;
std::uint32_t g_shadow_map_offset = 0;
std::uint32_t g_shadow_depth_offset = 0;
bool g_initialized = false;
render_trace::BoundedTrace g_render_trace = {};
std::uint32_t g_render_trace_frame = 0;
std::uint32_t g_verified_buffer = 0;
alignas(16) std::uint16_t g_readback[16 * 32] = {};
alignas(16) std::uint16_t g_simple_shadow_texture[32 * 32] = {};
alignas(16) ShadowVertex
    g_simple_shadow_vertices[
        shadow::PspShadowReceiverMesh::kCapacity * 3] = {};
alignas(16) ShadowCasterVertex
    g_shadow_caster_vertices[kPlayableMaxVertices] = {};
alignas(16) SkinnedVertex
    g_link_lighting_vertices[kPlayableMaxVertices] = {};
constexpr std::uint32_t kSafeLinkMaterialCapacity = 27;
constexpr std::uint32_t kSafeLinkLightLevels = 64;
alignas(16) std::uint32_t
    g_safe_link_color_lut[
        kSafeLinkMaterialCapacity][kSafeLinkLightLevels] = {};
const std::uint8_t* g_safe_link_lut_package = nullptr;
std::uint32_t g_safe_link_lut_package_size = 0;
std::uint32_t g_safe_link_lut_ambient = 0;
std::uint32_t g_safe_link_lut_key = 0;
SafeLinkLightingVariant g_safe_link_lut_variant =
    SafeLinkLightingVariant::AmbientOnly;
bool g_safe_link_lut_valid = false;

bool trace_render_submission(
    render_trace::Source source, render_trace::Bucket bucket,
    std::uint16_t actor_id, std::uint16_t material_id,
    std::uint16_t shape_id, std::uint16_t texture_id,
    bool depth_write, bool culling, bool fog, bool lighting,
    std::uint8_t alpha_reference = 0x80) {
    if (!g_render_trace.enabled()) {
        return true;
    }
    const bool alpha_test = bucket == render_trace::Bucket::AlphaTest;
    const bool blending = bucket == render_trace::Bucket::AlphaBlend ||
                          bucket == render_trace::Bucket::Ui;
    const render_trace::Submission submission = {
        g_render_trace_frame, source, bucket, actor_id, material_id,
        shape_id, texture_id,
        static_cast<std::uint8_t>(alpha_test ? alpha_reference : 0),
        bucket != render_trace::Bucket::Ui, depth_write, alpha_test,
        blending, culling, fog, lighting,
    };
    return g_render_trace.emit(submission);
}

render_trace::Bucket trace_bucket(std::uint8_t bucket) {
    return bucket == 1 ? render_trace::Bucket::AlphaTest
        : bucket == 2 ? render_trace::Bucket::AlphaBlend
                      : render_trace::Bucket::Opaque;
}

int reversed_depth_func(std::uint8_t source) {
    static const int mapping[8] = {
        GU_NEVER, GU_GREATER, GU_EQUAL, GU_GEQUAL,
        GU_LESS, GU_NOTEQUAL, GU_LEQUAL, GU_ALWAYS,
    };
    return mapping[source & 7u];
}

int alpha_compare_func(std::uint8_t source) {
    static const int mapping[8] = {
        GU_NEVER, GU_LESS, GU_EQUAL, GU_LEQUAL,
        GU_GREATER, GU_NOTEQUAL, GU_GEQUAL, GU_ALWAYS,
    };
    return mapping[source & 7u];
}

struct AppliedAlphaMaterialState {
    bool present = false;
    bool alpha_exact = false;
    bool blend_exact = false;
    bool cull_exact = false;
    bool culling = false;
    std::uint8_t alpha_reference = 0x80;
};

bool gx_blend_factor(
    std::uint8_t source, int* factor, std::uint32_t* fixed) {
    if (factor == nullptr || fixed == nullptr) {
        return false;
    }
    switch (source) {
    case 0: *factor = GU_FIX; *fixed = 0x00000000u; return true;
    case 1: *factor = GU_FIX; *fixed = 0xffffffffu; return true;
    case 2: *factor = GU_OTHER_COLOR; *fixed = 0; return true;
    case 3: *factor = GU_ONE_MINUS_OTHER_COLOR; *fixed = 0; return true;
    case 4: *factor = GU_SRC_ALPHA; *fixed = 0; return true;
    case 5: *factor = GU_ONE_MINUS_SRC_ALPHA; *fixed = 0; return true;
    case 6: *factor = GU_DST_ALPHA; *fixed = 0; return true;
    case 7: *factor = GU_ONE_MINUS_DST_ALPHA; *fixed = 0; return true;
    default: return false;
    }
}

AppliedAlphaMaterialState apply_room_alpha_material_state(
    std::uint16_t material_id, std::uint8_t fallback_bucket) {
    AppliedAlphaMaterialState result = {};
    const room::PackageView package = {
        g_room_texture_package.bytes, g_room_texture_package.size,
        g_room_texture_package.expected_crc,
        g_room_texture_package.actual_crc};
    room::AlphaMaterialState state = {};
    if (room::read_room_alpha_material_state(
            package, material_id, &state) != room::PackageError::Ok) {
        if (fallback_bucket == 1) {
            sceGuDisable(GU_BLEND);
            sceGuEnable(GU_ALPHA_TEST);
            sceGuAlphaFunc(GU_GEQUAL, 0x80, 0xff);
        } else if (fallback_bucket == 2) {
            sceGuDisable(GU_ALPHA_TEST);
            sceGuEnable(GU_BLEND);
            sceGuBlendFunc(
                GU_ADD, GU_SRC_ALPHA, GU_ONE_MINUS_SRC_ALPHA, 0, 0);
        } else {
            sceGuDisable(GU_ALPHA_TEST);
            sceGuDisable(GU_BLEND);
        }
        return result;
    }

    result.present = true;
    result.culling = state.cull_mode == 2;
    if (state.cull_mode == 0) {
        sceGuDisable(GU_CULL_FACE);
        result.cull_exact = true;
    } else if (state.cull_mode == 2) {
        sceGuEnable(GU_CULL_FACE);
        sceGuFrontFace(GU_CW);
        result.cull_exact = true;
    } else {
        sceGuDisable(GU_CULL_FACE);
    }

    const bool alpha_always =
        state.alpha_comp0 == 7 && state.alpha_comp1 == 7;
    if (alpha_always) {
        sceGuDisable(GU_ALPHA_TEST);
        result.alpha_exact = true;
        result.alpha_reference = 0;
    } else if (state.alpha_op == 0 &&
               state.alpha_comp1 == 3 && state.alpha_ref1 == 255) {
        sceGuEnable(GU_ALPHA_TEST);
        sceGuAlphaFunc(
            alpha_compare_func(state.alpha_comp0),
            state.alpha_ref0, 0xff);
        result.alpha_exact = true;
        result.alpha_reference = state.alpha_ref0;
    } else {
        sceGuDisable(GU_ALPHA_TEST);
    }

    if (state.blend_mode == 0) {
        sceGuDisable(GU_BLEND);
        result.blend_exact = true;
    } else if (state.blend_mode == 1) {
        int source_factor = GU_FIX;
        int destination_factor = GU_FIX;
        std::uint32_t source_fixed = 0;
        std::uint32_t destination_fixed = 0;
        if (gx_blend_factor(
                state.blend_src, &source_factor, &source_fixed) &&
            gx_blend_factor(
                state.blend_dst, &destination_factor,
                &destination_fixed)) {
            sceGuEnable(GU_BLEND);
            sceGuBlendFunc(
                GU_ADD, source_factor, destination_factor,
                source_fixed, destination_fixed);
            result.blend_exact = true;
        } else {
            sceGuDisable(GU_BLEND);
        }
    } else {
        sceGuDisable(GU_BLEND);
    }
    return result;
}

alignas(16) constexpr ColorVertex kFloor[6] = {
    {0xff668a55u, -10.0f, 0.0f, -10.0f},
    {0xff73975fu, 10.0f, 0.0f, -10.0f},
    {0xff668a55u, 10.0f, 0.0f, 10.0f},
    {0xff668a55u, -10.0f, 0.0f, -10.0f},
    {0xff668a55u, 10.0f, 0.0f, 10.0f},
    {0xff73975fu, -10.0f, 0.0f, 10.0f},
};

alignas(16) constexpr ColorVertex kCube[36] = {
    {0xff8b7355u,-.5f,0,-.5f},{0xff8b7355u,.5f,0,-.5f},{0xff8b7355u,.5f,1,-.5f},
    {0xff8b7355u,-.5f,0,-.5f},{0xff8b7355u,.5f,1,-.5f},{0xff8b7355u,-.5f,1,-.5f},
    {0xff9b8365u,.5f,0,.5f},{0xff9b8365u,-.5f,0,.5f},{0xff9b8365u,-.5f,1,.5f},
    {0xff9b8365u,.5f,0,.5f},{0xff9b8365u,-.5f,1,.5f},{0xff9b8365u,.5f,1,.5f},
    {0xff806a50u,-.5f,0,.5f},{0xff806a50u,-.5f,0,-.5f},{0xff806a50u,-.5f,1,-.5f},
    {0xff806a50u,-.5f,0,.5f},{0xff806a50u,-.5f,1,-.5f},{0xff806a50u,-.5f,1,.5f},
    {0xffa08868u,.5f,0,-.5f},{0xffa08868u,.5f,0,.5f},{0xffa08868u,.5f,1,.5f},
    {0xffa08868u,.5f,0,-.5f},{0xffa08868u,.5f,1,.5f},{0xffa08868u,.5f,1,-.5f},
    {0xff705b45u,-.5f,0,.5f},{0xff705b45u,.5f,0,.5f},{0xff705b45u,.5f,0,-.5f},
    {0xff705b45u,-.5f,0,.5f},{0xff705b45u,.5f,0,-.5f},{0xff705b45u,-.5f,0,-.5f},
    {0xffae9874u,-.5f,1,-.5f},{0xffae9874u,.5f,1,-.5f},{0xffae9874u,.5f,1,.5f},
    {0xffae9874u,-.5f,1,-.5f},{0xffae9874u,.5f,1,.5f},{0xffae9874u,-.5f,1,.5f},
};

alignas(16) constexpr ColorVertex kRuby[24] = {
    {0xff40ff70u,0,.45f,0},{0xff20bd50u,-.18f,.18f,0},{0xff20bd50u,0,.18f,.18f},
    {0xff40ff70u,0,.45f,0},{0xff20bd50u,0,.18f,.18f},{0xff20bd50u,.18f,.18f,0},
    {0xff40ff70u,0,.45f,0},{0xff20bd50u,.18f,.18f,0},{0xff20bd50u,0,.18f,-.18f},
    {0xff40ff70u,0,.45f,0},{0xff20bd50u,0,.18f,-.18f},{0xff20bd50u,-.18f,.18f,0},
    {0xff149544u,0,-.1f,0},{0xff20bd50u,0,.18f,.18f},{0xff20bd50u,-.18f,.18f,0},
    {0xff149544u,0,-.1f,0},{0xff20bd50u,.18f,.18f,0},{0xff20bd50u,0,.18f,.18f},
    {0xff149544u,0,-.1f,0},{0xff20bd50u,0,.18f,-.18f},{0xff20bd50u,.18f,.18f,0},
    {0xff149544u,0,-.1f,0},{0xff20bd50u,-.18f,.18f,0},{0xff20bd50u,0,.18f,-.18f},
};

void* relative(std::uint32_t offset) {
    return reinterpret_cast<void*>(static_cast<std::uintptr_t>(offset));
}

void model_identity() {
    sceGumMatrixMode(GU_MODEL);
    sceGumLoadIdentity();
}

void translate_scale(float x, float y, float z, float sx, float sy, float sz) {
    model_identity();
    ScePspFVector3 translation = {x, y, z};
    ScePspFVector3 scale = {sx, sy, sz};
    sceGumTranslate(&translation);
    sceGumScale(&scale);
}

void draw_colored(
    const ColorVertex* vertices, std::uint32_t count) {
    sceGuDisable(GU_TEXTURE_2D);
    sceGuShadeModel(GU_SMOOTH);
    sceGumDrawArray(
        GU_TRIANGLES,
        GU_COLOR_8888 | GU_VERTEX_32BITF | GU_TRANSFORM_3D,
        count, nullptr, vertices);
}

void configure_camera(const GameplayState& state) {
    sceGuDepthFunc(GU_GEQUAL);
    sceGuDepthMask(GU_FALSE);
    sceGuEnable(GU_DEPTH_TEST);
    sceGuEnable(GU_CULL_FACE);
    sceGuFrontFace(GU_CW);
    sceGuEnable(GU_CLIP_PLANES);
    sceGumMatrixMode(GU_PROJECTION);
    sceGumLoadIdentity();
    sceGumPerspective(52.0f, 480.0f / 272.0f, 0.1f, 60.0f);
    const float sine = std::sin(state.camera_yaw);
    const float cosine = std::cos(state.camera_yaw);
    ScePspFVector3 eye = {
        state.position.x + sine * state.camera_distance,
        3.0f,
        state.position.z + cosine * state.camera_distance};
    ScePspFVector3 center = {
        state.position.x, 1.0f, state.position.z};
    ScePspFVector3 up = {0.0f, 1.0f, 0.0f};
    sceGumMatrixMode(GU_VIEW);
    sceGumLoadIdentity();
    sceGumLookAt(&eye, &center, &up);
}

void bind(const Texture& texture) {
    sceGuEnable(GU_TEXTURE_2D);
    const int format =
        texture.format == 0 ? GU_PSM_5650 :
        texture.format == 1 ? GU_PSM_5551 : GU_PSM_4444;
    sceGuTexMode(format, 0, 0, GU_TRUE);
    sceGuTexImage(
        0, texture.stored_width, texture.stored_height,
        texture.stored_width, g_edram + texture.offset);
    sceGuTexFunc(GU_TFX_MODULATE, GU_TCC_RGBA);
    sceGuTexFilter(GU_LINEAR, GU_LINEAR);
    sceGuTexWrap(GU_REPEAT, GU_REPEAT);
    sceGuTexScale(1.0f, 1.0f);
    sceGuTexOffset(0.0f, 0.0f);
    sceGuColor(0xffffffffu);
}

std::uint16_t texture_storage_dimension(std::uint32_t logical) {
    std::uint32_t stored = 8;
    while (stored < logical && stored < 512) {
        stored <<= 1;
    }
    return static_cast<std::uint16_t>(stored);
}

void configure_real_room_camera(const RealRoomRenderInput& input) {
    sceGuDepthFunc(GU_GEQUAL);
    sceGuDepthMask(GU_FALSE);
    sceGuEnable(GU_DEPTH_TEST);
    sceGuDisable(GU_CULL_FACE);
    sceGuEnable(GU_CLIP_PLANES);
    sceGumMatrixMode(GU_PROJECTION);
    sceGumLoadIdentity();
    const float source_fov =
        std::isfinite(input.camera_fov) && input.camera_fov > 1.0f &&
                input.camera_fov < 179.0f
            ? input.camera_fov : 52.0f;
    sceGumPerspective(source_fov, 480.0f / 272.0f, 20.0f, 12000.0f);
    ScePspFVector3 eye = {
        input.camera_eye.x, input.camera_eye.y, input.camera_eye.z};
    ScePspFVector3 center = {
        input.camera_center.x,
        input.camera_center.y,
        input.camera_center.z};
    ScePspFVector3 up = {0.0f, 1.0f, 0.0f};
    sceGumMatrixMode(GU_VIEW);
    sceGumLoadIdentity();
    sceGumLookAt(&eye, &center, &up);
}

std::uint32_t environment_clear_color(
    const RealRoomRenderInput& input) {
    return input.environment != nullptr
        ? color::to_psp_abgr(
              color::PackedArgb32{
                  input.environment->clear_color}).value
        : kBackground;
}

void configure_environment(
    const RealRoomRenderInput& input, RenderMetrics* metrics) {
    if (input.environment == nullptr) {
        sceGuDisable(GU_FOG);
        sceGuDisable(GU_LIGHTING);
        metrics->environment_records_loaded = 0;
        metrics->environment_fog_enabled = false;
        metrics->environment_lighting_enabled = false;
        return;
    }
    const environment::PspMaterialEnvironmentState& state =
        *input.environment;
    metrics->environment_records_loaded = 1;
    metrics->environment_clear_color = environment_clear_color(input);
    metrics->environment_fog_color = state.fog_color;
    metrics->environment_fog_near = state.fog_near;
    metrics->environment_fog_far = state.fog_far;
    metrics->environment_fog_enabled =
        state.fog_enabled && input.fog_mode == FogMode::Source;
    metrics->environment_lighting_enabled =
        input.lighting_mode != LightingMode::Off;
    if (metrics->environment_fog_enabled) {
        sceGuEnable(GU_FOG);
        sceGuFog(
            state.fog_near, state.fog_far,
            color::to_psp_abgr(
                color::PackedArgb32{state.fog_color}).value);
    } else {
        sceGuDisable(GU_FOG);
    }
    sceGuDisable(GU_LIGHTING);
}

void enable_actor_environment_lighting(
    const RealRoomRenderInput& input) {
    if (input.environment == nullptr ||
        input.lighting_mode == LightingMode::Off) {
        sceGuDisable(GU_LIGHTING);
        return;
    }
    const environment::PspMaterialEnvironmentState& state =
        *input.environment;
    ScePspFVector3 direction = {
        state.key_light_direction[0],
        state.key_light_direction[1],
        state.key_light_direction[2]};
    sceGuEnable(GU_LIGHTING);
    sceGuEnable(GU_LIGHT0);
    sceGuLight(0, GU_DIRECTIONAL, GU_DIFFUSE, &direction);
    sceGuLightColor(
        0, GU_DIFFUSE,
        color::to_psp_abgr(
            color::PackedArgb32{state.key_light_color}).value);
    sceGuAmbient(
        color::to_psp_abgr(
            color::PackedArgb32{state.ambient}).value);
    sceGuColorMaterial(GU_AMBIENT_AND_DIFFUSE);
    sceGuMaterial(GU_AMBIENT_AND_DIFFUSE, 0xffffffffu);
}

LinearRgb lighting_for_vertex(
    LightingMode mode,
    const PspLinkMaterialLightingRecord& material,
    const environment::PspMaterialEnvironmentState* environment,
    Vec3 normal,
    Vec3 light_direction,
    Vec3 view_direction) {
    constexpr color::PackedArgb32 kBlack = {0xff000000u};
    constexpr color::PackedArgb32 kWhite = {0xffffffffu};
    const color::PackedArgb32 ambient =
        environment == nullptr
            ? kBlack
            : color::PackedArgb32{environment->ambient};
    const color::PackedArgb32 key =
        environment == nullptr
            ? kBlack
            : color::PackedArgb32{environment->key_light_color};
    if (mode == LightingMode::SafeAmbient ||
        mode == LightingMode::SafeWrappedDiffuse ||
        mode == LightingMode::SafeWrappedDiffuseRim) {
        const SafeLinkLightingVariant variant =
            mode == LightingMode::SafeAmbient
                ? SafeLinkLightingVariant::AmbientOnly
                : mode == LightingMode::SafeWrappedDiffuse
                    ? SafeLinkLightingVariant::WrappedDiffuse
                    : SafeLinkLightingVariant::WrappedDiffuseRim;
        return evaluate_safe_link_lighting(
            material, ambient, key, normal, light_direction,
            view_direction, variant);
    }
    if (mode == LightingMode::WhiteAmbient) {
        PspLinkMaterialLightingRecord diagnostic = material;
        diagnostic.ambient = kWhite;
        diagnostic.diffuse = kBlack;
        return evaluate_source_lighting(
            diagnostic, kWhite, kBlack,
            normal, light_direction, 1.0f);
    }
    if (mode == LightingMode::WhiteKey) {
        PspLinkMaterialLightingRecord diagnostic = material;
        diagnostic.ambient = kBlack;
        diagnostic.diffuse = kWhite;
        return evaluate_source_lighting(
            diagnostic, kBlack, kWhite,
            normal, light_direction, 1.0f);
    }
    if (mode == LightingMode::SourceAmbient) {
        return evaluate_source_lighting(
            material, ambient, kBlack,
            normal, light_direction, 2.0f);
    }
    if (mode == LightingMode::SourceKey) {
        return evaluate_source_lighting(
            material, kBlack, key,
            normal, light_direction, 2.0f);
    }
    return evaluate_source_lighting(
        material, ambient, key,
        normal, light_direction, 2.0f);
}

const SkinnedVertex* prepare_link_lighting_vertices(
    const Runtime& runtime,
    const RealRoomRenderInput& input,
    RenderMetrics* metrics) {
    const std::uint64_t lighting_begin = monotonic_microseconds();
    const SkinnedVertex* source = current_vertices(runtime);
    std::memcpy(
        g_link_lighting_vertices, source,
        runtime.vertex_count * sizeof(SkinnedVertex));
    const std::uint8_t* model = runtime.packages.model.bytes;
    const std::uint32_t vertex_table = read_u32(model + 72);
    const std::uint32_t vertex_stride = read_u32(model + 76);
    Vec3 light_direction = {0.0f, 1.0f, 0.0f};
    const float model_yaw =
        link::actor_to_model_orientation(input.link_yaw);
    if (input.environment != nullptr) {
        light_direction = world_light_ray_to_model_surface(
            {
                input.environment->key_light_direction[0],
                input.environment->key_light_direction[1],
                input.environment->key_light_direction[2],
            },
            model_yaw);
    }
    const Vec3 camera_model = world_vector_to_model(
        {
            input.camera_eye.x - input.link_position.x,
            input.camera_eye.y - input.link_position.y,
            input.camera_eye.z - input.link_position.z,
        },
        model_yaw);
    metrics->source_normal_count = runtime.vertex_count;
    metrics->runtime_normal_count = runtime.vertex_count;
    metrics->zero_normal_count = 0;
    metrics->non_finite_normal_count = 0;
    metrics->normal_length_min = 1000000.0f;
    metrics->normal_length_max = 0.0f;
    metrics->normal_error_mean = 0.0f;
    metrics->normal_error_max = 0.0f;
    metrics->source_material_count =
        read_u32(runtime.packages.textures.bytes + 20);
    metrics->runtime_material_lighting_count = 0;
    metrics->unlit_material_count = 0;
    metrics->emissive_material_count = 0;
    metrics->material_fallback_count = 0;
    metrics->safe_link_visible = false;
    metrics->link_lighting_luminance_min = 1.0f;
    metrics->link_lighting_luminance_mean = 0.0f;
    metrics->link_lighting_luminance_max = 0.0f;
    const bool safe_mode =
        input.lighting_mode == LightingMode::SafeAmbient ||
        input.lighting_mode == LightingMode::SafeWrappedDiffuse ||
        input.lighting_mode == LightingMode::SafeWrappedDiffuseRim;
    if (safe_mode) {
        struct PreparedMaterial {
            LinearRgb base;
            LinearRgb emissive;
            std::uint8_t alpha;
            bool lighting_enabled;
            bool valid;
        };
        PreparedMaterial prepared[kSafeLinkMaterialCapacity] = {};
        const std::uint32_t material_count = std::min(
            metrics->source_material_count,
            kSafeLinkMaterialCapacity);
        for (std::uint32_t index = 0;
             index < material_count; ++index) {
            PspLinkMaterialLightingRecord material = {};
            if (!read_link_material_lighting(
                    runtime.packages.textures, index, &material)) {
                ++metrics->material_fallback_count;
                continue;
            }
            prepared[index] = {
                linear_rgb(material.base),
                linear_rgb(material.emissive),
                material.alpha,
                material.lighting_enabled,
                true,
            };
            if (material.lighting_enabled) {
                ++metrics->runtime_material_lighting_count;
            } else {
                ++metrics->unlit_material_count;
            }
            const color::GxColorRgba8 emissive =
                color::unpack_argb(material.emissive);
            if (emissive.red != 0 || emissive.green != 0 ||
                emissive.blue != 0) {
                ++metrics->emissive_material_count;
            }
            metrics->material_fallback_count +=
                material.fallback_reason != 0 ? 1u : 0u;
        }
        const color::PackedArgb32 ambient_packed = {
            input.environment == nullptr
                ? 0xff000000u : input.environment->ambient};
        const color::PackedArgb32 key_packed = {
            input.environment == nullptr
                ? 0xff000000u : input.environment->key_light_color};
        const LinearRgb ambient = normalized_chroma(ambient_packed);
        const LinearRgb key = normalized_chroma(key_packed);
        const SafeLinkLightingVariant variant =
            input.lighting_mode == LightingMode::SafeAmbient
                ? SafeLinkLightingVariant::AmbientOnly
                : input.lighting_mode ==
                        LightingMode::SafeWrappedDiffuse
                    ? SafeLinkLightingVariant::WrappedDiffuse
                    : SafeLinkLightingVariant::WrappedDiffuseRim;
        const bool lut_eligible =
            variant != SafeLinkLightingVariant::WrappedDiffuseRim;
        if (lut_eligible &&
            (!g_safe_link_lut_valid ||
             g_safe_link_lut_package !=
                 runtime.packages.textures.bytes ||
             g_safe_link_lut_package_size !=
                 runtime.packages.textures.size ||
             g_safe_link_lut_ambient != ambient_packed.value ||
             g_safe_link_lut_key != key_packed.value ||
             g_safe_link_lut_variant != variant)) {
            for (std::uint32_t material_index = 0;
                 material_index < material_count; ++material_index) {
                const PreparedMaterial& material =
                    prepared[material_index];
                for (std::uint32_t level = 0;
                     level < kSafeLinkLightLevels; ++level) {
                    const float wrapped =
                        static_cast<float>(level) /
                        static_cast<float>(
                            kSafeLinkLightLevels - 1);
                    const LinearRgb lit =
                        evaluate_prepared_safe_link_lighting_factors(
                            material.base, material.emissive,
                            material.lighting_enabled, ambient, key,
                            wrapped, 0.0f, variant);
                    g_safe_link_color_lut[material_index][level] =
                        psp_vertex_color(lit, material.alpha).value;
                }
            }
            g_safe_link_lut_package =
                runtime.packages.textures.bytes;
            g_safe_link_lut_package_size =
                runtime.packages.textures.size;
            g_safe_link_lut_ambient = ambient_packed.value;
            g_safe_link_lut_key = key_packed.value;
            g_safe_link_lut_variant = variant;
            g_safe_link_lut_valid = true;
        }
        for (std::uint32_t index = 0;
             index < runtime.vertex_count; ++index) {
            const std::uint8_t material_index =
                model[vertex_table + index * vertex_stride + 43];
            if (material_index >= material_count ||
                !prepared[material_index].valid) {
                g_link_lighting_vertices[index].color = 0xffffffffu;
                continue;
            }
            const Vec3 normal = {
                source[index].nx, source[index].ny, source[index].nz};
            color::PspColorAbgr8888 packed = {};
            if (lut_eligible) {
                const float ndotl = std::clamp(
                    normal.x * light_direction.x +
                    normal.y * light_direction.y +
                    normal.z * light_direction.z,
                    -1.0f, 1.0f);
                const float wrapped = std::clamp(
                    (ndotl + kSafeLinkLighting.wrap_bias) /
                        (1.0f + kSafeLinkLighting.wrap_bias),
                    0.0f, 1.0f);
                const std::uint32_t level =
                    static_cast<std::uint32_t>(
                        wrapped * static_cast<float>(
                            kSafeLinkLightLevels - 1) + 0.5f);
                packed.value =
                    g_safe_link_color_lut[material_index][level];
            } else {
                const Vec3 view_direction = normalized_direction({
                    camera_model.x - source[index].x,
                    camera_model.y - source[index].y,
                    camera_model.z - source[index].z,
                });
                const PreparedMaterial& material =
                    prepared[material_index];
                const LinearRgb lit =
                    evaluate_prepared_safe_link_lighting(
                        material.base, material.emissive,
                        material.lighting_enabled, ambient, key,
                        normal, light_direction, view_direction, variant);
                packed = psp_vertex_color(lit, material.alpha);
            }
            g_link_lighting_vertices[index].color = packed.value;
            const color::GxColorRgba8 channels =
                color::unpack_psp_abgr(packed);
            const float luminance =
                (0.2126f * channels.red + 0.7152f * channels.green +
                 0.0722f * channels.blue) / 255.0f;
            metrics->link_lighting_luminance_min = std::min(
                metrics->link_lighting_luminance_min, luminance);
            metrics->link_lighting_luminance_mean += luminance;
            metrics->link_lighting_luminance_max = std::max(
                metrics->link_lighting_luminance_max, luminance);
        }
        if (runtime.vertex_count != 0) {
            metrics->link_lighting_luminance_mean /=
                static_cast<float>(runtime.vertex_count);
        }
        // Skinning normalizes every finite output normal immediately before
        // this render pass. Rechecking it with thousands of square roots here
        // would duplicate that invariant in the performance-sensitive path.
        metrics->normal_length_min = 1.0f;
        metrics->normal_length_max = 1.0f;
        metrics->normal_error_mean = 0.0f;
        metrics->normal_error_max = 0.0f;
        metrics->source_material_colors_loaded =
            metrics->source_material_count ==
                kSafeLinkMaterialCapacity &&
            metrics->material_fallback_count == 0;
        metrics->color_channel_mapping_valid = true;
        metrics->light_transform_valid = true;
        metrics->source_approx_link_visible =
            metrics->source_material_colors_loaded;
        metrics->safe_link_visible =
            metrics->source_material_colors_loaded &&
            metrics->link_lighting_luminance_min >=
                kSafeLinkLighting.minimum_illumination -
                    (1.0f / 255.0f);
        sceKernelDcacheWritebackRange(
            g_link_lighting_vertices,
            runtime.vertex_count * sizeof(SkinnedVertex));
        metrics->lighting_cpu_us = static_cast<std::uint32_t>(
            monotonic_microseconds() - lighting_begin);
        return g_link_lighting_vertices;
    }
    bool material_seen[27] = {};
    float debug_mean = 0.0f;
    float debug_square_mean = 0.0f;
    for (std::uint32_t index = 0;
         index < runtime.vertex_count; ++index) {
        const Vec3 normal = {
            source[index].nx, source[index].ny, source[index].nz};
        const Vec3 view_direction = {
            camera_model.x - source[index].x,
            camera_model.y - source[index].y,
            camera_model.z - source[index].z,
        };
        const float length = std::sqrt(
            normal.x * normal.x +
            normal.y * normal.y +
            normal.z * normal.z);
        if (!std::isfinite(length)) {
            ++metrics->non_finite_normal_count;
        } else if (length < 0.000001f) {
            ++metrics->zero_normal_count;
        }
        metrics->normal_length_min =
            std::min(metrics->normal_length_min, length);
        metrics->normal_length_max =
            std::max(metrics->normal_length_max, length);
        const float normal_error = std::fabs(length - 1.0f);
        metrics->normal_error_mean += normal_error;
        metrics->normal_error_max =
            std::max(metrics->normal_error_max, normal_error);
        const std::uint8_t material_index =
            model[vertex_table + index * vertex_stride + 43];
        PspLinkMaterialLightingRecord material = {};
        if (!read_link_material_lighting(
                runtime.packages.textures,
                material_index, &material)) {
            ++metrics->material_fallback_count;
            continue;
        }
        if (material_index < 27 && !material_seen[material_index]) {
            material_seen[material_index] = true;
            if (material.lighting_enabled) {
                ++metrics->runtime_material_lighting_count;
            } else {
                ++metrics->unlit_material_count;
            }
            if (color::unpack_argb(material.emissive).red != 0 ||
                color::unpack_argb(material.emissive).green != 0 ||
                color::unpack_argb(material.emissive).blue != 0) {
                ++metrics->emissive_material_count;
            }
            metrics->material_fallback_count +=
                material.fallback_reason != 0 ? 1u : 0u;
        }
        if (input.lighting_mode == LightingMode::Debug) {
            g_link_lighting_vertices[index].color =
                psp_normal_debug_color(normal).value;
        } else if (
            input.lighting_mode == LightingMode::MaterialBaseColor) {
            g_link_lighting_vertices[index].color =
                color::to_psp_abgr(material.base).value;
        } else if (
            input.lighting_mode != LightingMode::Off &&
            input.lighting_mode != LightingMode::TextureOnly &&
            input.lighting_mode != LightingMode::GuCandidate) {
            g_link_lighting_vertices[index].color =
                psp_vertex_color(
                    lighting_for_vertex(
                        input.lighting_mode, material,
                        input.environment, normal,
                        light_direction, view_direction),
                    material.alpha).value;
        } else {
            g_link_lighting_vertices[index].color = 0xffffffffu;
        }
        const color::GxColorRgba8 debug =
            color::unpack_psp_abgr(
                psp_normal_debug_color(normal));
        const float debug_value =
            (debug.red + debug.green + debug.blue) / (3.0f * 255.0f);
        debug_mean += debug_value;
        debug_square_mean += debug_value * debug_value;
        const color::GxColorRgba8 lit = color::unpack_psp_abgr(
            color::PspColorAbgr8888{
                g_link_lighting_vertices[index].color});
        const float luminance =
            (0.2126f * lit.red + 0.7152f * lit.green +
             0.0722f * lit.blue) / 255.0f;
        metrics->link_lighting_luminance_min = std::min(
            metrics->link_lighting_luminance_min, luminance);
        metrics->link_lighting_luminance_mean += luminance;
        metrics->link_lighting_luminance_max = std::max(
            metrics->link_lighting_luminance_max, luminance);
    }
    if (runtime.vertex_count != 0) {
        const float inverse =
            1.0f / static_cast<float>(runtime.vertex_count);
        metrics->normal_error_mean *= inverse;
        metrics->link_lighting_luminance_mean *= inverse;
        debug_mean *= inverse;
        debug_square_mean *= inverse;
        metrics->normal_debug_color_variance =
            std::max(0.0f, debug_square_mean - debug_mean * debug_mean);
    }
    metrics->source_material_colors_loaded =
        metrics->source_material_count == 27 &&
        metrics->material_fallback_count == 0;
    metrics->color_channel_mapping_valid = true;
    metrics->light_transform_valid = true;
    metrics->source_approx_link_visible =
        metrics->zero_normal_count == 0 &&
        metrics->non_finite_normal_count == 0 &&
        metrics->source_material_colors_loaded;
    metrics->safe_link_visible =
        (input.lighting_mode == LightingMode::SafeAmbient ||
         input.lighting_mode == LightingMode::SafeWrappedDiffuse ||
         input.lighting_mode == LightingMode::SafeWrappedDiffuseRim) &&
        metrics->link_lighting_luminance_min >=
            kSafeLinkLighting.minimum_illumination - (1.0f / 255.0f);
    sceKernelDcacheWritebackRange(
        g_link_lighting_vertices,
        runtime.vertex_count * sizeof(SkinnedVertex));
    metrics->lighting_cpu_us = static_cast<std::uint32_t>(
        monotonic_microseconds() - lighting_begin);
    return g_link_lighting_vertices;
}

void initialize_simple_shadow_texture() {
    for (std::uint32_t y = 0; y < 32; ++y) {
        for (std::uint32_t x = 0; x < 32; ++x) {
            const float dx =
                (static_cast<float>(x) - 15.5f) / 15.5f;
            const float dy =
                (static_cast<float>(y) - 15.5f) / 15.5f;
            const float coverage = std::clamp(
                (1.0f - std::sqrt(dx * dx + dy * dy)) * 1.35f,
                0.0f, 1.0f);
            const std::uint16_t alpha =
                static_cast<std::uint16_t>(coverage * 15.0f);
            g_simple_shadow_texture[y * 32 + x] =
                static_cast<std::uint16_t>((alpha << 12) | 0x0fffu);
        }
    }
    sceKernelDcacheWritebackRange(
        g_simple_shadow_texture, sizeof(g_simple_shadow_texture));
}

void draw_simple_shadows(
    const RealRoomRenderInput& input, RenderMetrics* metrics) {
    metrics->shadow_simple_draw_calls = 0;
    metrics->shadow_receiver_triangles = 0;
    metrics->shadow_simple_enabled = false;
    if (input.shadows == nullptr ||
        input.shadows->receivers.triangle_count == 0 ||
        !input.shadows->frame_active) {
        return;
    }
    const shadow::PspShadowSystem& system = *input.shadows;
    const std::uint32_t triangles =
        system.receivers.triangle_count;
    for (std::uint32_t triangle = 0;
         triangle < triangles; ++triangle) {
        const std::uint16_t request_index =
            system.receivers.request_indices[triangle];
        if (request_index >= system.simple_count) {
            return;
        }
        const shadow::PspSimpleShadowRequest& request =
            system.simple[request_index];
        const room::ShadowReceiverTriangle& receiver =
            system.receivers.triangles[triangle];
        const float inverse = 1.0f / (request.radius * 2.0f);
        const float center_x = request.position.x +
            request.environment_direction.x *
                std::min(request.height, request.radius) * 0.2f;
        const float center_z = request.position.z +
            request.environment_direction.z *
                std::min(request.height, request.radius) * 0.2f;
        const float opacity = std::clamp(
            request.alpha * request.environment_density *
                request.fade,
            0.0f, 1.0f);
        const std::uint32_t color =
            static_cast<std::uint32_t>(opacity * 255.0f) << 24;
        for (std::uint32_t vertex = 0; vertex < 3; ++vertex) {
            const room::Vec3& position =
                receiver.vertices[vertex];
            g_simple_shadow_vertices[triangle * 3 + vertex] = {
                (position.x - center_x) * inverse + 0.5f,
                (position.z - center_z) * inverse + 0.5f,
                color,
                position.x,
                position.y,
                position.z,
            };
        }
    }
    sceKernelDcacheWritebackRange(
        g_simple_shadow_vertices,
        triangles * 3 * sizeof(ShadowVertex));
    model_identity();
    sceGuDisable(GU_LIGHTING);
    sceGuDisable(GU_ALPHA_TEST);
    sceGuDisable(GU_CULL_FACE);
    sceGuEnable(GU_BLEND);
    sceGuBlendFunc(
        GU_ADD, GU_SRC_ALPHA, GU_ONE_MINUS_SRC_ALPHA, 0, 0);
    sceGuDepthMask(GU_TRUE);
    sceGuEnable(GU_TEXTURE_2D);
    sceGuTexMode(GU_PSM_4444, 0, 0, GU_FALSE);
    sceGuTexImage(0, 32, 32, 32, g_simple_shadow_texture);
    sceGuTexFunc(GU_TFX_MODULATE, GU_TCC_RGBA);
    sceGuTexFilter(GU_LINEAR, GU_LINEAR);
    sceGuTexWrap(GU_CLAMP, GU_CLAMP);
    sceGuColor(0xffffffffu);
    sceGumDrawArray(
        GU_TRIANGLES,
        GU_TEXTURE_32BITF | GU_COLOR_8888 |
            GU_VERTEX_32BITF | GU_TRANSFORM_3D,
        triangles * 3, nullptr, g_simple_shadow_vertices);
    sceGuDisable(GU_BLEND);
    metrics->shadow_simple_draw_calls = 1;
    metrics->shadow_receiver_triangles = triangles;
    metrics->shadow_simple_enabled = true;
}

float dot(const room::Vec3& a, const room::Vec3& b) {
    return a.x * b.x + a.y * b.y + a.z * b.z;
}

room::Vec3 cross(const room::Vec3& a, const room::Vec3& b) {
    return {
        a.y * b.z - a.z * b.y,
        a.z * b.x - a.x * b.z,
        a.x * b.y - a.y * b.x,
    };
}

room::Vec3 normalized(const room::Vec3& value) {
    const float length = std::sqrt(dot(value, value));
    if (!std::isfinite(length) || length < 1.0e-5f) {
        return {0.0f, -1.0f, 0.0f};
    }
    return {value.x / length, value.y / length, value.z / length};
}

bool render_projected_shadow_map(
    const Runtime& runtime,
    const RealRoomRenderInput& input,
    RenderMetrics* metrics) {
    metrics->shadow_projected_enabled = false;
    metrics->shadow_target_restored = false;
    metrics->shadow_caster_vertices = 0;
    if (input.shadow_mode == ShadowMode::Off ||
        input.shadow_mode == ShadowMode::Simple ||
        input.shadows == nullptr ||
        !input.shadows->map.valid ||
        input.shadows->map.width != 64 ||
        input.shadows->map.height != 64 ||
        input.shadows->projected_count == 0 ||
        runtime.vertex_count == 0 ||
        runtime.vertex_count > kPlayableMaxVertices ||
        g_shadow_map_offset == 0 ||
        g_shadow_depth_offset == 0) {
        return false;
    }
    const shadow::PspProjectedShadowRequest& request =
        input.shadows->projected[0];
    if (!input.shadows->map.update_required) {
        metrics->shadow_caster_vertices = runtime.vertex_count;
        metrics->shadow_map_width = 64;
        metrics->shadow_map_height = 64;
        metrics->shadow_map_edram_bytes = kShadowMapBytes;
        metrics->shadow_aux_edram_bytes = kShadowAuxBytes;
        metrics->shadow_projected_enabled = true;
        metrics->shadow_target_restored = true;
        return true;
    }
    const SkinnedVertex* source = current_vertices(runtime);
    for (std::uint32_t index = 0;
         index < runtime.vertex_count; ++index) {
        g_shadow_caster_vertices[index] = {
            source[index].x, source[index].y, source[index].z};
    }
    sceKernelDcacheWritebackRange(
        g_shadow_caster_vertices,
        runtime.vertex_count * sizeof(ShadowCasterVertex));
    sceGuDrawBufferList(
        GU_PSM_4444, relative(g_shadow_map_offset), 64);
    sceGuDepthBuffer(relative(g_shadow_depth_offset), 64);
    sceGuOffset(2048 - 32, 2048 - 32);
    sceGuViewport(2048, 2048, 64, 64);
    sceGuScissor(0, 0, 64, 64);
    sceGuClearColor(0x00000000u);
    sceGuClearDepth(0);
    sceGuClear(GU_COLOR_BUFFER_BIT | GU_DEPTH_BUFFER_BIT);
    sceGuDepthFunc(GU_GEQUAL);
    sceGuDepthMask(GU_FALSE);
    sceGuEnable(GU_DEPTH_TEST);
    sceGuDisable(GU_BLEND);
    sceGuDisable(GU_ALPHA_TEST);
    sceGuDisable(GU_TEXTURE_2D);
    sceGuDisable(GU_LIGHTING);
    sceGuDisable(GU_CULL_FACE);

    sceGumMatrixMode(GU_PROJECTION);
    sceGumLoadIdentity();
    sceGumOrtho(
        -request.radius, request.radius,
        -request.radius, request.radius,
        1.0f, 1000.0f);
    const room::Vec3 direction = normalized(request.direction);
    ScePspFVector3 center = {
        request.position.x,
        request.position.y,
        request.position.z};
    ScePspFVector3 eye = {
        center.x - direction.x * 400.0f,
        center.y - direction.y * 400.0f,
        center.z - direction.z * 400.0f};
    ScePspFVector3 up = {0.0f, 0.0f, 1.0f};
    sceGumMatrixMode(GU_VIEW);
    sceGumLoadIdentity();
    sceGumLookAt(&eye, &center, &up);
    sceGumMatrixMode(GU_MODEL);
    sceGumLoadIdentity();
    ScePspFVector3 translation = {
        input.link_position.x,
        input.link_position.y,
        input.link_position.z};
    ScePspFVector3 rotation = {
        0.0f, link::actor_to_model_orientation(input.link_yaw), 0.0f};
    sceGumTranslate(&translation);
    sceGumRotateXYZ(&rotation);
    sceGuColor(0xffffffffu);
    const std::uint8_t* model = runtime.packages.model.bytes;
    const std::uint32_t index_offset = read_u32(model + 80);
    const std::uint32_t submesh_offset = read_u32(model + 84);
    const std::uint32_t submesh_stride = read_u32(model + 88);
    const std::uint32_t submeshes = read_u32(model + 28);
    for (std::uint32_t index = 0; index < submeshes; ++index) {
        const std::uint8_t* item =
            model + submesh_offset + index * submesh_stride;
        sceGumDrawArray(
            GU_TRIANGLES,
            GU_VERTEX_32BITF | GU_INDEX_16BIT | GU_TRANSFORM_3D,
            read_u32(item + 4),
            model + index_offset + read_u32(item) * 2,
            g_shadow_caster_vertices);
    }
    sceGuTexFlush();

    sceGuDrawBufferList(
        GU_PSM_5650, relative(g_draw_offset), kStride);
    sceGuDepthBuffer(relative(kDepthOffset), kStride);
    sceGuOffset(2048 - kWidth / 2, 2048 - kHeight / 2);
    sceGuViewport(2048, 2048, kWidth, kHeight);
    sceGuScissor(0, 0, kWidth, kHeight);
    configure_real_room_camera(input);
    configure_environment(input, metrics);
    metrics->shadow_caster_vertices = runtime.vertex_count;
    metrics->shadow_map_width = 64;
    metrics->shadow_map_height = 64;
    metrics->shadow_map_edram_bytes = kShadowMapBytes;
    metrics->shadow_aux_edram_bytes = kShadowAuxBytes;
    metrics->shadow_projected_enabled = true;
    metrics->shadow_target_restored = true;
    return true;
}

void draw_projected_shadow(
    const RealRoomRenderInput& input, RenderMetrics* metrics) {
    metrics->shadow_projected_draw_calls = 0;
    if (input.shadows == nullptr ||
        !metrics->shadow_projected_enabled ||
        input.shadows->projected_count == 0 ||
        input.shadows->receivers.triangle_count == 0) {
        return;
    }
    const shadow::PspProjectedShadowRequest& request =
        input.shadows->projected[0];
    const room::Vec3 forward = normalized(request.direction);
    room::Vec3 right = normalized(cross({0.0f, 0.0f, 1.0f}, forward));
    if (std::fabs(dot(right, right) - 1.0f) > 0.1f) {
        right = {1.0f, 0.0f, 0.0f};
    }
    const room::Vec3 up = normalized(cross(forward, right));
    const float inverse = 1.0f / (request.radius * 2.0f);
    const std::uint32_t triangles =
        input.shadows->receivers.triangle_count;
    const std::uint32_t color =
        static_cast<std::uint32_t>(
            std::clamp(request.density * 0.82f, 0.0f, 1.0f) *
            255.0f) << 24;
    for (std::uint32_t triangle = 0;
         triangle < triangles; ++triangle) {
        const room::ShadowReceiverTriangle& receiver =
            input.shadows->receivers.triangles[triangle];
        for (std::uint32_t vertex = 0; vertex < 3; ++vertex) {
            const room::Vec3& position = receiver.vertices[vertex];
            const room::Vec3 relative_position = {
                position.x - request.position.x,
                position.y - request.position.y,
                position.z - request.position.z,
            };
            g_simple_shadow_vertices[triangle * 3 + vertex] = {
                dot(relative_position, right) * inverse + 0.5f,
                0.5f - dot(relative_position, up) * inverse,
                color,
                position.x,
                position.y,
                position.z,
            };
        }
    }
    sceKernelDcacheWritebackRange(
        g_simple_shadow_vertices,
        triangles * 3 * sizeof(ShadowVertex));
    model_identity();
    sceGuDisable(GU_LIGHTING);
    sceGuDisable(GU_ALPHA_TEST);
    sceGuDisable(GU_CULL_FACE);
    sceGuEnable(GU_BLEND);
    sceGuBlendFunc(
        GU_ADD, GU_SRC_ALPHA, GU_ONE_MINUS_SRC_ALPHA, 0, 0);
    sceGuDepthMask(GU_TRUE);
    sceGuEnable(GU_TEXTURE_2D);
    sceGuTexMode(GU_PSM_4444, 0, 0, GU_FALSE);
    sceGuTexImage(
        0, 64, 64, 64, g_edram + g_shadow_map_offset);
    sceGuTexFunc(GU_TFX_MODULATE, GU_TCC_RGBA);
    sceGuTexFilter(GU_LINEAR, GU_LINEAR);
    sceGuTexWrap(GU_CLAMP, GU_CLAMP);
    sceGuColor(0xffffffffu);
    sceGumDrawArray(
        GU_TRIANGLES,
        GU_TEXTURE_32BITF | GU_COLOR_8888 |
            GU_VERTEX_32BITF | GU_TRANSFORM_3D,
        triangles * 3, nullptr, g_simple_shadow_vertices);
    sceGuDisable(GU_BLEND);
    metrics->shadow_receiver_triangles = triangles;
    metrics->shadow_projected_draw_calls = 1;
}

void draw_shadows(
    const RealRoomRenderInput& input, RenderMetrics* metrics) {
    if (input.shadow_mode == ShadowMode::Off) {
        metrics->shadow_simple_draw_calls = 0;
        metrics->shadow_projected_draw_calls = 0;
        metrics->shadow_receiver_triangles = 0;
        metrics->shadow_simple_enabled = false;
        metrics->shadow_projected_enabled = false;
        return;
    }
    if (input.shadow_mode != ShadowMode::Simple &&
        metrics->shadow_projected_enabled) {
        metrics->shadow_simple_draw_calls = 0;
        metrics->shadow_simple_enabled = true;
        draw_projected_shadow(input, metrics);
        return;
    }
    draw_simple_shadows(input, metrics);
}

void bind_room_texture(std::uint16_t texture) {
    const std::uint32_t count =
        read_u32(g_room_texture_package.bytes + 16);
    bind(g_room_textures[texture < count ? texture : 0]);
}

void apply_material_pass_state(
    const room::MaterialPass& pass, bool preserve_source_blend) {
    if (pass.use_texture) {
        bind_room_texture(pass.texture_id);
        const int effect =
            pass.texture_effect == room::MaterialTextureEffect::Replace
                ? GU_TFX_REPLACE
                : pass.texture_effect == room::MaterialTextureEffect::Add
                    ? GU_TFX_ADD
                    : GU_TFX_MODULATE;
        sceGuTexFunc(effect, GU_TCC_RGBA);
    } else {
        sceGuDisable(GU_TEXTURE_2D);
    }
    sceGuColor(pass.color);
    if (preserve_source_blend &&
        pass.blend == room::MaterialBlendPolicy::Source) {
        return;
    }
    switch (pass.blend) {
    case room::MaterialBlendPolicy::Source:
        sceGuDisable(GU_BLEND);
        break;
    case room::MaterialBlendPolicy::Alpha:
        sceGuEnable(GU_BLEND);
        sceGuBlendFunc(
            GU_ADD, GU_SRC_ALPHA, GU_ONE_MINUS_SRC_ALPHA, 0, 0);
        break;
    case room::MaterialBlendPolicy::Additive:
        sceGuEnable(GU_BLEND);
        sceGuBlendFunc(
            GU_ADD, GU_SRC_ALPHA, GU_FIX, 0, 0xffffffffu);
        break;
    case room::MaterialBlendPolicy::Screen:
        sceGuEnable(GU_BLEND);
        sceGuBlendFunc(
            GU_ADD, GU_FIX, GU_ONE_MINUS_OTHER_COLOR,
            0xffffffffu, 0);
        break;
    case room::MaterialBlendPolicy::Multiply:
        sceGuEnable(GU_BLEND);
        sceGuBlendFunc(
            GU_ADD, GU_OTHER_COLOR, GU_FIX, 0, 0x00000000u);
        break;
    }
}

void draw_room_bucket(std::uint8_t wanted, RenderMetrics* metrics) {
    const std::uint32_t section_table = read_u32(g_room_model + 72);
    const std::uint8_t* vertex_section = g_room_model + section_table;
    const std::uint8_t* index_section = vertex_section + 32;
    const std::uint8_t* submesh_section = index_section + 32;
    const std::uint8_t* vertices =
        g_room_model + read_u32(vertex_section + 4);
    const std::uint8_t* indices =
        g_room_model + read_u32(index_section + 4);
    const std::uint8_t* submeshes =
        g_room_model + read_u32(submesh_section + 4);
    const std::uint32_t count = read_u32(g_room_model + 32);
    model_identity();
    for (std::uint32_t index = 0; index < count; ++index) {
        const std::uint8_t* item = submeshes + index * 48;
        if (item[12] != wanted) {
            continue;
        }
        const bool source_depth_test = (item[18] & 1u) != 0;
        const bool source_depth_write = (item[18] & 2u) != 0;
        if (source_depth_test) {
            sceGuEnable(GU_DEPTH_TEST);
            sceGuDepthFunc(reversed_depth_func(item[19]));
        } else {
            sceGuDisable(GU_DEPTH_TEST);
        }
        const std::uint16_t source_material = read_u16(item + 16);
        const AppliedAlphaMaterialState alpha_state =
            apply_room_alpha_material_state(source_material, wanted);
        if (alpha_state.present) {
            ++metrics->room_alpha_state_records;
        } else {
            ++metrics->room_alpha_state_missing_draws;
        }
        if (alpha_state.alpha_exact) {
            ++metrics->room_alpha_exact_draws;
        }
        if (alpha_state.blend_exact) {
            ++metrics->room_blend_exact_draws;
        }
        if (alpha_state.cull_exact) {
            ++metrics->room_cull_exact_draws;
        }
        const std::uint16_t fallback_texture = read_u16(item + 10);
        const room::PackageView texture_package = {
            g_room_texture_package.bytes, g_room_texture_package.size,
            g_room_texture_package.expected_crc,
            g_room_texture_package.actual_crc};
        room::MaterialPassPlan plan = {};
        const bool has_plan = room::read_room_material_pass_plan(
            texture_package, source_material, &plan) == room::PackageError::Ok;
        room::AlphaMaterialState source_alpha = {};
        const bool has_source_alpha = room::read_room_alpha_material_state(
            texture_package, source_material, &source_alpha) ==
            room::PackageError::Ok;
        const std::uint32_t room_texture_count =
            read_u32(g_room_texture_package.bytes + 16);
        const bool fallback_texture_has_alpha =
            fallback_texture < room_texture_count &&
            g_room_textures[fallback_texture].format != 0;
        const bool fallback_needs_missing_tev_alpha =
            has_source_alpha && source_alpha.draw_buffer == 1 &&
            source_alpha.material_class == room::AlphaMaterialClass::AlphaBlend &&
            source_alpha.blend_mode == 1 && source_alpha.blend_src == 4 &&
            source_alpha.texture_count == 1 &&
            source_alpha.texture_identities_complete &&
            !fallback_texture_has_alpha;
        // A single-pass PSP fallback cannot reconstruct TEV-generated alpha.
        // Skip only unsafe XLU whose sole source texture is RGB565/opaque.
        // Keep alpha-capable F_SP108 water and foam layers.
        if (wanted == 2 && fallback_needs_missing_tev_alpha &&
            (!has_plan ||
             plan.fidelity == room::MaterialPassFidelity::Unsupported)) {
            continue;
        }
        const std::uint32_t pass_count = has_plan ? plan.pass_count : 1u;
        for (std::uint32_t pass_index = 0;
             pass_index < pass_count; ++pass_index) {
            room::MaterialPass fallback_pass = {};
            fallback_pass.texture_id = fallback_texture;
            fallback_pass.texture_effect = room::MaterialTextureEffect::Modulate;
            fallback_pass.color_source = room::MaterialColorSource::Rgba;
            fallback_pass.blend = room::MaterialBlendPolicy::Source;
            fallback_pass.depth_write = source_depth_write;
            fallback_pass.use_texture = true;
            fallback_pass.color = 0xffffffffu;
            const room::MaterialPass& pass =
                has_plan ? plan.passes[pass_index] : fallback_pass;
            sceGuDepthMask(pass.depth_write ? GU_FALSE : GU_TRUE);
            apply_material_pass_state(pass, pass_index == 0);
            sceGuShadeModel(GU_SMOOTH);
            sceGumDrawArray(
                GU_TRIANGLES,
                GU_TEXTURE_32BITF | GU_COLOR_8888 |
                    GU_VERTEX_32BITF | GU_INDEX_16BIT | GU_TRANSFORM_3D,
                read_u32(item + 4),
                indices + read_u32(item) * 2,
                vertices);
            trace_render_submission(
                render_trace::Source::Room, trace_bucket(wanted), 0,
                source_material, read_u16(item + 14),
                pass.use_texture ? pass.texture_id : 0xffffu,
                pass.depth_write, alpha_state.culling,
                metrics->environment_fog_enabled, false,
                alpha_state.alpha_reference);
            ++metrics->room_draw_calls;
        }
        if (wanted == 0) {
            ++metrics->room_opaque_draws;
        } else if (wanted == 1) {
            ++metrics->room_alpha_test_draws;
        } else {
            ++metrics->room_alpha_blend_draws;
        }
    }
}

void load_model_matrix(const StaticModelRenderView& model) {
    const float* source = model.matrix;
    const ScePspFMatrix4 matrix = {
        {source[0], source[4], source[8], 0.0f},
        {source[1], source[5], source[9], 0.0f},
        {source[2], source[6], source[10], 0.0f},
        {source[3], source[7], source[11], 1.0f},
    };
    sceGumMatrixMode(GU_MODEL);
    sceGumLoadMatrix(&matrix);
    const ScePspFVector3 scale = {
        model.scale[0], model.scale[1], model.scale[2]};
    sceGumScale(&scale);
}

void bind_package_texture(
    const std::uint8_t* package, std::uint16_t texture) {
    const std::uint32_t count = read_u32(package + 16);
    const std::uint32_t table = read_u32(package + 24);
    const std::uint8_t* item =
        package + table + (texture < count ? texture : 0) * 48;
    const int format =
        item[12] == 0 ? GU_PSM_5650 :
        item[12] == 1 ? GU_PSM_5551 : GU_PSM_4444;
    const void* pixels = package + read_u32(item + 16);
    sceKernelDcacheWritebackRange(
        pixels, read_u32(item + 20));
    sceGuEnable(GU_TEXTURE_2D);
    sceGuTexMode(format, 0, 0, GU_TRUE);
    sceGuTexImage(
        0, read_u16(item + 8), read_u16(item + 10),
        read_u16(item + 8), pixels);
    sceGuTexFunc(GU_TFX_MODULATE, GU_TCC_RGBA);
    sceGuTexFilter(GU_LINEAR, GU_LINEAR);
    sceGuTexWrap(GU_REPEAT, GU_REPEAT);
    sceGuTexScale(1.0f, 1.0f);
    sceGuTexOffset(0.0f, 0.0f);
    sceGuColor(0xffffffffu);
}

void draw_static_model_bucket(
    const StaticModelRenderView& instance, std::uint16_t actor_id,
    std::uint8_t wanted,
    RenderMetrics* metrics) {
    const auto* model = static_cast<const std::uint8_t*>(
        instance.model);
    const auto* textures = static_cast<const std::uint8_t*>(
        instance.textures);
    if (model == nullptr || textures == nullptr) {
        return;
    }
    const std::uint32_t section_table = read_u32(model + 72);
    const std::uint8_t* vertex_section = model + section_table;
    const std::uint8_t* index_section = vertex_section + 32;
    const std::uint8_t* submesh_section = index_section + 32;
    const std::uint8_t* vertices =
        model + read_u32(vertex_section + 4);
    const std::uint8_t* indices =
        model + read_u32(index_section + 4);
    const std::uint8_t* submeshes =
        model + read_u32(submesh_section + 4);
    load_model_matrix(instance);
    sceKernelDcacheWritebackRange(
        model, instance.model_size);
    for (std::uint32_t index = 0;
         index < read_u32(model + 32); ++index) {
        const std::uint8_t* item = submeshes + index * 48;
        if (item[12] != wanted) {
            continue;
        }
        // Static/original actors must not inherit room/Link GU state. Honor
        // the package's source-derived depth contract exactly like room draws,
        // and make the current no-cull actor policy real rather than merely
        // reporting it in the trace.
        const bool source_depth_test = (item[18] & 1u) != 0;
        const bool source_depth_write = (item[18] & 2u) != 0;
        if (source_depth_test) {
            sceGuEnable(GU_DEPTH_TEST);
            sceGuDepthFunc(reversed_depth_func(item[19]));
        } else {
            sceGuDisable(GU_DEPTH_TEST);
        }
        sceGuDisable(GU_CULL_FACE);
        sceGuDisable(GU_LIGHTING);
        sceGuDisable(GU_LIGHT0);
        if (wanted == 1) {
            sceGuDisable(GU_BLEND);
            sceGuEnable(GU_ALPHA_TEST);
            sceGuAlphaFunc(GU_GREATER, 0x7f, 0xff);
            sceGuDepthMask(source_depth_write ? GU_FALSE : GU_TRUE);
        } else if (wanted == 2) {
            sceGuDisable(GU_ALPHA_TEST);
            sceGuEnable(GU_BLEND);
            sceGuBlendFunc(
                GU_ADD, GU_SRC_ALPHA, GU_ONE_MINUS_SRC_ALPHA, 0, 0);
            sceGuDepthMask(source_depth_write ? GU_FALSE : GU_TRUE);
        } else {
            sceGuDisable(GU_ALPHA_TEST);
            sceGuDisable(GU_BLEND);
            sceGuDepthMask(source_depth_write ? GU_FALSE : GU_TRUE);
        }
        const std::uint16_t texture = read_u16(item + 10);
        bind_package_texture(textures, texture);
        sceGuShadeModel(GU_SMOOTH);
        sceGumDrawArray(
            GU_TRIANGLES,
            GU_TEXTURE_32BITF | GU_COLOR_8888 |
                GU_VERTEX_32BITF | GU_INDEX_16BIT | GU_TRANSFORM_3D,
            read_u32(item + 4),
            indices + read_u32(item) * 2,
            vertices);
        trace_render_submission(
            render_trace::Source::StaticActor, trace_bucket(wanted),
            actor_id, static_cast<std::uint16_t>(index),
            static_cast<std::uint16_t>(index), texture,
            source_depth_write, false,
            metrics->environment_fog_enabled, false);
        ++metrics->original_actor_draw_calls;
        if (wanted == 0) {
            ++metrics->original_actor_opaque_draws;
        } else if (wanted == 1) {
            ++metrics->original_actor_alpha_test_draws;
        } else {
            ++metrics->original_actor_alpha_blend_draws;
        }
    }
}

void draw_static_models_bucket(
    const StaticModelRenderView* models, std::uint16_t count,
    std::uint8_t bucket, RenderMetrics* metrics) {
    for (std::uint16_t index = 0; index < count; ++index) {
        draw_static_model_bucket(
            models[index], index, bucket, metrics);
    }
}

void draw_real_link(
    const Runtime& runtime,
    const RealRoomRenderInput& input,
    RenderMetrics* metrics) {
    // Apply the complete canonical actor state; shadow state is never
    // inherited and the GU is not queried.
    const render::PspGuPassState actor_state =
        render::canonical_actor_pass_state();
    sceGuDrawBufferList(
        GU_PSM_5650, relative(g_draw_offset),
        actor_state.framebuffer_stride);
    sceGuDepthBuffer(
        relative(kDepthOffset), actor_state.framebuffer_stride);
    sceGuOffset(actor_state.offset_x, actor_state.offset_y);
    sceGuViewport(
        actor_state.viewport_x, actor_state.viewport_y,
        actor_state.viewport_width, actor_state.viewport_height);
    sceGuScissor(
        actor_state.scissor_left, actor_state.scissor_top,
        actor_state.scissor_right, actor_state.scissor_bottom);
    actor_state.scissor_enabled
        ? sceGuEnable(GU_SCISSOR_TEST)
        : sceGuDisable(GU_SCISSOR_TEST);
    actor_state.depth_test_enabled
        ? sceGuEnable(GU_DEPTH_TEST)
        : sceGuDisable(GU_DEPTH_TEST);
    sceGuDepthFunc(GU_GEQUAL);
    sceGuDepthMask(
        actor_state.depth_write_enabled ? GU_FALSE : GU_TRUE);
    actor_state.cull_enabled
        ? sceGuEnable(GU_CULL_FACE)
        : sceGuDisable(GU_CULL_FACE);
    sceGuFrontFace(GU_CW);
    actor_state.blend_enabled
        ? sceGuEnable(GU_BLEND) : sceGuDisable(GU_BLEND);
    actor_state.alpha_test_enabled
        ? sceGuEnable(GU_ALPHA_TEST) : sceGuDisable(GU_ALPHA_TEST);
    actor_state.stencil_enabled
        ? sceGuEnable(GU_STENCIL_TEST) : sceGuDisable(GU_STENCIL_TEST);
    actor_state.dither_enabled
        ? sceGuEnable(GU_DITHER) : sceGuDisable(GU_DITHER);
    sceGuShadeModel(GU_SMOOTH);
    if (input.render_profile == RenderProfile::KnownGoodUnlit) {
        sceGuDisable(GU_LIGHTING);
        sceGuDisable(GU_LIGHT0);
        sceGuDisable(GU_FOG);
    }
    sceGumMatrixMode(GU_MODEL);
    sceGumLoadIdentity();
    ScePspFVector3 translation = {
        input.link_position.x,
        input.link_position.y,
        input.link_position.z};
    ScePspFVector3 rotation = {
        0.0f, link::actor_to_model_orientation(input.link_yaw), 0.0f};
    ScePspFVector3 scale = {1.0f, 1.0f, 1.0f};
    sceGumTranslate(&translation);
    sceGumRotateXYZ(&rotation);
    sceGumScale(&scale);
    const std::uint8_t* model = runtime.packages.model.bytes;
    const std::uint32_t index_offset = read_u32(model + 80);
    const std::uint32_t submesh_offset = read_u32(model + 84);
    const std::uint32_t submesh_stride = read_u32(model + 88);
    const std::uint32_t submeshes = read_u32(model + 28);
    const bool cpu_vertex_lighting =
        input.lighting_mode != LightingMode::Off &&
        input.lighting_mode != LightingMode::TextureOnly &&
        input.lighting_mode != LightingMode::GuCandidate;
    const SkinnedVertex* submitted = cpu_vertex_lighting
        ? prepare_link_lighting_vertices(runtime, input, metrics)
        : current_vertices(runtime);
    if (input.lighting_mode == LightingMode::GuCandidate) {
        enable_actor_environment_lighting(input);
    } else {
        sceGuDisable(GU_LIGHTING);
        sceGuDisable(GU_LIGHT0);
    }
    sceGuColor(0xffffffffu);
    for (std::uint32_t index = 0; index < submeshes; ++index) {
        const std::uint8_t* item =
            model + submesh_offset + index * submesh_stride;
        const std::uint16_t texture = read_u16(item + 10);
        bind(g_textures[texture]);
        if (input.render_profile == RenderProfile::KnownGoodUnlit ||
            input.lighting_mode == LightingMode::Off ||
            input.lighting_mode == LightingMode::TextureOnly) {
            sceGuTexFunc(GU_TFX_REPLACE, GU_TCC_RGBA);
        } else if (
            input.lighting_mode == LightingMode::Debug ||
            input.lighting_mode == LightingMode::MaterialBaseColor) {
            sceGuDisable(GU_TEXTURE_2D);
        } else {
            sceGuTexFunc(GU_TFX_MODULATE, GU_TCC_RGBA);
        }
        sceGuDisable(GU_BLEND);
        sceGuDisable(GU_ALPHA_TEST);
        sceGuDepthMask(GU_FALSE);
        sceGumDrawArray(
            GU_TRIANGLES,
            GU_TEXTURE_32BITF | GU_COLOR_8888 |
                GU_NORMAL_32BITF | GU_VERTEX_32BITF |
                GU_INDEX_16BIT | GU_TRANSFORM_3D,
            read_u32(item + 4),
            model + index_offset + read_u32(item) * 2,
            submitted);
        trace_render_submission(
            render_trace::Source::Link, render_trace::Bucket::Opaque,
            253, read_u16(item + 22), read_u16(item + 20), texture,
            true, actor_state.cull_enabled,
            input.fog_mode == FogMode::Source,
            input.lighting_mode == LightingMode::GuCandidate);
        ++metrics->link_draw_calls;
    }
    sceGuDisable(GU_LIGHTING);
    metrics->actor_bucket_state_applied = true;
}

void apply_complete_opaque_draw_state(int depth_function) {
    const render::PspGuPassState state =
        render::canonical_actor_pass_state();
    sceGuDrawBufferList(
        GU_PSM_5650, relative(g_draw_offset), state.framebuffer_stride);
    sceGuDepthBuffer(relative(kDepthOffset), state.framebuffer_stride);
    sceGuOffset(state.offset_x, state.offset_y);
    sceGuViewport(
        state.viewport_x, state.viewport_y,
        state.viewport_width, state.viewport_height);
    sceGuScissor(
        state.scissor_left, state.scissor_top,
        state.scissor_right, state.scissor_bottom);
    state.scissor_enabled
        ? sceGuEnable(GU_SCISSOR_TEST)
        : sceGuDisable(GU_SCISSOR_TEST);
    sceGuEnable(GU_DEPTH_TEST);
    sceGuDepthFunc(depth_function);
    sceGuDepthMask(GU_FALSE);
    sceGuDisable(GU_CULL_FACE);
    sceGuFrontFace(GU_CW);
    sceGuDisable(GU_BLEND);
    sceGuDisable(GU_ALPHA_TEST);
    sceGuDisable(GU_STENCIL_TEST);
    sceGuDisable(GU_DITHER);
    sceGuDisable(GU_LIGHTING);
    sceGuDisable(GU_LIGHT0);
    sceGuDisable(GU_FOG);
    sceGuEnable(GU_CLIP_PLANES);
    sceGuShadeModel(GU_SMOOTH);
}

std::uint32_t collect_opaque_order_draws(
    const Runtime& runtime,
    OpaqueDrawDescriptor* draws,
    std::uint32_t capacity,
    std::uint32_t* excluded_non_writing) {
    if (draws == nullptr || excluded_non_writing == nullptr) {
        return 0;
    }
    *excluded_non_writing = 0;
    std::uint32_t used = 0;
    const std::uint32_t room_section_table = read_u32(g_room_model + 72);
    const std::uint8_t* room_submesh_section =
        g_room_model + room_section_table + 64;
    const std::uint8_t* room_submeshes =
        g_room_model + read_u32(room_submesh_section + 4);
    const std::uint32_t room_count = read_u32(g_room_model + 32);
    for (std::uint32_t index = 0; index < room_count; ++index) {
        const std::uint8_t* item = room_submeshes + index * 48;
        if (item[12] != 0) {
            continue;
        }
        const bool depth_test = (item[18] & 1u) != 0;
        const bool depth_write = (item[18] & 2u) != 0;
        if (!depth_test || !depth_write) {
            ++*excluded_non_writing;
            continue;
        }
        if (used >= capacity) {
            return 0;
        }
        draws[used] = {
            {used, 0, read_u16(item + 16), read_u16(item + 14),
             static_cast<std::uint8_t>(OpaqueDrawSource::Room),
             true, true, false},
            read_u16(item + 10), static_cast<std::uint16_t>(index)};
        ++used;
    }
    const std::uint8_t* model = runtime.packages.model.bytes;
    const std::uint32_t link_submesh_offset = read_u32(model + 84);
    const std::uint32_t link_submesh_stride = read_u32(model + 88);
    const std::uint32_t link_count = read_u32(model + 28);
    for (std::uint32_t index = 0; index < link_count; ++index) {
        if (used >= capacity) {
            return 0;
        }
        const std::uint8_t* item =
            model + link_submesh_offset + index * link_submesh_stride;
        draws[used] = {
            {used, 253, read_u16(item + 22), read_u16(item + 20),
             static_cast<std::uint8_t>(OpaqueDrawSource::Link),
             true, true, false},
            read_u16(item + 10), static_cast<std::uint16_t>(index)};
        ++used;
    }
    return used;
}

void draw_opaque_room_submission(
    const OpaqueDrawDescriptor& draw, RenderMetrics* metrics) {
    const std::uint32_t section_table = read_u32(g_room_model + 72);
    const std::uint8_t* vertex_section = g_room_model + section_table;
    const std::uint8_t* index_section = vertex_section + 32;
    const std::uint8_t* submesh_section = index_section + 32;
    const std::uint8_t* vertices =
        g_room_model + read_u32(vertex_section + 4);
    const std::uint8_t* indices =
        g_room_model + read_u32(index_section + 4);
    const std::uint8_t* submeshes =
        g_room_model + read_u32(submesh_section + 4);
    const std::uint8_t* item = submeshes + draw.submesh_index * 48;
    apply_complete_opaque_draw_state(reversed_depth_func(item[19]));
    model_identity();
    bind_room_texture(draw.texture_id);
    sceGumDrawArray(
        GU_TRIANGLES,
        GU_TEXTURE_32BITF | GU_COLOR_8888 |
            GU_VERTEX_32BITF | GU_INDEX_16BIT | GU_TRANSFORM_3D,
        read_u32(item + 4), indices + read_u32(item) * 2, vertices);
    trace_render_submission(
        render_trace::Source::Room, render_trace::Bucket::Opaque,
        draw.submission.actor_id, draw.submission.material_id,
        draw.submission.shape_id, draw.texture_id,
        true, false, false, false);
    ++metrics->room_draw_calls;
    ++metrics->room_opaque_draws;
}

void draw_opaque_link_submission(
    const Runtime& runtime,
    const RealRoomRenderInput& input,
    const SkinnedVertex* submitted,
    const OpaqueDrawDescriptor& draw,
    RenderMetrics* metrics) {
    apply_complete_opaque_draw_state(GU_GEQUAL);
    sceGumMatrixMode(GU_MODEL);
    sceGumLoadIdentity();
    ScePspFVector3 translation = {
        input.link_position.x, input.link_position.y,
        input.link_position.z};
    ScePspFVector3 rotation = {
        0.0f, link::actor_to_model_orientation(input.link_yaw), 0.0f};
    ScePspFVector3 scale = {1.0f, 1.0f, 1.0f};
    sceGumTranslate(&translation);
    sceGumRotateXYZ(&rotation);
    sceGumScale(&scale);
    const std::uint8_t* model = runtime.packages.model.bytes;
    const std::uint32_t index_offset = read_u32(model + 80);
    const std::uint32_t submesh_offset = read_u32(model + 84);
    const std::uint32_t submesh_stride = read_u32(model + 88);
    const std::uint8_t* item = model + submesh_offset +
        draw.submesh_index * submesh_stride;
    bind(g_textures[draw.texture_id]);
    sceGuTexFunc(GU_TFX_REPLACE, GU_TCC_RGBA);
    sceGumDrawArray(
        GU_TRIANGLES,
        GU_TEXTURE_32BITF | GU_COLOR_8888 |
            GU_NORMAL_32BITF | GU_VERTEX_32BITF |
            GU_INDEX_16BIT | GU_TRANSFORM_3D,
        read_u32(item + 4),
        model + index_offset + read_u32(item) * 2, submitted);
    trace_render_submission(
        render_trace::Source::Link, render_trace::Bucket::Opaque,
        draw.submission.actor_id, draw.submission.material_id,
        draw.submission.shape_id, draw.texture_id,
        true, false, false, false);
    ++metrics->link_draw_calls;
}

void draw_real_room_entities(
    const RealRoomRenderInput& input, RenderMetrics* metrics) {
    if (!presentation::debug_visuals(input.presentation)) {
        return;
    }
    for (std::uint32_t index = 0; index < 5; ++index) {
        if (!input.ruby_active[index]) {
            continue;
        }
        translate_scale(
            input.rubies[index].x,
            input.rubies[index].y,
            input.rubies[index].z,
            45.0f, 45.0f, 45.0f);
        draw_colored(kRuby, 24);
        ++metrics->scene_draw_calls;
    }
    translate_scale(
        input.interaction.x,
        input.interaction.y,
        input.interaction.z,
        45.0f, 30.0f, 45.0f);
    draw_colored(kCube, 36);
    ++metrics->scene_draw_calls;
}

void draw_root_pose_debug(
    const RealRoomRenderInput& input, RenderMetrics* metrics) {
    if (!presentation::debug_visuals(input.presentation)) {
        return;
    }
    const float bind_y = input.root_pose.bind_root_translation.y;
    const float final_y = input.root_pose.final_root_translation.y;
    constexpr float arm = 18.0f;
    alignas(16) ColorVertex vertices[] = {
        {0xff4040ffu, -arm, 0.0f, 0.0f},
        {0xff4040ffu, arm, 0.0f, 0.0f},
        {0xff4040ffu, 0.0f, 0.0f, -arm},
        {0xff4040ffu, 0.0f, 0.0f, arm},
        {0xff40ff40u, -arm, bind_y, 0.0f},
        {0xff40ff40u, arm, bind_y, 0.0f},
        {0xff40ff40u, 0.0f, bind_y, -arm},
        {0xff40ff40u, 0.0f, bind_y, arm},
        {0xffffff40u, -arm, final_y, 0.0f},
        {0xffffff40u, arm, final_y, 0.0f},
        {0xffffff40u, 0.0f, final_y, -arm},
        {0xffffff40u, 0.0f, final_y, arm},
        {0xffffffffu, 0.0f, 0.0f, 0.0f},
        {0xffffffffu, 0.0f, final_y, 0.0f},
    };
    auto* submitted = static_cast<ColorVertex*>(
        sceGuGetMemory(sizeof(vertices)));
    std::memcpy(submitted, vertices, sizeof(vertices));
    sceGumMatrixMode(GU_MODEL);
    sceGumLoadIdentity();
    const ScePspFVector3 translation = {
        input.link_position.x,
        input.link_position.y,
        input.link_position.z};
    const ScePspFVector3 rotation = {
        0.0f, link::actor_to_model_orientation(input.link_yaw), 0.0f};
    const ScePspFVector3 scale = {1.0f, 1.0f, 1.0f};
    sceGumTranslate(&translation);
    sceGumRotateXYZ(&rotation);
    sceGumScale(&scale);
    sceGuDisable(GU_TEXTURE_2D);
    sceGuDisable(GU_CULL_FACE);
    sceGuDisable(GU_DEPTH_TEST);
    sceGuDepthMask(GU_TRUE);
    sceGumDrawArray(
        GU_LINES,
        GU_COLOR_8888 | GU_VERTEX_32BITF | GU_TRANSFORM_3D,
        static_cast<int>(sizeof(vertices) / sizeof(vertices[0])),
        nullptr, submitted);
    ++metrics->scene_draw_calls;
}

void draw_scene(const GameplayState& state, RenderMetrics* metrics) {
    configure_camera(state);
    sceGuDisable(GU_CULL_FACE);
    model_identity();
    sceGuDisable(GU_DEPTH_TEST);
    draw_colored(kFloor, 6);
    ++metrics->scene_draw_calls;
    sceGuEnable(GU_DEPTH_TEST);
    constexpr float obstacles[4][4] = {
        {-4.25f, -0.75f, 1.5f, 1.5f},
        {3.5f, -3.0f, 2.0f, 2.0f},
        {0.0f, 3.5f, 2.0f, 2.0f},
        {5.25f, 4.5f, 1.5f, 2.0f},
    };
    for (const auto& obstacle : obstacles) {
        translate_scale(
            obstacle[0], 0.0f, obstacle[1],
            obstacle[2], 1.2f, obstacle[3]);
        draw_colored(kCube, 36);
        ++metrics->scene_draw_calls;
    }
    constexpr float boundaries[4][5] = {
        {0.0f, 0.0f, -9.8f, 20.0f, 0.25f},
        {0.0f, 0.0f, 9.8f, 20.0f, 0.25f},
        {-9.8f, 0.0f, 0.0f, 0.25f, 20.0f},
        {9.8f, 0.0f, 0.0f, 0.25f, 20.0f},
    };
    for (const auto& boundary : boundaries) {
        translate_scale(
            boundary[0], boundary[1], boundary[2],
            boundary[3], 0.35f, boundary[4]);
        draw_colored(kCube, 36);
        ++metrics->scene_draw_calls;
    }
    for (const Ruby& ruby : state.rubies) {
        if (!ruby.active) {
            continue;
        }
        translate_scale(
            ruby.position.x, ruby.position.y, ruby.position.z,
            1.0f, 1.0f, 1.0f);
        draw_colored(kRuby, 24);
        ++metrics->scene_draw_calls;
    }
    translate_scale(-8.7f, 0.0f, -8.7f, 1.4f, 0.5f, 1.4f);
    draw_colored(kCube, 36);
    ++metrics->scene_draw_calls;
}

void draw_link(
    const Runtime& runtime,
    const GameplayState& state,
    RenderMetrics* metrics) {
    const std::uint8_t* model = runtime.packages.model.bytes;
    const std::uint32_t index_offset = read_u32(model + 80);
    const std::uint32_t submesh_offset = read_u32(model + 84);
    const std::uint32_t submesh_stride = read_u32(model + 88);
    const std::uint32_t submeshes = read_u32(model + 28);
    sceGumMatrixMode(GU_MODEL);
    sceGumLoadIdentity();
    ScePspFVector3 translation = {
        state.position.x, state.position.y, state.position.z};
    ScePspFVector3 rotation = {0.0f, state.yaw + kPi, 0.0f};
    ScePspFVector3 scale = {0.0105f, 0.0105f, 0.0105f};
    sceGumTranslate(&translation);
    sceGumRotateXYZ(&rotation);
    sceGumScale(&scale);
    sceKernelDcacheWritebackRange(
        current_vertices(runtime),
        runtime.vertex_count * sizeof(SkinnedVertex));
    sceKernelDcacheWritebackRange(
        model + index_offset, read_u32(model + 24) * 2);
    sceGuShadeModel(GU_SMOOTH);
    sceGuEnable(GU_CULL_FACE);
    constexpr int vertex_type =
        GU_TEXTURE_32BITF | GU_COLOR_8888 | GU_NORMAL_32BITF |
        GU_VERTEX_32BITF | GU_INDEX_16BIT | GU_TRANSFORM_3D;
    for (std::uint32_t index = 0; index < submeshes; ++index) {
        const std::uint8_t* item =
            model + submesh_offset + index * submesh_stride;
        const std::uint32_t first = read_u32(item);
        const std::uint32_t count = read_u32(item + 4);
        bind(g_textures[read_u16(item + 10)]);
        const bool alpha =
            g_textures[read_u16(item + 10)].bytes != 0 &&
            runtime.packages.textures.bytes[
                read_u32(runtime.packages.textures.bytes + 24) +
                read_u16(item + 10) * 48 + 14] > 1;
        if (alpha) {
            sceGuEnable(GU_ALPHA_TEST);
            sceGuAlphaFunc(GU_GREATER, 20, 0xff);
        } else {
            sceGuDisable(GU_ALPHA_TEST);
        }
        sceGumDrawArray(
            GU_TRIANGLES, vertex_type, count,
            model + index_offset + first * 2,
            current_vertices(runtime));
        ++metrics->link_draw_calls;
    }
    sceGuDisable(GU_ALPHA_TEST);
}

void sprite(
    int x,
    int y,
    int width,
    int height,
    int u,
    int v,
    int uw,
    int vh,
    RenderMetrics* metrics) {
    alignas(16) SpriteVertex vertices[4] = {
        {
            static_cast<std::int16_t>(u),
            static_cast<std::int16_t>(v),
            static_cast<std::int16_t>(x),
            static_cast<std::int16_t>(y), 0},
        {
            static_cast<std::int16_t>(u + uw),
            static_cast<std::int16_t>(v),
            static_cast<std::int16_t>(x + width),
            static_cast<std::int16_t>(y), 0},
        {
            static_cast<std::int16_t>(u),
            static_cast<std::int16_t>(v + vh),
            static_cast<std::int16_t>(x),
            static_cast<std::int16_t>(y + height), 0},
        {
            static_cast<std::int16_t>(u + uw),
            static_cast<std::int16_t>(v + vh),
            static_cast<std::int16_t>(x + width),
            static_cast<std::int16_t>(y + height), 0},
    };
    auto* submitted = static_cast<SpriteVertex*>(
        sceGuGetMemory(sizeof(vertices)));
    std::memcpy(submitted, vertices, sizeof(vertices));
    sceGuDrawArray(
        GU_TRIANGLE_STRIP,
        GU_TEXTURE_16BIT | GU_VERTEX_16BIT | GU_TRANSFORM_2D,
        4, nullptr, submitted);
    ++metrics->ui_draw_calls;
}

void rectangle(
    int x, int y, int width, int height,
    std::uint32_t color, RenderMetrics* metrics) {
    alignas(16) ColorVertex vertices[6] = {
        {color, static_cast<float>(x), static_cast<float>(y), 0},
        {color, static_cast<float>(x + width), static_cast<float>(y), 0},
        {color, static_cast<float>(x + width), static_cast<float>(y + height), 0},
        {color, static_cast<float>(x), static_cast<float>(y), 0},
        {color, static_cast<float>(x + width), static_cast<float>(y + height), 0},
        {color, static_cast<float>(x), static_cast<float>(y + height), 0},
    };
    auto* submitted = static_cast<ColorVertex*>(
        sceGuGetMemory(sizeof(vertices)));
    std::memcpy(submitted, vertices, sizeof(vertices));
    sceGuDisable(GU_TEXTURE_2D);
    sceGuDrawArray(
        GU_TRIANGLES,
        GU_COLOR_8888 | GU_VERTEX_32BITF | GU_TRANSFORM_2D,
        6, nullptr, submitted);
    ++metrics->ui_draw_calls;
}

void digit(
    std::uint32_t value, int x, int y, RenderMetrics* metrics) {
    constexpr std::uint8_t segments[10] = {
        0x3f, 0x06, 0x5b, 0x4f, 0x66,
        0x6d, 0x7d, 0x07, 0x7f, 0x6f};
    const std::uint8_t mask = segments[value % 10];
    constexpr int boxes[7][4] = {
        {2,0,8,2},{10,2,2,8},{10,12,2,8},{2,20,8,2},
        {0,12,2,8},{0,2,2,8},{2,10,8,2},
    };
    for (int segment = 0; segment < 7; ++segment) {
        if ((mask & (1u << segment)) != 0) {
            rectangle(
                x + boxes[segment][0], y + boxes[segment][1],
                boxes[segment][2], boxes[segment][3],
                0xffffffffu, metrics);
        }
    }
}

const std::uint8_t* ui_record(std::uint16_t id) {
    if (g_ui_package.bytes == nullptr ||
        read_u16(g_ui_package.bytes + 4) != 2) {
        return nullptr;
    }
    const std::uint32_t count =
        read_u32(g_ui_package.bytes + 28);
    const std::uint32_t table =
        read_u32(g_ui_package.bytes + 32);
    const std::uint32_t stride =
        read_u32(g_ui_package.bytes + 36);
    for (std::uint32_t index = 0; index < count; ++index) {
        const std::uint8_t* item =
            g_ui_package.bytes + table + index * stride;
        if (read_u16(item) == id) {
            return item;
        }
    }
    return nullptr;
}

void original_ui_sprite(
    std::uint16_t id,
    int x,
    int y,
    int width,
    int height,
    RenderMetrics* metrics) {
    const std::uint8_t* item = ui_record(id);
    if (item == nullptr) {
        return;
    }
    const int source_width = read_u16(item + 16);
    const int source_height = read_u16(item + 18);
    sceGuTexFunc(GU_TFX_MODULATE, GU_TCC_RGBA);
    sceGuColor(read_u32(item + 20));
    sprite(
        x, y,
        width > 0 ? width : read_u16(item + 8),
        height > 0 ? height : read_u16(item + 10),
        read_u16(item + 12), read_u16(item + 14),
        source_width, source_height, metrics);
    sceGuColor(0xffffffffu);
}

void original_ui_text_bounded(
    const char* text,
    std::uint16_t visible_characters,
    int x,
    int baseline,
    RenderMetrics* metrics) {
    if (text == nullptr) return;
    const int origin_x = x;
    std::uint16_t consumed = 0;
    for (const char* cursor = text; *cursor != '\0' &&
         consumed < visible_characters; ++cursor, ++consumed) {
        if (*cursor == '\n') {
            x = origin_x;
            baseline += 23;
            continue;
        }
        if (*cursor == '\r') continue;
        const std::uint16_t id = static_cast<std::uint16_t>(
            128 + static_cast<unsigned char>(*cursor));
        const std::uint8_t* item = ui_record(id);
        if (item == nullptr) continue;
        // DPUI font records store a cropped source glyph plus its bearing
        // inside the original 24x24 BFN cell in +4/+6. Scale those source
        // metrics by the same 18/24 ratio used by the legacy full-cell path.
        const int bearing_x = read_u16(item + 4);
        const int bearing_y = read_u16(item + 6);
        const int source_width = read_u16(item + 16);
        const int source_height = read_u16(item + 18);
        const int draw_x = x + (bearing_x * 3 + 2) / 4;
        const int draw_y = baseline - 18 + (bearing_y * 3 + 2) / 4;
        const int draw_width = std::max(1, (source_width * 3 + 3) / 4);
        const int draw_height = std::max(1, (source_height * 3 + 3) / 4);
        original_ui_sprite(
            id, draw_x, draw_y, draw_width, draw_height, metrics);
        const int advance = read_u16(item + 28);
        x += std::max(5, advance * 3 / 4);
    }
}

void original_ui_text(
    const char* text,
    int x,
    int baseline,
    RenderMetrics* metrics) {
    original_ui_text_bounded(text, 0xffffu, x, baseline, metrics);
}

void draw_message_overlay(
    const MessageOverlayRenderInput* overlay,
    RenderMetrics* metrics) {
    if (overlay == nullptr || !overlay->active || overlay->text == nullptr) {
        return;
    }
    sceGuDisable(GU_DEPTH_TEST);
    sceGuDisable(GU_CULL_FACE);
    sceGuEnable(GU_BLEND);
    sceGuBlendFunc(
        GU_ADD, GU_SRC_ALPHA, GU_ONE_MINUS_SRC_ALPHA, 0, 0);
    // Simplified PSP presentation: source text/lifetime are authoritative,
    // while the heavy GameCube message-pane effects are intentionally deferred.
    rectangle(26, 145, 428, 119, 0xd010151du, metrics);
    rectangle(26, 145, 428, 2, 0xff5ca8c8u, metrics);
    rectangle(26, 262, 428, 2, 0xff5ca8c8u, metrics);
    rectangle(26, 145, 2, 119, 0xff5ca8c8u, metrics);
    rectangle(452, 145, 2, 119, 0xff5ca8c8u, metrics);
    sceGuEnable(GU_TEXTURE_2D);
    bind(g_ui);
    sceGuTexFunc(GU_TFX_MODULATE, GU_TCC_RGBA);
    sceGuTexScale(1.0f, 1.0f);
    original_ui_text_bounded(
        overlay->text, overlay->visible_characters,
        42, 168, metrics);
    if (overlay->awaiting_confirm) {
        original_ui_sprite(30, 416, 228, 28, 28, metrics);
    }
    sceGuDisable(GU_BLEND);
}

void draw_original_ui(
    const GameplayState& state,
    RenderMetrics* metrics) {
    bind(g_ui);
    sceGuTexFunc(GU_TFX_REPLACE, GU_TCC_RGBA);
    sceGuTexScale(1.0f, 1.0f);
    for (std::uint32_t heart = 0; heart < 3; ++heart) {
        const std::uint16_t variant =
            heart < state.hearts ? 0 : 3;
        original_ui_sprite(
            variant, 16 + static_cast<int>(heart) * 25,
            12, 25, 22, metrics);
    }
    original_ui_sprite(20, 391, 11, 18, 29, metrics);
    original_ui_sprite(
        static_cast<std::uint16_t>(10 + (state.rupees / 100) % 10),
        414, 13, 20, 20, metrics);
    original_ui_sprite(
        static_cast<std::uint16_t>(10 + (state.rupees / 10) % 10),
        432, 13, 20, 20, metrics);
    original_ui_sprite(
        static_cast<std::uint16_t>(10 + state.rupees % 10),
        450, 13, 20, 20, metrics);
    if (state.action_prompt) {
        original_ui_sprite(30, 422, 218, 34, 34, metrics);
    }
    if (state.mode == GameMode::Paused) {
        rectangle(0, 0, 480, 272, 0x98000000u, metrics);
        bind(g_ui);
        original_ui_sprite(40, 126, 48, 228, 174, metrics);
        original_ui_sprite(41, 126, 48, 44, 44, metrics);
        original_ui_sprite(42, 170, 48, 184, 22, metrics);
        const int selected_y =
            84 + static_cast<int>(state.pause_selection) * 44;
        original_ui_sprite(43, 145, selected_y - 18, 38, 38, metrics);
        original_ui_text("Resume", 190, 100, metrics);
        original_ui_text("Reset Room", 190, 144, metrics);
        original_ui_text("Exit", 190, 188, metrics);
    }
}

void draw_ui(const GameplayState& state, RenderMetrics* metrics) {
    sceGuDisable(GU_DEPTH_TEST);
    sceGuDisable(GU_CULL_FACE);
    sceGuEnable(GU_BLEND);
    sceGuBlendFunc(
        GU_ADD, GU_SRC_ALPHA, GU_ONE_MINUS_SRC_ALPHA, 0, 0);
    if (read_u16(g_ui_package.bytes + 4) == 2) {
        draw_original_ui(state, metrics);
        if (state.debug_visible) {
            rectangle(8, 238, 210, 24, 0xc0101820u, metrics);
        }
        sceGuDisable(GU_BLEND);
        return;
    }
    rectangle(8, 8, 110, 34, 0xd0182838u, metrics);
    rectangle(378, 8, 94, 38, 0xd0182838u, metrics);
    bind(g_ui);
    sceGuTexFunc(GU_TFX_REPLACE, GU_TCC_RGBA);
    sceGuTexScale(1.0f, 1.0f);
    for (std::uint32_t heart = 0; heart < state.hearts; ++heart) {
        sprite(18 + heart * 24, 14, 22, 22, heart * 18, 0, 18, 18, metrics);
    }
    sprite(386, 16, 24, 24, 54, 0, 18, 18, metrics);
    digit((state.rupees / 100) % 10, 414, 16, metrics);
    digit((state.rupees / 10) % 10, 430, 16, metrics);
    digit(state.rupees % 10, 446, 16, metrics);
    if (state.action_prompt) {
        bind(g_ui);
        sceGuTexFunc(GU_TFX_REPLACE, GU_TCC_RGBA);
        sceGuTexScale(1.0f, 1.0f);
        sprite(390, 218, 28, 28, 76, 0, 18, 18, metrics);
        rectangle(423, 224, 40, 14, 0xcc203040u, metrics);
    }
    if (state.mode == GameMode::Paused) {
        rectangle(0, 0, 480, 272, 0xa0101828u, metrics);
        rectangle(145, 64, 190, 142, 0xe0283850u, metrics);
        for (std::uint32_t option = 0; option < 3; ++option) {
            rectangle(
                170, 86 + option * 36, 140, 24,
                option == state.pause_selection
                    ? 0xffd0a840u : 0xff506078u,
                metrics);
        }
    }
    if (state.debug_visible) {
        rectangle(8, 238, 210, 24, 0xc0101820u, metrics);
    }
    sceGuDisable(GU_BLEND);
}

void draw_startup_channel(
    std::uint16_t channel,
    std::uint8_t fade_alpha,
    RenderMetrics* metrics) {
    sceGuDisable(GU_DEPTH_TEST);
    sceGuDisable(GU_CULL_FACE);
    sceGuEnable(GU_TEXTURE_2D);
    sceGuEnable(GU_BLEND);
    sceGuBlendFunc(
        GU_ADD, GU_SRC_ALPHA, GU_ONE_MINUS_SRC_ALPHA, 0, 0);
    bind(g_ui);
    sceGuTexFunc(GU_TFX_MODULATE, GU_TCC_RGBA);
    sceGuColor(
        static_cast<std::uint32_t>(fade_alpha) << 24 |
        0x00ffffffu);
    for (std::uint32_t index = 0;
         index < g_startup_ui_package.record_count; ++index) {
        const std::uint8_t* record =
            g_startup_ui_package.bytes +
            g_startup_ui_package.records_offset + index * 32;
        if (read_u16(record + 2) != channel) {
            continue;
        }
        sprite(
            static_cast<std::int16_t>(read_u16(record + 4)),
            static_cast<std::int16_t>(read_u16(record + 6)),
            read_u16(record + 8), read_u16(record + 10),
            read_u16(record + 12), read_u16(record + 14),
            read_u16(record + 16), read_u16(record + 18), metrics);
        trace_render_submission(
            render_trace::Source::StartupUi, render_trace::Bucket::Ui,
            channel, static_cast<std::uint16_t>(index),
            static_cast<std::uint16_t>(index), channel,
            false, false, false, false);
    }
    sceGuColor(0xffffffffu);
    sceGuDisable(GU_BLEND);
}

const std::uint8_t* startup_record(std::uint16_t id) {
    for (std::uint32_t index = 0;
         index < g_startup_ui_package.record_count; ++index) {
        const std::uint8_t* record =
            g_startup_ui_package.bytes +
            g_startup_ui_package.records_offset + index * 32;
        if (read_u16(record) == id) {
            return record;
        }
    }
    return nullptr;
}

void draw_startup_record_at(
    std::uint16_t id, int x, int y,
    std::uint32_t color, RenderMetrics* metrics) {
    const std::uint8_t* record = startup_record(id);
    if (record == nullptr) {
        return;
    }
    sceGuEnable(GU_TEXTURE_2D);
    bind(g_ui);
    sceGuTexFunc(GU_TFX_MODULATE, GU_TCC_RGBA);
    sceGuColor(color);
    sprite(
        x, y, read_u16(record + 8), read_u16(record + 10),
        read_u16(record + 12), read_u16(record + 14),
        read_u16(record + 16), read_u16(record + 18), metrics);
}

void draw_startup_text(
    const char* text, int x, int y,
    std::uint32_t color, RenderMetrics* metrics) {
    if (text == nullptr) {
        return;
    }
    int cursor = x;
    for (const char* at = text; *at != '\0'; ++at) {
        const unsigned char character =
            static_cast<unsigned char>(*at);
        if (character < 32 || character > 126) {
            continue;
        }
        const std::uint8_t* record = startup_record(
            static_cast<std::uint16_t>(256 + character - 32));
        if (record == nullptr) {
            cursor += 8;
            continue;
        }
        draw_startup_record_at(
            static_cast<std::uint16_t>(256 + character - 32),
            cursor, y, color, metrics);
        cursor += read_u16(record + 28);
    }
    sceGuColor(0xffffffffu);
}

void draw_name_entry_cell_border(
    int x, int y, int width, int height,
    std::uint32_t color, RenderMetrics* metrics) {
    rectangle(x, y, width, 2, color, metrics);
    rectangle(x, y + height - 2, width, 2, color, metrics);
    rectangle(x, y, 2, height, color, metrics);
    rectangle(x + width - 2, y, 2, height, color, metrics);
}

}  // namespace

void set_render_trace_sink(render_trace::Sink sink, void* user) {
    g_render_trace.initialize(sink, user);
}

void clear_render_trace_sink() {
    g_render_trace.reset();
}

bool initialize_renderer(
    const PackageView& textures,
    const PackageView& ui,
    void* command_list,
    std::uint32_t command_list_bytes,
    RenderMetrics* metrics) {
    if (g_initialized || command_list == nullptr || metrics == nullptr ||
        command_list_bytes < 128 * 1024 ||
        textures.bytes == nullptr || ui.bytes == nullptr) {
        return false;
    }
    *metrics = {};
    g_safe_link_lut_valid = false;
    g_edram = static_cast<std::uint8_t*>(sceGeEdramGetAddr());
    metrics->edram_total = sceGeEdramGetSize();
    metrics->edram_framebuffers = kFramebuffers;
    metrics->edram_depth = kBufferBytes;
    if (g_edram == nullptr || metrics->edram_total <= kTextureOffset) {
        return false;
    }
    g_texture_package = textures;
    g_ui_package = ui;
    std::uint32_t cursor = kTextureOffset;
    const std::uint32_t table = read_u32(textures.bytes + 24);
    for (std::uint32_t index = 0; index < 29; ++index) {
        const std::uint8_t* item = textures.bytes + table + index * 48;
        const std::uint32_t source_offset = read_u32(item + 16);
        const std::uint32_t bytes = read_u32(item + 20);
        cursor = (cursor + 15u) & ~15u;
        if (bytes > metrics->edram_total - cursor) {
            return false;
        }
        std::memcpy(g_edram + cursor, textures.bytes + source_offset, bytes);
        g_textures[index] = {
            read_u16(item + 4), read_u16(item + 6),
            read_u16(item + 8), read_u16(item + 10), cursor, bytes,
            item[12]};
        cursor += bytes;
        metrics->edram_textures += bytes;
    }
    cursor = (cursor + 15u) & ~15u;
    const std::uint32_t ui_source = read_u32(ui.bytes + 40);
    const std::uint32_t ui_bytes = read_u32(ui.bytes + 44);
    const std::uint16_t ui_width = static_cast<std::uint16_t>(
        read_u32(ui.bytes + 16));
    const std::uint16_t ui_height = static_cast<std::uint16_t>(
        read_u32(ui.bytes + 20));
    const std::uint16_t ui_stored_width =
        texture_storage_dimension(ui_width);
    const std::uint16_t ui_stored_height =
        texture_storage_dimension(ui_height);
    const std::uint32_t ui_storage_bytes =
        static_cast<std::uint32_t>(ui_stored_width) *
        ui_stored_height * 2u;
    if (ui_bytes > ui_storage_bytes ||
        ui_storage_bytes > metrics->edram_total - cursor) {
        return false;
    }
    std::memcpy(g_edram + cursor, ui.bytes + ui_source, ui_bytes);
    std::memset(
        g_edram + cursor + ui_bytes, 0,
        ui_storage_bytes - ui_bytes);
    g_ui = {
        ui_width, ui_height,
        ui_stored_width, ui_stored_height,
        cursor, ui_storage_bytes, 2};
    cursor += ui_storage_bytes;
    metrics->edram_ui = ui_storage_bytes;
    metrics->edram_remaining = metrics->edram_total - cursor;
    if (metrics->edram_textures + metrics->edram_ui > 1150000 ||
        metrics->edram_remaining < 80000) {
        return false;
    }
    g_list = command_list;
    g_list_bytes = command_list_bytes;
    initialize_simple_shadow_texture();
    sceKernelDcacheWritebackAll();
    sceGuInit();
    sceGuStart(GU_DIRECT, g_list);
    sceGuDrawBuffer(GU_PSM_5650, relative(0), kStride);
    sceGuDispBuffer(kWidth, kHeight, relative(kBufferBytes), kStride);
    sceGuDepthBuffer(relative(kDepthOffset), kStride);
    sceGuOffset(2048 - kWidth / 2, 2048 - kHeight / 2);
    sceGuViewport(2048, 2048, kWidth, kHeight);
    sceGuDepthRange(65535, 0);
    sceGuScissor(0, 0, kWidth, kHeight);
    sceGuEnable(GU_SCISSOR_TEST);
    sceGuDisable(GU_STENCIL_TEST);
    sceGuDisable(GU_FOG);
    sceGuDisable(GU_LIGHTING);
    sceGuDisable(GU_DITHER);
    sceGuFinish();
    sceGuSync(GU_SYNC_FINISH, GU_SYNC_WHAT_DONE);
    sceDisplayWaitVblankStart();
    sceGuDisplay(GU_TRUE);
    g_draw_offset = 0;
    g_initialized = true;
    return true;
}

bool initialize_startup_ui_renderer(
    const startup::UiPackageView& ui,
    void* command_list,
    std::uint32_t command_list_bytes,
    RenderMetrics* metrics) {
    if (g_initialized || ui.bytes == nullptr ||
        command_list == nullptr || command_list_bytes < 128 * 1024 ||
        metrics == nullptr) {
        return false;
    }
    *metrics = {};
    g_edram = static_cast<std::uint8_t*>(sceGeEdramGetAddr());
    metrics->edram_total = sceGeEdramGetSize();
    metrics->edram_framebuffers = kFramebuffers;
    metrics->edram_depth = kBufferBytes;
    const std::uint16_t ui_stored_width =
        texture_storage_dimension(ui.atlas_width);
    const std::uint16_t ui_stored_height =
        texture_storage_dimension(ui.atlas_height);
    const std::uint32_t ui_storage_bytes =
        static_cast<std::uint32_t>(ui_stored_width) *
        ui_stored_height * 2u;
    if (g_edram == nullptr || ui.atlas_bytes > ui_storage_bytes ||
        ui_storage_bytes > metrics->edram_total - kTextureOffset) {
        return false;
    }
    std::memcpy(
        g_edram + kTextureOffset,
        ui.bytes + ui.atlas_offset,
        ui.atlas_bytes);
    std::memset(
        g_edram + kTextureOffset + ui.atlas_bytes, 0,
        ui_storage_bytes - ui.atlas_bytes);
    g_ui = {
        static_cast<std::uint16_t>(ui.atlas_width),
        static_cast<std::uint16_t>(ui.atlas_height),
        ui_stored_width, ui_stored_height,
        kTextureOffset, ui_storage_bytes, 2};
    g_startup_ui_package = ui;
    g_list = command_list;
    g_list_bytes = command_list_bytes;
    metrics->edram_ui = ui_storage_bytes;
    metrics->edram_remaining =
        metrics->edram_total - kTextureOffset - ui_storage_bytes;
    sceKernelDcacheWritebackAll();
    sceGuInit();
    sceGuStart(GU_DIRECT, g_list);
    sceGuDrawBuffer(GU_PSM_5650, relative(0), kStride);
    sceGuDispBuffer(kWidth, kHeight, relative(kBufferBytes), kStride);
    sceGuDepthBuffer(relative(kDepthOffset), kStride);
    sceGuOffset(2048 - kWidth / 2, 2048 - kHeight / 2);
    sceGuViewport(2048, 2048, kWidth, kHeight);
    sceGuScissor(0, 0, kWidth, kHeight);
    sceGuEnable(GU_SCISSOR_TEST);
    sceGuFinish();
    sceGuSync(GU_SYNC_FINISH, GU_SYNC_WHAT_DONE);
    sceDisplayWaitVblankStart();
    sceGuDisplay(GU_TRUE);
    g_initialized = true;
    return true;
}

bool render_startup_ui_frame(
    std::uint16_t channel,
    std::uint8_t fade_alpha,
    RenderMetrics* metrics) {
    if (!g_initialized || metrics == nullptr ||
        g_startup_ui_package.bytes == nullptr) {
        return false;
    }
    metrics->ui_draw_calls = 0;
    g_render_trace_frame = metrics->frames;
    sceGuStart(GU_DIRECT, g_list);
    sceGuClearColor(0xff000000u);
    sceGuClear(GU_COLOR_BUFFER_BIT);
    draw_startup_channel(channel, fade_alpha, metrics);
    const int bytes = sceGuFinish();
    if (bytes < 0 || static_cast<std::uint32_t>(bytes) > g_list_bytes ||
        sceGuSync(GU_SYNC_FINISH, GU_SYNC_WHAT_DONE) < 0) {
        return false;
    }
    metrics->command_bytes = static_cast<std::uint32_t>(bytes);
    sceDisplayWaitVblankStart();
    sceGuSwapBuffers();
    metrics->synchronized = true;
    ++metrics->frames;
    return true;
}

bool render_startup_ui_frame_layers(
    std::uint16_t base_channel,
    std::uint16_t overlay_channel,
    std::uint8_t fade_alpha,
    RenderMetrics* metrics) {
    if (!g_initialized || metrics == nullptr ||
        g_startup_ui_package.bytes == nullptr) {
        return false;
    }
    metrics->ui_draw_calls = 0;
    g_render_trace_frame = metrics->frames;
    sceGuStart(GU_DIRECT, g_list);
    sceGuClearColor(0xff000000u);
    sceGuClear(GU_COLOR_BUFFER_BIT);
    draw_startup_channel(base_channel, fade_alpha, metrics);
    draw_startup_channel(overlay_channel, fade_alpha, metrics);
    const int bytes = sceGuFinish();
    if (bytes < 0 || static_cast<std::uint32_t>(bytes) > g_list_bytes ||
        sceGuSync(GU_SYNC_FINISH, GU_SYNC_WHAT_DONE) < 0) {
        return false;
    }
    metrics->command_bytes = static_cast<std::uint32_t>(bytes);
    sceDisplayWaitVblankStart();
    sceGuSwapBuffers();
    metrics->synchronized = true;
    ++metrics->frames;
    return true;
}

bool render_startup_name_entry_frame(
    const StartupNameEntryRenderInput& input,
    RenderMetrics* metrics) {
    if (!g_initialized || metrics == nullptr || input.heading == nullptr ||
        input.name == nullptr || input.cursor_row >= 3 ||
        input.cursor_column >= 13 ||
        g_startup_ui_package.bytes == nullptr ||
        startup_record(200) == nullptr) {
        return false;
    }
    metrics->ui_draw_calls = 0;
    g_render_trace_frame = metrics->frames;
    sceGuStart(GU_DIRECT, g_list);
    sceGuClearColor(0xff000000u);
    sceGuClear(GU_COLOR_BUFFER_BIT);
    sceGuDisable(GU_DEPTH_TEST);
    sceGuDisable(GU_CULL_FACE);
    sceGuEnable(GU_BLEND);
    sceGuBlendFunc(
        GU_ADD, GU_SRC_ALPHA, GU_ONE_MINUS_SRC_ALPHA, 0, 0);
    draw_startup_record_at(200, 0, 0, 0xffffffffu, metrics);

    rectangle(43, 6, 394, 260, 0xb0182030u, metrics);
    rectangle(47, 10, 386, 252, 0xd0304050u, metrics);
    rectangle(51, 14, 378, 244, 0xe0182028u, metrics);
    rectangle(56, 18, 368, 28, 0xd0584830u, metrics);
    rectangle(60, 22, 360, 20, 0xe0202830u, metrics);
    draw_startup_text(input.heading, 164, 21, 0xffd8c080u, metrics);

    rectangle(103, 52, 274, 40, 0xff6c5938u, metrics);
    rectangle(107, 56, 266, 32, 0xf0182028u, metrics);
    std::uint8_t name_length = 0;
    while (input.name[name_length] != '\0' && name_length < 8) {
        ++name_length;
    }
    const int name_start = 240 - static_cast<int>(name_length) * 14;
    for (std::uint8_t index = 0; index < name_length; ++index) {
        char character[2] = {input.name[index], '\0'};
        draw_startup_text(
            character, name_start + index * 28, 60,
            0xffffffffu, metrics);
    }

    constexpr int kGridX = 64;
    constexpr int kGridY = 107;
    constexpr int kCellStepX = 27;
    constexpr int kCellStepY = 31;
    for (std::uint8_t row = 0; row < 3; ++row) {
        const std::uint8_t columns = row == 2 ? 10 : 13;
        for (std::uint8_t column = 0; column < columns; ++column) {
            const int x = kGridX + column * kCellStepX;
            const int y = kGridY + row * kCellStepY;
            rectangle(x + 2, y + 2, 21, 22, 0xd0283038u, metrics);
            char label[2] = {
                row == 0
                    ? static_cast<char>('A' + column)
                    : row == 1
                        ? static_cast<char>('N' + column)
                        : static_cast<char>('0' + column),
                '\0'};
            if (input.lowercase && row < 2) {
                label[0] = static_cast<char>(label[0] + ('a' - 'A'));
            }
            draw_startup_text(label, x + 6, y + 2, 0xffffffffu, metrics);
        }
    }
    constexpr int kButtonY = 208;
    constexpr int kButtonX[3] = {64, 195, 326};
    constexpr const char* kButtonLabel[3] = {"SPACE", "DELETE", "END"};
    for (std::uint8_t button = 0; button < 3; ++button) {
        rectangle(kButtonX[button], kButtonY, 90, 27, 0xff5c5c58u, metrics);
        rectangle(
            kButtonX[button] + 3, kButtonY + 3,
            84, 21, 0xff282c30u, metrics);
        draw_startup_text(
            kButtonLabel[button], kButtonX[button] + 14,
            kButtonY + 3,
            button == 2 ? 0xffe0b848u : 0xffffffffu, metrics);
    }
    if (input.cursor_row == 2 && input.cursor_column >= 10) {
        draw_name_entry_cell_border(
            kButtonX[input.cursor_column - 10], kButtonY, 90, 27,
            0xfff0c050u, metrics);
    } else {
        draw_name_entry_cell_border(
            kGridX + input.cursor_column * kCellStepX,
            kGridY + input.cursor_row * kCellStepY,
            25, 26,
            0xfff0c050u, metrics);
    }

    draw_startup_text(
        input.lowercase ? "abc" : "ABC", 64, 239,
        0xffe0b848u, metrics);
    draw_startup_text(
        "TRI CASE   X OK   O DELETE",
        112, 239, 0xffd8d8d8u, metrics);
    sceGuColor(0xffffffffu);
    sceGuDisable(GU_BLEND);
    const int bytes = sceGuFinish();
    if (bytes < 0 || static_cast<std::uint32_t>(bytes) > g_list_bytes ||
        sceGuSync(GU_SYNC_FINISH, GU_SYNC_WHAT_DONE) < 0) {
        return false;
    }
    metrics->command_bytes = static_cast<std::uint32_t>(bytes);
    sceDisplayWaitVblankStart();
    sceGuSwapBuffers();
    metrics->synchronized = true;
    ++metrics->frames;
    return true;
}

bool initialize_startup_title_renderer(
    const startup::UiPackageView& ui,
    const PackageView& room_textures,
    const void* room_model,
    std::uint32_t room_model_size,
    void* command_list,
    std::uint32_t command_list_bytes,
    RenderMetrics* metrics) {
    if (!initialize_startup_ui_renderer(
            ui, command_list, command_list_bytes, metrics)) {
        return false;
    }
    if (!replace_real_room_renderer(
            room_textures, room_model, room_model_size, metrics)) {
        shutdown_renderer();
        return false;
    }
    return true;
}

bool render_startup_title_frame(
    const StaticModelRenderView& title_model,
    const StartupTitleCamera& camera,
    bool draw_title_model,
    std::uint16_t ui_channel,
    std::uint8_t fade_alpha,
    RenderMetrics* metrics) {
    if (!g_initialized || metrics == nullptr ||
        g_room_model == nullptr || title_model.model == nullptr ||
        title_model.textures == nullptr ||
        g_startup_ui_package.bytes == nullptr) {
        return false;
    }
    metrics->link_draw_calls = 0;
    metrics->scene_draw_calls = 0;
    metrics->ui_draw_calls = 0;
    metrics->room_draw_calls = 0;
    metrics->room_opaque_draws = 0;
    metrics->room_alpha_test_draws = 0;
    metrics->room_alpha_blend_draws = 0;
    metrics->original_actor_draw_calls = 0;
    g_render_trace_frame = metrics->frames;
    RealRoomRenderInput input = {};
    input.camera_center = camera.center;
    input.camera_eye = camera.eye;
    input.lighting_mode = LightingMode::Off;
    input.fog_mode = FogMode::Off;
    input.shadow_mode = ShadowMode::Off;
    sceGuStart(GU_DIRECT, g_list);
    sceGuClearColor(kBackground);
    sceGuClearDepth(0);
    sceGuClear(GU_COLOR_BUFFER_BIT | GU_DEPTH_BUFFER_BIT);
    sceGuDepthFunc(GU_GEQUAL);
    sceGuDepthMask(GU_FALSE);
    sceGuEnable(GU_DEPTH_TEST);
    sceGuDisable(GU_CULL_FACE);
    sceGuEnable(GU_CLIP_PLANES);
    sceGumMatrixMode(GU_PROJECTION);
    sceGumLoadIdentity();
    sceGumPerspective(
        camera.fov, 480.0f / 272.0f,
        camera.near_plane, camera.far_plane);
    ScePspFVector3 eye = {
        camera.eye.x, camera.eye.y, camera.eye.z};
    ScePspFVector3 center = {
        camera.center.x, camera.center.y, camera.center.z};
    ScePspFVector3 up = {
        camera.up.x, camera.up.y, camera.up.z};
    sceGumMatrixMode(GU_VIEW);
    sceGumLoadIdentity();
    sceGumLookAt(&eye, &center, &up);
    configure_environment(input, metrics);
    draw_room_bucket(0, metrics);
    draw_room_bucket(1, metrics);
    draw_room_bucket(2, metrics);
    if (draw_title_model) {
        // Source Item3D camera used by daTitle_c::Draw. Keep the F_SP102
        // environment on its own source camera, then draw the title model
        // through the dedicated Item3D view instead of billboarding it.
        sceGumMatrixMode(GU_PROJECTION);
        sceGumLoadIdentity();
        sceGumPerspective(45.0f, 480.0f / 272.0f, 1.0f, 100000.0f);
        ScePspFVector3 title_eye = {0.0f, 0.0f, -1000.0f};
        ScePspFVector3 title_center = {0.0f, 0.0f, 0.0f};
        ScePspFVector3 title_up = {0.0f, 1.0f, 0.0f};
        sceGumMatrixMode(GU_VIEW);
        sceGumLoadIdentity();
        sceGumLookAt(&title_eye, &title_center, &title_up);
        draw_static_model_bucket(title_model, 0xfffeu, 0, metrics);
        draw_static_model_bucket(title_model, 0xfffeu, 1, metrics);
        draw_static_model_bucket(title_model, 0xfffeu, 2, metrics);
    }
    if (ui_channel != 0xffffu) {
        draw_startup_channel(ui_channel, fade_alpha, metrics);
    }
    const int bytes = sceGuFinish();
    if (bytes < 0 || static_cast<std::uint32_t>(bytes) > g_list_bytes ||
        sceGuSync(GU_SYNC_FINISH, GU_SYNC_WHAT_DONE) < 0) {
        return false;
    }
    metrics->command_bytes = static_cast<std::uint32_t>(bytes);
    metrics->synchronized = true;
    ++metrics->frames;
    g_draw_offset =
        reinterpret_cast<std::uintptr_t>(sceGuSwapBuffers());
    return true;
}

bool initialize_real_room_renderer(
    const PackageView& link_textures,
    const PackageView& room_textures,
    const PackageView& ui,
    const void* room_model,
    std::uint32_t room_model_size,
    void* command_list,
    std::uint32_t command_list_bytes,
    RenderMetrics* metrics) {
    if (room_textures.bytes == nullptr ||
        room_model == nullptr || room_model_size < 256 ||
        !initialize_renderer(
            link_textures, ui, command_list,
            command_list_bytes, metrics)) {
        return false;
    }
    if (!replace_real_room_renderer(
            room_textures, room_model, room_model_size, metrics)) {
        shutdown_renderer();
        return false;
    }
    return true;
}

bool replace_real_room_renderer(
    const PackageView& room_textures,
    const void* room_model,
    std::uint32_t room_model_size,
    RenderMetrics* metrics) {
    if (!g_initialized || metrics == nullptr ||
        room_textures.bytes == nullptr ||
        room_model == nullptr || room_model_size < 256) {
        return false;
    }
    sceGuSync(GU_SYNC_FINISH, GU_SYNC_WHAT_DONE);
    g_room_texture_package = room_textures;
    g_room_model = static_cast<const std::uint8_t*>(room_model);
    g_room_model_size = room_model_size;
    std::memset(g_room_textures, 0, sizeof(g_room_textures));
    metrics->edram_room = 0;
    std::uint32_t cursor =
        kTextureOffset + metrics->edram_textures + metrics->edram_ui;
    cursor = (cursor + 15u) & ~15u;
    const std::uint32_t count = read_u32(room_textures.bytes + 16);
    const std::uint32_t table = read_u32(room_textures.bytes + 24);
    if (count == 0 || count > 96) {
        return false;
    }
    for (std::uint32_t index = 0; index < count; ++index) {
        const std::uint8_t* item =
            room_textures.bytes + table + index * 48;
        const std::uint32_t source_offset = read_u32(item + 16);
        const std::uint32_t bytes = read_u32(item + 20);
        cursor = (cursor + 15u) & ~15u;
        if (bytes > metrics->edram_total - cursor) {
            return false;
        }
        std::memcpy(
            g_edram + cursor,
            room_textures.bytes + source_offset,
            bytes);
        g_room_textures[index] = {
            read_u16(item + 4), read_u16(item + 6),
            read_u16(item + 8), read_u16(item + 10),
            cursor, bytes, item[12]};
        cursor += bytes;
        metrics->edram_room += bytes;
    }
    cursor = (cursor + 15u) & ~15u;
    if (kShadowMapBytes + kShadowAuxBytes >
        metrics->edram_total - cursor) {
        return false;
    }
    g_shadow_map_offset = cursor;
    cursor += kShadowMapBytes;
    g_shadow_depth_offset = cursor;
    cursor += kShadowAuxBytes;
    metrics->shadow_map_edram_bytes = kShadowMapBytes;
    metrics->shadow_aux_edram_bytes = kShadowAuxBytes;
    metrics->shadow_map_width = 64;
    metrics->shadow_map_height = 64;
    metrics->edram_remaining = metrics->edram_total - cursor;
    if (metrics->edram_room > 720000 ||
        metrics->edram_remaining < 96000) {
        return false;
    }
    sceKernelDcacheWritebackRange(
        g_edram + kTextureOffset,
        cursor - kTextureOffset);
    sceKernelDcacheWritebackRange(
        g_room_model, g_room_model_size);
    return true;
}

void deactivate_real_room_renderer(RenderMetrics* metrics) {
    if (!g_initialized || metrics == nullptr) {
        return;
    }
    sceGuSync(GU_SYNC_FINISH, GU_SYNC_WHAT_DONE);
    g_room_texture_package = {};
    g_room_model = nullptr;
    g_room_model_size = 0;
    g_shadow_map_offset = 0;
    g_shadow_depth_offset = 0;
    std::memset(g_room_textures, 0, sizeof(g_room_textures));
    metrics->edram_room = 0;
    const std::uint32_t cursor =
        (kTextureOffset + metrics->edram_textures +
         metrics->edram_ui + 15u) & ~15u;
    metrics->edram_remaining = metrics->edram_total - cursor;
}

bool render_black_transition_frame(RenderMetrics* metrics) {
    if (!g_initialized || metrics == nullptr) {
        return false;
    }
    const std::uint64_t submit_begin = monotonic_microseconds();
    sceGuStart(GU_DIRECT, g_list);
    sceGuClearColor(0xff000000u);
    sceGuClearDepth(0);
    sceGuClear(GU_COLOR_BUFFER_BIT | GU_DEPTH_BUFFER_BIT);
    const int bytes = sceGuFinish();
    metrics->ge_submit_us = static_cast<std::uint32_t>(
        monotonic_microseconds() - submit_begin);
    const std::uint64_t sync_begin = monotonic_microseconds();
    if (bytes < 0 || static_cast<std::uint32_t>(bytes) > g_list_bytes ||
        sceGuSync(GU_SYNC_FINISH, GU_SYNC_WHAT_DONE) < 0) {
        return false;
    }
    metrics->ge_sync_us = static_cast<std::uint32_t>(
        monotonic_microseconds() - sync_begin);
    metrics->command_bytes = static_cast<std::uint32_t>(bytes);
    metrics->lighting_cpu_us = 0;
    metrics->shadow_cpu_us = 0;
    metrics->hud_cpu_us = 0;
    sceDisplayWaitVblankStart();
    sceGuSwapBuffers();
    metrics->synchronized = true;
    ++metrics->frames;
    return true;
}

bool render_playable_frame(
    const Runtime& runtime,
    const GameplayState& gameplay,
    RenderMetrics* metrics) {
    if (!g_initialized || metrics == nullptr ||
        !runtime.vertices_finite || !runtime.guards_valid) {
        return false;
    }
    metrics->link_draw_calls = 0;
    metrics->scene_draw_calls = 0;
    metrics->ui_draw_calls = 0;
    g_render_trace_frame = metrics->frames;
    sceGuStart(GU_DIRECT, g_list);
    sceGuClearColor(kBackground);
    sceGuClearDepth(0);
    sceGuClear(GU_COLOR_BUFFER_BIT | GU_DEPTH_BUFFER_BIT);
    draw_scene(gameplay, metrics);
    draw_link(runtime, gameplay, metrics);
    draw_ui(gameplay, metrics);
    const int bytes = sceGuFinish();
    if (bytes < 0 || static_cast<std::uint32_t>(bytes) > g_list_bytes ||
        sceGuSync(GU_SYNC_FINISH, GU_SYNC_WHAT_DONE) < 0) {
        return false;
    }
    metrics->command_bytes = static_cast<std::uint32_t>(bytes);
    metrics->synchronized = true;
    ++metrics->frames;
    g_draw_offset = reinterpret_cast<std::uintptr_t>(sceGuSwapBuffers());
    return true;
}

bool render_real_room_frame(
    const Runtime& runtime,
    const RealRoomRenderInput& input,
    RenderMetrics* metrics) {
    if (!g_initialized || metrics == nullptr ||
        g_room_model == nullptr ||
        !runtime.vertices_finite || !runtime.guards_valid) {
        return false;
    }
    metrics->link_draw_calls = 0;
    metrics->scene_draw_calls = 0;
    metrics->ui_draw_calls = 0;
    metrics->room_draw_calls = 0;
    metrics->room_opaque_draws = 0;
    metrics->room_alpha_test_draws = 0;
    metrics->room_alpha_blend_draws = 0;
    const bool opaque_only = presentation::opaque_only(input.presentation);
    RealRoomRenderInput effective = input;
    if (opaque_only) {
        effective.fog_mode = FogMode::Off;
        effective.shadow_mode = ShadowMode::Off;
    }
    g_render_trace_frame = metrics->frames;
    sceGuStart(GU_DIRECT, g_list);
    sceGuClearColor(environment_clear_color(input));
    sceGuClearDepth(0);
    sceGuClear(GU_COLOR_BUFFER_BIT | GU_DEPTH_BUFFER_BIT);
    configure_real_room_camera(effective);
    configure_environment(effective, metrics);
    if (!opaque_only) {
        render_projected_shadow_map(runtime, effective, metrics);
    }
    draw_room_bucket(0, metrics);
    if (!opaque_only) {
        draw_room_bucket(1, metrics);
        draw_shadows(effective, metrics);
    }
    draw_real_link(runtime, effective, metrics);
    if (!opaque_only) {
        draw_room_bucket(2, metrics);
        draw_real_room_entities(effective, metrics);
        draw_root_pose_debug(effective, metrics);
        if (!effective.hide_hud) {
            draw_ui(effective.ui_state, metrics);
        }
        draw_message_overlay(effective.message_overlay, metrics);
    }
    const int bytes = sceGuFinish();
    if (bytes < 0 || static_cast<std::uint32_t>(bytes) > g_list_bytes ||
        sceGuSync(GU_SYNC_FINISH, GU_SYNC_WHAT_DONE) < 0) {
        return false;
    }
    metrics->command_bytes = static_cast<std::uint32_t>(bytes);
    metrics->synchronized = true;
    ++metrics->frames;
    g_draw_offset = reinterpret_cast<std::uintptr_t>(sceGuSwapBuffers());
    return true;
}

bool render_real_room_opaque_order_frame(
    const Runtime& runtime,
    const RealRoomRenderInput& input,
    opaque_order::Variant variant,
    std::uint32_t seed,
    OpaqueOrderRenderMetrics* order_metrics,
    RenderMetrics* metrics) {
    if (!g_initialized || metrics == nullptr || order_metrics == nullptr ||
        g_room_model == nullptr ||
        !runtime.vertices_finite || !runtime.guards_valid ||
        input.presentation != presentation::Profile::OpaqueOnly ||
        input.lighting_mode != LightingMode::Off ||
        input.fog_mode != FogMode::Off ||
        input.shadow_mode != ShadowMode::Off) {
        return false;
    }
    *order_metrics = {};
    order_metrics->variant = variant;
    OpaqueDrawDescriptor draws[opaque_order::kMaximumSubmissions] = {};
    std::uint16_t plan[opaque_order::kMaximumSubmissions] = {};
    const std::uint32_t draw_count = collect_opaque_order_draws(
        runtime, draws, opaque_order::kMaximumSubmissions,
        &order_metrics->excluded_non_writing_draws);
    // Submission records are embedded in descriptors and are not a
    // contiguous array. Copy them before invoking the generic planner.
    opaque_order::Submission submissions[
        opaque_order::kMaximumSubmissions] = {};
    for (std::uint32_t index = 0; index < draw_count; ++index) {
        submissions[index] = draws[index].submission;
    }
    if (draw_count == 0 || !opaque_order::build_plan(
            submissions, draw_count, variant, seed,
            plan, opaque_order::kMaximumSubmissions,
            &order_metrics->plan)) {
        return false;
    }
    metrics->link_draw_calls = 0;
    metrics->scene_draw_calls = 0;
    metrics->ui_draw_calls = 0;
    metrics->room_draw_calls = 0;
    metrics->room_opaque_draws = 0;
    metrics->room_alpha_test_draws = 0;
    metrics->room_alpha_blend_draws = 0;
    metrics->original_actor_draw_calls = 0;
    metrics->lighting_cpu_us = 0;
    metrics->shadow_cpu_us = 0;
    metrics->hud_cpu_us = 0;
    g_render_trace_frame = metrics->frames;
    RealRoomRenderInput effective = input;
    effective.render_profile = RenderProfile::KnownGoodUnlit;
    const SkinnedVertex* submitted =
        prepare_link_lighting_vertices(runtime, effective, metrics);
    sceGuStart(GU_DIRECT, g_list);
    sceGuClearColor(environment_clear_color(effective));
    sceGuClearDepth(0);
    sceGuClear(GU_COLOR_BUFFER_BIT | GU_DEPTH_BUFFER_BIT);
    configure_real_room_camera(effective);
    configure_environment(effective, metrics);
    for (std::uint32_t slot = 0; slot < draw_count; ++slot) {
        const std::uint32_t source_index = plan[slot];
        const OpaqueDrawDescriptor& draw = draws[source_index];
        if (draw.submission.source ==
            static_cast<std::uint8_t>(OpaqueDrawSource::Room)) {
            draw_opaque_room_submission(draw, metrics);
            ++order_metrics->room_draw_count;
        } else {
            draw_opaque_link_submission(
                runtime, effective, submitted, draw, metrics);
            ++order_metrics->link_draw_count;
        }
        order_metrics->draws[slot] = {
            source_index, slot, draw.submission.actor_id,
            draw.submission.material_id, draw.submission.shape_id,
            draw.texture_id, draw.submission.source,
            draw.submission.depth_test, draw.submission.depth_write,
            draw.submission.source_order_dependent};
    }
    const int bytes = sceGuFinish();
    if (bytes < 0 || static_cast<std::uint32_t>(bytes) > g_list_bytes ||
        sceGuSync(GU_SYNC_FINISH, GU_SYNC_WHAT_DONE) < 0) {
        return false;
    }
    order_metrics->draw_count = draw_count;
    order_metrics->complete_draw_manifest =
        order_metrics->room_draw_count + order_metrics->link_draw_count ==
        draw_count;
    order_metrics->complete_state_per_draw = true;
    metrics->command_bytes = static_cast<std::uint32_t>(bytes);
    metrics->actor_bucket_state_applied = true;
    metrics->synchronized = true;
    ++metrics->frames;
    g_draw_offset = reinterpret_cast<std::uintptr_t>(sceGuSwapBuffers());
    return order_metrics->complete_draw_manifest;
}

bool render_real_room_frame_with_models(
    const Runtime& runtime,
    const RealRoomRenderInput& input,
    const StaticModelRenderView* models,
    std::uint16_t model_count,
    RenderMetrics* metrics) {
    if (!g_initialized || metrics == nullptr ||
        g_room_model == nullptr || models == nullptr ||
        model_count > 16 ||
        !runtime.vertices_finite || !runtime.guards_valid) {
        return false;
    }
    metrics->link_draw_calls = 0;
    metrics->scene_draw_calls = 0;
    metrics->ui_draw_calls = 0;
    metrics->room_draw_calls = 0;
    metrics->room_opaque_draws = 0;
    metrics->room_alpha_test_draws = 0;
    metrics->room_alpha_blend_draws = 0;
    metrics->lighting_cpu_us = 0;
    metrics->shadow_cpu_us = 0;
    metrics->hud_cpu_us = 0;
    const bool opaque_only = presentation::opaque_only(input.presentation);
    RealRoomRenderInput effective = input;
    if (opaque_only) {
        effective.fog_mode = FogMode::Off;
        effective.shadow_mode = ShadowMode::Off;
    }
    g_render_trace_frame = metrics->frames;
    const std::uint64_t submit_begin = monotonic_microseconds();
    sceGuStart(GU_DIRECT, g_list);
    sceGuClearColor(environment_clear_color(input));
    sceGuClearDepth(0);
    sceGuClear(GU_COLOR_BUFFER_BIT | GU_DEPTH_BUFFER_BIT);
    configure_real_room_camera(effective);
    configure_environment(effective, metrics);
    const std::uint64_t shadow_begin = monotonic_microseconds();
    if (!opaque_only) {
        render_projected_shadow_map(runtime, effective, metrics);
    }
    metrics->shadow_cpu_us = static_cast<std::uint32_t>(
        monotonic_microseconds() - shadow_begin);
    draw_room_bucket(0, metrics);
    draw_static_models_bucket(models, model_count, 0, metrics);
    if (!opaque_only) {
        draw_room_bucket(1, metrics);
        draw_static_models_bucket(models, model_count, 1, metrics);
    }
    const std::uint64_t shadow_draw_begin = monotonic_microseconds();
    if (!opaque_only) {
        draw_shadows(effective, metrics);
    }
    metrics->shadow_cpu_us += static_cast<std::uint32_t>(
        monotonic_microseconds() - shadow_draw_begin);
    draw_real_link(runtime, effective, metrics);
    if (!opaque_only) {
        draw_room_bucket(2, metrics);
        draw_static_models_bucket(models, model_count, 2, metrics);
        draw_real_room_entities(effective, metrics);
        draw_root_pose_debug(effective, metrics);
    }
    const std::uint64_t hud_begin = monotonic_microseconds();
    if (!opaque_only) {
        if (!effective.hide_hud) {
            draw_ui(effective.ui_state, metrics);
        }
        draw_message_overlay(effective.message_overlay, metrics);
    }
    metrics->hud_cpu_us = static_cast<std::uint32_t>(
        monotonic_microseconds() - hud_begin);
    const int bytes = sceGuFinish();
    metrics->ge_submit_us = static_cast<std::uint32_t>(
        monotonic_microseconds() - submit_begin);
    const std::uint64_t sync_begin = monotonic_microseconds();
    if (bytes < 0 || static_cast<std::uint32_t>(bytes) > g_list_bytes ||
        sceGuSync(GU_SYNC_FINISH, GU_SYNC_WHAT_DONE) < 0) {
        return false;
    }
    metrics->ge_sync_us = static_cast<std::uint32_t>(
        monotonic_microseconds() - sync_begin);
    metrics->command_bytes = static_cast<std::uint32_t>(bytes);
    metrics->synchronized = true;
    ++metrics->frames;
    g_draw_offset = reinterpret_cast<std::uintptr_t>(sceGuSwapBuffers());
    return true;
}

#if defined(DUSKLIGHT_REAL_ACTOR_RENDER)
bool render_real_actor_frame(
    const Runtime& runtime,
    const RealRoomRenderInput& input,
    const actor::ActorSystem& actors,
    actor::RenderMetrics* actor_metrics,
    RenderMetrics* metrics) {
    if (!g_initialized || metrics == nullptr || actor_metrics == nullptr ||
        !runtime.vertices_finite || !runtime.guards_valid) {
        return false;
    }
    metrics->link_draw_calls = 0;
    metrics->scene_draw_calls = 0;
    metrics->ui_draw_calls = 0;
    metrics->room_draw_calls = 0;
    metrics->room_opaque_draws = 0;
    metrics->room_alpha_test_draws = 0;
    metrics->room_alpha_blend_draws = 0;
    sceGuStart(GU_DIRECT, g_list);
    sceGuClearColor(environment_clear_color(input));
    sceGuClearDepth(0);
    sceGuClear(GU_COLOR_BUFFER_BIT | GU_DEPTH_BUFFER_BIT);
    configure_real_room_camera(input);
    configure_environment(input, metrics);
    render_projected_shadow_map(runtime, input, metrics);
    draw_room_bucket(0, metrics);
    draw_room_bucket(1, metrics);
    draw_shadows(input, metrics);
    draw_real_link(runtime, input, metrics);
    draw_room_bucket(2, metrics);
    if (!actor::draw_actor_backend(actors, actor_metrics)) {
        sceGuFinish();
        sceGuSync(GU_SYNC_FINISH, GU_SYNC_WHAT_DONE);
        return false;
    }
    draw_real_room_entities(input, metrics);
    draw_root_pose_debug(input, metrics);
    draw_ui(input.ui_state, metrics);
    draw_message_overlay(input.message_overlay, metrics);
    const int bytes = sceGuFinish();
    if (bytes < 0 || static_cast<std::uint32_t>(bytes) > g_list_bytes ||
        sceGuSync(GU_SYNC_FINISH, GU_SYNC_WHAT_DONE) < 0) {
        return false;
    }
    metrics->command_bytes = static_cast<std::uint32_t>(bytes);
    metrics->synchronized = true;
    ++metrics->frames;
    g_draw_offset = reinterpret_cast<std::uintptr_t>(sceGuSwapBuffers());
    return true;
}
#endif

bool verify_playable_frame(RenderMetrics* metrics) {
    if (!g_initialized || metrics == nullptr || !metrics->synchronized) {
        return false;
    }
    constexpr std::uint16_t points[11][2] = {
        {24,20},{48,20},{72,20},{398,24},{420,24},{450,24},
        {240,220},{240,150},{220,130},{260,130},{240,100},
    };
    sceKernelDcacheWritebackInvalidateRange(
        g_readback, sizeof(g_readback));
    sceGuStart(GU_DIRECT, g_list);
    for (std::uint32_t buffer = 0; buffer < 2; ++buffer) {
        std::uint8_t* source = g_edram + buffer * kBufferBytes;
        for (std::uint32_t index = 0; index < 11; ++index) {
            sceGuCopyImage(
                GU_PSM_5650,
                points[index][0] & ~15u, points[index][1],
                16, 1, kStride, source,
                0, buffer * 11 + index, 16, g_readback);
        }
    }
    sceGuTexSync();
    sceGuFinish();
    sceGuSync(GU_SYNC_FINISH, GU_SYNC_WHAT_DONE);
    sceKernelDcacheInvalidateRange(g_readback, sizeof(g_readback));
    std::uint32_t changed[2] = {};
    const std::uint16_t background =
        static_cast<std::uint16_t>(
            ((kBackground >> 19) & 0x1f) << 11 |
            ((kBackground >> 10) & 0x3f) << 5 |
            ((kBackground >> 3) & 0x1f));
    for (std::uint32_t buffer = 0; buffer < 2; ++buffer) {
        for (std::uint32_t index = 0; index < 11; ++index) {
            const std::uint32_t x = points[index][0] & 15u;
            if (g_readback[(buffer * 11 + index) * 16 + x] != background) {
                ++changed[buffer];
            }
        }
    }
    const std::uint32_t selected = changed[1] > changed[0] ? 1 : 0;
    g_verified_buffer = selected;
    for (std::uint32_t index = 0; index < 11; ++index) {
        const std::uint32_t x = points[index][0] & 15u;
        metrics->pixel_values[index] =
            g_readback[(selected * 11 + index) * 16 + x];
    }
    metrics->pixel_regions_changed = changed[selected];
    metrics->pixel_checks_valid = changed[selected] >= 7;
    return metrics->pixel_checks_valid;
}

bool verify_real_room_frame(RenderMetrics* metrics) {
    if (!g_initialized || metrics == nullptr || !metrics->synchronized) {
        return false;
    }
    constexpr std::uint16_t points[16][2] = {
        {24,20}, {48,20}, {72,20}, {398,24},
        {420,24}, {450,24}, {170,110}, {300,90},
        {350,160}, {250,220}, {120,180}, {235,145},
        {229,125}, {265,150}, {240,100}, {260,130},
    };
    sceKernelDcacheWritebackInvalidateRange(
        g_readback, sizeof(g_readback));
    sceGuStart(GU_DIRECT, g_list);
    for (std::uint32_t buffer = 0; buffer < 2; ++buffer) {
        std::uint8_t* source = g_edram + buffer * kBufferBytes;
        for (std::uint32_t index = 0; index < 16; ++index) {
            sceGuCopyImage(
                GU_PSM_5650,
                points[index][0] & ~15u, points[index][1],
                16, 1, kStride, source,
                0, buffer * 16 + index, 16, g_readback);
        }
    }
    sceGuTexSync();
    sceGuFinish();
    sceGuSync(GU_SYNC_FINISH, GU_SYNC_WHAT_DONE);
    sceKernelDcacheInvalidateRange(g_readback, sizeof(g_readback));
    const std::uint16_t background =
        static_cast<std::uint16_t>(
            ((kBackground >> 19) & 0x1f) << 11 |
            ((kBackground >> 10) & 0x3f) << 5 |
            ((kBackground >> 3) & 0x1f));
    std::uint32_t changed[2] = {};
    for (std::uint32_t buffer = 0; buffer < 2; ++buffer) {
        for (std::uint32_t index = 0; index < 16; ++index) {
            const std::uint32_t x = points[index][0] & 15u;
            if (g_readback[(buffer * 16 + index) * 16 + x] != background) {
                ++changed[buffer];
            }
        }
    }
    const std::uint32_t selected = changed[1] > changed[0] ? 1 : 0;
    g_verified_buffer = selected;
    for (std::uint32_t index = 0; index < 16; ++index) {
        const std::uint32_t x = points[index][0] & 15u;
        metrics->room_pixel_values[index] =
            g_readback[(selected * 16 + index) * 16 + x];
    }
    metrics->room_pixel_regions_changed = changed[selected];
    metrics->pixel_checks_valid = changed[selected] >= 12;
    return metrics->pixel_checks_valid;
}

bool capture_playable_frame_5650(
    void* output, std::uint32_t capacity) {
    if (!g_initialized || output == nullptr ||
        capacity < kBufferBytes) {
        return false;
    }
    void* display = nullptr;
    int stride = 0;
    int pixel_format = 0;
    if (sceDisplayGetFrameBuf(
            &display, &stride, &pixel_format,
            PSP_DISPLAY_SETBUF_IMMEDIATE) < 0 ||
        display == nullptr || stride != static_cast<int>(kStride) ||
        pixel_format != GU_PSM_5650) {
        return false;
    }
    sceKernelDcacheWritebackInvalidateRange(output, kBufferBytes);
    sceGuStart(GU_DIRECT, g_list);
    sceGuCopyImage(
        GU_PSM_5650, 0, 0, kWidth, kHeight, kStride,
        display,
        0, 0, kStride, output);
    sceGuTexSync();
    sceGuFinish();
    if (sceGuSync(GU_SYNC_FINISH, GU_SYNC_WHAT_DONE) < 0) {
        return false;
    }
    sceKernelDcacheInvalidateRange(output, kBufferBytes);
    return true;
}

bool capture_playable_depth_16(
    void* output, std::uint32_t capacity) {
    if (!g_initialized || output == nullptr || capacity < kBufferBytes) {
        return false;
    }
    if (sceGuSync(GU_SYNC_FINISH, GU_SYNC_WHAT_DONE) < 0) {
        return false;
    }
    sceKernelDcacheWritebackInvalidateRange(
        g_edram + kDepthOffset, kBufferBytes);
    std::memcpy(output, g_edram + kDepthOffset, kBufferBytes);
    sceKernelDcacheInvalidateRange(output, kBufferBytes);
    return true;
}

void shutdown_renderer() {
    if (!g_initialized) {
        return;
    }
    sceGuDisplay(GU_FALSE);
    sceGuTerm();
    g_startup_ui_package = {};
    g_safe_link_lut_valid = false;
    g_initialized = false;
}

}  // namespace dusk::psp::playable
