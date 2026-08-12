#ifndef DUSK_PSP_SOURCE_MESSAGE_BMG_HPP
#define DUSK_PSP_SOURCE_MESSAGE_BMG_HPP

#include <cstdint>

namespace dusk::psp::message {

constexpr std::uint16_t kSourceMessageTextCapacity = 768;
constexpr std::uint8_t kSourceMessageHighlightCapacity = 8;
constexpr std::uint8_t kSourceMessagePauseCapacity = 8;

struct HighlightSpan {
    std::uint16_t begin = 0;
    std::uint16_t end = 0;
    std::uint8_t color = 0;
};

struct PausePoint {
    std::uint16_t offset = 0;
    std::uint16_t frames = 0;
};

struct SourceMessageText {
    std::uint32_t id = 0;
    char text[kSourceMessageTextCapacity] = {};
    std::uint16_t length = 0;
    std::uint16_t instant_prefix = 0;
    HighlightSpan highlights[kSourceMessageHighlightCapacity] = {};
    PausePoint pauses[kSourceMessagePauseCapacity] = {};
    std::uint8_t highlight_count = 0;
    std::uint8_t pause_count = 0;
    std::uint8_t type_count = 0;
    std::uint8_t unknown_controls = 0;
    bool truncated = false;
};

struct SourceBmgMetrics {
    std::uint32_t initializes = 0;
    std::uint32_t lookups = 0;
    std::uint32_t controls = 0;
    std::uint32_t failures = 0;
};

class SourceBmgDatabase {
public:
    bool initialize(const void* data, std::uint32_t size);
    void reset();
    bool extract(std::uint32_t id, SourceMessageText* output);

    bool valid() const { return valid_; }
    std::uint16_t message_count() const { return message_count_; }
    std::uint8_t encoding() const { return encoding_; }
    SourceBmgMetrics metrics = {};

private:
    std::uint16_t read_u16(std::uint32_t offset) const;
    std::uint32_t read_u32(std::uint32_t offset) const;
    bool range(std::uint32_t offset, std::uint32_t bytes) const;
    bool parse_message(std::uint32_t start, SourceMessageText* output);

    const std::uint8_t* data_ = nullptr;
    std::uint32_t size_ = 0;
    std::uint32_t inf_ = 0;
    std::uint32_t dat_ = 0;
    std::uint32_t mid_ = 0;
    std::uint32_t inf_size_ = 0;
    std::uint32_t dat_size_ = 0;
    std::uint32_t mid_size_ = 0;
    std::uint16_t message_count_ = 0;
    std::uint16_t info_stride_ = 0;
    std::uint8_t encoding_ = 0;
    bool valid_ = false;
};

// Small input-driven lifetime used by PSP gameplay. It deliberately does not
// invent an autonomous timeout: an item cannot be committed merely because a
// fixed number of frames elapsed. Normal gameplay should feed action_pressed;
// automated fixtures may inject one deliberate press after visible evidence.
class SourceMessageRuntime {
public:
    bool begin(SourceBmgDatabase* database, std::uint32_t id);
    bool tick(bool action_pressed);
    void reset();

    bool active() const { return active_; }
    bool acknowledged() const { return acknowledged_; }
    bool awaiting_confirm() const {
        return active_ && revealed_characters_ >= message_.length && pause_remaining_ == 0;
    }
    std::uint16_t revealed_characters() const { return revealed_characters_; }
    std::uint32_t frames_visible() const { return frames_visible_; }
    const SourceMessageText& message() const { return message_; }

private:
    void advance_reveal();

    SourceMessageText message_ = {};
    std::uint32_t frames_visible_ = 0;
    std::uint16_t revealed_characters_ = 0;
    std::uint16_t pause_remaining_ = 0;
    std::uint8_t next_pause_ = 0;
    bool active_ = false;
    bool acknowledged_ = false;
};

}  // namespace dusk::psp::message

#endif
