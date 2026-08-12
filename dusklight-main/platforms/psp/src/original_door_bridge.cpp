#include "dusk/psp/original_door_bridge.hpp"

#include "d/actor/d_a_obj_lv4PoGate.h"
#include "dusk/psp/model_runtime.hpp"
#include "dusk/psp/switch_runtime.hpp"
#include "f_pc/f_pc_name.h"

#include <cmath>

extern const actor_process_profile_definition g_profile_Obj_Lv4PoGate;

namespace dusk::psp::compat {
namespace {

const OriginalDoorDescriptor kDescriptor = {
    "ACTR",
    30,
    "L4Pgate",
    0x771AFAA0u,
    0x009Du,
    "fpcNm_Obj_Lv4PoGate_e",
    "g_profile_Obj_Lv4PoGate",
    "daLv4PoGate_c",
    "dusklight-main/include/d/actor/d_a_obj_lv4PoGate.h",
    "dusklight-main/src/d/actor/d_a_obj_lv4PoGate.cpp",
    "L4R02Gate",
    0x00000045u,
    {0.0f, 425.0f, -3450.0f},
    {0, 0, 0},
    2,
    0x45,
    4,
    7,
};

process::PspProcessManager* g_manager = nullptr;
switches::PspSwitchSurface* g_switches = nullptr;
std::int8_t g_room = -1;
bool g_closing = false;
bool g_opening = false;
OriginalDoorMetrics g_metrics = {};

bool close(float left, float right) {
    return std::fabs(left - right) < 0.05f;
}

bool matrices_match(
    const Mtx& source, const movebg::Matrix34& collision) {
    for (std::uint32_t row = 0; row < 3; ++row) {
        for (std::uint32_t column = 0; column < 4; ++column) {
            if (!close(
                    source[row][column],
                    collision.value[row][column])) {
                return false;
            }
        }
    }
    return true;
}

}  // namespace

bool register_original_door_profile(
    process::PspProcessManager* manager) {
    return manager != nullptr &&
           manager->register_profile(&g_profile_Obj_Lv4PoGate);
}

bool original_door_profile_valid() {
    return g_profile_Obj_Lv4PoGate.base.base.name ==
               fpcNm_Obj_Lv4PoGate_e &&
           g_profile_Obj_Lv4PoGate.base.base.process_size ==
               sizeof(daLv4PoGate_c) &&
           g_profile_Obj_Lv4PoGate.sub_method != nullptr &&
           g_profile_Obj_Lv4PoGate.base.priority ==
               fpcDwPi_Obj_Lv4PoGate_e;
}

const OriginalDoorDescriptor& original_door_descriptor() {
    return kDescriptor;
}

bool activate_original_door_validation(
    process::PspProcessManager* manager,
    switches::PspSwitchSurface* switch_surface,
    const room::PackageView& scene,
    std::int8_t room_number) {
    deactivate_original_door_validation(g_room);
    if (room_number != static_cast<std::int8_t>(kDescriptor.room)) {
        return true;
    }
    if (manager == nullptr || switch_surface == nullptr ||
        scene.bytes == nullptr) {
        return false;
    }
    bool placement_valid = false;
    const std::uint32_t count = room::read_u32(scene.bytes + 136);
    for (std::uint32_t index = 0; index < count; ++index) {
        room::SceneActorV3 placement = {};
        if (room::read_dpsc_actor_v3(scene, index, &placement) !=
            room::PackageError::Ok) {
            return false;
        }
        if (placement.source_index !=
            kDescriptor.source_record_index) {
            continue;
        }
        placement_valid =
            placement.supported == 1 &&
            placement.process_id == kDescriptor.process_id &&
            placement.name_hash == kDescriptor.source_name_hash;
        g_metrics.record_mapping_valid = placement_valid;
        g_metrics.parameters_preserved =
            placement.parameters == kDescriptor.parameters &&
            close(placement.position[0], kDescriptor.position[0]) &&
            close(placement.position[1], kDescriptor.position[1]) &&
            close(placement.position[2], kDescriptor.position[2]);
        break;
    }
    auto* actor = static_cast<daLv4PoGate_c*>(
        manager->first_instance(fpcNm_Obj_Lv4PoGate_e));
    if (!placement_valid || !g_metrics.parameters_preserved ||
        actor == nullptr ||
        actor->duskPspSwitch() != kDescriptor.switch_number ||
        !close(actor->duskPspMoveValue(), 1000.0f) ||
        actor->duskPspMode() != daLv4PoGate_c::MODE_WAIT_e ||
        !switch_surface->on_switch(
            kDescriptor.switch_number, room_number)) {
        return false;
    }
    g_manager = manager;
    g_switches = switch_surface;
    g_room = room_number;
    g_closing = true;
    g_opening = false;
    ++g_metrics.context_activations;
    ++g_metrics.switch_on_requests;
    g_metrics.active = true;
    g_metrics.visit_complete = false;
    return true;
}

bool sample_original_door_validation(
    process::PspProcessManager* manager,
    model::PspStaticModelRuntime* models,
    movebg::PspMoveBgWorld* world,
    std::int8_t room_number,
    bool paused) {
    if (room_number != static_cast<std::int8_t>(kDescriptor.room)) {
        return true;
    }
    if (!g_metrics.active || manager != g_manager ||
        models == nullptr || world == nullptr) {
        return false;
    }
    auto* actor = static_cast<daLv4PoGate_c*>(
        manager->first_instance(fpcNm_Obj_Lv4PoGate_e));
    if (actor == nullptr) {
        return false;
    }
    if (paused) {
        return true;
    }
    movebg::Handle handle = {};
    const J3DModel* source_model = actor->duskPspModel();
    if (source_model == nullptr ||
        !models->move_bg_handle(actor, &handle)) {
        return false;
    }
    const movebg::Matrix34* collision = world->matrix(handle);
    if (collision == nullptr ||
        !matrices_match(source_model->getBaseTRMtx(), *collision)) {
        ++g_metrics.matrix_mismatches;
        return false;
    }
    ++g_metrics.matrix_parity_samples;
    ++g_metrics.source_samples;
    if (g_closing &&
        close(actor->duskPspMoveValue(), 0.0f) &&
        actor->duskPspMode() == daLv4PoGate_c::MODE_WAIT_e) {
        if (!g_switches->off_switch(
                kDescriptor.switch_number, room_number)) {
            return false;
        }
        g_closing = false;
        g_opening = true;
        ++g_metrics.switch_off_requests;
        ++g_metrics.doors_closed;
    } else if (g_opening &&
               close(actor->duskPspMoveValue(), 1000.0f) &&
               actor->duskPspMode() ==
                   daLv4PoGate_c::MODE_WAIT_e) {
        g_opening = false;
        ++g_metrics.doors_opened;
        ++g_metrics.completed_cycles;
        g_metrics.visit_complete = true;
    }
    return true;
}

void deactivate_original_door_validation(
    std::int8_t room_number) {
    if (g_metrics.active && g_room == room_number) {
        g_manager = nullptr;
        g_switches = nullptr;
        g_room = -1;
        g_closing = false;
        g_opening = false;
        ++g_metrics.context_deactivations;
        g_metrics.active = false;
    }
}

bool original_door_transition_ready(std::int8_t room_number) {
    return room_number != static_cast<std::int8_t>(kDescriptor.room) ||
           g_metrics.visit_complete;
}

const OriginalDoorMetrics& original_door_metrics() {
    return g_metrics;
}

}  // namespace dusk::psp::compat
