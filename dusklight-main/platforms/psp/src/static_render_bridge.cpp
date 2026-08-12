#include "dusk/psp/static_render_bridge.hpp"

#include <cmath>
#include <cstddef>
#include <cstdint>

namespace dusk::psp::render_bridge {
namespace {

std::uint32_t hash_bytes(const void* data, std::size_t size) {
    const auto* bytes = static_cast<const std::uint8_t*>(data);
    std::uint32_t hash = 2166136261u;
    for (std::size_t index = 0; index < size; ++index) {
        hash ^= bytes[index];
        hash *= 16777619u;
    }
    return hash;
}

bool source_matrix_finite(const Mtx matrix) {
    for (std::size_t row = 0; row < 3; ++row) {
        for (std::size_t column = 0; column < 4; ++column) {
            if (!std::isfinite(matrix[row][column])) {
                return false;
            }
        }
    }
    return true;
}

ModelMatrix adapt_matrix(const Mtx matrix) {
    ModelMatrix result = {};
    for (std::size_t column = 0; column < 3; ++column) {
        for (std::size_t row = 0; row < 3; ++row) {
            result.column[column][row] = matrix[row][column];
        }
    }
    result.column[3][0] = matrix[0][3];
    result.column[3][1] = matrix[1][3];
    result.column[3][2] = matrix[2][3];
    result.column[3][3] = 1.0f;
    return result;
}

bool matrix_mapping_valid(
    const Mtx source,
    const ModelMatrix& adapted) {
    for (std::size_t column = 0; column < 3; ++column) {
        for (std::size_t row = 0; row < 3; ++row) {
            if (adapted.column[column][row] != source[row][column]) {
                return false;
            }
        }
        if (adapted.column[column][3] != 0.0f) {
            return false;
        }
    }
    return adapted.column[3][0] == source[0][3] &&
           adapted.column[3][1] == source[1][3] &&
           adapted.column[3][2] == source[2][3] &&
           adapted.column[3][3] == 1.0f;
}

void reject(Error error, Metrics* metrics) {
    if (metrics == nullptr) {
        return;
    }
    ++metrics->commands_rejected;
    if (error == Error::UnsupportedState) {
        ++metrics->unsupported_state_count;
    }
}

}  // namespace

StaticRenderState supported_state() {
    return {
        Primitive::Triangles,
        1,
        1,
        16,
        true,
        true,
        true,
        true,
        true,
        true,
        false,
        false,
        false,
        false,
        false,
        false,
        false,
        false,
        false,
    };
}

StaticRenderState supported_diagnostic_state(bool back_face_culling) {
    StaticRenderState state = supported_state();
    state.texture_count = 0;
    state.texture_psp8888_linear = false;
    state.back_face_culling = back_face_culling;
    state.white_texture_modulation = false;
    return state;
}

bool state_supported(const StaticRenderState& state) {
    return state.primitive == Primitive::Triangles &&
           state.texture_count == 1 &&
           state.pass_count == 1 &&
           state.index_bits == 16 &&
           state.texture_psp8888_linear &&
           state.opaque &&
           state.depth_test &&
           state.depth_write &&
           state.back_face_culling &&
           state.white_texture_modulation &&
           !state.blending &&
           !state.complex_alpha_test &&
           !state.fog &&
           !state.lighting &&
           !state.normals &&
           !state.animation &&
           !state.skinning &&
           !state.morphing &&
           !state.arbitrary_gx_tev;
}

bool diagnostic_state_supported(const StaticRenderState& state) {
    return state.primitive == Primitive::Triangles &&
           state.texture_count == 0 &&
           state.pass_count == 1 &&
           state.index_bits == 16 &&
           !state.texture_psp8888_linear &&
           state.opaque &&
           state.depth_test &&
           state.depth_write &&
           !state.white_texture_modulation &&
           !state.blending &&
           !state.complex_alpha_test &&
           !state.fog &&
           !state.lighting &&
           !state.normals &&
           !state.animation &&
           !state.skinning &&
           !state.morphing &&
           !state.arbitrary_gx_tev;
}

bool package_supported(const dpsm::PackageView& package) {
    return package.vertices != nullptr &&
           package.indices != nullptr &&
           package.texture != nullptr &&
           package.vertex_count > 0 &&
           package.index_count > 0 &&
           package.index_count % 3 == 0 &&
           package.triangle_count == package.index_count / 3 &&
           package.vertex_bytes >=
               package.vertex_count * sizeof(dpsm::Vertex) &&
           package.index_bytes >=
               package.index_count * sizeof(std::uint16_t) &&
           package.texture_width > 0 &&
           package.texture_height > 0 &&
           package.texture_stride >= package.texture_width &&
           package.texture_bytes >=
               package.texture_stride *
                   package.texture_height *
                   sizeof(std::uint32_t);
}

bool diagnostic_package_supported(const dpmd::PackageView& package) {
    if (package.chunk_count != 5 ||
        package.triangle_count != 4329) {
        return false;
    }
    for (std::uint32_t index = 0; index < package.chunk_count; ++index) {
        const dpmd::ChunkView& chunk = package.chunks[index];
        if (chunk.vertices == nullptr ||
            chunk.indices == nullptr ||
            chunk.vertex_count == 0 ||
            chunk.index_count == 0 ||
            chunk.index_count % 3 != 0 ||
            chunk.triangle_count != chunk.index_count / 3 ||
            chunk.vertex_bytes <
                chunk.vertex_count * sizeof(dpmd::Vertex) ||
            chunk.index_bytes <
                chunk.index_count * sizeof(std::uint16_t)) {
            return false;
        }
    }
    return true;
}

const char* error_name(Error error) {
    switch (error) {
    case Error::Ok:
        return "Ok";
    case Error::NullOutput:
        return "NullOutput";
    case Error::NullResource:
        return "NullResource";
    case Error::InvalidPackage:
        return "InvalidPackage";
    case Error::MatrixNotFinite:
        return "MatrixNotFinite";
    case Error::UnsupportedState:
        return "UnsupportedState";
    case Error::BackendFailure:
        return "BackendFailure";
    }
    return "Unknown";
}

Error adapt_static_instance(
    const ::dMdl_obj_c& source,
    const StaticResourceBinding& binding,
    PspStaticRenderCommand* output,
    Metrics* metrics) {
    if (metrics != nullptr) {
        ++metrics->source_object_count;
        ++metrics->adapter_calls;
        metrics->source_object_address =
            reinterpret_cast<std::uintptr_t>(&source);
        metrics->source_next_address =
            reinterpret_cast<std::uintptr_t>(source.mpObj);
    }
    if (output == nullptr) {
        reject(Error::NullOutput, metrics);
        return Error::NullOutput;
    }
    if (binding.package == nullptr) {
        reject(Error::NullResource, metrics);
        return Error::NullResource;
    }
    if (!package_supported(*binding.package)) {
        reject(Error::InvalidPackage, metrics);
        return Error::InvalidPackage;
    }
    if (!source_matrix_finite(source.mMtx)) {
        reject(Error::MatrixNotFinite, metrics);
        return Error::MatrixNotFinite;
    }
    if (!state_supported(binding.state) ||
        !binding.resource_is_external ||
        binding.external_package_path == nullptr) {
        reject(Error::UnsupportedState, metrics);
        return Error::UnsupportedState;
    }

    const ModelMatrix model =
        adapt_matrix(source.mMtx);
    if (!matrix_mapping_valid(source.mMtx, model)) {
        reject(Error::BackendFailure, metrics);
        return Error::BackendFailure;
    }

    *output = {
        &source,
        binding.package,
        binding.external_package_path,
        model,
        binding.state,
    };
    if (metrics != nullptr) {
        ++metrics->commands_emitted;
        metrics->source_matrix_hash =
            hash_bytes(source.mMtx, sizeof(source.mMtx));
        metrics->adapted_matrix_hash =
            hash_bytes(&model, sizeof(model));
        for (std::size_t row = 0; row < 3; ++row) {
            for (std::size_t column = 0; column < 4; ++column) {
                metrics->source_matrix_values[row * 4 + column] =
                    source.mMtx[row][column];
            }
        }
        for (std::size_t column = 0; column < 4; ++column) {
            for (std::size_t row = 0; row < 4; ++row) {
                metrics->adapted_matrix_values[column * 4 + row] =
                    model.column[column][row];
            }
        }
        metrics->matrix_conversion_valid = true;
        metrics->resource_binding_external = true;
    }
    return Error::Ok;
}

Error adapt_diagnostic_instance(
    const ::dMdl_obj_c& source,
    const DiagnosticResourceBinding& binding,
    PspDiagnosticRenderCommand* output,
    Metrics* metrics) {
    if (metrics != nullptr) {
        ++metrics->source_object_count;
        ++metrics->adapter_calls;
        metrics->source_object_address =
            reinterpret_cast<std::uintptr_t>(&source);
        metrics->source_next_address =
            reinterpret_cast<std::uintptr_t>(source.mpObj);
    }
    if (output == nullptr) {
        reject(Error::NullOutput, metrics);
        return Error::NullOutput;
    }
    if (binding.package == nullptr) {
        reject(Error::NullResource, metrics);
        return Error::NullResource;
    }
    if (!diagnostic_package_supported(*binding.package)) {
        reject(Error::InvalidPackage, metrics);
        return Error::InvalidPackage;
    }
    if (!source_matrix_finite(source.mMtx)) {
        reject(Error::MatrixNotFinite, metrics);
        return Error::MatrixNotFinite;
    }
    if (!diagnostic_state_supported(binding.state) ||
        !binding.resource_is_external ||
        binding.external_package_path == nullptr) {
        reject(Error::UnsupportedState, metrics);
        return Error::UnsupportedState;
    }
    const ModelMatrix model = adapt_matrix(source.mMtx);
    if (!matrix_mapping_valid(source.mMtx, model)) {
        reject(Error::BackendFailure, metrics);
        return Error::BackendFailure;
    }
    *output = {
        &source,
        binding.package,
        binding.external_package_path,
        model,
        binding.state,
    };
    if (metrics != nullptr) {
        ++metrics->commands_emitted;
        metrics->source_matrix_hash =
            hash_bytes(source.mMtx, sizeof(source.mMtx));
        metrics->adapted_matrix_hash =
            hash_bytes(&model, sizeof(model));
        metrics->matrix_conversion_valid = true;
        metrics->resource_binding_external = true;
    }
    return Error::Ok;
}

}  // namespace dusk::psp::render_bridge
