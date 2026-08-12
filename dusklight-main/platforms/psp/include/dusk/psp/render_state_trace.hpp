#ifndef DUSK_PSP_RENDER_STATE_TRACE_HPP
#define DUSK_PSP_RENDER_STATE_TRACE_HPP

#include <cstdint>

namespace dusk::psp::render_trace {

enum class Source : std::uint8_t {
    Room,
    StaticActor,
    Link,
    StartupUi,
};

enum class Bucket : std::uint8_t {
    Opaque,
    AlphaTest,
    AlphaBlend,
    Ui,
};

struct Submission {
    std::uint32_t frame;
    Source source;
    Bucket bucket;
    std::uint16_t actor_id;
    std::uint16_t material_id;
    std::uint16_t shape_id;
    std::uint16_t texture_id;
    std::uint8_t alpha_reference;
    bool depth_test;
    bool depth_write;
    bool alpha_test;
    bool blending;
    bool culling;
    bool fog;
    bool lighting;
};

using Sink = bool (*)(void* user, const Submission& submission);

class BoundedTrace {
public:
    static constexpr std::uint32_t kMaximumFrames = 4;
    static constexpr std::uint32_t kMaximumSubmissions = 8192;

    bool initialize(Sink sink, void* user);
    bool emit(const Submission& submission);
    void reset();
    bool enabled() const;
    std::uint32_t frames_observed() const;
    std::uint32_t submissions_written() const;
    std::uint32_t dropped_submissions() const;

private:
    Sink sink_ = nullptr;
    void* user_ = nullptr;
    std::uint32_t last_frame_ = 0;
    std::uint32_t frames_observed_ = 0;
    std::uint32_t submissions_written_ = 0;
    std::uint32_t dropped_submissions_ = 0;
    bool have_frame_ = false;
};

}  // namespace dusk::psp::render_trace

#endif
