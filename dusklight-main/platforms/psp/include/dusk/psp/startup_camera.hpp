#ifndef DUSK_PSP_STARTUP_CAMERA_HPP
#define DUSK_PSP_STARTUP_CAMERA_HPP

#include "dusk/psp/playable_render.hpp"
#include "dusk/psp/startup_camera_track.hpp"

#include <cstdint>

namespace dusk::psp::playable {

// Samples the source-observed F_SP102 camera. Only the PSP depth range is
// adapted here; renderer projection setup owns the 480/272 aspect ratio.
StartupTitleCamera startup_title_camera_from_source(
    std::uint32_t opening_frame);

// Replays the source JStudio camera track at its native 30 Hz while the PSP
// presentation loop runs at 60 Hz. Returns false only when the validated track
// cannot provide a finite camera sample.
bool startup_title_camera_from_track(
    const camera::TrackView& track,
    std::uint32_t display_frame,
    StartupTitleCamera* output);

std::uint32_t startup_title_camera_display_frames(
    const camera::TrackView& track);

}  // namespace dusk::psp::playable

#endif
