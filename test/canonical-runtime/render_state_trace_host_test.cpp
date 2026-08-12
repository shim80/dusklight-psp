#include "dusk/psp/render_state_trace.hpp"

#include <cstdint>
#include <cstdio>

namespace trace = dusk::psp::render_trace;

namespace {

bool count_submission(void* user, const trace::Submission&) {
    auto* count = static_cast<std::uint32_t*>(user);
    ++*count;
    return true;
}

trace::Submission submission(std::uint32_t frame) {
    return {
        frame, trace::Source::Room, trace::Bucket::AlphaTest,
        7, 3, 4, 5, 0x7f,
        true, true, true, false, false, true, false,
    };
}

}  // namespace

int main() {
    trace::BoundedTrace bounded = {};
    std::uint32_t writes = 0;
    if (!bounded.initialize(count_submission, &writes)) return 1;
    for (std::uint32_t frame = 10; frame < 15; ++frame) {
        if (!bounded.emit(submission(frame))) return 2;
    }
    if (writes != 4 || bounded.frames_observed() != 4 ||
        bounded.submissions_written() != 4 ||
        bounded.dropped_submissions() != 0) return 3;

    writes = 0;
    if (!bounded.initialize(count_submission, &writes)) return 4;
    for (std::uint32_t index = 0;
         index < trace::BoundedTrace::kMaximumSubmissions + 1;
         ++index) {
        bounded.emit(submission(20));
    }
    if (writes != trace::BoundedTrace::kMaximumSubmissions ||
        bounded.dropped_submissions() != 1) return 5;
    bounded.reset();
    if (bounded.enabled()) return 6;

    std::puts(
        "PSP_RENDER_STATE_TRACE_HOST_OK frames=4 submissions=8192");
    return 0;
}
