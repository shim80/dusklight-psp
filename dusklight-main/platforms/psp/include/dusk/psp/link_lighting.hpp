#ifndef DUSK_PSP_LINK_LIGHTING_HPP
#define DUSK_PSP_LINK_LIGHTING_HPP

#include "dusk/psp/color_packing.hpp"
#include "dusk/psp/playable_package.hpp"
#include "dusk/psp/playable_runtime.hpp"

#include <algorithm>
#include <cmath>
#include <cstdint>

namespace dusk::psp::playable {

struct PspLinkMaterialLightingRecord {
    color::PackedArgb32 base;
    color::PackedArgb32 ambient;
    color::PackedArgb32 diffuse;
    color::PackedArgb32 emissive;
    std::uint16_t texture;
    std::uint16_t source_material_id;
    std::uint8_t texture_function;
    std::uint8_t alpha;
    std::uint8_t fallback_reason;
    bool lighting_enabled;
};

struct LinearRgb {
    float red;
    float green;
    float blue;
};

enum class SafeLinkLightingVariant : std::uint8_t {
    AmbientOnly,
    WrappedDiffuse,
    WrappedDiffuseRim,
};

struct SafeLinkLightingParameters {
    float ambient_strength;
    float key_strength;
    float wrap_bias;
    float minimum_illumination;
    float rim_strength;
    LinearRgb rim_color;
};

constexpr SafeLinkLightingParameters kSafeLinkLighting = {
    0.58f,
    0.32f,
    0.35f,
    0.52f,
    0.045f,
    {0.84f, 0.90f, 1.0f},
};

inline bool read_link_material_lighting(
    const PackageView& package,
    std::uint32_t index,
    PspLinkMaterialLightingRecord* record) {
    if (record == nullptr || package.bytes == nullptr ||
        index >= read_u32(package.bytes + 20)) {
        return false;
    }
    const std::uint32_t table = read_u32(package.bytes + 32);
    const std::uint32_t stride = read_u32(package.bytes + 36);
    if (stride != 32 || table > package.size ||
        index > (package.size - table) / stride) {
        return false;
    }
    const std::uint8_t* item = package.bytes + table + index * stride;
    *record = {
        {read_u32(item + 4)},
        {read_u32(item + 8)},
        {read_u32(item + 12)},
        {read_u32(item + 16)},
        read_u16(item),
        read_u16(item + 20),
        item[22],
        item[23],
        item[26],
        item[3] != 0,
    };
    return true;
}

constexpr LinearRgb linear_rgb(color::PackedArgb32 packed) {
    const color::GxColorRgba8 value = color::unpack_argb(packed);
    return {
        static_cast<float>(value.red) / 255.0f,
        static_cast<float>(value.green) / 255.0f,
        static_cast<float>(value.blue) / 255.0f,
    };
}

inline Vec3 normalized_direction(Vec3 value) {
    const float length = std::sqrt(
        value.x * value.x + value.y * value.y + value.z * value.z);
    if (!std::isfinite(length) || length < 0.000001f) {
        return {0.0f, 1.0f, 0.0f};
    }
    return {value.x / length, value.y / length, value.z / length};
}

inline Vec3 world_vector_to_model(Vec3 world, float model_yaw) {
    const float sine = std::sin(model_yaw);
    const float cosine = std::cos(model_yaw);
    return {
        cosine * world.x + sine * world.z,
        world.y,
        -sine * world.x + cosine * world.z,
    };
}

inline Vec3 world_light_to_model(Vec3 world, float model_yaw) {
    return normalized_direction(world_vector_to_model(world, model_yaw));
}

inline Vec3 world_light_ray_to_model_surface(
    Vec3 world_ray, float model_yaw) {
    const Vec3 model_ray =
        world_light_to_model(world_ray, model_yaw);
    return {-model_ray.x, -model_ray.y, -model_ray.z};
}

inline LinearRgb evaluate_source_lighting(
    const PspLinkMaterialLightingRecord& material,
    color::PackedArgb32 environment_ambient,
    color::PackedArgb32 environment_key,
    Vec3 normal,
    Vec3 light_to_surface,
    float exposure) {
    const LinearRgb base = linear_rgb(material.base);
    if (!material.lighting_enabled) {
        return base;
    }
    const LinearRgb ambient = linear_rgb(material.ambient);
    const LinearRgb diffuse = linear_rgb(material.diffuse);
    const LinearRgb emissive = linear_rgb(material.emissive);
    const LinearRgb environment_a = linear_rgb(environment_ambient);
    const LinearRgb environment_d = linear_rgb(environment_key);
    normal = normalized_direction(normal);
    light_to_surface = normalized_direction(light_to_surface);
    const float ndotl = std::max(
        0.0f,
        normal.x * light_to_surface.x +
        normal.y * light_to_surface.y +
        normal.z * light_to_surface.z);
    return {
        std::clamp(
            (emissive.red + ambient.red * environment_a.red +
             diffuse.red * environment_d.red * ndotl) * exposure,
            0.0f, 1.0f),
        std::clamp(
            (emissive.green + ambient.green * environment_a.green +
             diffuse.green * environment_d.green * ndotl) * exposure,
            0.0f, 1.0f),
        std::clamp(
            (emissive.blue + ambient.blue * environment_a.blue +
             diffuse.blue * environment_d.blue * ndotl) * exposure,
            0.0f, 1.0f),
    };
}

inline LinearRgb normalized_chroma(color::PackedArgb32 packed) {
    const LinearRgb value = linear_rgb(packed);
    const float maximum = std::max(
        value.red, std::max(value.green, value.blue));
    if (!std::isfinite(maximum) || maximum < 0.000001f) {
        return {1.0f, 1.0f, 1.0f};
    }
    return {
        value.red / maximum,
        value.green / maximum,
        value.blue / maximum,
    };
}

inline LinearRgb evaluate_prepared_safe_link_lighting_factors(
    LinearRgb base,
    LinearRgb emissive,
    bool lighting_enabled,
    LinearRgb ambient,
    LinearRgb key,
    float wrapped,
    float rim,
    SafeLinkLightingVariant variant,
    const SafeLinkLightingParameters& parameters = kSafeLinkLighting) {
    if (!lighting_enabled) {
        return base;
    }
    wrapped = std::clamp(wrapped, 0.0f, 1.0f);
    rim = std::clamp(rim, 0.0f, 1.0f);
    const float key_strength =
        variant == SafeLinkLightingVariant::AmbientOnly
            ? 0.0f : parameters.key_strength;
    const float rim_strength =
        variant == SafeLinkLightingVariant::WrappedDiffuseRim
            ? parameters.rim_strength : 0.0f;
    const auto channel = [&](float base_channel,
                             float ambient_channel,
                             float key_channel,
                             float rim_channel,
                             float emissive_channel) {
        const float illumination = std::max(
            parameters.minimum_illumination,
            ambient_channel * parameters.ambient_strength +
                key_channel * wrapped * key_strength +
                rim_channel * rim * rim_strength);
        return std::clamp(
            emissive_channel + base_channel * illumination,
            0.0f, 1.0f);
    };
    return {
        channel(
            base.red, ambient.red, key.red,
            parameters.rim_color.red, emissive.red),
        channel(
            base.green, ambient.green, key.green,
            parameters.rim_color.green, emissive.green),
        channel(
            base.blue, ambient.blue, key.blue,
            parameters.rim_color.blue, emissive.blue),
    };
}

inline LinearRgb evaluate_prepared_safe_link_lighting(
    LinearRgb base,
    LinearRgb emissive,
    bool lighting_enabled,
    LinearRgb ambient,
    LinearRgb key,
    Vec3 unit_normal,
    Vec3 unit_light_to_surface,
    Vec3 unit_view_to_camera,
    SafeLinkLightingVariant variant,
    const SafeLinkLightingParameters& parameters = kSafeLinkLighting) {
    const float ndotl = std::clamp(
        unit_normal.x * unit_light_to_surface.x +
        unit_normal.y * unit_light_to_surface.y +
        unit_normal.z * unit_light_to_surface.z,
        -1.0f, 1.0f);
    const float wrapped = std::clamp(
        (ndotl + parameters.wrap_bias) /
            (1.0f + parameters.wrap_bias),
        0.0f, 1.0f);
    float rim = 0.0f;
    if (variant == SafeLinkLightingVariant::WrappedDiffuseRim) {
        const float ndotv = std::clamp(
            unit_normal.x * unit_view_to_camera.x +
            unit_normal.y * unit_view_to_camera.y +
            unit_normal.z * unit_view_to_camera.z,
            0.0f, 1.0f);
        rim = (1.0f - ndotv) * (1.0f - ndotv);
    }
    return evaluate_prepared_safe_link_lighting_factors(
        base, emissive, lighting_enabled, ambient, key,
        wrapped, rim, variant, parameters);
}

inline LinearRgb evaluate_safe_link_lighting(
    const PspLinkMaterialLightingRecord& material,
    color::PackedArgb32 environment_ambient,
    color::PackedArgb32 environment_key,
    Vec3 normal,
    Vec3 light_to_surface,
    Vec3 view_to_camera,
    SafeLinkLightingVariant variant,
    const SafeLinkLightingParameters& parameters = kSafeLinkLighting) {
    const LinearRgb base = linear_rgb(material.base);
    const LinearRgb emissive = linear_rgb(material.emissive);
    const LinearRgb ambient = normalized_chroma(environment_ambient);
    const LinearRgb key = normalized_chroma(environment_key);
    normal = normalized_direction(normal);
    light_to_surface = normalized_direction(light_to_surface);
    view_to_camera = normalized_direction(view_to_camera);
    return evaluate_prepared_safe_link_lighting(
        base, emissive, material.lighting_enabled,
        ambient, key, normal, light_to_surface,
        view_to_camera, variant, parameters);
}

inline color::PspColorAbgr8888 psp_vertex_color(
    LinearRgb value, std::uint8_t alpha = 255) {
    const auto channel = [](float component) {
        return static_cast<std::uint8_t>(
            std::clamp(component, 0.0f, 1.0f) * 255.0f + 0.5f);
    };
    return color::to_psp_abgr({
        channel(value.red),
        channel(value.green),
        channel(value.blue),
        alpha,
    });
}

inline color::PspColorAbgr8888 psp_normal_debug_color(Vec3 normal) {
    normal = normalized_direction(normal);
    return psp_vertex_color({
        normal.x * 0.5f + 0.5f,
        normal.y * 0.5f + 0.5f,
        normal.z * 0.5f + 0.5f,
    });
}

}  // namespace dusk::psp::playable

#endif
