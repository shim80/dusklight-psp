#include "playable_export.hpp"

#include <JSystem/J3DGraphAnimator/J3DAnimation.h>
#include <JSystem/J3DGraphAnimator/J3DJoint.h>
#include <JSystem/J3DGraphAnimator/J3DModel.h>
#include <JSystem/J3DGraphAnimator/J3DModelData.h>
#include <JSystem/J3DGraphBase/J3DMaterial.h>
#include <JSystem/J3DGraphBase/J3DShape.h>
#include <JSystem/J3DGraphBase/J3DShapeDraw.h>
#include <JSystem/J3DGraphBase/J3DShapeMtx.h>
#include <JSystem/J3DGraphBase/J3DTexture.h>
#include <JSystem/J3DGraphLoader/J3DAnmLoader.h>
#include <JSystem/JKernel/JKRDecomp.h>
#include <JSystem/JKernel/JKRMemArchive.h>
#include <JSystem/JUtility/JUTNameTab.h>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <limits>
#include <memory>
#include <sstream>
#include <stdexcept>
#include <string>
#include <unordered_map>
#include <vector>

namespace {

using Byte = std::uint8_t;

constexpr std::size_t kDpskHeaderSize = 192;
constexpr std::size_t kDpskJointSize = 128;
constexpr std::size_t kDpskVertexSize = 64;
constexpr std::size_t kDpskSubmeshSize = 32;
constexpr std::size_t kDpanHeaderSize = 128;
constexpr std::size_t kDpanClipSize = 48;
constexpr std::size_t kDpanSampleSize = 40;
constexpr std::size_t kDptxHeaderSize = 128;
constexpr std::size_t kDptxTextureSize = 48;
constexpr std::size_t kDptxMaterialSize = 32;
constexpr std::size_t kDpuiHeaderSize = 128;
constexpr std::size_t kDpuiQuadSize = 32;
constexpr std::uint32_t kJointCount = 35;
constexpr std::uint32_t kMaxWeights = 5;

std::size_t align16(std::size_t value) {
    return (value + 15u) & ~std::size_t(15u);
}

void put_u16(std::vector<Byte>& out, std::size_t at, std::uint16_t value) {
    out[at] = static_cast<Byte>(value);
    out[at + 1] = static_cast<Byte>(value >> 8);
}

void put_s16(std::vector<Byte>& out, std::size_t at, std::int16_t value) {
    put_u16(out, at, static_cast<std::uint16_t>(value));
}

void put_u32(std::vector<Byte>& out, std::size_t at, std::uint32_t value) {
    out[at] = static_cast<Byte>(value);
    out[at + 1] = static_cast<Byte>(value >> 8);
    out[at + 2] = static_cast<Byte>(value >> 16);
    out[at + 3] = static_cast<Byte>(value >> 24);
}

void put_f32(std::vector<Byte>& out, std::size_t at, float value) {
    std::uint32_t bits = 0;
    std::memcpy(&bits, &value, sizeof(bits));
    put_u32(out, at, bits);
}

std::uint32_t crc32(const std::vector<Byte>& bytes) {
    std::uint32_t crc = 0xffffffffu;
    for (Byte byte : bytes) {
        crc ^= byte;
        for (int bit = 0; bit < 8; ++bit) {
            crc = (crc >> 1) ^
                  (0xedb88320u & (0u - (crc & 1u)));
        }
    }
    return ~crc;
}

std::uint32_t fnv1a(const char* text) {
    std::uint32_t value = 2166136261u;
    for (; text != nullptr && *text != '\0'; ++text) {
        value = (value ^ static_cast<Byte>(*text)) * 16777619u;
    }
    return value;
}

const char* environment_or(const char* name, const char* fallback) {
    const char* value = std::getenv(name);
    return value != nullptr && value[0] != '\0' ? value : fallback;
}

void write_file(const char* path, const std::vector<Byte>& bytes) {
    std::ofstream stream(path, std::ios::binary | std::ios::trunc);
    if (!stream ||
        !stream.write(
            reinterpret_cast<const char*>(bytes.data()),
            static_cast<std::streamsize>(bytes.size()))) {
        throw std::runtime_error(std::string("cannot write ") + path);
    }
}

struct Attribute {
    GXAttr attr;
    GXAttrType type;
    std::uint8_t offset;
    std::uint8_t size;
};

std::vector<Attribute> layout_of(const GXVtxDescList* descriptions) {
    std::vector<Attribute> result;
    std::uint8_t offset = 0;
    for (const GXVtxDescList* item = descriptions;
         item->attr != GX_VA_NULL;
         ++item) {
        if (item->type == GX_NONE) {
            continue;
        }
        std::uint8_t size = 0;
        if (item->type == GX_INDEX8) {
            size = 1;
        } else if (item->type == GX_INDEX16) {
            size = 2;
        } else if (
            item->type == GX_DIRECT &&
            item->attr >= GX_VA_PNMTXIDX &&
            item->attr <= GX_VA_TEX7MTXIDX) {
            size = 1;
        } else {
            throw std::runtime_error("unsupported GX corner attribute");
        }
        result.push_back({item->attr, item->type, offset, size});
        offset += size;
    }
    return result;
}

std::uint16_t corner_index(
    const Byte* corner,
    const std::vector<Attribute>& layout,
    GXAttr wanted,
    bool* present = nullptr) {
    for (const Attribute& item : layout) {
        if (item.attr != wanted) {
            continue;
        }
        if (present != nullptr) {
            *present = true;
        }
        return item.type == GX_INDEX16
            ? static_cast<std::uint16_t>(
                  (corner[item.offset] << 8) | corner[item.offset + 1])
            : corner[item.offset];
    }
    if (present != nullptr) {
        *present = false;
    }
    return 0;
}

Vec position_of(J3DVertexData& data, std::uint16_t index) {
    if (index >= data.getVtxNum()) {
        throw std::runtime_error("invalid DPSK position");
    }
    if (data.getVtxPosType() == GX_F32) {
        return static_cast<Vec*>(data.getVtxPosArray())[index];
    }
    if (data.getVtxPosType() == GX_S16) {
        const auto* values =
            static_cast<const std::int16_t*>(data.getVtxPosArray()) + index * 3;
        const float scale =
            1.0f / static_cast<float>(1u << data.getVtxPosFrac());
        return {values[0] * scale, values[1] * scale, values[2] * scale};
    }
    throw std::runtime_error("unsupported DPSK position type");
}

Vec normal_of(J3DVertexData& data, std::uint16_t index) {
    if (index >= data.getNrmNum()) {
        throw std::runtime_error("invalid DPSK normal");
    }
    if (data.getVtxNrmType() == GX_F32) {
        return static_cast<Vec*>(data.getVtxNrmArray())[index];
    }
    if (data.getVtxNrmType() != GX_S16) {
        throw std::runtime_error("unsupported DPSK normal type");
    }
    const auto* values =
        static_cast<const std::int16_t*>(data.getVtxNrmArray()) + index * 3;
    const float scale =
        1.0f / static_cast<float>(1u << data.getVtxNrmFrac());
    return {values[0] * scale, values[1] * scale, values[2] * scale};
}

struct UvFormat {
    GXCompType type = GX_F32;
    std::uint8_t frac = 0;
};

UvFormat uv_format_of(J3DVertexData& data) {
    for (const GXVtxAttrFmtList* item = data.getVtxAttrFmtList();
         item != nullptr && item->attr != GX_VA_NULL;
         ++item) {
        if (item->attr == GX_VA_TEX0) {
            return {item->type, item->frac};
        }
    }
    throw std::runtime_error("TEX0 format absent");
}

std::array<float, 2> uv_of(
    J3DVertexData& data, std::uint16_t index, const UvFormat& format) {
    if (index >= data.getVtxArrNum(GX_VA_TEX0)) {
        throw std::runtime_error("DPSK UV outside VTX1");
    }
    if (format.type == GX_F32) {
        const float* values =
            static_cast<const float*>(data.getVtxTexCoordArray(0)) + index * 2;
        return {values[0], values[1]};
    }
    if (format.type == GX_S16) {
        const auto* values =
            static_cast<const std::int16_t*>(data.getVtxTexCoordArray(0)) +
            index * 2;
        const float scale =
            1.0f / static_cast<float>(1u << format.frac);
        return {values[0] * scale, values[1] * scale};
    }
    throw std::runtime_error("unsupported DPSK UV type");
}

Vec transform_point(MtxP matrix, const Vec& value) {
    return {
        matrix[0][0] * value.x + matrix[0][1] * value.y +
            matrix[0][2] * value.z + matrix[0][3],
        matrix[1][0] * value.x + matrix[1][1] * value.y +
            matrix[1][2] * value.z + matrix[1][3],
        matrix[2][0] * value.x + matrix[2][1] * value.y +
            matrix[2][2] * value.z + matrix[2][3],
    };
}

Vec transform_vector(MtxP matrix, const Vec& value) {
    Vec result = {
        matrix[0][0] * value.x + matrix[0][1] * value.y +
            matrix[0][2] * value.z,
        matrix[1][0] * value.x + matrix[1][1] * value.y +
            matrix[1][2] * value.z,
        matrix[2][0] * value.x + matrix[2][1] * value.y +
            matrix[2][2] * value.z,
    };
    const float length =
        std::sqrt(result.x * result.x + result.y * result.y +
                  result.z * result.z);
    if (!std::isfinite(length) || length < 1.0e-8f) {
        throw std::runtime_error("non-finite DPSK normal");
    }
    result.x /= length;
    result.y /= length;
    result.z /= length;
    return result;
}

struct InfluenceSet {
    std::array<std::uint8_t, 5> joints{};
    std::array<std::uint8_t, 5> weights{};
};

InfluenceSet quantize(
    const std::vector<std::pair<std::uint16_t, float>>& source) {
    if (source.empty() || source.size() > kMaxWeights) {
        throw std::runtime_error("DPSK influence count invalid");
    }
    struct Item {
        std::uint16_t joint;
        float source;
        int quantized;
        float remainder;
    };
    std::vector<Item> items;
    int total = 0;
    for (const auto& [joint, weight] : source) {
        if (joint >= kJointCount || !std::isfinite(weight) || weight < 0.0f) {
            throw std::runtime_error("DPSK source influence invalid");
        }
        const float scaled = weight * 255.0f;
        const int base = static_cast<int>(std::floor(scaled));
        items.push_back({joint, weight, base, scaled - base});
        total += base;
    }
    std::stable_sort(
        items.begin(), items.end(),
        [](const Item& a, const Item& b) {
            return a.remainder > b.remainder;
        });
    for (int index = 0; total < 255; ++index, ++total) {
        ++items[static_cast<std::size_t>(index) % items.size()].quantized;
    }
    std::stable_sort(
        items.begin(), items.end(),
        [](const Item& a, const Item& b) {
            return a.source > b.source;
        });
    InfluenceSet result;
    int check = 0;
    for (std::size_t index = 0; index < items.size(); ++index) {
        const float reconstructed =
            static_cast<float>(items[index].quantized) / 255.0f;
        if (std::fabs(reconstructed - items[index].source) >
            1.0f / 255.0f + 1.0e-6f) {
            throw std::runtime_error("DPSK weight quantization error");
        }
        result.joints[index] = static_cast<std::uint8_t>(items[index].joint);
        result.weights[index] =
            static_cast<std::uint8_t>(items[index].quantized);
        check += items[index].quantized;
    }
    if (check != 255) {
        throw std::runtime_error("DPSK quantized weights do not sum to 255");
    }
    return result;
}

std::vector<std::pair<std::uint16_t, float>> influences_for(
    J3DModelData& data, std::uint16_t draw_matrix) {
    if (draw_matrix >= data.getDrawMtxNum()) {
        throw std::runtime_error("DPSK draw matrix outside DRW1");
    }
    const std::uint16_t index = data.getDrawMtxIndex(draw_matrix);
    if (data.getDrawMtxFlag(draw_matrix) == 0) {
        return {{index, 1.0f}};
    }
    if (index >= data.getWEvlpMtxNum()) {
        throw std::runtime_error("DPSK envelope outside EVP1");
    }
    std::size_t cursor = 0;
    for (std::uint16_t envelope = 0; envelope < index; ++envelope) {
        cursor += data.getWEvlpMixMtxNum(envelope);
    }
    std::vector<std::pair<std::uint16_t, float>> result;
    for (std::uint8_t item = 0;
         item < data.getWEvlpMixMtxNum(index);
         ++item) {
        result.emplace_back(
            static_cast<std::uint16_t>(
                data.getWEvlpMixMtxIndex()[cursor + item]),
            static_cast<float>(data.getWEvlpMixWeight()[cursor + item]));
    }
    return result;
}

struct Vertex {
    Vec position;
    Vec normal;
    std::array<float, 2> uv;
    InfluenceSet influences;
    std::uint8_t part;
    std::uint8_t material;
};

struct VertexKey {
    std::uint16_t shape;
    std::uint16_t position;
    std::uint16_t normal;
    std::uint16_t uv;
    std::uint16_t matrix;
    bool operator==(const VertexKey&) const = default;
};

struct VertexKeyHash {
    std::size_t operator()(const VertexKey& key) const {
        std::size_t value = key.shape;
        value = value * 65537u + key.position;
        value = value * 65537u + key.normal;
        value = value * 65537u + key.uv;
        return value * 65537u + key.matrix;
    }
};

struct Submesh {
    std::uint32_t first_index;
    std::uint32_t index_count;
    std::uint16_t material;
    std::uint16_t texture;
    std::uint8_t part;
    std::uint16_t source_material;
    std::uint16_t source_shape;
};

struct MeshBuild {
    std::vector<Vertex> vertices;
    std::vector<std::uint16_t> indices;
    std::vector<Submesh> submeshes;
    Vec minimum = {INFINITY, INFINITY, INFINITY};
    Vec maximum = {-INFINITY, -INFINITY, -INFINITY};
};

struct Piece {
    J3DModel* model;
    std::vector<std::uint16_t> shapes;
    std::uint8_t part;
    int attachment_joint;
    std::uint16_t texture_base;
    bool bake_weighted = false;
};

void append_piece(MeshBuild& out, const Piece& piece) {
    J3DModelData& data = *piece.model->getModelData();
    J3DVertexData& vertex_data = data.getVertexData();
    const UvFormat uv_format = uv_format_of(vertex_data);
    for (std::uint16_t shape_index : piece.shapes) {
        J3DShape* shape = data.getShapeNodePointer(shape_index);
        const std::vector<Attribute> layout = layout_of(shape->getVtxDesc());
        std::uint32_t stride = 0;
        for (const Attribute& item : layout) {
            stride += item.size;
        }
        J3DMaterial* source_material = nullptr;
        std::uint16_t material = 0;
        for (std::uint16_t material_index = 0;
             material_index < data.getMaterialNum(); ++material_index) {
            J3DMaterial* candidate =
                data.getMaterialNodePointer(material_index);
            if (candidate != nullptr && candidate->getShape() != nullptr &&
                candidate->getShape()->getIndex() == shape_index) {
                source_material = candidate;
                material = material_index;
                break;
            }
        }
        std::uint16_t texture = 0;
        if (source_material != nullptr) {
            const std::uint16_t local = source_material->getTexNo(0);
            texture = piece.texture_base +
                (local < data.getTexture()->getNum() ? local : 0);
        }
        Submesh submesh = {
            static_cast<std::uint32_t>(out.indices.size()),
            0,
            static_cast<std::uint16_t>(out.submeshes.size()),
            texture,
            piece.part,
            material,
            shape_index,
        };
        std::unordered_map<VertexKey, std::uint16_t, VertexKeyHash> unique;
        std::array<std::uint16_t, 10> matrices;
        matrices.fill(0xffff);

        auto corner = [&](const Byte* bytes) -> std::uint16_t {
            bool has_position = false;
            bool has_normal = false;
            bool has_uv = false;
            bool has_matrix = false;
            const std::uint16_t position_index =
                corner_index(bytes, layout, GX_VA_POS, &has_position);
            const std::uint16_t normal_index =
                corner_index(bytes, layout, GX_VA_NRM, &has_normal);
            const std::uint16_t uv_index =
                corner_index(bytes, layout, GX_VA_TEX0, &has_uv);
            const std::uint16_t matrix_register =
                corner_index(bytes, layout, GX_VA_PNMTXIDX, &has_matrix);
            if (!has_position || !has_normal || !has_uv ||
                (has_matrix && matrix_register % 3 != 0)) {
                throw std::runtime_error("DPSK corner attributes invalid");
            }
            const std::uint16_t slot = has_matrix ? matrix_register / 3 : 0;
            if (slot >= matrices.size() || matrices[slot] == 0xffff) {
                throw std::runtime_error("DPSK unresolved matrix slot");
            }
            const std::uint16_t draw_matrix = matrices[slot];
            const VertexKey key = {
                shape_index, position_index, normal_index, uv_index, draw_matrix};
            const auto found = unique.find(key);
            if (found != unique.end()) {
                return found->second;
            }
            Vec position = position_of(vertex_data, position_index);
            Vec normal = normal_of(vertex_data, normal_index);
            std::vector<std::pair<std::uint16_t, float>> influences;
            if (piece.attachment_joint >= 0) {
                MtxP matrix = nullptr;
                const std::uint16_t source =
                    data.getDrawMtxIndex(draw_matrix);
                if (data.getDrawMtxFlag(draw_matrix) == 0) {
                    matrix = piece.model->getAnmMtx(source);
                } else {
                    matrix = piece.model->getWeightAnmMtx(source);
                }
                position = transform_point(matrix, position);
                normal = transform_vector(matrix, normal);
                influences = {{
                    static_cast<std::uint16_t>(piece.attachment_joint), 1.0f}};
            } else {
                influences = influences_for(data, draw_matrix);
                if (data.getDrawMtxFlag(draw_matrix) == 0) {
                    const std::uint16_t joint =
                        data.getDrawMtxIndex(draw_matrix);
                    position = transform_point(
                        piece.model->getAnmMtx(joint), position);
                    normal = transform_vector(
                        piece.model->getAnmMtx(joint), normal);
                } else if (piece.bake_weighted) {
                    const std::uint16_t envelope =
                        data.getDrawMtxIndex(draw_matrix);
                    position = transform_point(
                        piece.model->getWeightAnmMtx(envelope), position);
                    normal = transform_vector(
                        piece.model->getWeightAnmMtx(envelope), normal);
                }
            }
            if (!std::isfinite(position.x) || !std::isfinite(position.y) ||
                !std::isfinite(position.z)) {
                throw std::runtime_error("DPSK non-finite position");
            }
            out.minimum.x = std::min(out.minimum.x, position.x);
            out.minimum.y = std::min(out.minimum.y, position.y);
            out.minimum.z = std::min(out.minimum.z, position.z);
            out.maximum.x = std::max(out.maximum.x, position.x);
            out.maximum.y = std::max(out.maximum.y, position.y);
            out.maximum.z = std::max(out.maximum.z, position.z);
            if (out.vertices.size() >= 65535) {
                throw std::runtime_error("DPSK vertex limit");
            }
            const std::uint16_t result =
                static_cast<std::uint16_t>(out.vertices.size());
            out.vertices.push_back({
                position,
                normal,
                uv_of(vertex_data, uv_index, uv_format),
                quantize(influences),
                piece.part,
                static_cast<std::uint8_t>(submesh.material),
            });
            unique.emplace(key, result);
            return result;
        };

        auto triangle = [&](const Byte* vertices, std::uint16_t a,
                            std::uint16_t b, std::uint16_t c) {
            const std::uint16_t ia = corner(vertices + stride * a);
            const std::uint16_t ib = corner(vertices + stride * b);
            const std::uint16_t ic = corner(vertices + stride * c);
            if (ia == ib || ib == ic || ia == ic) {
                return;
            }
            out.indices.insert(out.indices.end(), {ia, ib, ic});
        };

        for (std::uint16_t group = 0;
             group < shape->getMtxGroupNum();
             ++group) {
            J3DShapeMtx* shape_mtx = shape->getShapeMtx(group);
            const std::uint16_t matrix_count = shape_mtx->getUseMtxNum();
            if (matrix_count > matrices.size()) {
                throw std::runtime_error("DPSK GX matrix slots overflow");
            }
            for (std::uint16_t slot = 0; slot < matrix_count; ++slot) {
                const std::uint16_t candidate =
                    shape_mtx->getUseMtxIndex(slot);
                if (candidate != 0xffff) {
                    matrices[slot] = candidate;
                } else if (matrices[slot] == 0xffff) {
                    throw std::runtime_error("DPSK matrix inheritance invalid");
                }
            }
            J3DShapeDraw* draw = shape->getShapeDraw(group);
            const Byte* bytes = draw->getDisplayList();
            const std::uint32_t size = draw->getDisplayListSize();
            for (std::uint32_t cursor = 0; cursor < size;) {
                const Byte command = bytes[cursor++];
                if (command == 0) {
                    continue;
                }
                const Byte primitive = command & 0xf8;
                if (cursor + 2 > size) {
                    throw std::runtime_error("DPSK GX header truncated");
                }
                const std::uint16_t count =
                    static_cast<std::uint16_t>(
                        (bytes[cursor] << 8) | bytes[cursor + 1]);
                cursor += 2;
                if (static_cast<std::uint64_t>(count) * stride >
                    size - cursor) {
                    throw std::runtime_error("DPSK GX payload truncated");
                }
                const Byte* vertices = bytes + cursor;
                if (primitive == GX_TRIANGLES) {
                    for (std::uint16_t v = 0; v + 2 < count; v += 3) {
                        triangle(vertices, v, v + 1, v + 2);
                    }
                } else if (primitive == GX_TRIANGLESTRIP) {
                    for (std::uint16_t v = 2; v < count; ++v) {
                        triangle(
                            vertices,
                            (v & 1) == 0 ? v - 2 : v - 1,
                            (v & 1) == 0 ? v - 1 : v - 2,
                            v);
                    }
                } else if (primitive == GX_TRIANGLEFAN) {
                    for (std::uint16_t v = 2; v < count; ++v) {
                        triangle(vertices, 0, v - 1, v);
                    }
                } else if (primitive == GX_QUADS) {
                    for (std::uint16_t v = 0; v + 3 < count; v += 4) {
                        triangle(vertices, v, v + 1, v + 2);
                        triangle(vertices, v + 2, v + 3, v);
                    }
                } else {
                    throw std::runtime_error("DPSK GX primitive unsupported");
                }
                cursor += static_cast<std::uint32_t>(count) * stride;
            }
        }
        submesh.index_count =
            static_cast<std::uint32_t>(out.indices.size()) -
            submesh.first_index;
        out.submeshes.push_back(submesh);
    }
}

void fill_parents(
    J3DJoint* joint,
    std::int16_t parent,
    std::array<std::int16_t, 35>& parents) {
    for (J3DJoint* current = joint;
         current != nullptr;
         current = current->getYounger()) {
        parents[current->getJntNo()] = parent;
        if (current->getChild() != nullptr) {
            fill_parents(
                current->getChild(),
                static_cast<std::int16_t>(current->getJntNo()),
                parents);
        }
    }
}

std::vector<Byte> serialize_dpsk(const std::array<J3DModelData*, 4>& models) {
    J3DModel body(models[0], J3DMdlFlag_None, 1);
    J3DModel head(models[1], J3DMdlFlag_None, 1);
    J3DModel hands(models[2], J3DMdlFlag_None, 1);
    J3DModel face(models[3], J3DMdlFlag_None, 1);
    Mtx identity;
    MTXIdentity(identity);
    body.setBaseTRMtx(identity);
    body.calc();
    head.setBaseTRMtx(body.getAnmMtx(4));
    head.calc();
    face.setBaseTRMtx(body.getAnmMtx(4));
    face.calc();
    hands.setBaseTRMtx(identity);
    hands.calc();
    hands.setAnmMtx(1, body.getAnmMtx(9));
    hands.setAnmMtx(2, body.getAnmMtx(14));

    auto all_shapes = [](J3DModelData* model) {
        std::vector<std::uint16_t> result;
        for (std::uint16_t index = 0; index < model->getShapeNum(); ++index) {
            result.push_back(index);
        }
        return result;
    };
    MeshBuild mesh;
    append_piece(mesh, {&body, all_shapes(models[0]), 1, -1, 0});
    append_piece(mesh, {&head, all_shapes(models[1]), 2, 4, 12});
    append_piece(mesh, {&face, all_shapes(models[3]), 3, 4, 15});
    append_piece(mesh, {&hands, {4}, 4, 9, 14});
    append_piece(mesh, {&hands, {10}, 5, 14, 14});
    if (mesh.indices.size() != 4329u * 3u || mesh.submeshes.size() > 40) {
        throw std::runtime_error("DPSK visible topology mismatch");
    }

    const std::size_t joint_offset = kDpskHeaderSize;
    const std::size_t vertex_offset =
        align16(joint_offset + kDpskJointSize * kJointCount);
    const std::size_t index_offset =
        align16(vertex_offset + kDpskVertexSize * mesh.vertices.size());
    const std::size_t submesh_offset =
        align16(index_offset + sizeof(std::uint16_t) * mesh.indices.size());
    const std::size_t total =
        align16(submesh_offset + kDpskSubmeshSize * mesh.submeshes.size());
    if (total > std::numeric_limits<std::uint32_t>::max()) {
        throw std::runtime_error("DPSK size overflow");
    }
    std::vector<Byte> out(total, 0);
    std::memcpy(out.data(), "DPSK", 4);
    put_u16(out, 4, 1);
    put_u16(out, 6, kDpskHeaderSize);
    put_u32(out, 8, static_cast<std::uint32_t>(out.size()));
    put_u32(out, 16, kJointCount);
    put_u32(out, 20, static_cast<std::uint32_t>(mesh.vertices.size()));
    put_u32(out, 24, static_cast<std::uint32_t>(mesh.indices.size()));
    put_u32(out, 28, static_cast<std::uint32_t>(mesh.submeshes.size()));
    put_u32(out, 32, 4329);
    put_u32(out, 36, kMaxWeights);
    put_f32(out, 40, mesh.minimum.x);
    put_f32(out, 44, mesh.minimum.y);
    put_f32(out, 48, mesh.minimum.z);
    put_f32(out, 52, mesh.maximum.x);
    put_f32(out, 56, mesh.maximum.y);
    put_f32(out, 60, mesh.maximum.z);
    put_u32(out, 64, static_cast<std::uint32_t>(joint_offset));
    put_u32(out, 68, kDpskJointSize);
    put_u32(out, 72, static_cast<std::uint32_t>(vertex_offset));
    put_u32(out, 76, kDpskVertexSize);
    put_u32(out, 80, static_cast<std::uint32_t>(index_offset));
    put_u32(out, 84, static_cast<std::uint32_t>(submesh_offset));
    put_u32(out, 88, kDpskSubmeshSize);
    put_u32(out, 92, 5);
    put_u32(out, 96, 4);
    put_u32(out, 100, 4);
    put_u32(out, 104, 9);
    put_u32(out, 108, 14);

    std::array<std::int16_t, 35> parents;
    parents.fill(-1);
    fill_parents(models[0]->getJointTree().getRootNode(), -1, parents);
    JUTNameTab* joint_names = models[0]->getJointName();
    for (std::uint32_t joint = 0; joint < kJointCount; ++joint) {
        const std::size_t at = joint_offset + joint * kDpskJointSize;
        put_s16(out, at, parents[joint]);
        put_u32(
            out, at + 4,
            fnv1a(joint_names != nullptr ? joint_names->getName(joint) : ""));
        const J3DTransformInfo& bind =
            models[0]->getJointNodePointer(joint)->getTransformInfo();
        put_f32(out, at + 8, bind.mTranslate.x);
        put_f32(out, at + 12, bind.mTranslate.y);
        put_f32(out, at + 16, bind.mTranslate.z);
        put_s16(out, at + 20, bind.mRotation.x);
        put_s16(out, at + 22, bind.mRotation.y);
        put_s16(out, at + 24, bind.mRotation.z);
        put_f32(out, at + 28, bind.mScale.x);
        put_f32(out, at + 32, bind.mScale.y);
        put_f32(out, at + 36, bind.mScale.z);
        Mtx inverse;
        models[0]->getInvJointMtx(joint).to_host(inverse);
        for (std::uint32_t row = 0; row < 3; ++row) {
            for (std::uint32_t column = 0; column < 4; ++column) {
                put_f32(
                    out, at + 48 + (row * 4 + column) * 4,
                    inverse[row][column]);
            }
        }
    }
    for (std::size_t index = 0; index < mesh.vertices.size(); ++index) {
        const Vertex& vertex = mesh.vertices[index];
        const std::size_t at = vertex_offset + index * kDpskVertexSize;
        put_f32(out, at, vertex.position.x);
        put_f32(out, at + 4, vertex.position.y);
        put_f32(out, at + 8, vertex.position.z);
        put_f32(out, at + 12, vertex.normal.x);
        put_f32(out, at + 16, vertex.normal.y);
        put_f32(out, at + 20, vertex.normal.z);
        put_f32(out, at + 24, vertex.uv[0]);
        put_f32(out, at + 28, vertex.uv[1]);
        std::copy(
            vertex.influences.joints.begin(),
            vertex.influences.joints.end(),
            out.begin() + static_cast<std::ptrdiff_t>(at + 32));
        std::copy(
            vertex.influences.weights.begin(),
            vertex.influences.weights.end(),
            out.begin() + static_cast<std::ptrdiff_t>(at + 37));
        out[at + 42] = vertex.part;
        out[at + 43] = vertex.material;
    }
    for (std::size_t index = 0; index < mesh.indices.size(); ++index) {
        put_u16(out, index_offset + index * 2, mesh.indices[index]);
    }
    for (std::size_t index = 0; index < mesh.submeshes.size(); ++index) {
        const Submesh& submesh = mesh.submeshes[index];
        const std::size_t at = submesh_offset + index * kDpskSubmeshSize;
        put_u32(out, at, submesh.first_index);
        put_u32(out, at + 4, submesh.index_count);
        put_u16(out, at + 8, submesh.material);
        put_u16(out, at + 10, submesh.texture);
        out[at + 12] = submesh.part;
        put_u32(out, at + 16, 1);
        put_u16(out, at + 20, submesh.source_shape);
        put_u16(out, at + 22, submesh.source_material);
    }
    put_u32(out, 12, 0);
    put_u32(out, 12, crc32(out));
    return out;
}

struct Rgba {
    Byte r;
    Byte g;
    Byte b;
    Byte a;
};

std::uint16_t read_be16(const Byte* source) {
    return static_cast<std::uint16_t>(
        (source[0] << 8) | source[1]);
}

std::uint32_t read_be32(const Byte* source) {
    return
        (static_cast<std::uint32_t>(source[0]) << 24) |
        (static_cast<std::uint32_t>(source[1]) << 16) |
        (static_cast<std::uint32_t>(source[2]) << 8) |
        static_cast<std::uint32_t>(source[3]);
}

Rgba rgb565(std::uint16_t value) {
    return {
        static_cast<Byte>(((value >> 11) & 31) * 255 / 31),
        static_cast<Byte>(((value >> 5) & 63) * 255 / 63),
        static_cast<Byte>((value & 31) * 255 / 31),
        255,
    };
}

void set_pixel(
    std::vector<Rgba>& pixels,
    std::uint32_t width,
    std::uint32_t height,
    std::uint32_t x,
    std::uint32_t y,
    Rgba color) {
    if (x < width && y < height) {
        pixels[y * width + x] = color;
    }
}

Rgba decode_palette_color(std::uint16_t value, Byte format) {
    if (format == GX_TL_IA8) {
        return {
            static_cast<Byte>(value >> 8),
            static_cast<Byte>(value >> 8),
            static_cast<Byte>(value >> 8),
            static_cast<Byte>(value)};
    }
    if (format == GX_TL_RGB565) {
        return rgb565(value);
    }
    Rgba color = {};
    if ((value & 0x8000) != 0) {
        color = {
            static_cast<Byte>(((value >> 10) & 31) * 255 / 31),
            static_cast<Byte>(((value >> 5) & 31) * 255 / 31),
            static_cast<Byte>((value & 31) * 255 / 31),
            255};
    } else {
        color = {
            static_cast<Byte>(((value >> 8) & 15) * 17),
            static_cast<Byte>(((value >> 4) & 15) * 17),
            static_cast<Byte>((value & 15) * 17),
            static_cast<Byte>(((value >> 12) & 7) * 255 / 7)};
    }
    return color;
}

std::vector<Rgba> decode_timg(
    const ResTIMG& info,
    const Byte* source,
    const Byte* palette_source = nullptr) {
    const std::uint32_t width = info.width;
    const std::uint32_t height = info.height;
    std::vector<Rgba> pixels(width * height, {0, 0, 0, 0});
    std::size_t cursor = 0;
    if (info.format == GX_TF_CMPR) {
        for (std::uint32_t by = 0; by < height; by += 8) {
            for (std::uint32_t bx = 0; bx < width; bx += 8) {
                for (std::uint32_t sub = 0; sub < 4; ++sub) {
                    const Byte* block = source + cursor;
                    cursor += 8;
                    const std::uint16_t c0 = read_be16(block);
                    const std::uint16_t c1 = read_be16(block + 2);
                    std::array<Rgba, 4> palette = {
                        rgb565(c0), rgb565(c1), Rgba{}, Rgba{}};
                    if (c0 > c1) {
                        palette[2] = {
                            static_cast<Byte>((2 * palette[0].r + palette[1].r) / 3),
                            static_cast<Byte>((2 * palette[0].g + palette[1].g) / 3),
                            static_cast<Byte>((2 * palette[0].b + palette[1].b) / 3),
                            255};
                        palette[3] = {
                            static_cast<Byte>((palette[0].r + 2 * palette[1].r) / 3),
                            static_cast<Byte>((palette[0].g + 2 * palette[1].g) / 3),
                            static_cast<Byte>((palette[0].b + 2 * palette[1].b) / 3),
                            255};
                    } else {
                        palette[2] = {
                            static_cast<Byte>((palette[0].r + palette[1].r) / 2),
                            static_cast<Byte>((palette[0].g + palette[1].g) / 2),
                            static_cast<Byte>((palette[0].b + palette[1].b) / 2),
                            255};
                        palette[3] = {0, 0, 0, 0};
                    }
                    const std::uint32_t ox = bx + (sub & 1u) * 4u;
                    const std::uint32_t oy = by + (sub >> 1u) * 4u;
                    for (std::uint32_t y = 0; y < 4; ++y) {
                        for (std::uint32_t x = 0; x < 4; ++x) {
                            const std::uint32_t selector =
                                (block[4 + y] >> (6 - x * 2)) & 3;
                            set_pixel(
                                pixels, width, height, ox + x, oy + y,
                                palette[selector]);
                        }
                    }
                }
            }
        }
    } else if (info.format == GX_TF_RGB5A3 ||
               info.format == GX_TF_RGB565) {
        for (std::uint32_t by = 0; by < height; by += 4) {
            for (std::uint32_t bx = 0; bx < width; bx += 4) {
                for (std::uint32_t y = 0; y < 4; ++y) {
                    for (std::uint32_t x = 0; x < 4; ++x) {
                        const std::uint16_t value = read_be16(source + cursor);
                        cursor += 2;
                        const Rgba color =
                            info.format == GX_TF_RGB565
                                ? rgb565(value)
                                : decode_palette_color(
                                      value, GX_TL_RGB5A3);
                        set_pixel(
                            pixels, width, height, bx + x, by + y, color);
                    }
                }
            }
        }
    } else if (info.format == GX_TF_I4) {
        for (std::uint32_t by = 0; by < height; by += 8) {
            for (std::uint32_t bx = 0; bx < width; bx += 8) {
                for (std::uint32_t y = 0; y < 8; ++y) {
                    for (std::uint32_t x = 0; x < 8; x += 2) {
                        const Byte value = source[cursor++];
                        const Byte high = static_cast<Byte>((value >> 4) * 17);
                        const Byte low = static_cast<Byte>((value & 15) * 17);
                        set_pixel(
                            pixels, width, height, bx + x, by + y,
                            {high, high, high, high});
                        set_pixel(
                            pixels, width, height, bx + x + 1, by + y,
                            {low, low, low, low});
                    }
                }
            }
        }
    } else if (info.format == GX_TF_I8) {
        for (std::uint32_t by = 0; by < height; by += 4) {
            for (std::uint32_t bx = 0; bx < width; bx += 8) {
                for (std::uint32_t y = 0; y < 4; ++y) {
                    for (std::uint32_t x = 0; x < 8; ++x) {
                        const Byte value = source[cursor++];
                        set_pixel(
                            pixels, width, height, bx + x, by + y,
                            {value, value, value, value});
                    }
                }
            }
        }
    } else if (info.format == GX_TF_IA4) {
        for (std::uint32_t by = 0; by < height; by += 4) {
            for (std::uint32_t bx = 0; bx < width; bx += 8) {
                for (std::uint32_t y = 0; y < 4; ++y) {
                    for (std::uint32_t x = 0; x < 8; ++x) {
                        const Byte value = source[cursor++];
                        const Byte intensity =
                            static_cast<Byte>((value >> 4) * 17);
                        const Byte alpha =
                            static_cast<Byte>((value & 15) * 17);
                        set_pixel(
                            pixels, width, height, bx + x, by + y,
                            {intensity, intensity, intensity, alpha});
                    }
                }
            }
        }
    } else if (info.format == GX_TF_IA8) {
        for (std::uint32_t by = 0; by < height; by += 4) {
            for (std::uint32_t bx = 0; bx < width; bx += 4) {
                for (std::uint32_t y = 0; y < 4; ++y) {
                    for (std::uint32_t x = 0; x < 4; ++x) {
                        const Byte alpha = source[cursor++];
                        const Byte intensity = source[cursor++];
                        set_pixel(
                            pixels, width, height, bx + x, by + y,
                            {intensity, intensity, intensity, alpha});
                    }
                }
            }
        }
    } else if (info.format == GX_TF_RGBA8) {
        for (std::uint32_t by = 0; by < height; by += 4) {
            for (std::uint32_t bx = 0; bx < width; bx += 4) {
                const Byte* ar = source + cursor;
                const Byte* gb = source + cursor + 32;
                cursor += 64;
                for (std::uint32_t y = 0; y < 4; ++y) {
                    for (std::uint32_t x = 0; x < 4; ++x) {
                        const std::size_t at = (y * 4 + x) * 2;
                        set_pixel(
                            pixels, width, height, bx + x, by + y,
                            {ar[at + 1], gb[at], gb[at + 1], ar[at]});
                    }
                }
            }
        }
    } else if (info.format == GX_TF_C4 ||
               info.format == GX_TF_C8) {
        if (palette_source == nullptr || info.numColors == 0) {
            throw std::runtime_error(
                "indexed BTI has no source palette");
        }
        const std::uint32_t block_width =
            info.format == GX_TF_C4 ? 8u : 8u;
        const std::uint32_t block_height =
            info.format == GX_TF_C4 ? 8u : 4u;
        for (std::uint32_t by = 0; by < height; by += block_height) {
            for (std::uint32_t bx = 0; bx < width; bx += block_width) {
                for (std::uint32_t y = 0; y < block_height; ++y) {
                    for (std::uint32_t x = 0; x < block_width;) {
                        if (info.format == GX_TF_C4) {
                            const Byte packed = source[cursor++];
                            for (int nibble = 0; nibble < 2; ++nibble) {
                                const Byte index = nibble == 0
                                    ? packed >> 4 : packed & 15;
                                set_pixel(
                                    pixels, width, height,
                                    bx + x++, by + y,
                                    decode_palette_color(
                                        read_be16(
                                            palette_source + index * 2),
                                        info.colorFormat));
                            }
                        } else {
                            const Byte index = source[cursor++];
                            set_pixel(
                                pixels, width, height, bx + x++, by + y,
                                decode_palette_color(
                                    read_be16(
                                        palette_source + index * 2),
                                    info.colorFormat));
                        }
                    }
                }
            }
        }
    } else {
        throw std::runtime_error(
            "DPTX unsupported GX texture format=" +
            std::to_string(static_cast<unsigned>(info.format)));
    }
    return pixels;
}

std::vector<Rgba> decode_bti(
    const std::vector<Byte>& bytes) {
    if (bytes.size() < sizeof(ResTIMG)) {
        throw std::runtime_error("BTI resource truncated");
    }
    const auto& info =
        *reinterpret_cast<const ResTIMG*>(bytes.data());
    const std::int32_t image_offset = info.imageOffset;
    const std::int32_t palette_offset = info.paletteOffset;
    if (image_offset < static_cast<std::int32_t>(sizeof(ResTIMG)) ||
        static_cast<std::size_t>(image_offset) >= bytes.size()) {
        throw std::runtime_error("BTI image offset invalid");
    }
    const Byte* palette = nullptr;
    if (info.indexTexture != 0) {
        if (palette_offset < static_cast<std::int32_t>(sizeof(ResTIMG)) ||
            static_cast<std::size_t>(palette_offset) +
                    static_cast<std::size_t>(info.numColors) * 2 >
                bytes.size()) {
            throw std::runtime_error("BTI palette offset invalid");
        }
        palette = bytes.data() + palette_offset;
    }
    return decode_timg(
        info, bytes.data() + image_offset, palette);
}

std::vector<Byte> rgba4444_swizzled(
    const std::vector<Rgba>& pixels,
    std::uint32_t width,
    std::uint32_t height,
    std::uint32_t* stored_width,
    std::uint32_t* stored_height) {
    *stored_width = (width + 7u) & ~7u;
    *stored_height = (height + 7u) & ~7u;
    const std::uint32_t row_bytes = *stored_width * 2;
    std::vector<Byte> linear(row_bytes * *stored_height, 0);
    for (std::uint32_t y = 0; y < height; ++y) {
        for (std::uint32_t x = 0; x < width; ++x) {
            const Rgba color = pixels[y * width + x];
            const std::uint16_t value =
                static_cast<std::uint16_t>(
                    (color.r >> 4) |
                    ((color.g >> 4) << 4) |
                    ((color.b >> 4) << 8) |
                    ((color.a >> 4) << 12));
            const std::size_t at = y * row_bytes + x * 2;
            linear[at] = static_cast<Byte>(value);
            linear[at + 1] = static_cast<Byte>(value >> 8);
        }
    }
    std::vector<Byte> swizzled(linear.size(), 0);
    std::size_t output = 0;
    for (std::uint32_t by = 0; by < *stored_height; by += 8) {
        for (std::uint32_t bx = 0; bx < row_bytes; bx += 16) {
            for (std::uint32_t y = 0; y < 8; ++y) {
                std::copy_n(
                    linear.begin() + static_cast<std::ptrdiff_t>(
                        (by + y) * row_bytes + bx),
                    16,
                    swizzled.begin() + static_cast<std::ptrdiff_t>(output));
                output += 16;
            }
        }
    }
    return swizzled;
}

std::vector<Byte> rgba16_swizzled(
    const std::vector<Rgba>& pixels,
    std::uint32_t width,
    std::uint32_t height,
    Byte format,
    std::uint32_t* stored_width,
    std::uint32_t* stored_height) {
    *stored_width = (width + 7u) & ~7u;
    *stored_height = (height + 7u) & ~7u;
    const std::uint32_t row_bytes = *stored_width * 2;
    std::vector<Byte> linear(row_bytes * *stored_height, 0);
    for (std::uint32_t y = 0; y < height; ++y) {
        for (std::uint32_t x = 0; x < width; ++x) {
            const Rgba color = pixels[y * width + x];
            std::uint16_t value = 0;
            if (format == 0) {
                value = static_cast<std::uint16_t>(
                    (color.r >> 3) |
                    ((color.g >> 2) << 5) |
                    ((color.b >> 3) << 11));
            } else if (format == 1) {
                value = static_cast<std::uint16_t>(
                    (color.r >> 3) |
                    ((color.g >> 3) << 5) |
                    ((color.b >> 3) << 10) |
                    ((color.a >= 128 ? 1u : 0u) << 15));
            } else {
                value = static_cast<std::uint16_t>(
                    (color.r >> 4) |
                    ((color.g >> 4) << 4) |
                    ((color.b >> 4) << 8) |
                    ((color.a >> 4) << 12));
            }
            const std::size_t at = y * row_bytes + x * 2;
            linear[at] = static_cast<Byte>(value);
            linear[at + 1] = static_cast<Byte>(value >> 8);
        }
    }
    std::vector<Byte> swizzled(linear.size(), 0);
    std::size_t output = 0;
    for (std::uint32_t by = 0; by < *stored_height; by += 8) {
        for (std::uint32_t bx = 0; bx < row_bytes; bx += 16) {
            for (std::uint32_t y = 0; y < 8; ++y) {
                std::copy_n(
                    linear.begin() + static_cast<std::ptrdiff_t>(
                        (by + y) * row_bytes + bx),
                    16,
                    swizzled.begin() + static_cast<std::ptrdiff_t>(output));
                output += 16;
            }
        }
    }
    return swizzled;
}

struct ConvertedTexture {
    std::uint32_t name_hash;
    std::uint16_t width;
    std::uint16_t height;
    std::uint16_t stored_width;
    std::uint16_t stored_height;
    Byte psp_format;
    Byte source_format;
    Byte alpha;
    Byte mipmaps;
    std::vector<Byte> data;
};

std::uint8_t room_material_bucket(J3DMaterial* material);

std::uint32_t packed_argb(const GXColor& color) {
    return
        (static_cast<std::uint32_t>(color.a) << 24) |
        (static_cast<std::uint32_t>(color.r) << 16) |
        (static_cast<std::uint32_t>(color.g) << 8) |
        static_cast<std::uint32_t>(color.b);
}

struct LinkMaterialSource {
    J3DMaterial* material;
    std::uint16_t texture;
};

std::vector<LinkMaterialSource> link_material_sources(
    const std::array<J3DModelData*, 4>& models) {
    std::vector<LinkMaterialSource> result;
    auto append_all = [&](std::size_t model_index,
                          std::uint16_t texture_base) {
        J3DModelData* model = models[model_index];
        for (std::uint16_t shape_index = 0;
             shape_index < model->getShapeNum(); ++shape_index) {
            J3DMaterial* material =
                model->getShapeNodePointer(shape_index)->getMaterial();
            const std::uint16_t local =
                material == nullptr ? 0 : material->getTexNo(0);
            result.push_back({
                material,
                static_cast<std::uint16_t>(
                    texture_base +
                    (local < model->getTexture()->getNum() ? local : 0)),
            });
        }
    };
    append_all(0, 0);
    append_all(1, 12);
    append_all(3, 15);
    for (const std::uint16_t shape_index : {4u, 10u}) {
        J3DModelData* model = models[2];
        J3DMaterial* material =
            model->getShapeNodePointer(shape_index)->getMaterial();
        const std::uint16_t local =
            material == nullptr ? 0 : material->getTexNo(0);
        result.push_back({
            material,
            static_cast<std::uint16_t>(
                14 + (local < model->getTexture()->getNum() ? local : 0)),
        });
    }
    return result;
}

std::vector<Byte> serialize_dptx(
    const std::array<J3DModelData*, 4>& models) {
    std::vector<ConvertedTexture> textures;
    for (J3DModelData* model : models) {
        J3DTexture* source = model->getTexture();
        JUTNameTab* names = model->getTextureName();
        for (std::uint16_t index = 0; index < source->getNum(); ++index) {
            const ResTIMG& info = *source->getResTIMG(index);
            if (info.width == 0 || info.height == 0 ||
                info.width > 512 || info.height > 512) {
                throw std::runtime_error("DPTX texture dimensions invalid");
            }
            std::uint32_t stored_width = 0;
            std::uint32_t stored_height = 0;
            std::vector<Byte> converted = rgba4444_swizzled(
                decode_timg(info, source->getImgDataPtr(index)),
                info.width, info.height, &stored_width, &stored_height);
            textures.push_back({
                fnv1a(names != nullptr ? names->getName(index) : ""),
                info.width,
                info.height,
                static_cast<std::uint16_t>(stored_width),
                static_cast<std::uint16_t>(stored_height),
                2,
                info.format,
                info.alphaEnabled,
                info.mipmapCount,
                std::move(converted),
            });
        }
    }
    if (textures.size() != 29) {
        throw std::runtime_error("DPTX Link texture inventory mismatch");
    }
    const std::vector<LinkMaterialSource> materials =
        link_material_sources(models);
    constexpr std::uint32_t material_count = 27;
    if (materials.size() != material_count) {
        throw std::runtime_error("DPTX Link material inventory mismatch");
    }
    const std::size_t texture_table = kDptxHeaderSize;
    const std::size_t material_table =
        align16(texture_table + textures.size() * kDptxTextureSize);
    std::size_t cursor =
        align16(material_table + material_count * kDptxMaterialSize);
    std::vector<std::size_t> offsets;
    for (const ConvertedTexture& texture : textures) {
        offsets.push_back(cursor);
        cursor = align16(cursor + texture.data.size());
    }
    if (cursor > 1150000) {
        throw std::runtime_error("DPTX EDRAM texture budget exceeded");
    }
    std::vector<Byte> out(cursor, 0);
    std::memcpy(out.data(), "DPTX", 4);
    put_u16(out, 4, 1);
    put_u16(out, 6, kDptxHeaderSize);
    put_u32(out, 8, static_cast<std::uint32_t>(out.size()));
    put_u32(out, 16, static_cast<std::uint32_t>(textures.size()));
    put_u32(out, 20, material_count);
    put_u32(out, 24, static_cast<std::uint32_t>(texture_table));
    put_u32(out, 28, kDptxTextureSize);
    put_u32(out, 32, static_cast<std::uint32_t>(material_table));
    put_u32(out, 36, kDptxMaterialSize);
    put_u32(out, 40, 2);
    put_u32(out, 44, 1);
    put_u32(out, 48, static_cast<std::uint32_t>(cursor));
    for (std::size_t index = 0; index < textures.size(); ++index) {
        const ConvertedTexture& texture = textures[index];
        const std::size_t at = texture_table + index * kDptxTextureSize;
        put_u32(out, at, texture.name_hash);
        put_u16(out, at + 4, texture.width);
        put_u16(out, at + 6, texture.height);
        put_u16(out, at + 8, texture.stored_width);
        put_u16(out, at + 10, texture.stored_height);
        out[at + 12] = texture.psp_format;
        out[at + 13] = texture.source_format;
        out[at + 14] = texture.alpha;
        out[at + 15] = texture.mipmaps;
        put_u32(out, at + 16, static_cast<std::uint32_t>(offsets[index]));
        put_u32(out, at + 20, static_cast<std::uint32_t>(texture.data.size()));
        put_f32(
            out, at + 24,
            static_cast<float>(texture.width) / texture.stored_width);
        put_f32(
            out, at + 28,
            static_cast<float>(texture.height) / texture.stored_height);
        std::copy(
            texture.data.begin(), texture.data.end(),
            out.begin() + static_cast<std::ptrdiff_t>(offsets[index]));
    }
    for (std::size_t index = 0; index < materials.size(); ++index) {
        const LinkMaterialSource& source = materials[index];
        if (source.material == nullptr ||
            source.texture >= textures.size()) {
            throw std::runtime_error("DPTX Link material source missing");
        }
        J3DGXColor* material_color =
            source.material->getMatColor(0);
        J3DGXColor* ambient_color =
            source.material->getColorBlock()->getAmbColor(0);
        J3DColorChan* channel =
            source.material->getColorChan(0);
        const GXColor white = {255, 255, 255, 255};
        const GXColor black = {0, 0, 0, 255};
        const GXColor base =
            material_color == nullptr ? white : *material_color;
        const GXColor ambient =
            ambient_color == nullptr ? base : *ambient_color;
        const bool lighting =
            channel != nullptr && channel->getEnable() != 0;
        const GXColor emissive = lighting ? black : base;
        const std::size_t at = material_table + index * kDptxMaterialSize;
        put_u16(out, at, source.texture);
        out[at + 2] = textures[source.texture].alpha > 1 ? 2 : 0;
        out[at + 3] = lighting ? 1 : 0;
        put_u32(out, at + 4, packed_argb(base));
        put_u32(out, at + 8, packed_argb(ambient));
        put_u32(out, at + 12, packed_argb(base));
        put_u32(out, at + 16, packed_argb(emissive));
        put_u16(out, at + 20, source.material->getIndex());
        out[at + 22] = 0;
        out[at + 23] = base.a;
        out[at + 24] = source.material->getTevStageNum();
        out[at + 25] = source.material->getTexGenNum();
        out[at + 26] =
            material_color == nullptr || ambient_color == nullptr ? 1 : 0;
        out[at + 27] = channel == nullptr ? 0 :
            static_cast<Byte>(
                (channel->getDiffuseFn() & 3u) |
                ((channel->getAttnFn() & 3u) << 2));
    }
    put_u32(out, 12, 0);
    put_u32(out, 12, crc32(out));
    return out;
}

std::vector<Byte> serialize_room_dptx(J3DModelData* model) {
    if (model == nullptr || model->getTexture() == nullptr) {
        throw std::runtime_error("room DPTX model absent");
    }
    J3DTexture* source = model->getTexture();
    JUTNameTab* names = model->getTextureName();
    std::vector<ConvertedTexture> textures;
    const std::uint32_t maximum_dimension = static_cast<std::uint32_t>(
        std::strtoul(
            environment_or("DUSKLIGHT_ROOM_TEXTURE_MAX", "512"),
            nullptr, 10));
    const std::uint32_t maximum_source_dimension =
        static_cast<std::uint32_t>(std::strtoul(
            environment_or(
                "DUSKLIGHT_ROOM_SOURCE_TEXTURE_MAX", "512"),
            nullptr, 10));
    if (maximum_dimension < 32 || maximum_dimension > 512) {
        throw std::runtime_error("room DPTX texture limit invalid");
    }
    if (maximum_source_dimension < 512 ||
        maximum_source_dimension > 2048) {
        throw std::runtime_error(
            "room DPTX source texture limit invalid");
    }
    for (std::uint16_t index = 0; index < source->getNum(); ++index) {
        const ResTIMG& info = *source->getResTIMG(index);
        if (info.width == 0 || info.height == 0 ||
            info.width > maximum_source_dimension ||
            info.height > maximum_source_dimension) {
            throw std::runtime_error("room DPTX texture dimensions invalid");
        }
        std::uint32_t stored_width = 0;
        std::uint32_t stored_height = 0;
        const std::vector<Rgba> source_pixels =
            decode_timg(info, source->getImgDataPtr(index));
        const std::uint16_t width = static_cast<std::uint16_t>(
            std::min<std::uint32_t>(info.width, maximum_dimension));
        const std::uint16_t height = static_cast<std::uint16_t>(
            std::min<std::uint32_t>(info.height, maximum_dimension));
        std::vector<Rgba> pixels(width * height);
        for (std::uint32_t y = 0; y < height; ++y) {
            for (std::uint32_t x = 0; x < width; ++x) {
                const std::uint32_t source_x =
                    x * info.width / width;
                const std::uint32_t source_y =
                    y * info.height / height;
                pixels[y * width + x] =
                    source_pixels[source_y * info.width + source_x];
            }
        }
        bool has_transparent = false;
        bool has_partial = false;
        for (const Rgba& pixel : pixels) {
            has_transparent = has_transparent || pixel.a != 255;
            has_partial = has_partial || (pixel.a != 0 && pixel.a != 255);
        }
        const Byte psp_format =
            !has_transparent ? 0 : !has_partial ? 1 : 2;
        std::vector<Byte> converted = rgba16_swizzled(
            pixels, width, height, psp_format,
            &stored_width, &stored_height);
        textures.push_back({
            fnv1a(names != nullptr ? names->getName(index) : ""),
            width,
            height,
            static_cast<std::uint16_t>(stored_width),
            static_cast<std::uint16_t>(stored_height),
            psp_format,
            info.format,
            info.alphaEnabled,
            info.mipmapCount,
            std::move(converted),
        });
    }
    const std::uint32_t material_count = model->getMaterialNum();
    if (textures.empty() || material_count == 0 ||
        textures.size() > 256 || material_count > 256) {
        throw std::runtime_error("room DPTX inventory invalid");
    }
    const std::size_t texture_table = kDptxHeaderSize;
    const std::size_t material_table =
        align16(texture_table + textures.size() * kDptxTextureSize);
    std::size_t cursor =
        align16(material_table + material_count * kDptxMaterialSize);
    std::vector<std::size_t> offsets;
    for (const ConvertedTexture& texture : textures) {
        offsets.push_back(cursor);
        cursor = align16(cursor + texture.data.size());
    }
    const std::size_t texture_bytes =
        cursor - align16(material_table + material_count * kDptxMaterialSize);
    if (texture_bytes > 720000) {
        throw std::runtime_error("room DPTX EDRAM hard limit exceeded");
    }
    std::vector<Byte> out(cursor, 0);
    std::memcpy(out.data(), "DPTX", 4);
    put_u16(out, 4, 1);
    put_u16(out, 6, kDptxHeaderSize);
    put_u32(out, 8, static_cast<std::uint32_t>(out.size()));
    put_u32(out, 16, static_cast<std::uint32_t>(textures.size()));
    put_u32(out, 20, material_count);
    put_u32(out, 24, static_cast<std::uint32_t>(texture_table));
    put_u32(out, 28, kDptxTextureSize);
    put_u32(out, 32, static_cast<std::uint32_t>(material_table));
    put_u32(out, 36, kDptxMaterialSize);
    put_u32(out, 40, 2);
    put_u32(out, 44, 1);
    put_u32(out, 48, static_cast<std::uint32_t>(texture_bytes));
    put_u32(out, 52, 0);
    put_u32(out, 56, 0);
    put_u32(out, 60, static_cast<std::uint32_t>(textures.size()));
    for (std::size_t index = 0; index < textures.size(); ++index) {
        const ConvertedTexture& texture = textures[index];
        const std::size_t at = texture_table + index * kDptxTextureSize;
        put_u32(out, at, texture.name_hash);
        put_u16(out, at + 4, texture.width);
        put_u16(out, at + 6, texture.height);
        put_u16(out, at + 8, texture.stored_width);
        put_u16(out, at + 10, texture.stored_height);
        out[at + 12] = texture.psp_format;
        out[at + 13] = texture.source_format;
        out[at + 14] = texture.alpha;
        out[at + 15] = 1;
        put_u32(out, at + 16, static_cast<std::uint32_t>(offsets[index]));
        put_u32(out, at + 20, static_cast<std::uint32_t>(texture.data.size()));
        put_f32(
            out, at + 24,
            static_cast<float>(texture.width) / texture.stored_width);
        put_f32(
            out, at + 28,
            static_cast<float>(texture.height) / texture.stored_height);
        put_u32(out, at + 32, texture.mipmaps > 1 ? 1 : 0);
        std::copy(
            texture.data.begin(), texture.data.end(),
            out.begin() + static_cast<std::ptrdiff_t>(offsets[index]));
    }
    for (std::uint32_t index = 0; index < material_count; ++index) {
        J3DMaterial* material = model->getMaterialNodePointer(index);
        std::uint16_t texture = material->getTexNo(0);
        if (texture >= textures.size()) {
            texture = 0;
        }
        const std::size_t at = material_table + index * kDptxMaterialSize;
        put_u16(out, at, texture);
        out[at + 2] = room_material_bucket(material);
        out[at + 3] = 1;
        put_u32(out, at + 4, 0xffffffffu);
        put_u32(out, at + 8, 0xff606060u);
        put_u32(out, at + 12, material->getTevStageNum());
        put_u32(out, at + 16, material->getTexGenNum());
    }
    put_u32(out, 12, 0);
    put_u32(out, 12, crc32(out));
    return out;
}

struct UiSourceSprite {
    std::uint16_t id = 0;
    std::uint16_t channel = 0;
    std::string name;
    std::uint16_t width = 0;
    std::uint16_t height = 0;
    std::uint16_t advance = 0;
    std::int16_t screen_x = 0;
    std::int16_t screen_y = 0;
    std::vector<Rgba> pixels;
    std::uint16_t atlas_x = 0;
    std::uint16_t atlas_y = 0;
    std::uint32_t color = 0xffffffffu;
};

std::vector<Rgba> decode_i4_page(
    const Byte* source,
    std::uint32_t width,
    std::uint32_t height) {
    std::vector<Rgba> pixels(width * height, {0, 0, 0, 0});
    std::size_t cursor = 0;
    for (std::uint32_t by = 0; by < height; by += 8) {
        for (std::uint32_t bx = 0; bx < width; bx += 8) {
            for (std::uint32_t y = 0; y < 8; ++y) {
                for (std::uint32_t x = 0; x < 8; x += 2) {
                    const Byte packed = source[cursor++];
                    const Byte high =
                        static_cast<Byte>((packed >> 4) * 17);
                    const Byte low =
                        static_cast<Byte>((packed & 15) * 17);
                    set_pixel(
                        pixels, width, height, bx + x, by + y,
                        {255, 255, 255, high});
                    set_pixel(
                        pixels, width, height, bx + x + 1, by + y,
                        {255, 255, 255, low});
                }
            }
        }
    }
    return pixels;
}

UiSourceSprite font_glyph(
    const std::vector<Byte>& font,
    char character) {
    if (font.size() < 0x40 ||
        std::memcmp(font.data(), "FONTbfn1", 8) != 0) {
        throw std::runtime_error("BFN source font invalid");
    }
    const std::uint32_t blocks = read_be32(font.data() + 0x0c);
    const Byte* gly = nullptr;
    const Byte* map = nullptr;
    const Byte* wid = nullptr;
    std::size_t gly_offset = 0;
    std::size_t cursor = 0x20;
    for (std::uint32_t index = 0; index < blocks; ++index) {
        if (cursor > font.size() - 8) {
            throw std::runtime_error("BFN block table truncated");
        }
        const std::uint32_t size = read_be32(font.data() + cursor + 4);
        if (size < 8 || size > font.size() - cursor) {
            throw std::runtime_error("BFN block range invalid");
        }
        if (std::memcmp(font.data() + cursor, "GLY1", 4) == 0) {
            gly = font.data() + cursor;
            gly_offset = cursor;
        } else if (
            std::memcmp(font.data() + cursor, "MAP1", 4) == 0) {
            map = font.data() + cursor;
        } else if (
            std::memcmp(font.data() + cursor, "WID1", 4) == 0) {
            wid = font.data() + cursor;
        }
        cursor += size;
    }
    if (gly == nullptr || map == nullptr || wid == nullptr ||
        read_be16(map + 8) != 0 ||
        static_cast<unsigned char>(character) <
            read_be16(map + 10) ||
        static_cast<unsigned char>(character) >
            read_be16(map + 12)) {
        throw std::runtime_error("BFN ASCII mapping unavailable");
    }
    const std::uint16_t font_code = static_cast<std::uint16_t>(
        static_cast<unsigned char>(character) -
        read_be16(map + 10));
    const std::uint16_t gly_start = read_be16(gly + 8);
    const std::uint16_t gly_end = read_be16(gly + 10);
    const std::uint16_t cell_width = read_be16(gly + 12);
    const std::uint16_t cell_height = read_be16(gly + 14);
    const std::uint32_t texture_size = read_be32(gly + 16);
    const std::uint16_t texture_format = read_be16(gly + 20);
    const std::uint16_t rows = read_be16(gly + 22);
    const std::uint16_t columns = read_be16(gly + 24);
    const std::uint16_t texture_width = read_be16(gly + 26);
    const std::uint16_t texture_height = read_be16(gly + 28);
    if (font_code < gly_start || font_code > gly_end ||
        texture_format != GX_TF_I4 || rows == 0 || columns == 0 ||
        texture_width == 0 || texture_height == 0) {
        throw std::runtime_error("BFN glyph layout unsupported");
    }
    const std::uint32_t relative = font_code - gly_start;
    const std::uint32_t cells_per_page = rows * columns;
    const std::uint32_t page = relative / cells_per_page;
    const std::uint32_t cell = relative % cells_per_page;
    const std::uint32_t cell_row = cell / rows;
    const std::uint32_t cell_column = cell % rows;
    const std::size_t page_offset =
        gly_offset + 0x20 + page * texture_size;
    if (page_offset > font.size() ||
        texture_size > font.size() - page_offset) {
        throw std::runtime_error("BFN glyph page outside source");
    }
    const std::vector<Rgba> page_pixels = decode_i4_page(
        font.data() + page_offset, texture_width, texture_height);
    UiSourceSprite sprite;
    sprite.id = static_cast<std::uint16_t>(
        128 + static_cast<unsigned char>(character));
    sprite.channel = 8;
    sprite.name = std::string("rodan_b_24_22:") + character;
    sprite.width = cell_width;
    sprite.height = cell_height;
    sprite.advance = cell_width;
    sprite.pixels.resize(
        static_cast<std::size_t>(cell_width) * cell_height);
    for (std::uint32_t y = 0; y < cell_height; ++y) {
        for (std::uint32_t x = 0; x < cell_width; ++x) {
            sprite.pixels[y * cell_width + x] =
                page_pixels[
                    (cell_row * cell_height + y) * texture_width +
                    cell_column * cell_width + x];
        }
    }
    const std::uint16_t wid_start = read_be16(wid + 8);
    const std::uint16_t wid_end = read_be16(wid + 10);
    if (font_code >= wid_start && font_code <= wid_end) {
        const std::size_t at =
            12 + static_cast<std::size_t>(font_code - wid_start) * 2;
        if (at + 1 < read_be32(wid + 4)) {
            sprite.advance = wid[at + 1];
        }
    }
    std::uint16_t opaque_width = 0;
    for (std::uint16_t y = 0; y < sprite.height; ++y) {
        for (std::uint16_t x = 0; x < sprite.width; ++x) {
            if (sprite.pixels[y * sprite.width + x].a != 0) {
                opaque_width = std::max<std::uint16_t>(
                    opaque_width, static_cast<std::uint16_t>(x + 1));
            }
        }
    }
    const std::uint16_t cropped_width = std::min<std::uint16_t>(
        sprite.width,
        std::max<std::uint16_t>(1, std::max(sprite.advance, opaque_width)));
    if (cropped_width < sprite.width) {
        std::vector<Rgba> cropped(
            static_cast<std::size_t>(cropped_width) * sprite.height);
        for (std::uint16_t y = 0; y < sprite.height; ++y) {
            std::copy_n(
                sprite.pixels.begin() + y * sprite.width,
                cropped_width,
                cropped.begin() + y * cropped_width);
        }
        sprite.width = cropped_width;
        sprite.pixels = std::move(cropped);
    }
    return sprite;
}

std::vector<Byte> serialize_original_dpui(
    const std::vector<Byte>& layout,
    const std::vector<HudSourceResource>& resources,
    const std::vector<Byte>& font,
    std::string* inventory) {
    if (layout.size() < 0x40 ||
        std::memcmp(layout.data(), "SCRNblo2", 8) != 0 ||
        std::memcmp(layout.data() + 0x20, "INF1", 4) != 0) {
        throw std::runtime_error("HUD BLO2 source layout invalid");
    }
    const std::uint16_t source_width = read_be16(layout.data() + 0x28);
    const std::uint16_t source_height = read_be16(layout.data() + 0x2a);
    if (source_width != 604 || source_height != 448) {
        throw std::runtime_error("HUD BLO2 dimensions changed");
    }
    std::vector<UiSourceSprite> sprites;
    const std::array<const char*, 10> digit_names = {
        "im_font_number_32_32_ganshinkyo_0_02.bti",
        "im_font_number_32_32_ganshinkyo_1_02.bti",
        "im_font_number_32_32_ganshinkyo_2_02.bti",
        "im_font_number_32_32_ganshinkyo_3_02.bti",
        "im_font_number_32_32_ganshinkyo_4_03.bti",
        "im_font_number_32_32_ganshinkyo_5_02.bti",
        "im_font_number_32_32_ganshinkyo_6_02.bti",
        "im_font_number_32_32_ganshinkyo_7_02.bti",
        "im_font_number_32_32_ganshinkyo_8_02.bti",
        "im_font_number_32_32_ganshinkyo_9_02.bti",
    };
    auto identity = [&](const std::string& name) {
        if (name.rfind("tt_heart_", 0) == 0) {
            return static_cast<int>(name[10] - '0');
        }
        for (std::size_t digit = 0; digit < digit_names.size(); ++digit) {
            if (name == digit_names[digit]) {
                return static_cast<int>(10 + digit);
            }
        }
        if (name == "tt_rupy_green_icon2.bti") return 20;
        if (name == "tt_zelda_button_ab_maru.bti") return 30;
        if (name == "im_newwindow_try03_02_64x16_gre.bti") return 40;
        if (name == "tt_horiwaku_lu.bti") return 41;
        if (name == "tt_horiwaku_top_rr.bti") return 42;
        if (name == "tt_select_square_4i_00.bti") return 43;
        return -1;
    };
    std::ostringstream listing;
    listing << "layout=zelda_game_image.blo"
            << " source_width=" << source_width
            << " source_height=" << source_height << '\n';
    for (const HudSourceResource& resource : resources) {
        const int id = identity(resource.name);
        if (id < 0) {
            continue;
        }
        if (resource.bytes.size() < sizeof(ResTIMG)) {
            throw std::runtime_error(
                "HUD BTI resource truncated: " + resource.name);
        }
        const auto& info =
            *reinterpret_cast<const ResTIMG*>(resource.bytes.data());
        UiSourceSprite sprite;
        sprite.id = static_cast<std::uint16_t>(id);
        sprite.channel =
            id < 4 ? 1 : id < 20 ? 2 :
            id == 20 ? 2 : id == 30 ? 4 : 8;
        sprite.name = resource.name;
        sprite.width = info.width;
        sprite.height = info.height;
        sprite.advance = info.width;
        sprite.pixels = decode_bti(resource.bytes);
        if (id >= 0 && id < 4) {
            sprite.color = id == 3
                ? 0xffb0b0b0u : 0xff4050ffu;
        } else if (id == 30) {
            sprite.color = 0xff60d060u;
        } else if (id == 43) {
            sprite.color = 0xff70d8ffu;
        }
        if (id >= 40 &&
            (sprite.width > 64 || sprite.height > 64)) {
            const float scale = std::min(
                64.0f / sprite.width,
                64.0f / sprite.height);
            const std::uint16_t resized_width =
                static_cast<std::uint16_t>(
                    std::max(1.0f, std::floor(sprite.width * scale)));
            const std::uint16_t resized_height =
                static_cast<std::uint16_t>(
                    std::max(1.0f, std::floor(sprite.height * scale)));
            std::vector<Rgba> resized(
                static_cast<std::size_t>(resized_width) *
                resized_height);
            for (std::uint32_t py = 0; py < resized_height; ++py) {
                for (std::uint32_t px = 0; px < resized_width; ++px) {
                    resized[py * resized_width + px] =
                        sprite.pixels[
                            (py * sprite.height / resized_height) *
                                sprite.width +
                            px * sprite.width / resized_width];
                }
            }
            sprite.width = resized_width;
            sprite.height = resized_height;
            sprite.advance = resized_width;
            sprite.pixels = std::move(resized);
        }
        listing << "asset=" << resource.name
                << " id=" << id
                << " format=" << static_cast<unsigned>(info.format)
                << " palette_format="
                << static_cast<unsigned>(info.colorFormat)
                << " colors=" << static_cast<unsigned>(info.numColors)
                << " width=" << sprite.width
                << " height=" << sprite.height << '\n';
        sprites.push_back(std::move(sprite));
    }
    for (unsigned code = 0x20; code <= 0x7e; ++code) {
        const char character = static_cast<char>(code);
        UiSourceSprite glyph = font_glyph(font, character);
        listing << "glyph=" << static_cast<unsigned>(
                       static_cast<unsigned char>(character))
                << " source=rodan_b_24_22.bfn"
                << " width=" << glyph.width
                << " height=" << glyph.height
                << " advance=" << glyph.advance << '\n';
        sprites.push_back(std::move(glyph));
    }
    if (sprites.size() < 30) {
        throw std::runtime_error("HUD original source inventory incomplete");
    }
    std::sort(
        sprites.begin(), sprites.end(),
        [](const UiSourceSprite& left, const UiSourceSprite& right) {
            return left.height != right.height
                ? left.height > right.height
                : left.id < right.id;
        });
    constexpr std::uint16_t atlas_width = 512;
    constexpr std::uint16_t atlas_height = 192;
    std::vector<Rgba> atlas_pixels(
        atlas_width * atlas_height, {0, 0, 0, 0});
    std::uint16_t x = 1;
    std::uint16_t y = 0;
    std::uint16_t row_height = 0;
    for (UiSourceSprite& sprite : sprites) {
        if (x + sprite.width + 1 > atlas_width) {
            x = 1;
            y = static_cast<std::uint16_t>(y + row_height);
            row_height = 0;
        }
        if (y + sprite.height > atlas_height) {
            throw std::runtime_error(
                "HUD atlas overflow at " + sprite.name +
                " cursor=" + std::to_string(x) + "," +
                std::to_string(y) + " size=" +
                std::to_string(sprite.width) + "x" +
                std::to_string(sprite.height));
        }
        sprite.atlas_x = x;
        sprite.atlas_y = y;
        for (std::uint32_t py = 0; py < sprite.height; ++py) {
            for (std::uint32_t px = 0; px < sprite.width; ++px) {
                atlas_pixels[
                    (y + py) * atlas_width + x + px] =
                    sprite.pixels[py * sprite.width + px];
            }
        }
        x = static_cast<std::uint16_t>(x + sprite.width + 1);
        row_height = std::max(row_height, sprite.height);
    }
    std::uint32_t stored_width = 0;
    std::uint32_t stored_height = 0;
    const std::vector<Byte> atlas = rgba4444_swizzled(
        atlas_pixels, atlas_width, atlas_height,
        &stored_width, &stored_height);
    const std::size_t table = kDpuiHeaderSize;
    const std::size_t atlas_offset =
        align16(table + sprites.size() * kDpuiQuadSize);
    std::vector<Byte> out(
        align16(atlas_offset + atlas.size()), 0);
    std::memcpy(out.data(), "DPUI", 4);
    put_u16(out, 4, 2);
    put_u16(out, 6, kDpuiHeaderSize);
    put_u32(out, 8, static_cast<std::uint32_t>(out.size()));
    put_u32(out, 16, atlas_width);
    put_u32(out, 20, atlas_height);
    put_u32(out, 24, 2);
    put_u32(out, 28, static_cast<std::uint32_t>(sprites.size()));
    put_u32(out, 32, static_cast<std::uint32_t>(table));
    put_u32(out, 36, kDpuiQuadSize);
    put_u32(out, 40, static_cast<std::uint32_t>(atlas_offset));
    put_u32(out, 44, static_cast<std::uint32_t>(atlas.size()));
    put_u32(out, 48, 0x1fu);
    put_u32(out, 52, source_width);
    put_u32(out, 56, source_height);
    put_u32(out, 60, static_cast<std::uint32_t>(resources.size()));
    put_u32(out, 64, 1);
    put_u32(out, 68, 0);
    put_u32(out, 72, 0);
    for (std::size_t index = 0; index < sprites.size(); ++index) {
        const UiSourceSprite& sprite = sprites[index];
        const std::size_t at = table + index * kDpuiQuadSize;
        put_u16(out, at, sprite.id);
        put_u16(out, at + 2, sprite.channel);
        put_u16(
            out, at + 4,
            static_cast<std::uint16_t>(sprite.screen_x));
        put_u16(
            out, at + 6,
            static_cast<std::uint16_t>(sprite.screen_y));
        put_u16(out, at + 8, sprite.width);
        put_u16(out, at + 10, sprite.height);
        put_u16(out, at + 12, sprite.atlas_x);
        put_u16(out, at + 14, sprite.atlas_y);
        put_u16(out, at + 16, sprite.width);
        put_u16(out, at + 18, sprite.height);
        put_u32(out, at + 20, sprite.color);
        put_u32(out, at + 24, fnv1a(sprite.name.c_str()));
        put_u16(out, at + 28, sprite.advance);
    }
    std::copy(
        atlas.begin(), atlas.end(),
        out.begin() + static_cast<std::ptrdiff_t>(atlas_offset));
    put_u32(out, 12, crc32(out));
    listing << "dpui_version=2"
            << " atlas_width=" << atlas_width
            << " atlas_height=" << atlas_height
            << " atlas_format=4444"
            << " atlas_bytes=" << atlas.size()
            << " records=" << sprites.size()
            << " original_assets=" << resources.size()
            << " source_fonts=1"
            << " procedural_game_sprites=0\n";
    *inventory = listing.str();
    return out;
}

struct AnimationData {
    std::unique_ptr<Byte, decltype(&std::free)> bytes{nullptr, &std::free};
    J3DAnmTransformKey* animation = nullptr;
    const char* name = nullptr;
    std::uint16_t id = 0;
};

AnimationData load_animation(
    JKRMemArchive& archive, std::uint16_t id, const char* expected_name) {
    const char* name = nullptr;
    for (std::uint32_t index = 0; index < archive.countFile(); ++index) {
        JKRArchive::SDirEntry entry = {};
        if (archive.getDirEntry(&entry, index) && entry.id == id) {
            name = entry.name;
            break;
        }
    }
    if (name == nullptr ||
        (expected_name != nullptr &&
         std::strcmp(name, expected_name) != 0)) {
        throw std::runtime_error("DPAN BCK identity mismatch");
    }
    void* source = archive.getResource(id);
    const std::uint32_t stored = archive.getResSize(source);
    const auto* input = static_cast<const Byte*>(source);
    const bool compressed =
        stored >= 16 && std::memcmp(input, "Yaz0", 4) == 0;
    const std::uint32_t size = compressed
        ? JKRDecompExpandSize(const_cast<Byte*>(input))
        : stored;
    Byte* bytes = static_cast<Byte*>(std::malloc(size));
    if (bytes == nullptr) {
        throw std::runtime_error("DPAN allocation failed");
    }
    if (compressed) {
        JKRDecomp::decode(
            const_cast<Byte*>(input), bytes, size, 0);
    } else {
        std::memcpy(bytes, input, size);
    }
    J3DAnmBase* base = J3DAnmLoaderDataBase::load(bytes);
    auto* animation = dynamic_cast<J3DAnmTransformKey*>(base);
    if (animation == nullptr || animation->field_0x1e == 0 ||
        animation->field_0x1e > 64) {
        std::free(bytes);
        throw std::runtime_error("DPAN clip joint count is unsupported");
    }
    return {
        std::unique_ptr<Byte, decltype(&std::free)>(bytes, &std::free),
        animation,
        expected_name,
        id,
    };
}

std::array<float, 4> quaternion(const S16Vec& rotation) {
    constexpr float scale =
        3.14159265358979323846f / 32768.0f * 0.5f;
    const float x = rotation.x * scale;
    const float y = rotation.y * scale;
    const float z = rotation.z * scale;
    const float sx = std::sin(x);
    const float cx = std::cos(x);
    const float sy = std::sin(y);
    const float cy = std::cos(y);
    const float sz = std::sin(z);
    const float cz = std::cos(z);
    std::array<float, 4> q = {
        sx * cy * cz - cx * sy * sz,
        cx * sy * cz + sx * cy * sz,
        cx * cy * sz - sx * sy * cz,
        cx * cy * cz + sx * sy * sz,
    };
    const float length = std::sqrt(
        q[0] * q[0] + q[1] * q[1] +
        q[2] * q[2] + q[3] * q[3]);
    for (float& value : q) {
        value /= length;
    }
    return q;
}

std::vector<Byte> serialize_dpan(std::vector<Byte>& archive_bytes) {
    JKRMemArchive archive(
        archive_bytes.data(),
        static_cast<std::uint32_t>(archive_bytes.size()),
        JKRMEMBREAK_FLAG_UNKNOWN0);
    std::array<AnimationData, 4> clips = {
        load_animation(archive, 0x26a, "waits.bck"),
        load_animation(archive, 0x277, "walks.bck"),
        load_animation(archive, 0x0cd, "dashs.bck"),
        load_animation(archive, 0x233, "stepl.bck"),
    };
    std::array<std::uint32_t, 4> samples{};
    std::size_t data_offset =
        align16(kDpanHeaderSize + kDpanClipSize * clips.size());
    std::size_t total = data_offset;
    for (std::size_t index = 0; index < clips.size(); ++index) {
        if (clips[index].animation->field_0x1e != kJointCount) {
            throw std::runtime_error(
                "playable DPAN clip is not a 35-joint transform");
        }
        samples[index] =
            static_cast<std::uint32_t>(
                std::floor(clips[index].animation->getFrameMax())) + 1;
        total = align16(
            total + samples[index] * kJointCount * kDpanSampleSize);
    }
    std::vector<Byte> out(total, 0);
    std::memcpy(out.data(), "DPAN", 4);
    put_u16(out, 4, 1);
    put_u16(out, 6, kDpanHeaderSize);
    put_u32(out, 8, static_cast<std::uint32_t>(out.size()));
    put_u32(out, 16, static_cast<std::uint32_t>(clips.size()));
    put_u32(out, 20, kJointCount);
    put_u32(out, 24, 30);
    put_u32(out, 28, 6);
    put_u32(out, 32, kDpanHeaderSize);
    put_u32(out, 36, kDpanClipSize);
    put_u32(out, 40, static_cast<std::uint32_t>(data_offset));

    std::size_t cursor = data_offset;
    for (std::size_t clip_index = 0;
         clip_index < clips.size();
         ++clip_index) {
        AnimationData& clip = clips[clip_index];
        const std::size_t entry =
            kDpanHeaderSize + clip_index * kDpanClipSize;
        put_u32(out, entry, clip.id);
        put_u32(out, entry + 4, fnv1a(clip.name));
        put_u32(
            out, entry + 8,
            static_cast<std::uint32_t>(clip.animation->getFrameMax()));
        put_u32(out, entry + 12, samples[clip_index]);
        put_u32(out, entry + 16, kJointCount);
        put_u32(
            out, entry + 20,
            static_cast<std::uint32_t>(clip.animation->getAttribute()));
        put_u32(out, entry + 24, static_cast<std::uint32_t>(cursor));
        put_u32(
            out, entry + 28,
            samples[clip_index] * kJointCount * kDpanSampleSize);
        std::array<std::array<float, 4>, 35> previous{};
        for (std::uint32_t sample = 0; sample < samples[clip_index]; ++sample) {
            clip.animation->setFrame(static_cast<float>(sample));
            for (std::uint32_t joint = 0; joint < kJointCount; ++joint) {
                J3DTransformInfo transform = {};
                clip.animation->getTransform(joint, &transform);
                std::array<float, 4> q = quaternion(transform.mRotation);
                if (sample != 0) {
                    float dot = 0.0f;
                    for (std::size_t item = 0; item < 4; ++item) {
                        dot += q[item] * previous[joint][item];
                    }
                    if (dot < 0.0f) {
                        for (float& value : q) {
                            value = -value;
                        }
                    }
                }
                previous[joint] = q;
                const std::size_t at = cursor +
                    (sample * kJointCount + joint) * kDpanSampleSize;
                put_f32(out, at, transform.mTranslate.x);
                put_f32(out, at + 4, transform.mTranslate.y);
                put_f32(out, at + 8, transform.mTranslate.z);
                for (std::size_t item = 0; item < 4; ++item) {
                    put_f32(out, at + 12 + item * 4, q[item]);
                }
                put_f32(out, at + 28, transform.mScale.x);
                put_f32(out, at + 32, transform.mScale.y);
                put_f32(out, at + 36, transform.mScale.z);
            }
        }
        cursor = align16(
            cursor + samples[clip_index] * kJointCount * kDpanSampleSize);
    }
    put_u32(out, 12, 0);
    put_u32(out, 12, crc32(out));
    return out;
}

template <typename T>
void write_curve_keys(
    std::ostream& out, const J3DAnmKeyTableBase& info, const T* data) {
    const std::uint16_t count = info.mMaxFrame;
    const std::uint16_t offset = info.mOffset;
    const std::uint16_t tangent_type = info.mType;
    out << "{\"key_count\":" << count
        << ",\"offset\":" << offset
        << ",\"tangent_type\":" << tangent_type
        << ",\"interpolation\":\""
        << (count > 1 ? "hermite" : "constant")
        << "\",\"keys\":[";
    if (count == 1) {
        out << "{\"time\":0,\"value\":"
            << static_cast<double>(data[offset])
            << ",\"tangent_in\":null,\"tangent_out\":null}";
    } else if (count > 1) {
        const std::size_t stride = tangent_type == 0 ? 3 : 4;
        for (std::uint16_t key = 0; key < count; ++key) {
            if (key != 0) {
                out << ',';
            }
            const std::size_t at = offset + key * stride;
            const double tangent_in = static_cast<double>(data[at + 2]);
            const double tangent_out = static_cast<double>(
                data[at + (tangent_type == 0 ? 2 : 3)]);
            out << "{\"time\":" << static_cast<double>(data[at])
                << ",\"value\":" << static_cast<double>(data[at + 1])
                << ",\"tangent_in\":" << tangent_in
                << ",\"tangent_out\":" << tangent_out << '}';
        }
    }
    out << "]}";
}

void write_bck_curve_dump(
    std::vector<Byte>& archive_bytes, const char* output_path) {
    JKRMemArchive archive(
        archive_bytes.data(),
        static_cast<std::uint32_t>(archive_bytes.size()),
        JKRMEMBREAK_FLAG_UNKNOWN0);
    std::array<AnimationData, 4> clips = {
        load_animation(archive, 0x26a, "waits.bck"),
        load_animation(archive, 0x277, "walks.bck"),
        load_animation(archive, 0x0cd, "dashs.bck"),
        load_animation(archive, 0x233, "stepl.bck"),
    };
    std::ofstream out(output_path, std::ios::binary | std::ios::trunc);
    if (!out) {
        throw std::runtime_error("unable to open BCK curve output");
    }
    out << std::setprecision(std::numeric_limits<double>::max_digits10)
        << "{\"schema\":\"dusklight.bck.ank1.curves.v1\",\"clips\":[";
    constexpr const char* components[] = {"scale", "rotation", "translation"};
    constexpr const char* axes[] = {"x", "y", "z"};
    for (std::size_t clip_index = 0; clip_index < clips.size(); ++clip_index) {
        if (clip_index != 0) {
            out << ',';
        }
        const AnimationData& clip = clips[clip_index];
        out << "{\"resource_id\":" << clip.id
            << ",\"name\":\"" << clip.name
            << "\",\"duration\":" << clip.animation->getFrameMax()
            << ",\"loop_mode\":"
            << static_cast<unsigned>(clip.animation->getAttribute())
            << ",\"rotation_decimal_shift\":" << clip.animation->mDecShift
            << ",\"joint_count\":" << clip.animation->field_0x1e
            << ",\"tracks\":[";
        bool first_track = true;
        for (std::uint16_t joint = 0;
             joint < clip.animation->field_0x1e; ++joint) {
            for (std::uint16_t axis = 0; axis < 3; ++axis) {
                const J3DAnmTransformKeyTable& table =
                    clip.animation->mAnmTable[joint * 3 + axis];
                const J3DAnmKeyTableBase* infos[] = {
                    &table.mScaleInfo,
                    &table.mRotationInfo,
                    &table.mTranslateInfo,
                };
                for (std::uint16_t component = 0; component < 3; ++component) {
                    if (!first_track) {
                        out << ',';
                    }
                    first_track = false;
                    out << "{\"joint\":" << joint
                        << ",\"component\":\"" << components[component]
                        << "\",\"axis\":\"" << axes[axis]
                        << "\",\"curve\":";
                    if (component == 0) {
                        write_curve_keys(
                            out, *infos[component], clip.animation->mScaleData);
                    } else if (component == 1) {
                        write_curve_keys(
                            out, *infos[component], clip.animation->mRotData);
                    } else {
                        write_curve_keys(
                            out, *infos[component], clip.animation->mTransData);
                    }
                    out << '}';
                }
            }
        }
        out << "]}";
    }
    out << "]}\n";
    if (!out) {
        throw std::runtime_error("unable to write BCK curve output");
    }
}

std::vector<Byte> serialize_single_dpan(
    std::vector<Byte>& archive_bytes, std::uint16_t resource_id) {
    JKRMemArchive archive(
        archive_bytes.data(),
        static_cast<std::uint32_t>(archive_bytes.size()),
        JKRMEMBREAK_FLAG_UNKNOWN0);
    AnimationData clip =
        load_animation(archive, resource_id, nullptr);
    const std::uint32_t joints = clip.animation->field_0x1e;
    const std::uint32_t samples =
        static_cast<std::uint32_t>(
            std::floor(clip.animation->getFrameMax())) + 1;
    const std::size_t data_offset =
        align16(kDpanHeaderSize + kDpanClipSize);
    const std::size_t data_size =
        static_cast<std::size_t>(samples) * joints * kDpanSampleSize;
    std::vector<Byte> out(align16(data_offset + data_size), 0);
    std::memcpy(out.data(), "DPAN", 4);
    put_u16(out, 4, 1);
    put_u16(out, 6, kDpanHeaderSize);
    put_u32(out, 8, static_cast<std::uint32_t>(out.size()));
    put_u32(out, 16, 1);
    put_u32(out, 20, joints);
    put_u32(out, 24, 30);
    put_u32(out, 28, 6);
    put_u32(out, 32, kDpanHeaderSize);
    put_u32(out, 36, kDpanClipSize);
    put_u32(out, 40, static_cast<std::uint32_t>(data_offset));
    put_u32(out, kDpanHeaderSize, clip.id);
    put_u32(out, kDpanHeaderSize + 4, fnv1a(clip.name));
    put_u32(
        out, kDpanHeaderSize + 8,
        static_cast<std::uint32_t>(clip.animation->getFrameMax()));
    put_u32(out, kDpanHeaderSize + 12, samples);
    put_u32(out, kDpanHeaderSize + 16, joints);
    put_u32(
        out, kDpanHeaderSize + 20,
        static_cast<std::uint32_t>(clip.animation->getAttribute()));
    put_u32(
        out, kDpanHeaderSize + 24,
        static_cast<std::uint32_t>(data_offset));
    put_u32(
        out, kDpanHeaderSize + 28,
        static_cast<std::uint32_t>(data_size));
    std::vector<std::array<float, 4>> previous(joints);
    for (std::uint32_t sample = 0; sample < samples; ++sample) {
        clip.animation->setFrame(static_cast<float>(sample));
        for (std::uint32_t joint = 0; joint < joints; ++joint) {
            J3DTransformInfo transform = {};
            clip.animation->getTransform(joint, &transform);
            std::array<float, 4> q = quaternion(transform.mRotation);
            if (sample != 0) {
                float dot = 0.0f;
                for (std::size_t item = 0; item < 4; ++item) {
                    dot += q[item] * previous[joint][item];
                }
                if (dot < 0.0f) {
                    for (float& value : q) {
                        value = -value;
                    }
                }
            }
            previous[joint] = q;
            const std::size_t at = data_offset +
                (sample * joints + joint) * kDpanSampleSize;
            put_f32(out, at, transform.mTranslate.x);
            put_f32(out, at + 4, transform.mTranslate.y);
            put_f32(out, at + 8, transform.mTranslate.z);
            for (std::size_t item = 0; item < 4; ++item) {
                put_f32(out, at + 12 + item * 4, q[item]);
            }
            put_f32(out, at + 28, transform.mScale.x);
            put_f32(out, at + 32, transform.mScale.y);
            put_f32(out, at + 36, transform.mScale.z);
        }
    }
    put_u32(out, 12, 0);
    put_u32(out, 12, crc32(out));
    return out;
}

std::uint8_t room_material_bucket(J3DMaterial* material) {
    if (material == nullptr) {
        return 3;
    }
    const std::uint32_t mode = material->getMaterialMode();
    if ((mode & 4u) != 0) {
        return 2;
    }
    if ((mode & 2u) != 0) {
        return 1;
    }
    return 0;
}

std::uint32_t baked_room_color(
    const Vec& normal,
    bool demo01_actor = false) {
    // F_SP108 DENV source ray=(0,.707107,.707107), transformed through the
    // demo01_01 actor yaw (-115 degrees) and inverted to light-to-surface.
    constexpr Vec room_light = {0.30304575f, 0.80812204f, 0.50507627f};
    constexpr Vec demo01_light = {0.6408564f, -0.7071068f, 0.2988362f};
    const Vec light = demo01_actor ? demo01_light : room_light;
    const float length = std::sqrt(
        normal.x * normal.x + normal.y * normal.y + normal.z * normal.z);
    const float diffuse = length > 1.0e-6f
        ? std::max(
              0.0f,
              (normal.x * light.x + normal.y * light.y + normal.z * light.z) /
                  length)
        : 0.0f;
    const float intensity = demo01_actor
        ? std::min(1.0f, 0.68f + 0.44f * diffuse)
        : 0.38f + 0.62f * diffuse;
    const Byte red = static_cast<Byte>(
        std::clamp(intensity * (demo01_actor ? 255.0f : 232.0f),
                   0.0f, 255.0f));
    const Byte green = static_cast<Byte>(
        std::clamp(intensity * (demo01_actor ? 224.0f : 224.0f),
                   0.0f, 255.0f));
    const Byte blue = static_cast<Byte>(
        std::clamp(intensity * (demo01_actor ? 190.0f : 208.0f),
                   0.0f, 255.0f));
    return static_cast<std::uint32_t>(red) |
           (static_cast<std::uint32_t>(green) << 8) |
           (static_cast<std::uint32_t>(blue) << 16) |
           0xff000000u;
}

std::vector<Byte> serialize_dprm(
    J3DModelData* source,
    J3DAnmTransform* animation = nullptr,
    float animation_frame = 0.0f) {
    if (source == nullptr) {
        throw std::runtime_error("DPRM room model absent");
    }
    J3DModel model(source, J3DMdlFlag_None, 1);
    Mtx identity;
    MTXIdentity(identity);
    model.setBaseTRMtx(identity);
    J3DJoint* root = source->getJointNodePointer(0);
    J3DMtxCalc* previous_calc = root != nullptr ? root->getMtxCalc() : nullptr;
    using AnimationCalc = J3DMtxCalcAnimation<
        J3DMtxCalcAnimationAdaptorDefault<J3DMtxCalcCalcTransformMaya>,
        J3DMtxCalcJ3DSysInitMaya>;
    std::unique_ptr<AnimationCalc> animation_calc;
    if (animation != nullptr) {
        if (root == nullptr || animation->field_0x1e != source->getJointNum()) {
            throw std::runtime_error(
                "DPRM animation/model joint count mismatch");
        }
        animation->setFrame(animation_frame);
        animation_calc = std::make_unique<AnimationCalc>(animation);
        root->setMtxCalc(animation_calc.get());
    }
    model.calc();
    if (root != nullptr) {
        root->setMtxCalc(previous_calc);
    }
    std::vector<std::uint16_t> shapes;
    for (std::uint16_t index = 0; index < source->getShapeNum(); ++index) {
        shapes.push_back(index);
    }
    MeshBuild mesh;
    append_piece(
        mesh,
        {&model, shapes, 0, -1, 0, animation != nullptr});
    if (mesh.indices.empty() || mesh.indices.size() % 3 != 0 ||
        mesh.indices.size() / 3 > 30000 ||
        mesh.vertices.empty() || mesh.vertices.size() > 45000 ||
        mesh.submeshes.empty() || mesh.submeshes.size() > 96) {
        throw std::runtime_error("DPRM room topology outside PSP budgets");
    }

    constexpr std::size_t header_size = 256;
    constexpr std::size_t section_size = 32;
    constexpr std::size_t vertex_size = 24;
    constexpr std::size_t submesh_size = 48;
    constexpr std::uint32_t section_count = 4;
    const std::size_t section_table = 128;
    const std::size_t vertex_offset = header_size;
    const std::size_t index_offset =
        align16(vertex_offset + mesh.vertices.size() * vertex_size);
    const std::size_t submesh_offset =
        align16(index_offset + mesh.indices.size() * sizeof(std::uint16_t));
    const std::size_t provenance_offset =
        align16(submesh_offset + mesh.submeshes.size() * submesh_size);
    const std::size_t total =
        align16(provenance_offset + mesh.submeshes.size() * 16);
    std::vector<Byte> out(total, 0);
    std::memcpy(out.data(), "DPRM", 4);
    put_u16(out, 4, 1);
    put_u16(out, 6, header_size);
    put_u32(out, 8, static_cast<std::uint32_t>(out.size()));
    put_u32(out, 16, section_count);
    put_u32(out, 20, static_cast<std::uint32_t>(mesh.vertices.size()));
    put_u32(out, 24, static_cast<std::uint32_t>(mesh.indices.size()));
    put_u32(out, 28, static_cast<std::uint32_t>(mesh.indices.size() / 3));
    put_u32(out, 32, static_cast<std::uint32_t>(mesh.submeshes.size()));
    put_u32(out, 36, source->getMaterialNum());
    put_u32(out, 40, source->getTexture()->getNum());
    put_u32(out, 44, 1);
    put_f32(out, 48, mesh.minimum.x);
    put_f32(out, 52, mesh.minimum.y);
    put_f32(out, 56, mesh.minimum.z);
    put_f32(out, 60, mesh.maximum.x);
    put_f32(out, 64, mesh.maximum.y);
    put_f32(out, 68, mesh.maximum.z);
    put_u32(out, 72, static_cast<std::uint32_t>(section_table));
    put_u32(out, 76, section_size);
    put_u32(out, 80, 24);
    put_u32(out, 84, 0);
    put_u32(out, 88, 4);
    put_u32(out, 92, 8);
    put_u32(out, 96, 12);
    put_u32(out, 100, 16);
    put_u32(out, 104, 20);
    put_u32(out, 108, 1);
    const char* source_id = std::getenv("DUSKLIGHT_ROOM_SOURCE_ID");
    put_u32(
        out, 112,
        fnv1a(source_id != nullptr && source_id[0] != '\0'
                  ? source_id
                  : "F_SP110/R02_00.arc/model.bmd"));
    put_u32(out, 116, 0);
    put_u32(out, 120, 0);
    put_u32(out, 124, 0);

    auto section = [&](std::uint32_t index,
                       std::uint32_t kind,
                       std::size_t offset,
                       std::size_t bytes,
                       std::uint32_t count,
                       std::uint32_t stride) {
        const std::size_t at = section_table + index * section_size;
        put_u32(out, at, kind);
        put_u32(out, at + 4, static_cast<std::uint32_t>(offset));
        put_u32(out, at + 8, static_cast<std::uint32_t>(bytes));
        put_u32(out, at + 12, count);
        put_u32(out, at + 16, stride);
        put_u32(out, at + 20, 16);
    };
    section(
        0, 1, vertex_offset, mesh.vertices.size() * vertex_size,
        static_cast<std::uint32_t>(mesh.vertices.size()), vertex_size);
    section(
        1, 2, index_offset, mesh.indices.size() * sizeof(std::uint16_t),
        static_cast<std::uint32_t>(mesh.indices.size()), sizeof(std::uint16_t));
    section(
        2, 3, submesh_offset, mesh.submeshes.size() * submesh_size,
        static_cast<std::uint32_t>(mesh.submeshes.size()), submesh_size);
    section(
        3, 4, provenance_offset, mesh.submeshes.size() * 16,
        static_cast<std::uint32_t>(mesh.submeshes.size()), 16);

    for (std::size_t index = 0; index < mesh.vertices.size(); ++index) {
        const Vertex& vertex = mesh.vertices[index];
        const std::size_t at = vertex_offset + index * vertex_size;
        put_f32(out, at, vertex.uv[0]);
        put_f32(out, at + 4, vertex.uv[1]);
        put_u32(
            out, at + 8,
            baked_room_color(vertex.normal, animation != nullptr));
        put_f32(out, at + 12, vertex.position.x);
        put_f32(out, at + 16, vertex.position.y);
        put_f32(out, at + 20, vertex.position.z);
    }
    for (std::size_t index = 0; index < mesh.indices.size(); ++index) {
        put_u16(out, index_offset + index * 2, mesh.indices[index]);
    }
    for (std::size_t index = 0; index < mesh.submeshes.size(); ++index) {
        const Submesh& submesh = mesh.submeshes[index];
        J3DMaterial* material =
            source->getMaterialNodePointer(submesh.source_material);
        const std::uint8_t bucket = room_material_bucket(material);
        const std::size_t at = submesh_offset + index * submesh_size;
        put_u32(out, at, submesh.first_index);
        put_u32(out, at + 4, submesh.index_count);
        put_u16(out, at + 8, submesh.material);
        put_u16(out, at + 10, submesh.texture);
        out[at + 12] = bucket;
        out[at + 13] = 1;
        put_u16(out, at + 14, submesh.source_shape);
        put_u16(out, at + 16, submesh.source_material);
        J3DZMode* depth = material != nullptr ? material->getZMode() : nullptr;
        const std::uint16_t depth_id =
            depth != nullptr ? depth->mZModeID : calcZModeID(0, 7, 0);
        out[at + 18] = static_cast<Byte>(
            (depth_id >= 16 ? 1u : 0u) | ((depth_id & 1u) != 0 ? 2u : 0u));
        out[at + 19] = static_cast<Byte>((depth_id & 15u) / 2u);
        put_u32(out, at + 20, fnv1a("model.bmd"));
        put_u32(out, at + 24, 1);
        const std::size_t provenance = provenance_offset + index * 16;
        put_u32(out, provenance, fnv1a("model.bmd"));
        put_u16(out, provenance + 4, submesh.source_shape);
        put_u16(out, provenance + 6, submesh.source_material);
        put_u32(out, provenance + 8, submesh.first_index / 3);
        put_u32(out, provenance + 12, submesh.index_count / 3);
    }
    put_u32(out, 116, 1);
    put_u32(out, 12, 0);
    put_u32(out, 12, crc32(out));
    return out;
}

std::uint32_t read_be32_room(const Byte* source) {
    return (static_cast<std::uint32_t>(source[0]) << 24) |
           (static_cast<std::uint32_t>(source[1]) << 16) |
           (static_cast<std::uint32_t>(source[2]) << 8) |
           source[3];
}

float read_be_float_room(const Byte* source) {
    const std::uint32_t bits = read_be32_room(source);
    float value = 0.0f;
    std::memcpy(&value, &bits, sizeof(value));
    return value;
}

Vec room_cross(const Vec& left, const Vec& right) {
    return {
        left.y * right.z - left.z * right.y,
        left.z * right.x - left.x * right.z,
        left.x * right.y - left.y * right.x,
    };
}

float room_dot(const Vec& left, const Vec& right) {
    return left.x * right.x + left.y * right.y + left.z * right.z;
}

struct CollisionTriangle {
    Vec point[3];
    Vec normal;
    std::uint16_t attribute;
};

std::vector<CollisionTriangle> decode_room_kcl(
    const Byte* bytes, std::uint32_t size) {
    if (bytes == nullptr || size < 0x38) {
        throw std::runtime_error("DPCL KCL truncated");
    }
    const std::uint32_t positions = read_be32_room(bytes);
    const std::uint32_t normals = read_be32_room(bytes + 4);
    const std::uint32_t prisms = read_be32_room(bytes + 8);
    const std::uint32_t blocks = read_be32_room(bytes + 12);
    if (positions < 0x38 || positions > normals ||
        normals > prisms || prisms > blocks || blocks > size ||
        (normals - positions) % 12 != 0 ||
        (blocks - prisms) % 16 != 0) {
        throw std::runtime_error("DPCL KCL layout invalid");
    }
    const std::uint32_t position_count = (normals - positions) / 12;
    // Dusklight's dBgWKCol swaps normals through prism_data + sizeof(Vec).
    // KCL deliberately overlaps the final two normal vectors with the
    // non-polygon prism record at index zero.
    const std::uint32_t normal_count =
        (prisms + 12 - normals + 11) / 12;
    const std::uint32_t prism_count = (blocks - prisms) / 16;
    auto read_vec = [&](std::uint32_t offset, std::uint32_t index) {
        const Byte* value = bytes + offset + index * 12;
        return Vec{
            read_be_float_room(value),
            read_be_float_room(value + 4),
            read_be_float_room(value + 8),
        };
    };
    std::vector<CollisionTriangle> triangles;
    for (std::uint32_t index = 1; index < prism_count; ++index) {
        const Byte* prism = bytes + prisms + index * 16;
        const float height = read_be_float_room(prism);
        const std::uint16_t position_index = read_be16(prism + 4);
        const std::uint16_t face_index = read_be16(prism + 6);
        const std::uint16_t edge1_index = read_be16(prism + 8);
        const std::uint16_t edge2_index = read_be16(prism + 10);
        const std::uint16_t edge3_index = read_be16(prism + 12);
        if (position_index >= position_count ||
            face_index >= normal_count ||
            edge1_index >= normal_count ||
            edge2_index >= normal_count ||
            edge3_index >= normal_count) {
            throw std::runtime_error(
                "DPCL KCL prism index invalid: prism=" +
                std::to_string(index) +
                " positions=" + std::to_string(position_count) +
                "/" + std::to_string(position_index) +
                " normals=" + std::to_string(normal_count) +
                "/" + std::to_string(face_index) +
                "," + std::to_string(edge1_index) +
                "," + std::to_string(edge2_index) +
                "," + std::to_string(edge3_index));
        }
        const Vec first = read_vec(positions, position_index);
        const Vec face = read_vec(normals, face_index);
        const Vec edge1 = read_vec(normals, edge1_index);
        const Vec edge2 = read_vec(normals, edge2_index);
        const Vec edge3 = read_vec(normals, edge3_index);
        const Vec first_direction = room_cross(face, edge1);
        const Vec second_direction = room_cross(edge2, face);
        const float first_denominator = room_dot(first_direction, edge3);
        const float second_denominator = room_dot(second_direction, edge3);
        if (!std::isfinite(height) ||
            std::fabs(first_denominator) < 1.0e-6f ||
            std::fabs(second_denominator) < 1.0e-6f) {
            throw std::runtime_error("DPCL KCL prism degenerate");
        }
        const float first_scale = height / first_denominator;
        const float second_scale = height / second_denominator;
        const Vec third = {
            first.x + first_direction.x * first_scale,
            first.y + first_direction.y * first_scale,
            first.z + first_direction.z * first_scale,
        };
        const Vec second = {
            first.x + second_direction.x * second_scale,
            first.y + second_direction.y * second_scale,
            first.z + second_direction.z * second_scale,
        };
        if (!std::isfinite(second.x) || !std::isfinite(second.y) ||
            !std::isfinite(second.z) || !std::isfinite(third.x) ||
            !std::isfinite(third.y) || !std::isfinite(third.z)) {
            throw std::runtime_error("DPCL KCL triangle non-finite");
        }
        triangles.push_back({
            {first, second, third},
            face,
            read_be16(prism + 14),
        });
    }
    if (triangles.empty() || triangles.size() > 25000) {
        throw std::runtime_error("DPCL room triangle budget invalid");
    }
    return triangles;
}

std::vector<CollisionTriangle> decode_movebg_dzb(
    const Byte* bytes, std::uint32_t size) {
    if (bytes == nullptr || size < 0x34) {
        throw std::runtime_error("DPCL DZB truncated");
    }
    const std::uint32_t vertex_count = read_be32_room(bytes);
    const std::uint32_t vertex_offset = read_be32_room(bytes + 4);
    const std::uint32_t triangle_count = read_be32_room(bytes + 8);
    const std::uint32_t triangle_offset = read_be32_room(bytes + 12);
    if (vertex_count == 0 || vertex_count > 65535 ||
        triangle_count == 0 || triangle_count > 25000 ||
        vertex_offset > size ||
        vertex_count > (size - vertex_offset) / 12 ||
        triangle_offset > size ||
        triangle_count > (size - triangle_offset) / 10) {
        throw std::runtime_error("DPCL DZB layout invalid");
    }
    std::vector<Vec> vertices;
    vertices.reserve(vertex_count);
    for (std::uint32_t index = 0; index < vertex_count; ++index) {
        const Byte* value = bytes + vertex_offset + index * 12;
        vertices.push_back({
            read_be_float_room(value),
            read_be_float_room(value + 4),
            read_be_float_room(value + 8),
        });
    }
    std::vector<CollisionTriangle> triangles;
    triangles.reserve(triangle_count);
    for (std::uint32_t index = 0; index < triangle_count; ++index) {
        const Byte* value = bytes + triangle_offset + index * 10;
        const std::uint16_t first = read_be16(value);
        const std::uint16_t second = read_be16(value + 2);
        const std::uint16_t third = read_be16(value + 4);
        if (first >= vertex_count || second >= vertex_count ||
            third >= vertex_count) {
            throw std::runtime_error("DPCL DZB triangle index invalid");
        }
        const Vec edge_a = {
            vertices[second].x - vertices[first].x,
            vertices[second].y - vertices[first].y,
            vertices[second].z - vertices[first].z,
        };
        const Vec edge_b = {
            vertices[third].x - vertices[first].x,
            vertices[third].y - vertices[first].y,
            vertices[third].z - vertices[first].z,
        };
        Vec normal = room_cross(edge_a, edge_b);
        const float length = std::sqrt(room_dot(normal, normal));
        if (!std::isfinite(length) || length < 1.0e-6f) {
            throw std::runtime_error("DPCL DZB triangle degenerate");
        }
        normal.x /= length;
        normal.y /= length;
        normal.z /= length;
        triangles.push_back({
            {vertices[first], vertices[second], vertices[third]},
            normal,
            read_be16(value + 6),
        });
    }
    return triangles;
}

std::vector<Byte> serialize_collision_triangles(
    const std::vector<CollisionTriangle>& triangles) {
    Vec minimum = {INFINITY, INFINITY, INFINITY};
    Vec maximum = {-INFINITY, -INFINITY, -INFINITY};
    for (const CollisionTriangle& triangle : triangles) {
        for (const Vec& point : triangle.point) {
            minimum.x = std::min(minimum.x, point.x);
            minimum.y = std::min(minimum.y, point.y);
            minimum.z = std::min(minimum.z, point.z);
            maximum.x = std::max(maximum.x, point.x);
            maximum.y = std::max(maximum.y, point.y);
            maximum.z = std::max(maximum.z, point.z);
        }
    }
    constexpr float cell_size = 512.0f;
    const std::uint32_t cells_x = static_cast<std::uint32_t>(
        std::ceil((maximum.x - minimum.x) / cell_size)) + 1;
    const std::uint32_t cells_z = static_cast<std::uint32_t>(
        std::ceil((maximum.z - minimum.z) / cell_size)) + 1;
    if (cells_x == 0 || cells_z == 0 || cells_x * cells_z > 4096) {
        throw std::runtime_error("DPCL grid dimensions invalid");
    }
    std::vector<std::vector<std::uint16_t>> cells(cells_x * cells_z);
    for (std::uint32_t index = 0; index < triangles.size(); ++index) {
        const CollisionTriangle& triangle = triangles[index];
        float min_x = INFINITY;
        float max_x = -INFINITY;
        float min_z = INFINITY;
        float max_z = -INFINITY;
        for (const Vec& point : triangle.point) {
            min_x = std::min(min_x, point.x);
            max_x = std::max(max_x, point.x);
            min_z = std::min(min_z, point.z);
            max_z = std::max(max_z, point.z);
        }
        const std::uint32_t first_x = std::min(
            cells_x - 1,
            static_cast<std::uint32_t>((min_x - minimum.x) / cell_size));
        const std::uint32_t last_x = std::min(
            cells_x - 1,
            static_cast<std::uint32_t>((max_x - minimum.x) / cell_size));
        const std::uint32_t first_z = std::min(
            cells_z - 1,
            static_cast<std::uint32_t>((min_z - minimum.z) / cell_size));
        const std::uint32_t last_z = std::min(
            cells_z - 1,
            static_cast<std::uint32_t>((max_z - minimum.z) / cell_size));
        for (std::uint32_t z = first_z; z <= last_z; ++z) {
            for (std::uint32_t x = first_x; x <= last_x; ++x) {
                cells[z * cells_x + x].push_back(
                    static_cast<std::uint16_t>(index));
            }
        }
    }
    std::uint32_t reference_count = 0;
    for (const auto& cell : cells) {
        reference_count += static_cast<std::uint32_t>(cell.size());
    }
    constexpr std::size_t header_size = 256;
    constexpr std::size_t triangle_size = 32;
    constexpr std::size_t cell_size_bytes = 16;
    const std::uint32_t vertex_count =
        static_cast<std::uint32_t>(triangles.size() * 3);
    const std::size_t vertex_offset = header_size;
    const std::size_t triangle_offset =
        align16(vertex_offset + vertex_count * 12);
    const std::size_t cell_offset =
        align16(triangle_offset + triangles.size() * triangle_size);
    const std::size_t reference_offset =
        align16(cell_offset + cells.size() * cell_size_bytes);
    const std::size_t total =
        align16(reference_offset + reference_count * sizeof(std::uint16_t));
    std::vector<Byte> out(total, 0);
    std::memcpy(out.data(), "DPCL", 4);
    put_u16(out, 4, 1);
    put_u16(out, 6, header_size);
    put_u32(out, 8, static_cast<std::uint32_t>(out.size()));
    put_u32(out, 16, vertex_count);
    put_u32(out, 20, static_cast<std::uint32_t>(triangles.size()));
    put_u32(out, 24, cells_x);
    put_u32(out, 28, cells_z);
    put_u32(out, 32, static_cast<std::uint32_t>(cells.size()));
    put_u32(out, 36, reference_count);
    put_f32(out, 40, cell_size);
    put_f32(out, 44, minimum.x);
    put_f32(out, 48, minimum.y);
    put_f32(out, 52, minimum.z);
    put_f32(out, 56, maximum.x);
    put_f32(out, 60, maximum.y);
    put_f32(out, 64, maximum.z);
    put_u32(out, 68, static_cast<std::uint32_t>(vertex_offset));
    put_u32(out, 72, 12);
    put_u32(out, 76, static_cast<std::uint32_t>(triangle_offset));
    put_u32(out, 80, triangle_size);
    put_u32(out, 84, static_cast<std::uint32_t>(cell_offset));
    put_u32(out, 88, cell_size_bytes);
    put_u32(out, 92, static_cast<std::uint32_t>(reference_offset));
    put_u32(out, 96, 2);
    put_u32(out, 100, fnv1a("F_SP110/R02_00.arc/room.kcl"));
    for (std::uint32_t index = 0; index < triangles.size(); ++index) {
        const CollisionTriangle& triangle = triangles[index];
        for (std::uint32_t point = 0; point < 3; ++point) {
            const std::size_t at =
                vertex_offset + (index * 3 + point) * 12;
            put_f32(out, at, triangle.point[point].x);
            put_f32(out, at + 4, triangle.point[point].y);
            put_f32(out, at + 8, triangle.point[point].z);
        }
        const std::size_t at = triangle_offset + index * triangle_size;
        put_u16(out, at, static_cast<std::uint16_t>(index * 3));
        put_u16(out, at + 2, static_cast<std::uint16_t>(index * 3 + 1));
        put_u16(out, at + 4, static_cast<std::uint16_t>(index * 3 + 2));
        put_u16(out, at + 6, triangle.attribute);
        put_f32(out, at + 8, triangle.normal.x);
        put_f32(out, at + 12, triangle.normal.y);
        put_f32(out, at + 16, triangle.normal.z);
        put_f32(
            out, at + 20,
            -room_dot(triangle.normal, triangle.point[0]));
    }
    std::uint32_t reference_cursor = 0;
    for (std::uint32_t index = 0; index < cells.size(); ++index) {
        const std::size_t at = cell_offset + index * cell_size_bytes;
        put_u32(out, at, reference_cursor);
        put_u32(out, at + 4, static_cast<std::uint32_t>(cells[index].size()));
        put_u32(out, at + 8, index % cells_x);
        put_u32(out, at + 12, index / cells_x);
        for (std::uint16_t triangle : cells[index]) {
            put_u16(
                out, reference_offset + reference_cursor * 2, triangle);
            ++reference_cursor;
        }
    }
    put_u32(out, 12, 0);
    put_u32(out, 12, crc32(out));
    return out;
}

std::vector<Byte> serialize_dpcl(
    const Byte* collision, std::uint32_t collision_size) {
    return serialize_collision_triangles(
        decode_room_kcl(collision, collision_size));
}

std::vector<Byte> serialize_movebg_dpcl(
    const Byte* collision, std::uint32_t collision_size) {
    return serialize_collision_triangles(
        decode_movebg_dzb(collision, collision_size));
}

struct ScenePlacement {
    std::array<char, 8> name{};
    std::uint32_t parameters = 0;
    Vec position = {};
    std::array<std::int16_t, 3> rotation{};
    Vec scale = {1.0f, 1.0f, 1.0f};
    std::uint32_t chunk_type = 0;
    std::uint16_t source_index = 0;
    std::uint16_t process_id = 0xffff;
};

struct SceneExit {
    std::array<char, 8> destination_stage{};
    std::uint16_t source_index = 0;
    std::int8_t destination_room = -1;
    std::int8_t destination_layer = -1;
    std::uint8_t destination_start = 0xff;
    std::uint8_t wipe = 0;
    std::uint8_t packed_a = 0;
    std::uint8_t packed_b = 0;
    std::uint16_t trigger_index = 0xffff;
    std::uint16_t return_exit_index = 0xffff;
};

struct SceneSource {
    ScenePlacement spawn;
    std::vector<ScenePlacement> spawns;
    std::vector<SceneExit> exits;
    std::vector<ScenePlacement> triggers;
    std::vector<ScenePlacement> actors;
    bool spawn_valid = false;
};

struct DzsChunk {
    const Byte* bytes;
    std::uint32_t count;
    std::uint32_t stride;
};

DzsChunk dzs_chunk(
    const Byte* bytes,
    std::uint32_t size,
    const char tag[4],
    std::uint32_t stride,
    bool required = true) {
    if (bytes == nullptr || size < 4) {
        if (!required) {
            return {};
        }
        throw std::runtime_error("DPSC environment source missing");
    }
    const std::uint32_t chunks = read_be32_room(bytes);
    if (chunks > 128 || chunks > (size - 4) / 12) {
        throw std::runtime_error("DPSC environment chunk table invalid");
    }
    for (std::uint32_t index = 0; index < chunks; ++index) {
        const Byte* node = bytes + 4 + index * 12;
        if (std::memcmp(node, tag, 4) != 0) {
            continue;
        }
        const std::uint32_t count = read_be32_room(node + 4);
        const std::uint32_t offset = read_be32_room(node + 8);
        if (stride == 0 || offset > size ||
            count > (size - offset) / stride) {
            throw std::runtime_error("DPSC environment chunk range invalid");
        }
        return {bytes + offset, count, stride};
    }
    if (required) {
        throw std::runtime_error(
            std::string("DPSC environment chunk missing: ") +
            std::string(tag, 4));
    }
    return {};
}

std::uint32_t rgba(const Byte* color) {
    return 0xff000000u |
           (static_cast<std::uint32_t>(color[0]) << 16) |
           (static_cast<std::uint32_t>(color[1]) << 8) |
           static_cast<std::uint32_t>(color[2]);
}

struct EnvironmentSource {
    std::uint32_t stage_hash;
    std::uint32_t room_index;
    std::uint16_t environment_id;
    std::uint8_t pattern;
    std::uint8_t schedule_slot;
    std::uint16_t pselect_id;
    std::uint16_t palette_id;
    std::uint32_t ambient_room;
    std::uint32_t ambient_actor;
    std::uint32_t key_color;
    std::uint32_t fog_color;
    std::uint32_t clear_color;
    std::uint32_t local_color;
    Vec key_direction;
    Vec local_position;
    float local_power;
    float fog_near;
    float fog_far;
    float shadow_density;
    Vec shadow_direction;
    float transition_rate;
    std::uint32_t local_count;
    std::uint32_t source_counts;
};

EnvironmentSource decode_environment_source(
    const Byte* stage,
    std::uint32_t stage_size,
    const Byte* placement,
    std::uint32_t placement_size,
    const SceneSource& scene) {
    const char* stage_id =
        environment_or("DUSKLIGHT_ROOM_STAGE", "F_SP110");
    const std::uint32_t room_index = static_cast<std::uint32_t>(
        std::strtoul(
            environment_or("DUSKLIGHT_ROOM_INDEX", "2"), nullptr, 10));
    if (room_index > 63) {
        throw std::runtime_error("DPSC environment room invalid");
    }
    const DzsChunk palettes =
        dzs_chunk(stage, stage_size, "PAL0", 0x34);
    const DzsChunk selects =
        dzs_chunk(stage, stage_size, "Col0", 0x0c);
    const DzsChunk environments =
        dzs_chunk(stage, stage_size, "Env0", 0x41);
    const DzsChunk key_lights =
        dzs_chunk(placement, placement_size, "LGT0", 0x20, false);
    const DzsChunk local_lights =
        dzs_chunk(placement, placement_size, "LGHT", 0x1c, false);
    const std::uint32_t environment_id = room_index;
    if (environment_id >= environments.count) {
        throw std::runtime_error("DPSC environment room record absent");
    }
    std::uint8_t pattern = 0;
    if (std::strcmp(stage_id, "D_MN10") == 0) {
        pattern = room_index == 2 ? 14 :
            (room_index == 3 || room_index == 8 ? 6 : 8);
    }
    const Byte* environment =
        environments.bytes + environment_id * environments.stride;
    const std::uint16_t pselect_id = environment[pattern];
    if (pselect_id >= selects.count) {
        throw std::runtime_error("DPSC environment pselect invalid");
    }
    const Byte* select =
        selects.bytes + pselect_id * selects.stride;
    constexpr std::uint8_t schedule_slot = 2;
    const std::uint16_t palette_id = select[schedule_slot];
    if (palette_id >= palettes.count) {
        throw std::runtime_error("DPSC environment palette invalid");
    }
    const Byte* palette =
        palettes.bytes + palette_id * palettes.stride;
    const float fog_near = read_be_float_room(palette + 0x24);
    const float fog_far = read_be_float_room(palette + 0x28);
    if (!std::isfinite(fog_near) || !std::isfinite(fog_far) ||
        fog_far <= fog_near) {
        throw std::runtime_error("DPSC environment fog invalid");
    }
    const Byte* selected_key = nullptr;
    float closest = INFINITY;
    for (std::uint32_t index = 0; index < key_lights.count; ++index) {
        const Byte* light =
            key_lights.bytes + index * key_lights.stride;
        const Vec position = {
            read_be_float_room(light),
            read_be_float_room(light + 4),
            read_be_float_room(light + 8)};
        const float dx = position.x - scene.spawn.position.x;
        const float dy = position.y - scene.spawn.position.y;
        const float dz = position.z - scene.spawn.position.z;
        const float distance = dx * dx + dy * dy + dz * dz;
        if (!std::isfinite(distance)) {
            throw std::runtime_error(
                "DPSC environment key light invalid");
        }
        if (distance < closest) {
            closest = distance;
            selected_key = light;
        }
    }
    Vec key_position = {
        scene.spawn.position.x,
        scene.spawn.position.y + 1000.0f,
        scene.spawn.position.z + 1000.0f};
    if (selected_key != nullptr) {
        key_position = {
            read_be_float_room(selected_key),
            read_be_float_room(selected_key + 4),
            read_be_float_room(selected_key + 8)};
    }
    const Byte* selected_local = nullptr;
    closest = INFINITY;
    for (std::uint32_t index = 0; index < local_lights.count; ++index) {
        const Byte* light =
            local_lights.bytes + index * local_lights.stride;
        const Vec position = {
            read_be_float_room(light),
            read_be_float_room(light + 4),
            read_be_float_room(light + 8)};
        const float power = read_be_float_room(light + 12);
        const float dx = position.x - scene.spawn.position.x;
        const float dy = position.y - scene.spawn.position.y;
        const float dz = position.z - scene.spawn.position.z;
        const float distance = dx * dx + dy * dy + dz * dz;
        if (!std::isfinite(distance) || !std::isfinite(power)) {
            throw std::runtime_error(
                "DPSC environment local light invalid");
        }
        if (distance < closest) {
            closest = distance;
            selected_local = light;
        }
    }
    Vec local_position = key_position;
    float local_power = 0.0f;
    std::uint32_t local_color = rgba(palette + 0x0f);
    if (selected_local != nullptr) {
        local_position = {
            read_be_float_room(selected_local),
            read_be_float_room(selected_local + 4),
            read_be_float_room(selected_local + 8)};
        local_power = std::max(
            0.0f, read_be_float_room(selected_local + 12) * 200.0f);
        local_color = rgba(selected_local + 0x18);
    }
    Vec direction = {
        key_position.x - scene.spawn.position.x,
        key_position.y - scene.spawn.position.y,
        key_position.z - scene.spawn.position.z};
    float length = std::sqrt(
        direction.x * direction.x +
        direction.y * direction.y +
        direction.z * direction.z);
    if (!std::isfinite(length) || length < 0.001f) {
        direction = {0.0f, 1.0f, 0.0f};
        length = 1.0f;
    }
    direction = {
        direction.x / length,
        direction.y / length,
        direction.z / length};
    Vec shadow = {-direction.x, 0.0f, -direction.z};
    length = std::sqrt(shadow.x * shadow.x + shadow.z * shadow.z);
    if (length < 0.001f) {
        shadow = {0.0f, 0.0f, -1.0f};
    } else {
        shadow.x /= length;
        shadow.z /= length;
    }
    EnvironmentSource result = {};
    result.stage_hash = fnv1a(stage_id);
    result.room_index = room_index;
    result.environment_id = static_cast<std::uint16_t>(environment_id);
    result.pattern = pattern;
    result.schedule_slot = schedule_slot;
    result.pselect_id = pselect_id;
    result.palette_id = palette_id;
    result.ambient_room = rgba(palette + 3);
    result.ambient_actor = rgba(palette);
    result.key_color = rgba(palette + 0x0f);
    result.fog_color = rgba(palette + 0x21);
    result.clear_color = result.ambient_room;
    result.local_color = local_color;
    result.key_direction = direction;
    result.local_position = local_position;
    result.local_power = local_power;
    result.fog_near = fog_near;
    result.fog_far = fog_far;
    result.shadow_density =
        static_cast<float>(palette[0x2e]) / 255.0f;
    result.shadow_direction = shadow;
    result.transition_rate = read_be_float_room(select + 8);
    result.local_count = local_lights.count;
    result.source_counts =
        (palettes.count & 0xffu) |
        ((selects.count & 0xffu) << 8) |
        ((environments.count & 0xffu) << 16) |
        ((key_lights.count & 0x0fu) << 24) |
        ((local_lights.count & 0x0fu) << 28);
    return result;
}

SceneSource decode_room_placement(
    const Byte* bytes, std::uint32_t size) {
    if (bytes == nullptr || size < 4) {
        throw std::runtime_error("DPSC DZR truncated");
    }
    const std::uint32_t chunk_count = read_be32_room(bytes);
    if (chunk_count > 128 || chunk_count > (size - 4) / 12) {
        throw std::runtime_error("DPSC DZR chunk table invalid");
    }
    SceneSource result;
    std::uint16_t actor_source_index = 0;
    for (std::uint32_t chunk = 0; chunk < chunk_count; ++chunk) {
        const Byte* node = bytes + 4 + chunk * 12;
        const std::string tag(
            reinterpret_cast<const char*>(node), 4);
        const std::uint32_t count = read_be32_room(node + 4);
        const std::uint32_t offset = read_be32_room(node + 8);
        const bool player =
            tag == "PLYR" || tag.compare(0, 3, "PLY") == 0;
        const bool actor =
            tag == "ACTR" || tag == "TGOB" || tag == "TRES" ||
            tag == "TGSC" || tag == "SCOB" || tag == "TGDR" ||
            tag.compare(0, 3, "ACT") == 0 ||
            tag.compare(0, 3, "TGO") == 0 ||
            tag.compare(0, 3, "TRE") == 0 ||
            tag.compare(0, 3, "SCO") == 0;
        if (player || actor) {
            const std::uint32_t stride =
                tag == "TGSC" || tag == "TGDR" ||
                tag.compare(0, 3, "SCO") == 0
                    ? 36 : 32;
            if (offset > size || count > (size - offset) / stride) {
                throw std::runtime_error("DPSC DZR actor range invalid");
            }
            for (std::uint32_t index = 0; index < count; ++index) {
                const Byte* entry = bytes + offset + index * stride;
                ScenePlacement placement;
                std::memcpy(placement.name.data(), entry, 8);
                placement.parameters = read_be32_room(entry + 8);
                placement.position = {
                    read_be_float_room(entry + 12),
                    read_be_float_room(entry + 16),
                    read_be_float_room(entry + 20),
                };
                placement.rotation = {
                    static_cast<std::int16_t>(read_be16(entry + 24)),
                    static_cast<std::int16_t>(read_be16(entry + 26)),
                    static_cast<std::int16_t>(read_be16(entry + 28)),
                };
                if (stride == 36) {
                    placement.scale = {
                        static_cast<float>(entry[32]) / 10.0f,
                        static_cast<float>(entry[33]) / 10.0f,
                        static_cast<float>(entry[34]) / 10.0f,
                    };
                }
                placement.chunk_type = fnv1a(tag.c_str());
                if (!std::isfinite(placement.position.x) ||
                    !std::isfinite(placement.position.y) ||
                    !std::isfinite(placement.position.z)) {
                    throw std::runtime_error("DPSC placement non-finite");
                }
                if (player) {
                    placement.source_index =
                        static_cast<std::uint16_t>(result.spawns.size());
                    result.spawns.push_back(placement);
                    if (!result.spawn_valid) {
                        result.spawn = placement;
                        result.spawn_valid = true;
                    }
                } else if (actor) {
                    placement.source_index = actor_source_index++;
                    const std::string name(
                        placement.name.data(),
                        strnlen(placement.name.data(), placement.name.size()));
                    if (name == "geyser") {
                        placement.process_id = 0x0167;
                    } else if (name == "CamArea") {
                        placement.process_id = 0x02cf;
                    } else if (name == "Grass" || name == "flower" ||
                               name == "flwr7" || name == "flwr17") {
                        placement.process_id = 0x0310;
                    } else if (name == "Fish") {
                        placement.process_id = 0x0136;
                    } else if (name == "Yousei") {
                        placement.process_id = 0x013f;
                    } else if (name == "Obj_Uma") {
                        placement.process_id = 0x014d;
                    } else if (name == "SwAreaS" || name == "SwAreaC") {
                        placement.process_id = 0x0225;
                    } else if (name == "spring") {
                        placement.process_id = 0x01ad;
                    } else if (name == "Savmem") {
                        placement.process_id = 0x02b8;
                    } else if (name == "FSeirei") {
                        placement.process_id = 0x0252;
                    } else if (name == "Mhint") {
                        placement.process_id = 0x02c1;
                    } else if (name == "Digpl") {
                        placement.process_id = 0x0053;
                    } else if (name == "CamChg" || name == "CamAreC") {
                        placement.process_id = 0x02cf;
                    } else if (name == "TagEv" || name == "TagEvC") {
                        placement.process_id = 0x02d1;
                    } else if (name == "atkItem") {
                        placement.process_id = 0x01aa;
                    } else if (name == "Horse") {
                        placement.process_id = 0x00ee;
                    } else if (name == "Seirei") {
                        placement.process_id = 0x026a;
                    } else if (name == "Bd") {
                        placement.process_id = 0x0300;
                    } else if (name == "scnChg") {
                        placement.process_id = 0x030c;
                        result.triggers.push_back(placement);
                    } else if (name == "L4hmato") {
                        placement.process_id = 0x009f;
                    } else if (name == "L4Pgate") {
                        placement.process_id = 0x009d;
                    } else if (name == "spnGear") {
                        placement.process_id = 0x0183;
                    } else if (name == "swspin") {
                        placement.process_id = 0x00b3;
                    } else if (name == "tboxB0") {
                        placement.process_id = 0x00fb;
                    }
                    result.actors.push_back(placement);
                }
            }
        } else if (tag == "SCLS") {
            if (offset > size || count > (size - offset) / 13) {
                throw std::runtime_error("DPSC SCLS range invalid");
            }
            for (std::uint32_t index = 0; index < count; ++index) {
                const Byte* entry = bytes + offset + index * 13;
                SceneExit exit;
                std::memcpy(exit.destination_stage.data(), entry, 8);
                exit.source_index =
                    static_cast<std::uint16_t>(result.exits.size());
                exit.destination_start = entry[8];
                exit.destination_room =
                    static_cast<std::int8_t>(entry[9]);
                exit.destination_layer =
                    (entry[11] & 0x0f) == 0x0f
                        ? static_cast<std::int8_t>(-1)
                        : static_cast<std::int8_t>(entry[11] & 0x0f);
                exit.packed_a = entry[10];
                exit.packed_b = entry[11];
                exit.wipe = entry[12];
                result.exits.push_back(exit);
            }
        }
    }
    for (std::uint16_t trigger_index = 0;
         trigger_index < result.triggers.size(); ++trigger_index) {
        const std::uint8_t exit_index =
            static_cast<std::uint8_t>(
                result.triggers[trigger_index].parameters);
        if (exit_index < result.exits.size()) {
            result.exits[exit_index].trigger_index = trigger_index;
        }
    }
    const char* selected_exit_value =
        std::getenv("DUSKLIGHT_SELECTED_EXIT");
    const char* return_exit_value =
        std::getenv("DUSKLIGHT_RETURN_EXIT");
    if (selected_exit_value != nullptr && return_exit_value != nullptr) {
        const unsigned long selected_exit =
            std::strtoul(selected_exit_value, nullptr, 10);
        const unsigned long return_exit =
            std::strtoul(return_exit_value, nullptr, 10);
        if (selected_exit >= result.exits.size() || return_exit > 0xffff) {
            throw std::runtime_error("DPSC return exit mapping invalid");
        }
        result.exits[selected_exit].return_exit_index =
            static_cast<std::uint16_t>(return_exit);
    }
    if (!result.spawn_valid || result.spawns.empty()) {
        throw std::runtime_error("DPSC room has no player spawn");
    }
    return result;
}

bool collision_floor(
    const std::vector<CollisionTriangle>& triangles,
    const Vec& position,
    float* height,
    Vec* normal) {
    bool found = false;
    float best = -INFINITY;
    for (const CollisionTriangle& triangle : triangles) {
        if (triangle.normal.y < 0.643f) {
            continue;
        }
        const Vec& a = triangle.point[0];
        const Vec& b = triangle.point[1];
        const Vec& c = triangle.point[2];
        const float denominator =
            (b.z - c.z) * (a.x - c.x) +
            (c.x - b.x) * (a.z - c.z);
        if (std::fabs(denominator) < 1.0e-6f) {
            continue;
        }
        const float wa =
            ((b.z - c.z) * (position.x - c.x) +
             (c.x - b.x) * (position.z - c.z)) / denominator;
        const float wb =
            ((c.z - a.z) * (position.x - c.x) +
             (a.x - c.x) * (position.z - c.z)) / denominator;
        const float wc = 1.0f - wa - wb;
        if (wa < -1.0e-4f || wb < -1.0e-4f || wc < -1.0e-4f) {
            continue;
        }
        const float candidate = wa * a.y + wb * b.y + wc * c.y;
        if (candidate <= position.y + 200.0f && candidate > best) {
            best = candidate;
            *normal = triangle.normal;
            found = true;
        }
    }
    if (found) {
        *height = best;
    }
    return found;
}

std::vector<Vec> choose_scene_rubies(
    const std::vector<CollisionTriangle>& triangles,
    const Vec& spawn) {
    std::vector<Vec> candidates;
    for (const CollisionTriangle& triangle : triangles) {
        if (triangle.normal.y < 0.75f) {
            continue;
        }
        Vec point = {
            (triangle.point[0].x + triangle.point[1].x +
             triangle.point[2].x) / 3.0f,
            (triangle.point[0].y + triangle.point[1].y +
             triangle.point[2].y) / 3.0f,
            (triangle.point[0].z + triangle.point[1].z +
             triangle.point[2].z) / 3.0f,
        };
        const float dx = point.x - spawn.x;
        const float dz = point.z - spawn.z;
        if (dx * dx + dz * dz > 80.0f * 80.0f) {
            candidates.push_back(point);
        }
    }
    std::stable_sort(
        candidates.begin(), candidates.end(),
        [&](const Vec& left, const Vec& right) {
            const float ldx = left.x - spawn.x;
            const float ldz = left.z - spawn.z;
            const float rdx = right.x - spawn.x;
            const float rdz = right.z - spawn.z;
            return ldx * ldx + ldz * ldz <
                   rdx * rdx + rdz * rdz;
        });
    std::vector<Vec> selected;
    for (const Vec& candidate : candidates) {
        bool separated = true;
        for (const Vec& prior : selected) {
            const float dx = candidate.x - prior.x;
            const float dz = candidate.z - prior.z;
            separated = separated && dx * dx + dz * dz >= 180.0f * 180.0f;
        }
        if (separated) {
            selected.push_back({
                candidate.x, candidate.y + 35.0f, candidate.z});
            if (selected.size() == 5) {
                break;
            }
        }
    }
    if (selected.size() != 5) {
        throw std::runtime_error("DPSC unable to place five rubies");
    }
    return selected;
}

std::vector<Byte> serialize_dpsc(
    const Byte* collision,
    std::uint32_t collision_size,
    const Byte* placement,
    std::uint32_t placement_size,
    const Byte* stage,
    std::uint32_t stage_size) {
    const std::vector<CollisionTriangle> triangles =
        decode_room_kcl(collision, collision_size);
    SceneSource scene =
        decode_room_placement(placement, placement_size);
    const char* version_value = std::getenv("DUSKLIGHT_DPSC_VERSION");
    const std::uint16_t version =
        version_value != nullptr && std::strcmp(version_value, "4") == 0
            ? 4
        : version_value != nullptr && std::strcmp(version_value, "3") == 0
            ? 3
            : version_value != nullptr && std::strcmp(version_value, "2") == 0
                ? 2 : 1;
    if (version < 3 && scene.actors.size() != 3) {
        throw std::runtime_error("DPSC v1/v2 selected room placement mismatch");
    }
    const char* initial_start_value =
        std::getenv("DUSKLIGHT_ROOM_INITIAL_START");
    if (initial_start_value != nullptr) {
        const unsigned long initial_start =
            std::strtoul(initial_start_value, nullptr, 10);
        const auto selected = std::find_if(
            scene.spawns.begin(), scene.spawns.end(),
            [&](const ScenePlacement& candidate) {
                return static_cast<std::uint8_t>(candidate.rotation[2]) ==
                    initial_start;
            });
        if (initial_start > 255 || selected == scene.spawns.end()) {
            throw std::runtime_error("DPSC initial start is absent");
        }
        scene.spawn = *selected;
    }
    float spawn_floor = 0.0f;
    Vec spawn_normal = {};
    if (!collision_floor(
            triangles, scene.spawn.position, &spawn_floor, &spawn_normal) ||
        std::fabs(spawn_floor - scene.spawn.position.y) > 35.0f) {
        throw std::runtime_error("DPSC spawn has no valid floor");
    }
    const std::vector<Vec> rubies =
        choose_scene_rubies(triangles, scene.spawn.position);
    Vec minimum = {INFINITY, INFINITY, INFINITY};
    Vec maximum = {-INFINITY, -INFINITY, -INFINITY};
    for (const CollisionTriangle& triangle : triangles) {
        for (const Vec& point : triangle.point) {
            minimum.x = std::min(minimum.x, point.x);
            minimum.y = std::min(minimum.y, point.y);
            minimum.z = std::min(minimum.z, point.z);
            maximum.x = std::max(maximum.x, point.x);
            maximum.y = std::max(maximum.y, point.y);
            maximum.z = std::max(maximum.z, point.z);
        }
    }
    constexpr std::size_t header_size = 256;
    constexpr std::size_t placement_size_bytes = 32;
    constexpr std::size_t actor_v2_size_bytes = 64;
    constexpr std::size_t exit_v3_size_bytes = 64;
    constexpr std::size_t trigger_v3_size_bytes = 80;
    constexpr std::size_t spawn_v3_size_bytes = 64;
    constexpr std::size_t environment_v4_size_bytes = 128;
    const std::size_t actor_size_bytes =
        version >= 2 ? actor_v2_size_bytes : placement_size_bytes;
    const std::size_t ruby_offset = header_size;
    const std::size_t legacy_exit_offset =
        align16(ruby_offset + rubies.size() * 16);
    const std::size_t actor_offset =
        align16(
            legacy_exit_offset +
            scene.exits.size() * placement_size_bytes);
    const std::size_t exit_v3_offset =
        align16(actor_offset + scene.actors.size() * actor_size_bytes);
    const std::size_t trigger_v3_offset =
        align16(
            exit_v3_offset +
            (version >= 3
                ? scene.exits.size() * exit_v3_size_bytes : 0));
    const std::size_t spawn_v3_offset =
        align16(
            trigger_v3_offset +
            (version >= 3
                ? scene.triggers.size() * trigger_v3_size_bytes : 0));
    const std::size_t environment_v4_offset = align16(
        spawn_v3_offset +
        (version >= 3
            ? scene.spawns.size() * spawn_v3_size_bytes : 0));
    const std::size_t total = align16(
        environment_v4_offset +
        (version >= 4 ? environment_v4_size_bytes : 0));
    std::vector<Byte> out(total, 0);
    std::memcpy(out.data(), "DPSC", 4);
    put_u16(out, 4, version);
    put_u16(out, 6, header_size);
    put_u32(out, 8, static_cast<std::uint32_t>(out.size()));
    const char* stage_id =
        environment_or("DUSKLIGHT_ROOM_STAGE", "F_SP110");
    const std::uint32_t room_index = static_cast<std::uint32_t>(
        std::strtoul(
            environment_or("DUSKLIGHT_ROOM_INDEX", "2"), nullptr, 10));
    put_u32(out, 16, fnv1a(stage_id));
    put_u32(out, 20, room_index);
    put_u32(out, 24, 0);
    put_u32(out, 28, fnv1a("room.dprm"));
    put_u32(out, 32, fnv1a("room.dptx"));
    put_u32(out, 36, fnv1a("room.dpcl"));
    put_f32(out, 40, scene.spawn.position.x);
    put_f32(out, 44, scene.spawn.position.y);
    put_f32(out, 48, scene.spawn.position.z);
    put_u16(out, 52, static_cast<std::uint16_t>(scene.spawn.rotation[1]));
    put_u16(out, 54, 0);
    put_u32(out, 56, scene.spawn.parameters);
    put_u32(out, 60, 1);
    put_f32(out, 64, scene.spawn.position.x);
    put_f32(out, 68, spawn_floor + 220.0f);
    put_f32(out, 72, scene.spawn.position.z + 420.0f);
    put_f32(out, 76, scene.spawn.position.x);
    put_f32(out, 80, spawn_floor + 90.0f);
    put_f32(out, 84, scene.spawn.position.z);
    put_u32(out, 88, 0xffb8a078u);
    put_u32(out, 92, 0xff806848u);
    put_f32(out, 96, minimum.x);
    put_f32(out, 100, minimum.y);
    put_f32(out, 104, minimum.z);
    put_f32(out, 108, maximum.x);
    put_f32(out, 112, maximum.y);
    put_f32(out, 116, maximum.z);
    put_u32(out, 120, static_cast<std::uint32_t>(rubies.size()));
    put_u32(out, 124, static_cast<std::uint32_t>(ruby_offset));
    put_u32(out, 128, static_cast<std::uint32_t>(scene.exits.size()));
    put_u32(
        out, 132, static_cast<std::uint32_t>(legacy_exit_offset));
    put_u32(out, 136, static_cast<std::uint32_t>(scene.actors.size()));
    put_u32(out, 140, static_cast<std::uint32_t>(actor_offset));
    put_f32(out, 144, scene.spawn.position.x + 120.0f);
    put_f32(out, 148, spawn_floor);
    put_f32(out, 152, scene.spawn.position.z - 120.0f);
    put_u32(out, 156, 0);
    put_u32(out, 160, 0);
    put_u32(out, 164, 0);
    if (version >= 2) {
        put_u32(out, 168, actor_v2_size_bytes);
        std::uint32_t supported_actors = 0;
        for (const ScenePlacement& actor : scene.actors) {
            supported_actors +=
                actor.process_id == 0x0167 ||
                actor.process_id == 0x009f ||
                actor.process_id == 0x009d ||
                actor.process_id == 0x00b3 ||
                actor.process_id == 0x0183 ||
                (std::string(stage_id) == "F_SP108" &&
                 (actor.source_index == 0 ||
                  (actor.source_index >= 5 &&
                   actor.source_index <= 12))) ? 1 : 0;
        }
        put_u32(out, 172, supported_actors);
    }
    std::memcpy(
        out.data() + 176, stage_id,
        std::min<std::size_t>(std::strlen(stage_id), 8));
    const char* room_name =
        environment_or("DUSKLIGHT_ROOM_NAME", "Death Mountain Trail");
    std::memcpy(
        out.data() + 192, room_name,
        std::min<std::size_t>(std::strlen(room_name), 16));
    if (version >= 3) {
        put_u32(out, 156, exit_v3_size_bytes);
        put_u32(out, 160, trigger_v3_size_bytes);
        put_u32(out, 164, spawn_v3_size_bytes);
        put_u32(out, 208, static_cast<std::uint32_t>(exit_v3_offset));
        put_u32(out, 212, static_cast<std::uint32_t>(scene.exits.size()));
        put_u32(out, 216, static_cast<std::uint32_t>(trigger_v3_offset));
        put_u32(out, 220, static_cast<std::uint32_t>(scene.triggers.size()));
        put_u32(out, 224, static_cast<std::uint32_t>(spawn_v3_offset));
        put_u32(out, 228, static_cast<std::uint32_t>(scene.spawns.size()));
        put_u32(out, 232, static_cast<std::uint32_t>(scene.exits.size()));
        put_u32(
            out, 236,
            fnv1a(
                (std::string(stage_id) + "/R" +
                 environment_or("DUSKLIGHT_ROOM_INDEX", "2")).c_str()));
        put_u32(out, 240, fnv1a(stage_id));
        put_u32(out, 244, 0x00000007u);
    }
    if (version >= 4) {
        put_u32(
            out, 248,
            static_cast<std::uint32_t>(environment_v4_offset));
        put_u32(out, 252, 1);
    }
    for (std::uint32_t index = 0; index < rubies.size(); ++index) {
        const std::size_t at = ruby_offset + index * 16;
        put_f32(out, at, rubies[index].x);
        put_f32(out, at + 4, rubies[index].y);
        put_f32(out, at + 8, rubies[index].z);
        put_u32(out, at + 12, index);
    }
    auto write_placement = [&](std::size_t at, const ScenePlacement& item) {
        std::memcpy(out.data() + at, item.name.data(), 8);
        put_u32(out, at + 8, item.parameters);
        put_f32(out, at + 12, item.position.x);
        put_f32(out, at + 16, item.position.y);
        put_f32(out, at + 20, item.position.z);
        put_u16(
            out, at + 24, static_cast<std::uint16_t>(item.rotation[1]));
        put_u32(out, at + 28, item.chunk_type);
    };
    for (std::uint32_t index = 0; index < scene.exits.size(); ++index) {
        ScenePlacement legacy;
        legacy.name = scene.exits[index].destination_stage;
        legacy.parameters =
            (static_cast<std::uint32_t>(
                 scene.exits[index].destination_start) << 24) |
            (static_cast<std::uint32_t>(
                 static_cast<std::uint8_t>(
                     scene.exits[index].destination_room)) << 16) |
            (static_cast<std::uint32_t>(
                 scene.exits[index].packed_a) << 8) |
            scene.exits[index].packed_b;
        legacy.chunk_type = fnv1a("SCLS");
        write_placement(
            legacy_exit_offset + index * placement_size_bytes,
            legacy);
    }
    for (std::uint32_t index = 0; index < scene.actors.size(); ++index) {
        const ScenePlacement& actor = scene.actors[index];
        const std::size_t at = actor_offset + index * actor_size_bytes;
        if (version == 1) {
            write_placement(at, actor);
            continue;
        }
        std::memcpy(out.data() + at, actor.name.data(), actor.name.size());
        put_u32(
            out, at + 8,
            fnv1a(std::string(
                actor.name.data(),
                strnlen(actor.name.data(), actor.name.size())).c_str()));
        put_u16(out, at + 12, actor.process_id);
        put_u16(out, at + 14, 1);
        put_u32(out, at + 16, actor.parameters);
        put_f32(out, at + 20, actor.position.x);
        put_f32(out, at + 24, actor.position.y);
        put_f32(out, at + 28, actor.position.z);
        put_u16(out, at + 32, static_cast<std::uint16_t>(actor.rotation[0]));
        put_u16(out, at + 34, static_cast<std::uint16_t>(actor.rotation[1]));
        put_u16(out, at + 36, static_cast<std::uint16_t>(actor.rotation[2]));
        out[at + 38] =
            static_cast<Byte>((actor.rotation[0] >> 8) & 0x0f);
        out[at + 39] = static_cast<Byte>(room_index);
        out[at + 40] = 0;
        out[at + 41] =
            actor.process_id == 0x0167 ||
            actor.process_id == 0x009f ||
            actor.process_id == 0x009d ||
            actor.process_id == 0x00b3 ||
            actor.process_id == 0x0183 ||
            (std::string(stage_id) == "F_SP108" &&
             (actor.source_index == 0 ||
              (actor.source_index >= 5 &&
               actor.source_index <= 12))) ? 1 : 0;
        put_u16(out, at + 42, actor.source_index);
        put_f32(out, at + 44, actor.scale.x);
        put_f32(out, at + 48, actor.scale.y);
        put_f32(out, at + 52, actor.scale.z);
        put_u32(out, at + 56, actor.chunk_type);
        put_u32(out, at + 60, 0);
    }
    if (version >= 3) {
        for (std::uint32_t index = 0; index < scene.exits.size(); ++index) {
            const SceneExit& exit = scene.exits[index];
            const std::size_t at =
                exit_v3_offset + index * exit_v3_size_bytes;
            put_u16(out, at, exit.source_index);
            out[at + 2] =
                static_cast<Byte>(exit.destination_room);
            out[at + 3] =
                static_cast<Byte>(exit.destination_layer);
            out[at + 4] = exit.destination_start;
            out[at + 5] = exit.wipe;
            put_u16(
                out, at + 6,
                static_cast<std::uint16_t>(
                    exit.packed_a |
                    (static_cast<std::uint16_t>(exit.packed_b) << 8)));
            put_u16(out, at + 8, exit.trigger_index);
            put_u16(out, at + 10, exit.return_exit_index);
            std::memcpy(
                out.data() + at + 12,
                exit.destination_stage.data(),
                exit.destination_stage.size());
            put_u32(
                out, at + 20,
                fnv1a(std::string(
                    exit.destination_stage.data(),
                    strnlen(
                        exit.destination_stage.data(),
                        exit.destination_stage.size())).c_str()));
        }
        for (std::uint32_t index = 0;
             index < scene.triggers.size(); ++index) {
            const ScenePlacement& trigger = scene.triggers[index];
            const std::size_t at =
                trigger_v3_offset + index * trigger_v3_size_bytes;
            put_u16(out, at, 1);
            put_u16(out, at + 2, trigger.process_id);
            put_u32(out, at + 4, fnv1a("scnChg"));
            put_u32(out, at + 8, trigger.parameters);
            put_f32(out, at + 12, trigger.position.x);
            put_f32(out, at + 16, trigger.position.y);
            put_f32(out, at + 20, trigger.position.z);
            put_u16(
                out, at + 24,
                static_cast<std::uint16_t>(trigger.rotation[0]));
            put_u16(
                out, at + 26,
                static_cast<std::uint16_t>(trigger.rotation[1]));
            put_u16(
                out, at + 28,
                static_cast<std::uint16_t>(trigger.rotation[2]));
            put_u16(out, at + 30, 1);
            put_f32(out, at + 32, trigger.scale.x);
            put_f32(out, at + 36, trigger.scale.y);
            put_f32(out, at + 40, trigger.scale.z);
            put_f32(out, at + 44, trigger.scale.x * 75.0f);
            put_f32(out, at + 48, trigger.scale.y * 150.0f);
            put_f32(out, at + 52, trigger.scale.z * 75.0f);
            out[at + 56] = 1;
            out[at + 57] =
                static_cast<Byte>(trigger.parameters);
            out[at + 58] = 1;
            out[at + 59] = 0;
            put_u32(out, at + 60, 1);
        }
        for (std::uint32_t index = 0;
             index < scene.spawns.size(); ++index) {
            const ScenePlacement& spawn = scene.spawns[index];
            const std::size_t at =
                spawn_v3_offset + index * spawn_v3_size_bytes;
            out[at] = static_cast<Byte>(spawn.rotation[2]);
            out[at + 1] =
                static_cast<Byte>((spawn.parameters >> 24) & 0xff);
            put_u16(out, at + 2, 0);
            put_f32(out, at + 4, spawn.position.x);
            put_f32(out, at + 8, spawn.position.y);
            put_f32(out, at + 12, spawn.position.z);
            put_u16(
                out, at + 16,
                static_cast<std::uint16_t>(spawn.rotation[0]));
            put_u16(
                out, at + 18,
                static_cast<std::uint16_t>(spawn.rotation[1]));
            put_u16(
                out, at + 20,
                static_cast<std::uint16_t>(spawn.rotation[2]));
            out[at + 22] = 0;
            put_u32(out, at + 24, spawn.parameters);
            float floor = 0.0f;
            Vec normal = {};
            const bool floor_valid = collision_floor(
                triangles, spawn.position, &floor, &normal);
            out[at + 28] = floor_valid ? 1 : 0;
            put_f32(out, at + 32, floor);
            put_f32(out, at + 36, normal.x);
            put_f32(out, at + 40, normal.y);
            put_f32(out, at + 44, normal.z);
        }
    }
    if (version >= 4) {
        const EnvironmentSource environment =
            decode_environment_source(
                stage, stage_size, placement, placement_size, scene);
        const std::size_t at = environment_v4_offset;
        std::memcpy(out.data() + at, "DENV", 4);
        put_u16(out, at + 4, 1);
        put_u16(out, at + 6, environment_v4_size_bytes);
        put_u32(out, at + 8, environment.stage_hash);
        put_u32(out, at + 12, environment.room_index);
        put_u16(out, at + 16, environment.environment_id);
        out[at + 18] = environment.pattern;
        out[at + 19] = environment.schedule_slot;
        put_u16(out, at + 20, environment.pselect_id);
        put_u16(out, at + 22, environment.palette_id);
        put_u32(out, at + 24, 0x0000000fu);
        put_u32(out, at + 28, environment.ambient_room);
        put_u32(out, at + 32, environment.ambient_actor);
        put_u32(out, at + 36, environment.key_color);
        put_u32(out, at + 40, environment.fog_color);
        put_u32(out, at + 44, environment.clear_color);
        put_u32(out, at + 48, environment.local_color);
        put_f32(out, at + 52, environment.key_direction.x);
        put_f32(out, at + 56, environment.key_direction.y);
        put_f32(out, at + 60, environment.key_direction.z);
        put_f32(out, at + 64, environment.local_position.x);
        put_f32(out, at + 68, environment.local_position.y);
        put_f32(out, at + 72, environment.local_position.z);
        put_f32(out, at + 76, environment.local_power);
        put_f32(out, at + 80, environment.fog_near);
        put_f32(out, at + 84, environment.fog_far);
        put_f32(out, at + 88, environment.shadow_density);
        put_f32(out, at + 92, environment.shadow_direction.x);
        put_f32(out, at + 96, environment.shadow_direction.y);
        put_f32(out, at + 100, environment.shadow_direction.z);
        put_f32(out, at + 104, environment.transition_rate);
        put_u32(out, at + 108, environment.local_count);
        put_u32(out, at + 112, environment.source_counts);
        put_u32(out, at + 116, fnv1a("Env0>Col0>PAL0"));
        put_u32(out, at + 120, fnv1a("D_MN10:dKy"));
        put_u32(out, at + 124, 0);
        std::cout
            << "ENVIRONMENT_EXPORT_OK"
            << " room=" << environment.room_index
            << " env=" << environment.environment_id
            << " pattern=" << static_cast<unsigned>(environment.pattern)
            << " schedule_slot="
            << static_cast<unsigned>(environment.schedule_slot)
            << " pselect=" << environment.pselect_id
            << " palette=" << environment.palette_id
            << " local_lights=" << environment.local_count
            << " fog_near=" << environment.fog_near
            << " fog_far=" << environment.fog_far
            << " shadow_density=" << environment.shadow_density
            << '\n';
    }
    put_u32(out, 12, 0);
    put_u32(out, 12, crc32(out));
    std::cout << "REAL_ROOM_SCENE"
              << " spawn=" << scene.spawn.position.x << ','
              << spawn_floor << ',' << scene.spawn.position.z
              << " yaw=" << scene.spawn.rotation[1]
              << " dpsc_version=" << version
              << " exits=" << scene.exits.size()
              << " actors=" << scene.actors.size();
    for (const ScenePlacement& actor : scene.actors) {
        std::cout << " actor="
                  << std::string(actor.name.data(), strnlen(actor.name.data(), 8))
                  << '@' << actor.position.x << ','
                  << actor.position.y << ',' << actor.position.z
                  << "/rot=" << actor.rotation[0] << ','
                  << actor.rotation[1] << ',' << actor.rotation[2]
                  << "/scale=" << actor.scale.x << ','
                  << actor.scale.y << ',' << actor.scale.z
                  << "/process=0x" << std::hex << actor.process_id
                  << "/params=0x" << actor.parameters << std::dec;
    }
    std::cout << '\n';
    return out;
}

std::vector<Rgba> resize_pixels(
    const std::vector<Rgba>& source,
    std::uint16_t source_width,
    std::uint16_t source_height,
    std::uint16_t width,
    std::uint16_t height) {
    if (source.size() !=
            static_cast<std::size_t>(source_width) * source_height ||
        width == 0 || height == 0) {
        throw std::runtime_error("startup UI resize arguments invalid");
    }
    std::vector<Rgba> output(
        static_cast<std::size_t>(width) * height);
    for (std::uint32_t y = 0; y < height; ++y) {
        for (std::uint32_t x = 0; x < width; ++x) {
            output[y * width + x] =
                source[
                    (y * source_height / height) * source_width +
                    x * source_width / width];
        }
    }
    return output;
}

UiSourceSprite startup_bti_sprite(
    const HudSourceResource& resource,
    std::uint16_t id,
    std::uint16_t channel) {
    if (resource.bytes.size() < sizeof(ResTIMG)) {
        throw std::runtime_error(
            "startup BTI truncated: " + resource.name);
    }
    const auto& info =
        *reinterpret_cast<const ResTIMG*>(resource.bytes.data());
    UiSourceSprite sprite;
    sprite.id = id;
    sprite.channel = channel;
    sprite.name = resource.name;
    sprite.width = info.width;
    sprite.height = info.height;
    sprite.advance = info.width;
    sprite.pixels = decode_bti(resource.bytes);
    return sprite;
}

std::vector<Byte> serialize_startup_ui(
    std::vector<UiSourceSprite> sprites,
    std::uint16_t atlas_width,
    std::uint16_t atlas_height,
    std::uint32_t source_assets) {
    std::vector<Rgba> atlas_pixels(
        static_cast<std::size_t>(atlas_width) * atlas_height,
        {0, 0, 0, 0});
    std::uint16_t x = 0;
    std::uint16_t y = 0;
    std::uint16_t row_height = 0;
    for (UiSourceSprite& sprite : sprites) {
        if (x + sprite.width > atlas_width) {
            x = 0;
            y = static_cast<std::uint16_t>(y + row_height);
            row_height = 0;
        }
        if (y + sprite.height > atlas_height) {
            throw std::runtime_error("startup UI atlas overflow");
        }
        sprite.atlas_x = x;
        sprite.atlas_y = y;
        for (std::uint32_t py = 0; py < sprite.height; ++py) {
            for (std::uint32_t px = 0; px < sprite.width; ++px) {
                atlas_pixels[
                    (y + py) * atlas_width + x + px] =
                    sprite.pixels[py * sprite.width + px];
            }
        }
        x = static_cast<std::uint16_t>(x + sprite.width);
        row_height = std::max(row_height, sprite.height);
    }
    std::uint32_t stored_width = 0;
    std::uint32_t stored_height = 0;
    const std::vector<Byte> atlas = rgba4444_swizzled(
        atlas_pixels, atlas_width, atlas_height,
        &stored_width, &stored_height);
    if (stored_width != atlas_width || stored_height != atlas_height) {
        throw std::runtime_error("startup UI atlas alignment changed");
    }
    const std::size_t atlas_offset =
        align16(128 + sprites.size() * 32);
    std::vector<Byte> output(
        align16(atlas_offset + atlas.size()), 0);
    std::memcpy(output.data(), "DPSU", 4);
    put_u16(output, 4, 1);
    put_u16(output, 6, 128);
    put_u32(output, 8, static_cast<std::uint32_t>(output.size()));
    put_u32(output, 16, atlas_width);
    put_u32(output, 20, atlas_height);
    put_u32(output, 24, 2);
    put_u32(output, 28, static_cast<std::uint32_t>(sprites.size()));
    put_u32(output, 32, 128);
    put_u32(output, 36, 32);
    put_u32(output, 40, static_cast<std::uint32_t>(atlas_offset));
    put_u32(output, 44, static_cast<std::uint32_t>(atlas.size()));
    put_u32(output, 48, source_assets);
    for (std::size_t index = 0; index < sprites.size(); ++index) {
        const UiSourceSprite& sprite = sprites[index];
        const std::size_t at = 128 + index * 32;
        put_u16(output, at, sprite.id);
        put_u16(output, at + 2, sprite.channel);
        put_s16(output, at + 4, sprite.screen_x);
        put_s16(output, at + 6, sprite.screen_y);
        put_u16(output, at + 8, sprite.width);
        put_u16(output, at + 10, sprite.height);
        put_u16(output, at + 12, sprite.atlas_x);
        put_u16(output, at + 14, sprite.atlas_y);
        put_u16(output, at + 16, sprite.width);
        put_u16(output, at + 18, sprite.height);
        put_u32(output, at + 20, sprite.color);
        put_u32(output, at + 24, fnv1a(sprite.name.c_str()));
        put_u16(output, at + 28, sprite.advance);
    }
    std::copy(
        atlas.begin(), atlas.end(),
        output.begin() + static_cast<std::ptrdiff_t>(atlas_offset));
    put_u32(output, 12, crc32(output));
    return output;
}

}  // namespace

void export_original_startup_ui_packages(
    const std::vector<HudSourceResource>& logo_resources,
    const std::vector<HudSourceResource>& warning_resources,
    const std::vector<std::uint8_t>& font) {
    const char* logos_output =
        std::getenv("DUSKLIGHT_STARTUP_LOGOS_DPSU_OUTPUT");
    const char* title_output =
        std::getenv("DUSKLIGHT_STARTUP_TITLE_UI_DPSU_OUTPUT");
    if (logos_output == nullptr || logos_output[0] == '\0' ||
        title_output == nullptr || title_output[0] == '\0' ||
        logo_resources.size() != 2 || warning_resources.size() != 2) {
        throw std::runtime_error("startup UI export arguments invalid");
    }
    UiSourceSprite warning =
        startup_bti_sprite(warning_resources[0], 0, 0);
    UiSourceSprite prompt =
        startup_bti_sprite(warning_resources[1], 3, 0);
    warning.pixels = resize_pixels(
        warning.pixels, warning.width, warning.height, 368, 272);
    warning.width = 368;
    warning.height = 272;
    warning.screen_x = 56;
    warning.screen_y = 0;
    const std::vector<Rgba> prompt_pixels = resize_pixels(
        prompt.pixels, prompt.width, prompt.height, 368, 29);
    for (std::uint32_t py = 0; py < 29; ++py) {
        for (std::uint32_t px = 0; px < 368; ++px) {
            const Rgba pixel = prompt_pixels[py * 368 + px];
            if (pixel.a != 0) {
                warning.pixels[(243 + py) * 368 + px] = pixel;
            }
        }
    }
    UiSourceSprite nintendo =
        startup_bti_sprite(logo_resources[0], 1, 1);
    nintendo.screen_x =
        static_cast<std::int16_t>((480 - nintendo.width) / 2);
    nintendo.screen_y =
        static_cast<std::int16_t>((272 - nintendo.height) / 2);
    UiSourceSprite dolby =
        startup_bti_sprite(logo_resources[1], 2, 2);
    dolby.screen_x =
        static_cast<std::int16_t>((480 - dolby.width) / 2);
    dolby.screen_y =
        static_cast<std::int16_t>((272 - dolby.height) / 2);
    write_file(
        logos_output,
        serialize_startup_ui(
            {std::move(warning), std::move(nintendo), std::move(dolby)},
            512, 512, 4));

    constexpr char text[] = "Appuyez sur START";
    std::vector<UiSourceSprite> glyphs;
    std::uint16_t total_advance = 0;
    std::uint16_t glyph_index = 0;
    for (char character : std::string(text)) {
        UiSourceSprite glyph = font_glyph(font, character);
        total_advance =
            static_cast<std::uint16_t>(total_advance + glyph.advance);
        glyph.id = static_cast<std::uint16_t>(128 + glyph_index++);
        glyphs.push_back(std::move(glyph));
    }
    std::int16_t cursor =
        static_cast<std::int16_t>((480 - total_advance) / 2);
    for (UiSourceSprite& glyph : glyphs) {
        glyph.screen_x = cursor;
        glyph.screen_y = 220;
        cursor = static_cast<std::int16_t>(
            cursor + glyph.advance);
    }
    write_file(
        title_output,
        serialize_startup_ui(std::move(glyphs), 256, 64, 1));
    std::cout << "ORIGINAL_STARTUP_UI_EXPORT_OK"
              << " logos=warning_fr,nintendo,dolby"
              << " title_message_id=100"
              << " title_text=Appuyez_sur_START"
              << " formats=DPSU1,DPSU1\n";
}

void export_original_file_select_ui_package(
    const std::vector<HudSourceResource>& resources,
    const char* output) {
    if (output == nullptr || output[0] == '\0' ||
        resources.size() != 4) {
        throw std::runtime_error(
            "file select UI export arguments invalid");
    }
    UiSourceSprite background =
        startup_bti_sprite(resources[0], 200, 9);
    background.pixels = resize_pixels(
        background.pixels, background.width, background.height,
        480, 272);
    background.width = 480;
    background.height = 272;
    background.screen_x = 0;
    background.screen_y = 0;
    std::vector<UiSourceSprite> sprites;
    sprites.push_back(std::move(background));
    for (std::uint16_t slot = 0; slot < 3; ++slot) {
        UiSourceSprite frame =
            startup_bti_sprite(
                resources[1],
                static_cast<std::uint16_t>(201 + slot), 9);
        frame.screen_x =
            static_cast<std::int16_t>((480 - frame.width) / 2);
        frame.screen_y =
            static_cast<std::int16_t>(38 + slot * 70);
        sprites.push_back(std::move(frame));
    }
    for (std::uint16_t slot = 0; slot < 3; ++slot) {
        UiSourceSprite cursor =
            startup_bti_sprite(
                resources[2],
                static_cast<std::uint16_t>(204 + slot),
                static_cast<std::uint16_t>(10 + slot));
        cursor.screen_x = 44;
        cursor.screen_y =
            static_cast<std::int16_t>(38 + slot * 70);
        sprites.push_back(std::move(cursor));
    }
    UiSourceSprite button =
        startup_bti_sprite(resources[3], 207, 9);
    button.screen_x = static_cast<std::int16_t>(
        462 - button.width);
    button.screen_y = static_cast<std::int16_t>(
        260 - button.height);
    sprites.push_back(std::move(button));
    write_file(
        output,
        serialize_startup_ui(
            std::move(sprites), 512, 512, 4));
    std::cout << "ORIGINAL_FILE_SELECT_UI_EXPORT_OK"
              << " archive=/res/Object/fileSel.arc"
              << " source_layout=zelda_file_select.blo"
              << " source_slots=3"
              << " layout_status=bounded_position_shim"
              << " format=DPSU1\n";
}

void export_original_hud_package(
    const std::vector<std::uint8_t>& layout,
    const std::vector<HudSourceResource>& resources,
    const std::vector<std::uint8_t>& font) {
    const char* output = std::getenv("DUSKLIGHT_DPUI_OUTPUT");
    if (output == nullptr || output[0] == '\0') {
        return;
    }
    std::string inventory;
    const std::vector<Byte> bytes =
        serialize_original_dpui(layout, resources, font, &inventory);
    write_file(output, bytes);
    const char* inventory_output =
        std::getenv("DUSKLIGHT_HUD_INVENTORY_OUTPUT");
    if (inventory_output != nullptr &&
        inventory_output[0] != '\0') {
        write_file(
            inventory_output,
            std::vector<Byte>(
                inventory.begin(), inventory.end()));
    }
    std::cout << "ORIGINAL_HUD_EXPORT_OK"
              << " dpui_version=2"
              << " source_layout=604x448"
              << " resources=" << resources.size()
              << " font=rodan_b_24_22.bfn"
              << " bytes=" << bytes.size() << '\n';
}

void export_playable_model_and_animations(
    const std::array<J3DModelData*, 4>& models,
    std::vector<std::uint8_t>& animation_archive) {
    const char* dpsk_path = std::getenv("DUSKLIGHT_DPSK_OUTPUT");
    const char* dpan_path = std::getenv("DUSKLIGHT_DPAN_OUTPUT");
    const char* dptx_path = std::getenv("DUSKLIGHT_DPTX_OUTPUT");
    const char* dpui_path = std::getenv("DUSKLIGHT_DPUI_OUTPUT");
    const char* bck_curve_path = std::getenv("DUSKLIGHT_BCK_CURVE_OUTPUT");
    if ((dpsk_path == nullptr || dpsk_path[0] == '\0') &&
        (dpan_path == nullptr || dpan_path[0] == '\0') &&
        (dptx_path == nullptr || dptx_path[0] == '\0') &&
        (dpui_path == nullptr || dpui_path[0] == '\0') &&
        (bck_curve_path == nullptr || bck_curve_path[0] == '\0')) {
        return;
    }
    for (J3DModelData* model : models) {
        if (model == nullptr) {
            throw std::runtime_error("playable BMD missing");
        }
    }
    if (dpsk_path != nullptr && dpsk_path[0] != '\0') {
        const std::vector<Byte> bytes = serialize_dpsk(models);
        write_file(dpsk_path, bytes);
    }
    if (dpan_path != nullptr && dpan_path[0] != '\0') {
        const std::vector<Byte> bytes = serialize_dpan(animation_archive);
        write_file(dpan_path, bytes);
    }
    if (bck_curve_path != nullptr && bck_curve_path[0] != '\0') {
        write_bck_curve_dump(animation_archive, bck_curve_path);
    }
    if (dptx_path != nullptr && dptx_path[0] != '\0') {
        const std::vector<Byte> bytes = serialize_dptx(models);
        write_file(dptx_path, bytes);
    }
    (void)dpui_path;
}

void export_real_room_packages(
    J3DModelData* model,
    const std::uint8_t* collision,
    std::uint32_t collision_size,
    const std::uint8_t* placement,
    std::uint32_t placement_size,
    const std::uint8_t* stage,
    std::uint32_t stage_size) {
    (void)collision;
    (void)collision_size;
    (void)placement;
    (void)placement_size;
    const char* dprm_path = std::getenv("DUSKLIGHT_DPRM_OUTPUT");
    const char* dptx_path = std::getenv("DUSKLIGHT_ROOM_DPTX_OUTPUT");
    const char* dpcl_path = std::getenv("DUSKLIGHT_DPCL_OUTPUT");
    const char* dpsc_path = std::getenv("DUSKLIGHT_DPSC_OUTPUT");
    if (dprm_path != nullptr && dprm_path[0] != '\0') {
        write_file(dprm_path, serialize_dprm(model));
    }
    if (dptx_path != nullptr && dptx_path[0] != '\0') {
        write_file(dptx_path, serialize_room_dptx(model));
    }
    if (dpcl_path != nullptr && dpcl_path[0] != '\0') {
        write_file(dpcl_path, serialize_dpcl(collision, collision_size));
    }
    if (dpsc_path != nullptr && dpsc_path[0] != '\0') {
        write_file(
            dpsc_path,
            serialize_dpsc(
                collision, collision_size, placement, placement_size,
                stage, stage_size));
    }
}

void export_static_model_packages(
    J3DModelData* model,
    const char* dprm_path,
    const char* dptx_path) {
    if (model == nullptr || dprm_path == nullptr ||
        dprm_path[0] == '\0' || dptx_path == nullptr ||
        dptx_path[0] == '\0') {
        throw std::runtime_error("static model export arguments invalid");
    }
    write_file(dprm_path, serialize_dprm(model));
    write_file(dptx_path, serialize_room_dptx(model));
}

void export_animated_static_model_package(
    J3DModelData* model,
    J3DAnmTransform* animation,
    float frame,
    const char* dprm_path) {
    if (model == nullptr || animation == nullptr ||
        dprm_path == nullptr || dprm_path[0] == '\0' ||
        !std::isfinite(frame)) {
        throw std::runtime_error(
            "animated static model export arguments invalid");
    }
    write_file(dprm_path, serialize_dprm(model, animation, frame));
}

void export_static_model_texture_package(
    J3DModelData* model,
    const char* dptx_path) {
    if (model == nullptr || dptx_path == nullptr || dptx_path[0] == '\0') {
        throw std::runtime_error(
            "static model texture export arguments invalid");
    }
    write_file(dptx_path, serialize_room_dptx(model));
}

void export_static_movebg_packages(
    J3DModelData* model,
    const std::uint8_t* collision,
    std::uint32_t collision_size,
    const char* dprm_path,
    const char* dptx_path,
    const char* dpcl_path) {
    if (collision == nullptr || collision_size == 0 ||
        dpcl_path == nullptr || dpcl_path[0] == '\0') {
        throw std::runtime_error(
            "static MoveBG collision export arguments invalid");
    }
    export_static_model_packages(model, dprm_path, dptx_path);
    write_file(
        dpcl_path, serialize_movebg_dpcl(collision, collision_size));
}

void export_movebg_collision_package(
    const std::uint8_t* collision,
    std::uint32_t collision_size,
    const char* dpcl_path) {
    if (collision == nullptr || collision_size == 0 ||
        dpcl_path == nullptr || dpcl_path[0] == '\0') {
        throw std::runtime_error(
            "MoveBG collision export arguments invalid");
    }
    write_file(
        dpcl_path, serialize_movebg_dpcl(collision, collision_size));
}

void export_static_bck_package(
    std::vector<std::uint8_t>& archive_bytes,
    std::uint16_t resource_id,
    const char* dpan_path) {
    if (archive_bytes.empty() || dpan_path == nullptr ||
        dpan_path[0] == '\0') {
        throw std::runtime_error("static BCK export arguments invalid");
    }
    write_file(
        dpan_path,
        serialize_single_dpan(archive_bytes, resource_id));
}
