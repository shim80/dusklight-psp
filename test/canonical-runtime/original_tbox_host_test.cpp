#include "d/actor/d_a_tbox.h"
#include "dusk/psp/event_runtime.hpp"
#include "dusk/psp/interaction_runtime.hpp"
#include "dusk/psp/item_runtime.hpp"
#include "dusk/psp/model_runtime.hpp"
#include "dusk/psp/original_scene_exit_bridge.hpp"
#include "dusk/psp/original_tbox_bridge.hpp"
#include "dusk/psp/render_queue.hpp"
#include "dusk/psp/resource_manager.hpp"
#include "dusk/psp/room_package.hpp"

#include <cstdint>
#include <cstdio>
#include <cstring>
#include <fstream>
#include <iterator>
#include <string>
#include <vector>

namespace compat = dusk::psp::compat;
namespace events = dusk::psp::events;
namespace interaction = dusk::psp::interaction;
namespace items = dusk::psp::items;
namespace model = dusk::psp::model;
namespace movebg = dusk::psp::movebg;
namespace process = dusk::psp::process;
namespace render = dusk::psp::render;
namespace resources = dusk::psp::resources;
namespace room = dusk::psp::room;

namespace {

resources::PspResourceManager g_resources;
render::PspRenderQueue g_queue;
movebg::PspMoveBgWorld g_world;
model::PspStaticModelRuntime g_models;
process::PspProcessManager g_processes;
events::PspEventContext g_events;
interaction::PspInteractionContext g_interactions;
items::PspItemContext g_items;

std::vector<std::uint8_t> read_file(const std::string& path) {
    std::ifstream input(path, std::ios::binary);
    return {
        std::istreambuf_iterator<char>(input),
        std::istreambuf_iterator<char>()};
}

bool read_resource(
    void* user, const char* path, void* output,
    std::uint32_t capacity, std::uint32_t* size) {
    const auto* root = static_cast<const std::string*>(user);
    const char* relative =
        std::strncmp(path, "./data/", 7) == 0 ? path + 7 : path;
    const auto bytes = read_file(*root + "/" + relative);
    if (bytes.empty() || bytes.size() > capacity) {
        return false;
    }
    std::memcpy(output, bytes.data(), bytes.size());
    *size = static_cast<std::uint32_t>(bytes.size());
    return true;
}

bool treasure_query(void* user, std::uint8_t number) {
    return static_cast<items::PspItemContext*>(user)
        ->is_treasure_open(number);
}

bool run_test(const std::string& root, const char* scene_path) {
    const auto manifest = read_file(root + "/RESOURCE.MANIFEST");
    const auto scene_bytes = read_file(scene_path);
    room::PackageView scene = {};
    if (manifest.empty() || scene_bytes.empty() ||
        room::validate_dpsc(
            scene_bytes.data(),
            static_cast<std::uint32_t>(scene_bytes.size()),
            &scene) != room::PackageError::Ok) {
        return false;
    }

    g_processes.initialize();
    g_queue.initialize();
    g_world.initialize();
    g_events.initialize();
    g_interactions.initialize();
    g_items.initialize();
    if (!g_resources.initialize(
            ".", manifest.data(),
            static_cast<std::uint32_t>(manifest.size()),
            read_resource, const_cast<std::string*>(&root)) ||
        !g_models.initialize(&g_resources, &g_queue, &g_world)) {
        return false;
    }
    process::bind_process_manager(&g_processes);
    model::bind_model_runtime(&g_models);
    compat::bind_scene_exit_facade({
        &g_items, nullptr, nullptr, nullptr, nullptr, nullptr,
        treasure_query,
    });
    bool valid =
        compat::bind_original_tbox_context(
            &g_processes, &g_events, &g_interactions, &g_items) &&
        compat::register_original_tbox_profile(&g_processes) &&
        compat::original_tbox_profile_valid();

    process::ProcessHandle handles[2] = {};
    std::uint16_t created = 0;
    valid = valid && compat::create_original_tboxes(
        &g_processes, scene, 2, handles, 2, &created);
    const std::uint32_t model_errors_after_create =
        g_models.metrics.errors;
    if (!valid) {
        const std::uint32_t count = room::read_u32(scene.bytes + 136);
        for (std::uint32_t index = 0; index < count; ++index) {
            room::SceneActorV3 placement = {};
            if (room::read_dpsc_actor_v3(scene, index, &placement) ==
                    room::PackageError::Ok &&
                placement.name_hash == 0x2A0E83C6u) {
                std::fprintf(
                    stderr,
                    "placement index=%u process=0x%04X params=0x%08X "
                    "room=%u supported=%u rotation_z=0x%04X\n",
                    index, placement.process_id, placement.parameters,
                    placement.room, placement.supported,
                    static_cast<std::uint16_t>(
                        placement.rotation[2]));
            }
        }
    }
    daTbox_c* heart = nullptr;
    for (std::uint16_t index = 0; index < created; ++index) {
        auto* actor = static_cast<daTbox_c*>(
            g_processes.instance(handles[index]));
        if (actor != nullptr &&
            actor->getItemNo() == dItemNo_KAKERA_HEART_e) {
            heart = actor;
        }
    }
    if (heart == nullptr) {
        valid = false;
    }

    if (valid) {
        const float player[3] = {
            heart->current.pos.x,
            heart->current.pos.y,
            heart->current.pos.z,
        };
        compat::set_original_tbox_player_position(player);
        valid = g_processes.execute_all() &&
                compat::sample_original_tbox_interaction(
                    &g_processes, false) &&
                g_interactions.available() &&
                compat::sample_original_tbox_interaction(
                    &g_processes, true) &&
                g_processes.execute_all() &&
                g_processes.execute_all();
    }

    g_queue.begin_frame();
    valid = valid && g_processes.draw_all() &&
            created == 2 &&
            g_items.quantity(dItemNo_KAKERA_HEART_e) == 1 &&
            g_items.is_treasure_open(19) &&
            g_events.state() == events::State::None &&
            g_queue.size() == 2;
    const auto metrics = compat::original_tbox_metrics();
    valid = valid && metrics.source_profile_valid &&
            metrics.parameters_preserved &&
            metrics.placements_seen == 2 &&
            metrics.placements_created == 2 &&
            metrics.interactions_accepted == 1 &&
            metrics.items_created == 1 &&
            metrics.events_completed == 1 &&
            g_processes.metrics.errors == 0 &&
            g_models.metrics.errors == 0;
    if (!valid) {
        std::fprintf(
            stderr,
            "created=%u heart=%p placements=%u/%u parameters=%u "
            "requests=%u accepted=%u items=%u events=%u state=%u "
            "quantity=%u treasure=%u queue=%u process_errors=%u "
            "model_errors=%u\n",
            created, static_cast<void*>(heart),
            metrics.placements_seen, metrics.placements_created,
            metrics.parameters_preserved ? 1u : 0u,
            metrics.interaction_requests,
            metrics.interactions_accepted, metrics.items_created,
            metrics.events_completed,
            static_cast<unsigned>(g_events.state()),
            g_items.quantity(dItemNo_KAKERA_HEART_e),
            g_items.is_treasure_open(19) ? 1u : 0u,
            g_queue.size(), g_processes.metrics.errors,
            g_models.metrics.errors);
        std::fprintf(
            stderr,
            "model_errors_after_create=%u invalid_model=%u "
            "missing_owner=%u missing_arena=%u heap_overflows=%u "
            "movebg=%u/%u/%u\n",
            model_errors_after_create,
            g_models.metrics.invalid_model_data,
            g_models.metrics.missing_model_owner,
            g_models.metrics.missing_actor_arena,
            g_models.metrics.actor_heap_overflows,
            g_models.metrics.movebg_creates,
            g_models.metrics.movebg_updates,
            g_models.metrics.movebg_deletes);
    }

    g_processes.destroy_room(2);
    valid = valid &&
            g_models.metrics.models_peak > 0 &&
            g_models.metrics.animation_players_peak > 0 &&
            g_models.metrics.animation_players_current == 0;
    compat::unbind_original_tbox_context();
    compat::unbind_scene_exit_facade();
    model::unbind_model_runtime();
    process::unbind_process_manager();
    g_models.shutdown();
    g_resources.shutdown();
    g_items.shutdown();
    g_interactions.shutdown();
    g_events.shutdown();
    g_world.shutdown();
    g_queue.shutdown();
    return valid;
}

}  // namespace

int main(int argc, char** argv) {
    if (argc != 3 || !run_test(argv[1], argv[2])) {
        std::fputs("ORIGINAL_TBOX_HOST_FAILED\n", stderr);
        return 1;
    }
    const auto& metrics = compat::original_tbox_metrics();
    std::printf(
        "ORIGINAL_TBOX_HOST_OK process_id=0x00FB placements=%u "
        "item=0x21 treasure=19 events=%u source_modified_lines=0\n",
        metrics.placements_created, metrics.events_completed);
    return 0;
}
