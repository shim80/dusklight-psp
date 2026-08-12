#include "dusk/psp/static_render_bridge.hpp"
#include "dusk/psp/static_mesh_3d.hpp"

namespace dusk::psp::render_bridge {

Error submit_static_command(
    const PspStaticRenderCommand& command,
    const Camera& camera,
    BackendMetrics* backend_metrics,
    Metrics* metrics) {
    if (metrics != nullptr) {
        ++metrics->backend_commands_received;
    }
    if (command.source_object == nullptr ||
        command.package == nullptr ||
        backend_metrics == nullptr ||
        metrics == nullptr ||
        !state_supported(command.state) ||
        !package_supported(*command.package)) {
        return Error::BackendFailure;
    }
    static_mesh_3d::RenderMetrics render_metrics = {};

    const dpsm::PackageView& package = *command.package;
    const static_mesh_3d::Camera mesh_camera = {
        camera.fov_degrees,
        camera.aspect,
        camera.near_plane,
        camera.far_plane,
        {camera.eye.x, camera.eye.y, camera.eye.z},
        {camera.center.x, camera.center.y, camera.center.z},
        {camera.up.x, camera.up.y, camera.up.z},
    };
    static_mesh_3d::ModelMatrix mesh_model = {};
    static_assert(sizeof(mesh_model) == sizeof(command.model));
    for (std::size_t column = 0; column < 4; ++column) {
        for (std::size_t row = 0; row < 4; ++row) {
            mesh_model.column[column][row] =
                command.model.column[column][row];
        }
    }
    static_assert(
        sizeof(dpsm::Vertex) ==
        sizeof(static_mesh_3d::TexturedVertex));
    static_assert(
        alignof(dpsm::Vertex) ==
        alignof(static_mesh_3d::TexturedVertex));

    const static_mesh_3d::MeshView mesh = {
        reinterpret_cast<const static_mesh_3d::TexturedVertex*>(
            package.vertices),
        package.vertex_count,
        package.vertex_bytes,
        package.indices,
        package.index_count,
        package.index_bytes,
    };
    const static_mesh_3d::TextureView texture = {
        package.texture,
        package.texture_width,
        package.texture_height,
        package.texture_stride,
        package.texture_bytes,
    };

    static_mesh_3d::writeback_range_for_ge(
        package.vertices, package.vertex_bytes, &render_metrics);
    static_mesh_3d::writeback_range_for_ge(
        package.indices, package.index_bytes, &render_metrics);
    static_mesh_3d::writeback_range_for_ge(
        package.texture, package.texture_bytes, &render_metrics);

    if (!static_mesh_3d::configure_3d(
            mesh_camera, mesh_model, &render_metrics) ||
        !static_mesh_3d::bind_texture(texture) ||
        !static_mesh_3d::submit_indexed(mesh, &render_metrics)) {
        return Error::BackendFailure;
    }
    *backend_metrics = {
        render_metrics.draw_call_count,
        render_metrics.cache_writeback_operations,
        render_metrics.cache_invalidate_operations,
        render_metrics.matrices_finite,
    };
    ++metrics->backend_draw_calls;
    return Error::Ok;
}

Error submit_diagnostic_command(
    const PspDiagnosticRenderCommand& command,
    const Camera& camera,
    BackendMetrics* backend_metrics,
    Metrics* metrics) {
    if (metrics != nullptr) {
        ++metrics->backend_commands_received;
    }
    if (command.source_object == nullptr ||
        command.package == nullptr ||
        backend_metrics == nullptr ||
        metrics == nullptr ||
        !diagnostic_state_supported(command.state) ||
        !diagnostic_package_supported(*command.package)) {
        return Error::BackendFailure;
    }
    static_mesh_3d::RenderMetrics render_metrics = {};
    const static_mesh_3d::Camera mesh_camera = {
        camera.fov_degrees,
        camera.aspect,
        camera.near_plane,
        camera.far_plane,
        {camera.eye.x, camera.eye.y, camera.eye.z},
        {camera.center.x, camera.center.y, camera.center.z},
        {camera.up.x, camera.up.y, camera.up.z},
    };
    static_mesh_3d::ModelMatrix mesh_model = {};
    for (std::size_t column = 0; column < 4; ++column) {
        for (std::size_t row = 0; row < 4; ++row) {
            mesh_model.column[column][row] =
                command.model.column[column][row];
        }
    }
    if (!static_mesh_3d::configure_3d(
            mesh_camera, mesh_model, &render_metrics)) {
        return Error::BackendFailure;
    }
    static_mesh_3d::set_back_face_culling(
        command.state.back_face_culling);
    for (std::uint32_t index = 0;
         index < command.package->chunk_count;
         ++index) {
        const dpmd::ChunkView& chunk = command.package->chunks[index];
        static_assert(
            sizeof(dpmd::Vertex) ==
            sizeof(static_mesh_3d::ColoredVertex));
        const static_mesh_3d::ColoredMeshView mesh = {
            reinterpret_cast<const static_mesh_3d::ColoredVertex*>(
                chunk.vertices),
            chunk.vertex_count,
            chunk.vertex_bytes,
            chunk.indices,
            chunk.index_count,
            chunk.index_bytes,
        };
        static_mesh_3d::writeback_range_for_ge(
            chunk.vertices, chunk.vertex_bytes, &render_metrics);
        static_mesh_3d::writeback_range_for_ge(
            chunk.indices, chunk.index_bytes, &render_metrics);
        if (!static_mesh_3d::submit_colored_indexed(
                mesh, &render_metrics)) {
            return Error::BackendFailure;
        }
    }
    *backend_metrics = {
        render_metrics.draw_call_count,
        render_metrics.cache_writeback_operations,
        render_metrics.cache_invalidate_operations,
        render_metrics.matrices_finite,
    };
    metrics->backend_draw_calls += render_metrics.draw_call_count;
    return Error::Ok;
}

}  // namespace dusk::psp::render_bridge
