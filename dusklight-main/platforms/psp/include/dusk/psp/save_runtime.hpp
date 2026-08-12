#ifndef DUSK_PSP_SAVE_RUNTIME_HPP
#define DUSK_PSP_SAVE_RUNTIME_HPP

#include <array>
#include <cstddef>
#include <cstdint>

namespace dusk::psp::save {

constexpr std::size_t kSlotCount = 3;
constexpr std::size_t kStageNameBytes = 8;
constexpr std::size_t kEncodedBankBytes = 128;

struct StartContext {
    std::array<char, kStageNameBytes> stage = {};
    std::int8_t room = 0;
    std::uint8_t start_point = 0;
    std::uint8_t layer = 0;
};

struct Slot {
    bool occupied = false;
    StartContext start = {};
    std::uint32_t play_seconds = 0;
    std::uint32_t save_counter = 0;
};

StartContext default_new_game_start();

class SaveBank {
public:
    void reset();

    std::size_t selected_slot() const;
    bool select_slot(std::size_t index);
    bool move_up();
    bool move_down();

    const Slot& slot(std::size_t index) const;
    bool create_new_game(
        std::size_t index,
        const StartContext& start = default_new_game_start());
    bool update_slot(
        std::size_t index,
        const StartContext& start,
        std::uint32_t play_seconds);
    bool restore_slot(std::size_t index, const Slot& slot);
    bool load_start(std::size_t index, StartContext* output) const;

private:
    std::array<Slot, kSlotCount> slots_ = {};
    std::size_t selected_slot_ = 0;
};

enum class FileSelectAction : std::uint8_t {
    None = 0,
    NewGame,
    Continue,
};

struct FileSelectInput {
    bool up = false;
    bool down = false;
    bool confirm = false;
    bool start = false;
};

struct FileSelectDecision {
    FileSelectAction action = FileSelectAction::None;
    std::size_t slot = 0;
    StartContext start = {};
};

class FileSelectRuntime {
public:
    explicit FileSelectRuntime(SaveBank* bank);
    bool tick(const FileSelectInput& input, FileSelectDecision* decision);

private:
    SaveBank* bank_ = nullptr;
};

enum class BankError : std::uint8_t {
    Ok = 0,
    NullInput,
    TooSmall,
    BadMagic,
    BadVersion,
    BadSize,
    BadSlotCount,
    BadSelectedSlot,
    CrcMismatch,
};

BankError encode_bank(
    const SaveBank& bank,
    std::uint8_t* output,
    std::size_t capacity,
    std::size_t* written = nullptr);
BankError decode_bank(
    const std::uint8_t* bytes,
    std::size_t size,
    SaveBank* output);
const char* bank_error_name(BankError error);

}  // namespace dusk::psp::save

#endif
