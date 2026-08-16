#include "dusk/psp/startup_route_capture.hpp"

#include "dusk/psp/platform.hpp"
#include "dusk/psp/playable_render.hpp"

#include <cstdint>
#include <cstring>

namespace dusk::psp::game {
namespace {

constexpr std::uint32_t kCaptureBytes = 512 * 272 * 2;
constexpr char kCaptureRequest[] = "route_v1";
constexpr char kCaptureMarker[] = "DUSKLIGHT_PSP_STARTUP_ROUTE_CAPTURE_OK";
constexpr char kCaptureMetrics[] =
    "mode=startup_route_capture\n"
    "capture_format=5650\n"
    "capture_count=6\n"
    "route=team_logo,fsp102_scene,title_logo,title_prompt,file_select,fsp108\n"
    "error_code=0\n";
alignas(64) std::uint8_t g_capture[kCaptureBytes] = {};

}  // namespace

bool startup_route_capture_enabled() {
    char path[256] = {};
    if (!make_game_path("DUSKLIGHT.ROUTE.CAPTURE", path, sizeof(path))) {
        return false;
    }
    char request[16] = {};
    std::uint32_t size = 0;
    const std::uint32_t token_size = sizeof(kCaptureRequest) - 1;
    return read_file(path, request, sizeof(request), &size) &&
           (size == token_size ||
            (size == token_size + 1 && request[token_size] == '\n')) &&
           std::memcmp(request, kCaptureRequest, token_size) == 0;
}

bool capture_startup_route_frame(const char* leaf) {
    char path[256] = {};
    return startup_route_capture_enabled() &&
           make_game_path(leaf, path, sizeof(path)) &&
           playable::capture_playable_frame_5650(
               g_capture, sizeof(g_capture)) &&
           write_file(path, g_capture, sizeof(g_capture));
}

bool complete_startup_route_capture() {
    char metrics_path[256] = {};
    char marker_path[256] = {};
    return startup_route_capture_enabled() &&
           make_game_path(
               "STARTUP.ROUTE.METRICS", metrics_path,
               sizeof(metrics_path)) &&
           make_game_path(
               "STARTUP.ROUTE.OK", marker_path, sizeof(marker_path)) &&
           write_file(
               metrics_path, kCaptureMetrics,
               sizeof(kCaptureMetrics) - 1) &&
           write_file(
               marker_path, kCaptureMarker, sizeof(kCaptureMarker) - 1);
}

}  // namespace dusk::psp::game
