#include "dusk/psp/source_message_bmg.hpp"

#include <cstring>

namespace dusk::psp::message {
namespace {
constexpr std::uint32_t kHeader = 0x20;
constexpr std::uint32_t kTagInstant = 0x000001;
constexpr std::uint32_t kTagType = 0x000002;
constexpr std::uint32_t kTagPause = 0x000007;
constexpr std::uint32_t kTagColor = 0xFF0000;

bool tag_equal(const std::uint8_t* tag, std::uint32_t wanted) {
    const std::uint32_t value =
        (static_cast<std::uint32_t>(tag[0]) << 16) |
        (static_cast<std::uint32_t>(tag[1]) << 8) |
        static_cast<std::uint32_t>(tag[2]);
    return value == wanted;
}
}  // namespace

std::uint16_t SourceBmgDatabase::read_u16(std::uint32_t offset) const {
    if (!range(offset, 2)) return 0;
    return static_cast<std::uint16_t>(
        (static_cast<std::uint16_t>(data_[offset]) << 8) | data_[offset + 1]);
}
std::uint32_t SourceBmgDatabase::read_u32(std::uint32_t offset) const {
    if (!range(offset, 4)) return 0;
    return (static_cast<std::uint32_t>(data_[offset]) << 24) |
           (static_cast<std::uint32_t>(data_[offset + 1]) << 16) |
           (static_cast<std::uint32_t>(data_[offset + 2]) << 8) |
           static_cast<std::uint32_t>(data_[offset + 3]);
}
bool SourceBmgDatabase::range(
    std::uint32_t offset, std::uint32_t bytes) const {
    return data_ != nullptr && offset <= size_ && bytes <= size_ - offset;
}

void SourceBmgDatabase::reset() {
    data_ = nullptr;
    size_ = inf_ = dat_ = mid_ = 0;
    inf_size_ = dat_size_ = mid_size_ = 0;
    message_count_ = info_stride_ = 0;
    encoding_ = 0;
    valid_ = false;
}

bool SourceBmgDatabase::initialize(const void* source, std::uint32_t size) {
    reset();
    if (source == nullptr || size < kHeader) {
        ++metrics.failures;
        return false;
    }
    data_ = static_cast<const std::uint8_t*>(source);
    size_ = size;
    if (std::memcmp(data_, "MESGbmg1", 8) != 0 ||
        read_u32(12) == 0 || data_[16] != 1) {
        ++metrics.failures;
        reset();
        return false;
    }
    // Some Twilight Princess BMG resources append FLW1/FLI1 flow sections
    // beyond the length stored at +8, while the RARC entry itself remains the
    // authoritative byte range. Text lookup only needs INF1/DAT1/MID1, so use
    // the supplied archive-entry size and validate those sections directly.
    encoding_ = data_[16];
    const std::uint32_t section_count = read_u32(12);
    std::uint32_t cursor = kHeader;
    for (std::uint32_t i = 0; i < section_count; ++i) {
        if (!range(cursor, 8)) { ++metrics.failures; reset(); return false; }
        const std::uint32_t section_size = read_u32(cursor + 4);
        if (section_size < 8) { ++metrics.failures; reset(); return false; }
        if (std::memcmp(data_ + cursor, "INF1", 4) == 0) {
            if (!range(cursor, section_size)) { ++metrics.failures; reset(); return false; }
            inf_ = cursor; inf_size_ = section_size;
        } else if (std::memcmp(data_ + cursor, "DAT1", 4) == 0) {
            if (!range(cursor, section_size)) { ++metrics.failures; reset(); return false; }
            dat_ = cursor; dat_size_ = section_size;
        } else if (std::memcmp(data_ + cursor, "MID1", 4) == 0) {
            if (!range(cursor, section_size)) { ++metrics.failures; reset(); return false; }
            mid_ = cursor; mid_size_ = section_size;
        }
        cursor += section_size;
        if (inf_ != 0 && dat_ != 0 && mid_ != 0) break;
    }
    if (inf_ == 0 || dat_ == 0 || mid_ == 0 ||
        inf_size_ < 16 || dat_size_ < 9 || mid_size_ < 16) {
        ++metrics.failures; reset(); return false;
    }
    message_count_ = read_u16(inf_ + 8);
    info_stride_ = read_u16(inf_ + 10);
    if (message_count_ == 0 || info_stride_ < 4 ||
        read_u16(mid_ + 8) != message_count_ ||
        16u + static_cast<std::uint32_t>(message_count_) * info_stride_ > inf_size_ ||
        16u + static_cast<std::uint32_t>(message_count_) * 4u > mid_size_) {
        ++metrics.failures; reset(); return false;
    }
    valid_ = true;
    ++metrics.initializes;
    return true;
}

bool SourceBmgDatabase::parse_message(
    std::uint32_t start, SourceMessageText* output) {
    if (!valid_ || output == nullptr || start < dat_ + 8 ||
        start >= dat_ + dat_size_) return false;
    std::uint32_t cursor = start;
    std::uint8_t active_color = 0;
    std::uint16_t color_start = 0;
    bool color_open = false;
    while (cursor < dat_ + dat_size_) {
        const std::uint8_t value = data_[cursor];
        if (value == 0) break;
        if (value == 0x1A) {
            if (!range(cursor, 5)) return false;
            const std::uint8_t length = data_[cursor + 1];
            if (length < 5 || !range(cursor, length)) return false;
            const std::uint8_t* tag = data_ + cursor + 2;
            ++metrics.controls;
            if (tag_equal(tag, kTagInstant)) {
                output->instant_prefix = output->length;
            } else if (tag_equal(tag, kTagType)) {
                ++output->type_count;
                output->instant_prefix = output->length;
            } else if (tag_equal(tag, kTagPause)) {
                if (output->pause_count < kSourceMessagePauseCapacity) {
                    const std::uint16_t frames = length >= 7
                        ? static_cast<std::uint16_t>(
                            (static_cast<std::uint16_t>(data_[cursor + 5]) << 8) |
                            data_[cursor + 6])
                        : 0;
                    output->pauses[output->pause_count++] = {
                        output->length, frames};
                } else {
                    ++output->unknown_controls;
                }
            } else if (tag_equal(tag, kTagColor) && length >= 6) {
                const std::uint8_t next_color = data_[cursor + 5];
                if (color_open && output->highlight_count < kSourceMessageHighlightCapacity) {
                    output->highlights[output->highlight_count++] = {
                        color_start, output->length, active_color};
                }
                active_color = next_color;
                color_start = output->length;
                color_open = next_color != 0;
            } else {
                ++output->unknown_controls;
            }
            cursor += length;
            continue;
        }
        if (output->length + 1 >= kSourceMessageTextCapacity) {
            output->truncated = true;
            return false;
        }
        // Encoding 1 in this PAL file is single-byte western text for this
        // message group. Preserve source line breaks exactly.
        output->text[output->length++] = static_cast<char>(value);
        cursor += 1;
    }
    if (color_open && output->highlight_count < kSourceMessageHighlightCapacity) {
        output->highlights[output->highlight_count++] = {
            color_start, output->length, active_color};
    }
    output->text[output->length] = '\0';
    return cursor < dat_ + dat_size_ && data_[cursor] == 0;
}

bool SourceBmgDatabase::extract(
    std::uint32_t id, SourceMessageText* output) {
    if (!valid_ || output == nullptr) return false;
    *output = {};
    for (std::uint32_t i = 0; i < message_count_; ++i) {
        if (read_u32(mid_ + 16 + i * 4u) != id) continue;
        const std::uint32_t info = inf_ + 16 + i * info_stride_;
        const std::uint32_t text_offset = read_u32(info);
        const std::uint32_t start = dat_ + 8 + text_offset;
        output->id = id;
        ++metrics.lookups;
        if (!parse_message(start, output)) {
            ++metrics.failures;
            return false;
        }
        return true;
    }
    ++metrics.failures;
    return false;
}

void SourceMessageRuntime::reset() {
    message_ = {};
    frames_visible_ = 0;
    revealed_characters_ = 0;
    pause_remaining_ = 0;
    next_pause_ = 0;
    active_ = false;
    acknowledged_ = false;
}

void SourceMessageRuntime::advance_reveal() {
    if (pause_remaining_ > 0) {
        --pause_remaining_;
        return;
    }
    if (revealed_characters_ >= message_.length) return;
    ++revealed_characters_;
    if (next_pause_ < message_.pause_count &&
        revealed_characters_ >= message_.pauses[next_pause_].offset) {
        pause_remaining_ = message_.pauses[next_pause_].frames;
        ++next_pause_;
    }
}

bool SourceMessageRuntime::begin(
    SourceBmgDatabase* database, std::uint32_t id) {
    reset();
    if (database == nullptr || !database->extract(id, &message_)) return false;
    active_ = true;
    // MSGTAG_INSTANT makes the title appear immediately. Include a following
    // source newline/pause boundary so its configured delay starts before the
    // normal-speed body is revealed.
    revealed_characters_ = message_.instant_prefix;
    if (next_pause_ < message_.pause_count &&
        message_.pauses[next_pause_].offset == revealed_characters_ + 1 &&
        message_.text[revealed_characters_] == '\n') {
        ++revealed_characters_;
        pause_remaining_ = message_.pauses[next_pause_].frames;
        ++next_pause_;
    }
    return true;
}

bool SourceMessageRuntime::tick(bool action_pressed) {
    if (!active_) return false;
    ++frames_visible_;
    if (action_pressed) {
        if (revealed_characters_ < message_.length || pause_remaining_ != 0) {
            // Standard message behavior: the first confirm accelerates/reveals
            // text, but does not also close the box.
            revealed_characters_ = message_.length;
            pause_remaining_ = 0;
            next_pause_ = message_.pause_count;
            return true;
        }
        acknowledged_ = true;
        active_ = false;
        return true;
    }
    advance_reveal();
    return true;
}

}  // namespace dusk::psp::message
