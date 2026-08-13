#include "dusk/psp/canonical_game.hpp"

#include "dusk/psp/actor_runtime.hpp"
#include "dusk/psp/canonical_assets.hpp"
#include "dusk/psp/canonical_room_loader.hpp"
#include "dusk/psp/platform.hpp"
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

void draw_gameplay(
    const room::RealRoomRuntime& runtime,
    const actor::ActorSystem& actors) {
    pspDebugScreenSetBackColor(0x00000000);
    pspDebugScreenSetTextColor(0x00FFFFFF);
    pspDebugScreenClear();
    pspDebugScreenSetXY(4, 2);
    pspDebugScreenPrintf("DUSKLIGHT PSP - ASSET-BACKED ROOM ACTIVE");
    pspDebugScreenSetXY(4, 5);
    pspDebugScreenPrintf("F_SP108  ROOM 01  START 21");
    pspDebugScreenSetXY(4, 8);
    pspDebugScreenPrintf(
        "POS X:%d Y:%d Z:%d",
        static_cast<int>(runtime.state.position.x),
        static_cast<int>(runtime.state.position.y),
        static_cast<int>(runtime.state.position.z));
    pspDebugScreenSetXY(4, 10);
    pspDebugScreenPrintf(
        "UPDATES:%u  ACTORS:%u  SOURCE ESSENTIAL:%u",
        static_cast<unsigned>(runtime.state.updates),
        static_cast<unsigned>(actors.active_count),
        static_cast<unsigned>(actors.essential_source_actor_count));
    pspDebugScreenSetXY(4, 13);
    pspDebugScreenPrintf("STICK MOVE  L/R CAMERA  X ACTION  START PAUSE");
    pspDebugScreenSetXY(4, 15);
    pspDebugScreenPrintf("Rendering remains intentionally minimal/unlit");
}

void draw_room_error(
    const save::StartContext& start,
    const char* category,
    const char* detail) {
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

bool initialize_first_playable(
    const save::StartContext& start,
    CanonicalRoomPackages* packages,
    room::RealRoomRuntime* runtime,
    actor::ActorSystem* actors,
    const char** error_category,
    const char** error_detail) {
    if (packages == nullptr || runtime == nullptr || actors == nullptr ||
        error_category == nullptr || error_detail == nullptr) {
        return false;
    }

    CanonicalRoomAssets assets = {};
    const CanonicalAssetError asset_error =
        resolve_canonical_room_assets(start, &assets);
    if (asset_error != CanonicalAssetError::Ok) {
        *error_category = "asset contract";
        *error_detail = canonical_asset_error_name(asset_error);
        return false;
    }

    const CanonicalRoomLoadError load_error =
        load_canonical_room_packages(assets, packages);
    if (load_error != CanonicalRoomLoadError::Ok) {
        *error_category = "room packages";
        *error_detail = canonical_room_load_error_name(load_error);
        return false;
    }

    if (room::read_u32(packages->scene.view.bytes + 136) !=
        kExpectedSourceActors) {
        *error_category = "scene contract";
        *error_detail = "unexpected source actor count";
        unload_canonical_room_packages(packages);
        return false;
    }

    room::SceneSpawnV3 spawn = {};
    if (room::find_dpsc_spawn_v3(
            packages->scene.view, start.start_point, &spawn) !=
        room::PackageError::Ok) {
        *error_category = "scene contract";
        *error_detail = "start point missing";
        unload_canonical_room_packages(packages);
        return false;
    }

    if (!room::initialize_real_room_runtime(
            runtime, packages->model.view, packages->textures.view,
            packages->collision.view, packages->scene.view) ||
        !room::spawn_real_room(runtime, start.start_point) ||
        !room::real_room_state_consistent(*runtime)) {
        *error_category = "real room runtime";
        *error_detail = "initialization or spawn failed";
        unload_canonical_room_packages(packages);
        return false;
    }

    if (actor::initialize_actor_system(actors, packages->scene.view) !=
            actor::Error::Ok ||
        actors->essential_source_actor_count != kExpectedEssentialActors ||
        actors->active_count != kExpectedEssentialActors ||
        actors->create_calls != kExpectedEssentialActors ||
        !actor::actor_system_consistent(*actors)) {
        *error_category = "actor runtime";
        *error_detail = "first-playable actor contract failed";
        actor::destroy_actor_system(actors);
        unload_canonical_room_packages(packages);
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

    static CanonicalRoomPackages packages = {};
    static room::RealRoomRuntime runtime = {};
    static actor::ActorSystem actors = {};
    controls::MapperState mapper = {};
    bool gameplay_active = false;
    bool actor_system_active = false;
    bool terminal_room_error = false;
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
            (input.up_pressed || input.down_pressed ||
             input.action_pressed || input.pause_pressed)) {
            startup::SaveFlowInput flow_input = {};
            flow_input.up = input.up_pressed;
            flow_input.down = input.down_pressed;
            flow_input.confirm = input.action_pressed;
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
                        handoff, &packages, &runtime, &actors,
                        &error_category, &error_detail)) {
                    draw_room_error(handoff, error_category, error_detail);
                    terminal_room_error = true;
                } else {
                    gameplay_active = true;
                    actor_system_active = true;
                    draw_gameplay(runtime, actors);
                }
            }
        }

        if (gameplay_active) {
            room::update_real_room(&runtime, input, 1.0f / 30.0f);
            actor::Context context = {};
            context.player_position = runtime.state.position;
            context.player_heavy_boots = false;
            context.paused = runtime.state.mode == room::LoadState::Paused;
            actor::Interaction interaction = {};
            if (actor::update_actor_system(&actors, context, &interaction) !=
                    actor::Error::Ok ||
                !room::real_room_state_consistent(runtime) ||
                !actor::actor_system_consistent(actors)) {
                draw_room_error(
                    handoff, "runtime update", "gameplay state became invalid");
                gameplay_active = false;
                terminal_room_error = true;
            } else if ((runtime.state.updates % 3u) == 0u) {
                draw_gameplay(runtime, actors);
            }
        }

        sleep_microseconds(kFrameDelayMicroseconds);
    }

    if (actor_system_active) {
        actor::destroy_actor_system(&actors);
    }
    unload_canonical_room_packages(&packages);
    shutdown();
    return terminal_room_error ? 6 : 0;
}

}  // namespace dusk::psp::game
