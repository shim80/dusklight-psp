#include "dusk/psp/model_runtime.hpp"
#include "dusk/psp/original_rendered_actor_bridge.hpp"

#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <fstream>
#include <iterator>
#include <string>
#include <vector>

namespace compat = dusk::psp::compat;
namespace model = dusk::psp::model;
namespace movebg = dusk::psp::movebg;
namespace process = dusk::psp::process;
namespace render = dusk::psp::render;
namespace resources = dusk::psp::resources;
namespace room = dusk::psp::room;

namespace {

struct Files {
    const char* model_path;
    const char* texture_path;
    const char* collision_path;
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
    const char* selected = std::strstr(path, "model.dprm") != nullptr
        ? files->model_path
        : std::strstr(path, "textures.dptx") != nullptr
            ? files->texture_path
            : std::strstr(path, "collision.dpcl") != nullptr
                ? files->collision_path : nullptr;
    if (selected == nullptr) {
        return false;
    }
    const std::vector<std::uint8_t> bytes = read_file(selected);
    if (bytes.empty() || bytes.size() > capacity) {
        return false;
    }
    std::memcpy(output, bytes.data(), bytes.size());
    *size = static_cast<std::uint32_t>(bytes.size());
    return true;
}

bool close(float left, float right) {
    return std::fabs(left - right) < 0.01f;
}

struct Submission {
    std::uint32_t calls;
    bool valid;
    bool transform_seen;
    bool collision_seen;
    const room::SceneActorV3* expected;
    model::PspStaticModelRuntime* models;
    movebg::PspMoveBgWorld* movebg_world;
};

bool inspect_submission(void* user, const render::Command& command) {
    auto* submission = static_cast<Submission*>(user);
    const auto* instance = static_cast<const J3DModel*>(command.payload);
    ++submission->calls;
    submission->valid =
        submission->valid &&
        command.kind == model::kStaticModelCommand &&
        command.bucket == render::Bucket::ActorOpaque &&
        instance != nullptr && instance->active() &&
        instance->getModelData() != nullptr;
    if (instance != nullptr && instance->owner() != nullptr) {
        const auto* actor = static_cast<const fopAc_ac_c*>(
            instance->owner());
        Mtx& matrix = const_cast<J3DModel*>(instance)->getBaseTRMtx();
        if (close(actor->current.pos.x, submission->expected->position[0]) &&
            close(actor->current.pos.y, submission->expected->position[1]) &&
            close(actor->current.pos.z, submission->expected->position[2]) &&
            close(matrix[0][3], submission->expected->position[0]) &&
            close(matrix[1][3], submission->expected->position[1]) &&
            close(matrix[2][3], submission->expected->position[2])) {
            submission->transform_seen = true;
        }
        movebg::Handle handle = {};
        const movebg::Matrix34* collision = nullptr;
        const auto* move_actor =
            static_cast<const dBgS_MoveBgActor*>(instance->owner());
        bool matrix_match =
            submission->models->move_bg_handle(
                move_actor, &handle) &&
            (collision = submission->movebg_world->matrix(handle)) != nullptr;
        for (std::uint32_t row = 0; row < 3 && matrix_match; ++row) {
            for (std::uint32_t column = 0;
                 column < 4 && matrix_match; ++column) {
                matrix_match = close(
                    matrix[row][column],
                    collision->value[row][column]);
            }
        }
        submission->collision_seen =
            submission->collision_seen || matrix_match;
    }
    return submission->valid;
}

bool exercise_room(
    process::PspProcessManager* processes,
    render::PspRenderQueue* queue,
    model::PspStaticModelRuntime* models,
    movebg::PspMoveBgWorld* movebg_world,
    const char* scene_path, std::int8_t room_number,
    std::uint16_t expected_count) {
    const std::vector<std::uint8_t> bytes = read_file(scene_path);
    room::PackageView scene = {};
    if (room::validate_dpsc(
            bytes.data(), static_cast<std::uint32_t>(bytes.size()),
            &scene) != room::PackageError::Ok) {
        return false;
    }
    room::SceneActorV3 first = {};
    bool found = false;
    for (std::uint32_t index = 0;
         index < room::read_u32(scene.bytes + 136); ++index) {
        room::SceneActorV3 candidate = {};
        if (room::read_dpsc_actor_v3(
                scene, index, &candidate) != room::PackageError::Ok) {
            return false;
        }
        if (candidate.supported != 0 &&
            processes->profile_registered(
                static_cast<std::int16_t>(candidate.process_id))) {
            first = candidate;
            found = true;
            break;
        }
    }
    std::uint16_t created = 0;
    if (!found ||
        !compat::create_registered_room_actors(
            processes, scene, room_number, &created) ||
        created != expected_count ||
        processes->active_count() != expected_count) {
        return false;
    }
    queue->begin_frame();
    if (!processes->execute_all() || !processes->draw_all() ||
        queue->size() != expected_count) {
        return false;
    }
    Submission submission = {
        0, true, false, false, &first, models, movebg_world,
    };
    if (!queue->flush(inspect_submission, &submission) ||
        !submission.valid || !submission.transform_seen ||
        !submission.collision_seen ||
        submission.calls != expected_count) {
        return false;
    }
    processes->destroy_room(room_number);
    return processes->active_count() == 0 && queue->size() == 0;
}

}  // namespace

int main(int argc, char** argv) {
    if (argc != 6 || !compat::original_rendered_actor_profile_valid()) {
        std::fprintf(stderr, "profile_or_arguments\n");
        return 1;
    }
    const std::vector<std::uint8_t> model_bytes = read_file(argv[1]);
    const std::vector<std::uint8_t> texture_bytes = read_file(argv[2]);
    const std::vector<std::uint8_t> collision_bytes = read_file(argv[3]);
    room::PackageView model_view = {};
    room::PackageView texture_view = {};
    room::PackageView collision_view = {};
    if (room::validate_dprm(
            model_bytes.data(),
            static_cast<std::uint32_t>(model_bytes.size()),
            &model_view) != room::PackageError::Ok ||
        room::validate_room_dptx(
            texture_bytes.data(),
            static_cast<std::uint32_t>(texture_bytes.size()),
            &texture_view) != room::PackageError::Ok ||
        room::validate_dpcl(
            collision_bytes.data(),
            static_cast<std::uint32_t>(collision_bytes.size()),
            &collision_view) != room::PackageError::Ok) {
        std::fprintf(stderr, "package_validation\n");
        return 1;
    }
    char manifest[512] = {};
    const int manifest_size = std::snprintf(
        manifest, sizeof(manifest),
        "DUSKLIGHT_RESOURCE_MANIFEST_V1\n"
        "object:L4HsMato:4:model|StaticModel|model.dprm|%08X\n"
        "object:L4HsMato:4:textures|TextureArchive|textures.dptx|%08X\n"
        "object:L4HsMato:7:collision|RoomCollision|collision.dpcl|%08X\n",
        model_view.actual_crc, texture_view.actual_crc,
        collision_view.actual_crc);
    Files files = {argv[1], argv[2], argv[3]};
    resources::PspResourceManager resource_manager;
    render::PspRenderQueue queue;
    process::PspProcessManager processes;
    movebg::PspMoveBgWorld movebg_world;
    model::PspStaticModelRuntime models;
    queue.initialize();
    processes.initialize();
    movebg_world.initialize();
    if (manifest_size <= 0 ||
        !resource_manager.initialize(
            ".", manifest, static_cast<std::uint32_t>(manifest_size),
            read_resource, &files) ||
        !models.initialize(&resource_manager, &queue, &movebg_world) ||
        !compat::register_original_rendered_actor_profiles(&processes)) {
        std::fprintf(stderr, "runtime_initialization\n");
        return 1;
    }
    model::bind_model_runtime(&models);
    const bool rooms_valid =
        exercise_room(
            &processes, &queue, &models, &movebg_world, argv[4], 9, 1) &&
        models.reference_count() == 0 &&
        exercise_room(
            &processes, &queue, &models, &movebg_world, argv[5], 2, 6) &&
        models.reference_count() == 0 &&
        exercise_room(
            &processes, &queue, &models, &movebg_world, argv[4], 9, 1) &&
        models.reference_count() == 0;
    const bool lifecycle =
        models.metrics.archive_requests == 16 &&
        models.metrics.resource_get_calls == 8 &&
        models.metrics.resource_release_calls == 8 &&
        models.metrics.models_created == 8 &&
        models.metrics.models_destroyed == 8 &&
        models.metrics.models_peak > 0 &&
        models.metrics.render_commands == 8 &&
        models.metrics.movebg_creates == 8 &&
        models.metrics.movebg_deletes == 8 &&
        models.metrics.actor_heap_overflows == 0 &&
        models.metrics.errors == 0 &&
        processes.metrics.create_calls == 16 &&
        processes.metrics.execute_calls == 8 &&
        processes.metrics.draw_calls == 8 &&
        processes.metrics.delete_calls == 8 &&
        processes.metrics.errors == 0 &&
        resource_manager.metrics.load_calls == 9 &&
        resource_manager.metrics.release_calls == 9 &&
        movebg_world.metrics.creates == 8 &&
        movebg_world.metrics.deletes == 8 &&
        movebg_world.metrics.dynamic_collision_frame_lag == 0 &&
        resource_manager.metrics.errors == 0;
    model::unbind_model_runtime();
    models.shutdown();
    movebg_world.shutdown();
    processes.shutdown();
    queue.shutdown();
    resource_manager.shutdown();
    if (!rooms_valid || !lifecycle) {
        std::fprintf(
            stderr,
            "rooms=%u refs=%u archive=%u get=%u release=%u "
            "created=%u destroyed=%u commands=%u model_errors=%u "
            "bad_data=%u no_owner=%u no_arena=%u "
            "process_create=%u execute=%u draw=%u delete=%u "
            "process_errors=%u loads=%u resource_releases=%u "
            "resource_errors=%u\n",
            rooms_valid ? 1u : 0u, models.reference_count(),
            models.metrics.archive_requests,
            models.metrics.resource_get_calls,
            models.metrics.resource_release_calls,
            models.metrics.models_created,
            models.metrics.models_destroyed,
            models.metrics.render_commands,
            models.metrics.errors,
            models.metrics.invalid_model_data,
            models.metrics.missing_model_owner,
            models.metrics.missing_actor_arena,
            processes.metrics.create_calls,
            processes.metrics.execute_calls,
            processes.metrics.draw_calls,
            processes.metrics.delete_calls,
            processes.metrics.errors,
            resource_manager.metrics.load_calls,
            resource_manager.metrics.release_calls,
            resource_manager.metrics.errors);
        return 1;
    }
    std::puts(
        "ORIGINAL_RENDERED_ACTOR_HOST_OK process_id=0x009F "
        "instances=8 phase_create_calls=16 render_commands=8 "
        "movebg_creates=8 matrix_parity=true frame_lag=0");
    return 0;
}
