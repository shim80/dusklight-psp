#ifndef DUSK_PSP_PSP_CONTROLS_HPP
#define DUSK_PSP_PSP_CONTROLS_HPP

#include "dusk/psp/playable_runtime.hpp"

#include <cstdint>

namespace dusk::psp::controls {

// Values intentionally mirror pspctrl.h so the mapper remains host-testable
// without requiring PSP SDK headers.
constexpr std::uint32_t kSelect = 0x000001u;
constexpr std::uint32_t kStart = 0x000008u;
constexpr std::uint32_t kUp = 0x000010u;
constexpr std::uint32_t kRight = 0x000020u;
constexpr std::uint32_t kDown = 0x000040u;
constexpr std::uint32_t kLeft = 0x000080u;
constexpr std::uint32_t kLeftTrigger = 0x000100u;
constexpr std::uint32_t kRightTrigger = 0x000200u;
constexpr std::uint32_t kTriangle = 0x001000u;
constexpr std::uint32_t kCircle = 0x002000u;
constexpr std::uint32_t kCross = 0x004000u;
constexpr std::uint32_t kSquare = 0x008000u;

struct PadSample {
    std::uint32_t buttons = 0;
    std::uint8_t analog_x = 128;
    std::uint8_t analog_y = 128;
};

struct MapperState {
    std::uint32_t previous_buttons = 0;
};

playable::Input map_gameplay_input(
    const PadSample& sample,
    MapperState* state);

}  // namespace dusk::psp::controls

#endif
