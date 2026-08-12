#include "dusk/psp/startup_runtime.hpp"

namespace dusk::psp::startup {

bool StartupRuntime::initialize(
    const PackageView& package,
    std::uint32_t available_capabilities) {
    reset();
    if (package.bytes == nullptr || package.segment_count == 0) {
        return false;
    }
    package_ = package;
    available_capabilities_ = available_capabilities;
    initialized_ = load_record(0);
    return initialized_;
}

void StartupRuntime::reset() {
    package_ = {};
    current_ = {};
    metrics_ = {};
    available_capabilities_ = 0;
    missing_capabilities_ = 0;
    current_index_ = 0;
    last_transition_cause_ = TransitionCause::None;
    initialized_ = false;
    finished_ = false;
    blocked_ = false;
}

bool StartupRuntime::load_record(std::uint32_t index) {
    if (!package_.segment(index, &current_)) {
        return false;
    }
    current_index_ = index;
    metrics_.segment_frame = 0;
    missing_capabilities_ =
        current_.required_capabilities & ~available_capabilities_;
    blocked_ =
        current_.policy == AdvancePolicy::UnsupportedBoundary ||
        current_.completeness == Completeness::Unsupported ||
        missing_capabilities_ != 0;
    if (current_.policy == AdvancePolicy::UnsupportedBoundary) {
        last_transition_cause_ = TransitionCause::UnsupportedBoundary;
        ++metrics_.unsupported_boundaries;
    }
    return true;
}

bool StartupRuntime::advance(TransitionCause cause) {
    last_transition_cause_ = cause;
    ++metrics_.transitions;
    switch (cause) {
    case TransitionCause::Timeout:
        ++metrics_.timeout_transitions;
        break;
    case TransitionCause::Input:
        ++metrics_.input_transitions;
        break;
    case TransitionCause::ResourceReady:
        ++metrics_.resource_transitions;
        break;
    case TransitionCause::SourceEvent:
        ++metrics_.source_event_transitions;
        break;
    default:
        break;
    }
    if (current_index_ + 1 >= package_.segment_count) {
        finished_ = true;
        last_transition_cause_ = TransitionCause::SequenceComplete;
        return true;
    }
    return load_record(current_index_ + 1);
}

bool StartupRuntime::tick(
    const Input& input,
    bool resources_ready,
    bool source_event_complete) {
    if (!initialized_ || finished_) {
        return false;
    }
    ++metrics_.frame_number;
    ++metrics_.segment_frame;
    const bool pressed = input.confirm || input.start;

    if (missing_capabilities_ != 0 ||
        current_.policy == AdvancePolicy::UnsupportedBoundary ||
        current_.completeness == Completeness::Unsupported) {
        return true;
    }
    switch (current_.policy) {
    case AdvancePolicy::Timed:
        if (metrics_.segment_frame >= current_.duration_frames) {
            return advance(TransitionCause::Timeout);
        }
        break;
    case AdvancePolicy::TimedOrInput:
        if (pressed) {
            return advance(TransitionCause::Input);
        }
        if (metrics_.segment_frame >= current_.duration_frames) {
            return advance(TransitionCause::Timeout);
        }
        break;
    case AdvancePolicy::InputRequired:
        if (pressed) {
            return advance(TransitionCause::Input);
        }
        break;
    case AdvancePolicy::ResourceReady:
        if (resources_ready) {
            return advance(TransitionCause::ResourceReady);
        }
        break;
    case AdvancePolicy::SourceEvent:
        if (source_event_complete) {
            return advance(TransitionCause::SourceEvent);
        }
        break;
    case AdvancePolicy::UnsupportedBoundary:
        break;
    }
    return true;
}

bool StartupRuntime::initialized() const {
    return initialized_;
}

bool StartupRuntime::finished() const {
    return finished_;
}

bool StartupRuntime::blocked() const {
    return blocked_;
}

Segment StartupRuntime::current_segment() const {
    return current_.segment;
}

Completeness StartupRuntime::current_completeness() const {
    return current_.completeness;
}

std::uint32_t StartupRuntime::missing_capabilities() const {
    return missing_capabilities_;
}

TransitionCause StartupRuntime::last_transition_cause() const {
    return last_transition_cause_;
}

const RuntimeMetrics& StartupRuntime::metrics() const {
    return metrics_;
}

}  // namespace dusk::psp::startup
