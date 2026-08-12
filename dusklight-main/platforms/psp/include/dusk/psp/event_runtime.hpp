#ifndef DUSK_PSP_EVENT_RUNTIME_HPP
#define DUSK_PSP_EVENT_RUNTIME_HPP

#include <cstdint>

namespace dusk::psp::events {

enum class State : std::uint8_t {
    None,
    Requested,
    Accepted,
    Running,
    Completed,
    Cancelled,
};

enum class Kind : std::uint8_t {
    Interaction,
    Door,
    Item,
    Transition,
};

struct Request {
    const void* source_actor;
    std::int16_t event_id;
    std::uint8_t map_tool_id;
    Kind kind;
};

struct Metrics {
    std::uint32_t requests;
    std::uint32_t accepts;
    std::uint32_t starts;
    std::uint32_t completions;
    std::uint32_t cancellations;
    std::uint32_t resets;
    std::uint32_t invalid_transitions;
    std::uint32_t competing_requests;
    std::uint32_t item_partner_writes;
};

class PspEventContext {
public:
    bool initialize();
    void shutdown();
    bool request(const Request& request);
    bool accept(const void* source_actor);
    bool start(const void* source_actor);
    bool complete(const void* source_actor);
    bool cancel(const void* source_actor);
    bool reset();
    bool set_item_partner(std::uint32_t process_id);

    bool initialized() const;
    State state() const;
    const Request& active() const;
    std::uint32_t item_partner() const;
    bool completed(std::int16_t event_id) const;

    Metrics metrics = {};

private:
    Request active_ = {};
    State state_ = State::None;
    std::uint32_t item_partner_ = 0xFFFFFFFFu;
    bool initialized_ = false;
};

}  // namespace dusk::psp::events

#endif
