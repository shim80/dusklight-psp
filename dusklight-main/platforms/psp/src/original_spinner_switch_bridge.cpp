#include "dusk/psp/original_spinner_switch_bridge.hpp"

#include "d/actor/d_a_obj_lv4gear.h"
#include "d/actor/d_a_obj_swspinner.h"
#include "d/actor/d_a_spinner.h"
#include "f_pc/f_pc_name.h"

#include <new>

extern const actor_process_profile_definition g_profile_Obj_SwSpinner;

namespace dusk::psp::compat {
namespace {

constexpr std::uint16_t kSourceRecord = 15;
constexpr std::uint32_t kSourceParameters = 0x00004EE1u;
constexpr std::uint32_t kSourceNameHash = 0xC85729E3u;
constexpr std::uint8_t kPrimarySwitch = 0xE1;
constexpr std::int16_t kAngleStep = 4096;

alignas(daSpinner_c)
std::uint8_t g_spinner_storage[sizeof(daSpinner_c)] = {};
daSpinner_c* g_spinner = nullptr;
process::PspProcessManager* g_manager = nullptr;
interaction::PspInteractionContext* g_interactions = nullptr;
switches::PspSwitchSurface* g_switches = nullptr;
std::int8_t g_room = -1;
std::int16_t g_prior_gear_rotation = 0;
OriginalSpinnerSwitchMetrics g_metrics = {};

}  // namespace

bool activate_original_spinner_switch(
    process::PspProcessManager* manager,
    interaction::PspInteractionContext* interactions,
    switches::PspSwitchSurface* switch_surface,
    const room::PackageView& scene,
    std::int8_t room_number) {
    deactivate_original_spinner_switch(g_room);
    if (manager == nullptr || interactions == nullptr ||
        switch_surface == nullptr || !interactions->initialized() ||
        !manager->profile_registered(fpcNm_Obj_SwSpinner_e)) {
        return false;
    }
    room::SceneActorV3 selected = {};
    bool found = false;
    const std::uint32_t count = room::read_u32(scene.bytes + 136);
    for (std::uint32_t index = 0; index < count; ++index) {
        room::SceneActorV3 actor = {};
        if (room::read_dpsc_actor_v3(scene, index, &actor) !=
            room::PackageError::Ok) {
            return false;
        }
        if (actor.source_index == kSourceRecord &&
            actor.process_id == fpcNm_Obj_SwSpinner_e &&
            actor.room == static_cast<std::uint8_t>(room_number)) {
            selected = actor;
            found = true;
            break;
        }
    }
    if (!found) {
        return room_number != 9;
    }
    g_metrics.record_mapping_valid =
        selected.name_hash == kSourceNameHash &&
        selected.parameters == kSourceParameters;
    g_metrics.profile_valid = original_spinner_switch_profile_valid();
    if (!g_metrics.record_mapping_valid || !g_metrics.profile_valid ||
        manager->first_instance(fpcNm_Obj_SwSpinner_e) == nullptr) {
        return false;
    }
    g_spinner = new (g_spinner_storage) daSpinner_c;
    g_spinner->process_id = fpcNm_SPINNER_e;
    g_spinner->home.roomNo = room_number;
    g_spinner->current.roomNo = room_number;
    g_spinner->home.pos.set(
        selected.position[0], selected.position[1],
        selected.position[2]);
    g_spinner->current.pos = g_spinner->home.pos;
    fpc_ProcID id = fpcM_ERROR_PROCESS_ID_e;
    if (!manager->add_external(
            g_spinner, fpcNm_SPINNER_e, room_number, &id)) {
        g_spinner->~daSpinner_c();
        g_spinner = nullptr;
        return false;
    }
    g_manager = manager;
    g_interactions = interactions;
    g_switches = switch_surface;
    g_room = room_number;
    g_metrics.source_record_index = kSourceRecord;
    g_metrics.source_parameters = kSourceParameters;
    g_metrics.facade_active = true;
    ++g_metrics.context_activations;
    return true;
}

void deactivate_original_spinner_switch(std::int8_t room_number) {
    if (g_room != room_number) {
        return;
    }
    if (g_spinner != nullptr) {
        if (g_manager != nullptr &&
            g_manager->id_of(g_spinner) != fpcM_ERROR_PROCESS_ID_e) {
            g_manager->remove_external(g_spinner);
        }
        g_spinner->~daSpinner_c();
    }
    g_spinner = nullptr;
    g_manager = nullptr;
    g_interactions = nullptr;
    g_switches = nullptr;
    g_room = -1;
    g_metrics.facade_active = false;
    ++g_metrics.context_deactivations;
}

bool update_original_spinner_switch(
    bool paused, bool action_pressed) {
    if (g_spinner == nullptr || g_interactions == nullptr) {
        return g_room != 9;
    }
    g_interactions->begin_frame();
    if (!g_interactions->publish({
            g_manager->first_instance(fpcNm_Obj_SwSpinner_e),
            "Spin", interaction::ActionType::Spin,
            interaction::Button::Cross, 0.0f, 0, 10, paused})) {
        return false;
    }
    ++g_metrics.prompt_frames;
    if (paused || !action_pressed) {
        return true;
    }
    if (!g_interactions->accept(interaction::Button::Cross)) {
        return false;
    }
    g_metrics.published_angle =
        static_cast<std::int16_t>(
            g_metrics.published_angle + kAngleStep);
    g_spinner->duskPspPublish(
        g_metrics.published_angle,
        daSpinner_c::TAG_INTO_INC_ROT);
    g_interactions->complete();
    ++g_metrics.input_frames;
    ++g_metrics.accepted_frames;
    return true;
}

bool sample_original_spinner_switch() {
    if (g_room != 9) {
        return true;
    }
    const auto* spinner_switch =
        static_cast<const daObjSwSpinner_c*>(
            g_manager->first_instance(fpcNm_Obj_SwSpinner_e));
    const auto* gear = static_cast<const daObjLv4Gear_c*>(
        g_manager->first_instance(fpcNm_Obj_Lv4Gear_e));
    if (spinner_switch == nullptr || gear == nullptr ||
        g_switches == nullptr) {
        return false;
    }
    ++g_metrics.original_execute_samples;
    if (spinner_switch->duskPspSpinnerIn()) {
        ++g_metrics.spinner_in_frames;
    }
    if (spinner_switch->duskPspRotationSpeed() != 0) {
        ++g_metrics.rotation_frames;
    }
    const bool active =
        g_switches->is_switch(kPrimarySwitch, g_room);
    if (active && !g_metrics.source_switch_active) {
        ++g_metrics.switch_activations;
    }
    g_metrics.source_switch_active = active;
    if (active) {
        ++g_metrics.post_switch_frames;
    }
    const std::int16_t gear_rotation = gear->duskPspRotation();
    if (gear_rotation != g_prior_gear_rotation) {
        ++g_metrics.mechanism_changes;
    }
    g_prior_gear_rotation = gear_rotation;
    return spinner_switch->duskPspBaseModel() != nullptr &&
           spinner_switch->duskPspTopModel() != nullptr;
}

bool original_spinner_switch_profile_valid() {
    return g_profile_Obj_SwSpinner.base.base.name ==
               fpcNm_Obj_SwSpinner_e &&
           g_profile_Obj_SwSpinner.base.base.process_size ==
               sizeof(daObjSwSpinner_c) &&
           g_profile_Obj_SwSpinner.sub_method != nullptr &&
           g_profile_Obj_SwSpinner.base.priority ==
               fpcDwPi_Obj_SwSpinner_e;
}

const OriginalSpinnerSwitchMetrics&
original_spinner_switch_metrics() {
    return g_metrics;
}

}  // namespace dusk::psp::compat
