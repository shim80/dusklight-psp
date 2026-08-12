#include "dusk/psp/event_runtime.hpp"

namespace dusk::psp::events {
namespace {

bool valid_request(const Request& request) {
    return request.source_actor != nullptr &&
           request.event_id >= -1 &&
           static_cast<std::uint8_t>(request.kind) <=
               static_cast<std::uint8_t>(Kind::Transition);
}

}  // namespace

bool PspEventContext::initialize() {
    active_ = {};
    state_ = State::None;
    item_partner_ = 0xFFFFFFFFu;
    metrics = {};
    initialized_ = true;
    return true;
}

void PspEventContext::shutdown() {
    active_ = {};
    state_ = State::None;
    item_partner_ = 0xFFFFFFFFu;
    initialized_ = false;
}

bool PspEventContext::request(const Request& request_value) {
    if (!initialized_ || !valid_request(request_value)) {
        ++metrics.invalid_transitions;
        return false;
    }
    if (state_ != State::None &&
        state_ != State::Completed &&
        state_ != State::Cancelled) {
        ++metrics.competing_requests;
        return false;
    }
    active_ = request_value;
    state_ = State::Requested;
    item_partner_ = 0xFFFFFFFFu;
    ++metrics.requests;
    return true;
}

bool PspEventContext::accept(const void* source_actor) {
    if (!initialized_ || state_ != State::Requested ||
        source_actor != active_.source_actor) {
        ++metrics.invalid_transitions;
        return false;
    }
    state_ = State::Accepted;
    ++metrics.accepts;
    return true;
}

bool PspEventContext::start(const void* source_actor) {
    if (!initialized_ || state_ != State::Accepted ||
        source_actor != active_.source_actor) {
        ++metrics.invalid_transitions;
        return false;
    }
    state_ = State::Running;
    ++metrics.starts;
    return true;
}

bool PspEventContext::complete(const void* source_actor) {
    if (!initialized_ ||
        (state_ != State::Accepted && state_ != State::Running) ||
        source_actor != active_.source_actor) {
        ++metrics.invalid_transitions;
        return false;
    }
    state_ = State::Completed;
    ++metrics.completions;
    return true;
}

bool PspEventContext::cancel(const void* source_actor) {
    if (!initialized_ ||
        (state_ != State::Requested && state_ != State::Accepted &&
         state_ != State::Running) ||
        source_actor != active_.source_actor) {
        ++metrics.invalid_transitions;
        return false;
    }
    state_ = State::Cancelled;
    ++metrics.cancellations;
    return true;
}

bool PspEventContext::reset() {
    if (!initialized_ ||
        (state_ != State::None && state_ != State::Completed &&
         state_ != State::Cancelled)) {
        ++metrics.invalid_transitions;
        return false;
    }
    active_ = {};
    state_ = State::None;
    item_partner_ = 0xFFFFFFFFu;
    ++metrics.resets;
    return true;
}

bool PspEventContext::set_item_partner(std::uint32_t process_id) {
    if (!initialized_ ||
        (state_ != State::Accepted && state_ != State::Running) ||
        process_id == 0xFFFFFFFFu) {
        ++metrics.invalid_transitions;
        return false;
    }
    item_partner_ = process_id;
    ++metrics.item_partner_writes;
    return true;
}

bool PspEventContext::initialized() const {
    return initialized_;
}

State PspEventContext::state() const {
    return state_;
}

const Request& PspEventContext::active() const {
    return active_;
}

std::uint32_t PspEventContext::item_partner() const {
    return item_partner_;
}

bool PspEventContext::completed(std::int16_t event_id) const {
    return initialized_ && state_ == State::Completed &&
           (event_id == active_.event_id || event_id == -1);
}

}  // namespace dusk::psp::events
