#ifndef DUSK_PSP_STARTUP_RUNTIME_HPP
#define DUSK_PSP_STARTUP_RUNTIME_HPP

#include <cstdint>

#include "dusk/psp/startup_package.hpp"

namespace dusk::psp::startup {

enum class TransitionCause : std::uint8_t {
    None = 0,
    Timeout,
    Input,
    ResourceReady,
    SourceEvent,
    UnsupportedBoundary,
    SequenceComplete,
};

struct Input {
    bool confirm;
    bool start;
};

struct RuntimeMetrics {
    std::uint32_t frame_number;
    std::uint32_t segment_frame;
    std::uint32_t transitions;
    std::uint32_t input_transitions;
    std::uint32_t timeout_transitions;
    std::uint32_t resource_transitions;
    std::uint32_t source_event_transitions;
    std::uint32_t unsupported_boundaries;
};

class StartupRuntime {
public:
    bool initialize(
        const PackageView& package,
        std::uint32_t available_capabilities);
    void reset();
    bool tick(
        const Input& input,
        bool resources_ready,
        bool source_event_complete = false);

    bool initialized() const;
    bool finished() const;
    bool blocked() const;
    Segment current_segment() const;
    Completeness current_completeness() const;
    std::uint32_t missing_capabilities() const;
    TransitionCause last_transition_cause() const;
    const RuntimeMetrics& metrics() const;

private:
    bool load_record(std::uint32_t index);
    bool advance(TransitionCause cause);

    PackageView package_ = {};
    SegmentRecord current_ = {};
    RuntimeMetrics metrics_ = {};
    std::uint32_t available_capabilities_ = 0;
    std::uint32_t missing_capabilities_ = 0;
    std::uint32_t current_index_ = 0;
    TransitionCause last_transition_cause_ = TransitionCause::None;
    bool initialized_ = false;
    bool finished_ = false;
    bool blocked_ = false;
};

}  // namespace dusk::psp::startup

#endif
