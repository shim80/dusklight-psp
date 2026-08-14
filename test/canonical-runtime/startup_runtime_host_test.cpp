#include "dusk/psp/startup_package.hpp"
#include "dusk/psp/startup_runtime.hpp"

#include <array>
#include <cassert>
#include <cstdint>
#include <cstdio>
#include <cstring>

namespace {

using dusk::psp::startup::AdvancePolicy;
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

constexpr std::uint32_t kCount = 6;
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
    std::uint32_t source_token) {
    std::uint8_t* record =
        fixture.data() + dusk::psp::startup::kPackageHeaderBytes +
        index * dusk::psp::startup::kSegmentRecordBytes;
    record[0] = static_cast<std::uint8_t>(segment);
    record[1] = static_cast<std::uint8_t>(policy);
    record[2] = static_cast<std::uint8_t>(completeness);
    write_u32(record + 4, duration);
    write_u32(record + 8, 30);
    write_u32(record + 12, 30);
    write_u32(record + 16, capabilities);
    write_u32(record + 20, source_token);
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
        fixture, 0, Segment::TeamLogo,
        AdvancePolicy::TimedOrInput, Completeness::Complete,
        3510, dusk::psp::startup::Capability::Ui, 0x4C4F474Fu);
    set_record(
        fixture, 1, Segment::NintendoLogo,
        AdvancePolicy::Timed, Completeness::Complete,
        90, dusk::psp::startup::Capability::Ui, 0x4E494E54u);
    set_record(
        fixture, 2, Segment::OpeningLoad,
        AdvancePolicy::ResourceReady, Completeness::Complete,
        0, dusk::psp::startup::Capability::Stage, 0x46533130u);
    set_record(
        fixture, 3, Segment::OpeningRealtime,
        AdvancePolicy::SourceEvent, Completeness::Partial,
        0, dusk::psp::startup::Capability::Stage, 0x4F50454Eu);
    set_record(
        fixture, 4, Segment::TitlePrompt,
        AdvancePolicy::InputRequired, Completeness::Complete,
        0, dusk::psp::startup::Capability::Ui, 0x5449544Cu);
    set_record(
        fixture, 5, Segment::UnsupportedGameplay,
        AdvancePolicy::UnsupportedBoundary, Completeness::Unsupported,
        0, dusk::psp::startup::Capability::Gameplay, 0x46533130u);
    write_u32(
        fixture.data() + dusk::psp::startup::kPackageCrcOffset,
        dusk::psp::startup::startup_crc32(
            fixture.data(), fixture.size()));
    return fixture;
}

void refresh_crc(Fixture& fixture) {
    write_u32(
        fixture.data() + dusk::psp::startup::kPackageCrcOffset,
        dusk::psp::startup::startup_crc32(
            fixture.data(), fixture.size()));
}

}  // namespace

int main() {
    using namespace dusk::psp::startup;
    Fixture fixture = make_fixture();
    PackageView package = {};
    assert(validate_startup_package(
        fixture.data(), fixture.size(), &package) == PackageError::Ok);
    assert(package.segment_count == kCount);

    SegmentRecord record = {};
    assert(package.segment(0, &record));
    assert(record.segment == Segment::TeamLogo);
    assert(record.duration_frames == 3510);
    assert(std::strcmp(segment_name(record.segment), "team_logo") == 0);

    StartupRuntime runtime;
    const std::uint32_t available =
        Capability::Ui | Capability::Stage;
    assert(runtime.initialize(package, available));
    assert(runtime.current_segment() == Segment::TeamLogo);
    assert(runtime.tick({true, false}, false));
    assert(runtime.current_segment() == Segment::NintendoLogo);
    for (std::uint32_t frame = 0; frame < 89; ++frame) {
        assert(runtime.tick({}, false));
        assert(runtime.current_segment() == Segment::NintendoLogo);
    }
    assert(runtime.tick({}, false));
    assert(runtime.current_segment() == Segment::OpeningLoad);
    assert(runtime.tick({}, false));
    assert(runtime.current_segment() == Segment::OpeningLoad);
    assert(runtime.tick({}, true));
    assert(runtime.current_segment() == Segment::OpeningRealtime);
    assert(runtime.current_completeness() == Completeness::Partial);
    assert(runtime.tick({}, false, false));
    assert(runtime.current_segment() == Segment::OpeningRealtime);
    assert(runtime.tick({}, false, true));
    assert(runtime.current_segment() == Segment::TitlePrompt);
    assert(runtime.tick({}, false));
    assert(runtime.current_segment() == Segment::TitlePrompt);
    assert(runtime.tick({false, true}, false));
    assert(runtime.current_segment() == Segment::UnsupportedGameplay);
    assert(runtime.blocked());
    assert(runtime.missing_capabilities() == Capability::Gameplay);
    assert(runtime.metrics().transitions == 5);
    assert(runtime.metrics().input_transitions == 2);
    assert(runtime.metrics().timeout_transitions == 1);
    assert(runtime.metrics().resource_transitions == 1);
    assert(runtime.metrics().source_event_transitions == 1);
    assert(runtime.metrics().unsupported_boundaries == 1);

    Fixture invalid = fixture;
    invalid[0] = 'X';
    assert(validate_startup_package(
        invalid.data(), invalid.size(), &package) ==
        PackageError::BadMagic);
    invalid = fixture;
    invalid[70] ^= 1;
    assert(validate_startup_package(
        invalid.data(), invalid.size(), &package) ==
        PackageError::CrcMismatch);
    invalid = fixture;
    invalid[64 + 32] = static_cast<std::uint8_t>(Segment::TeamLogo);
    refresh_crc(invalid);
    assert(validate_startup_package(
        invalid.data(), invalid.size(), &package) ==
        PackageError::InvalidOrder);
    invalid = fixture;
    invalid[64 + 1] = static_cast<std::uint8_t>(AdvancePolicy::Timed);
    write_u32(invalid.data() + 64 + 4, 0);
    refresh_crc(invalid);
    assert(validate_startup_package(
        invalid.data(), invalid.size(), &package) ==
        PackageError::InvalidDuration);
    assert(validate_startup_package(
        nullptr, fixture.size(), &package) == PackageError::NullInput);

    std::puts(
        "STARTUP_RUNTIME_HOST_OK format=DPST1 segments=6 "
        "team_logo_timer=3510 legacy_logo_timer=90 deterministic=true negatives=5");
    return 0;
}
