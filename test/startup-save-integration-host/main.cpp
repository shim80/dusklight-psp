#include "dusk/psp/startup_save_flow.hpp"

#include <array>
#include <cassert>
#include <cstdint>
#include <cstdio>
#include <cstring>

namespace {

using dusk::psp::startup::AdvancePolicy;
using dusk::psp::startup::Capability;
using dusk::psp::startup::Completeness;
using dusk::psp::startup::Segment;

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

constexpr std::uint32_t kCount = 7;
constexpr std::uint32_t kBytes =
    dusk::psp::startup::kPackageHeaderBytes +
    kCount * dusk::psp::startup::kSegmentRecordBytes;
using Fixture = std::array<std::uint8_t, kBytes>;

void set_record(
    Fixture& fixture,
    std::uint32_t index,
    Segment segment,
    AdvancePolicy policy,
    Completeness completeness,
    std::uint32_t duration,
    std::uint32_t capabilities,
    std::uint32_t token) {
    std::uint8_t* record =
        fixture.data() + dusk::psp::startup::kPackageHeaderBytes +
        index * dusk::psp::startup::kSegmentRecordBytes;
    record[0] = static_cast<std::uint8_t>(segment);
    record[1] = static_cast<std::uint8_t>(policy);
    record[2] = static_cast<std::uint8_t>(completeness);
    write_u32(record + 4, duration);
    write_u32(record + 8, 1);
    write_u32(record + 12, 1);
    write_u32(record + 16, capabilities);
    write_u32(record + 20, token);
}

Fixture make_fixture() {
    Fixture fixture = {};
    std::memcpy(fixture.data(), "DPST", 4);
    write_u16(fixture.data() + 4, 1);
    write_u16(
        fixture.data() + 6,
        dusk::psp::startup::kPackageHeaderBytes);
    write_u32(fixture.data() + 8, fixture.size());
    write_u32(fixture.data() + 12, kCount);
    write_u32(
        fixture.data() + 16,
        dusk::psp::startup::kPackageHeaderBytes);
    write_u32(
        fixture.data() + 20,
        dusk::psp::startup::kSegmentRecordBytes);

    set_record(
        fixture, 0, Segment::BootWarning,
        AdvancePolicy::TimedOrInput, Completeness::Complete,
        60, Capability::Ui, 0x4C4F474Fu);
    set_record(
        fixture, 1, Segment::OpeningLoad,
        AdvancePolicy::ResourceReady, Completeness::Complete,
        0, Capability::Stage, 0x46533130u);
    set_record(
        fixture, 2, Segment::OpeningRealtime,
        AdvancePolicy::SourceEvent, Completeness::Complete,
        0, Capability::Stage | Capability::Events, 0x4F50454Eu);
    set_record(
        fixture, 3, Segment::TitlePrompt,
        AdvancePolicy::InputRequired, Completeness::Complete,
        0, Capability::Ui | Capability::TitleModel, 0x5449544Cu);
    set_record(
        fixture, 4, Segment::FileSelect,
        AdvancePolicy::InputRequired, Completeness::Complete,
        0, Capability::Ui | Capability::FileSelection, 0x46494C45u);
    set_record(
        fixture, 5, Segment::NewGameTransition,
        AdvancePolicy::SourceEvent, Completeness::Complete,
        0, Capability::Stage | Capability::Events, 0x4E455747u);
    set_record(
        fixture, 6, Segment::UnsupportedGameplay,
        AdvancePolicy::UnsupportedBoundary, Completeness::Unsupported,
        0, Capability::Gameplay, 0x46533130u);
    write_u32(
        fixture.data() + dusk::psp::startup::kPackageCrcOffset,
        dusk::psp::startup::startup_crc32(
            fixture.data(), fixture.size()));
    return fixture;
}

void drive_to_file_select(dusk::psp::startup::StartupSaveFlow* flow) {
    using dusk::psp::startup::SaveFlowPhase;
    assert(flow != nullptr);
    assert(flow->phase() == SaveFlowPhase::Startup);
    assert(flow->tick({false, false, true}));
    assert(flow->startup().current_segment() == Segment::OpeningLoad);
    assert(flow->tick({false, false, false, false, true}));
    assert(flow->startup().current_segment() == Segment::OpeningRealtime);
    assert(flow->tick({false, false, false, false, false, true}));
    assert(flow->startup().current_segment() == Segment::TitlePrompt);
    assert(flow->tick({false, false, true}));
    assert(flow->startup().current_segment() == Segment::FileSelect);
    assert(flow->phase() == SaveFlowPhase::FileSelect);
}

}  // namespace

int main() {
    using namespace dusk::psp::save;
    using namespace dusk::psp::startup;

    constexpr const char* kPath = "startup_save_integration_test.bin";
    std::remove(kPath);

    Fixture fixture = make_fixture();
    PackageView package = {};
    assert(validate_startup_package(
        fixture.data(), fixture.size(), &package) == PackageError::Ok);

    const std::uint32_t capabilities =
        Capability::Ui | Capability::Stage | Capability::Events |
        Capability::TitleModel | Capability::FileSelection |
        Capability::Gameplay;

    StartupSaveFlow first;
    assert(first.initialize(package, capabilities, kPath));
    assert(first.last_storage_result() == StorageResult::NotFound);
    drive_to_file_select(&first);

    assert(first.tick({false, true}));
    assert(first.selected_slot() == 1);
    assert(first.tick({false, false, true}));
    assert(first.selected_action() == FileSelectAction::NewGame);
    assert(first.phase() == SaveFlowPhase::Transition);
    assert(first.startup().current_segment() == Segment::NewGameTransition);
    assert(first.last_storage_result() == StorageResult::Ok);

    SaveBank persisted_new_game;
    persisted_new_game.reset();
    assert(load_bank_file(kPath, &persisted_new_game) == StorageResult::Ok);
    assert(persisted_new_game.selected_slot() == 1);
    assert(persisted_new_game.slot(1).occupied);
    assert(std::strcmp(
        persisted_new_game.slot(1).start.stage.data(), "F_SP108") == 0);
    assert(persisted_new_game.slot(1).start.room == 1);
    assert(persisted_new_game.slot(1).start.start_point == 21);

    assert(first.tick({false, false, false, false, false, true}));
    assert(first.phase() == SaveFlowPhase::HandoffReady);
    assert(first.startup().current_segment() == Segment::UnsupportedGameplay);
    StartContext handoff = {};
    assert(first.consume_handoff(&handoff));
    assert(first.phase() == SaveFlowPhase::Gameplay);
    assert(std::strcmp(handoff.stage.data(), "F_SP108") == 0);
    assert(handoff.room == 1);
    assert(handoff.start_point == 21);

    StartContext checkpoint = {};
    std::memcpy(checkpoint.stage.data(), "D_MN05A", 7);
    checkpoint.stage[7] = '\0';
    checkpoint.room = 50;
    checkpoint.start_point = 3;
    checkpoint.layer = 0;
    assert(first.save_game(checkpoint, 3456));

    SaveBank persisted_checkpoint;
    persisted_checkpoint.reset();
    assert(load_bank_file(kPath, &persisted_checkpoint) == StorageResult::Ok);
    assert(persisted_checkpoint.selected_slot() == 1);
    assert(persisted_checkpoint.slot(1).play_seconds == 3456);
    assert(persisted_checkpoint.slot(1).save_counter == 2);
    assert(std::strcmp(
        persisted_checkpoint.slot(1).start.stage.data(), "D_MN05A") == 0);

    StartupSaveFlow resumed;
    assert(resumed.initialize(package, capabilities, kPath));
    assert(resumed.last_storage_result() == StorageResult::Ok);
    assert(resumed.selected_slot() == 1);
    drive_to_file_select(&resumed);
    assert(resumed.tick({false, false, false, true}));
    assert(resumed.selected_action() == FileSelectAction::Continue);
    assert(resumed.phase() == SaveFlowPhase::Transition);
    assert(resumed.tick({false, false, false, false, false, true}));
    assert(resumed.phase() == SaveFlowPhase::HandoffReady);
    StartContext resumed_handoff = {};
    assert(resumed.consume_handoff(&resumed_handoff));
    assert(std::strcmp(resumed_handoff.stage.data(), "D_MN05A") == 0);
    assert(resumed_handoff.room == 50);
    assert(resumed_handoff.start_point == 3);

    std::remove(kPath);
    std::puts(
        "STARTUP_SAVE_INTEGRATION_HOST_OK "
        "flow=intro-title-file-select-transition-gameplay "
        "new_game=F_SP108/R01/start21 immediate_persist=true "
        "continue_context=true gameplay_checkpoint=true");
    return 0;
}
