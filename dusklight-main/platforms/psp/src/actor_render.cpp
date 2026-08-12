#include "dusk/psp/actor_render.hpp"

#include <pspgu.h>
#include <pspgum.h>

#include <algorithm>
#include <cmath>
#include <cstdint>

namespace dusk::psp::actor {
namespace {

constexpr float kPi = 3.14159265358979323846f;
constexpr std::uint32_t kMaximumVertices = 160;
constexpr std::uint32_t kDynamicBudget = 24576;

struct Vertex {
    std::uint32_t color;
    float x;
    float y;
    float z;
};

static_assert(sizeof(Vertex) == 16);

void push_triangle(
    Vertex* vertices,
    std::uint32_t* count,
    std::uint32_t color,
    const room::Vec3& a,
    const room::Vec3& b,
    const room::Vec3& c) {
    vertices[(*count)++] = {color, a.x, a.y, a.z};
    vertices[(*count)++] = {color, b.x, b.y, b.z};
    vertices[(*count)++] = {color, c.x, c.y, c.z};
}

void push_quad(
    Vertex* vertices,
    std::uint32_t* count,
    std::uint32_t color,
    const room::Vec3& a,
    const room::Vec3& b,
    const room::Vec3& c,
    const room::Vec3& d) {
    push_triangle(vertices, count, color, a, b, c);
    push_triangle(vertices, count, color, a, c, d);
}

room::Vec3 add(
    const room::Vec3& a, const room::Vec3& b, float multiplier = 1.0f) {
    return {
        a.x + b.x * multiplier,
        a.y + b.y * multiplier,
        a.z + b.z * multiplier};
}

}  // namespace

bool draw_actor_backend(
    const ActorSystem& system, RenderMetrics* metrics) {
    if (metrics == nullptr || !actor_system_consistent(system)) {
        return false;
    }
    *metrics = {};
    Vertex* geometry = static_cast<Vertex*>(
        sceGuGetMemory(kMaximumVertices * sizeof(Vertex)));
    if (geometry == nullptr) return false;
    std::uint32_t count = 0;
    for (std::uint32_t index = 0; index < system.actor_count; ++index) {
        const GeyserActor& actor = system.actors[index];
        if (!actor.alive || count + 48 > kMaximumVertices) return false;
        const float yaw =
            static_cast<float>(actor.placement.rotation[1]) *
            (2.0f * kPi / 65536.0f);
        const room::Vec3 direction =
            actor.placement.type == 0
                ? room::Vec3{0.0f, 1.0f, 0.0f}
                : room::Vec3{std::sin(yaw), 0.0f, std::cos(yaw)};
        const room::Vec3 side =
            actor.placement.type == 0
                ? room::Vec3{1.0f, 0.0f, 0.0f}
                : room::Vec3{0.0f, 1.0f, 0.0f};
        const room::Vec3 side2 =
            actor.placement.type == 0
                ? room::Vec3{0.0f, 0.0f, 1.0f}
                : room::Vec3{std::cos(yaw), 0.0f, -std::sin(yaw)};
        const float radius =
            (actor.placement.type == 0 ? 70.0f : 30.0f) *
            actor.placement.scale.x;
        const float visible_strength = std::max(0.08f, actor.strength);
        const float length =
            (actor.placement.type == 0 ? 500.0f : 400.0f) *
            visible_strength;
        const room::Vec3 start = actor.placement.position;
        const room::Vec3 end = add(start, direction, length);
        const room::Vec3 a = add(start, side, radius);
        const room::Vec3 b = add(start, side, -radius);
        const room::Vec3 c = add(end, side, -radius * 0.35f);
        const room::Vec3 d = add(end, side, radius * 0.35f);
        const room::Vec3 e = add(start, side2, radius);
        const room::Vec3 f = add(start, side2, -radius);
        const room::Vec3 g = add(end, side2, -radius * 0.35f);
        const room::Vec3 h = add(end, side2, radius * 0.35f);
        const std::uint32_t alpha =
            actor.state == GeyserState::On ? 0xb0u :
            actor.state == GeyserState::Warning ? 0x78u : 0x48u;
        const std::uint32_t jet_color =
            (alpha << 24) | (actor.placement.type == 0
                ? 0x00d8f0d0u : 0x00f0c080u);
        const std::uint32_t base_color =
            actor.placement.type == 0 ? 0xff506858u : 0xff765840u;
        const room::Vec3 base_up = {0.0f, 18.0f, 0.0f};
        push_quad(
            geometry, &count, base_color,
            add(a, base_up, -1.0f), add(b, base_up, -1.0f), b, a);
        push_quad(geometry, &count, jet_color, a, b, c, d);
        push_quad(geometry, &count, jet_color, e, f, g, h);
    }
    sceGumMatrixMode(GU_MODEL);
    sceGumLoadIdentity();
    sceGuDisable(GU_TEXTURE_2D);
    sceGuDisable(GU_ALPHA_TEST);
    sceGuEnable(GU_BLEND);
    sceGuBlendFunc(
        GU_ADD, GU_SRC_ALPHA, GU_ONE_MINUS_SRC_ALPHA, 0, 0);
    sceGuDepthMask(GU_TRUE);
    sceGuShadeModel(GU_SMOOTH);
    sceGumDrawArray(
        GU_TRIANGLES,
        GU_COLOR_8888 | GU_VERTEX_32BITF | GU_TRANSFORM_3D,
        count, nullptr, geometry);
    metrics->actor_draw_calls = 1;
    metrics->vertices = count;

    const std::uint32_t particles = active_particle_count(system);
    if (particles != 0) {
        Vertex* points = static_cast<Vertex*>(
            sceGuGetMemory(particles * sizeof(Vertex)));
        if (points == nullptr) return false;
        std::uint32_t point = 0;
        for (const Particle& particle : system.particles) {
            if (!particle.active) continue;
            const std::uint32_t alpha = static_cast<std::uint32_t>(
                std::clamp(particle.alpha, 0.0f, 1.0f) * 180.0f);
            points[point++] = {
                (alpha << 24) | 0x00e8e0c0u,
                particle.position.x,
                particle.position.y,
                particle.position.z};
        }
        sceGumDrawArray(
            GU_POINTS,
            GU_COLOR_8888 | GU_VERTEX_32BITF | GU_TRANSFORM_3D,
            point, nullptr, points);
        metrics->particle_draw_calls = 1;
        metrics->dynamic_buffer_bytes += point * sizeof(Vertex);
    }
    sceGuDisable(GU_BLEND);
    sceGuDepthMask(GU_FALSE);
    metrics->dynamic_buffer_bytes += count * sizeof(Vertex);
    metrics->texture_edram_bytes = 0;
    metrics->visual_valid =
        metrics->actor_draw_calls <= 8 &&
        metrics->dynamic_buffer_bytes <= kDynamicBudget;
    return metrics->visual_valid;
}

}  // namespace dusk::psp::actor
