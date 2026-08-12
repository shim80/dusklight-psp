#include "dusk/psp/switch_runtime.hpp"

#include <cstring>

namespace dusk::psp::switches {
namespace {

bool valid_stage_name(const char* name) {
    if (name == nullptr || name[0] == '\0') {
        return false;
    }
    std::uint32_t length = 0;
    while (name[length] != '\0' && length < 9) {
        ++length;
    }
    return length <= 8 && name[length] == '\0';
}

bool valid_room(std::int8_t room) {
    return room >= 0 &&
           static_cast<std::uint8_t>(room) < kRoomCount;
}

std::uint32_t mask_for(int number) {
    return 1u << (static_cast<std::uint32_t>(number) & 31u);
}

}  // namespace

bool PspSwitchSurface::initialize(
    stage::PersistentDemoState* persistent) {
    if (persistent == nullptr) {
        return false;
    }
    std::memset(room_banks_, 0, sizeof(room_banks_));
    std::memset(&stage_bank_, 0, sizeof(stage_bank_));
    std::memset(&event_bits_, 0, sizeof(event_bits_));
    std::memset(stage_name_, 0, sizeof(stage_name_));
    persistent_ = persistent;
    current_room_ = -1;
    metrics = {};
    initialized_ = true;
    return true;
}

void PspSwitchSurface::shutdown() {
    persistent_ = nullptr;
    current_room_ = -1;
    stage_name_[0] = '\0';
    initialized_ = false;
}

bool PspSwitchSurface::enter_stage(
    const char* stage_name, std::int8_t room) {
    if (!initialized_ || !valid_stage_name(stage_name) ||
        !valid_room(room)) {
        ++metrics.invalid_requests;
        return false;
    }
    if (std::strcmp(stage_name_, stage_name) != 0) {
        std::memset(room_banks_, 0, sizeof(room_banks_));
        std::memset(&stage_bank_, 0, sizeof(stage_bank_));
        std::strcpy(stage_name_, stage_name);
        ++metrics.stage_changes;
    }
    current_room_ = room;
    ++metrics.room_transitions;
    return true;
}

bool PspSwitchSurface::transition_to_room(std::int8_t room) {
    if (!initialized_ || stage_name_[0] == '\0' ||
        !valid_room(room)) {
        ++metrics.invalid_requests;
        return false;
    }
    current_room_ = room;
    ++metrics.room_transitions;
    return true;
}

bool PspSwitchSurface::reload_room(std::int8_t room) {
    if (!transition_to_room(room)) {
        return false;
    }
    ++metrics.room_reloads;
    return true;
}

bool PspSwitchSurface::reset_room(std::int8_t room) {
    if (!initialized_ || !valid_room(room)) {
        ++metrics.invalid_requests;
        return false;
    }
    std::memset(
        &room_banks_[static_cast<std::uint8_t>(room)], 0,
        sizeof(stage::RoomSwitchBank));
    ++metrics.room_resets;
    return true;
}

PspSwitchSurface::Scope PspSwitchSurface::scope_for(
    int number) const {
    if (number == -1 || number == kSwitchSentinel) {
        return Scope::Sentinel;
    }
    if (number >= 0 && number < kStageSwitchEnd) {
        return Scope::Stage;
    }
    if (number >= kRoomSwitchBegin &&
        number < kRoomSwitchEnd) {
        return Scope::Room;
    }
    return Scope::Invalid;
}

std::uint32_t* PspSwitchSurface::word_for(
    int number, std::int8_t room) {
    const Scope scope = scope_for(number);
    if (scope == Scope::Stage) {
        return &stage_bank_.bits[number / 32];
    }
    if (scope == Scope::Room && valid_room(room)) {
        return &room_banks_[static_cast<std::uint8_t>(room)]
                    .bits[number / 32];
    }
    return nullptr;
}

const std::uint32_t* PspSwitchSurface::word_for(
    int number, std::int8_t room) const {
    return const_cast<PspSwitchSurface*>(this)->word_for(
        number, room);
}

bool PspSwitchSurface::is_switch(
    int number, std::int8_t room) {
    ++metrics.reads;
    const Scope scope = scope_for(number);
    if (scope == Scope::Sentinel) {
        ++metrics.sentinel_ignores;
        return false;
    }
    const std::uint32_t* word = word_for(number, room);
    if (word == nullptr) {
        ++metrics.invalid_requests;
        return false;
    }
    return (*word & mask_for(number)) != 0;
}

bool PspSwitchSurface::on_switch(
    int number, std::int8_t room) {
    const Scope scope = scope_for(number);
    if (scope == Scope::Sentinel) {
        ++metrics.sentinel_ignores;
        return true;
    }
    std::uint32_t* word = word_for(number, room);
    if (word == nullptr) {
        ++metrics.invalid_requests;
        return false;
    }
    *word |= mask_for(number);
    ++metrics.on_writes;
    return true;
}

bool PspSwitchSurface::off_switch(
    int number, std::int8_t room) {
    const Scope scope = scope_for(number);
    if (scope == Scope::Sentinel) {
        ++metrics.sentinel_ignores;
        return true;
    }
    std::uint32_t* word = word_for(number, room);
    if (word == nullptr) {
        ++metrics.invalid_requests;
        return false;
    }
    *word &= ~mask_for(number);
    ++metrics.off_writes;
    return true;
}

bool PspSwitchSurface::is_event_bit(std::uint16_t flag) {
    ++metrics.event_reads;
    const std::uint8_t byte = event_bits_.bytes[flag >> 8];
    return (byte & static_cast<std::uint8_t>(flag)) != 0;
}

bool PspSwitchSurface::on_event_bit(std::uint16_t flag) {
    const std::uint8_t mask = static_cast<std::uint8_t>(flag);
    if (mask == 0) {
        ++metrics.invalid_requests;
        return false;
    }
    event_bits_.bytes[flag >> 8] |= mask;
    ++metrics.event_on_writes;
    return true;
}

bool PspSwitchSurface::off_event_bit(std::uint16_t flag) {
    const std::uint8_t mask = static_cast<std::uint8_t>(flag);
    if (mask == 0) {
        ++metrics.invalid_requests;
        return false;
    }
    event_bits_.bytes[flag >> 8] &= ~mask;
    ++metrics.event_off_writes;
    return true;
}

const stage::PersistentDemoState*
PspSwitchSurface::persistent_state() const {
    return persistent_;
}

std::int8_t PspSwitchSurface::current_room() const {
    return current_room_;
}

const char* PspSwitchSurface::current_stage() const {
    return stage_name_;
}

bool PspSwitchSurface::initialized() const {
    return initialized_;
}

}  // namespace dusk::psp::switches
