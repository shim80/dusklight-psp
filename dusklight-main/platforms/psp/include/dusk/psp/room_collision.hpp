#ifndef DUSK_PSP_ROOM_COLLISION_HPP
#define DUSK_PSP_ROOM_COLLISION_HPP

#include "dusk/psp/room_package.hpp"

#include <cstdint>

namespace dusk::psp::room {

struct Vec3 {
    float x;
    float y;
    float z;
};

struct FloorHit {
    float height;
    Vec3 normal;
    std::uint16_t attribute;
    std::uint16_t triangle;
    bool hit;
};

struct LineHit {
    Vec3 position;
    Vec3 normal;
    float fraction;
    std::uint16_t triangle;
    bool hit;
};

struct ShadowReceiverTriangle {
    Vec3 vertices[3];
    Vec3 normal;
    std::uint16_t source_triangle;
    std::uint16_t attribute;
};

struct CollisionWorld {
    PackageView package;
    std::uint32_t floor_queries;
    std::uint32_t wall_queries;
    std::uint32_t wall_hits;
    std::uint32_t camera_queries;
    std::uint32_t camera_hits;
    std::uint32_t out_of_bounds_rejections;
};

bool initialize_collision_world(
    CollisionWorld* world, const void* bytes, std::uint32_t size);
bool outside_bounds(const CollisionWorld& world, const Vec3& position);
FloorHit find_floor(
    CollisionWorld* world,
    const Vec3& position,
    float maximum_above,
    float slope_limit_cosine);
bool resolve_horizontal(
    CollisionWorld* world,
    const Vec3& current,
    const Vec3& desired,
    float radius,
    float height,
    Vec3* resolved);
LineHit trace_camera(
    CollisionWorld* world, const Vec3& start, const Vec3& end);
std::uint32_t collect_shadow_receivers(
    const CollisionWorld& world,
    const Vec3& center,
    float radius,
    float maximum_drop,
    const Vec3& projection_direction,
    ShadowReceiverTriangle* output,
    std::uint32_t capacity,
    bool* overflow);

}  // namespace dusk::psp::room

#endif
