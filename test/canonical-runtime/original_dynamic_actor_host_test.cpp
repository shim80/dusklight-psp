#include "dusk/psp/model_runtime.hpp"
#include "dusk/psp/original_dynamic_actor_bridge.hpp"
#include "dusk/psp/original_rendered_actor_bridge.hpp"

#include <cstdint>
#include <cstdio>
#include <cstring>
#include <fstream>
#include <iterator>
#include <string>
#include <vector>

namespace compat = dusk::psp::compat;
namespace model = dusk::psp::model;
namespace process = dusk::psp::process;
namespace render = dusk::psp::render;
namespace resources = dusk::psp::resources;
namespace room = dusk::psp::room;

namespace {

struct Files {
    const char* small_model;
    const char* small_textures;
    const char* large_model;
    const char* large_textures;
};

std::vector<std::uint8_t> read_file(const char* path) {
    std::ifstream input(path, std::ios::binary);
    return {
        std::istreambuf_iterator<char>(input),
        std::istreambuf_iterator<char>()};
}

bool read_resource(
    void* user, const char* path, void* output,
    std::uint32_t capacity, std::uint32_t* size) {
    const auto* files = static_cast<const Files*>(user);
    const bool small = std::strstr(path, "/small/") != nullptr;
    const bool model_file = std::strstr(path, "model.dprm") != nullptr;
    const char* selected =
        small ? (model_file ? files->small_model : files->small_textures)
              : (model_file ? files->large_model : files->large_textures);
    const std::vector<std::uint8_t> bytes = read_file(selected);
    if (bytes.empty() || bytes.size() > capacity) {
        return false;
    }
    std::memcpy(output, bytes.data(), bytes.size());
    *size = static_cast<std::uint32_t>(bytes.size());
    return true;
}

std::uint32_t package_crc(const char* path, bool model_package) {
    const std::vector<std::uint8_t> bytes = read_file(path);
    room::PackageView view = {};
    const room::PackageError error =
        model_package
            ? room::validate_dprm(
                  bytes.data(),
                  static_cast<std::uint32_t>(bytes.size()), &view)
            : room::validate_room_dptx(
                  bytes.data(),
                  static_cast<std::uint32_t>(bytes.size()), &view);
    return error == room::PackageError::Ok ? view.actual_crc : 0;
}

bool accept_command(void* user, const render::Command& command) {
    auto* count = static_cast<std::uint32_t*>(user);
    ++*count;
    return command.kind == model::kStaticModelCommand &&
           command.payload != nullptr;
}

bool exercise_cycle(
    process::PspProcessManager* processes,
    render::PspRenderQueue* queue,
    const room::PackageView& scene) {
    if (!compat::activate_original_dynamic_actor_context(
            processes, scene, 9)) {
        return false;
    }
    std::uint16_t created = 0;
    if (!compat::create_registered_room_actors(
            processes, scene, 9, &created) ||
        created != 4 || processes->active_count() != 4) {
        return false;
    }
    for (std::uint32_t update = 0; update < 120; ++update) {
        compat::update_original_dynamic_actor_context(update, false);
        queue->begin_frame();
        if (!processes->execute_all() ||
            !compat::sample_original_dynamic_actor(
                *processes, false) ||
            !processes->draw_all() || queue->size() != 4) {
            return false;
        }
        std::uint32_t commands = 0;
        if (!queue->flush(accept_command, &commands) ||
            commands != 4) {
            return false;
        }
    }
    const std::int16_t paused_rotation =
        compat::original_dynamic_actor_metrics().rotation;
    for (std::uint32_t pause = 0; pause < 10; ++pause) {
        compat::update_original_dynamic_actor_context(120 + pause, true);
        if (!compat::sample_original_dynamic_actor(
                *processes, true)) {
            return false;
        }
    }
    if (compat::original_dynamic_actor_metrics().rotation !=
        paused_rotation) {
        return false;
    }
    compat::deactivate_original_dynamic_actor_context(9);
    processes->destroy_room(9);
    return processes->active_count() == 0;
}

}  // namespace

int main(int argc, char** argv) {
    if (argc != 6 ||
        !compat::original_dynamic_actor_profile_valid()) {
        std::fprintf(stderr, "profile_or_arguments\n");
        return 1;
    }
    const std::uint32_t crc[4] = {
        package_crc(argv[1], true),
        package_crc(argv[2], false),
        package_crc(argv[3], true),
        package_crc(argv[4], false),
    };
    if (crc[0] == 0 || crc[1] == 0 ||
        crc[2] == 0 || crc[3] == 0) {
        std::fprintf(stderr, "package_validation\n");
        return 1;
    }
    char manifest[1024] = {};
    const int manifest_size = std::snprintf(
        manifest, sizeof(manifest),
        "DUSKLIGHT_RESOURCE_MANIFEST_V1\n"
        "object:P_Gear:4:model|StaticModel|small/model.dprm|%08X\n"
        "object:P_Gear:4:textures|TextureArchive|small/textures.dptx|%08X\n"
        "object:P_Gear:3:model|StaticModel|large/model.dprm|%08X\n"
        "object:P_Gear:3:textures|TextureArchive|large/textures.dptx|%08X\n",
        crc[0], crc[1], crc[2], crc[3]);
    const std::vector<std::uint8_t> scene_bytes = read_file(argv[5]);
    room::PackageView scene = {};
    if (manifest_size <= 0 ||
        room::validate_dpsc(
            scene_bytes.data(),
            static_cast<std::uint32_t>(scene_bytes.size()),
            &scene) != room::PackageError::Ok) {
        std::fprintf(stderr, "scene_validation\n");
        return 1;
    }
    Files files = {argv[1], argv[2], argv[3], argv[4]};
    resources::PspResourceManager resource_manager;
    render::PspRenderQueue queue;
    process::PspProcessManager processes;
    model::PspStaticModelRuntime models;
    queue.initialize();
    processes.initialize();
    if (!resource_manager.initialize(
            ".", manifest, static_cast<std::uint32_t>(manifest_size),
            read_resource, &files) ||
        !models.initialize(&resource_manager, &queue) ||
        !compat::register_original_dynamic_actor_profile(&processes)) {
        std::fprintf(stderr, "runtime_initialization\n");
        return 1;
    }
    model::bind_model_runtime(&models);
    process::bind_process_manager(&processes);
    const bool cycles_valid =
        exercise_cycle(&processes, &queue, scene) &&
        models.reference_count() == 0 &&
        exercise_cycle(&processes, &queue, scene) &&
        models.reference_count() == 0;
    const process::Metrics* profile =
        compat::original_dynamic_actor_process_metrics(processes);
    const compat::OriginalDynamicActorMetrics& dynamic =
        compat::original_dynamic_actor_metrics();
    const bool parity =
        profile != nullptr && profile->create_calls == 16 &&
        profile->execute_calls == 960 &&
        profile->draw_calls == 960 &&
        profile->delete_calls == 8 && profile->errors == 0 &&
        dynamic.record_mapping_valid && dynamic.params_preserved &&
        dynamic.rotation_updates > 0 &&
        dynamic.pause_samples == 20 &&
        dynamic.pause_violations == 0 && dynamic.matrix_valid &&
        models.metrics.models_created == 8 &&
        models.metrics.models_destroyed == 8 &&
        models.metrics.actor_heap_overflows == 0 &&
        models.metrics.errors == 0 &&
        resource_manager.metrics.errors == 0;
    process::unbind_process_manager();
    model::unbind_model_runtime();
    models.shutdown();
    processes.shutdown();
    queue.shutdown();
    resource_manager.shutdown();
    if (!cycles_valid || !parity) {
        std::fprintf(
            stderr,
            "cycles=%u create=%u execute=%u draw=%u delete=%u "
            "rotation_updates=%u pause_violations=%u "
            "models=%u/%u model_errors=%u process_errors=%u\n",
            cycles_valid ? 1u : 0u,
            profile != nullptr ? profile->create_calls : 0,
            profile != nullptr ? profile->execute_calls : 0,
            profile != nullptr ? profile->draw_calls : 0,
            profile != nullptr ? profile->delete_calls : 0,
            dynamic.rotation_updates, dynamic.pause_violations,
            models.metrics.models_created,
            models.metrics.models_destroyed,
            models.metrics.errors,
            profile != nullptr ? profile->errors : 0);
        return 1;
    }
    std::puts(
        "ORIGINAL_DYNAMIC_ACTOR_HOST_OK process_id=0x0183 "
        "records=4 cycles=2 pause_stable=true");
    return 0;
}
