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
#include <cstdio>
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
constexpr std::uint32_t kTitleLogoFrames = 300;
constexpr std::uint16_t kNoUiChannel = 0xffffu;
constexpr std::uint16_t kTitlePromptChannel = 8;
constexpr std::uint32_t kTitleLogoClip = 7;
constexpr std::uint32_t kTitleLogoVertices = 24;
constexpr std::uint32_t kTitleLogoSubmeshes = 6;
constexpr std::uint32_t kCommandListBytes = 256 * 1024;
constexpr std::uint32_t kStartupCaptureBytes = 512 * 272 * 2;
alignas(16) std::uint8_t g_startup_command_list[kCommandListBytes] = {};
alignas(16) std::uint8_t g_startup_capture[kStartupCaptureBytes] = {};

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

playable::StaticModelRenderView make_title_model(
    const CanonicalStartupPackages& packages,
    const playable::StartupTitleCamera&) {
    playable::StaticModelRenderView model = {};
    model.model = packages.title_logo_model.bytes;
    model.model_size = packages.title_logo_model.size;
    model.textures = packages.title_logo_textures.bytes;
    model.texture_size = packages.title_logo_textures.size;

    // daTitle_c::Draw uses cMtx_trans(0, 0, -430) and mirrors X.  Rendering
    // switches to the source Item3D view before this model is submitted, so
    // no camera-facing billboard or guessed distance belongs here.
    model.matrix[0] = 1.0f;
    model.matrix[5] = 1.0f;
    model.matrix[10] = 1.0f;
    model.matrix[11] = -430.0f;
    model.scale[0] = -1.0f;
    model.scale[1] = 1.0f;
    model.scale[2] = 1.0f;
    return model;
}

bool capture_requested() {
    constexpr char kRequest[] = "DUSKLIGHT_STARTUP_CAPTURE_V1\n";
    char path[256] = {};
    char payload[sizeof(kRequest)] = {};
    std::uint32_t size = 0;
    return make_game_path("STARTUP.CAPTURE.REQUEST", path, sizeof(path)) &&
        read_file(path, payload, sizeof(payload), &size) &&
        size == sizeof(kRequest) - 1 &&
        std::memcmp(payload, kRequest, sizeof(kRequest) - 1) == 0;
}

bool automation_requested() {
    constexpr char kRequest[] = "DUSKLIGHT_STARTUP_AUTOMATION_V1\n";
    char path[256] = {};
    char payload[sizeof(kRequest)] = {};
    std::uint32_t size = 0;
    return make_game_path("STARTUP.AUTOMATION.REQUEST", path, sizeof(path)) &&
        read_file(path, payload, sizeof(payload), &size) &&
        size == sizeof(kRequest) - 1 &&
        std::memcmp(payload, kRequest, sizeof(kRequest) - 1) == 0;
}

void write_startup_error(const char* stage, int code) {
    char path[256] = {};
    char payload[192] = {};
    if (stage == nullptr ||
        !make_game_path("STARTUP.ERROR", path, sizeof(path))) {
        return;
    }
    const int size = std::snprintf(
        payload, sizeof(payload),
        "format=DUSKLIGHT_STARTUP_ERROR_V1\nstage=%s\nerror_code=%d\n",
        stage, code);
    if (size > 0 && static_cast<std::size_t>(size) < sizeof(payload)) {
        write_file(path, payload, static_cast<std::uint32_t>(size));
    }
}

bool capture_startup_title(const playable::RenderMetrics& metrics) {
    char frame_path[256] = {};
    char metrics_path[256] = {};
    char marker_path[256] = {};
    if (!make_game_path("STARTUP_TITLE.5650", frame_path, sizeof(frame_path)) ||
        !make_game_path(
            "STARTUP_TITLE_CAPTURE.METRICS", metrics_path,
            sizeof(metrics_path)) ||
        !make_game_path("SMOKE.OK", marker_path, sizeof(marker_path)) ||
        !playable::capture_playable_frame_5650(
            g_startup_capture, sizeof(g_startup_capture)) ||
        !write_file(frame_path, g_startup_capture, sizeof(g_startup_capture))) {
        return false;
    }
    char report[512] = {};
    const int report_size = std::snprintf(
        report, sizeof(report),
        "format=DUSKLIGHT_STARTUP_TITLE_CAPTURE_V1\n"
        "width=480\nheight=272\nstride=512\n"
        "pixel_format=GU_PSM_5650\n"
        "title_translation=0,0,-430\n"
        "title_scale=-1,1,1\n"
        "room_material_plan_records=%lu\n"
        "room_material_plan_exact_draws=%lu\n"
        "room_material_plan_approximate_draws=%lu\n"
        "room_material_plan_unsupported_draws=%lu\n"
        "room_material_pass_draws=%lu\n",
        static_cast<unsigned long>(metrics.room_material_plan_records),
        static_cast<unsigned long>(
            metrics.room_material_plan_exact_draws),
        static_cast<unsigned long>(
            metrics.room_material_plan_approximate_draws),
        static_cast<unsigned long>(
            metrics.room_material_plan_unsupported_draws),
        static_cast<unsigned long>(metrics.room_material_pass_draws));
    constexpr char kMarker[] = "DUSKLIGHT_PSP_STARTUP_CAPTURE_OK";
    return report_size > 0 &&
        static_cast<std::size_t>(report_size) < sizeof(report) &&
        write_file(
            metrics_path, report, static_cast<std::uint32_t>(report_size)) &&
        write_file(marker_path, kMarker, sizeof(kMarker) - 1);
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
        channel = 0;
        break;
    case startup::Segment::ProgressivePrompt:
        channel = 3;
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
        write_startup_error(
            canonical_startup_load_error_name(load_error),
            -30 - static_cast<int>(load_error));
        log_error(
            canonical_startup_load_error_name(load_error),
            -30 - static_cast<int>(load_error));
        shutdown();
        return 21;
    }
    const std::uint32_t opening_display_frames =
        playable::startup_title_camera_display_frames(packages.title_camera.view);
    if (opening_display_frames == 0) {
        write_startup_error("opening_display_frames", 22);
        unload_canonical_startup_packages(&packages);
        shutdown();
        return 22;
    }

    TitleLogoGeometry title_geometry = {};
    if (!initialize_title_logo_geometry(packages, &title_geometry)) {
        write_startup_error("title_logo_geometry", 23);
        unload_canonical_startup_packages(&packages);
        shutdown();
        return 23;
    }

    startup::StartupRuntime startup_runtime;
    if (!startup_runtime.initialize(
            packages.sequence.view, kStartupCapabilities)) {
        write_startup_error("startup_runtime", 24);
        unload_canonical_startup_packages(&packages);
        shutdown();
        return 24;
    }

    playable::RenderMetrics render_metrics = {};
    if (!initialize_logo_renderer(packages, &render_metrics)) {
        write_startup_error("logo_renderer", 25);
        unload_canonical_startup_packages(&packages);
        shutdown();
        return 25;
    }

    animation::PspBckPlayer title_animation;
    if (!title_animation.initialize(
            packages.title_logo_animation.view, kTitleLogoClip,
            animation::LoopMode::Once, 0.5f, 0.0f, -1.0f)) {
        write_startup_error("title_animation", 26);
        playable::shutdown_renderer();
        unload_canonical_startup_packages(&packages);
        shutdown();
        return 26;
    }

    startup::Segment previous_segment = startup_runtime.current_segment();
    std::uint32_t opening_frame = 0;
    std::uint32_t title_frame = 0;
    std::uint32_t previous_buttons = 0;
    bool reached_title_prompt = false;
    const bool should_capture_title = capture_requested();
    const bool should_automate_route = automation_requested();
    bool captured_title = false;

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
                    return 27;
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
            return 28;
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
            playable::StartupTitleCamera camera = {};
            if (!playable::startup_title_camera_from_track(
                    packages.title_camera.view, opening_frame, &camera)) {
                render_ok = false;
            } else {
                const playable::StaticModelRenderView title_model =
                    make_title_model(packages, camera);
                render_ok = playable::render_startup_title_frame(
                    title_model, camera, false,
                    kNoUiChannel, 255, &render_metrics);
            }
            ++opening_frame;
        } else if (segment == startup::Segment::TitleLogo ||
                   segment == startup::Segment::TitlePrompt) {
            if (!apply_title_logo_animation(
                    &packages, title_geometry, &title_animation)) {
                render_ok = false;
            } else {
                playable::StartupTitleCamera camera = {};
                if (!playable::startup_title_camera_from_track(
                        packages.title_camera.view,
                        opening_display_frames, &camera)) {
                    render_ok = false;
                } else {
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
            return 29;
        }
        if (should_capture_title && !captured_title &&
            segment == startup::Segment::TitlePrompt) {
            if (!capture_startup_title(render_metrics)) {
                playable::shutdown_renderer();
                unload_canonical_startup_packages(&packages);
                shutdown();
                return 32;
            }
            captured_title = true;
        }

        startup::Input input = {};
        if (segment == startup::Segment::TitlePrompt) {
            input.start = start_pressed ||
                (should_automate_route &&
                 (!should_capture_title || captured_title));
        } else {
            input.start = start_pressed;
            input.confirm = cross_pressed ||
                (should_automate_route &&
                 (segment == startup::Segment::BootWarning ||
                  segment == startup::Segment::ProgressivePrompt));
        }
        const bool resources_ready = segment == startup::Segment::OpeningLoad;
        const bool source_event_complete =
            (segment == startup::Segment::OpeningRealtime &&
             opening_frame >= opening_display_frames) ||
            (segment == startup::Segment::TitleLogo &&
             title_frame >= kTitleLogoFrames);
        if (!startup_runtime.tick(
                input, resources_ready, source_event_complete)) {
            playable::shutdown_renderer();
            unload_canonical_startup_packages(&packages);
            shutdown();
            return 30;
        }

        sleep_microseconds(kStartupFrameDelayMicroseconds);
    }

    if (!reached_title_prompt) {
        playable::shutdown_renderer();
        unload_canonical_startup_packages(&packages);
        shutdown();
        return 31;
    }

    log(
        "DUSKLIGHT_PSP_STARTUP_TITLE_OK "
        "logos=source opening=F_SP102/DPCM2400@30fps/4800display "
        "title_logo=DPAN7/300 prompt=source_dpsu start_gate=1");
    playable::shutdown_renderer();
    unload_canonical_startup_packages(&packages);
    shutdown();

    return run_canonical_game();
}

}  // namespace dusk::psp::game
