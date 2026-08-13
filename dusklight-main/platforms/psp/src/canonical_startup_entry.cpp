#include "dusk/psp/canonical_startup_entry.hpp"

#include "dusk/psp/bck_runtime.hpp"
#include "dusk/psp/canonical_game.hpp"
#include "dusk/psp/canonical_startup_loader.hpp"
#include "dusk/psp/platform.hpp"
#include "dusk/psp/playable_render.hpp"
#include "dusk/psp/room_package.hpp"
#include "dusk/psp/startup_camera.hpp"
#include "dusk/psp/startup_runtime.hpp"

#include <pspctrl.h>

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstring>

namespace dusk::psp::game {
namespace {

constexpr std::uint32_t kStartupCapabilities =
    startup::Capability::Ui | startup::Capability::Stage |
    startup::Capability::Camera | startup::Capability::Events |
    startup::Capability::TitleModel | startup::Capability::FileSelection |
    startup::Capability::Gameplay;
constexpr std::uint32_t kStartupFrameDelayMicroseconds = 16667;
constexpr std::uint32_t kOpeningFrames = 1800;
constexpr std::uint32_t kTitleLogoFrames = 300;
constexpr std::uint16_t kNoUiChannel = 0xffffu;
constexpr std::uint16_t kTitlePromptChannel = 8;
constexpr std::uint32_t kTitleLogoClip = 7;
constexpr std::uint32_t kTitleLogoVertices = 24;
constexpr std::uint32_t kTitleLogoSubmeshes = 6;
constexpr std::uint32_t kCommandListBytes = 256 * 1024;
alignas(16) std::uint8_t g_startup_command_list[kCommandListBytes] = {};

struct TitleLogoGeometry {
    float positions[kTitleLogoVertices][3];
    std::uint32_t vertex_offset;
    std::uint32_t index_offset;
    std::uint32_t submesh_offset;
    bool valid;
};

bool record_for_segment(
    const startup::PackageView& package,
    startup::Segment segment,
    startup::SegmentRecord* output) {
    if (output == nullptr) {
        return false;
    }
    for (std::uint32_t index = 0; index < package.segment_count; ++index) {
        startup::SegmentRecord record = {};
        if (!package.segment(index, &record)) {
            return false;
        }
        if (record.segment == segment) {
            *output = record;
            return true;
        }
    }
    return false;
}

std::uint8_t timed_fade_alpha(
    const startup::SegmentRecord& record,
    std::uint32_t frame) {
    std::uint32_t alpha = 255;
    if (record.fade_in_frames != 0 && frame < record.fade_in_frames) {
        alpha = std::min<std::uint32_t>(
            alpha, frame * 255u / record.fade_in_frames);
    }
    if (record.duration_frames != 0 && record.fade_out_frames != 0 &&
        frame + record.fade_out_frames >= record.duration_frames) {
        const std::uint32_t remaining =
            record.duration_frames > frame
                ? record.duration_frames - frame
                : 0;
        alpha = std::min<std::uint32_t>(
            alpha, remaining * 255u / record.fade_out_frames);
    }
    return static_cast<std::uint8_t>(alpha);
}

std::uint8_t title_prompt_alpha(std::uint32_t frame) {
    constexpr std::uint32_t kPeriod = 120;
    constexpr std::uint32_t kHalf = kPeriod / 2;
    const std::uint32_t phase = frame % kPeriod;
    const std::uint32_t ramp = phase < kHalf ? phase : kPeriod - phase;
    return static_cast<std::uint8_t>(96u + ramp * 159u / kHalf);
}

playable::PackageView playable_view(const room::PackageView& view) {
    return {view.bytes, view.size, view.expected_crc, view.actual_crc};
}

void write_f32(std::uint8_t* bytes, float value) {
    std::uint32_t bits = 0;
    std::memcpy(&bits, &value, sizeof(bits));
    bytes[0] = static_cast<std::uint8_t>(bits);
    bytes[1] = static_cast<std::uint8_t>(bits >> 8);
    bytes[2] = static_cast<std::uint8_t>(bits >> 16);
    bytes[3] = static_cast<std::uint8_t>(bits >> 24);
}

bool initialize_title_logo_geometry(
    const CanonicalStartupPackages& packages,
    TitleLogoGeometry* geometry) {
    if (geometry == nullptr || packages.title_logo_model.bytes == nullptr) {
        return false;
    }
    *geometry = {};
    const std::uint8_t* model = packages.title_logo_model.bytes;
    if (room::read_u32(model + 20) != kTitleLogoVertices ||
        room::read_u32(model + 32) != kTitleLogoSubmeshes) {
        return false;
    }
    const std::uint32_t section_table = room::read_u32(model + 72);
    geometry->vertex_offset = room::read_u32(model + section_table + 4);
    geometry->index_offset = room::read_u32(model + section_table + 32 + 4);
    geometry->submesh_offset = room::read_u32(model + section_table + 64 + 4);
    bool claimed[kTitleLogoVertices] = {};
    for (std::uint32_t part = 0; part < kTitleLogoSubmeshes; ++part) {
        const std::uint8_t* submesh =
            model + geometry->submesh_offset + part * 48;
        const std::uint32_t first = room::read_u32(submesh);
        const std::uint32_t count = room::read_u32(submesh + 4);
        for (std::uint32_t index = 0; index < count; ++index) {
            const std::uint16_t vertex = room::read_u16(
                model + geometry->index_offset + (first + index) * 2);
            if (vertex >= kTitleLogoVertices) {
                return false;
            }
            claimed[vertex] = true;
        }
    }
    for (std::uint32_t vertex = 0; vertex < kTitleLogoVertices; ++vertex) {
        if (!claimed[vertex]) {
            return false;
        }
        const std::uint8_t* source =
            model + geometry->vertex_offset + vertex * 24 + 12;
        geometry->positions[vertex][0] = room::read_f32(source);
        geometry->positions[vertex][1] = room::read_f32(source + 4);
        geometry->positions[vertex][2] = room::read_f32(source + 8);
    }
    geometry->valid = true;
    return true;
}

bool apply_title_logo_animation(
    CanonicalStartupPackages* packages,
    const TitleLogoGeometry& geometry,
    animation::PspBckPlayer* player) {
    if (packages == nullptr || !geometry.valid || player == nullptr) {
        return false;
    }
    animation::Transform root = {};
    if (!player->sample_joint(0, &root)) {
        return false;
    }
    std::uint8_t* model = packages->title_logo_model.bytes;
    bool written[kTitleLogoVertices] = {};
    for (std::uint32_t part = 0; part < kTitleLogoSubmeshes; ++part) {
        animation::Transform local = {};
        if (!player->sample_joint(
                static_cast<std::uint16_t>(part + 1), &local)) {
            return false;
        }
        const std::uint8_t* submesh =
            model + geometry.submesh_offset + part * 48;
        const std::uint32_t first = room::read_u32(submesh);
        const std::uint32_t count = room::read_u32(submesh + 4);
        for (std::uint32_t index = 0; index < count; ++index) {
            const std::uint16_t vertex = room::read_u16(
                model + geometry.index_offset + (first + index) * 2);
            if (written[vertex]) {
                continue;
            }
            written[vertex] = true;
            std::uint8_t* destination =
                model + geometry.vertex_offset + vertex * 24 + 12;
            for (std::uint32_t axis = 0; axis < 3; ++axis) {
                const float part_value =
                    geometry.positions[vertex][axis] * local.scale[axis] +
                    local.translation[axis];
                const float value =
                    part_value * root.scale[axis] + root.translation[axis];
                write_f32(destination + axis * 4, value);
            }
        }
    }
    return true;
}

playable::Vec3 subtract(playable::Vec3 a, playable::Vec3 b) {
    return {a.x - b.x, a.y - b.y, a.z - b.z};
}

playable::Vec3 add(playable::Vec3 a, playable::Vec3 b) {
    return {a.x + b.x, a.y + b.y, a.z + b.z};
}

playable::Vec3 multiply(playable::Vec3 value, float scale) {
    return {value.x * scale, value.y * scale, value.z * scale};
}

float dot(playable::Vec3 a, playable::Vec3 b) {
    return a.x * b.x + a.y * b.y + a.z * b.z;
}

playable::Vec3 cross(playable::Vec3 a, playable::Vec3 b) {
    return {
        a.y * b.z - a.z * b.y,
        a.z * b.x - a.x * b.z,
        a.x * b.y - a.y * b.x};
}

playable::Vec3 normalized(playable::Vec3 value) {
    const float length = std::sqrt(dot(value, value));
    if (!std::isfinite(length) || length < 0.00001f) {
        return {0.0f, 0.0f, 1.0f};
    }
    return multiply(value, 1.0f / length);
}

playable::StaticModelRenderView make_title_model(
    const CanonicalStartupPackages& packages,
    const playable::StartupTitleCamera& camera) {
    playable::StaticModelRenderView model = {};
    model.model = packages.title_logo_model.bytes;
    model.model_size = packages.title_logo_model.size;
    model.textures = packages.title_logo_textures.bytes;
    model.texture_size = packages.title_logo_textures.size;

    const playable::Vec3 forward = normalized(
        subtract(camera.center, camera.eye));
    const playable::Vec3 right = normalized(cross(forward, camera.up));
    const playable::Vec3 up = normalized(cross(right, forward));
    const playable::Vec3 position =
        add(camera.eye, multiply(forward, 3000.0f));
    model.matrix[0] = right.x;
    model.matrix[1] = up.x;
    model.matrix[2] = -forward.x;
    model.matrix[3] = position.x;
    model.matrix[4] = right.y;
    model.matrix[5] = up.y;
    model.matrix[6] = -forward.y;
    model.matrix[7] = position.y;
    model.matrix[8] = right.z;
    model.matrix[9] = up.z;
    model.matrix[10] = -forward.z;
    model.matrix[11] = position.z;
    model.scale[0] = 1.0f;
    model.scale[1] = 1.0f;
    model.scale[2] = 1.0f;
    return model;
}

bool initialize_logo_renderer(
    const CanonicalStartupPackages& packages,
    playable::RenderMetrics* metrics) {
    return playable::initialize_startup_ui_renderer(
        packages.logos_ui.view,
        g_startup_command_list, sizeof(g_startup_command_list), metrics);
}

bool initialize_title_renderer(
    const CanonicalStartupPackages& packages,
    playable::RenderMetrics* metrics) {
    return playable::initialize_startup_title_renderer(
        packages.title_ui.view,
        playable_view(packages.title_room_textures.view),
        packages.title_room_model.bytes,
        packages.title_room_model.size,
        g_startup_command_list, sizeof(g_startup_command_list), metrics);
}

bool render_logo_segment(
    startup::Segment segment,
    std::uint8_t alpha,
    playable::RenderMetrics* metrics) {
    std::uint16_t channel = 0;
    switch (segment) {
    case startup::Segment::BootWarning:
    case startup::Segment::ProgressivePrompt:
        channel = 0;
        break;
    case startup::Segment::NintendoLogo:
        channel = 1;
        break;
    case startup::Segment::DolbyLogo:
        channel = 2;
        break;
    default:
        return false;
    }
    return playable::render_startup_ui_frame(channel, alpha, metrics);
}

}  // namespace

int run_canonical_startup_then_game() {
    const RuntimeConfig config = {"Dusklight PSP", "."};
    if (!initialize(config)) {
        return 20;
    }
    sceCtrlSetSamplingCycle(0);
    sceCtrlSetSamplingMode(PSP_CTRL_MODE_ANALOG);

    CanonicalStartupPackages packages = {};
    const CanonicalStartupLoadError load_error =
        load_canonical_startup_packages(&packages);
    if (load_error != CanonicalStartupLoadError::Ok) {
        log_error(
            canonical_startup_load_error_name(load_error),
            -30 - static_cast<int>(load_error));
        shutdown();
        return 21;
    }

    TitleLogoGeometry title_geometry = {};
    if (!initialize_title_logo_geometry(packages, &title_geometry)) {
        unload_canonical_startup_packages(&packages);
        shutdown();
        return 22;
    }

    startup::StartupRuntime startup_runtime;
    if (!startup_runtime.initialize(
            packages.sequence.view, kStartupCapabilities)) {
        unload_canonical_startup_packages(&packages);
        shutdown();
        return 23;
    }

    playable::RenderMetrics render_metrics = {};
    if (!initialize_logo_renderer(packages, &render_metrics)) {
        unload_canonical_startup_packages(&packages);
        shutdown();
        return 24;
    }

    animation::PspBckPlayer title_animation;
    if (!title_animation.initialize(
            packages.title_logo_animation.view, kTitleLogoClip,
            animation::LoopMode::Once, 0.5f, 0.0f, -1.0f)) {
        playable::shutdown_renderer();
        unload_canonical_startup_packages(&packages);
        shutdown();
        return 25;
    }

    startup::Segment previous_segment = startup_runtime.current_segment();
    std::uint32_t opening_frame = 0;
    std::uint32_t title_frame = 0;
    std::uint32_t previous_buttons = 0;
    bool reached_title_prompt = false;

    for (;;) {
        SceCtrlData pad = {};
        sceCtrlPeekBufferPositive(&pad, 1);
        if ((pad.Buttons & PSP_CTRL_HOME) != 0) {
            playable::shutdown_renderer();
            unload_canonical_startup_packages(&packages);
            shutdown();
            return 0;
        }
        const std::uint32_t pressed = pad.Buttons & ~previous_buttons;
        previous_buttons = pad.Buttons;
        const bool start_pressed = (pressed & PSP_CTRL_START) != 0;
        const bool cross_pressed = (pressed & PSP_CTRL_CROSS) != 0;

        const startup::Segment segment = startup_runtime.current_segment();
        if (segment != previous_segment) {
            if (segment == startup::Segment::OpeningLoad) {
                playable::shutdown_renderer();
                if (!initialize_title_renderer(packages, &render_metrics)) {
                    unload_canonical_startup_packages(&packages);
                    shutdown();
                    return 26;
                }
            }
            if (segment == startup::Segment::OpeningRealtime) {
                opening_frame = 0;
            }
            if (segment == startup::Segment::TitleLogo) {
                title_frame = 0;
                title_animation.set_frame(0.0f);
                title_animation.set_speed(0.5f);
            }
            previous_segment = segment;
        }

        startup::SegmentRecord record = {};
        if (!record_for_segment(packages.sequence.view, segment, &record)) {
            playable::shutdown_renderer();
            unload_canonical_startup_packages(&packages);
            shutdown();
            return 27;
        }

        const std::uint32_t segment_frame =
            startup_runtime.metrics().segment_frame;
        bool render_ok = true;
        if (segment == startup::Segment::BootWarning ||
            segment == startup::Segment::NintendoLogo ||
            segment == startup::Segment::DolbyLogo ||
            segment == startup::Segment::ProgressivePrompt) {
            render_ok = render_logo_segment(
                segment,
                record.duration_frames != 0
                    ? timed_fade_alpha(record, segment_frame)
                    : 255,
                &render_metrics);
        } else if (segment == startup::Segment::OpeningLoad) {
            render_ok = playable::render_black_transition_frame(&render_metrics);
        } else if (segment == startup::Segment::OpeningRealtime) {
            const playable::StartupTitleCamera camera =
                playable::startup_title_camera_from_source(opening_frame);
            const playable::StaticModelRenderView title_model =
                make_title_model(packages, camera);
            render_ok = playable::render_startup_title_frame(
                title_model, camera, false,
                kNoUiChannel, 255, &render_metrics);
            ++opening_frame;
        } else if (segment == startup::Segment::TitleLogo ||
                   segment == startup::Segment::TitlePrompt) {
            if (!apply_title_logo_animation(
                    &packages, title_geometry, &title_animation)) {
                render_ok = false;
            } else {
                const playable::StartupTitleCamera camera =
                    playable::startup_title_camera_from_source(kOpeningFrames);
                const playable::StaticModelRenderView title_model =
                    make_title_model(packages, camera);
                render_ok = playable::render_startup_title_frame(
                    title_model, camera, true,
                    segment == startup::Segment::TitlePrompt
                        ? kTitlePromptChannel
                        : kNoUiChannel,
                    segment == startup::Segment::TitlePrompt
                        ? title_prompt_alpha(segment_frame)
                        : 255,
                    &render_metrics);
            }
            if (segment == startup::Segment::TitleLogo) {
                title_animation.play();
                ++title_frame;
            } else {
                reached_title_prompt = true;
            }
        } else if (segment == startup::Segment::FileSelect) {
            break;
        }

        if (!render_ok) {
            playable::shutdown_renderer();
            unload_canonical_startup_packages(&packages);
            shutdown();
            return 28;
        }

        startup::Input input = {};
        if (segment == startup::Segment::TitlePrompt) {
            input.start = start_pressed;
        } else {
            input.start = start_pressed;
            input.confirm = cross_pressed;
        }
        const bool resources_ready = segment == startup::Segment::OpeningLoad;
        const bool source_event_complete =
            (segment == startup::Segment::OpeningRealtime &&
             opening_frame >= kOpeningFrames) ||
            (segment == startup::Segment::TitleLogo &&
             title_frame >= kTitleLogoFrames);
        if (!startup_runtime.tick(
                input, resources_ready, source_event_complete)) {
            playable::shutdown_renderer();
            unload_canonical_startup_packages(&packages);
            shutdown();
            return 29;
        }

        sleep_microseconds(kStartupFrameDelayMicroseconds);
    }

    if (!reached_title_prompt) {
        playable::shutdown_renderer();
        unload_canonical_startup_packages(&packages);
        shutdown();
        return 30;
    }

    log(
        "DUSKLIGHT_PSP_STARTUP_TITLE_OK "
        "logos=source opening=F_SP102/1800 "
        "title_logo=DPAN7/300 prompt=source_dpsu start_gate=1");
    playable::shutdown_renderer();
    unload_canonical_startup_packages(&packages);
    shutdown();

    return run_canonical_game();
}

}  // namespace dusk::psp::game
