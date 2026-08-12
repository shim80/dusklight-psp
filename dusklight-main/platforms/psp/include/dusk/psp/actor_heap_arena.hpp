#ifndef DUSK_PSP_ACTOR_HEAP_ARENA_HPP
#define DUSK_PSP_ACTOR_HEAP_ARENA_HPP

#include <cstdint>

namespace dusk::psp::model {

class PspActorHeapArena {
public:
    static constexpr std::uint32_t kCapacity = 0x2200;
    bool open(void* owner, std::uint32_t requested);
    void* allocate(std::uint32_t bytes, std::uint32_t alignment);
    void seal();
    void release(void* owner);
    bool owns(const void* owner) const;

    std::uint32_t peak() const { return peak_; }
    std::uint32_t overflows() const { return overflows_; }
    bool sealed() const { return sealed_; }

private:
    alignas(16) std::uint8_t bytes_[kCapacity] = {};
    void* owner_ = nullptr;
    std::uint32_t used_ = 0;
    std::uint32_t peak_ = 0;
    std::uint32_t overflows_ = 0;
    bool sealed_ = false;
};

}  // namespace dusk::psp::model

#endif
