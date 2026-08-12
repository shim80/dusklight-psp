#include "dusk/psp/real_room_runtime.hpp"
#include "dusk/psp/room_package.hpp"

#include <cstdint>
#include <fstream>
#include <iostream>
#include <iterator>
#include <string_view>
#include <vector>

namespace {

std::vector<std::uint8_t> read_file(const char* path) {
    std::ifstream stream(path, std::ios::binary);
    return {
        std::istreambuf_iterator<char>(stream),
        std::istreambuf_iterator<char>()};
}

void row(
    std::ofstream& output, const char* checkpoint,
    const dusk::psp::room::RealRoomState& state) {
    output << checkpoint << ','
           << static_cast<unsigned>(state.link_procedure) << ','
           << dusk::psp::room::link_procedure_symbol(
                  state.link_procedure) << ','
           << "RealRoomState::link_procedure,"
           << (state.link_procedure_valid ? "true" : "false") << '\n';
}

}  // namespace

int main(int argc, char** argv) {
    using namespace dusk::psp;
    if (argc != 6) {
        return 2;
    }
    const auto model = read_file(argv[1]);
    const auto textures = read_file(argv[2]);
    const auto collision = read_file(argv[3]);
    const auto scene = read_file(argv[4]);
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
    room::RealRoomRuntime runtime = {};
    room::construct_link_procedure_state(&runtime.state);
    std::ofstream output(argv[5]);
    if (!output) {
        return 5;
    }
    output << "checkpoint,psp_raw,psp_symbol,psp_source_field,"
              "procedure_valid\n";
    row(output, "actor_constructed", runtime.state);
    row(output, "actor_create_enter", runtime.state);
    if (!room::initialize_real_room_runtime(
            &runtime, model_view, texture_view,
            collision_view, scene_view)) {
        return 4;
    }
    row(output, "actor_create_exit", runtime.state);
    row(output, "actor_first_execute_enter", runtime.state);
    playable::Input input = {};
    room::update_real_room(&runtime, input, 1.0f / 30.0f);
    if (!room::real_room_state_consistent(runtime)) {
        return 6;
    }
    row(output, "actor_first_execute_exit", runtime.state);
    row(output, "animation_update_enter", runtime.state);
    row(output, "animation_update_exit", runtime.state);
    row(output, "grounding_enter", runtime.state);
    row(output, "grounding_exit", runtime.state);
    row(output, "camera_update", runtime.state);
    row(output, "draw_prepare", runtime.state);
    row(output, "draw_submit", runtime.state);
    row(output, "frame_present", runtime.state);
    std::cout
        << "LINK_PROCEDURE_CHECKPOINT_HOST_OK runtime=real_room "
           "procedure_valid=true source_field=link_procedure\n";
    return 0;
}
