#include "dusk/psp/link_fidelity.hpp"
#include "dusk/psp/real_room_runtime.hpp"
#include "dusk/psp/room_package.hpp"

#include <array>
#include <cmath>
#include <cstdint>
#include <fstream>
#include <iostream>
#include <iterator>
#include <limits>
#include <string>
#include <string_view>
#include <vector>

namespace {

constexpr std::uint32_t kFirstTick = 24;
constexpr std::uint32_t kLastTick = 40;
constexpr std::uint32_t kMaximumTick = kLastTick + 1;

struct DesktopSample {
    std::int16_t camera_yaw;
    std::int16_t target_yaw;
    std::int16_t actor_yaw;
    bool valid;
};

struct RuntimeSample {
    std::uint32_t tick;
    dusk::psp::room::RealRoomCheckpoint checkpoint;
    std::int16_t camera_yaw;
    std::int16_t target_yaw;
    std::int16_t actor_yaw;
};

struct ObserverContext {
    std::uint32_t tick;
    std::vector<RuntimeSample>* samples;
};

std::vector<std::uint8_t> read_file(const char* path) {
    std::ifstream stream(path, std::ios::binary);
    return {
        std::istreambuf_iterator<char>(stream),
        std::istreambuf_iterator<char>()};
}

bool json_integer(
    const std::string& line, std::string_view key,
    std::int32_t* value) {
    const std::string token = "\"" + std::string(key) + "\":";
    const std::size_t begin = line.find(token);
    if (begin == std::string::npos) {
        return false;
    }
    std::size_t cursor = begin + token.size();
    bool negative = false;
    if (cursor < line.size() && line[cursor] == '-') {
        negative = true;
        ++cursor;
    }
    if (cursor >= line.size() || line[cursor] < '0' ||
        line[cursor] > '9') {
        return false;
    }
    std::int32_t parsed = 0;
    while (cursor < line.size() && line[cursor] >= '0' &&
           line[cursor] <= '9') {
        parsed = parsed * 10 + (line[cursor] - '0');
        ++cursor;
    }
    *value = negative ? -parsed : parsed;
    return true;
}

bool read_desktop_samples(
    const char* path,
    std::array<DesktopSample, kMaximumTick + 1>* samples) {
    std::ifstream input(path);
    if (!input) {
        return false;
    }
    std::string line;
    while (std::getline(input, line)) {
        if (line.find("\"event_type\":\"actor_state\"") ==
            std::string::npos) {
            continue;
        }
        std::int32_t tick = 0;
        std::int32_t camera = 0;
        std::int32_t target = 0;
        std::int32_t actor = 0;
        if (!json_integer(line, "game_tick", &tick) ||
            !json_integer(line, "camera_yaw", &camera) ||
            !json_integer(line, "target_yaw", &target) ||
            !json_integer(line, "current_yaw", &actor) ||
            tick < 0 || tick > static_cast<std::int32_t>(kMaximumTick)) {
            continue;
        }
        (*samples)[static_cast<std::size_t>(tick)] = {
            static_cast<std::int16_t>(camera),
            static_cast<std::int16_t>(target),
            static_cast<std::int16_t>(actor), true};
    }
    for (std::uint32_t tick = kFirstTick;
         tick <= kMaximumTick; ++tick) {
        if (!(*samples)[tick].valid) {
            return false;
        }
    }
    return true;
}

std::int16_t camera_yaw_raw(
    const dusk::psp::room::RealRoomState& state) {
    return dusk::psp::link::radians_to_s16(
        dusk::psp::link::source_camera_heading(state.camera_yaw));
}

void observe(
    dusk::psp::room::RealRoomCheckpoint checkpoint,
    const dusk::psp::room::RealRoomState& state,
    void* user) {
    auto* context = static_cast<ObserverContext*>(user);
    if (context->tick < kFirstTick || context->tick > kLastTick) {
        return;
    }
    context->samples->push_back({
        context->tick, checkpoint, camera_yaw_raw(state),
        dusk::psp::link::radians_to_s16(state.target_yaw),
        dusk::psp::link::radians_to_s16(state.yaw)});
}

std::int32_t signed_angle_error(
    std::int16_t expected, std::int16_t actual) {
    return static_cast<std::int16_t>(actual - expected);
}

bool camera_exit_checkpoint(
    dusk::psp::room::RealRoomCheckpoint checkpoint) {
    return checkpoint ==
            dusk::psp::room::RealRoomCheckpoint::CameraUpdateExit ||
        checkpoint == dusk::psp::room::RealRoomCheckpoint::FramePresent;
}

bool causal_camera_checkpoint(
    dusk::psp::room::RealRoomCheckpoint checkpoint) {
    return checkpoint ==
            dusk::psp::room::RealRoomCheckpoint::InputUpdateEnter ||
        camera_exit_checkpoint(checkpoint);
}

}  // namespace

int main(int argc, char** argv) {
    using namespace dusk::psp;
    if (argc != 8) {
        return 2;
    }
    const bool expect_parity =
        std::string_view(argv[1]) == "--expect-parity";
    const bool expect_mismatch =
        std::string_view(argv[1]) == "--expect-mismatch";
    if (!expect_parity && !expect_mismatch) {
        return 2;
    }
    const auto model = read_file(argv[2]);
    const auto textures = read_file(argv[3]);
    const auto collision = read_file(argv[4]);
    const auto scene = read_file(argv[5]);
    room::PackageView model_view = {};
    room::PackageView texture_view = {};
    room::PackageView collision_view = {};
    room::PackageView scene_view = {};
    if (room::validate_dprm(
            model.data(), static_cast<std::uint32_t>(model.size()),
            &model_view) != room::PackageError::Ok ||
        room::validate_room_dptx(
            textures.data(), static_cast<std::uint32_t>(textures.size()),
            &texture_view) != room::PackageError::Ok ||
        room::validate_dpcl(
            collision.data(), static_cast<std::uint32_t>(collision.size()),
            &collision_view) != room::PackageError::Ok ||
        room::validate_dpsc(
            scene.data(), static_cast<std::uint32_t>(scene.size()),
            &scene_view) != room::PackageError::Ok) {
        return 3;
    }
    std::array<DesktopSample, kMaximumTick + 1> desktop = {};
    if (!read_desktop_samples(argv[6], &desktop)) {
        return 4;
    }
    room::RealRoomRuntime runtime = {};
    if (!room::initialize_real_room_runtime(
            &runtime, model_view, texture_view,
            collision_view, scene_view)) {
        return 5;
    }
    std::vector<RuntimeSample> samples;
    ObserverContext context = {0, &samples};
    room::set_real_room_checkpoint_observer(
        &runtime, observe, &context);
    for (std::uint32_t tick = 0; tick <= kLastTick; ++tick) {
        context.tick = tick;
        playable::Input input = {};
        if (tick >= 30 && tick <= 39) {
            input.analog_y = 1.0f;
        }
        room::update_real_room(&runtime, input, 1.0f / 30.0f);
    }
    std::ofstream output(argv[7]);
    if (!output) {
        return 6;
    }
    output << "tick,checkpoint,desktop_raw,desktop_angle,psp_raw,"
              "psp_angle,error,classification\n";
    bool camera_mismatch = false;
    bool downstream_target_mismatch = false;
    for (const RuntimeSample& sample : samples) {
        const std::uint32_t desktop_tick = sample.tick;
        const DesktopSample& expected = desktop[desktop_tick];
        const std::int32_t error = signed_angle_error(
            expected.camera_yaw, sample.camera_yaw);
        const bool mismatch = error != 0;
        camera_mismatch = camera_mismatch ||
            (causal_camera_checkpoint(sample.checkpoint) && mismatch);
        if (sample.tick == 33 &&
            sample.checkpoint ==
                room::RealRoomCheckpoint::InputUpdateExit) {
            downstream_target_mismatch =
                sample.target_yaw != desktop[33].target_yaw;
        }
        output << sample.tick << ','
               << room::real_room_checkpoint_name(sample.checkpoint)
               << ',' << expected.camera_yaw << ','
               << link::s16_to_radians(
                      static_cast<std::uint16_t>(expected.camera_yaw))
               << ',' << sample.camera_yaw << ','
               << link::s16_to_radians(
                      static_cast<std::uint16_t>(sample.camera_yaw))
               << ',' << error << ','
               << (mismatch ? "CAMERA_ANGLE_QUANTIZATION_ERROR" : "MATCH")
               << '\n';
    }
    const bool expectation_met =
        expect_parity ? !camera_mismatch : camera_mismatch;
    if (!expectation_met ||
        (expect_mismatch && !downstream_target_mismatch)) {
        return 7;
    }
    std::cout
        << "LINK_CAMERA_YAW_CHECKPOINT_HOST_OK classification="
        << (camera_mismatch ? "CAMERA_ANGLE_QUANTIZATION_ERROR" : "MATCH")
        << " ticks=24-40 source_type=cSAngle source_unit=s16 "
           "source_field=dCamera_c::mControlledYaw "
           "source_producer=dCamera_c::chaseCamera "
           "source_consumer=daAlink_c::setStickData "
           "runtime_field=RealRoomState::camera_yaw "
           "downstream_target="
        << (downstream_target_mismatch ? "mismatch" : "match")
        << '\n';
    return 0;
}
