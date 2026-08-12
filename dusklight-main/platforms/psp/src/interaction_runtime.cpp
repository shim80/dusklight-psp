#include "dusk/psp/interaction_runtime.hpp"

#include <cmath>

namespace dusk::psp::interaction {

bool PspInteractionContext::initialize() {
    selected_ = {};
    result_ = Result::None;
    metrics = {};
    available_ = false;
    initialized_ = true;
    return true;
}

void PspInteractionContext::shutdown() {
    selected_ = {};
    result_ = Result::None;
    available_ = false;
    initialized_ = false;
}

void PspInteractionContext::begin_frame() {
    if (!initialized_) {
        return;
    }
    selected_ = {};
    result_ = Result::None;
    available_ = false;
    ++metrics.frames;
}

bool PspInteractionContext::publish(const Candidate& candidate) {
    if (!initialized_ || candidate.actor == nullptr ||
        candidate.prompt == nullptr || candidate.prompt[0] == '\0' ||
        candidate.action == ActionType::None ||
        candidate.button == Button::None ||
        !std::isfinite(candidate.distance) ||
        candidate.distance < 0.0f) {
        ++metrics.invalid_candidates;
        return false;
    }
    ++metrics.candidates_published;
    if (available_ &&
        (candidate.priority < selected_.priority ||
         (candidate.priority == selected_.priority &&
          candidate.distance >= selected_.distance))) {
        return true;
    }
    if (available_) {
        ++metrics.candidates_replaced;
    }
    selected_ = candidate;
    result_ = Result::Available;
    available_ = true;
    return true;
}

bool PspInteractionContext::accept(Button pressed) {
    if (!initialized_ || !available_ || selected_.input_lock ||
        pressed != selected_.button) {
        ++metrics.actions_rejected;
        result_ = Result::Rejected;
        return false;
    }
    ++metrics.actions_accepted;
    result_ = Result::Accepted;
    return true;
}

void PspInteractionContext::complete() {
    if (initialized_ && result_ == Result::Accepted) {
        result_ = Result::Completed;
    }
}

bool PspInteractionContext::initialized() const {
    return initialized_;
}

bool PspInteractionContext::available() const {
    return initialized_ && available_;
}

const Candidate* PspInteractionContext::selected() const {
    return available() ? &selected_ : nullptr;
}

Result PspInteractionContext::result() const {
    return result_;
}

}  // namespace dusk::psp::interaction
