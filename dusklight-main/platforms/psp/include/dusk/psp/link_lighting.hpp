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

inline Vec3 world_light_to_model(Vec3 world, float model_yaw) {
    const Vec3 direction = normalized_direction(world);
    const float sine = std::sin(model_yaw);
    const float cosine = std::cos(model_yaw);
    return normalized_direction({
        cosine * direction.x + sine * direction.z,
        direction.y,
        -sine * direction.x + cosine * direction.z,
    });
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
