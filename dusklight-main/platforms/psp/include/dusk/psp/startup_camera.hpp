#ifndef DUSK_PSP_STARTUP_CAMERA_HPP
#define DUSK_PSP_STARTUP_CAMERA_HPP

#include "dusk/psp/playable_render.hpp"

#include <cstdint>

namespace dusk::psp::playable {

// Samples the source-observed F_SP102 camera. Only the PSP depth range is
// adapted here; renderer projection setup owns the 480/272 aspect ratio.
StartupTitleCamera startup_title_camera_from_source(
    std::uint32_t opening_frame);

}  // namespace dusk::psp::playable

#endif
