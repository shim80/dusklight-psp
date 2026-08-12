#include "dusk/psp/render_state_trace.hpp"

namespace dusk::psp::render_trace {

bool BoundedTrace::initialize(Sink sink, void* user) {
    reset();
    sink_ = sink;
    user_ = sink == nullptr ? nullptr : user;
    return sink_ != nullptr;
}

bool BoundedTrace::emit(const Submission& submission) {
    if (sink_ == nullptr) {
        return false;
    }
    if (!have_frame_ || submission.frame != last_frame_) {
        if (frames_observed_ >= kMaximumFrames) {
            return true;
        }
        last_frame_ = submission.frame;
        have_frame_ = true;
        ++frames_observed_;
    }
    if (submissions_written_ >= kMaximumSubmissions) {
        ++dropped_submissions_;
        return false;
    }
    if (!sink_(user_, submission)) {
        ++dropped_submissions_;
        return false;
    }
    ++submissions_written_;
    return true;
}

void BoundedTrace::reset() {
    sink_ = nullptr;
    user_ = nullptr;
    last_frame_ = 0;
    frames_observed_ = 0;
    submissions_written_ = 0;
    dropped_submissions_ = 0;
    have_frame_ = false;
}

bool BoundedTrace::enabled() const {
    return sink_ != nullptr;
}

std::uint32_t BoundedTrace::frames_observed() const {
    return frames_observed_;
}

std::uint32_t BoundedTrace::submissions_written() const {
    return submissions_written_;
}

std::uint32_t BoundedTrace::dropped_submissions() const {
    return dropped_submissions_;
}

}  // namespace dusk::psp::render_trace
