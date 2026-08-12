#include "dusk/psp/switch_runtime.hpp"

#include <cstdio>

namespace {

bool run_test() {
    dusk::psp::stage::PersistentDemoState persistent = {
        3, 3, 17, 42, false};
    dusk::psp::switches::PspSwitchSurface surface;
    if (!surface.initialize(&persistent) ||
        !surface.enter_stage("D_MN10", 9)) {
        return false;
    }

    if (surface.is_switch(0, 9) ||
        !surface.on_switch(0, 9) ||
        !surface.is_switch(0, 9) ||
        !surface.off_switch(0, 9) ||
        surface.is_switch(0, 9)) {
        return false;
    }
    if (surface.is_switch(0xFF, 9) ||
        !surface.on_switch(0xFF, 9) ||
        surface.is_switch(0xFF, 9) ||
        surface.is_switch(0xF0, 9) ||
        surface.on_switch(-2, 9)) {
        return false;
    }

    if (!surface.on_switch(0, 9) ||
        !surface.on_switch(0xC0, 9) ||
        !surface.is_switch(0xC0, 9) ||
        surface.is_switch(0xC0, 2) ||
        !surface.transition_to_room(2) ||
        !surface.on_switch(0xC1, 2) ||
        !surface.is_switch(0xC1, 2) ||
        !surface.is_switch(0, 2) ||
        !surface.transition_to_room(9) ||
        !surface.is_switch(0xC0, 9) ||
        surface.is_switch(0xC1, 9)) {
        return false;
    }

    if (!surface.reload_room(9) ||
        !surface.is_switch(0xC0, 9) ||
        !surface.reset_room(9) ||
        surface.is_switch(0xC0, 9) ||
        !surface.is_switch(0, 9)) {
        return false;
    }

    if (!surface.on_event_bit(0x1204) ||
        !surface.is_event_bit(0x1204) ||
        !surface.off_event_bit(0x1204) ||
        surface.is_event_bit(0x1204)) {
        return false;
    }

    if (!surface.on_switch(0xC2, 9) ||
        !surface.on_event_bit(0x2208)) {
        return false;
    }
    persistent.rupees = 23;
    if (!surface.enter_stage("F_SP00", 0) ||
        surface.is_switch(0, 0) ||
        surface.is_switch(0xC2, 9) ||
        !surface.is_event_bit(0x2208) ||
        surface.persistent_state() != &persistent ||
        surface.persistent_state()->rupees != 23) {
        return false;
    }
    surface.shutdown();
    return !surface.initialized();
}

}  // namespace

int main() {
    if (!run_test()) {
        std::fputs("SWITCH_SURFACE_HOST_FAILED\n", stderr);
        return 1;
    }
    std::printf(
        "SWITCH_SURFACE_HOST_OK stage_scope=true room_scope=true "
        "event_persistent=true sentinel_ignored=true\n");
    return 0;
}
