#include "dusk/psp/save_runtime.hpp"
#include "dusk/psp/save_storage.hpp"

#include <pspctrl.h>
#include <pspdebug.h>
#include <pspkernel.h>

#include <cstddef>
#include <cstdint>
#include <cstdio>

PSP_MODULE_INFO("DusklightSaveSelect", 0, 1, 0);
PSP_MAIN_THREAD_ATTR(THREAD_ATTR_USER);
PSP_HEAP_SIZE_KB(-256);

namespace save = dusk::psp::save;

namespace {
constexpr char kSavePath[] = "dusklight-save.bin";

const char* action_name(save::FileSelectAction action) {
    switch (action) {
    case save::FileSelectAction::None: return "READY";
    case save::FileSelectAction::NewGame: return "NEW GAME CREATED";
    case save::FileSelectAction::Continue: return "CONTINUE";
    }
    return "READY";
}

void draw(const save::SaveBank& bank, const char* status) {
    pspDebugScreenSetBackColor(0x00000000);
    pspDebugScreenSetTextColor(0x00FFFFFF);
    pspDebugScreenClear();
    pspDebugScreenSetXY(7, 3);
    pspDebugScreenPrintf("D U S K L I G H T   P S P");
    pspDebugScreenSetXY(11, 6);
    pspDebugScreenPrintf("SELECT A QUEST LOG");

    for (std::size_t i = 0; i < save::kSlotCount; ++i) {
        const save::Slot& slot = bank.slot(i);
        pspDebugScreenSetXY(7, static_cast<int>(10 + i * 3));
        pspDebugScreenPrintf("%c FILE %u   ", bank.selected_slot() == i ? '>' : ' ',
                             static_cast<unsigned>(i + 1));
        if (!slot.occupied) {
            pspDebugScreenPrintf("NEW GAME");
        } else {
            pspDebugScreenPrintf("CONTINUE  %-7s R%02d S%02u",
                                 slot.start.stage.data(),
                                 static_cast<int>(slot.start.room),
                                 static_cast<unsigned>(slot.start.start_point));
        }
    }

    pspDebugScreenSetXY(6, 21);
    pspDebugScreenPrintf("UP/DOWN  SELECT       X/START  CONFIRM");
    pspDebugScreenSetXY(6, 24);
    pspDebugScreenPrintf("%-48s", status);
    pspDebugScreenSetXY(6, 27);
    pspDebugScreenPrintf("Gameplay-first PSP checkpoint - save flow v1");
}
}

int main() {
    pspDebugScreenInit();
    sceCtrlSetSamplingCycle(0);
    sceCtrlSetSamplingMode(PSP_CTRL_MODE_DIGITAL);

    save::SaveBank bank;
    bank.reset();
    const save::StorageResult loaded = save::load_bank_file(kSavePath, &bank);
    if (loaded != save::StorageResult::Ok && loaded != save::StorageResult::NotFound) {
        bank.reset();
    }

    save::FileSelectRuntime selector(&bank);
    const char* status = loaded == save::StorageResult::Ok ? "SAVE BANK LOADED" : "READY";
    draw(bank, status);

    std::uint32_t previous = 0;
    for (;;) {
        SceCtrlData pad = {};
        sceCtrlPeekBufferPositive(&pad, 1);
        const std::uint32_t pressed = pad.Buttons & ~previous;
        previous = pad.Buttons;

        save::FileSelectInput input = {};
        input.up = (pressed & PSP_CTRL_UP) != 0;
        input.down = (pressed & PSP_CTRL_DOWN) != 0;
        input.confirm = (pressed & PSP_CTRL_CROSS) != 0;
        input.start = (pressed & PSP_CTRL_START) != 0;

        if (input.up || input.down || input.confirm || input.start) {
            save::FileSelectDecision decision = {};
            if (!selector.tick(input, &decision)) {
                status = "FILE SELECT ERROR";
            } else if (decision.action != save::FileSelectAction::None) {
                const save::StorageResult stored = save::store_bank_file(kSavePath, bank);
                status = stored == save::StorageResult::Ok
                    ? action_name(decision.action)
                    : "SAVE WRITE ERROR";
            } else {
                status = "READY";
            }
            draw(bank, status);
        }

        if ((pressed & PSP_CTRL_HOME) != 0) {
            break;
        }
        sceKernelDelayThread(16666);
    }

    sceKernelExitGame();
    return 0;
}
