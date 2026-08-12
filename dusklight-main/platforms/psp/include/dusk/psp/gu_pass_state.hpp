#ifndef DUSK_PSP_GU_PASS_STATE_HPP
#define DUSK_PSP_GU_PASS_STATE_HPP

#include <cstdint>

namespace dusk::psp::render {

enum class MatrixBinding : std::uint8_t {
    MainProjection,
    MainView,
    ActorModel,
    IdentityTexture,
    ShadowProjection,
    ShadowView,
    ShadowActorModel,
};

struct PspGuPassState {
    std::uint32_t draw_buffer;
    std::uint32_t display_buffer;
    std::uint32_t depth_buffer;
    std::uint16_t framebuffer_width;
    std::uint16_t framebuffer_height;
    std::uint16_t framebuffer_stride;
    std::uint8_t framebuffer_format;
    std::uint8_t depth_format;
    std::int16_t offset_x;
    std::int16_t offset_y;
    std::int16_t viewport_x;
    std::int16_t viewport_y;
    std::int16_t viewport_width;
    std::int16_t viewport_height;
    std::int16_t scissor_left;
    std::int16_t scissor_top;
    std::int16_t scissor_right;
    std::int16_t scissor_bottom;
    MatrixBinding projection;
    MatrixBinding view;
    MatrixBinding model;
    MatrixBinding texture_matrix;
    std::uint8_t texture_format;
    std::uint8_t texture_function;
    std::uint8_t texture_min_filter;
    std::uint8_t texture_mag_filter;
    std::uint8_t texture_wrap_u;
    std::uint8_t texture_wrap_v;
    float texture_scale_u;
    float texture_scale_v;
    float texture_offset_u;
    float texture_offset_v;
    std::uint32_t clut_address;
    std::uint32_t vertex_color;
    std::uint32_t color_material;
    std::uint32_t material_ambient;
    std::uint32_t material_diffuse;
    std::uint32_t material_specular;
    std::uint32_t ambient_global;
    std::uint8_t light_type;
    float light_direction[3];
    std::uint32_t light_ambient;
    std::uint32_t light_diffuse;
    std::uint32_t light_specular;
    std::uint8_t depth_function;
    std::uint8_t alpha_function;
    std::uint8_t blend_operation;
    std::uint8_t blend_source;
    std::uint8_t blend_destination;
    std::uint8_t cull_direction;
    bool scissor_enabled;
    bool texture_enabled;
    bool clut_enabled;
    bool lighting_enabled;
    bool light_enabled;
    bool cull_enabled;
    bool depth_test_enabled;
    bool depth_write_enabled;
    bool alpha_test_enabled;
    bool blend_enabled;
    bool fog_enabled;
    bool dither_enabled;
    bool stencil_enabled;

    constexpr bool operator==(const PspGuPassState&) const = default;
};

constexpr PspGuPassState canonical_actor_pass_state() {
    return {
        0, 0x44000, 0x88000,
        480, 272, 512, 0, 0,
        1808, 1912, 2048, 2048, 480, 272,
        0, 0, 480, 272,
        MatrixBinding::MainProjection,
        MatrixBinding::MainView,
        MatrixBinding::ActorModel,
        MatrixBinding::IdentityTexture,
        2, 0, 1, 1, 0, 0,
        1.0f, 1.0f, 0.0f, 0.0f,
        0, 0xffffffffu, 3,
        0xffffffffu, 0xffffffffu, 0xff000000u, 0xffffffffu,
        0, {0.0f, 1.0f, 0.0f},
        0xff000000u, 0xffffffffu, 0xff000000u,
        1, 0, 0, 0, 0, 0,
        true, true, false, false, false, true, true, true,
        false, false, false, false, false,
    };
}

constexpr PspGuPassState canonical_shadow_map_pass_state() {
    PspGuPassState result = canonical_actor_pass_state();
    result.draw_buffer = 0xffffffffu;
    result.depth_buffer = 0xffffffffu;
    result.framebuffer_width = 64;
    result.framebuffer_height = 64;
    result.framebuffer_stride = 64;
    result.framebuffer_format = 2;
    result.offset_x = 2016;
    result.offset_y = 2016;
    result.viewport_width = 64;
    result.viewport_height = 64;
    result.scissor_right = 64;
    result.scissor_bottom = 64;
    result.projection = MatrixBinding::ShadowProjection;
    result.view = MatrixBinding::ShadowView;
    result.model = MatrixBinding::ShadowActorModel;
    result.texture_enabled = false;
    result.cull_enabled = false;
    return result;
}

}  // namespace dusk::psp::render

#endif
