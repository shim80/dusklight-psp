#include "dusk/psp/original_rendered_actor_bridge.hpp"

#include "d/actor/d_a_obj_lv4HsTarget.h"
#include "f_pc/f_pc_name.h"

extern const actor_process_profile_definition g_profile_Obj_Lv4HsTarget;

namespace dusk::psp::compat {
namespace {

const OriginalRenderedActorDescriptor kDescriptor = {
    "ACTR", 26, "L4hmato", 0xDD2F1294u, 0x009Fu,
    "fpcNm_Obj_Lv4HsTarget_e",
    "g_profile_Obj_Lv4HsTarget",
    "daLv4HsTarget_c",
    "dusklight-main/include/d/actor/d_a_obj_lv4HsTarget.h",
    "dusklight-main/src/d/actor/d_a_obj_lv4HsTarget.cpp",
    "L4HsMato", 0xFFFFFFFFu,
    {1445.0f, 1200.0f, -7240.0f},
    {0, 0, 0}, {1.0f, 1.0f, 1.0f},
    9, 0, 12, 97,
};

}  // namespace

bool register_original_rendered_actor_profiles(
    process::PspProcessManager* manager) {
    return manager != nullptr &&
           manager->register_profile(&g_profile_Obj_Lv4HsTarget);
}

bool create_registered_room_actors(
    process::PspProcessManager* manager,
    const room::PackageView& scene,
    std::int8_t room_number,
    std::uint16_t* created_count) {
    if (manager == nullptr || created_count == nullptr ||
        scene.bytes == nullptr || room::read_u16(scene.bytes + 4) < 3) {
        return false;
    }
    *created_count = 0;
    const std::uint32_t count = room::read_u32(scene.bytes + 136);
    for (std::uint32_t index = 0; index < count; ++index) {
        room::SceneActorV3 actor = {};
        if (room::read_dpsc_actor_v3(
                scene, index, &actor) != room::PackageError::Ok) {
            return false;
        }
        if (actor.supported == 0 ||
            !manager->profile_registered(
                static_cast<std::int16_t>(actor.process_id))) {
            continue;
        }
        // Real treasure chests need their bounded event/item context and are
        // instantiated by original_tbox_bridge after validating the exact
        // source variant. The generic path must not duplicate them.
        if (actor.process_id == fpcNm_TBOX_e) {
            continue;
        }
        const process::CreateInput input = {
            static_cast<std::int16_t>(actor.process_id),
            actor.parameters,
            {actor.position[0], actor.position[1], actor.position[2]},
            {actor.rotation[0], actor.rotation[1], actor.rotation[2]},
            {actor.scale[0], actor.scale[1], actor.scale[2]},
            room_number,
            actor.table_hash,
            actor.source_index,
            actor.name_hash,
            static_cast<std::int8_t>(actor.layer),
        };
        process::ProcessHandle handle = {};
        if (!manager->create(input, &handle)) {
            return false;
        }
        ++*created_count;
    }
    return *created_count != 0;
}

bool original_rendered_actor_profile_valid() {
    return g_profile_Obj_Lv4HsTarget.base.base.name ==
               fpcNm_Obj_Lv4HsTarget_e &&
           g_profile_Obj_Lv4HsTarget.base.base.process_size ==
               sizeof(daLv4HsTarget_c) &&
           g_profile_Obj_Lv4HsTarget.sub_method != nullptr &&
           g_profile_Obj_Lv4HsTarget.base.priority ==
               fpcDwPi_Obj_Lv4HsTarget_e;
}

const process::Metrics* original_rendered_actor_process_metrics(
    const process::PspProcessManager& manager) {
    return manager.profile_metrics(fpcNm_Obj_Lv4HsTarget_e);
}

const OriginalRenderedActorDescriptor&
original_rendered_actor_descriptor() {
    return kDescriptor;
}

}  // namespace dusk::psp::compat
