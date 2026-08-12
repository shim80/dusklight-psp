#include "dusk/psp/startup_ui_package.hpp"

#include <cstdint>
#include <cstdio>
#include <fstream>
#include <iterator>
#include <vector>

namespace {

std::vector<std::uint8_t> load(const char* path) {
    std::ifstream stream(path, std::ios::binary);
    return {
        std::istreambuf_iterator<char>(stream),
        std::istreambuf_iterator<char>()};
}

bool validate(
    const char* path,
    std::uint32_t expected_records,
    std::uint32_t expected_width,
    std::uint32_t expected_height) {
    const std::vector<std::uint8_t> bytes = load(path);
    dusk::psp::startup::UiPackageView view = {};
    return dusk::psp::startup::validate_startup_ui(
               bytes.data(), static_cast<std::uint32_t>(bytes.size()),
               &view) ==
               dusk::psp::startup::UiPackageError::Ok &&
           view.record_count == expected_records &&
           view.atlas_width == expected_width &&
           view.atlas_height == expected_height;
}

}  // namespace

int main(int argc, char** argv) {
    if ((argc != 3 && argc != 4) ||
        !validate(argv[1], 3, 512, 512) ||
        !validate(argv[2], 17, 256, 64) ||
        (argc == 4 && !validate(argv[3], 8, 512, 512))) {
        std::fputs("startup UI package validation failed\n", stderr);
        return 1;
    }
    std::puts(
        "STARTUP_UI_HOST_OK format=DPSU1 logos=warning,nintendo,dolby "
        "title_text=Appuyez_sur_START file_select=source_textures");
    return 0;
}
