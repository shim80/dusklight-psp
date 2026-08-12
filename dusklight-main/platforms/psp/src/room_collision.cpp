#include "dusk/psp/room_collision.hpp"

#include <algorithm>
#include <cmath>
#include <cstring>

namespace dusk::psp::room {
namespace {

Vec3 vertex_at(const CollisionWorld& world, std::uint16_t index) {
    const std::uint8_t* bytes = world.package.bytes;
    const std::uint32_t offset = read_u32(bytes + 68);
    const std::uint8_t* vertex = bytes + offset + index * 12;
    return {read_f32(vertex), read_f32(vertex + 4), read_f32(vertex + 8)};
}

const std::uint8_t* triangle_at(
    const CollisionWorld& world, std::uint32_t index) {
    const std::uint8_t* bytes = world.package.bytes;
    return bytes + read_u32(bytes + 76) + index * read_u32(bytes + 80);
}

Vec3 subtract(const Vec3& a, const Vec3& b) {
    return {a.x - b.x, a.y - b.y, a.z - b.z};
}

Vec3 add(const Vec3& a, const Vec3& b) {
    return {a.x + b.x, a.y + b.y, a.z + b.z};
}

Vec3 scale(const Vec3& value, float amount) {
    return {value.x * amount, value.y * amount, value.z * amount};
}

float dot(const Vec3& a, const Vec3& b) {
    return a.x * b.x + a.y * b.y + a.z * b.z;
}

Vec3 cross(const Vec3& a, const Vec3& b) {
    return {
        a.y * b.z - a.z * b.y,
        a.z * b.x - a.x * b.z,
        a.x * b.y - a.y * b.x,
    };
}

bool barycentric_xz(
    const Vec3& a,
    const Vec3& b,
    const Vec3& c,
    float x,
    float z,
    float* wa,
    float* wb,
    float* wc) {
    const float denominator =
        (b.z - c.z) * (a.x - c.x) +
        (c.x - b.x) * (a.z - c.z);
    if (std::fabs(denominator) < 1.0e-6f) {
        return false;
    }
    *wa = ((b.z - c.z) * (x - c.x) +
           (c.x - b.x) * (z - c.z)) / denominator;
    *wb = ((c.z - a.z) * (x - c.x) +
           (a.x - c.x) * (z - c.z)) / denominator;
    *wc = 1.0f - *wa - *wb;
    return *wa >= -1.0e-4f && *wb >= -1.0e-4f && *wc >= -1.0e-4f;
}

template <typename Visitor>
void visit_cell(
    const CollisionWorld& world,
    float x,
    float z,
    const Visitor& visitor) {
    const std::uint8_t* bytes = world.package.bytes;
    const float cell_size = read_f32(bytes + 40);
    const float minimum_x = read_f32(bytes + 44);
    const float minimum_z = read_f32(bytes + 52);
    const std::uint32_t cells_x = read_u32(bytes + 24);
    const std::uint32_t cells_z = read_u32(bytes + 28);
    if (x < minimum_x || z < minimum_z) {
        return;
    }
    const std::uint32_t cell_x =
        static_cast<std::uint32_t>((x - minimum_x) / cell_size);
    const std::uint32_t cell_z =
        static_cast<std::uint32_t>((z - minimum_z) / cell_size);
    if (cell_x >= cells_x || cell_z >= cells_z) {
        return;
    }
    const std::uint32_t cell_offset = read_u32(bytes + 84);
    const std::uint32_t reference_offset = read_u32(bytes + 92);
    const std::uint8_t* cell =
        bytes + cell_offset + (cell_z * cells_x + cell_x) * 16;
    const std::uint32_t first = read_u32(cell);
    const std::uint32_t count = read_u32(cell + 4);
    for (std::uint32_t index = 0; index < count; ++index) {
        visitor(read_u16(bytes + reference_offset + (first + index) * 2));
    }
}

}  // namespace

bool initialize_collision_world(
    CollisionWorld* world, const void* bytes, std::uint32_t size) {
    if (world == nullptr) {
        return false;
    }
    PackageView view = {};
    if (validate_dpcl(bytes, size, &view) != PackageError::Ok) {
        return false;
    }
    std::memset(world, 0, sizeof(*world));
    world->package = view;
    return true;
}

bool outside_bounds(const CollisionWorld& world, const Vec3& position) {
    const std::uint8_t* bytes = world.package.bytes;
    return position.x < read_f32(bytes + 44) ||
           position.y < read_f32(bytes + 48) ||
           position.z < read_f32(bytes + 52) ||
           position.x > read_f32(bytes + 56) ||
           position.y > read_f32(bytes + 60) ||
           position.z > read_f32(bytes + 64);
}

FloorHit find_floor(
    CollisionWorld* world,
    const Vec3& position,
    float maximum_above,
    float slope_limit_cosine) {
    FloorHit best = {};
    best.height = -INFINITY;
    if (world == nullptr) {
        return best;
    }
    ++world->floor_queries;
    visit_cell(*world, position.x, position.z, [&](std::uint16_t index) {
        const std::uint8_t* triangle = triangle_at(*world, index);
        const Vec3 normal = {
            read_f32(triangle + 8),
            read_f32(triangle + 12),
            read_f32(triangle + 16),
        };
        if (normal.y < slope_limit_cosine) {
            return;
        }
        const Vec3 a = vertex_at(*world, read_u16(triangle));
        const Vec3 b = vertex_at(*world, read_u16(triangle + 2));
        const Vec3 c = vertex_at(*world, read_u16(triangle + 4));
        float wa = 0.0f;
        float wb = 0.0f;
        float wc = 0.0f;
        if (!barycentric_xz(
                a, b, c, position.x, position.z, &wa, &wb, &wc)) {
            return;
        }
        const float height = wa * a.y + wb * b.y + wc * c.y;
        if (height <= position.y + maximum_above && height > best.height) {
            best = {
                height,
                normal,
                read_u16(triangle + 6),
                index,
                true,
            };
        }
    });
    return best;
}

bool resolve_horizontal(
    CollisionWorld* world,
    const Vec3& current,
    const Vec3& desired,
    float radius,
    float height,
    Vec3* resolved) {
    if (world == nullptr || resolved == nullptr) {
        return false;
    }
    ++world->wall_queries;
    *resolved = desired;
    bool hit = false;
    visit_cell(*world, desired.x, desired.z, [&](std::uint16_t index) {
        const std::uint8_t* triangle = triangle_at(*world, index);
        Vec3 normal = {
            read_f32(triangle + 8),
            read_f32(triangle + 12),
            read_f32(triangle + 16),
        };
        if (std::fabs(normal.y) > 0.65f) {
            return;
        }
        const Vec3 a = vertex_at(*world, read_u16(triangle));
        const Vec3 b = vertex_at(*world, read_u16(triangle + 2));
        const Vec3 c = vertex_at(*world, read_u16(triangle + 4));
        const float min_y = std::min(a.y, std::min(b.y, c.y));
        const float max_y = std::max(a.y, std::max(b.y, c.y));
        if (desired.y + height < min_y || desired.y > max_y) {
            return;
        }
        const float plane = read_f32(triangle + 20);
        const float current_distance = dot(normal, current) + plane;
        const float desired_distance = dot(normal, desired) + plane;
        if (std::fabs(desired_distance) >= radius ||
            std::fabs(current_distance) < std::fabs(desired_distance)) {
            return;
        }
        const float push = radius - std::fabs(desired_distance);
        const float sign = desired_distance >= 0.0f ? 1.0f : -1.0f;
        resolved->x += normal.x * push * sign;
        resolved->z += normal.z * push * sign;
        hit = true;
    });
    if (outside_bounds(*world, *resolved)) {
        *resolved = current;
        ++world->out_of_bounds_rejections;
        hit = true;
    }
    if (hit) {
        ++world->wall_hits;
    }
    return hit;
}

LineHit trace_camera(
    CollisionWorld* world, const Vec3& start, const Vec3& end) {
    LineHit best = {};
    best.fraction = 1.0f;
    if (world == nullptr) {
        return best;
    }
    ++world->camera_queries;
    const Vec3 direction = subtract(end, start);
    const std::uint32_t triangles = read_u32(world->package.bytes + 20);
    for (std::uint32_t index = 0; index < triangles; ++index) {
        const std::uint8_t* triangle = triangle_at(*world, index);
        const Vec3 a = vertex_at(*world, read_u16(triangle));
        const Vec3 b = vertex_at(*world, read_u16(triangle + 2));
        const Vec3 c = vertex_at(*world, read_u16(triangle + 4));
        const Vec3 edge1 = subtract(b, a);
        const Vec3 edge2 = subtract(c, a);
        const Vec3 p = cross(direction, edge2);
        const float determinant = dot(edge1, p);
        if (std::fabs(determinant) < 1.0e-6f) {
            continue;
        }
        const float inverse = 1.0f / determinant;
        const Vec3 t = subtract(start, a);
        const float u = dot(t, p) * inverse;
        if (u < 0.0f || u > 1.0f) {
            continue;
        }
        const Vec3 q = cross(t, edge1);
        const float v = dot(direction, q) * inverse;
        const float fraction = dot(edge2, q) * inverse;
        if (v < 0.0f || u + v > 1.0f ||
            fraction < 0.0f || fraction >= best.fraction) {
            continue;
        }
        best.position = add(start, scale(direction, fraction));
        best.normal = {
            read_f32(triangle + 8),
            read_f32(triangle + 12),
            read_f32(triangle + 16),
        };
        best.fraction = fraction;
        best.triangle = static_cast<std::uint16_t>(index);
        best.hit = true;
    }
    if (best.hit) {
        ++world->camera_hits;
    }
    return best;
}

std::uint32_t collect_shadow_receivers(
    const CollisionWorld& world,
    const Vec3& center,
    float radius,
    float maximum_drop,
    const Vec3& projection_direction,
    ShadowReceiverTriangle* output,
    std::uint32_t capacity,
    bool* overflow) {
    if (overflow != nullptr) {
        *overflow = false;
    }
    if (world.package.bytes == nullptr || output == nullptr ||
        capacity == 0 || !std::isfinite(center.x) ||
        !std::isfinite(center.y) || !std::isfinite(center.z) ||
        !std::isfinite(radius) || radius <= 0.0f ||
        !std::isfinite(maximum_drop) || maximum_drop <= 0.0f) {
        return 0;
    }
    const float radius_squared = radius * radius;
    const std::uint32_t triangles = read_u32(world.package.bytes + 20);
    std::uint32_t count = 0;
    for (std::uint32_t index = 0; index < triangles; ++index) {
        const std::uint8_t* triangle = triangle_at(world, index);
        const Vec3 normal = {
            read_f32(triangle + 8),
            read_f32(triangle + 12),
            read_f32(triangle + 16),
        };
        const float facing = dot(normal, projection_direction);
        if (!std::isfinite(facing) || normal.y < 0.2f ||
            facing >= -0.2f) {
            continue;
        }
        const Vec3 vertices[3] = {
            vertex_at(world, read_u16(triangle)),
            vertex_at(world, read_u16(triangle + 2)),
            vertex_at(world, read_u16(triangle + 4)),
        };
        float minimum_x = vertices[0].x;
        float maximum_x = vertices[0].x;
        float minimum_y = vertices[0].y;
        float maximum_y = vertices[0].y;
        float minimum_z = vertices[0].z;
        float maximum_z = vertices[0].z;
        for (std::uint32_t vertex = 1; vertex < 3; ++vertex) {
            minimum_x = std::min(minimum_x, vertices[vertex].x);
            maximum_x = std::max(maximum_x, vertices[vertex].x);
            minimum_y = std::min(minimum_y, vertices[vertex].y);
            maximum_y = std::max(maximum_y, vertices[vertex].y);
            minimum_z = std::min(minimum_z, vertices[vertex].z);
            maximum_z = std::max(maximum_z, vertices[vertex].z);
        }
        const float closest_x =
            std::clamp(center.x, minimum_x, maximum_x);
        const float closest_z =
            std::clamp(center.z, minimum_z, maximum_z);
        const float dx = closest_x - center.x;
        const float dz = closest_z - center.z;
        if (dx * dx + dz * dz > radius_squared ||
            minimum_y > center.y + radius * 0.25f ||
            maximum_y < center.y - maximum_drop) {
            continue;
        }
        if (count >= capacity) {
            if (overflow != nullptr) {
                *overflow = true;
            }
            continue;
        }
        ShadowReceiverTriangle& receiver = output[count++];
        receiver.normal = normal;
        receiver.source_triangle =
            static_cast<std::uint16_t>(index);
        receiver.attribute = read_u16(triangle + 6);
        for (std::uint32_t vertex = 0; vertex < 3; ++vertex) {
            receiver.vertices[vertex] = add(
                vertices[vertex], scale(normal, 0.5f));
        }
    }
    return count;
}

}  // namespace dusk::psp::room
