#ifndef DUSK_LINK_PROBE_SDL_IOSTREAM_H
#define DUSK_LINK_PROBE_SDL_IOSTREAM_H

#include <cstddef>
#include <cstdint>

using Sint64 = std::int64_t;

struct SDL_IOStream;

enum {
    SDL_IO_SEEK_SET = 0,
    SDL_IO_SEEK_CUR = 1,
    SDL_IO_SEEK_END = 2,
};

extern "C" {

SDL_IOStream* SDL_IOFromFile(const char* path, const char* mode);
Sint64 SDL_SeekIO(SDL_IOStream* context, Sint64 offset, int whence);
std::size_t SDL_ReadIO(SDL_IOStream* context, void* destination, std::size_t size);
Sint64 SDL_GetIOSize(SDL_IOStream* context);
bool SDL_CloseIO(SDL_IOStream* context);

}

#endif
