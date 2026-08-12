#include "dusk/psp/static_mesh_3d.hpp"

#include <pspgu.h>
#include <pspgum.h>
#include <psputils.h>

#include <cmath>
#include <cstdint>
#include <cstring>

namespace dusk::psp::static_mesh_3d {
namespace {

bool finite_value(float value) {
    return std::isfinite(value);
}

bool finite_vec3(const Vec3& value) {
    return finite_value(value.x) &&
           finite_value(value.y) &&
           finite_value(value.z);
}

bool finite_matrix(const ScePspFMatrix4& matrix) {
    static_assert(sizeof(ScePspFMatrix4) == sizeof(float) * 16);
    float values[16] = {};
    std::memcpy(values, &matrix, sizeof(values));
    for (float value : values) {
        if (!finite_value(value)) {
            return false;
        }
    }
    return true;
}

bool store_current_matrix(RenderMetrics* metrics) {
    ScePspFMatrix4 matrix = {};
    sceGumStoreMatrix(&matrix);
    const bool finite = finite_matrix(matrix);
    if (metrics != nullptr) {
        metrics->matrices_finite =
            metrics->matrices_finite && finite;
    }
    return finite;
}

ScePspFVector3 to_psp_vector(const Vec3& value) {
    return {value.x, value.y, value.z};
}

bool configure_camera(const Camera& camera, RenderMetrics* metrics) {
    if (!valid_camera(camera) || metrics == nullptr) {
        return false;
    }
    metrics->matrices_finite = true;

    sceGuDepthFunc(GU_GEQUAL);
    sceGuDepthMask(GU_FALSE);
    sceGuEnable(GU_DEPTH_TEST);
    sceGuFrontFace(GU_CW);
    sceGuEnable(GU_CULL_FACE);
    sceGuEnable(GU_CLIP_PLANES);
    sceGuEnable(GU_TEXTURE_2D);
    sceGuDisable(GU_DITHER);
    sceGuDisable(GU_BLEND);
    sceGuShadeModel(GU_FLAT);

    sceGumMatrixMode(GU_PROJECTION);
    sceGumLoadIdentity();
    sceGumPerspective(
        camera.fov_degrees,
        camera.aspect,
        camera.near_plane,
        camera.far_plane);
    if (!store_current_matrix(metrics)) {
        return false;
    }

    ScePspFVector3 eye = to_psp_vector(camera.eye);
    ScePspFVector3 center = to_psp_vector(camera.center);
    ScePspFVector3 up = to_psp_vector(camera.up);
    sceGumMatrixMode(GU_VIEW);
    sceGumLoadIdentity();
    sceGumLookAt(&eye, &center, &up);
    return store_current_matrix(metrics);
}

}  // namespace

bool valid_camera(const Camera& camera) {
    return finite_value(camera.fov_degrees) &&
           finite_value(camera.aspect) &&
           finite_value(camera.near_plane) &&
           finite_value(camera.far_plane) &&
           finite_vec3(camera.eye) &&
           finite_vec3(camera.center) &&
           finite_vec3(camera.up) &&
           camera.fov_degrees > 0.0f &&
           camera.fov_degrees < 180.0f &&
           camera.aspect > 0.0f &&
           camera.near_plane > 0.0f &&
           camera.far_plane > camera.near_plane;
}

bool valid_model(const ModelTransform& model) {
    return finite_vec3(model.translation) &&
           finite_vec3(model.rotation_radians) &&
           finite_vec3(model.scale) &&
           model.scale.x != 0.0f &&
           model.scale.y != 0.0f &&
           model.scale.z != 0.0f;
}

bool valid_model_matrix(const ModelMatrix& model) {
    for (const auto& column : model.column) {
        for (float value : column) {
            if (!finite_value(value)) {
                return false;
            }
        }
    }
    return true;
}

void writeback_range_for_ge(
    const void* address, std::uint32_t bytes, RenderMetrics* metrics) {
    if (address == nullptr || bytes == 0) {
        return;
    }
    sceKernelDcacheWritebackRange(address, bytes);
    if (metrics != nullptr) {
        ++metrics->cache_writeback_operations;
    }
}

void prepare_readback_range(
    void* address, std::uint32_t bytes, RenderMetrics* metrics) {
    if (address == nullptr || bytes == 0) {
        return;
    }
    sceKernelDcacheWritebackInvalidateRange(address, bytes);
    if (metrics != nullptr) {
        ++metrics->cache_writeback_operations;
        ++metrics->cache_invalidate_operations;
    }
}

void finish_readback_range(
    void* address, std::uint32_t bytes, RenderMetrics* metrics) {
    if (address == nullptr || bytes == 0) {
        return;
    }
    sceKernelDcacheInvalidateRange(address, bytes);
    if (metrics != nullptr) {
        ++metrics->cache_invalidate_operations;
    }
}

bool configure_3d(
    const Camera& camera,
    const ModelTransform& model,
    RenderMetrics* metrics) {
    if (!valid_model(model) || !configure_camera(camera, metrics)) {
        return false;
    }

    const ScePspFVector3 translation =
        to_psp_vector(model.translation);
    const ScePspFVector3 rotation =
        to_psp_vector(model.rotation_radians);
    const ScePspFVector3 scale = to_psp_vector(model.scale);
    sceGumMatrixMode(GU_MODEL);
    sceGumLoadIdentity();
    sceGumTranslate(&translation);
    sceGumRotateXYZ(&rotation);
    sceGumScale(&scale);
    return store_current_matrix(metrics);
}

bool configure_3d(
    const Camera& camera,
    const ModelMatrix& model,
    RenderMetrics* metrics) {
    if (!valid_model_matrix(model) || !configure_camera(camera, metrics)) {
        return false;
    }
    static_assert(sizeof(ModelMatrix) == sizeof(ScePspFMatrix4));
    ScePspFMatrix4 psp_matrix = {};
    std::memcpy(&psp_matrix, &model, sizeof(psp_matrix));
    sceGumMatrixMode(GU_MODEL);
    sceGumLoadMatrix(&psp_matrix);
    return store_current_matrix(metrics);
}

bool load_identity_model(RenderMetrics* metrics) {
    if (metrics == nullptr) {
        return false;
    }
    sceGumMatrixMode(GU_MODEL);
    sceGumLoadIdentity();
    return store_current_matrix(metrics);
}

void set_back_face_culling(bool enabled) {
    if (enabled) {
        sceGuEnable(GU_CULL_FACE);
    } else {
        sceGuDisable(GU_CULL_FACE);
    }
}

bool bind_texture(const TextureView& texture) {
    if (texture.pixels == nullptr ||
        texture.width == 0 ||
        texture.height == 0 ||
        texture.stride < texture.width ||
        texture.bytes <
            texture.stride * texture.height * sizeof(std::uint32_t)) {
        return false;
    }
    sceGuEnable(GU_TEXTURE_2D);
    sceGuTexMode(GU_PSM_8888, 0, 0, GU_FALSE);
    sceGuTexImage(
        0,
        texture.width,
        texture.height,
        texture.stride,
        texture.pixels);
    sceGuTexFunc(GU_TFX_REPLACE, GU_TCC_RGBA);
    sceGuTexFilter(GU_NEAREST, GU_NEAREST);
    sceGuTexWrap(GU_CLAMP, GU_CLAMP);
    sceGuTexScale(1.0f, 1.0f);
    sceGuTexOffset(0.0f, 0.0f);
    return true;
}

bool submit_indexed(const MeshView& mesh, RenderMetrics* metrics) {
    if (metrics == nullptr ||
        mesh.vertices == nullptr ||
        mesh.indices == nullptr ||
        mesh.vertex_count == 0 ||
        mesh.index_count == 0 ||
        mesh.index_count % 3 != 0 ||
        mesh.vertex_bytes <
            mesh.vertex_count * sizeof(TexturedVertex) ||
        mesh.index_bytes <
            mesh.index_count * sizeof(std::uint16_t)) {
        return false;
    }
    constexpr int kVertexType =
        GU_TEXTURE_32BITF |
        GU_VERTEX_32BITF |
        GU_INDEX_16BIT |
        GU_TRANSFORM_3D;
    sceGumDrawArray(
        GU_TRIANGLES,
        kVertexType,
        mesh.index_count,
        mesh.indices,
        mesh.vertices);
    ++metrics->draw_call_count;
    return true;
}

bool submit_colored_indexed(
    const ColoredMeshView& mesh, RenderMetrics* metrics) {
    if (metrics == nullptr ||
        mesh.vertices == nullptr ||
        mesh.indices == nullptr ||
        mesh.vertex_count == 0 ||
        mesh.index_count == 0 ||
        mesh.index_count % 3 != 0 ||
        mesh.vertex_bytes <
            mesh.vertex_count * sizeof(ColoredVertex) ||
        mesh.index_bytes <
            mesh.index_count * sizeof(std::uint16_t)) {
        return false;
    }
    sceGuDisable(GU_TEXTURE_2D);
    sceGuShadeModel(GU_SMOOTH);
    constexpr int kVertexType =
        GU_COLOR_8888 |
        GU_VERTEX_32BITF |
        GU_INDEX_16BIT |
        GU_TRANSFORM_3D;
    sceGumDrawArray(
        GU_TRIANGLES,
        kVertexType,
        mesh.index_count,
        mesh.indices,
        mesh.vertices);
    ++metrics->draw_call_count;
    return true;
}

}  // namespace dusk::psp::static_mesh_3d
