#ifndef DUSK_PSP_SWITCH_RUNTIME_HPP
#define DUSK_PSP_SWITCH_RUNTIME_HPP

#include "dusk/psp/stage_runtime.hpp"

#include <cstdint>

namespace dusk::psp::switches {

constexpr int kStageSwitchEnd = 0xC0;
constexpr int kRoomSwitchBegin = 0xC0;
constexpr int kRoomSwitchEnd = 0xF0;
constexpr int kSwitchSentinel = 0xFF;
constexpr std::uint8_t kRoomCount = 64;

struct EventBitBank {
    std::uint8_t bytes[256];
};

struct Metrics {
    std::uint32_t reads;
    std::uint32_t on_writes;
    std::uint32_t off_writes;
    std::uint32_t event_reads;
    std::uint32_t event_on_writes;
    std::uint32_t event_off_writes;
    std::uint32_t stage_changes;
    std::uint32_t room_transitions;
    std::uint32_t room_resets;
    std::uint32_t room_reloads;
    std::uint32_t sentinel_ignores;
    std::uint32_t invalid_requests;
};

class PspSwitchSurface {
public:
    bool initialize(stage::PersistentDemoState* persistent);
    void shutdown();

    bool enter_stage(const char* stage_name, std::int8_t room);
    bool transition_to_room(std::int8_t room);
    bool reload_room(std::int8_t room);
    bool reset_room(std::int8_t room);

    bool is_switch(int number, std::int8_t room);
    bool on_switch(int number, std::int8_t room);
    bool off_switch(int number, std::int8_t room);

    bool is_event_bit(std::uint16_t flag);
    bool on_event_bit(std::uint16_t flag);
    bool off_event_bit(std::uint16_t flag);

    const stage::PersistentDemoState* persistent_state() const;
    std::int8_t current_room() const;
    const char* current_stage() const;
    bool initialized() const;

    Metrics metrics = {};

private:
    enum class Scope : std::uint8_t {
        Invalid,
        Sentinel,
        Stage,
        Room,
    };

    Scope scope_for(int number) const;
    std::uint32_t* word_for(int number, std::int8_t room);
    const std::uint32_t* word_for(int number, std::int8_t room) const;

    stage::RoomSwitchBank room_banks_[kRoomCount] = {};
    stage::StageSwitchBank stage_bank_ = {};
    EventBitBank event_bits_ = {};
    stage::PersistentDemoState* persistent_ = nullptr;
    char stage_name_[9] = {};
    std::int8_t current_room_ = -1;
    bool initialized_ = false;
};

}  // namespace dusk::psp::switches

#endif
