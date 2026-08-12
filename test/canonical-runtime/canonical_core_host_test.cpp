#include "dusk/psp/game_context.hpp"

#include "dusk/psp/room_package.hpp"

#include <cstdint>
#include <cstdio>
#include <cstring>

namespace game = dusk::psp::game;
namespace render = dusk::psp::render;
namespace resources = dusk::psp::resources;
namespace room = dusk::psp::room;

namespace {

constexpr std::uint8_t kPayload[] = {
    0x44, 0x55, 0x53, 0x4B, 0x4C, 0x49, 0x47, 0x48, 0x54,
};

struct Reader {
    std::uint32_t calls;
};

bool read(
    void* user, const char* path, void* output,
    std::uint32_t capacity, std::uint32_t* size) {
    auto* reader = static_cast<Reader*>(user);
    ++reader->calls;
    if (std::strcmp(path, "root/data/test.bin") != 0 ||
        capacity < sizeof(kPayload)) {
        return false;
    }
    std::memcpy(output, kPayload, sizeof(kPayload));
    *size = sizeof(kPayload);
    return true;
}

struct Draws {
    std::uint16_t kinds[4];
    std::uint8_t count;
};

bool submit(void* user, const render::Command& command) {
    auto* draws = static_cast<Draws*>(user);
    draws->kinds[draws->count++] = command.kind;
    return true;
}

}  // namespace

int main() {
    const std::uint32_t crc =
        room::package_crc32(kPayload, sizeof(kPayload));
    char manifest[256] = {};
    const int manifest_size = std::snprintf(
        manifest, sizeof(manifest),
        "DUSKLIGHT_RESOURCE_MANIFEST_V1\n"
        "test.resource|Scene|data/test.bin|%08X\n",
        crc);
    Reader reader = {};
    game::PspGameContext context = {};
    if (manifest_size <= 0 ||
        !context.initialize(
            "root", manifest,
            static_cast<std::uint32_t>(manifest_size),
            read, &reader) ||
        !context.consistent()) {
        return 1;
    }
    std::uint8_t output[32] = {};
    resources::PspResourceHandle handle = {};
    std::uint32_t size = 0;
    if (context.resources.load(
            "test.resource", resources::PspResourceType::RoomModel,
            output, sizeof(output), &handle, &size) ||
        !context.resources.load(
            "test.resource", resources::PspResourceType::Scene,
            output, sizeof(output), &handle, &size) ||
        size != sizeof(kPayload) ||
        !context.resources.handle_valid(handle) ||
        context.resources.data(handle) != output ||
        !context.resources.release(handle) ||
        context.resources.handle_valid(handle) ||
        context.resources.release(handle)) {
        return 2;
    }
    context.render_queue.begin_frame();
    if (!context.render_queue.enqueue(
            {render::Bucket::Ui, 3, 0, nullptr}) ||
        !context.render_queue.enqueue(
            {render::Bucket::RoomOpaque, 1, 0, nullptr}) ||
        !context.render_queue.enqueue(
            {render::Bucket::PlayerOpaque, 2, 0, nullptr})) {
        return 3;
    }
    Draws draws = {};
    if (!context.render_queue.flush(submit, &draws) ||
        draws.count != 3 ||
        draws.kinds[0] != 1 ||
        draws.kinds[1] != 2 ||
        draws.kinds[2] != 3) {
        return 4;
    }
    context.render_queue.begin_frame();
    for (std::uint16_t index = 0;
         index < render::PspRenderQueue::kCapacity; ++index) {
        if (!context.render_queue.enqueue(
                {render::Bucket::Effects, index, 0, nullptr})) {
            return 5;
        }
    }
    if (context.render_queue.enqueue(
            {render::Bucket::Effects, 999, 0, nullptr}) ||
        context.render_queue.overflows() != 1 ||
        context.resources.metrics.wrong_types != 1 ||
        context.resources.metrics.stale_handles != 1) {
        return 6;
    }
    context.render_queue.begin_frame();
    context.shutdown();
    if (context.resources.initialized() ||
        context.render_queue.initialized()) {
        return 7;
    }
    std::puts(
        "CANONICAL_CORE_HOST_OK resource_negatives=3 "
        "render_negatives=1 bucket_order=true");
    return 0;
}

