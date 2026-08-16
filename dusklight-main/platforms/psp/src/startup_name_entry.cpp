#include "dusk/psp/startup_name_entry.hpp"

namespace dusk::psp::startup {

void NameEntryRuntime::initialize(
    NameEntryKind kind, const char* default_name) {
    kind_ = kind;
    length_ = 0;
    cursor_row_ = 0;
    cursor_column_ = 0;
    lowercase_ = false;
    confirmed_ = false;
    for (char& character : name_) {
        character = '\0';
    }
    if (default_name == nullptr) {
        return;
    }
    while (length_ < kMaximumNameBytes &&
           default_name[length_] != '\0') {
        name_[length_] = default_name[length_];
        ++length_;
    }
    name_[length_] = '\0';
}

char NameEntryRuntime::grid_character(
    std::uint8_t row, std::uint8_t column, bool lowercase) {
    if (row == 0 && column < 13) {
        const char value = static_cast<char>('A' + column);
        return lowercase ? static_cast<char>(value + ('a' - 'A')) : value;
    }
    if (row == 1 && column < 13) {
        const char value = static_cast<char>('N' + column);
        return lowercase ? static_cast<char>(value + ('a' - 'A')) : value;
    }
    if (row == 2 && column < 10) {
        return static_cast<char>('0' + column);
    }
    return '\0';
}

void NameEntryRuntime::erase_last() {
    if (length_ == 0) {
        return;
    }
    --length_;
    name_[length_] = '\0';
}

void NameEntryRuntime::finish_if_possible() {
    if (length_ != 0) {
        confirmed_ = true;
    }
}

bool NameEntryRuntime::tick(const NameEntryInput& input) {
    if (confirmed_) {
        return true;
    }
    if (input.left) {
        cursor_column_ = cursor_column_ == 0
            ? static_cast<std::uint8_t>(kColumns - 1)
            : static_cast<std::uint8_t>(cursor_column_ - 1);
    } else if (input.right) {
        cursor_column_ = static_cast<std::uint8_t>(
            (cursor_column_ + 1) % kColumns);
    } else if (input.up) {
        cursor_row_ = cursor_row_ == 0
            ? static_cast<std::uint8_t>(kRows - 1)
            : static_cast<std::uint8_t>(cursor_row_ - 1);
    } else if (input.down) {
        cursor_row_ = static_cast<std::uint8_t>(
            (cursor_row_ + 1) % kRows);
    } else if (input.toggle_case) {
        lowercase_ = !lowercase_;
    } else if (input.erase) {
        erase_last();
    } else if (input.finish) {
        finish_if_possible();
    } else if (input.confirm) {
        const char character = grid_character(
            cursor_row_, cursor_column_, lowercase_);
        if (character != '\0' && length_ < kMaximumNameBytes) {
            name_[length_++] = character;
            name_[length_] = '\0';
        } else if (cursor_row_ == 2 && cursor_column_ == 10) {
            if (length_ < kMaximumNameBytes) {
                name_[length_++] = ' ';
                name_[length_] = '\0';
            }
        } else if (cursor_row_ == 2 && cursor_column_ == 11) {
            erase_last();
        } else if (cursor_row_ == 2 && cursor_column_ == 12) {
            finish_if_possible();
        }
    }
    return true;
}

NameEntryKind NameEntryRuntime::kind() const { return kind_; }
const char* NameEntryRuntime::name() const { return name_; }
std::size_t NameEntryRuntime::length() const { return length_; }
std::uint8_t NameEntryRuntime::cursor_row() const { return cursor_row_; }
std::uint8_t NameEntryRuntime::cursor_column() const {
    return cursor_column_;
}
bool NameEntryRuntime::lowercase() const { return lowercase_; }
bool NameEntryRuntime::confirmed() const { return confirmed_; }

}  // namespace dusk::psp::startup
