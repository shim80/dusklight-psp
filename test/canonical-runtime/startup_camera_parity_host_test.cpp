#include "dusk/psp/startup_camera.hpp"

#include <cmath>
#include <cstdint>
#include <cstdio>

namespace {

bool near(float value, float expected, float tolerance = 0.02f) {
    return std::fabs(value - expected) <= tolerance;
}

}  // namespace

int main() {
    using dusk::psp::playable::startup_title_camera_from_source;
    const auto first = startup_title_camera_from_source(0);
    const auto middle = startup_title_camera_from_source(900);
    const auto last = startup_title_camera_from_source(1800);
    const auto bounded = startup_title_camera_from_source(9999);
    if (!near(first.eye.x, 34941.055f) ||
        !near(first.center.z, -15853.734f) ||
        !near(first.fov, 60.0f) ||
        !near(middle.eye.y, 761.1907f) ||
        !near(middle.center.z, 2935.426f) ||
        !near(last.eye.x, 14387.364f) ||
        !near(last.center.y, 1735.7026f) ||
        !near(last.fov, 38.144184f) ||
        !near(bounded.eye.x, last.eye.x) ||
        !near(last.near_plane, 20.0f) ||
        !near(last.far_plane, 200000.0f)) {
        return 1;
    }
    std::puts(
        "STARTUP_CAMERA_PARITY_HOST_OK checkpoints=0,900,1800 "
        "tolerance=0.02 adaptation=aspect,near");
    return 0;
}
