#include "dusk/psp/link_lighting.hpp"
#include "dusk/psp/playable_render.hpp"

#include <cmath>
#include <cstdio>

namespace playable = dusk::psp::playable;

namespace {

constexpr float kTolerance = 0.0001f;

bool near(float left, float right) {
    return std::fabs(left - right) <= kTolerance;
}

float luminance(playable::LinearRgb value) {
    return value.red * 0.2126f + value.green * 0.7152f +
        value.blue * 0.0722f;
}

}  // namespace

int main() {
    const playable::PspLinkMaterialLightingRecord material = {
        {0xFFFFFFFFu},
        {0xFF101010u},
        {0xFFFFFFFFu},
        {0xFF000000u},
        0,
        0,
        0,
        255,
        0,
        true,
    };
    constexpr dusk::psp::color::PackedArgb32 ambient = {0xFF3E3539u};
    constexpr dusk::psp::color::PackedArgb32 key = {0xFF78615Bu};
    constexpr playable::Vec3 light = {0.0f, 0.0f, 1.0f};
    constexpr playable::Vec3 camera = {0.0f, 0.0f, 1.0f};

    const playable::LinearRgb ambient_only =
        playable::evaluate_safe_link_lighting(
            material, ambient, key, light, light, camera,
            playable::SafeLinkLightingVariant::AmbientOnly);
    const playable::LinearRgb lit =
        playable::evaluate_safe_link_lighting(
            material, ambient, key, light, light, camera,
            playable::SafeLinkLightingVariant::WrappedDiffuse);
    const playable::LinearRgb away =
        playable::evaluate_safe_link_lighting(
            material, ambient, key, {0.0f, 0.0f, -1.0f}, light, camera,
            playable::SafeLinkLightingVariant::WrappedDiffuse);
    const playable::LinearRgb silhouette =
        playable::evaluate_safe_link_lighting(
            material, ambient, key, {1.0f, 0.0f, 0.0f}, light, camera,
            playable::SafeLinkLightingVariant::WrappedDiffuse);
    const playable::LinearRgb silhouette_rim =
        playable::evaluate_safe_link_lighting(
            material, ambient, key, {1.0f, 0.0f, 0.0f}, light, camera,
            playable::SafeLinkLightingVariant::WrappedDiffuseRim);
    const playable::Vec3 rotated = playable::world_vector_to_model(
        {1.0f, 0.0f, 0.0f}, 1.57079632679f);

    const bool valid =
        ambient_only.red >= playable::kSafeLinkLighting.minimum_illumination &&
        ambient_only.green >= playable::kSafeLinkLighting.minimum_illumination &&
        ambient_only.blue >= playable::kSafeLinkLighting.minimum_illumination &&
        luminance(lit) > luminance(ambient_only) &&
        near(away.red, ambient_only.red) &&
        near(away.green, ambient_only.green) &&
        near(away.blue, ambient_only.blue) &&
        luminance(silhouette_rim) > luminance(silhouette) &&
        near(rotated.x, 0.0f) && near(rotated.y, 0.0f) &&
        near(rotated.z, -1.0f) &&
        playable::render_profile_config(
            playable::RenderProfile::CandidateGame).lighting ==
            playable::LightingMode::SafeWrappedDiffuse;
    if (!valid) {
        std::fprintf(stderr, "safe Link lighting contract failed\n");
        return 1;
    }
    std::printf(
        "SAFE_LINK_LIGHTING_HOST_OK ambient=%.6f lit=%.6f "
        "away=%.6f silhouette=%.6f rim=%.6f floor=%.2f\n",
        luminance(ambient_only), luminance(lit), luminance(away),
        luminance(silhouette), luminance(silhouette_rim),
        playable::kSafeLinkLighting.minimum_illumination);
    return 0;
}
