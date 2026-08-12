#include "dusk/psp/playable_package.hpp"

#include <cmath>
#include <cstring>

namespace dusk::psp::playable {
namespace {

bool range(
    std::uint32_t offset,
    std::uint32_t count,
    std::uint32_t stride,
    std::uint32_t size) {
    return offset <= size && stride != 0 &&
           count <= (size - offset) / stride;
}

PackageError base(
    const void* source,
    std::uint32_t size,
    const char magic[4],
    PackageView* view) {
    if (source == nullptr || view == nullptr) {
        return PackageError::Missing;
    }
    if (size < 128) {
        return PackageError::Truncated;
    }
    const auto* bytes = static_cast<const std::uint8_t*>(source);
    if (std::memcmp(bytes, magic, 4) != 0) {
        return PackageError::Magic;
    }
    const std::uint16_t version = read_u16(bytes + 4);
    if (version != 1 &&
        !(std::memcmp(bytes, "DPUI", 4) == 0 && version == 2)) {
        return PackageError::Version;
    }
    if (read_u32(bytes + 8) != size) {
        return PackageError::Size;
    }
    *view = {bytes, size, read_u32(bytes + 12), package_crc32(bytes, size)};
    return view->expected_crc == view->actual_crc
        ? PackageError::Ok
        : PackageError::Crc;
}

}  // namespace

std::uint16_t read_u16(const std::uint8_t* bytes) {
    return static_cast<std::uint16_t>(
        bytes[0] | (static_cast<std::uint16_t>(bytes[1]) << 8));
}

std::int16_t read_s16(const std::uint8_t* bytes) {
    return static_cast<std::int16_t>(read_u16(bytes));
}

std::uint32_t read_u32(const std::uint8_t* bytes) {
    return static_cast<std::uint32_t>(bytes[0]) |
           (static_cast<std::uint32_t>(bytes[1]) << 8) |
           (static_cast<std::uint32_t>(bytes[2]) << 16) |
           (static_cast<std::uint32_t>(bytes[3]) << 24);
}

float read_f32(const std::uint8_t* bytes) {
    const std::uint32_t bits = read_u32(bytes);
    float value = 0.0f;
    std::memcpy(&value, &bits, sizeof(value));
    return value;
}

std::uint32_t package_crc32(
    const std::uint8_t* bytes, std::uint32_t size) {
    std::uint32_t crc = 0xffffffffu;
    for (std::uint32_t index = 0; index < size; ++index) {
        const std::uint8_t byte =
            index >= 12 && index < 16 ? 0 : bytes[index];
        crc ^= byte;
        for (int bit = 0; bit < 8; ++bit) {
            crc = (crc >> 1) ^
                  (0xedb88320u & (0u - (crc & 1u)));
        }
    }
    return ~crc;
}

PackageError validate_package(
    const void* bytes,
    std::uint32_t size,
    const char magic[4],
    PackageView* view) {
    return base(bytes, size, magic, view);
}

PackageError validate_dpsk(
    const void* source, std::uint32_t size, PackageView* view) {
    PackageError error = base(source, size, "DPSK", view);
    if (error != PackageError::Ok) {
        return error;
    }
    const auto* bytes = view->bytes;
    const std::uint32_t joints = read_u32(bytes + 16);
    const std::uint32_t vertices = read_u32(bytes + 20);
    const std::uint32_t indices = read_u32(bytes + 24);
    const std::uint32_t submeshes = read_u32(bytes + 28);
    if (joints != 35 || read_u32(bytes + 32) != 4329 ||
        read_u32(bytes + 36) != 5 || vertices == 0 ||
        indices != 4329 * 3 || submeshes == 0 || submeshes > 40) {
        return PackageError::Count;
    }
    const std::uint32_t joint_offset = read_u32(bytes + 64);
    const std::uint32_t joint_stride = read_u32(bytes + 68);
    const std::uint32_t vertex_offset = read_u32(bytes + 72);
    const std::uint32_t vertex_stride = read_u32(bytes + 76);
    const std::uint32_t index_offset = read_u32(bytes + 80);
    const std::uint32_t submesh_offset = read_u32(bytes + 84);
    const std::uint32_t submesh_stride = read_u32(bytes + 88);
    if (joint_stride != 128 || vertex_stride != 64 ||
        submesh_stride != 32 ||
        !range(joint_offset, joints, joint_stride, size) ||
        !range(vertex_offset, vertices, vertex_stride, size) ||
        !range(index_offset, indices, 2, size) ||
        !range(submesh_offset, submeshes, submesh_stride, size)) {
        return PackageError::Range;
    }
    for (std::uint32_t index = 0; index < joints; ++index) {
        const std::int16_t parent =
            read_s16(bytes + joint_offset + index * joint_stride);
        if (parent >= static_cast<std::int16_t>(index) || parent < -1) {
            return PackageError::JointParent;
        }
    }
    for (std::uint32_t index = 0; index < vertices; ++index) {
        const std::uint8_t* vertex =
            bytes + vertex_offset + index * vertex_stride;
        std::uint32_t total = 0;
        for (std::uint32_t influence = 0; influence < 5; ++influence) {
            if (vertex[32 + influence] >= joints) {
                return PackageError::Weight;
            }
            total += vertex[37 + influence];
        }
        if (total != 255) {
            return PackageError::Weight;
        }
        for (std::uint32_t component = 0; component < 8; ++component) {
            if (!std::isfinite(read_f32(vertex + component * 4))) {
                return PackageError::NonFinite;
            }
        }
    }
    for (std::uint32_t index = 0; index < indices; ++index) {
        if (read_u16(bytes + index_offset + index * 2) >= vertices) {
            return PackageError::Index;
        }
    }
    for (std::uint32_t index = 0; index < submeshes; ++index) {
        const std::uint8_t* item =
            bytes + submesh_offset + index * submesh_stride;
        const std::uint32_t first = read_u32(item);
        const std::uint32_t count = read_u32(item + 4);
        if (first > indices || count > indices - first ||
            count == 0 || count % 3 != 0 ||
            read_u16(item + 8) >= 27 ||
            read_u16(item + 10) >= 29) {
            return PackageError::Index;
        }
    }
    return PackageError::Ok;
}

PackageError validate_dptx(
    const void* source, std::uint32_t size, PackageView* view) {
    PackageError error = base(source, size, "DPTX", view);
    if (error != PackageError::Ok) {
        return error;
    }
    const auto* bytes = view->bytes;
    const std::uint32_t textures = read_u32(bytes + 16);
    const std::uint32_t materials = read_u32(bytes + 20);
    const std::uint32_t texture_offset = read_u32(bytes + 24);
    const std::uint32_t texture_stride = read_u32(bytes + 28);
    const std::uint32_t material_offset = read_u32(bytes + 32);
    const std::uint32_t material_stride = read_u32(bytes + 36);
    if (textures != 29 || materials != 27 ||
        texture_stride != 48 || material_stride != 32 ||
        !range(texture_offset, textures, texture_stride, size) ||
        !range(material_offset, materials, material_stride, size)) {
        return PackageError::Count;
    }
    std::uint32_t total = 0;
    for (std::uint32_t index = 0; index < textures; ++index) {
        const std::uint8_t* item =
            bytes + texture_offset + index * texture_stride;
        const std::uint32_t width = read_u16(item + 4);
        const std::uint32_t height = read_u16(item + 6);
        const std::uint32_t stored_width = read_u16(item + 8);
        const std::uint32_t stored_height = read_u16(item + 10);
        const std::uint32_t offset = read_u32(item + 16);
        const std::uint32_t bytes_count = read_u32(item + 20);
        if (width == 0 || height == 0 || width > 512 || height > 512 ||
            stored_width < width || stored_height < height ||
            (stored_width & 7u) != 0 || (stored_height & 7u) != 0 ||
            item[12] != 2 || offset > size ||
            bytes_count != stored_width * stored_height * 2 ||
            bytes_count > size - offset) {
            return PackageError::Texture;
        }
        total += bytes_count;
        if (total > 1150000) {
            return PackageError::EdramBudget;
        }
    }
    for (std::uint32_t index = 0; index < materials; ++index) {
        const std::uint8_t* item =
            bytes + material_offset + index * material_stride;
        if (read_u16(item) >= textures || item[2] > 2) {
            return PackageError::Material;
        }
    }
    return PackageError::Ok;
}

PackageError validate_dpan(
    const void* source, std::uint32_t size, PackageView* view) {
    PackageError error = base(source, size, "DPAN", view);
    if (error != PackageError::Ok) {
        return error;
    }
    const auto* bytes = view->bytes;
    const std::uint32_t clips = read_u32(bytes + 16);
    const std::uint32_t joints = read_u32(bytes + 20);
    const std::uint32_t table = read_u32(bytes + 32);
    const std::uint32_t stride = read_u32(bytes + 36);
    if (clips == 0 || clips > 256 || joints == 0 || joints > 64 ||
        read_u32(bytes + 24) == 0 ||
        stride != 48 || !range(table, clips, stride, size)) {
        return PackageError::Count;
    }
    for (std::uint32_t clip = 0; clip < clips; ++clip) {
        const std::uint8_t* item = bytes + table + clip * stride;
        const std::uint32_t samples = read_u32(item + 12);
        const std::uint32_t clip_joints = read_u32(item + 16);
        const std::uint32_t offset = read_u32(item + 24);
        const std::uint32_t bytes_count = read_u32(item + 28);
        if (samples < 2 || clip_joints != joints ||
            !range(offset, samples * joints, 40, size) ||
            bytes_count != samples * joints * 40) {
            return PackageError::Animation;
        }
        for (std::uint32_t sample = 0; sample < samples * joints; ++sample) {
            const std::uint8_t* transform = bytes + offset + sample * 40;
            float length = 0.0f;
            for (std::uint32_t component = 0; component < 10; ++component) {
                const float value = read_f32(transform + component * 4);
                if (!std::isfinite(value)) {
                    return PackageError::NonFinite;
                }
                if (component >= 3 && component < 7) {
                    length += value * value;
                }
            }
            if (std::fabs(length - 1.0f) > 0.002f) {
                return PackageError::Animation;
            }
        }
    }
    return PackageError::Ok;
}

PackageError validate_dpui(
    const void* source, std::uint32_t size, PackageView* view) {
    PackageError error = base(source, size, "DPUI", view);
    if (error != PackageError::Ok) {
        return error;
    }
    const auto* bytes = view->bytes;
    const std::uint32_t width = read_u32(bytes + 16);
    const std::uint32_t height = read_u32(bytes + 20);
    const std::uint32_t quads = read_u32(bytes + 28);
    const std::uint32_t table = read_u32(bytes + 32);
    const std::uint32_t stride = read_u32(bytes + 36);
    const std::uint32_t atlas = read_u32(bytes + 40);
    const std::uint32_t atlas_bytes = read_u32(bytes + 44);
    const std::uint16_t version = read_u16(bytes + 4);
    const std::uint32_t maximum_quads = version == 2 ? 128 : 32;
    if (width == 0 || height == 0 || width > 512 || height > 512 ||
        read_u32(bytes + 24) != 2 || quads == 0 ||
        quads > maximum_quads ||
        stride != 32 || !range(table, quads, stride, size) ||
        atlas > size || atlas_bytes != width * height * 2 ||
        atlas_bytes > size - atlas) {
        return PackageError::Range;
    }
    if (version == 2 &&
        (quads > maximum_quads ||
         read_u32(bytes + 52) != 604 ||
         read_u32(bytes + 56) != 448 ||
         read_u32(bytes + 60) == 0 ||
         read_u32(bytes + 64) != 1)) {
        return PackageError::Range;
    }
    if (version == 2) {
        bool identities[384] = {};
        for (std::uint32_t index = 0; index < quads; ++index) {
            const std::uint8_t* item =
                bytes + table + index * stride;
            const std::uint16_t id = read_u16(item);
            const std::uint32_t u = read_u16(item + 12);
            const std::uint32_t v = read_u16(item + 14);
            const std::uint32_t item_width = read_u16(item + 16);
            const std::uint32_t item_height = read_u16(item + 18);
            if (id >= 384 || identities[id] ||
                item_width == 0 || item_height == 0 ||
                u > width || item_width > width - u ||
                v > height || item_height > height - v ||
                read_u32(item + 24) == 0 ||
                (id >= 128 && read_u16(item + 28) == 0)) {
                return PackageError::Range;
            }
            identities[id] = true;
        }
        constexpr std::uint16_t required_sprites[] = {
            0, 1, 2, 3,
            10, 11, 12, 13, 14, 15, 16, 17, 18, 19,
            20, 30, 40, 41, 42, 43,
        };
        for (std::uint16_t id : required_sprites) {
            if (!identities[id]) {
                return PackageError::Missing;
            }
        }
        // DPUI v2 is now the common text surface for pause/menu/message UI.
        // Require the complete printable ASCII range from the source Rodan
        // font so missing letters cannot silently turn into blank glyphs.
        for (std::uint16_t code = 0x20; code <= 0x7e; ++code) {
            if (!identities[128 + code]) {
                return PackageError::Missing;
            }
        }
        if (atlas_bytes > 196608) {
            return PackageError::EdramBudget;
        }
    }
    return PackageError::Ok;
}

PackageError validate_package_set(const PackageSet& packages) {
    if (packages.model.bytes == nullptr ||
        packages.textures.bytes == nullptr ||
        packages.animations.bytes == nullptr ||
        packages.ui.bytes == nullptr) {
        return PackageError::Missing;
    }
    return PackageError::Ok;
}

const char* package_error_name(PackageError error) {
    switch (error) {
    case PackageError::Ok: return "ok";
    case PackageError::Missing: return "missing";
    case PackageError::Truncated: return "truncated";
    case PackageError::Magic: return "magic";
    case PackageError::Version: return "version";
    case PackageError::Size: return "size";
    case PackageError::Crc: return "crc";
    case PackageError::Count: return "count";
    case PackageError::Range: return "range";
    case PackageError::JointParent: return "joint_parent";
    case PackageError::Weight: return "weight";
    case PackageError::Index: return "index";
    case PackageError::Texture: return "texture";
    case PackageError::Material: return "material";
    case PackageError::Animation: return "animation";
    case PackageError::NonFinite: return "non_finite";
    case PackageError::EdramBudget: return "edram_budget";
    }
    return "unknown";
}

}  // namespace dusk::psp::playable
