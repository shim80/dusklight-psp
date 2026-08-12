#ifndef DUSK_PSP_INTERACTION_RUNTIME_HPP
#define DUSK_PSP_INTERACTION_RUNTIME_HPP

#include <cstdint>

namespace dusk::psp::interaction {

enum class ActionType : std::uint8_t {
    None,
    Use,
    Spin,
    Open,
    PickUp,
};

enum class Button : std::uint8_t {
    None,
    Cross,
    Circle,
    Square,
    Triangle,
};

enum class Result : std::uint8_t {
    None,
    Available,
    Accepted,
    Rejected,
    Completed,
};

struct Candidate {
    void* actor;
    const char* prompt;
    ActionType action;
    Button button;
    float distance;
    std::int16_t orientation;
    std::uint8_t priority;
    bool input_lock;
};

struct Metrics {
    std::uint32_t frames;
    std::uint32_t candidates_published;
    std::uint32_t candidates_replaced;
    std::uint32_t actions_accepted;
    std::uint32_t actions_rejected;
    std::uint32_t invalid_candidates;
};

class PspInteractionContext {
public:
    bool initialize();
    void shutdown();
    void begin_frame();
    bool publish(const Candidate& candidate);
    bool accept(Button pressed);
    void complete();

    bool initialized() const;
    bool available() const;
    const Candidate* selected() const;
    Result result() const;

    Metrics metrics = {};

private:
    Candidate selected_ = {};
    Result result_ = Result::None;
    bool initialized_ = false;
    bool available_ = false;
};

}  // namespace dusk::psp::interaction

#endif
