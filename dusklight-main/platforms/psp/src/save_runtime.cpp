#include "dusk/psp/save_runtime.hpp"

#include <cstring>

namespace dusk::psp::save {
namespace {

constexpr std::uint16_t kFormatVersion = 1;
constexpr std::uint16_t kHeaderBytes = 32;
constexpr std::size_t kSlotBytes = 32;
constexpr std::size_t kCrcOffset = 12;
constexpr std::size_t kSelectedSlotOffset = 16;
constexpr std::size_t kSlotCountOffset = 17;
constexpr std::size_t kSlotsOffset = kHeaderBytes;

void write_u16(std::uint8_t* output, std::uint16_t value) {
    output[0] = static_cast<std::uint8_t>(value);
    output[1] = static_cast<std::uint8_t>(value >> 8);
}

void write_u32(std::uint8_t* output, std::uint32_t value) {
    output[0] = static_cast<std::uint8_t>(value);
    output[1] = static_cast<std::uint8_t>(value >> 8);
    output[2] = static_cast<std::uint8_t>(value >> 16);
    output[3] = static_cast<std::uint8_t>(value >> 24);
}

std::uint16_t read_u16(const std::uint8_t* input) {
    return static_cast<std::uint16_t>(input[0]) |
           static_cast<std::uint16_t>(input[1] << 8);
}

std::uint32_t read_u32(const std::uint8_t* input) {
    return static_cast<std::uint32_t>(input[0]) |
           (static_cast<std::uint32_t>(input[1]) << 8) |
           (static_cast<std::uint32_t>(input[2]) << 16) |
           (static_cast<std::uint32_t>(input[3]) << 24);
}

std::uint32_t encoded_crc32(const std::uint8_t* bytes, std::size_t size) {
    std::uint32_t crc = 0xFFFFFFFFu;
    for (std::size_t i = 0; i < size; ++i) {
        std::uint8_t value = bytes[i];
        if (i >= kCrcOffset && i < kCrcOffset + 4) {
            value = 0;
        }
        crc ^= value;
        for (unsigned bit = 0; bit < 8; ++bit) {
            const std::uint32_t mask =
                static_cast<std::uint32_t>(-(static_cast<std::int32_t>(crc & 1u)));
            crc = (crc >> 1) ^ (0xEDB88320u & mask);
        }
    }
    return ~crc;
}

void copy_stage(std::array<char, kStageNameBytes>* output, const char* stage) {
    output->fill('\0');
    if (stage == nullptr) {
        return;
    }
    const std::size_t length = std::strlen(stage);
    const std::size_t copy_length =
        length < output->size() - 1 ? length : output->size() - 1;
    std::memcpy(output->data(), stage, copy_length);
}

}  // namespace

StartContext default_new_game_start() {
    StartContext start = {};
    copy_stage(&start.stage, "F_SP108");
    start.room = 1;
    start.start_point = 21;
    start.layer = 0;
    return start;
}

void SaveBank::reset() {
    slots_ = {};
    selected_slot_ = 0;
}

std::size_t SaveBank::selected_slot() const {
    return selected_slot_;
}

bool SaveBank::select_slot(std::size_t index) {
    if (index >= kSlotCount) {
        return false;
    }
    selected_slot_ = index;
    return true;
}

bool SaveBank::move_up() {
    if (selected_slot_ == 0) {
        return false;
    }
    --selected_slot_;
    return true;
}

bool SaveBank::move_down() {
    if (selected_slot_ + 1 >= kSlotCount) {
        return false;
    }
    ++selected_slot_;
    return true;
}

const Slot& SaveBank::slot(std::size_t index) const {
    static const Slot kEmpty = {};
    return index < kSlotCount ? slots_[index] : kEmpty;
}

bool SaveBank::create_new_game(std::size_t index, const StartContext& start) {
    if (index >= kSlotCount || slots_[index].occupied) {
        return false;
    }
    slots_[index] = {};
    slots_[index].occupied = true;
    slots_[index].start = start;
    slots_[index].save_counter = 1;
    selected_slot_ = index;
    return true;
}

bool SaveBank::update_slot(
    std::size_t index,
    const StartContext& start,
    std::uint32_t play_seconds) {
    if (index >= kSlotCount || !slots_[index].occupied) {
        return false;
    }
    slots_[index].start = start;
    slots_[index].play_seconds = play_seconds;
    ++slots_[index].save_counter;
    selected_slot_ = index;
    return true;
}

bool SaveBank::restore_slot(std::size_t index, const Slot& slot) {
    if (index >= kSlotCount || !slot.occupied || slot.save_counter == 0) {
        return false;
    }
    slots_[index] = slot;
    return true;
}

bool SaveBank::load_start(std::size_t index, StartContext* output) const {
    if (index >= kSlotCount || output == nullptr || !slots_[index].occupied) {
        return false;
    }
    *output = slots_[index].start;
    return true;
}

FileSelectRuntime::FileSelectRuntime(SaveBank* bank) : bank_(bank) {}

bool FileSelectRuntime::tick(
    const FileSelectInput& input,
    FileSelectDecision* decision) {
    if (bank_ == nullptr || decision == nullptr) {
        return false;
    }
    *decision = {};
    decision->slot = bank_->selected_slot();

    // Source d_file_select::dataSelect() checks A/START before cursor movement.
    if (input.confirm || input.start) {
        const std::size_t index = bank_->selected_slot();
        decision->slot = index;
        if (bank_->slot(index).occupied) {
            decision->action = FileSelectAction::Continue;
            return bank_->load_start(index, &decision->start);
        }
        const StartContext start = default_new_game_start();
        if (!bank_->create_new_game(index, start)) {
            return false;
        }
        decision->action = FileSelectAction::NewGame;
        decision->start = start;
        return true;
    }
    if (input.up) {
        bank_->move_up();
    } else if (input.down) {
        bank_->move_down();
    }
    decision->slot = bank_->selected_slot();
    return true;
}

BankError encode_bank(
    const SaveBank& bank,
    std::uint8_t* output,
    std::size_t capacity,
    std::size_t* written) {
    if (output == nullptr) {
        return BankError::NullInput;
    }
    if (capacity < kEncodedBankBytes) {
        return BankError::TooSmall;
    }
    std::memset(output, 0, kEncodedBankBytes);
    std::memcpy(output, "DPSV", 4);
    write_u16(output + 4, kFormatVersion);
    write_u16(output + 6, kHeaderBytes);
    write_u32(output + 8, static_cast<std::uint32_t>(kEncodedBankBytes));
    output[kSelectedSlotOffset] =
        static_cast<std::uint8_t>(bank.selected_slot());
    output[kSlotCountOffset] = static_cast<std::uint8_t>(kSlotCount);

    for (std::size_t index = 0; index < kSlotCount; ++index) {
        const Slot& slot = bank.slot(index);
        std::uint8_t* record = output + kSlotsOffset + index * kSlotBytes;
        record[0] = slot.occupied ? 1u : 0u;
        record[1] = static_cast<std::uint8_t>(slot.start.room);
        record[2] = slot.start.start_point;
        record[3] = slot.start.layer;
        write_u32(record + 4, slot.play_seconds);
        write_u32(record + 8, slot.save_counter);
        std::memcpy(record + 12, slot.start.stage.data(), kStageNameBytes);
    }
    write_u32(output + kCrcOffset, encoded_crc32(output, kEncodedBankBytes));
    if (written != nullptr) {
        *written = kEncodedBankBytes;
    }
    return BankError::Ok;
}

BankError decode_bank(
    const std::uint8_t* bytes,
    std::size_t size,
    SaveBank* output) {
    if (bytes == nullptr || output == nullptr) {
        return BankError::NullInput;
    }
    if (size < kEncodedBankBytes) {
        return BankError::TooSmall;
    }
    if (std::memcmp(bytes, "DPSV", 4) != 0) {
        return BankError::BadMagic;
    }
    if (read_u16(bytes + 4) != kFormatVersion) {
        return BankError::BadVersion;
    }
    if (read_u16(bytes + 6) != kHeaderBytes ||
        read_u32(bytes + 8) != kEncodedBankBytes) {
        return BankError::BadSize;
    }
    if (bytes[kSlotCountOffset] != kSlotCount) {
        return BankError::BadSlotCount;
    }
    if (bytes[kSelectedSlotOffset] >= kSlotCount) {
        return BankError::BadSelectedSlot;
    }
    if (read_u32(bytes + kCrcOffset) != encoded_crc32(bytes, kEncodedBankBytes)) {
        return BankError::CrcMismatch;
    }

    SaveBank decoded;
    decoded.reset();
    for (std::size_t index = 0; index < kSlotCount; ++index) {
        const std::uint8_t* record = bytes + kSlotsOffset + index * kSlotBytes;
        if ((record[0] & 1u) == 0) {
            continue;
        }
        Slot slot = {};
        slot.occupied = true;
        slot.start.room = static_cast<std::int8_t>(record[1]);
        slot.start.start_point = record[2];
        slot.start.layer = record[3];
        std::memcpy(slot.start.stage.data(), record + 12, kStageNameBytes);
        slot.start.stage[kStageNameBytes - 1] = '\0';
        slot.play_seconds = read_u32(record + 4);
        slot.save_counter = read_u32(record + 8);
        if (!decoded.restore_slot(index, slot)) {
            return BankError::BadSize;
        }
    }
    decoded.select_slot(bytes[kSelectedSlotOffset]);
    *output = decoded;
    return BankError::Ok;
}

const char* bank_error_name(BankError error) {
    switch (error) {
    case BankError::Ok: return "ok";
    case BankError::NullInput: return "null_input";
    case BankError::TooSmall: return "too_small";
    case BankError::BadMagic: return "bad_magic";
    case BankError::BadVersion: return "bad_version";
    case BankError::BadSize: return "bad_size";
    case BankError::BadSlotCount: return "bad_slot_count";
    case BankError::BadSelectedSlot: return "bad_selected_slot";
    case BankError::CrcMismatch: return "crc_mismatch";
    }
    return "unknown";
}

}  // namespace dusk::psp::save
