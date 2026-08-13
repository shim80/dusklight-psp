#include "dusk/psp/canonical_startup_entry.hpp"

#include <pspkernel.h>

PSP_MODULE_INFO("DusklightPSP", 0, 1, 0);
PSP_MAIN_THREAD_ATTR(THREAD_ATTR_USER);
PSP_HEAP_SIZE_KB(-256);

int main() {
    const int result = dusk::psp::game::run_canonical_startup_then_game();
    sceKernelExitGame();
    return result;
}
