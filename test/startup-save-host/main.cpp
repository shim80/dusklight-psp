#include "dusk/psp/save_runtime.hpp"
#include "dusk/psp/save_storage.hpp"

#include <array>
#include <cassert>
#include <cstdio>
#include <cstring>

int main() {
    using namespace dusk::psp::save;

    SaveBank bank;
    bank.reset();
    assert(bank.selected_slot() == 0);
    assert(!bank.move_up());
    assert(bank.move_down());
    assert(bank.selected_slot() == 1);
    assert(bank.move_down());
    assert(bank.selected_slot() == 2);
    assert(!bank.move_down());
    assert(bank.move_up());
    assert(bank.selected_slot() == 1);

    FileSelectRuntime select(&bank);
    FileSelectDecision decision = {};
    assert(select.tick({false, false, true, false}, &decision));
    assert(decision.action == FileSelectAction::NewGame);
    assert(decision.slot == 1);
    assert(std::strcmp(decision.start.stage.data(), "F_SP108") == 0);
    assert(decision.start.room == 1);
    assert(decision.start.start_point == 21);
    assert(bank.slot(1).occupied);
    assert(bank.slot(1).save_counter == 1);

    StartContext dungeon = {};
    constexpr char kDungeonStage[] = "D_MN05A";
    std::memcpy(dungeon.stage.data(), kDungeonStage, sizeof(kDungeonStage));
    dungeon.room = 50;
    dungeon.start_point = 3;
    dungeon.layer = 0;
    assert(bank.update_slot(1, dungeon, 1234));
    assert(bank.update_slot(1, dungeon, 2345));
    assert(bank.update_slot(1, dungeon, 3456));
    assert(bank.slot(1).save_counter == 4);

    std::array<std::uint8_t, kEncodedBankBytes> bytes = {};
    std::size_t written = 0;
    assert(encode_bank(bank, bytes.data(), bytes.size(), &written) == BankError::Ok);
    assert(written == kEncodedBankBytes);

    SaveBank decoded;
    decoded.reset();
    assert(decode_bank(bytes.data(), bytes.size(), &decoded) == BankError::Ok);
    assert(decoded.selected_slot() == 1);
    assert(decoded.slot(1).occupied);
    assert(std::strcmp(decoded.slot(1).start.stage.data(), "D_MN05A") == 0);
    assert(decoded.slot(1).play_seconds == 3456);
    assert(decoded.slot(1).save_counter == 4);

    auto corrupt = bytes;
    corrupt[64] ^= 1;
    assert(decode_bank(corrupt.data(), corrupt.size(), &decoded) == BankError::CrcMismatch);

    constexpr const char* kPath = "startup_save_flow_test.bin";
    assert(store_bank_file(kPath, bank) == StorageResult::Ok);
    SaveBank restored;
    restored.reset();
    assert(load_bank_file(kPath, &restored) == StorageResult::Ok);
    std::remove(kPath);
    assert(restored.selected_slot() == 1);
    assert(restored.slot(1).occupied);
    assert(restored.slot(1).play_seconds == 3456);
    assert(restored.slot(1).save_counter == 4);
    StartContext restored_start = {};
    assert(restored.load_start(1, &restored_start));
    assert(std::strcmp(restored_start.stage.data(), "D_MN05A") == 0);
    assert(restored_start.room == 50);
    assert(restored_start.start_point == 3);

    FileSelectRuntime continue_select(&restored);
    decision = {};
    assert(continue_select.tick({false, false, false, true}, &decision));
    assert(decision.action == FileSelectAction::Continue);
    assert(decision.slot == 1);
    assert(std::strcmp(decision.start.stage.data(), "D_MN05A") == 0);

    std::puts(
        "STARTUP_SAVE_FLOW_HOST_OK slots=3 cursor_clamp=true "
        "confirm=cross_or_start new_game=F_SP108/R01/start21 "
        "persistence=DPSV1+crc32 exact_metadata=true continue_context=true");
    return 0;
}
