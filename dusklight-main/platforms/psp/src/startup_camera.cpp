#include "dusk/psp/startup_camera.hpp"

#include <algorithm>
#include <iterator>

namespace dusk::psp::playable {

StartupTitleCamera startup_title_camera_from_source(
    std::uint32_t opening_frame) {
    struct SourceCamera {
        std::uint32_t frame;
        Vec3 eye;
        Vec3 center;
        float fov;
    };
    // DTRC v2, dCamera::view_setup, F_SP102/R00.
    static constexpr SourceCamera kSource[] = {
        {0, {34941.055f, -44.00001f, -16153.734f},
            {34941.055f, -44.0f, -15853.734f}, 60.0f},
        {300, {36756.99f, -238.6534f, -34663.188f},
            {36067.875f, -114.336784f, -34588.38f}, 44.148586f},
        {600, {28388.906f, 50.846233f, -10762.603f},
            {27685.34f, 7.910655f, -10752.634f}, 20.861956f},
        {900, {28470.965f, 761.1907f, 2668.0593f},
            {27815.928f, 768.33344f, 2935.426f}, 20.861956f},
        {1200, {49348.95f, 1936.0022f, 10238.328f},
            {48720.12f, 1947.7101f, 10545.577f}, 32.89359f},
        {1500, {16072.069f, 1398.2045f, 16531.83f},
            {16222.244f, 1489.822f, 17200.803f}, 37.180233f},
        {1800, {14387.364f, 1574.3092f, 17307.227f},
            {13709.805f, 1735.7026f, 17248.018f}, 38.144184f},
    };
    const std::uint32_t bounded = std::min(
        opening_frame, std::uint32_t{1800});
    std::uint32_t right = 1;
    while (right + 1 < std::size(kSource) &&
           bounded > kSource[right].frame) {
        ++right;
    }
    const SourceCamera& a = kSource[right - 1];
    const SourceCamera& b = kSource[right];
    const float t = static_cast<float>(bounded - a.frame) /
        static_cast<float>(b.frame - a.frame);
    const auto blend = [t](float left, float right_value) {
        return left + (right_value - left) * t;
    };
    return {
        {blend(a.eye.x, b.eye.x), blend(a.eye.y, b.eye.y),
         blend(a.eye.z, b.eye.z)},
        {blend(a.center.x, b.center.x),
         blend(a.center.y, b.center.y),
         blend(a.center.z, b.center.z)},
        {0.0f, 1.0f, 0.0f},
        blend(a.fov, b.fov),
        20.0f,
        200000.0f,
    };
}

}  // namespace dusk::psp::playable
