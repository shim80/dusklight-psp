#include "dusk/psp/psp_controls.hpp"
#include "dusk/psp/startup_save_flow.hpp"

#include <pspctrl.h>
#include <pspdebug.h>
#include <pspkernel.h>

#include <array>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstring>

PSP_MODULE_INFO("DusklightStartupSave", 0, 1, 0);
PSP_MAIN_THREAD_ATTR(THREAD_ATTR_USER);
PSP_HEAP_SIZE_KB(-256);

namespace controls = dusk::psp::controls;
namespace save = dusk::psp::save;
namespace startup = dusk::psp::startup;

namespace {
constexpr char kSavePath[] = "dusklight-save.bin";
constexpr std::uint32_t kSegmentCount = 7;
constexpr std::uint32_t kFixtureBytes =
    startup::kPackageHeaderBytes +
    kSegmentCount * startup::kSegmentRecordBytes;
using Fixture = std::array<std::uint8_t, kFixtureBytes>;

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

void set_record(
    Fixture& fixture,
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

Fixture make_fixture() {
    Fixture fixture = {};
    std::memcpy(fixture.data(), "DPST", 4);
    write_u16(fixture.data() + 4, 1);
    write_u16(fixture.data() + 6, startup::kPackageHeaderBytes);
    write_u32(fixture.data() + 8, fixture.size());
    write_u32(fixture.data() + 12, kSegmentCount);
    write_u32(fixture.data() + 16, startup::kPackageHeaderBytes);
    write_u32(fixture.data() + 20, startup::kSegmentRecordBytes);

    set_record(
        fixture, 0, startup::Segment::BootWarning,
        startup::AdvancePolicy::TimedOrInput, startup::Completeness::Complete,
        startup::Capability::Ui, 0x4C4F474Fu);
    set_record(
        fixture, 1, startup::Segment::OpeningLoad,
        startup::AdvancePolicy::ResourceReady, startup::Completeness::Complete,
        startup::Capability::Stage, 0x46533130u);
    set_record(
        fixture, 2, startup::Segment::OpeningRealtime,
        startup::AdvancePolicy::SourceEvent, startup::Completeness::Complete,
        startup::Capability::Stage | startup::Capability::Events, 0x4F50454Eu);
    set_record(
        fixture, 3, startup::Segment::TitlePrompt,
        startup::AdvancePolicy::InputRequired, startup::Completeness::Complete,
        startup::Capability::Ui | startup::Capability::TitleModel, 0x5449544Cu);
    set_record(
        fixture, 4, startup::Segment::FileSelect,
        startup::AdvancePolicy::InputRequired, startup::Completeness::Complete,
        startup::Capability::Ui | startup::Capability::FileSelection, 0x46494C45u);
    set_record(
        fixture, 5, startup::Segment::NewGameTransition,
        startup::AdvancePolicy::SourceEvent, startup::Completeness::Complete,
        startup::Capability::Stage | startup::Capability::Events, 0x4E455747u);
    set_record(
        fixture, 6, startup::Segment::UnsupportedGameplay,
        startup::AdvancePolicy::UnsupportedBoundary, startup::Completeness::Unsupported,
        startup::Capability::Gameplay, 0x46533130u);
    write_u32(
        fixture.data() + startup::kPackageCrcOffset,
        startup::startup_crc32(fixture.data(), fixture.size()));
    return fixture;
}

const char* action_name(save::FileSelectAction action) {
    switch (action) {
    case save::FileSelectAction::None: return "READY";
    case save::FileSelectAction::NewGame: return "NEW GAME";
    case save::FileSelectAction::Continue: return "CONTINUE";
    }
    return "READY";
}

void draw(
    const startup::StartupSaveFlow& flow,
    const char* status,
    const save::StartContext* handoff) {
    pspDebugScreenSetBackColor(0x00000000);
    pspDebugScreenSetTextColor(0x00FFFFFF);
    pspDebugScreenClear();
    pspDebugScreenSetXY(6, 2);
    pspDebugScreenPrintf("D U S K L I G H T   P S P");
    pspDebugScreenSetXY(5, 4);
    pspDebugScreenPrintf("STARTUP -> SAVE -> GAMEPLAY HANDOFF");
    pspDebugScreenSetXY(5, 6);
    pspDebugScreenPrintf(
        "PHASE: %-12s  SEGMENT: %u",
        startup::save_flow_phase_name(flow.phase()),
        static_cast<unsigned>(flow.startup().current_segment()));

    for (std::size_t i = 0; i < save::kSlotCount; ++i) {
        const save::Slot& slot = flow.bank().slot(i);
        pspDebugScreenSetXY(6, static_cast<int>(9 + i * 3));
        pspDebugScreenPrintf(
            "%c FILE %u   ", flow.selected_slot() == i ? '>' : ' ',
            static_cast<unsigned>(i + 1));
        if (!slot.occupied) {
            pspDebugScreenPrintf("NEW GAME");
        } else {
            pspDebugScreenPrintf(
                "CONTINUE  %-7s R%02d S%02u",
                slot.start.stage.data(),
                static_cast<int>(slot.start.room),
                static_cast<unsigned>(slot.start.start_point));
        }
    }

    pspDebugScreenSetXY(5, 19);
    pspDebugScreenPrintf("STICK MOVE  L/R CAMERA  X ACTION  START PAUSE");
    pspDebugScreenSetXY(5, 21);
    pspDebugScreenPrintf("UP/DOWN SELECT FILE      X/START CONFIRM");
    pspDebugScreenSetXY(5, 23);
    pspDebugScreenPrintf("%-52s", status);
    if (handoff != nullptr) {
        pspDebugScreenSetXY(5, 25);
        pspDebugScreenPrintf(
            "HANDOFF: %-7s R%02d START %02u LAYER %u",
            handoff->stage.data(), static_cast<int>(handoff->room),
            static_cast<unsigned>(handoff->start_point),
            static_cast<unsigned>(handoff->layer));
    }
    pspDebugScreenSetXY(5, 28);
    pspDebugScreenPrintf("Public PSP flow probe - no commercial assets");
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
}

int main() {
    pspDebugScreenInit();
    sceCtrlSetSamplingCycle(0);
    sceCtrlSetSamplingMode(PSP_CTRL_MODE_ANALOG);

    Fixture fixture = make_fixture();
    startup::PackageView package = {};
    if (startup::validate_startup_package(
            fixture.data(), fixture.size(), &package) !=
        startup::PackageError::Ok) {
        pspDebugScreenPrintf("STARTUP PACKAGE ERROR\n");
        sceKernelDelayThread(2000000);
        sceKernelExitGame();
        return 2;
    }

    constexpr std::uint32_t kCapabilities =
        startup::Capability::Ui | startup::Capability::Stage |
        startup::Capability::Events | startup::Capability::TitleModel |
        startup::Capability::FileSelection | startup::Capability::Gameplay;

    startup::StartupSaveFlow flow;
    if (!flow.initialize(package, kCapabilities, kSavePath) ||
        !drive_public_startup_to_file_select(&flow)) {
        pspDebugScreenPrintf("STARTUP SAVE FLOW ERROR\n");
        sceKernelDelayThread(2000000);
        sceKernelExitGame();
        return 3;
    }

    const char* status =
        flow.last_storage_result() == save::StorageResult::Ok
            ? "SAVE BANK LOADED - SELECT QUEST LOG"
            : "STARTUP ROUTE COMPLETE - SELECT QUEST LOG";
    save::StartContext handoff = {};
    bool have_handoff = false;
    controls::MapperState mapper = {};
    draw(flow, status, nullptr);

    for (;;) {
        SceCtrlData pad = {};
        sceCtrlPeekBufferPositive(&pad, 1);
        controls::PadSample sample = {};
        sample.buttons = pad.Buttons;
        sample.analog_x = pad.Lx;
        sample.analog_y = pad.Ly;
        const dusk::psp::playable::Input input =
            controls::map_gameplay_input(sample, &mapper);

        if (flow.phase() == startup::SaveFlowPhase::FileSelect &&
            (input.up_pressed || input.down_pressed ||
             input.action_pressed || input.pause_pressed)) {
            startup::SaveFlowInput flow_input = {};
            flow_input.up = input.up_pressed;
            flow_input.down = input.down_pressed;
            flow_input.confirm = input.action_pressed;
            flow_input.start = input.pause_pressed;
            if (!flow.tick(flow_input)) {
                status = "SAVE FLOW ERROR";
            } else if (flow.phase() == startup::SaveFlowPhase::Transition) {
                status = action_name(flow.selected_action());
            } else {
                status = "SELECT QUEST LOG";
            }
            draw(flow, status, have_handoff ? &handoff : nullptr);
        }

        if (flow.phase() == startup::SaveFlowPhase::Transition) {
            if (!flow.tick({false, false, false, false, false, true})) {
                status = "NEW GAME TRANSITION ERROR";
            } else if (flow.phase() == startup::SaveFlowPhase::HandoffReady &&
                       flow.consume_handoff(&handoff)) {
                have_handoff = true;
                status = "GAMEPLAY HANDOFF READY";
            }
            draw(flow, status, have_handoff ? &handoff : nullptr);
        }

        if ((pad.Buttons & PSP_CTRL_HOME) != 0) {
            break;
        }
        sceKernelDelayThread(16666);
    }

    sceKernelExitGame();
    return 0;
}
