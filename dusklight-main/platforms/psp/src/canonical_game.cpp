#include "dusk/psp/canonical_game.hpp"

#include "dusk/psp/actor_runtime.hpp"
#include "dusk/psp/canonical_assets.hpp"
#include "dusk/psp/canonical_common_loader.hpp"
#include "dusk/psp/canonical_room_loader.hpp"
#include "dusk/psp/first_playable_controls.hpp"
#include "dusk/psp/platform.hpp"
#include "dusk/psp/playable_render.hpp"
#include "dusk/psp/playable_runtime.hpp"
#include "dusk/psp/psp_controls.hpp"
#include "dusk/psp/real_room_runtime.hpp"
#include "dusk/psp/room_package.hpp"
#include "dusk/psp/startup_save_flow.hpp"

#include <pspctrl.h>
#include <pspdebug.h>

#include <array>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstring>

namespace dusk::psp::game {
namespace {

constexpr std::uint32_t kStartupSegmentCount = 7;
constexpr std::uint32_t kStartupFixtureBytes =
    startup::kPackageHeaderBytes +
    kStartupSegmentCount * startup::kSegmentRecordBytes;
using StartupFixture = std::array<std::uint8_t, kStartupFixtureBytes>;

constexpr std::uint32_t kStartupCapabilities =
    startup::Capability::Ui | startup::Capability::Stage |
    startup::Capability::Events | startup::Capability::TitleModel |
    startup::Capability::FileSelection | startup::Capability::Gameplay;
constexpr std::uint32_t kExpectedSourceActors = 599;
constexpr std::uint32_t kExpectedEssentialActors = 9;
constexpr std::uint32_t kFrameDelayMicroseconds = 33333;
constexpr std::uint32_t kCommandListBytes = 256 * 1024;
constexpr std::uint32_t kAutomationCaptureBytes = 512 * 272 * 2;
alignas(16) std::uint8_t g_command_list[kCommandListBytes] = {};
alignas(16) std::uint8_t g_automation_capture[kAutomationCaptureBytes] = {};

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

bool write_automation_playable_proof() {
    char frame_path[256] = {};
    char marker_path[256] = {};
    constexpr char kMarker[] =
        "DUSKLIGHT_PSP_STARTUP_SAVE_GAMEPLAY_OK\n"
        "route=intro,start,file_select,new_game,F_SP108\n"
        "framebuffer=PLAYABLE.5650\n";
    return make_game_path("PLAYABLE.5650", frame_path, sizeof(frame_path)) &&
        make_game_path("PLAYABLE.OK", marker_path, sizeof(marker_path)) &&
        playable::capture_playable_frame_5650(
            g_automation_capture, sizeof(g_automation_capture)) &&
        write_file(
            frame_path, g_automation_capture,
            static_cast<std::uint32_t>(sizeof(g_automation_capture))) &&
        write_file(marker_path, kMarker, sizeof(kMarker) - 1);
}

void write_u16(std::uint8_t* output, std::uint16_t value) {
    output[0] = static_cast<std::uint8_t>(value);
    output[1] = static_cast<std::uint8_t>(value >> 8);
}

void write_u32(std::uint8_t* output, std::uint32_t value) {
    output[0] = static_cast<std::uint8_t>(value);
    output[1] = static_cast<std::uint8_t>(value >> 8);
    output[2] = static_cast<std::uint8_t>(value >> 16);
    output[3] = static_cast<std::uint8_t>(value >> 24);
}

void set_startup_record(
    StartupFixture& fixture,
    std::uint32_t index,
    startup::Segment segment,
    startup::AdvancePolicy policy,
    startup::Completeness completeness,
    std::uint32_t capabilities,
    std::uint32_t token) {
    std::uint8_t* record =
        fixture.data() + startup::kPackageHeaderBytes +
        index * startup::kSegmentRecordBytes;
    record[0] = static_cast<std::uint8_t>(segment);
    record[1] = static_cast<std::uint8_t>(policy);
    record[2] = static_cast<std::uint8_t>(completeness);
    write_u32(record + 4, 1);
    write_u32(record + 8, 1);
    write_u32(record + 12, 1);
    write_u32(record + 16, capabilities);
    write_u32(record + 20, token);
}

StartupFixture make_public_startup_fixture() {
    StartupFixture fixture = {};
    std::memcpy(fixture.data(), "DPST", 4);
    write_u16(fixture.data() + 4, 1);
    write_u16(fixture.data() + 6, startup::kPackageHeaderBytes);
    write_u32(fixture.data() + 8, fixture.size());
    write_u32(fixture.data() + 12, kStartupSegmentCount);
    write_u32(fixture.data() + 16, startup::kPackageHeaderBytes);
    write_u32(fixture.data() + 20, startup::kSegmentRecordBytes);

    set_startup_record(
        fixture, 0, startup::Segment::BootWarning,
        startup::AdvancePolicy::TimedOrInput, startup::Completeness::Complete,
        startup::Capability::Ui, 0x4C4F474Fu);
    set_startup_record(
        fixture, 1, startup::Segment::OpeningLoad,
        startup::AdvancePolicy::ResourceReady, startup::Completeness::Complete,
        startup::Capability::Stage, 0x46533130u);
    set_startup_record(
        fixture, 2, startup::Segment::OpeningRealtime,
        startup::AdvancePolicy::SourceEvent, startup::Completeness::Complete,
        startup::Capability::Stage | startup::Capability::Events, 0x4F50454Eu);
    set_startup_record(
        fixture, 3, startup::Segment::TitlePrompt,
        startup::AdvancePolicy::InputRequired, startup::Completeness::Complete,
        startup::Capability::Ui | startup::Capability::TitleModel, 0x5449544Cu);
    set_startup_record(
        fixture, 4, startup::Segment::FileSelect,
        startup::AdvancePolicy::InputRequired, startup::Completeness::Complete,
        startup::Capability::Ui | startup::Capability::FileSelection, 0x46494C45u);
    set_startup_record(
        fixture, 5, startup::Segment::NewGameTransition,
        startup::AdvancePolicy::SourceEvent, startup::Completeness::Complete,
        startup::Capability::Stage | startup::Capability::Events, 0x4E455747u);
    set_startup_record(
        fixture, 6, startup::Segment::UnsupportedGameplay,
        startup::AdvancePolicy::UnsupportedBoundary,
        startup::Completeness::Unsupported,
        startup::Capability::Gameplay, 0x46533130u);
    write_u32(
        fixture.data() + startup::kPackageCrcOffset,
        startup::startup_crc32(fixture.data(), fixture.size()));
    return fixture;
}

bool drive_public_startup_to_file_select(startup::StartupSaveFlow* flow) {
    if (flow == nullptr) {
        return false;
    }
    return flow->tick({false, false, true}) &&
           flow->tick({false, false, false, false, true}) &&
           flow->tick({false, false, false, false, false, true}) &&
           flow->tick({false, false, true}) &&
           flow->phase() == startup::SaveFlowPhase::FileSelect;
}

void draw_file_select(
    const startup::StartupSaveFlow& flow,
    const char* status) {
    pspDebugScreenSetBackColor(0x00000000);
    pspDebugScreenSetTextColor(0x00FFFFFF);
    pspDebugScreenClear();
    pspDebugScreenSetXY(5, 2);
    pspDebugScreenPrintf("D U S K L I G H T   P S P");
    pspDebugScreenSetXY(5, 4);
    pspDebugScreenPrintf("PUBLIC STARTUP ROUTE -> CANONICAL GAMEPLAY");
    for (std::size_t index = 0; index < save::kSlotCount; ++index) {
        const save::Slot& slot = flow.bank().slot(index);
        pspDebugScreenSetXY(6, static_cast<int>(8 + index * 3));
        pspDebugScreenPrintf(
            "%c FILE %u   ", flow.selected_slot() == index ? '>' : ' ',
            static_cast<unsigned>(index + 1));
        if (!slot.occupied) {
            pspDebugScreenPrintf("NEW GAME");
        } else {
            pspDebugScreenPrintf(
                "CONTINUE  %-7s R%02d S%02u",
                slot.start.stage.data(), static_cast<int>(slot.start.room),
                static_cast<unsigned>(slot.start.start_point));
        }
    }
    pspDebugScreenSetXY(5, 19);
    pspDebugScreenPrintf("UP/DOWN FILE    X/START CONFIRM");
    pspDebugScreenSetXY(5, 21);
    pspDebugScreenPrintf("%-55s", status != nullptr ? status : "READY");
    pspDebugScreenSetXY(5, 27);
    pspDebugScreenPrintf("Startup presentation is synthetic/public in this build");
}

void draw_room_error(
    const save::StartContext& start,
    const char* category,
    const char* detail) {
    pspDebugScreenInit();
    pspDebugScreenSetBackColor(0x00000000);
    pspDebugScreenSetTextColor(0x00FFFFFF);
    pspDebugScreenClear();
    pspDebugScreenSetXY(4, 3);
    pspDebugScreenPrintf("CANONICAL GAMEPLAY HANDOFF FAILED");
    pspDebugScreenSetXY(4, 6);
    pspDebugScreenPrintf(
        "%-7s R%02d START %02u LAYER %u",
        start.stage.data(), static_cast<int>(start.room),
        static_cast<unsigned>(start.start_point),
        static_cast<unsigned>(start.layer));
    pspDebugScreenSetXY(4, 9);
    pspDebugScreenPrintf("%s: %s", category, detail);
    pspDebugScreenSetXY(4, 12);
    pspDebugScreenPrintf("Local derived assets are required for real gameplay");
    pspDebugScreenSetXY(4, 15);
    pspDebugScreenPrintf("No gameplay parity is claimed from this failure state");
}

playable::RealRoomRenderInput make_render_input(
    const room::RealRoomRuntime& runtime,
    const playable::Runtime& link_runtime) {
    playable::RealRoomRenderInput output = {};
    output.link_position = {
        runtime.state.position.x,
        runtime.state.position.y,
        runtime.state.position.z};
    output.link_yaw = runtime.state.yaw;
    output.camera_eye = {
        runtime.state.camera_eye.x,
        runtime.state.camera_eye.y,
        runtime.state.camera_eye.z};
    output.camera_center = {
        runtime.state.camera_center.x,
        runtime.state.camera_center.y,
        runtime.state.camera_center.z};
    output.camera_fov = 0.0f;
    for (std::uint32_t index = 0; index < 5; ++index) {
        output.rubies[index] = {
            runtime.state.rubies[index].x,
            runtime.state.rubies[index].y,
            runtime.state.rubies[index].z};
        output.ruby_active[index] = runtime.state.ruby_active[index];
    }
    output.interaction = {
        runtime.state.interaction.x,
        runtime.state.interaction.y,
        runtime.state.interaction.z};
    output.presentation = presentation::Profile::OpaqueOnly;
    output.ui_state.mode =
        runtime.state.mode == room::LoadState::Paused
            ? playable::GameMode::Paused
            : playable::GameMode::Playing;
    output.ui_state.locomotion = runtime.state.locomotion;
    output.ui_state.position = output.link_position;
    output.ui_state.yaw = runtime.state.yaw;
    output.ui_state.camera_yaw = runtime.state.camera_yaw;
    output.ui_state.camera_distance = runtime.state.camera_distance;
    output.ui_state.rupees = runtime.state.rupees;
    output.ui_state.collected = runtime.state.total_collected;
    output.ui_state.hearts = 3;
    output.ui_state.pause_selection = runtime.state.pause_selection;
    output.ui_state.action_prompt = runtime.state.action_prompt;
    output.ui_state.debug_visible = runtime.state.debug_visible;
    output.root_pose = link_runtime.root_pose.metrics;
    output.environment = nullptr;
    output.shadows = nullptr;
    output.render_profile = playable::RenderProfile::KnownGoodUnlit;
    output.lighting_mode = playable::LightingMode::Off;
    output.fog_mode = playable::FogMode::Off;
    output.shadow_mode = playable::ShadowMode::Off;
    return output;
}

bool initialize_first_playable(
    const save::StartContext& start,
    CanonicalRoomPackages* room_packages,
    CanonicalCommonPackages* common_packages,
    room::RealRoomRuntime* runtime,
    actor::ActorSystem* actors,
    playable::Runtime* link_runtime,
    playable::RenderMetrics* render_metrics,
    const char** error_category,
    const char** error_detail) {
    if (room_packages == nullptr || common_packages == nullptr ||
        runtime == nullptr || actors == nullptr || link_runtime == nullptr ||
        render_metrics == nullptr || error_category == nullptr ||
        error_detail == nullptr) {
        return false;
    }

    CanonicalRoomAssets room_assets = {};
    CanonicalCommonAssets common_assets = {};
    const CanonicalAssetError room_asset_error =
        resolve_canonical_room_assets(start, &room_assets);
    const CanonicalAssetError common_asset_error =
        resolve_canonical_common_assets(&common_assets);
    if (room_asset_error != CanonicalAssetError::Ok ||
        common_asset_error != CanonicalAssetError::Ok) {
        *error_category = "asset contract";
        *error_detail = canonical_asset_error_name(
            room_asset_error != CanonicalAssetError::Ok
                ? room_asset_error
                : common_asset_error);
        return false;
    }

    const CanonicalRoomLoadError room_load_error =
        load_canonical_room_packages(room_assets, room_packages);
    if (room_load_error != CanonicalRoomLoadError::Ok) {
        *error_category = "room packages";
        *error_detail = canonical_room_load_error_name(room_load_error);
        return false;
    }

    const CanonicalCommonLoadError common_load_error =
        load_canonical_common_packages(common_assets, common_packages);
    if (common_load_error != CanonicalCommonLoadError::Ok) {
        *error_category = "Link/HUD packages";
        *error_detail = canonical_common_load_error_name(common_load_error);
        unload_canonical_room_packages(room_packages);
        return false;
    }

    if (room::read_u32(room_packages->scene.view.bytes + 136) !=
        kExpectedSourceActors) {
        *error_category = "scene contract";
        *error_detail = "unexpected source actor count";
        unload_canonical_common_packages(common_packages);
        unload_canonical_room_packages(room_packages);
        return false;
    }

    room::SceneSpawnV3 spawn = {};
    if (room::find_dpsc_spawn_v3(
            room_packages->scene.view, start.start_point, &spawn) !=
        room::PackageError::Ok) {
        *error_category = "scene contract";
        *error_detail = "start point missing";
        unload_canonical_common_packages(common_packages);
        unload_canonical_room_packages(room_packages);
        return false;
    }

    if (!room::initialize_real_room_runtime(
            runtime, room_packages->model.view, room_packages->textures.view,
            room_packages->collision.view, room_packages->scene.view) ||
        !room::spawn_real_room(runtime, start.start_point) ||
        !room::real_room_state_consistent(*runtime)) {
        *error_category = "real room runtime";
        *error_detail = "initialization or spawn failed";
        unload_canonical_common_packages(common_packages);
        unload_canonical_room_packages(room_packages);
        return false;
    }

    if (actor::initialize_actor_system(actors, room_packages->scene.view) !=
            actor::Error::Ok ||
        actors->essential_source_actor_count != kExpectedEssentialActors ||
        actors->active_count != kExpectedEssentialActors ||
        actors->create_calls != kExpectedEssentialActors ||
        !actor::actor_system_consistent(*actors)) {
        *error_category = "actor runtime";
        *error_detail = "first-playable actor contract failed";
        actor::destroy_actor_system(actors);
        unload_canonical_common_packages(common_packages);
        unload_canonical_room_packages(room_packages);
        return false;
    }

    if (!playable::initialize_runtime(
            link_runtime,
            canonical_playable_package_set(*common_packages))) {
        *error_category = "Link runtime";
        *error_detail = "animation/skin initialization failed";
        actor::destroy_actor_system(actors);
        unload_canonical_common_packages(common_packages);
        unload_canonical_room_packages(room_packages);
        return false;
    }

    const playable::PackageView room_texture_render_view = {
        room_packages->textures.view.bytes,
        room_packages->textures.view.size,
        room_packages->textures.view.expected_crc,
        room_packages->textures.view.actual_crc,
    };
    if (!playable::initialize_real_room_renderer(
            common_packages->link_textures.view,
            room_texture_render_view,
            common_packages->hud_ui.view,
            room_packages->model.bytes,
            room_packages->model.size,
            g_command_list, sizeof(g_command_list), render_metrics)) {
        *error_category = "renderer";
        *error_detail = "minimal room renderer initialization failed";
        actor::destroy_actor_system(actors);
        unload_canonical_common_packages(common_packages);
        unload_canonical_room_packages(room_packages);
        return false;
    }

    return true;
}

}  // namespace

int run_canonical_game() {
    const RuntimeConfig config = {"Dusklight PSP", "."};
    if (!initialize(config)) {
        return 2;
    }

    sceCtrlSetSamplingCycle(0);
    sceCtrlSetSamplingMode(PSP_CTRL_MODE_ANALOG);

    char save_path[256] = {};
    if (!make_game_path("dusklight-save.bin", save_path, sizeof(save_path))) {
        log_error(last_error_message(), last_error());
        shutdown();
        return 3;
    }

    StartupFixture fixture = make_public_startup_fixture();
    startup::PackageView startup_package = {};
    if (startup::validate_startup_package(
            fixture.data(), fixture.size(), &startup_package) !=
        startup::PackageError::Ok) {
        log_error("public startup package validation failed", -20);
        shutdown();
        return 4;
    }

    startup::StartupSaveFlow flow;
    if (!flow.initialize(startup_package, kStartupCapabilities, save_path) ||
        !drive_public_startup_to_file_select(&flow)) {
        log_error("startup/save route failed", -21);
        shutdown();
        return 5;
    }

    static CanonicalRoomPackages room_packages = {};
    static CanonicalCommonPackages common_packages = {};
    static room::RealRoomRuntime runtime = {};
    static actor::ActorSystem actors = {};
    static playable::Runtime link_runtime = {};
    static playable::RenderMetrics render_metrics = {};
    controls::MapperState mapper = {};
    FirstPlayableControlProof control_proof = {};
    bool gameplay_active = false;
    bool actor_system_active = false;
    bool renderer_active = false;
    bool terminal_room_error = false;
    bool first_render_proven = false;
    bool controls_proven = false;
    const bool should_automate_route = automation_requested();
    bool automation_complete = false;
    save::StartContext handoff = {};

    draw_file_select(
        flow,
        flow.last_storage_result() == save::StorageResult::Ok
            ? "SAVE BANK LOADED - SELECT FILE"
            : "SELECT FILE");

    for (;;) {
        SceCtrlData pad = {};
        sceCtrlPeekBufferPositive(&pad, 1);
        if ((pad.Buttons & PSP_CTRL_HOME) != 0) {
            break;
        }

        controls::PadSample sample = {};
        sample.buttons = pad.Buttons;
        sample.analog_x = pad.Lx;
        sample.analog_y = pad.Ly;
        const playable::Input input =
            controls::map_gameplay_input(sample, &mapper);

        if (!gameplay_active && !terminal_room_error &&
            flow.phase() == startup::SaveFlowPhase::FileSelect &&
            (should_automate_route || input.up_pressed || input.down_pressed ||
             input.action_pressed || input.pause_pressed)) {
            startup::SaveFlowInput flow_input = {};
            flow_input.up = input.up_pressed;
            flow_input.down = input.down_pressed;
            flow_input.confirm = input.action_pressed || should_automate_route;
            flow_input.start = input.pause_pressed;
            if (!flow.tick(flow_input)) {
                draw_file_select(flow, "SAVE FLOW ERROR");
            } else {
                draw_file_select(
                    flow,
                    flow.phase() == startup::SaveFlowPhase::Transition
                        ? "TRANSITION TO GAMEPLAY"
                        : "SELECT FILE");
            }
        }

        if (!gameplay_active && !terminal_room_error &&
            flow.phase() == startup::SaveFlowPhase::Transition) {
            if (!flow.tick({false, false, false, false, false, true})) {
                draw_file_select(flow, "NEW GAME TRANSITION ERROR");
                terminal_room_error = true;
            } else if (flow.phase() == startup::SaveFlowPhase::HandoffReady &&
                       flow.consume_handoff(&handoff)) {
                const char* error_category = "canonical gameplay";
                const char* error_detail = "unknown failure";
                if (!initialize_first_playable(
                        handoff, &room_packages, &common_packages,
                        &runtime, &actors, &link_runtime, &render_metrics,
                        &error_category, &error_detail)) {
                    draw_room_error(handoff, error_category, error_detail);
                    terminal_room_error = true;
                } else {
                    gameplay_active = true;
                    actor_system_active = true;
                    renderer_active = true;
                }
            }
        }

        if (gameplay_active) {
            room::set_link_animation_motion(
                &runtime,
                playable::source_foot_motion_raw(link_runtime),
                playable::source_old_frame_rate_next(link_runtime));
            const room::RealRoomState state_before_input = runtime.state;
            room::update_real_room(&runtime, input, 1.0f / 30.0f);
            observe_first_playable_controls(
                &control_proof, input, state_before_input, runtime.state);

            actor::Context context = {};
            context.player_position = runtime.state.position;
            context.player_heavy_boots = false;
            context.paused = runtime.state.mode == room::LoadState::Paused;
            actor::Interaction interaction = {};

            const bool room_ok = room::real_room_state_consistent(runtime);
            const bool actors_ok =
                actor::update_actor_system(&actors, context, &interaction) ==
                    actor::Error::Ok &&
                actor::actor_system_consistent(actors);
            const bool link_ok = playable::update_source_animation_and_skin(
                &link_runtime,
                runtime.state.locomotion,
                runtime.state.normal_speed,
                1.0f / 30.0f);

            if (!room_ok || !actors_ok || !link_ok) {
                if (renderer_active) {
                    playable::shutdown_renderer();
                    renderer_active = false;
                }
                draw_room_error(
                    handoff, "runtime update", "gameplay state became invalid");
                gameplay_active = false;
                terminal_room_error = true;
            } else {
                const playable::RealRoomRenderInput render_input =
                    make_render_input(runtime, link_runtime);
                if (!playable::render_real_room_frame(
                        link_runtime, render_input, &render_metrics)) {
                    playable::shutdown_renderer();
                    renderer_active = false;
                    draw_room_error(
                        handoff, "renderer", "frame submission failed");
                    gameplay_active = false;
                    terminal_room_error = true;
                } else if (!first_render_proven) {
                    log(
                        "DUSKLIGHT_PSP_FIRST_PLAYABLE_RENDER_OK "
                        "stage=F_SP108 room=1 start=21 actors=9 "
                        "render=opaque_unlit frame=1");
                    first_render_proven = true;
                    arm_first_playable_control_proof(
                        &control_proof, runtime.state);
                    if (should_automate_route) {
                        if (!write_automation_playable_proof()) {
                            if (renderer_active) {
                                playable::shutdown_renderer();
                                renderer_active = false;
                            }
                            draw_room_error(
                                handoff, "automation capture",
                                "frame or marker write failed");
                            gameplay_active = false;
                            terminal_room_error = true;
                        } else {
                            automation_complete = true;
                        }
                    }
                } else if (!controls_proven &&
                           first_playable_controls_complete(control_proof)) {
                    log(
                        "DUSKLIGHT_PSP_FIRST_PLAYABLE_CONTROLS_OK "
                        "stage=F_SP108 room=1 start=21 "
                        "movement=1 camera=1 action=1 pause=1 resume=1");
                    controls_proven = true;
                }
            }
        }

        if (automation_complete) {
            break;
        }

        sleep_microseconds(kFrameDelayMicroseconds);
    }

    if (renderer_active) {
        playable::shutdown_renderer();
    }
    if (actor_system_active) {
        actor::destroy_actor_system(&actors);
    }
    unload_canonical_common_packages(&common_packages);
    unload_canonical_room_packages(&room_packages);
    shutdown();
    return terminal_room_error ? 6 : 0;
}

}  // namespace dusk::psp::game
