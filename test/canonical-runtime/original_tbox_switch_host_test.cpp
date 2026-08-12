#include "d/actor/d_a_tboxSw.h"
#include "dusk/psp/item_runtime.hpp"
#include "dusk/psp/original_scene_exit_bridge.hpp"
#include "dusk/psp/process_runtime.hpp"
#include "dusk/psp/switch_runtime.hpp"
#include "f_pc/f_pc_name.h"

#include <cstdio>

extern const actor_process_profile_definition g_profile_TBOX_SW;

namespace {

struct Context {
    dusk::psp::items::PspItemContext items;
    dusk::psp::switches::PspSwitchSurface switches;
    dusk::psp::items::ItemAcquisition last_acquisition = {};
    std::uint32_t hud_updates = 0;
};

void item_acquired(
    void* user,
    const dusk::psp::items::ItemAcquisition& acquisition) {
    auto* context = static_cast<Context*>(user);
    context->last_acquisition = acquisition;
    ++context->hud_updates;
}

bool no_transition(
    void*, std::uint8_t, std::uint8_t, bool, const float[3]) {
    return false;
}

bool query_switch(void* user, std::int8_t room, std::uint8_t number) {
    return static_cast<Context*>(user)->switches.is_switch(number, room);
}

void write_switch(void* user, std::int8_t room, std::uint8_t number) {
    static_cast<Context*>(user)->switches.on_switch(number, room);
}

void clear_switch(void* user, std::int8_t room, std::uint8_t number) {
    static_cast<Context*>(user)->switches.off_switch(number, room);
}

bool no_event(void*, std::uint16_t) {
    return false;
}

bool query_treasure(void* user, std::uint8_t number) {
    return static_cast<Context*>(user)->items.is_treasure_open(number);
}

}  // namespace

int main() {
    dusk::psp::stage::PersistentDemoState persistent = {};
    Context context = {};
    if (!context.items.initialize() ||
        !context.switches.initialize(&persistent) ||
        !context.switches.enter_stage("D_MN10", 0) ||
        context.items.is_treasure_open(-1) ||
        context.items.set_treasure_open(64) ||
        context.items.acquire(-1, 1, &context) ||
        context.items.acquire(0x23, 0, &context) ||
        context.items.acquire(0x23, 1, nullptr)) {
        return 1;
    }
    context.items.set_acquired_callback(item_acquired, &context);

    dusk::psp::process::PspProcessManager manager;
    manager.initialize();
    dusk::psp::process::bind_process_manager(&manager);
    dusk::psp::compat::bind_scene_exit_facade({
        &context, no_transition, query_switch, write_switch,
        clear_switch, no_event, query_treasure});

    if (!manager.register_profile(&g_profile_TBOX_SW) ||
        !manager.profile_registered(fpcNm_TBOX_SW_e)) {
        return 2;
    }

    constexpr std::uint8_t kTreasure = 0x12;
    constexpr std::uint8_t kSwitch = 0x34;
    const dusk::psp::process::CreateInput input = {
        fpcNm_TBOX_SW_e,
        static_cast<std::uint32_t>(kTreasure) |
            (static_cast<std::uint32_t>(kSwitch) << 8u),
        {0.0f, 0.0f, 0.0f},
        {0, 0, 0},
        {1.0f, 1.0f, 1.0f},
        0,
        0,
        0,
        0,
        0,
    };
    dusk::psp::process::ProcessHandle handle = {};
    if (!manager.create(input, &handle) ||
        !manager.execute_all() ||
        !manager.handle_valid(handle) ||
        context.switches.is_switch(kSwitch, 0)) {
        return 3;
    }

    if (!context.items.set_treasure_open(kTreasure) ||
        !manager.execute_all() ||
        manager.handle_valid(handle) ||
        manager.active_count() != 0 ||
        !context.switches.is_switch(kSwitch, 0) ||
        manager.metrics.delete_calls != 1 ||
        manager.metrics.calls_after_delete != 0 ||
        manager.metrics.errors != 0) {
        return 4;
    }

    const void* source_actor = &context;
    if (!context.items.acquire(0x23, 1, source_actor) ||
        !context.items.acquire(0x23, 4, source_actor) ||
        context.items.quantity(0x23) != 5 ||
        context.hud_updates != 2 ||
        context.last_acquisition.item_id != 0x23 ||
        context.last_acquisition.quantity != 4 ||
        context.last_acquisition.total != 5 ||
        context.last_acquisition.source_actor != source_actor) {
        return 5;
    }
    context.items.set_acquired_callback(nullptr, nullptr);
    if (!context.items.acquire(0x24, 65535, source_actor) ||
        context.items.acquire(0x24, 1, source_actor) ||
        context.items.quantity(0x24) != 65535 ||
        context.items.metrics.quantity_overflows != 1) {
        return 6;
    }

    dusk::psp::process::ProcessHandle reopened_handle = {};
    if (!context.items.set_treasure_open(kTreasure) ||
        context.items.metrics.duplicate_writes != 1 ||
        context.items.metrics.invalid_requests != 5 ||
        context.items.metrics.acquisitions != 3 ||
        context.items.metrics.quantity_added != 65540 ||
        context.items.metrics.hud_notifications != 2 ||
        !context.switches.reload_room(0) ||
        !manager.create(input, &reopened_handle) ||
        !manager.execute_all() ||
        manager.handle_valid(reopened_handle) ||
        manager.active_count() != 0 ||
        manager.metrics.delete_calls != 2 ||
        manager.metrics.errors != 0) {
        return 7;
    }

    dusk::psp::compat::unbind_scene_exit_facade();
    dusk::psp::process::unbind_process_manager();
    manager.shutdown();
    context.switches.shutdown();
    context.items.shutdown();
    std::puts(
        "ORIGINAL_TBOX_SWITCH_HOST_OK process_id=0x016E "
        "treasure_state=true already_open=true switch_write=true "
        "item_quantity=5 hud_notifications=2 persistence=true "
        "deferred_delete=true negative_cases=6 "
        "source_modified_lines=0");
    return 0;
}
