#include "dusk/psp/platform.hpp"

#include <pspdebug.h>
#include <pspkernel.h>
#include <pspsysmem.h>
#include <pspthreadman.h>

#include <cstdio>
#include <cstring>

namespace dusk::psp {
namespace {

constexpr std::uint32_t kTextCapacity = 192;

bool g_initialized = false;
int g_last_error = 0;
char g_last_error_message[kTextCapacity] = {};
char g_application_name[kTextCapacity] = {};
char g_game_directory[kTextCapacity] = {};

void set_error(int code, const char* message) {
    g_last_error = code;
    std::snprintf(g_last_error_message, sizeof(g_last_error_message), "%s",
                  message != nullptr ? message : "unknown error");
}

bool copy_text(const char* source, char* destination, std::uint32_t capacity) {
    if (source == nullptr || source[0] == '\0' || destination == nullptr ||
        capacity == 0) {
        return false;
    }
    const int length = std::snprintf(destination, capacity, "%s", source);
    return length > 0 && static_cast<std::uint32_t>(length) < capacity;
}

bool valid_leaf(const char* leaf) {
    if (leaf == nullptr || leaf[0] == '\0' || std::strcmp(leaf, ".") == 0 ||
        std::strcmp(leaf, "..") == 0) {
        return false;
    }
    for (const char* cursor = leaf; *cursor != '\0'; ++cursor) {
        if (*cursor == '/' || *cursor == '\\' || *cursor == ':') {
            return false;
        }
    }
    return std::strstr(leaf, "..") == nullptr;
}

int exit_callback(int, int, void*) {
    sceKernelExitGame();
    return 0;
}

int callback_thread(SceSize, void*) {
    const int callback_id =
        sceKernelCreateCallback("Dusklight Exit Callback", exit_callback, nullptr);
    if (callback_id >= 0) {
        sceKernelRegisterExitCallback(callback_id);
    }
    sceKernelSleepThreadCB();
    return 0;
}

void setup_exit_callback() {
    const int thread_id = sceKernelCreateThread(
        "Dusklight Callback Thread", callback_thread, 0x11, 0xFA0, 0, nullptr);
    if (thread_id >= 0) {
        sceKernelStartThread(thread_id, 0, nullptr);
    }
}

}  // namespace

bool initialize(const RuntimeConfig& config) {
    if (g_initialized) {
        return true;
    }
    g_last_error = 0;
    g_last_error_message[0] = '\0';
    if (!copy_text(config.application_name, g_application_name,
                   sizeof(g_application_name)) ||
        !copy_text(config.game_directory, g_game_directory,
                   sizeof(g_game_directory))) {
        set_error(-1, "invalid or oversized runtime configuration");
        return false;
    }

    setup_exit_callback();
    pspDebugScreenInit();
    pspDebugScreenSetXY(0, 0);
    g_initialized = true;
    log(g_application_name);
    return true;
}

void shutdown() {
    if (!g_initialized) {
        return;
    }
    std::fflush(stdout);
    std::fflush(stderr);
    g_initialized = false;
}

void log(const char* message) {
    const char* safe_message = message != nullptr ? message : "(null)";
    pspDebugScreenPrintf("%s\n", safe_message);
    std::printf("%s\n", safe_message);
}

void log_error(const char* message, int error_code) {
    const char* safe_message = message != nullptr ? message : "(null)";
    pspDebugScreenPrintf("ERROR %d: %s\n", error_code, safe_message);
    std::fprintf(stderr, "ERROR %d: %s\n", error_code, safe_message);
}

std::uint64_t monotonic_microseconds() {
    return static_cast<std::uint64_t>(sceKernelGetSystemTimeWide());
}

void sleep_microseconds(std::uint32_t duration) {
    sceKernelDelayThread(duration);
}

bool make_game_path(const char* leaf, char* output, std::uint32_t capacity) {
    if (!g_initialized || !valid_leaf(leaf) || output == nullptr || capacity == 0) {
        set_error(-2, "invalid game path request");
        return false;
    }
    const int length =
        std::snprintf(output, capacity, "%s/%s", g_game_directory, leaf);
    if (length <= 0 || static_cast<std::uint32_t>(length) >= capacity) {
        set_error(-3, "game path exceeds output capacity");
        return false;
    }
    return true;
}

bool write_file(const char* path, const void* data, std::uint32_t size) {
    if (path == nullptr || data == nullptr || size == 0) {
        set_error(-4, "invalid file write request");
        return false;
    }
    std::FILE* file = std::fopen(path, "wb");
    if (file == nullptr) {
        set_error(-5, "unable to open file for writing");
        return false;
    }
    const std::size_t written = std::fwrite(data, 1, size, file);
    const int close_result = std::fclose(file);
    if (written != size || close_result != 0) {
        set_error(-6, "incomplete file write");
        return false;
    }
    return true;
}

bool read_file(const char* path, void* data, std::uint32_t capacity,
               std::uint32_t* size_read) {
    if (path == nullptr || data == nullptr || capacity == 0 || size_read == nullptr) {
        set_error(-7, "invalid file read request");
        return false;
    }
    *size_read = 0;
    std::FILE* file = std::fopen(path, "rb");
    if (file == nullptr) {
        set_error(-8, "unable to open file for reading");
        return false;
    }
    const std::size_t read = std::fread(data, 1, capacity, file);
    const bool failed = std::ferror(file) != 0;
    const int close_result = std::fclose(file);
    if (failed || close_result != 0) {
        set_error(-9, "file read failed");
        return false;
    }
    *size_read = static_cast<std::uint32_t>(read);
    return true;
}

MemorySnapshot memory_snapshot() {
    return {
        static_cast<std::uint32_t>(sceKernelTotalFreeMemSize()),
        static_cast<std::uint32_t>(sceKernelMaxFreeMemSize()),
    };
}

int last_error() {
    return g_last_error;
}

const char* last_error_message() {
    return g_last_error_message;
}

}  // namespace dusk::psp
