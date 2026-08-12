#ifndef DUSK_PSP_SHADOW_RUNTIME_HPP
#define DUSK_PSP_SHADOW_RUNTIME_HPP

#include "dusk/psp/movebg_runtime.hpp"

#include <cstdint>

namespace dusk::psp::shadow {

struct PspSimpleShadowRequest {
    room::Vec3 position;
    float radius;
    float height;
    room::Vec3 floor_normal;
    float alpha;
    float environment_density;
    room::Vec3 environment_direction;
    float fade;
    std::uint32_t generation;
};

struct PspProjectedShadowRequest {
    room::Vec3 position;
    float radius;
    float height;
    room::Vec3 direction;
    float density;
    std::uint32_t generation;
    std::uint8_t priority;
};

struct PspShadowReceiverMesh {
    static constexpr std::uint32_t kCapacity = 512;
    room::ShadowReceiverTriangle triangles[kCapacity];
    std::uint16_t request_indices[kCapacity];
    std::uint32_t triangle_count;
    std::uint32_t generation;
    bool overflow;
};

struct PspShadowMap {
    std::uint16_t width;
    std::uint16_t height;
    std::uint32_t format;
    std::uint32_t edram_bytes;
    std::uint32_t auxiliary_edram_bytes;
    std::uint32_t updates;
    std::uint32_t reuses;
    std::uint32_t content_signature;
    std::uint32_t generation;
    bool valid;
    bool update_required;
};

struct Metrics {
    std::uint32_t simple_requests;
    std::uint32_t projected_requests;
    std::uint32_t receiver_triangles;
    std::uint32_t receiver_overflows;
    std::uint32_t stale_handles;
    std::uint32_t allocations_during_playing;
    std::uint32_t non_finite_values;
    std::uint32_t draw_calls;
    std::uint32_t movebg_receiver_triangles;
};

struct PspShadowSystem {
    static constexpr std::uint32_t kSimpleCapacity = 64;
    static constexpr std::uint32_t kProjectedCapacity = 4;

    PspSimpleShadowRequest simple[kSimpleCapacity];
    PspProjectedShadowRequest projected[kProjectedCapacity];
    PspShadowReceiverMesh receivers;
    PspShadowMap map;
    Metrics metrics;
    std::uint32_t generation;
    std::uint16_t simple_count;
    std::uint8_t projected_count;
    bool frame_active;

    void initialize();
    void begin_frame(std::uint32_t room_generation);
    bool submit_simple(const PspSimpleShadowRequest& request);
    bool submit_projected(const PspProjectedShadowRequest& request);
    bool configure_projected_map(
        std::uint16_t width,
        std::uint16_t height,
        std::uint32_t format,
        std::uint32_t map_bytes,
        std::uint32_t auxiliary_bytes,
        std::uint32_t content_signature);
    bool prepare_simple(const room::CollisionWorld& collision);
    bool prepare_movebg(const movebg::PspMoveBgWorld& world);
    void unload(std::uint32_t room_generation);
    bool consistent(std::uint32_t room_generation) const;
};

}  // namespace dusk::psp::shadow

#endif
