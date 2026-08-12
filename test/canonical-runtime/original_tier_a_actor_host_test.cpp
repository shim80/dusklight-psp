#include "d/actor/d_a_tag_poFire.h"
#include "dusk/psp/process_runtime.hpp"
#include "f_pc/f_pc_name.h"

#include <cstdio>

extern const actor_process_profile_definition g_profile_Tag_poFire;

int main() {
    dusk::psp::process::PspProcessManager manager;
    manager.initialize();
    dusk::psp::process::bind_process_manager(&manager);
    if (!manager.register_profile(&g_profile_Tag_poFire) ||
        !manager.profile_registered(fpcNm_Tag_poFire_e)) {
        return 1;
    }
    const dusk::psp::process::CreateInput input = {
        fpcNm_Tag_poFire_e,
        0xFFu,
        {4162.5f, -250.0f, -3375.0f},
        {0, 0, 0},
        {1.0f, 1.0f, 1.0f},
        3,
        0x49CDF43Bu,
        7,
        0x01020304u,
        2,
    };
    dusk::psp::process::ProcessHandle handle = {};
    if (!manager.create(input, &handle) || !manager.handle_valid(handle) ||
        !manager.execute_all() || !manager.draw_all()) {
        return 2;
    }
    if (manager.lifecycle_event_count() != 4 ||
        manager.lifecycle_events_dropped() != 0) {
        return 5;
    }
    constexpr dusk::psp::process::ProcessLifecyclePhase kPhases[] = {
        dusk::psp::process::ProcessLifecyclePhase::CreateEnter,
        dusk::psp::process::ProcessLifecyclePhase::CreateExit,
        dusk::psp::process::ProcessLifecyclePhase::FirstExecuteEnter,
        dusk::psp::process::ProcessLifecyclePhase::FirstExecuteExit,
    };
    for (std::uint16_t index = 0; index < 4; ++index) {
        dusk::psp::process::ProcessLifecycleEvent event = {};
        if (!manager.lifecycle_event(index, &event) ||
            event.sequence != static_cast<std::uint32_t>(index + 1) ||
            event.phase != kPhases[index] ||
            event.process_id != fpcNm_Tag_poFire_e ||
            event.source_table_hash != 0x49CDF43Bu ||
            event.source_index != 7 ||
            event.source_name_hash != 0x01020304u ||
            event.room != 3 || event.layer != 2) {
            return 6;
        }
    }
    dusk::psp::process::ProcessObservation observation = {};
    if (!manager.observe_active(0, &observation) ||
        observation.actor != manager.instance(handle) ||
        observation.process_id != fpcNm_Tag_poFire_e ||
        observation.room != 3 || observation.layer != 2 ||
        observation.source_table_hash != 0x49CDF43Bu ||
        observation.source_index != 7 ||
        observation.source_name_hash != 0x01020304u ||
        observation.metrics.create_calls == 0 ||
        observation.metrics.execute_calls == 0 ||
        observation.metrics.draw_calls == 0 ||
        manager.observe_active(1, &observation)) {
        return 4;
    }
    auto* actor = static_cast<daTagPoFire_c*>(manager.instance(handle));
    actor->setFireFlag(1);
    if (!manager.execute_all() || manager.active_count() != 0 ||
        manager.metrics.delete_calls != 1 ||
        manager.metrics.calls_after_delete != 0 ||
        manager.metrics.errors != 0) {
        return 3;
    }
    dusk::psp::process::unbind_process_manager();
    manager.shutdown();
    std::puts(
        "ORIGINAL_TIER_A_ACTOR_HOST_OK process_id=0x017A "
        "self_delete=true source_modified_lines=0");
    return 0;
}
