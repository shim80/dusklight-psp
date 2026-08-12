#include "dusk/psp/event_runtime.hpp"

#include <cstdio>

using dusk::psp::events::Kind;
using dusk::psp::events::PspEventContext;
using dusk::psp::events::State;

namespace {

bool run_test() {
    PspEventContext context;
    int actor_a = 0;
    int actor_b = 0;
    if (!context.initialize() ||
        context.accept(&actor_a) ||
        !context.request({&actor_a, 7, 0xFF, Kind::Item}) ||
        context.request({&actor_b, 8, 0xFF, Kind::Door}) ||
        context.start(&actor_a) ||
        !context.accept(&actor_a) ||
        context.set_item_partner(0xFFFFFFFFu) ||
        !context.set_item_partner(42) ||
        !context.start(&actor_a) ||
        context.complete(&actor_b) ||
        !context.complete(&actor_a) ||
        !context.completed(7) ||
        context.completed(8) ||
        context.item_partner() != 42 ||
        !context.reset() ||
        context.state() != State::None ||
        !context.request({&actor_b, -1, 3, Kind::Interaction}) ||
        !context.cancel(&actor_b) ||
        !context.reset() ||
        context.metrics.requests != 2 ||
        context.metrics.accepts != 1 ||
        context.metrics.starts != 1 ||
        context.metrics.completions != 1 ||
        context.metrics.cancellations != 1 ||
        context.metrics.competing_requests != 1 ||
        context.metrics.invalid_transitions != 4) {
        return false;
    }
    context.shutdown();
    return !context.initialized();
}

}  // namespace

int main() {
    if (!run_test()) {
        std::fputs("EVENT_SURFACE_HOST_FAILED\n", stderr);
        return 1;
    }
    std::printf(
        "EVENT_SURFACE_HOST_OK capacity=1 states=6 "
        "competing_requests=1 invalid_transitions=4\n");
    return 0;
}
