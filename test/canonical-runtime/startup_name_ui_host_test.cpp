#include "dusk/psp/startup_ui_package.hpp"

#include <cstdint>
#include <cstdio>
#include <fstream>
#include <iterator>
#include <string>
#include <vector>

namespace {

std::uint16_t u16(const std::uint8_t* source) {
    return static_cast<std::uint16_t>(source[0]) |
           static_cast<std::uint16_t>(source[1]) << 8;
}

}  // namespace

int main(int argc, char** argv) {
    if (argc != 2) {
        std::fputs("file-select DPSU path required\n", stderr);
        return 1;
    }
    std::ifstream stream(argv[1], std::ios::binary);
    const std::vector<std::uint8_t> bytes{
        std::istreambuf_iterator<char>(stream),
        std::istreambuf_iterator<char>()};
    dusk::psp::startup::UiPackageView view = {};
    if (dusk::psp::startup::validate_startup_ui(
            bytes.data(), static_cast<std::uint32_t>(bytes.size()),
            &view) != dusk::psp::startup::UiPackageError::Ok ||
        view.record_count != 71 || view.atlas_width != 512 ||
        view.atlas_height != 512) {
        std::fputs("name-entry DPSU validation failed\n", stderr);
        return 1;
    }
    bool ids[384] = {};
    for (std::uint32_t index = 0; index < view.record_count; ++index) {
        const std::uint8_t* record =
            view.bytes + view.records_offset + index * 32;
        ids[u16(record)] = true;
    }
    for (char character : std::string(
             " 0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZ"
             "abcdefghijklmnopqrstuvwxyz")) {
        const std::uint16_t id = static_cast<std::uint16_t>(
            256 + static_cast<unsigned char>(character) - 32);
        if (!ids[id]) {
            std::fputs("name-entry glyph missing\n", stderr);
            return 1;
        }
    }
    std::puts(
        "STARTUP_NAME_UI_HOST_OK records=71 glyphs=63 "
        "font=source_rodan atlas=512x512");
    return 0;
}
