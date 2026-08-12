#include "dusk/psp/original_dynamic_actor_bridge.hpp"

#include "d/actor/d_a_obj_lv4gear.h"
#include "f_pc/f_pc_name.h"

#include <cmath>
#include <cstring>

extern const actor_process_profile_definition g_profile_Obj_Lv4Gear;

namespace dusk::psp::compat {
namespace {

constexpr std::uint8_t kPairingKey = 0xE1;
constexpr std::int16_t kPublishedSpinnerSpeed = 512;

const OriginalDynamicActorDescriptor kDescriptor = {
    "ORIGINAL_DYNAMIC_ACTOR_SPNGEAR_SELECTED",
    "ACTR", 19, "spnGear", 0x3E96B91Bu, 0x0183u,
    "fpcNm_Obj_Lv4Gear_e", "g_profile_Obj_Lv4Gear",
    "daObjLv4Gear_c",
    "dusklight-main/include/d/actor/d_a_obj_lv4gear.h",
    "dusklight-main/src/d/actor/d_a_obj_lv4gear.cpp",
    "P_Gear", 0x000000E1u,
    {325.0f, 380.0f, -7175.0f},
    {0, 0, 0}, {1.0f, 1.0f, 1.0f},
    9, 0,
};

alignas(daObjSwSpinner_c) std::uint8_t
    g_companion_storage[sizeof(daObjSwSpinner_c)] = {};
daObjSwSpinner_c* g_companion = nullptr;
process::PspProcessManager* g_manager = nullptr;
std::int8_t g_room = -1;
OriginalDynamicActorMetrics g_metrics = {};

bool finite_matrix(const Mtx& matrix) {
    for (const auto& row : matrix) {
        for (float value : row) {
            if (!std::isfinite(value)) {
                return false;
            }
        }
    }
    return true;
}

}  // namespace

bool register_original_dynamic_actor_profile(
    process::PspProcessManager* manager) {
    return manager != nullptr &&
           manager->register_profile(&g_profile_Obj_Lv4Gear);
}

bool activate_original_dynamic_actor_context(
    process::PspProcessManager* manager,
    const room::PackageView& scene,
    std::int8_t room_number) {
    deactivate_original_dynamic_actor_context(g_room);
    if (manager == nullptr || scene.bytes == nullptr ||
        room::read_u16(scene.bytes + 4) < 3) {
        return false;
    }
    room::SceneActorV3 companion = {};
    bool found_companion = false;
    bool found_gear = false;
    const std::uint32_t count = room::read_u32(scene.bytes + 136);
    for (std::uint32_t index = 0; index < count; ++index) {
        room::SceneActorV3 actor = {};
        if (room::read_dpsc_actor_v3(scene, index, &actor) !=
            room::PackageError::Ok) {
            return false;
        }
        if (actor.process_id == fpcNm_Obj_Lv4Gear_e &&
            actor.room == static_cast<std::uint8_t>(room_number) &&
            (actor.parameters & 0xFFu) == kPairingKey) {
            found_gear = true;
            if (actor.source_index ==
                kDescriptor.source_record_index) {
                g_metrics.record_mapping_valid =
                    actor.name_hash == kDescriptor.source_name_hash;
                g_metrics.params_preserved =
                    actor.parameters == kDescriptor.source_params &&
                    actor.position[0] ==
                        kDescriptor.source_position[0] &&
                    actor.position[1] ==
                        kDescriptor.source_position[1] &&
                    actor.position[2] ==
                        kDescriptor.source_position[2];
            }
        }
        if (!found_companion &&
            actor.process_id == fpcNm_Obj_SwSpinner_e &&
            actor.room == static_cast<std::uint8_t>(room_number) &&
            (actor.parameters & 0xFFu) == kPairingKey) {
            companion = actor;
            found_companion = true;
        }
    }
    if (!found_gear) {
        return room_number != 9;
    }
    if (!found_companion || !g_metrics.record_mapping_valid ||
        !g_metrics.params_preserved) {
        return false;
    }
    if (manager->profile_registered(fpcNm_Obj_SwSpinner_e)) {
        g_manager = manager;
        g_room = room_number;
        ++g_metrics.context_activations;
        ++g_metrics.companion_records;
        g_metrics.companion_active = false;
        return true;
    }
    std::memset(g_companion_storage, 0, sizeof(g_companion_storage));
    g_companion =
        reinterpret_cast<daObjSwSpinner_c*>(g_companion_storage);
    g_companion->parameters = companion.parameters;
    g_companion->process_id = companion.process_id;
    g_companion->home.roomNo = room_number;
    g_companion->current.roomNo = room_number;
    g_companion->home.pos.set(
        companion.position[0], companion.position[1],
        companion.position[2]);
    g_companion->current.pos = g_companion->home.pos;
    fpc_ProcID id = fpcM_ERROR_PROCESS_ID_e;
    if (!manager->add_external(
            g_companion, companion.process_id, room_number, &id)) {
        g_companion = nullptr;
        return false;
    }
    g_manager = manager;
    g_room = room_number;
    ++g_metrics.context_activations;
    ++g_metrics.companion_records;
    g_metrics.companion_active = true;
    return true;
}

void deactivate_original_dynamic_actor_context(
    std::int8_t room_number) {
    if (g_room == room_number) {
        if (g_manager != nullptr &&
            g_companion != nullptr &&
            g_manager->id_of(g_companion) !=
                fpcM_ERROR_PROCESS_ID_e) {
            g_manager->remove_external(g_companion);
        }
        g_companion = nullptr;
        g_manager = nullptr;
        g_room = -1;
        ++g_metrics.context_deactivations;
        g_metrics.companion_active = false;
    }
}

void update_original_dynamic_actor_context(
    std::uint32_t fixed_update, bool paused) {
    if (g_companion == nullptr) {
        return;
    }
    if (!paused) {
        const std::uint32_t phase = (fixed_update / 180u) & 1u;
        g_companion->duskPspSetRotSpeedY(
            phase == 0 ? kPublishedSpinnerSpeed
                       : static_cast<s16>(-kPublishedSpinnerSpeed));
        ++g_metrics.input_updates;
    }
}

bool sample_original_dynamic_actor(
    const process::PspProcessManager& manager, bool paused) {
    const auto* gear = static_cast<const daObjLv4Gear_c*>(
        manager.first_instance(fpcNm_Obj_Lv4Gear_e));
    if (gear == nullptr) {
        return g_room != 9;
    }
    const std::int16_t prior = g_metrics.rotation;
    const std::int16_t rotation = gear->duskPspRotation();
    if (g_metrics.rotation_samples == 0) {
        g_metrics.initial_rotation = rotation;
    } else if (rotation != prior) {
        ++g_metrics.rotation_updates;
        if (paused) {
            ++g_metrics.pause_violations;
        }
    }
    g_metrics.rotation = rotation;
    g_metrics.speed = gear->duskPspSpeed();
    g_metrics.target = gear->duskPspTarget();
    g_metrics.state_count = gear->duskPspCount();
    const bool moving = g_metrics.speed != 0;
    if (g_metrics.rotation_samples != 0 &&
        moving != g_metrics.moving) {
        ++g_metrics.state_transitions;
    }
    g_metrics.moving = moving;
    ++g_metrics.rotation_samples;
    if (paused) {
        ++g_metrics.pause_samples;
    }
    const J3DModel* model = gear->duskPspModel();
    g_metrics.matrix_valid =
        model != nullptr && finite_matrix(model->getBaseTRMtx());
    return g_metrics.matrix_valid && g_metrics.pause_violations == 0;
}

bool original_dynamic_actor_profile_valid() {
    return g_profile_Obj_Lv4Gear.base.base.name ==
               fpcNm_Obj_Lv4Gear_e &&
           g_profile_Obj_Lv4Gear.base.base.process_size ==
               sizeof(daObjLv4Gear_c) &&
           g_profile_Obj_Lv4Gear.sub_method != nullptr &&
           g_profile_Obj_Lv4Gear.base.priority ==
               fpcDwPi_Obj_Lv4Gear_e;
}

const process::Metrics* original_dynamic_actor_process_metrics(
    const process::PspProcessManager& manager) {
    return manager.profile_metrics(fpcNm_Obj_Lv4Gear_e);
}

const OriginalDynamicActorDescriptor&
original_dynamic_actor_descriptor() {
    return kDescriptor;
}

const OriginalDynamicActorMetrics&
original_dynamic_actor_metrics() {
    return g_metrics;
}

}  // namespace dusk::psp::compat
