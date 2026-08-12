#include "dusk/psp/link_fidelity.hpp"
#include "dusk/psp/real_room_runtime.hpp"
#include "dusk/psp/room_package.hpp"

#include <array>
#include <cmath>
#include <cstdint>
#include <fstream>
#include <iostream>
#include <iterator>
#include <string_view>
#include <vector>

namespace {

constexpr float kEpsilon = 0.0001f;

std::vector<std::uint8_t> read_file(const char* path) {
    std::ifstream stream(path, std::ios::binary);
    return {
        std::istreambuf_iterator<char>(stream),
        std::istreambuf_iterator<char>()};
}

bool near(float a, float b, float tolerance = kEpsilon) {
    return std::fabs(a - b) <= tolerance;
}

bool coordinate_test() {
    using namespace dusk::psp;
    if (!link::coordinate_contract_valid(
            link::kSourceToPspWorld)) {
        return false;
    }
    constexpr link::SourceToPspWorldTransform invalid[] = {
        {1.0f, true, false, false},
        {1.0f, false, true, true},
        {2.0f, false, false, false},
        {1.0f, false, true, false},
        {0.0f, false, false, false},
    };
    for (const auto& transform : invalid) {
        if (link::coordinate_contract_valid(transform)) {
            return false;
        }
    }
    const room::Vec3 actor = {123.0f, 45.0f, -67.0f};
    const std::array<std::uint16_t, 11> angles = {
        0x0000, 0x1000, 0x2000, 0x3000, 0x4000, 0x6000,
        0x8000, 0xa000, 0xc000, 0xe000, 0xffff,
    };
    for (const std::uint16_t angle : angles) {
        const link::ActorMatrix matrix =
            link::actor_matrix(actor, link::s16_to_radians(angle));
        const room::Vec3 origin =
            link::transform_point(matrix, {0.0f, 0.0f, 0.0f});
        const room::Vec3 root =
            link::transform_point(matrix, {0.0f, 103.0f, 0.0f});
        const room::Vec3 target = link::camera_target(actor);
        const room::Vec3 attention =
            link::normal_attention_origin(matrix);
        const float source_camera =
            link::source_camera_heading(
                link::wrap_angle(
                    link::s16_to_radians(angle) + link::kPi));
        if (!near(origin.x, actor.x) ||
            !near(origin.y, actor.y) ||
            !near(origin.z, actor.z) ||
            !near(root.x, actor.x) ||
            !near(root.y, actor.y + 103.0f) ||
            !near(root.z, actor.z) ||
            !near(target.x, actor.x) ||
            !near(target.y, actor.y + 95.0f) ||
            !near(target.z, actor.z) ||
            !near(attention.x, actor.x) ||
            !near(attention.y, actor.y + 150.0f) ||
            !near(attention.z, actor.z) ||
            !near(
                link::wrap_angle(
                    source_camera -
                    link::s16_to_radians(angle)),
                0.0f)) {
            return false;
        }
    }
    return true;
}

bool forward_test() {
    using namespace dusk::psp;
    struct Cardinal {
        std::uint16_t angle;
        float x;
        float z;
    };
    constexpr Cardinal cardinals[] = {
        {0x0000, 0.0f, 1.0f},
        {0x4000, 1.0f, 0.0f},
        {0x8000, 0.0f, -1.0f},
        {0xc000, -1.0f, 0.0f},
    };
    for (const Cardinal& cardinal : cardinals) {
        const link::Vec2 forward =
            link::forward_from_yaw(
                link::s16_to_radians(cardinal.angle));
        if (!near(forward.x, cardinal.x) ||
            !near(forward.z, cardinal.z)) {
            return false;
        }
    }

    struct Raw {
        std::uint8_t x;
        std::uint8_t y;
        bool moves;
    };
    constexpr Raw inputs[] = {
        {128, 128, false}, {128, 0, true}, {128, 255, true},
        {0, 128, true}, {255, 128, true}, {0, 0, true},
        {255, 0, true}, {0, 255, true}, {255, 255, true},
    };
    for (const Raw& raw : inputs) {
        const float x = std::clamp(
            (static_cast<float>(raw.x) - 128.0f) / 127.0f,
            -1.0f, 1.0f);
        const float y = std::clamp(
            (static_cast<float>(raw.y) - 128.0f) / 127.0f,
            -1.0f, 1.0f);
        const link::Stick stick = link::normalize_stick(x, y);
        if ((stick.magnitude > 0.0f) != raw.moves) {
            return false;
        }
        if (raw.x == 128 && raw.y == 0 &&
            !(stick.forward > 0.0f)) {
            return false;
        }
    }
    const link::Stick up = link::normalize_stick(0.0f, -1.0f);
    const link::Stick source_walk =
        link::normalize_stick(0.0f, -60.0f / 127.0f);
    if (!near(source_walk.magnitude, 28.0f / 54.0f) ||
        !near(source_walk.forward, 28.0f / 54.0f)) {
        std::cerr << "source stick clamp=" << source_walk.magnitude
                  << '\n';
        return false;
    }
    const link::Vec2 north =
        link::camera_relative_world(up, link::kPi);
    const float quarter_turn = link::source_move_yaw(
        -link::kPi * 0.5f, 0.0f);
    const float half_turn = link::source_move_yaw(-link::kPi, 0.0f);
    const link::Stick slope_stick =
        link::normalize_stick(-55.0f / 127.0f, -95.0f / 127.0f);
    const float slope_target = link::source_move_yaw(
        link::s16_to_radians(static_cast<std::uint16_t>(4754)), 0.0f);
    const float slope_yaw_30 = link::source_move_approach_yaw(
        link::s16_to_radians(0x8000u), slope_target,
        slope_stick.magnitude);
    const float slope_yaw_31 = link::source_move_approach_yaw(
        slope_yaw_30, slope_target, slope_stick.magnitude);
    const bool source_angles_ok =
        link::source_link_stick_angle_s16(slope_stick) == 4754 &&
        link::radians_to_s16(slope_yaw_30) == -31818 &&
        link::radians_to_s16(slope_yaw_31) == -31058;
    if (!source_angles_ok) {
        std::cerr << "source angle checkpoints stick="
                  << link::source_link_stick_angle_s16(slope_stick)
                  << " yaw30=" << link::radians_to_s16(slope_yaw_30)
                  << " yaw31=" << link::radians_to_s16(slope_yaw_31)
                  << '\n';
    }
    return near(north.x, 0.0f) && near(north.z, 1.0f) &&
           link::source_link_stick_angle_s16(
               link::normalize_stick(1.0f, 0.0f)) == -16384 &&
           link::source_link_stick_angle_s16(
               link::normalize_stick(0.0f, 1.0f)) == -32768 &&
           source_angles_ok &&
           link::radians_to_s16(quarter_turn) == 16384 &&
           link::radians_to_s16(half_turn) == 0;
}

bool locomotion_reference_test() {
    using namespace dusk::psp;
    constexpr float dt = 1.0f / 30.0f;
    constexpr float camera_yaws[] = {
        link::kPi, -link::kPi * 0.5f, 0.0f,
        link::kPi * 0.5f,
    };
    float minimum_dot = 1.0f;
    for (const float camera_yaw : camera_yaws) {
        const link::Stick stick = link::normalize_stick(0.0f, -1.0f);
        const link::Vec2 world =
            link::camera_relative_world(stick, camera_yaw);
        const float target = std::atan2(world.x, world.z);
        float yaw = target;
        float speed = 0.0f;
        for (std::uint32_t frame = 0; frame < 60; ++frame) {
            yaw = link::approach_yaw(yaw, target, dt);
            speed = link::approach_speed(
                speed, link::source_speed_target(stick.magnitude),
                stick.magnitude, dt);
            const link::Vec2 model = link::forward_from_yaw(yaw);
            const float velocity_x = model.x * speed;
            const float velocity_z = model.z * speed;
            const float length = std::sqrt(
                velocity_x * velocity_x + velocity_z * velocity_z);
            if (length > kEpsilon) {
                const float dot =
                    (model.x * velocity_x +
                     model.z * velocity_z) / length;
                minimum_dot = std::min(minimum_dot, dot);
            }
        }
        if (!near(
                speed,
                link::kSourceMaxSpeedPerUpdate *
                    link::kUpdatesPerSecond,
                0.01f)) {
            std::cerr << "reference speed=" << speed << '\n';
            return false;
        }
    }
    if (minimum_dot < 0.995f) {
        std::cerr << "reference dot=" << minimum_dot << '\n';
    }
    return minimum_dot >= 0.995f;
}

bool wait_turn_reference_test() {
    using namespace dusk::psp;
    float yaw = link::s16_to_radians(0x8000);
    const float target = 0.0f;
    if (!link::source_wait_turn_required(yaw, target)) {
        std::cerr << "source wait-turn threshold mismatch\n";
        return false;
    }
    if (link::source_yaw_error_s16(target, yaw) != -32768) {
        std::cerr << "source wait-turn error unit mismatch\n";
        return false;
    }
    constexpr std::int16_t expected[] = {
        24768, 16768, 8768, 768, 0,
    };
    for (const std::int16_t checkpoint : expected) {
        yaw = link::source_wait_turn_yaw(yaw, target);
        if (link::radians_to_s16(yaw) != checkpoint) {
            std::cerr << "source wait-turn yaw="
                      << link::radians_to_s16(yaw)
                      << " expected=" << checkpoint << '\n';
            return false;
        }
    }
    return link::source_wait_turn_reached(yaw, target);
}

bool runtime_test(
    const char* model_path, const char* texture_path,
    const char* collision_path, const char* scene_path) {
    using namespace dusk::psp;
    const std::vector<std::uint8_t> model = read_file(model_path);
    const std::vector<std::uint8_t> textures = read_file(texture_path);
    const std::vector<std::uint8_t> collision = read_file(collision_path);
    const std::vector<std::uint8_t> scene = read_file(scene_path);
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
        std::cerr << "runtime package validation failed\n";
        return false;
    }
    room::RealRoomRuntime runtime = {};
    if (!room::initialize_real_room_runtime(
            &runtime, model_view, texture_view,
            collision_view, scene_view)) {
        std::cerr << "runtime initialization failed\n";
        return false;
    }
    const float initial_camera_x =
        runtime.state.camera_eye.x - runtime.state.camera_center.x;
    const float initial_camera_y =
        runtime.state.camera_eye.y - runtime.state.camera_center.y;
    const float initial_camera_z =
        runtime.state.camera_eye.z - runtime.state.camera_center.z;
    const float initial_camera_distance = std::sqrt(
        initial_camera_x * initial_camera_x +
        initial_camera_y * initial_camera_y +
        initial_camera_z * initial_camera_z);
    if (!near(runtime.state.target_yaw, runtime.state.yaw) ||
        !(initial_camera_distance > 1.0f) ||
        initial_camera_distance > 500.0f) {
        std::cerr << "runtime initial camera invalid distance="
                  << initial_camera_distance << '\n';
        return false;
    }
    if (!near(runtime.state.camera_center.x, 0.0f, 0.001f) ||
        !near(runtime.state.camera_center.y, 592.5f, 0.001f) ||
        !near(runtime.state.camera_center.z, -4650.0f, 0.001f) ||
        !near(runtime.state.camera_eye.x, 0.0f, 0.001f) ||
        !near(runtime.state.camera_eye.y, 592.5f, 0.001f) ||
        !near(runtime.state.camera_eye.z, -4350.0f, 0.001f)) {
        std::cerr << "source camera initial checkpoint mismatch center="
                  << runtime.state.camera_center.x << ','
                  << runtime.state.camera_center.y << ','
                  << runtime.state.camera_center.z << " eye="
                  << runtime.state.camera_eye.x << ','
                  << runtime.state.camera_eye.y << ','
                  << runtime.state.camera_eye.z << '\n';
        return false;
    }
    playable::Input settle_input = {};
    room::update_real_room(&runtime, settle_input, 1.0f / 30.0f);
    if (!near(runtime.state.camera_center.y, 584.625f, 0.001f) ||
        !near(runtime.state.camera_center.z, -4650.75f, 0.001f) ||
        !near(runtime.state.camera_eye.y, 673.58014f, 0.002f) ||
        !near(runtime.state.camera_eye.z, -4364.2417f, 0.002f) ||
        runtime.state.camera_style_timer != 1 ||
        !runtime.state.camera_style_settled) {
        std::cerr << "source camera settle checkpoint mismatch eye_y="
                  << runtime.state.camera_eye.y << " eye_z="
                  << runtime.state.camera_eye.z << " center_y="
                  << runtime.state.camera_center.y << " actor_y="
                  << runtime.state.position.y << '\n';
        return false;
    }
    while (runtime.state.updates < 29) {
        room::update_real_room(
            &runtime, settle_input, 1.0f / 30.0f);
    }
    if (!near(runtime.state.input_stick_angle, -link::kPi, 0.0001f) ||
        !near(runtime.state.target_yaw, 0.0f, 0.0001f)) {
        std::cerr << "source demo input release checkpoint mismatch\n";
        return false;
    }
    room::RealRoomRuntime wait_turn_runtime = runtime;
    while (wait_turn_runtime.state.updates < 30) {
        room::update_real_room(
            &wait_turn_runtime, settle_input, 1.0f / 30.0f);
    }
    wait_turn_runtime.state.camera_yaw = 0.0f;
    wait_turn_runtime.state.camera_controlled_yaw_next = 0.0f;
    playable::Input wait_turn_input = {};
    wait_turn_input.analog_y = 1.0f;
    constexpr std::int16_t wait_turn_yaws[] = {
        -32768, 24768, 16768, 8768, 768, 0,
    };
    for (std::size_t frame = 0;
         frame < std::size(wait_turn_yaws); ++frame) {
        room::update_real_room(
            &wait_turn_runtime, wait_turn_input, 1.0f / 30.0f);
        const std::int16_t yaw =
            link::radians_to_s16(wait_turn_runtime.state.yaw);
        const room::LinkProcedure expected_procedure =
            frame + 1 == std::size(wait_turn_yaws)
                ? room::LinkProcedure::Move
                : room::LinkProcedure::WaitTurn;
        const playable::Locomotion expected_locomotion =
            frame + 1 == std::size(wait_turn_yaws)
                ? playable::Locomotion::Walk
                : playable::Locomotion::TurnInPlace;
        const float expected_normal_speed =
            frame + 1 == std::size(wait_turn_yaws)
                ? link::kSourceAccelerationPerUpdate : 0.0f;
        const float expected_effective_speed =
            link::source_effective_speed_base(expected_normal_speed);
        if (yaw != wait_turn_yaws[frame] ||
            wait_turn_runtime.state.link_procedure !=
                expected_procedure ||
            wait_turn_runtime.state.locomotion != expected_locomotion ||
            !near(
                wait_turn_runtime.state.normal_speed,
                expected_normal_speed) ||
            !near(
                wait_turn_runtime.state.speed,
                expected_effective_speed)) {
            std::cerr << "source wait-turn runtime frame=" << frame
                      << " yaw=" << yaw << " expected="
                      << wait_turn_yaws[frame] << " procedure="
                      << static_cast<unsigned>(
                             wait_turn_runtime.state.link_procedure)
                      << " locomotion="
                      << static_cast<unsigned>(
                             wait_turn_runtime.state.locomotion)
                      << " normal_speed="
                      << wait_turn_runtime.state.normal_speed
                      << " effective_speed="
                      << wait_turn_runtime.state.speed
                      << " target="
                      << link::radians_to_s16(
                             wait_turn_runtime.state.wait_turn_target_yaw)
                      << '\n';
            return false;
        }
    }
    runtime.state.camera_yaw = link::kPi;
    playable::Input input = {};
    input.analog_y = -1.0f;
    for (std::uint32_t frame = 0; frame < 60; ++frame) {
        room::update_real_room(&runtime, input, 1.0f / 30.0f);
        if (!room::real_room_state_consistent(runtime)) {
            std::cerr << "runtime inconsistent frame=" << frame
                      << " backward="
                      << runtime.state.backward_visual_frames
                      << " min="
                      << runtime.state.forward_alignment_dot_min
                      << '\n';
            return false;
        }
    }
    const bool ok =
        runtime.state.backward_visual_frames == 0 &&
        runtime.state.forward_alignment_dot_min >= 0.995f &&
        runtime.state.forward_alignment_samples > 0;
    if (!ok) {
        std::cerr
            << "runtime alignment backward="
            << runtime.state.backward_visual_frames
            << " min=" << runtime.state.forward_alignment_dot_min
            << " samples=" << runtime.state.forward_alignment_samples
            << " speed=" << runtime.state.speed
            << " phase="
            << static_cast<unsigned>(runtime.state.motion_phase)
            << '\n';
    }
    return ok;
}

}  // namespace

int main(int argc, char** argv) {
    if (argc != 7 || std::string_view(argv[1]) != "--target") {
        return 2;
    }
    const std::string_view target = argv[2];
    bool ok = coordinate_test();
    if (target == "forward" || target == "locomotion" ||
        target == "all") {
        ok = ok && forward_test();
    }
    if (target == "locomotion" || target == "all") {
        const bool reference_ok = locomotion_reference_test();
        const bool wait_turn_ok = wait_turn_reference_test();
        const bool runtime_ok =
            runtime_test(argv[3], argv[4], argv[5], argv[6]);
        if (!reference_ok || !wait_turn_ok || !runtime_ok) {
            std::cerr << "locomotion reference=" << reference_ok
                      << " wait_turn=" << wait_turn_ok
                      << " runtime=" << runtime_ok << '\n';
        }
        ok = ok && reference_ok && wait_turn_ok && runtime_ok;
    }
    if (target != "coordinate" && target != "pivot" &&
        target != "forward" && target != "locomotion" &&
        target != "all") {
        return 3;
    }
    if (!ok) {
        return 4;
    }
    std::cout
        << "LINK_FIDELITY_HOST_OK target=" << target
        << " world_scale=1 pivot_drift_max=0"
        << " forward_alignment_dot_min=1"
        << " backward_visual_frames=0"
        << " coordinate_negatives=5\n";
    return 0;
}
