#include "dusk/psp/playable_package.hpp"

#include <cstdint>
#include <cstdio>
#include <fstream>
#include <iterator>
#include <vector>

namespace {
std::vector<std::uint8_t> read_file(const char* path) {
    std::ifstream input(path, std::ios::binary);
    return {std::istreambuf_iterator<char>(input),
            std::istreambuf_iterator<char>()};
}
}

int main(int argc, char** argv) {
    if (argc != 2) return 2;
    const auto bytes = read_file(argv[1]);
    dusk::psp::playable::PackageView view = {};
    if (dusk::psp::playable::validate_dpui(
            bytes.data(), static_cast<std::uint32_t>(bytes.size()), &view) !=
        dusk::psp::playable::PackageError::Ok) {
        return 1;
    }
    const std::uint32_t width = dusk::psp::playable::read_u32(view.bytes + 16);
    const std::uint32_t height = dusk::psp::playable::read_u32(view.bytes + 20);
    const std::uint32_t count = dusk::psp::playable::read_u32(view.bytes + 28);
    const std::uint32_t table = dusk::psp::playable::read_u32(view.bytes + 32);
    const std::uint32_t stride = dusk::psp::playable::read_u32(view.bytes + 36);
    bool found[95] = {};
    for (std::uint32_t index = 0; index < count; ++index) {
        const std::uint8_t* record = view.bytes + table + index * stride;
        const std::uint16_t id = dusk::psp::playable::read_u16(record);
        if (id < 128 + 0x20 || id > 128 + 0x7e) continue;
        const std::uint16_t code = static_cast<std::uint16_t>(id - 128);
        const std::uint16_t bx = dusk::psp::playable::read_u16(record + 4);
        const std::uint16_t by = dusk::psp::playable::read_u16(record + 6);
        const std::uint16_t u = dusk::psp::playable::read_u16(record + 12);
        const std::uint16_t v = dusk::psp::playable::read_u16(record + 14);
        const std::uint16_t w = dusk::psp::playable::read_u16(record + 16);
        const std::uint16_t h = dusk::psp::playable::read_u16(record + 18);
        const std::uint16_t advance = dusk::psp::playable::read_u16(record + 28);
        if (bx >= 24 || by >= 24 || w == 0 || h == 0 || bx + w > 24 ||
            by + h > 24 || u + w > width || v + h > height || advance == 0) {
            return 3;
        }
        found[code - 0x20] = true;
    }
    for (bool present : found) if (!present) return 4;
    std::printf(
        "FULL_ASCII_DPUI_HOST_OK records=%u atlas=%ux%u glyphs=95 "
        "edram_bytes=%u\n",
        count, width, height,
        dusk::psp::playable::read_u32(view.bytes + 44));
    return 0;
}
