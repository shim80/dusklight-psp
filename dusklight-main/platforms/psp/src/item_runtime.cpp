#include "dusk/psp/item_runtime.hpp"

#include <cstring>
#include <limits>

namespace dusk::psp::items {
namespace {

bool valid_treasure(int number) {
    return number >= 0 && number < kTreasureStateCount;
}

bool valid_item(int item_id) {
    return item_id >= 0 && item_id < kItemIdCount;
}

std::uint32_t mask_for(int number) {
    return 1u << (static_cast<std::uint32_t>(number) & 31u);
}

}  // namespace

bool PspItemContext::initialize() {
    reset();
    initialized_ = true;
    return true;
}

void PspItemContext::shutdown() {
    std::memset(treasure_bits_, 0, sizeof(treasure_bits_));
    std::memset(quantities_, 0, sizeof(quantities_));
    acquired_callback_ = nullptr;
    acquired_user_ = nullptr;
    initialized_ = false;
}

bool PspItemContext::is_treasure_open(int number) {
    if (!initialized_ || !valid_treasure(number)) {
        ++metrics.invalid_requests;
        return false;
    }
    ++metrics.treasure_reads;
    return
        (treasure_bits_[static_cast<std::uint32_t>(number) >> 5u] &
         mask_for(number)) != 0;
}

bool PspItemContext::set_treasure_open(int number) {
    if (!initialized_ || !valid_treasure(number)) {
        ++metrics.invalid_requests;
        return false;
    }
    std::uint32_t& word =
        treasure_bits_[static_cast<std::uint32_t>(number) >> 5u];
    const std::uint32_t mask = mask_for(number);
    if ((word & mask) != 0) {
        ++metrics.duplicate_writes;
        return true;
    }
    word |= mask;
    ++metrics.treasure_writes;
    return true;
}

bool PspItemContext::clear_treasure(int number) {
    if (!initialized_ || !valid_treasure(number)) {
        ++metrics.invalid_requests;
        return false;
    }
    treasure_bits_[static_cast<std::uint32_t>(number) >> 5u] &=
        ~mask_for(number);
    return true;
}

bool PspItemContext::acquire(
    int item_id, int quantity_value, const void* source_actor) {
    if (!initialized_ || !valid_item(item_id) ||
        quantity_value <= 0 || source_actor == nullptr) {
        ++metrics.invalid_requests;
        return false;
    }
    std::uint16_t& current =
        quantities_[static_cast<std::uint32_t>(item_id)];
    if (quantity_value >
        static_cast<int>(
            std::numeric_limits<std::uint16_t>::max() - current)) {
        ++metrics.quantity_overflows;
        return false;
    }
    current = static_cast<std::uint16_t>(
        current + static_cast<std::uint16_t>(quantity_value));
    ++metrics.acquisitions;
    metrics.quantity_added +=
        static_cast<std::uint32_t>(quantity_value);
    if (acquired_callback_ != nullptr) {
        acquired_callback_(
            acquired_user_,
            {static_cast<std::uint8_t>(item_id),
             static_cast<std::uint16_t>(quantity_value),
             current, source_actor});
        ++metrics.hud_notifications;
    }
    return true;
}

std::uint16_t PspItemContext::quantity(int item_id) {
    if (!initialized_ || !valid_item(item_id)) {
        ++metrics.invalid_requests;
        return 0;
    }
    ++metrics.item_reads;
    return quantities_[static_cast<std::uint32_t>(item_id)];
}

void PspItemContext::set_acquired_callback(
    ItemAcquiredCallback callback, void* user) {
    acquired_callback_ = callback;
    acquired_user_ = user;
}

void PspItemContext::reset() {
    std::memset(treasure_bits_, 0, sizeof(treasure_bits_));
    std::memset(quantities_, 0, sizeof(quantities_));
    metrics = {};
}

bool PspItemContext::initialized() const {
    return initialized_;
}

}  // namespace dusk::psp::items
