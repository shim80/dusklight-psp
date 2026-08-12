#include "dusk/psp/render_queue.hpp"

#include <cstring>

namespace dusk::psp::render {

void PspRenderQueue::initialize() {
    std::memset(commands_, 0, sizeof(commands_));
    std::memset(bucket_draws_, 0, sizeof(bucket_draws_));
    size_ = 0;
    peak_ = 0;
    overflows_ = 0;
    initialized_ = true;
}

void PspRenderQueue::begin_frame() {
    size_ = 0;
}

bool PspRenderQueue::enqueue(const Command& command) {
    if (!initialized_ ||
        command.bucket >= Bucket::Count ||
        size_ >= kCapacity) {
        ++overflows_;
        return false;
    }
    commands_[size_++] = command;
    if (size_ > peak_) {
        peak_ = size_;
    }
    return true;
}

bool PspRenderQueue::flush(SubmitCommand submit, void* user) {
    if (!initialized_ || submit == nullptr) {
        return false;
    }
    for (std::uint8_t raw = 0;
         raw < static_cast<std::uint8_t>(Bucket::Count); ++raw) {
        const Bucket bucket = static_cast<Bucket>(raw);
        for (std::uint16_t index = 0; index < size_; ++index) {
            if (commands_[index].bucket != bucket) {
                continue;
            }
            if (!submit(user, commands_[index])) {
                return false;
            }
            ++bucket_draws_[raw];
        }
    }
    size_ = 0;
    return true;
}

void PspRenderQueue::shutdown() {
    size_ = 0;
    initialized_ = false;
}

bool PspRenderQueue::initialized() const {
    return initialized_;
}

std::uint16_t PspRenderQueue::size() const {
    return size_;
}

std::uint16_t PspRenderQueue::peak() const {
    return peak_;
}

std::uint32_t PspRenderQueue::overflows() const {
    return overflows_;
}

std::uint32_t PspRenderQueue::bucket_draws(Bucket bucket) const {
    const std::uint8_t raw = static_cast<std::uint8_t>(bucket);
    return raw < static_cast<std::uint8_t>(Bucket::Count)
        ? bucket_draws_[raw] : 0;
}

}  // namespace dusk::psp::render

