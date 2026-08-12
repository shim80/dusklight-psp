#ifndef DUSK_PSP_MOVEBG_RUNTIME_HPP
#define DUSK_PSP_MOVEBG_RUNTIME_HPP

#include "dusk/psp/room_collision.hpp"

#include <cstdint>

namespace dusk::psp::movebg {

struct Matrix34 {
    float value[3][4];
};

struct Handle {
    std::uint16_t slot;
    std::uint16_t generation;
};

struct Metrics {
    std::uint32_t creates;
    std::uint32_t peak_handles;
    std::uint32_t updates;
    std::uint32_t commits;
    std::uint32_t deletes;
    std::uint32_t floor_queries;
    std::uint32_t wall_queries;
    std::uint32_t camera_queries;
    std::uint32_t carry_updates;
    std::uint32_t stale_handles;
    std::uint32_t calls_after_delete;
    std::uint32_t invalid_matrices;
    std::uint32_t dynamic_collision_frame_lag;
};

class PspMoveBgWorld {
public:
    static constexpr std::uint16_t kCapacity = 16;

    void initialize();
    void shutdown();
    bool create(
        const void* dpcl, std::uint32_t size,
        const Matrix34& matrix, Handle* handle);
    bool update(Handle handle, const Matrix34& matrix);
    bool destroy(Handle handle);
    bool valid(Handle handle) const;

    room::FloorHit find_floor(
        Handle handle, const room::Vec3& world_position,
        float maximum_above, float slope_limit_cosine);
    bool resolve_horizontal(
        Handle handle, const room::Vec3& current,
        const room::Vec3& desired, float radius, float height,
        room::Vec3* resolved);
    room::LineHit trace_camera(
        Handle handle, const room::Vec3& start,
        const room::Vec3& end);
    bool carry_point(
        Handle handle, const room::Vec3& previous_world,
        room::Vec3* current_world);
    std::uint32_t collect_shadow_receivers(
        const room::Vec3& world_center,
        float radius,
        float maximum_drop,
        const room::Vec3& world_projection_direction,
        room::ShadowReceiverTriangle* output,
        std::uint32_t capacity,
        bool* overflow) const;

    const Matrix34* matrix(Handle handle) const;
    Metrics metrics = {};

private:
    struct Slot {
        room::CollisionWorld collision;
        Matrix34 previous;
        Matrix34 current;
        Matrix34 inverse;
        std::uint16_t generation;
        bool active;
    };

    Slot* resolve(Handle handle);
    const Slot* resolve(Handle handle) const;

    Slot slots_[kCapacity] = {};
    bool initialized_ = false;
};

Matrix34 identity_matrix();
bool source_compatibility_surface_valid();

}  // namespace dusk::psp::movebg

#endif
