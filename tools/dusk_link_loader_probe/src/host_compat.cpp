#include <SDL3/SDL_iostream.h>

#include <aurora/aurora.h>
#include <aurora/lib/logging.hpp>
#include <dolphin/os.h>
#include <JSystem/JKernel/JKRHeap.h>
#include <JSystem/JUtility/JUTException.h>
#include <dusk/frame_interpolation.h>

#include <cerrno>
#include <cstdarg>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <new>

namespace {

alignas(32) std::byte g_arena[128 * 1024 * 1024];
void* g_arena_lo = g_arena;
void* g_arena_hi = g_arena + sizeof(g_arena);

}

struct SDL_IOStream {
    std::FILE* file;
};

extern "C" SDL_IOStream* SDL_IOFromFile(const char* path, const char* mode) {
    if (path == nullptr || mode == nullptr || std::strcmp(mode, "rb") != 0) {
        return nullptr;
    }
    std::FILE* file = std::fopen(path, "rb");
    if (file == nullptr) {
        return nullptr;
    }
    SDL_IOStream* stream = new (std::nothrow) SDL_IOStream{file};
    if (stream == nullptr) {
        std::fclose(file);
    }
    return stream;
}

extern "C" Sint64 SDL_SeekIO(SDL_IOStream* context, Sint64 offset, int whence) {
    if (context == nullptr || context->file == nullptr) {
        return -1;
    }
    if (::fseeko(context->file, static_cast<off_t>(offset), whence) != 0) {
        return -1;
    }
    return static_cast<Sint64>(::ftello(context->file));
}

extern "C" std::size_t SDL_ReadIO(
    SDL_IOStream* context,
    void* destination,
    std::size_t size) {
    if (context == nullptr || context->file == nullptr ||
        (destination == nullptr && size != 0)) {
        return 0;
    }
    return std::fread(destination, 1, size, context->file);
}

extern "C" Sint64 SDL_GetIOSize(SDL_IOStream* context) {
    if (context == nullptr || context->file == nullptr) {
        return -1;
    }
    const off_t current = ::ftello(context->file);
    if (current < 0 || ::fseeko(context->file, 0, SEEK_END) != 0) {
        return -1;
    }
    const off_t size = ::ftello(context->file);
    if (size < 0 || ::fseeko(context->file, current, SEEK_SET) != 0) {
        return -1;
    }
    return static_cast<Sint64>(size);
}

extern "C" bool SDL_CloseIO(SDL_IOStream* context) {
    if (context == nullptr) {
        return false;
    }
    const bool closed = context->file != nullptr && std::fclose(context->file) == 0;
    delete context;
    return closed;
}

namespace aurora {

AuroraConfig g_config = {};
std::uint32_t g_sdlCustomEventsStart = 0;
char g_gameName[4] = {};

}

aurora::Module DuskLog("dusk_link_loader_probe");

extern "C" [[noreturn]] void OSPanic(
    const char* file,
    int line,
    const char* message,
    ...) {
    std::fprintf(stderr, "OSPanic at %s:%d: ", file != nullptr ? file : "?", line);
    std::va_list args;
    va_start(args, message);
    std::vfprintf(stderr, message != nullptr ? message : "unknown", args);
    va_end(args);
    std::fputc('\n', stderr);
    std::abort();
}

extern "C" void OSReport(const char* message, ...) {
    std::va_list args;
    va_start(args, message);
    std::vfprintf(stderr, message, args);
    va_end(args);
}

extern "C" void OSVReport(const char* message, std::va_list args) {
    std::vfprintf(stderr, message, args);
}

extern "C" void* OSGetArenaLo(void) {
    return g_arena_lo;
}

extern "C" void* OSGetArenaHi(void) {
    return g_arena_hi;
}

extern "C" void OSSetArenaLo(void* value) {
    g_arena_lo = value;
}

extern "C" void OSSetArenaHi(void* value) {
    g_arena_hi = value;
}

extern "C" u32 OSGetPhysicalMemSize(void) {
    return sizeof(g_arena);
}

extern "C" void OSInitMutex(OSMutex* mutex) {
    std::memset(mutex, 0, sizeof(*mutex));
}

extern "C" void OSLockMutex(OSMutex* mutex) {
    ++mutex->count;
}

extern "C" void OSUnlockMutex(OSMutex* mutex) {
    if (mutex->count <= 0) {
        OSPanic(__FILE__, __LINE__, "unbalanced host mutex unlock");
    }
    --mutex->count;
}

extern "C" BOOL OSTryLockMutex(OSMutex* mutex) {
    ++mutex->count;
    return TRUE;
}

extern "C" void DCInvalidateRange(void*, u32) {}
extern "C" void DCFlushRange(void*, u32) {}
extern "C" void DCFlushRangeNoSync(void*, u32) {}
extern "C" void DCStoreRange(void*, u32) {}
extern "C" void DCStoreRangeNoSync(void*, u32) {}
extern "C" void PPCSync(void) {}

namespace dusk::frame_interp {

void record_final_mtx(Mtx) {}

void add_interpolation_callback(InterpolationCallBack, void*) {}

}

extern "C" BOOL OSDisableInterrupts(void) {
    return TRUE;
}

extern "C" BOOL OSRestoreInterrupts(BOOL level) {
    return level;
}

extern "C" s32 OSDisableScheduler(void) {
    return 0;
}

extern "C" s32 OSEnableScheduler(void) {
    return 0;
}

extern "C" void OSInitMessageQueue(
    OSMessageQueue* queue,
    OSMessage* messages,
    s32 count) {
    std::memset(queue, 0, sizeof(*queue));
    queue->msgArray = messages;
    queue->msgCount = count;
}

extern "C" BOOL OSSendMessage(
    OSMessageQueue* queue,
    OSMessage message,
    s32) {
    if (queue == nullptr || queue->usedCount >= queue->msgCount) {
        return FALSE;
    }
    const s32 index = (queue->firstIndex + queue->usedCount) % queue->msgCount;
    queue->msgArray[index] = message;
    ++queue->usedCount;
    return TRUE;
}

extern "C" BOOL OSReceiveMessage(
    OSMessageQueue* queue,
    OSMessage* message,
    s32) {
    if (queue == nullptr || queue->usedCount == 0) {
        return FALSE;
    }
    if (message != nullptr) {
        *message = queue->msgArray[queue->firstIndex];
    }
    queue->firstIndex = (queue->firstIndex + 1) % queue->msgCount;
    --queue->usedCount;
    return TRUE;
}

extern "C" void JUTReportConsole(const char* message) {
    std::fputs(message, stderr);
}

extern "C" void JUTWarningConsole(const char* message) {
    std::fputs(message, stderr);
}

extern "C" void JUTReportConsole_f(const char* message, ...) {
    std::va_list args;
    va_start(args, message);
    std::vfprintf(stderr, message, args);
    va_end(args);
}

extern "C" void JUTWarningConsole_f(const char* message, ...) {
    std::va_list args;
    va_start(args, message);
    std::vfprintf(stderr, message, args);
    va_end(args);
}

void OSReport_Error(const char* message, ...) {
    std::va_list args;
    va_start(args, message);
    std::vfprintf(stderr, message, args);
    va_end(args);
}

void JUTException::panic_f_va(
    const char* file,
    int line,
    const char* message,
    std::va_list args) {
    std::fprintf(stderr, "JUT panic at %s:%d: ", file, line);
    std::vfprintf(stderr, message, args);
    std::fputc('\n', stderr);
    std::abort();
}

void JUTException::panic_f(
    const char* file,
    int line,
    const char* message,
    ...) {
    std::va_list args;
    va_start(args, message);
    panic_f_va(file, line, message, args);
    va_end(args);
}

bool JKRHeap::dump_sort() {
    return false;
}

namespace dusk::frame_interp {

bool lookup_replacement(const void*, Mtx) {
    return false;
}

bool lookup_concat_replacement(const void*, const void*, Mtx) {
    return false;
}

}
