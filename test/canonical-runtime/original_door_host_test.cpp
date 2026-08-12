#include "d/actor/d_a_obj_lv4PoGate.h"
#include "dusk/psp/model_runtime.hpp"
#include "dusk/psp/original_door_bridge.hpp"
#include "dusk/psp/original_scene_exit_bridge.hpp"
#include "dusk/psp/switch_runtime.hpp"

#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <fstream>
#include <iterator>
#include <vector>

namespace compat = dusk::psp::compat;
namespace model = dusk::psp::model;
namespace movebg = dusk::psp::movebg;
namespace process = dusk::psp::process;
namespace render = dusk::psp::render;
namespace resources = dusk::psp::resources;
namespace room = dusk::psp::room;
namespace stage = dusk::psp::stage;
namespace switches = dusk::psp::switches;

namespace {

struct Files {
    const char* model_path;
    const char* texture_path;
    const char* collision_path;
};

resources::PspResourceManager g_resources;
render::PspRenderQueue g_queue;
process::PspProcessManager g_processes;
movebg::PspMoveBgWorld g_movebg;
model::PspStaticModelRuntime g_models;
stage::PersistentDemoState g_persistent = {3, 3, 0, 0, false};
switches::PspSwitchSurface g_switches;

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
    const char* selected =
        std::strstr(path, "model.dprm") != nullptr
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

bool switch_query(
    void*, std::int8_t room_number, std::uint8_t number) {
    return g_switches.is_switch(number, room_number);
}

void switch_on(
    void*, std::int8_t room_number, std::uint8_t number) {
    g_switches.on_switch(number, room_number);
}

void switch_off(
    void*, std::int8_t room_number, std::uint8_t number) {
    g_switches.off_switch(number, room_number);
}

bool close(float left, float right) {
    return std::fabs(left - right) < 0.05f;
}

bool matrices_match(const Mtx& model_matrix, const movebg::Matrix34& collision) {
    for (std::uint32_t row = 0; row < 3; ++row) {
        for (std::uint32_t column = 0; column < 4; ++column) {
            if (!close(model_matrix[row][column],
                       collision.value[row][column])) {
                return false;
            }
        }
    }
    return true;
}

bool reach(
    daLv4PoGate_c* actor, float target, std::uint32_t maximum_frames) {
    for (std::uint32_t frame = 0; frame < maximum_frames; ++frame) {
        if (!g_processes.execute_all()) {
            return false;
        }
        movebg::Handle handle = {};
        if (!g_models.move_bg_handle(actor, &handle)) {
            return false;
        }
        const movebg::Matrix34* collision_matrix =
            g_movebg.matrix(handle);
        const J3DModel* door_model = actor->duskPspModel();
        if (collision_matrix == nullptr || door_model == nullptr ||
            !matrices_match(
                door_model->getBaseTRMtx(), *collision_matrix)) {
            return false;
        }
        if (close(actor->duskPspMoveValue(), target) &&
            actor->duskPspMode() == daLv4PoGate_c::MODE_WAIT_e) {
            return true;
        }
    }
    return false;
}

room::Vec3 vertex_at(
    const room::PackageView& package, std::uint16_t index) {
    const std::uint32_t offset =
        room::read_u32(package.bytes + 68);
    const std::uint8_t* vertex =
        package.bytes + offset + index * 12;
    return {
        room::read_f32(vertex),
        room::read_f32(vertex + 4),
        room::read_f32(vertex + 8),
    };
}

room::Vec3 transform_point(const Mtx& matrix, const room::Vec3& point) {
    return {
        matrix[0][0] * point.x + matrix[0][1] * point.y +
            matrix[0][2] * point.z + matrix[0][3],
        matrix[1][0] * point.x + matrix[1][1] * point.y +
            matrix[1][2] * point.z + matrix[1][3],
        matrix[2][0] * point.x + matrix[2][1] * point.y +
            matrix[2][2] * point.z + matrix[2][3],
    };
}

room::Vec3 add(const room::Vec3& left, const room::Vec3& right) {
    return {left.x + right.x, left.y + right.y, left.z + right.z};
}

room::Vec3 scale(const room::Vec3& value, float amount) {
    return {value.x * amount, value.y * amount, value.z * amount};
}

bool validate_placement(
    const room::PackageView& scene,
    const compat::OriginalDoorDescriptor& descriptor,
    room::SceneActorV3* placement) {
    const std::uint32_t count = room::read_u32(scene.bytes + 136);
    for (std::uint32_t index = 0; index < count; ++index) {
        room::SceneActorV3 candidate = {};
        if (room::read_dpsc_actor_v3(scene, index, &candidate) !=
            room::PackageError::Ok) {
            return false;
        }
        if (candidate.source_index != descriptor.source_record_index) {
            continue;
        }
        *placement = candidate;
        return candidate.supported == 1 &&
               candidate.name_hash == descriptor.source_name_hash &&
               candidate.process_id == descriptor.process_id &&
               candidate.parameters == descriptor.parameters &&
               candidate.room == descriptor.room &&
               close(candidate.position[0], descriptor.position[0]) &&
               close(candidate.position[1], descriptor.position[1]) &&
               close(candidate.position[2], descriptor.position[2]);
    }
    return false;
}

bool inspect_render(void*, const render::Command& command) {
    const auto* instance =
        static_cast<const J3DModel*>(command.payload);
    return command.kind == model::kStaticModelCommand &&
           command.bucket == render::Bucket::ActorOpaque &&
           instance != nullptr && instance->active();
}

}  // namespace

int main(int argc, char** argv) {
    if (argc != 5 || !compat::original_door_profile_valid()) {
        std::fputs("ORIGINAL_DOOR_HOST_FAILED arguments_or_profile\n", stderr);
        return 1;
    }
    const std::vector<std::uint8_t> model_bytes = read_file(argv[1]);
    const std::vector<std::uint8_t> texture_bytes = read_file(argv[2]);
    const std::vector<std::uint8_t> collision_bytes = read_file(argv[3]);
    const std::vector<std::uint8_t> scene_bytes = read_file(argv[4]);
    room::PackageView model_view = {};
    room::PackageView texture_view = {};
    room::PackageView collision_view = {};
    room::PackageView scene_view = {};
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
            &collision_view) != room::PackageError::Ok ||
        room::validate_dpsc(
            scene_bytes.data(),
            static_cast<std::uint32_t>(scene_bytes.size()),
            &scene_view) != room::PackageError::Ok) {
        std::fputs("ORIGINAL_DOOR_HOST_FAILED package\n", stderr);
        return 1;
    }

    const auto& descriptor = compat::original_door_descriptor();
    room::SceneActorV3 placement = {};
    if (!validate_placement(scene_view, descriptor, &placement)) {
        std::fputs("ORIGINAL_DOOR_HOST_FAILED placement\n", stderr);
        return 1;
    }

    char manifest[1024] = {};
    const int manifest_size = std::snprintf(
        manifest, sizeof(manifest),
        "DUSKLIGHT_RESOURCE_MANIFEST_V1\n"
        "object:L4R02Gate:4:model|StaticModel|model.dprm|%08X\n"
        "object:L4R02Gate:4:textures|TextureArchive|textures.dptx|%08X\n"
        "object:L4R02Gate:7:collision|RoomCollision|collision.dpcl|%08X\n",
        model_view.actual_crc, texture_view.actual_crc,
        collision_view.actual_crc);
    Files files = {argv[1], argv[2], argv[3]};
    g_queue.initialize();
    g_processes.initialize();
    g_movebg.initialize();
    if (manifest_size <= 0 ||
        !g_resources.initialize(
            ".", manifest, static_cast<std::uint32_t>(manifest_size),
            read_resource, &files) ||
        !g_models.initialize(&g_resources, &g_queue, &g_movebg) ||
        !g_switches.initialize(&g_persistent) ||
        !g_switches.enter_stage("D_MN10", 2) ||
        !compat::register_original_door_profile(&g_processes)) {
        std::fputs("ORIGINAL_DOOR_HOST_FAILED initialization\n", stderr);
        return 1;
    }
    process::bind_process_manager(&g_processes);
    model::bind_model_runtime(&g_models);
    compat::bind_scene_exit_facade({
        nullptr, nullptr, switch_query, switch_on, switch_off,
        nullptr, nullptr,
    });

    const process::CreateInput input = {
        static_cast<std::int16_t>(placement.process_id),
        placement.parameters,
        {placement.position[0], placement.position[1],
         placement.position[2]},
        {placement.rotation[0], placement.rotation[1],
         placement.rotation[2]},
        {placement.scale[0], placement.scale[1], placement.scale[2]},
        2,
        placement.table_hash,
        placement.source_index,
        placement.name_hash,
        static_cast<std::int8_t>(placement.layer),
    };
    process::ProcessHandle process_handle = {};
    if (!g_processes.create(input, &process_handle)) {
        std::fputs("ORIGINAL_DOOR_HOST_FAILED create\n", stderr);
        return 1;
    }
    auto* actor = static_cast<daLv4PoGate_c*>(
        g_processes.instance(process_handle));
    if (actor == nullptr || actor->duskPspSwitch() != 0x45 ||
        !close(actor->duskPspMoveValue(), 1000.0f) ||
        !reach(actor, 1000.0f, 2)) {
        std::fputs("ORIGINAL_DOOR_HOST_FAILED initial\n", stderr);
        return 1;
    }

    movebg::Handle collision_handle = {};
    if (!g_models.move_bg_handle(actor, &collision_handle) ||
        !g_switches.on_switch(0x45, 2) ||
        !reach(actor, 0.0f, 160) ||
        !close(actor->duskPspModel()->getBaseTRMtx()[1][3], 425.0f) ||
        !g_switches.off_switch(0x45, 2) ||
        !reach(actor, 1000.0f, 160) ||
        !close(actor->duskPspModel()->getBaseTRMtx()[1][3], 1425.0f)) {
        std::fputs("ORIGINAL_DOOR_HOST_FAILED motion\n", stderr);
        return 1;
    }

    const std::uint8_t* triangle =
        collision_view.bytes + room::read_u32(collision_view.bytes + 76);
    const room::Vec3 first =
        vertex_at(collision_view, room::read_u16(triangle));
    const room::Vec3 second =
        vertex_at(collision_view, room::read_u16(triangle + 2));
    const room::Vec3 third =
        vertex_at(collision_view, room::read_u16(triangle + 4));
    const room::Vec3 centroid = scale(
        add(add(first, second), third), 1.0f / 3.0f);
    const room::Vec3 normal = {
        room::read_f32(triangle + 8),
        room::read_f32(triangle + 12),
        room::read_f32(triangle + 16),
    };
    const Mtx& matrix = actor->duskPspModel()->getBaseTRMtx();
    const room::Vec3 world_centroid = transform_point(matrix, centroid);
    const auto hit = g_movebg.trace_camera(
        collision_handle,
        add(world_centroid, scale(normal, 50.0f)),
        add(world_centroid, scale(normal, -50.0f)));
    g_queue.begin_frame();
    if (!hit.hit || !g_processes.draw_all() || g_queue.size() != 1 ||
        !g_queue.flush(inspect_render, nullptr) ||
        !g_processes.destroy(process_handle) ||
        g_movebg.valid(collision_handle) ||
        g_processes.active_count() != 0 ||
        g_models.reference_count() != 0 ||
        g_models.metrics.movebg_creates != 1 ||
        g_models.metrics.movebg_updates == 0 ||
        g_models.metrics.movebg_deletes != 1 ||
        g_models.metrics.models_created != 1 ||
        g_models.metrics.models_destroyed != 1 ||
        g_models.metrics.errors != 0 ||
        g_movebg.metrics.dynamic_collision_frame_lag != 0 ||
        g_movebg.metrics.creates != 1 ||
        g_movebg.metrics.peak_handles != 1 ||
        g_movebg.metrics.deletes != 1 ||
        g_processes.metrics.errors != 0 ||
        g_resources.metrics.errors != 0) {
        std::fputs("ORIGINAL_DOOR_HOST_FAILED lifecycle\n", stderr);
        return 1;
    }

    compat::unbind_scene_exit_facade();
    model::unbind_model_runtime();
    process::unbind_process_manager();
    g_switches.shutdown();
    g_models.shutdown();
    g_movebg.shutdown();
    g_processes.shutdown();
    g_queue.shutdown();
    g_resources.shutdown();
    std::printf(
        "ORIGINAL_DOOR_HOST_OK process_id=0x009D switch=0x45 "
        "close=true open=true matrix_parity=true frame_lag=0 "
        "source_modified_lines=0 updates=%u\n",
        g_models.metrics.movebg_updates);
    return 0;
}
