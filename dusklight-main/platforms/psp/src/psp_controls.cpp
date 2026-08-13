#include "dusk/psp/psp_controls.hpp"

#include <cmath>

namespace dusk::psp::controls {
namespace {

constexpr float kAnalogCenter = 128.0f;
constexpr float kAnalogExtent = 127.0f;
constexpr float kDeadzone = 24.0f / kAnalogExtent;

float normalize_axis(std::uint8_t raw) {
    float value = (static_cast<float>(raw) - kAnalogCenter) / kAnalogExtent;
    if (value < -1.0f) {
        value = -1.0f;
    } else if (value > 1.0f) {
        value = 1.0f;
    }
    const float magnitude = std::fabs(value);
    if (magnitude <= kDeadzone) {
        return 0.0f;
    }
    const float remapped = (magnitude - kDeadzone) / (1.0f - kDeadzone);
    return value < 0.0f ? -remapped : remapped;
}

bool held(std::uint32_t buttons, std::uint32_t mask) {
    return (buttons & mask) != 0;
}

bool pressed(
    std::uint32_t buttons,
    std::uint32_t previous,
    std::uint32_t mask) {
    return held(buttons, mask) && !held(previous, mask);
}

}  // namespace

playable::Input map_gameplay_input(
    const PadSample& sample,
    MapperState* state) {
    playable::Input input = {};
    const std::uint32_t previous = state != nullptr
        ? state->previous_buttons
        : 0u;

    input.analog_x = normalize_axis(sample.analog_x);
    // PSP Y grows downward; gameplay input uses forward/up as positive.
    input.analog_y = -normalize_axis(sample.analog_y);
    input.camera_left = held(sample.buttons, kLeftTrigger);
    input.camera_right = held(sample.buttons, kRightTrigger);
    input.zoom_in = held(sample.buttons, kTriangle);
    input.zoom_out = held(sample.buttons, kSquare);
    input.action_pressed = pressed(sample.buttons, previous, kCross);
    input.pause_pressed = pressed(sample.buttons, previous, kStart);
    input.cancel_pressed = pressed(sample.buttons, previous, kCircle);
    input.up_pressed = pressed(sample.buttons, previous, kUp);
    input.down_pressed = pressed(sample.buttons, previous, kDown);
    input.debug_pressed = pressed(sample.buttons, previous, kSelect);

    if (state != nullptr) {
        state->previous_buttons = sample.buttons;
    }
    return input;
}

}  // namespace dusk::psp::controls
