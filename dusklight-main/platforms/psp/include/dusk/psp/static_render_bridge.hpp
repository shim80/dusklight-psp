#ifndef DUSK_PSP_STATIC_RENDER_BRIDGE_HPP
#define DUSK_PSP_STATIC_RENDER_BRIDGE_HPP

#include "d/d_model_obj.h"
#include "dusk/psp/dpmd.hpp"
#include "dusk/psp/dpsm.hpp"

#include <cstdint>

namespace dusk::psp::render_bridge {

enum class Error : std::uint32_t {
    Ok = 0,
    NullOutput = 1,
    NullResource = 2,
    InvalidPackage = 3,
    MatrixNotFinite = 4,
    UnsupportedState = 5,
    BackendFailure = 6,
};

enum class Primitive : std::uint32_t {
    Triangles = 0,
    Lines = 1,
};

struct StaticRenderState {
    Primitive primitive;
    std::uint32_t texture_count;
    std::uint32_t pass_count;
    std::uint32_t index_bits;
    bool texture_psp8888_linear;
    bool opaque;
    bool depth_test;
    bool depth_write;
    bool back_face_culling;
    bool white_texture_modulation;
    bool blending;
    bool complex_alpha_test;
    bool fog;
    bool lighting;
    bool normals;
    bool animation;
    bool skinning;
    bool morphing;
    bool arbitrary_gx_tev;
};

struct StaticResourceBinding {
    const dpsm::PackageView* package;
    StaticRenderState state;
    const char* external_package_path;
    bool resource_is_external;
};

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

struct ModelMatrix {
    float column[4][4];
};

static_assert(sizeof(ModelMatrix) == 16 * sizeof(float));

struct PspStaticRenderCommand {
    const ::dMdl_obj_c* source_object;
    const dpsm::PackageView* package;
    const char* external_package_path;
    ModelMatrix model;
    StaticRenderState state;
};

struct DiagnosticResourceBinding {
    const dpmd::PackageView* package;
    StaticRenderState state;
    const char* external_package_path;
    bool resource_is_external;
};

struct PspDiagnosticRenderCommand {
    const ::dMdl_obj_c* source_object;
    const dpmd::PackageView* package;
    const char* external_package_path;
    ModelMatrix model;
    StaticRenderState state;
};

struct Metrics {
    std::uint32_t source_object_count;
    std::uint32_t adapter_calls;
    std::uint32_t commands_emitted;
    std::uint32_t commands_rejected;
    std::uint32_t backend_commands_received;
    std::uint32_t backend_draw_calls;
    std::uint32_t direct_submit_count;
    std::uint32_t unsupported_state_count;
    std::uint32_t allocations_during_frame;
    std::uintptr_t source_object_address;
    std::uintptr_t source_next_address;
    std::uint32_t source_matrix_hash;
    std::uint32_t adapted_matrix_hash;
    float source_matrix_values[12];
    float adapted_matrix_values[16];
    bool matrix_conversion_valid;
    bool resource_binding_external;
};

struct BackendMetrics {
    std::uint32_t draw_call_count;
    std::uint32_t cache_writeback_operations;
    std::uint32_t cache_invalidate_operations;
    bool matrices_finite;
};

StaticRenderState supported_state();
StaticRenderState supported_diagnostic_state(bool back_face_culling);
bool state_supported(const StaticRenderState& state);
bool diagnostic_state_supported(const StaticRenderState& state);
bool package_supported(const dpsm::PackageView& package);
bool diagnostic_package_supported(const dpmd::PackageView& package);
const char* error_name(Error error);

Error adapt_static_instance(
    const ::dMdl_obj_c& source,
    const StaticResourceBinding& binding,
    PspStaticRenderCommand* output,
    Metrics* metrics);

Error submit_static_command(
    const PspStaticRenderCommand& command,
    const Camera& camera,
    BackendMetrics* backend_metrics,
    Metrics* metrics);

Error adapt_diagnostic_instance(
    const ::dMdl_obj_c& source,
    const DiagnosticResourceBinding& binding,
    PspDiagnosticRenderCommand* output,
    Metrics* metrics);

Error submit_diagnostic_command(
    const PspDiagnosticRenderCommand& command,
    const Camera& camera,
    BackendMetrics* backend_metrics,
    Metrics* metrics);

}  // namespace dusk::psp::render_bridge

#endif
