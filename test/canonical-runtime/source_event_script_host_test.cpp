#include "dusk/psp/source_event_script.hpp"
#include <cmath>
#include <cstdio>
#include <cstring>
#include <fstream>
#include <iterator>
#include <vector>

std::vector<unsigned char> read_file(const char* path) {
    std::ifstream f(path, std::ios::binary);
    return {std::istreambuf_iterator<char>(f), {}};
}

bool close(float a, float b) {
    return std::fabs(a - b) < 0.001f;
}

int main(int argc, char** argv) {
    if (argc != 2) return 2;
    const auto bytes = read_file(argv[1]);
    dusk::psp::events::SourceEventScript event;
    int actor = 1;
    const auto id = event.initialize(bytes.data(), bytes.size())
        ? event.event_id("DEFAULT_TREASURE_NORMAL") : -1;
    if (id != 0x020D || !event.start(id, &actor)) return 3;
    const int treasure = event.staff_id("TREASURE", 0);
    const int link = event.staff_id("Link", 0);
    const int camera = event.staff_id("CAMERA", 0);
    if (treasure < 0 || link < 0 || camera < 0) return 4;
    if (std::strcmp(event.current_cut_name(treasure), "WAIT") != 0 ||
        std::strcmp(event.current_cut_name(link), "058lchange") != 0 ||
        std::strcmp(event.current_cut_name(camera), "WAIT") != 0) return 5;
    if (!event.cut_end(link) || !event.cut_end(camera)) return 6;
    event.tick();
    if (std::strcmp(event.current_cut_name(link), "010open_treasure") != 0 ||
        std::strcmp(event.current_cut_name(camera), "FIXEDFRM") != 0) return 7;
    if (!event.cut_end(treasure)) return 8;
    event.tick();
    if (std::strcmp(event.current_cut_name(treasure), "OPEN_SHORT") != 0 ||
        std::strcmp(event.current_cut_name(camera), "FIXEDFRM") != 0) return 9;
    const auto* center = event.vector_data(camera, "Center");
    const auto* eye = event.vector_data(camera, "Eye");
    const auto* fovy = event.float_data(camera, "Fovy");
    const auto* timer = event.integer_data(camera, "Timer");
    const char* rel = event.string_data(camera, "RelActor");
    const char* mask = event.string_data(camera, "RelUseMask");
    if (center == nullptr || eye == nullptr || fovy == nullptr ||
        timer == nullptr || rel == nullptr || mask == nullptr ||
        !close(center->y, 85.0f) || !close(center->z, 65.0f) ||
        !close(eye->x, 100.0f) || !close(eye->y, 160.0f) ||
        !close(eye->z, 220.0f) || !close(*fovy, 60.0f) ||
        *timer != 1 || std::strcmp(rel, "@PARTNER") != 0 ||
        std::strcmp(mask, "on") != 0) return 10;
    std::printf(
        "SOURCE_EVENT_SCRIPT_HOST_OK id=0x%04X staffs=%d,%d,%d "
        "fixedfrm=Center/Eye/Fovy/Timer/RelActor/RelUseMask\n",
        static_cast<unsigned>(static_cast<unsigned short>(id)),
        treasure, link, camera);
    return 0;
}
