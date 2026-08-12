#ifndef DUSK_PSP_RENDER_QUEUE_HPP
#define DUSK_PSP_RENDER_QUEUE_HPP

#include <cstdint>

namespace dusk::psp::render {

enum class Bucket : std::uint8_t {
    RoomOpaque,
    ActorOpaque,
    PlayerOpaque,
    PlayerAlpha,
    ActorAlphaTest,
    ActorAlphaBlend,
    Effects,
    WorldDebug,
    Ui,
    Fade,
    Count,
};

struct Command {
    Bucket bucket;
    std::uint16_t kind;
    std::uint16_t source;
    const void* payload;
};

using SubmitCommand = bool (*)(void* user, const Command& command);

class PspRenderQueue {
public:
    static constexpr std::uint16_t kCapacity = 128;

    void initialize();
    void begin_frame();
    bool enqueue(const Command& command);
    bool flush(SubmitCommand submit, void* user);
    void shutdown();

    bool initialized() const;
    std::uint16_t size() const;
    std::uint16_t peak() const;
    std::uint32_t overflows() const;
    std::uint32_t bucket_draws(Bucket bucket) const;

private:
    Command commands_[kCapacity] = {};
    std::uint32_t bucket_draws_[
        static_cast<std::uint8_t>(Bucket::Count)] = {};
    std::uint16_t size_ = 0;
    std::uint16_t peak_ = 0;
    std::uint32_t overflows_ = 0;
    bool initialized_ = false;
};

}  // namespace dusk::psp::render

#endif

