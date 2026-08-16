#ifndef DUSK_PSP_STARTUP_NAME_ENTRY_HPP
#define DUSK_PSP_STARTUP_NAME_ENTRY_HPP

#include <cstddef>
#include <cstdint>

namespace dusk::psp::startup {

enum class NameEntryKind : std::uint8_t {
    Player,
    Horse,
};

struct NameEntryInput {
    bool left = false;
    bool right = false;
    bool up = false;
    bool down = false;
    bool confirm = false;
    bool erase = false;
    bool toggle_case = false;
    bool finish = false;
};

class NameEntryRuntime {
public:
    static constexpr std::size_t kMaximumNameBytes = 8;
    static constexpr std::uint8_t kRows = 3;
    static constexpr std::uint8_t kColumns = 13;

    void initialize(NameEntryKind kind, const char* default_name);
    bool tick(const NameEntryInput& input);

    NameEntryKind kind() const;
    const char* name() const;
    std::size_t length() const;
    std::uint8_t cursor_row() const;
    std::uint8_t cursor_column() const;
    bool lowercase() const;
    bool confirmed() const;

    static char grid_character(
        std::uint8_t row, std::uint8_t column, bool lowercase);

private:
    void erase_last();
    void finish_if_possible();

    char name_[kMaximumNameBytes + 1] = {};
    std::size_t length_ = 0;
    NameEntryKind kind_ = NameEntryKind::Player;
    std::uint8_t cursor_row_ = 0;
    std::uint8_t cursor_column_ = 0;
    bool lowercase_ = false;
    bool confirmed_ = false;
};

}  // namespace dusk::psp::startup

#endif
