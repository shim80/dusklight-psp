#include "dusk/psp/psp_controls.hpp"

#include <cassert>
#include <cmath>
#include <cstdio>

namespace {
bool near(float a, float b, float epsilon = 0.02f) {
    return std::fabs(a - b) <= epsilon;
}
}

int main() {
    using namespace dusk::psp::controls;

    MapperState state = {};
    auto input = map_gameplay_input({0, 128, 128}, &state);
    assert(near(input.analog_x, 0.0f));
    assert(near(input.analog_y, 0.0f));

    input = map_gameplay_input({0, 151, 105}, &state);
    assert(near(input.analog_x, 0.0f));
    assert(near(input.analog_y, 0.0f));

    input = map_gameplay_input({0, 255, 0}, &state);
    assert(input.analog_x > 0.99f);
    assert(input.analog_y > 0.99f);

    const std::uint32_t gameplay_buttons =
        kCross | kStart | kCircle | kUp | kLeftTrigger | kRightTrigger |
        kTriangle | kSquare | kSelect;
    input = map_gameplay_input({gameplay_buttons, 128, 128}, &state);
    assert(input.action_pressed);
    assert(input.pause_pressed);
    assert(input.cancel_pressed);
    assert(input.up_pressed);
    assert(!input.down_pressed);
    assert(input.camera_left);
    assert(input.camera_right);
    assert(input.zoom_in);
    assert(input.zoom_out);
    assert(input.debug_pressed);

    // Held buttons remain held for continuous camera/zoom, but gameplay/menu
    // actions are edge-triggered and therefore cannot double-fire per frame.
    input = map_gameplay_input({gameplay_buttons, 128, 128}, &state);
    assert(!input.action_pressed);
    assert(!input.pause_pressed);
    assert(!input.cancel_pressed);
    assert(!input.up_pressed);
    assert(input.camera_left);
    assert(input.camera_right);
    assert(input.zoom_in);
    assert(input.zoom_out);
    assert(!input.debug_pressed);

    input = map_gameplay_input({0, 128, 128}, &state);
    assert(!input.action_pressed);
    input = map_gameplay_input({kCross | kDown, 128, 128}, &state);
    assert(input.action_pressed);
    assert(input.down_pressed);

    std::puts(
        "PSP_CONTROLS_HOST_OK analog=deadzone+normalized "
        "move=stick action=cross camera=L/R zoom=triangle/square "
        "pause=start cancel=circle menu=dpad debug=select edges=debounced");
    return 0;
}
