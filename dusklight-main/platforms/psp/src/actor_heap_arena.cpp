#include "dusk/psp/actor_heap_arena.hpp"

#include <cstring>

namespace dusk::psp::model {

bool PspActorHeapArena::open(void* owner, std::uint32_t requested) {
    if (owner_ != nullptr) {
        return false;
    }
    if (owner == nullptr || requested > kCapacity) {
        ++overflows_;
        return false;
    }
    owner_ = owner;
    used_ = 0;
    sealed_ = false;
    std::memset(bytes_, 0, sizeof(bytes_));
    return true;
}

void* PspActorHeapArena::allocate(
    std::uint32_t bytes, std::uint32_t alignment) {
    if (owner_ == nullptr || sealed_ || alignment == 0 ||
        (alignment & (alignment - 1)) != 0) {
        ++overflows_;
        return nullptr;
    }
    const std::uint32_t aligned =
        (used_ + alignment - 1) & ~(alignment - 1);
    if (aligned > kCapacity || bytes > kCapacity - aligned) {
        ++overflows_;
        return nullptr;
    }
    used_ = aligned + bytes;
    if (used_ > peak_) peak_ = used_;
    return bytes_ + aligned;
}

void PspActorHeapArena::seal() {
    if (owner_ != nullptr) sealed_ = true;
}

void PspActorHeapArena::release(void* owner) {
    if (owner_ == owner) {
        owner_ = nullptr;
        used_ = 0;
        sealed_ = false;
    }
}

bool PspActorHeapArena::owns(const void* owner) const {
    return owner_ == owner && owner != nullptr;
}

}  // namespace dusk::psp::model
