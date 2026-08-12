#include "dusk/psp/link_fidelity.hpp"
#include "dusk/psp/real_room_runtime.hpp"
#include "dusk/psp/room_package.hpp"

#include <cmath>
#include <cstdint>
#include <fstream>
#include <iostream>
#include <iterator>
#include <string>
#include <string_view>
#include <vector>

namespace {

std::vector<std::uint8_t> read_file(const char* path) {
    std::ifstream stream(path, std::ios::binary);
    return {
        std::istreambuf_iterator<char>(stream),
        std::istreambuf_iterator<char>()};
}

bool json_float(
    const std::string& line, std::string_view key, float* value) {
    const std::string token = "\"" + std::string(key) + "\":";
    const std::size_t begin = line.find(token);
    if (begin == std::string::npos) {
        return false;
    }
    const char* first = line.c_str() + begin + token.size();
    char* last = nullptr;
    *value = std::strtof(first, &last);
    return last != first;
}

bool desktop_tick_30_speed(const char* path, float* speed) {
    std::ifstream input(path);
    std::string line;
    while (std::getline(input, line)) {
        if (line.find("\"event_type\":\"actor_state\"") !=
                std::string::npos &&
            line.find("\"game_tick\":30") != std::string::npos) {
            return json_float(line, "speed", speed);
        }
    }
    return false;
}

bool near(float a, float b, float tolerance = 0.0001f) {
    return std::fabs(a - b) <= tolerance;
}

}  // namespace

int main(int argc, char** argv) {
    using namespace dusk::psp;
    if (argc != 8) {
        return 2;
    }
    const bool expect_conflation =
        std::string_view(argv[1]) == "--expect-conflation";
    const bool expect_separated =
        std::string_view(argv[1]) == "--expect-separated";
    if (!expect_conflation && !expect_separated) {
        return 2;
    }
    constexpr float test_normal_speed = 0.5f;
    constexpr float test_foot_speed = 2.0f;
    constexpr float test_old_frame_rate = 0.25f;
    const float test_modifier =
        link::source_move_speed_modifier(test_normal_speed);
    const float test_base = link::source_effective_speed_base(
        test_normal_speed);
    const float test_animated = test_foot_speed *
        (1.0f - test_old_frame_rate) * test_modifier;
    if (!near(
            link::source_effective_speed(
                test_normal_speed, test_foot_speed,
                test_old_frame_rate),
            test_base + test_animated) ||
        !near(
            link::source_effective_speed(
                -test_normal_speed, test_foot_speed,
                test_old_frame_rate),
            -test_base - test_animated)) {
        return 8;
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
    float desktop_speed = 0.0f;
    if (!desktop_tick_30_speed(argv[6], &desktop_speed)) {
        return 4;
    }
    room::RealRoomRuntime runtime = {};
    if (!room::initialize_real_room_runtime(
            &runtime, model_view, texture_view,
            collision_view, scene_view)) {
        return 5;
    }
    room::Vec3 before = {};
    for (std::uint32_t tick = 0; tick <= 30; ++tick) {
        playable::Input input = {};
        if (tick == 30) {
            input.analog_y = -1.0f;
            before = runtime.state.position;
        }
        room::update_real_room(&runtime, input, 1.0f / 30.0f);
    }
    const float dx = runtime.state.position.x - before.x;
    const float dz = runtime.state.position.z - before.z;
    const float displacement = std::sqrt(dx * dx + dz * dz);
    const float accelerated_normal = link::kSourceAccelerationPerUpdate;
    const float expected_base =
        link::source_effective_speed_base(accelerated_normal);
    const bool conflated =
        near(displacement, accelerated_normal) &&
        near(runtime.state.speed / link::kUpdatesPerSecond,
             accelerated_normal);
    const bool separated =
        near(runtime.state.normal_speed, accelerated_normal) &&
        near(runtime.state.speed, expected_base) &&
        near(displacement, expected_base, 0.0002f);
    const bool expectation_met =
        expect_conflation ? conflated : separated;
    std::ofstream report(argv[7]);
    if (!report) {
        return 6;
    }
    report
        << "tick,desktop_speed,normal_speed,effective_speed,"
           "displacement,expected_blend_base,classification\n"
        << "30," << desktop_speed << ',' << runtime.state.normal_speed
        << ',' << runtime.state.speed << ',' << displacement << ','
        << expected_base << ','
        << (separated
                ? "NORMAL_EFFECTIVE_SPEED_SEPARATED"
                : conflated
                ? "NORMAL_EFFECTIVE_SPEED_CONFLATION"
                : "UNCLASSIFIED")
        << '\n';
    if (!expectation_met) {
        return 7;
    }
    std::cout
        << "LINK_MOVE_SPEED_CHECKPOINT_HOST_OK classification="
        << (separated
                ? "NORMAL_EFFECTIVE_SPEED_SEPARATED"
                : conflated
                ? "NORMAL_EFFECTIVE_SPEED_CONFLATION"
                : "UNCLASSIFIED")
        << " desktop_tick_30=" << desktop_speed
        << " displacement=" << displacement << '\n';
    return 0;
}
