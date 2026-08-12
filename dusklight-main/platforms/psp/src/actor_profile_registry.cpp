#include "dusk/psp/actor_profile_registry.hpp"

#include "dusk/psp/original_dynamic_actor_bridge.hpp"
#include "dusk/psp/original_door_bridge.hpp"
#include "dusk/psp/original_rendered_actor_bridge.hpp"
#include "dusk/psp/original_scene_exit_bridge.hpp"
#include "dusk/psp/original_tbox_bridge.hpp"

extern const actor_process_profile_definition g_profile_Tag_poFire;
extern const actor_process_profile_definition g_profile_TBOX_SW;
extern const actor_process_profile_definition g_profile_Obj_SwSpinner;

namespace dusk::psp::compat {

bool register_all_original_actor_profiles(
    process::PspProcessManager* manager) {
    return manager != nullptr &&
           register_original_scene_exit_profile(manager) &&
           register_original_rendered_actor_profiles(manager) &&
           register_original_dynamic_actor_profile(manager) &&
           register_original_door_profile(manager) &&
           register_original_tbox_profile(manager) &&
           manager->register_profile(&g_profile_Obj_SwSpinner) &&
           manager->register_profile(&g_profile_Tag_poFire) &&
           manager->register_profile(&g_profile_TBOX_SW);
}

std::uint16_t original_actor_profile_count() {
    return 8;
}

}  // namespace dusk::psp::compat
