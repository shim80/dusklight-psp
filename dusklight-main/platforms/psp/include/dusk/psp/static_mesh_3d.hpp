#ifndef DUSK_PSP_STATIC_MESH_3D_HPP
#define DUSK_PSP_STATIC_MESH_3D_HPP

#include <cstddef>
#include <cstdint>

namespace dusk::psp::static_mesh_3d {

struct TexturedVertex {
    float u;
    float v;
    float x;
    float y;
    float z;
};

struct ColoredVertex {
    std::uint32_t color;
    float x;
    float y;
    float z;
};

static_assert(sizeof(ColoredVertex) == 16);
static_assert(offsetof(ColoredVertex, color) == 0);
static_assert(offsetof(ColoredVertex, x) == 4);

static_assert(sizeof(TexturedVertex) == 20);
static_assert(alignof(TexturedVertex) == 4);
static_assert(offsetof(TexturedVertex, u) == 0);
static_assert(offsetof(TexturedVertex, v) == 4);
static_assert(offsetof(TexturedVertex, x) == 8);
static_assert(offsetof(TexturedVertex, y) == 12);
static_assert(offsetof(TexturedVertex, z) == 16);
static_assert(sizeof(std::uint16_t) == 2);
static_assert(alignof(std::uint16_t) == 2);

struct Vec3 {
    float x;
    float y;
    float z;
};

struct Camera {
    float fov_degrees;
    float aspect;
    float near_plane;
    float far_plane;
    Vec3 eye;
    Vec3 center;
    Vec3 up;
};

struct ModelTransform {
    Vec3 translation;
    Vec3 rotation_radians;
    Vec3 scale;
};

struct ModelMatrix {
    float column[4][4];
};

static_assert(sizeof(ModelMatrix) == 16 * sizeof(float));
static_assert(alignof(ModelMatrix) == alignof(float));

struct MeshView {
    const TexturedVertex* vertices;
    std::uint32_t vertex_count;
    std::uint32_t vertex_bytes;
    const std::uint16_t* indices;
    std::uint32_t index_count;
    std::uint32_t index_bytes;
};

struct ColoredMeshView {
    const ColoredVertex* vertices;
    std::uint32_t vertex_count;
    std::uint32_t vertex_bytes;
    const std::uint16_t* indices;
    std::uint32_t index_count;
    std::uint32_t index_bytes;
};

struct TextureView {
    const std::uint32_t* pixels;
    std::uint32_t width;
    std::uint32_t height;
    std::uint32_t stride;
    std::uint32_t bytes;
};

struct RenderMetrics {
    std::uint32_t draw_call_count;
    std::uint32_t cache_writeback_operations;
    std::uint32_t cache_invalidate_operations;
    bool matrices_finite;
};

bool valid_camera(const Camera& camera);
bool valid_model(const ModelTransform& model);
bool valid_model_matrix(const ModelMatrix& model);

void writeback_range_for_ge(
    const void* address, std::uint32_t bytes, RenderMetrics* metrics);
void prepare_readback_range(
    void* address, std::uint32_t bytes, RenderMetrics* metrics);
void finish_readback_range(
    void* address, std::uint32_t bytes, RenderMetrics* metrics);

bool configure_3d(
    const Camera& camera,
    const ModelTransform& model,
    RenderMetrics* metrics);
bool configure_3d(
    const Camera& camera,
    const ModelMatrix& model,
    RenderMetrics* metrics);
bool load_identity_model(RenderMetrics* metrics);
void set_back_face_culling(bool enabled);
bool bind_texture(const TextureView& texture);
bool submit_indexed(const MeshView& mesh, RenderMetrics* metrics);
bool submit_colored_indexed(
    const ColoredMeshView& mesh, RenderMetrics* metrics);

}  // namespace dusk::psp::static_mesh_3d

#endif
