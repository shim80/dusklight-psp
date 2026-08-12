#include "dusk/psp/interaction_runtime.hpp"
#include "dusk/psp/model_runtime.hpp"
#include "dusk/psp/original_scene_exit_bridge.hpp"
#include "dusk/psp/original_spinner_switch_bridge.hpp"
#include "dusk/psp/switch_runtime.hpp"
#include "f_pc/f_pc_name.h"

#include <cstdio>
#include <cstring>
#include <fstream>
#include <iterator>
#include <string>
#include <vector>

extern const actor_process_profile_definition g_profile_Obj_SwSpinner;
extern const actor_process_profile_definition g_profile_Obj_Lv4Gear;

namespace {

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
    const std::vector<std::uint8_t> bytes =
        read_file(*root + "/" + relative);
    if (bytes.empty() || bytes.size() > capacity) {
        std::fprintf(
            stderr, "resource_read_failed path=%s relative=%s\n",
            path, relative);
        return false;
    }
    std::memcpy(output, bytes.data(), bytes.size());
    *size = static_cast<std::uint32_t>(bytes.size());
    return true;
}

bool switch_query(void* user, std::int8_t room, std::uint8_t number) {
    return static_cast<dusk::psp::switches::PspSwitchSurface*>(user)
        ->is_switch(number, room);
}

void switch_on(void* user, std::int8_t room, std::uint8_t number) {
    static_cast<dusk::psp::switches::PspSwitchSurface*>(user)
        ->on_switch(number, room);
}

void switch_off(void* user, std::int8_t room, std::uint8_t number) {
    static_cast<dusk::psp::switches::PspSwitchSurface*>(user)
        ->off_switch(number, room);
}

bool create_selected(
    dusk::psp::process::PspProcessManager* manager,
    const dusk::psp::room::PackageView& scene) {
    const std::uint32_t count =
        dusk::psp::room::read_u32(scene.bytes + 136);
    std::uint32_t created = 0;
    for (std::uint32_t index = 0; index < count; ++index) {
        dusk::psp::room::SceneActorV3 actor = {};
        if (dusk::psp::room::read_dpsc_actor_v3(
                scene, index, &actor) !=
            dusk::psp::room::PackageError::Ok) {
            return false;
        }
        if (actor.process_id != fpcNm_Obj_SwSpinner_e &&
            actor.process_id != fpcNm_Obj_Lv4Gear_e) {
            continue;
        }
        dusk::psp::process::ProcessHandle handle = {};
        if (!manager->create({
                static_cast<std::int16_t>(actor.process_id),
                actor.parameters,
                {actor.position[0], actor.position[1], actor.position[2]},
                {actor.rotation[0], actor.rotation[1], actor.rotation[2]},
                {actor.scale[0], actor.scale[1], actor.scale[2]},
                9}, &handle)) {
            return false;
        }
        ++created;
    }
    return created == 6;
}

}  // namespace

int main(int argc, char** argv) {
    if (argc != 2) {
        return 1;
    }
    const std::string root = argv[1];
    const auto manifest = read_file(root + "/RESOURCE.MANIFEST");
    const auto scene_bytes =
        read_file(root + "/stages/D_MN10/R09/room.dpsc");
    dusk::psp::room::PackageView scene = {};
    if (manifest.empty() || scene_bytes.empty() ||
        dusk::psp::room::validate_dpsc(
            scene_bytes.data(),
            static_cast<std::uint32_t>(scene_bytes.size()),
            &scene) != dusk::psp::room::PackageError::Ok) {
        return 2;
    }
    dusk::psp::resources::PspResourceManager resources;
    dusk::psp::render::PspRenderQueue queue;
    dusk::psp::process::PspProcessManager processes;
    dusk::psp::movebg::PspMoveBgWorld movebg;
    dusk::psp::model::PspStaticModelRuntime models;
    dusk::psp::interaction::PspInteractionContext interactions;
    dusk::psp::switches::PspSwitchSurface switches;
    dusk::psp::stage::PersistentDemoState persistent = {};
    queue.initialize();
    processes.initialize();
    movebg.initialize();
    if (!resources.initialize(
            ".", manifest.data(),
            static_cast<std::uint32_t>(manifest.size()),
            read_resource, const_cast<std::string*>(&root)) ||
        !models.initialize(&resources, &queue, &movebg) ||
        !interactions.initialize() ||
        !switches.initialize(&persistent) ||
        !switches.enter_stage("D_MN10", 9) ||
        !processes.register_profile(&g_profile_Obj_SwSpinner) ||
        !processes.register_profile(&g_profile_Obj_Lv4Gear)) {
        return 3;
    }
    dusk::psp::model::bind_model_runtime(&models);
    dusk::psp::process::bind_process_manager(&processes);
    dusk::psp::compat::bind_scene_exit_facade({
        &switches, nullptr, switch_query, switch_on, switch_off,
        nullptr, nullptr});
    bool valid =
        create_selected(&processes, scene) &&
        dusk::psp::compat::activate_original_spinner_switch(
            &processes, &interactions, &switches, scene, 9);
    for (std::uint32_t frame = 0; valid && frame < 400; ++frame) {
        queue.begin_frame();
        valid =
            dusk::psp::compat::update_original_spinner_switch(
                false, true) &&
            processes.execute_all() &&
            dusk::psp::compat::sample_original_spinner_switch() &&
            processes.draw_all();
    }
    const auto metrics =
        dusk::psp::compat::original_spinner_switch_metrics();
    valid = valid && metrics.spinner_in_frames > 200 &&
            metrics.rotation_frames > 200 &&
            metrics.switch_activations > 0 &&
            metrics.mechanism_changes > 0 &&
            switches.is_switch(0xE1, 9);
    dusk::psp::compat::deactivate_original_spinner_switch(9);
    processes.destroy_room(9);
    valid = valid && processes.active_count() == 0 &&
            models.reference_count() == 0 &&
            models.metrics.models_created ==
                models.metrics.models_destroyed &&
            models.metrics.movebg_creates ==
                models.metrics.movebg_deletes &&
            models.metrics.errors == 0 &&
            movebg.metrics.creates == movebg.metrics.deletes;
    dusk::psp::compat::unbind_scene_exit_facade();
    dusk::psp::process::unbind_process_manager();
    dusk::psp::model::unbind_model_runtime();
    models.shutdown();
    movebg.shutdown();
    processes.shutdown();
    interactions.shutdown();
    switches.shutdown();
    queue.shutdown();
    resources.shutdown();
    if (!valid) {
        std::fprintf(
            stderr,
            "spinner_in=%u rotation=%u switch=%u mechanism=%u "
            "model_errors=%u process_errors=%u movebg=%u/%u "
            "heap_overflow=%u missing_arena=%u resources=%u "
            "active=%u\n",
            metrics.spinner_in_frames, metrics.rotation_frames,
            metrics.switch_activations, metrics.mechanism_changes,
            models.metrics.errors, processes.metrics.errors,
            movebg.metrics.creates, movebg.metrics.deletes,
            models.metrics.actor_heap_overflows,
            models.metrics.missing_actor_arena,
            resources.metrics.errors, processes.active_count());
        return 4;
    }
    std::printf(
        "ORIGINAL_SPINNER_SWITCH_HOST_OK process_id=0x00B3 "
        "switch=0xE1 spinner_in=%u mechanism_changes=%u\n",
        metrics.spinner_in_frames, metrics.mechanism_changes);
    return 0;
}
