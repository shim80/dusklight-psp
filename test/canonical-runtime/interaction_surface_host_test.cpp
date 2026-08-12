#include "dusk/psp/interaction_runtime.hpp"

#include <cstdio>

int main() {
    using namespace dusk::psp::interaction;
    PspInteractionContext context;
    int far_actor = 1;
    int near_actor = 2;
    if (!context.initialize()) {
        return 1;
    }
    context.begin_frame();
    if (!context.publish({
            &far_actor, "Spin", ActionType::Spin, Button::Cross,
            80.0f, 0, 1, false}) ||
        !context.publish({
            &near_actor, "Spin", ActionType::Spin, Button::Cross,
            20.0f, 0, 1, false}) ||
        context.selected() == nullptr ||
        context.selected()->actor != &near_actor ||
        !context.accept(Button::Cross) ||
        context.result() != Result::Accepted) {
        return 2;
    }
    context.complete();
    if (context.result() != Result::Completed ||
        context.metrics.candidates_published != 2 ||
        context.metrics.candidates_replaced != 1 ||
        context.metrics.actions_accepted != 1) {
        return 3;
    }
    context.begin_frame();
    if (context.available() ||
        context.publish({
            nullptr, "Invalid", ActionType::Use, Button::Cross,
            1.0f, 0, 1, false}) ||
        context.metrics.invalid_candidates != 1) {
        return 4;
    }
    context.shutdown();
    std::puts(
        "INTERACTION_SURFACE_HOST_OK "
        "selection=priority_then_distance source_logic_owner=actor");
    return 0;
}
