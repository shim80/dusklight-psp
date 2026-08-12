#ifndef DUSK_PSP_ITEM_RUNTIME_HPP
#define DUSK_PSP_ITEM_RUNTIME_HPP

#include <cstdint>

namespace dusk::psp::items {

constexpr std::uint8_t kTreasureStateCount = 64;
constexpr std::uint16_t kItemIdCount = 256;

struct ItemAcquisition {
    std::uint8_t item_id;
    std::uint16_t quantity;
    std::uint16_t total;
    const void* source_actor;
};

using ItemAcquiredCallback =
    void (*)(void* user, const ItemAcquisition& acquisition);

struct ItemMetrics {
    std::uint32_t treasure_reads;
    std::uint32_t treasure_writes;
    std::uint32_t duplicate_writes;
    std::uint32_t item_reads;
    std::uint32_t acquisitions;
    std::uint32_t quantity_added;
    std::uint32_t hud_notifications;
    std::uint32_t invalid_requests;
    std::uint32_t quantity_overflows;
};

class PspItemContext {
public:
    bool initialize();
    void shutdown();
    bool is_treasure_open(int number);
    bool set_treasure_open(int number);
    bool clear_treasure(int number);
    bool acquire(
        int item_id, int quantity, const void* source_actor);
    std::uint16_t quantity(int item_id);
    void set_acquired_callback(
        ItemAcquiredCallback callback, void* user);
    void reset();
    bool initialized() const;

    ItemMetrics metrics = {};

private:
    std::uint32_t treasure_bits_[2] = {};
    std::uint16_t quantities_[kItemIdCount] = {};
    ItemAcquiredCallback acquired_callback_ = nullptr;
    void* acquired_user_ = nullptr;
    bool initialized_ = false;
};

}  // namespace dusk::psp::items

#endif
