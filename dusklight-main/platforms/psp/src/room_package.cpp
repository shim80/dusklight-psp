#include "dusk/psp/room_package.hpp"

#include <cmath>
#include <cstring>
#include <initializer_list>

namespace dusk::psp::room {
namespace {

constexpr std::uint32_t kHeaderSize = 256;
constexpr std::uint32_t kSectionSize = 32;
constexpr std::uint32_t kVertexSize = 24;
constexpr std::uint32_t kSubmeshSize = 48;
constexpr std::uint32_t kMaximumVertices = 45000;
constexpr std::uint32_t kMaximumTriangles = 30000;
constexpr std::uint32_t kMaximumSubmeshes = 96;

bool range_valid(
    std::uint32_t offset,
    std::uint32_t count,
    std::uint32_t stride,
    std::uint32_t size) {
    return offset <= size &&
           stride != 0 &&
           count <= (size - offset) / stride;
}

PackageError validate_base(
    const void* source,
    std::uint32_t size,
    const char magic[4],
    PackageView* view,
    std::uint16_t maximum_version = 1) {
    if (source == nullptr || view == nullptr) {
        return PackageError::Missing;
    }
    if (size < 16) {
        return PackageError::Truncated;
    }
    const auto* bytes = static_cast<const std::uint8_t*>(source);
    if (std::memcmp(bytes, magic, 4) != 0) {
        return PackageError::Magic;
    }
    const std::uint16_t version = read_u16(bytes + 4);
    if (version == 0 || version > maximum_version) {
        return PackageError::Version;
    }
    if (read_u32(bytes + 8) != size) {
        return PackageError::Size;
    }
    view->bytes = bytes;
    view->size = size;
    view->expected_crc = read_u32(bytes + 12);
    view->actual_crc = package_crc32(bytes, size);
    if (view->expected_crc != view->actual_crc) {
        return PackageError::Crc;
    }
    return PackageError::Ok;
}

}  // namespace

std::uint16_t read_u16(const std::uint8_t* bytes) {
    return static_cast<std::uint16_t>(
        bytes[0] | (static_cast<std::uint16_t>(bytes[1]) << 8));
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
        std::uint8_t value = bytes[index];
        if (index >= 12 && index < 16) {
            value = 0;
        }
        crc ^= value;
        for (std::uint32_t bit = 0; bit < 8; ++bit) {
            crc = (crc >> 1) ^
                  (0xedb88320u & (0u - (crc & 1u)));
        }
    }
    return ~crc;
}

PackageError validate_dprm(
    const void* source, std::uint32_t size, PackageView* view) {
    const PackageError base = validate_base(source, size, "DPRM", view);
    if (base != PackageError::Ok) {
        return base;
    }
    const std::uint8_t* bytes = view->bytes;
    if (read_u16(bytes + 6) != kHeaderSize || size < kHeaderSize) {
        return PackageError::Layout;
    }
    const std::uint32_t sections = read_u32(bytes + 16);
    const std::uint32_t vertices = read_u32(bytes + 20);
    const std::uint32_t indices = read_u32(bytes + 24);
    const std::uint32_t triangles = read_u32(bytes + 28);
    const std::uint32_t submeshes = read_u32(bytes + 32);
    const std::uint32_t materials = read_u32(bytes + 36);
    const std::uint32_t textures = read_u32(bytes + 40);
    if (sections != 4 ||
        vertices == 0 || vertices > kMaximumVertices ||
        indices == 0 || indices % 3 != 0 ||
        triangles == 0 || triangles > kMaximumTriangles ||
        triangles != indices / 3 ||
        submeshes == 0 || submeshes > kMaximumSubmeshes ||
        materials == 0 || materials > 96 ||
        textures == 0 || textures > 96) {
        return PackageError::Count;
    }
    for (std::uint32_t offset = 48; offset < 72; offset += 4) {
        if (!std::isfinite(read_f32(bytes + offset))) {
            return PackageError::NonFinite;
        }
    }
    if (read_u32(bytes + 80) != kVertexSize ||
        read_u32(bytes + 84) != 0 ||
        read_u32(bytes + 88) != 4 ||
        read_u32(bytes + 92) != 8 ||
        read_u32(bytes + 96) != 12 ||
        read_u32(bytes + 100) != 16 ||
        read_u32(bytes + 104) != 20) {
        return PackageError::Layout;
    }
    const std::uint32_t section_table = read_u32(bytes + 72);
    if (!range_valid(
            section_table, sections, kSectionSize, size)) {
        return PackageError::Range;
    }
    const std::uint8_t* vertex_section = bytes + section_table;
    const std::uint8_t* index_section =
        vertex_section + kSectionSize;
    const std::uint8_t* submesh_section =
        index_section + kSectionSize;
    if (read_u32(vertex_section) != 1 ||
        read_u32(vertex_section + 12) != vertices ||
        read_u32(vertex_section + 16) != kVertexSize ||
        read_u32(index_section) != 2 ||
        read_u32(index_section + 12) != indices ||
        read_u32(index_section + 16) != 2 ||
        read_u32(submesh_section) != 3 ||
        read_u32(submesh_section + 12) != submeshes ||
        read_u32(submesh_section + 16) != kSubmeshSize) {
        return PackageError::Layout;
    }
    const std::uint32_t vertex_offset = read_u32(vertex_section + 4);
    const std::uint32_t index_offset = read_u32(index_section + 4);
    const std::uint32_t submesh_offset = read_u32(submesh_section + 4);
    if (!range_valid(vertex_offset, vertices, kVertexSize, size) ||
        !range_valid(index_offset, indices, 2, size) ||
        !range_valid(
            submesh_offset, submeshes, kSubmeshSize, size)) {
        return PackageError::Range;
    }
    for (std::uint32_t index = 0; index < vertices; ++index) {
        const std::uint8_t* vertex =
            bytes + vertex_offset + index * kVertexSize;
        for (std::uint32_t component : {0u, 4u, 12u, 16u, 20u}) {
            if (!std::isfinite(read_f32(vertex + component))) {
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
        const std::uint8_t* submesh =
            bytes + submesh_offset + index * kSubmeshSize;
        const std::uint32_t first = read_u32(submesh);
        const std::uint32_t count = read_u32(submesh + 4);
        if (first > indices || count > indices - first ||
            count == 0 || count % 3 != 0) {
            return PackageError::Range;
        }
        if (read_u16(submesh + 8) >= materials) {
            return PackageError::Material;
        }
        if (read_u16(submesh + 16) >= materials) {
            return PackageError::Material;
        }
        if (read_u16(submesh + 10) >= textures) {
            return PackageError::Texture;
        }
        if (submesh[12] > 3) {
            return PackageError::Bucket;
        }
        if ((submesh[18] & ~3u) != 0 || submesh[19] > 7) {
            return PackageError::Layout;
        }
    }
    return PackageError::Ok;
}

PackageError validate_room_dptx(
    const void* source, std::uint32_t size, PackageView* view) {
    const PackageError base = validate_base(source, size, "DPTX", view);
    if (base != PackageError::Ok) {
        return base;
    }
    const std::uint8_t* bytes = view->bytes;
    if (read_u16(bytes + 6) != 128 || size < 128) {
        return PackageError::Layout;
    }
    const std::uint32_t textures = read_u32(bytes + 16);
    const std::uint32_t materials = read_u32(bytes + 20);
    const std::uint32_t texture_offset = read_u32(bytes + 24);
    const std::uint32_t texture_stride = read_u32(bytes + 28);
    const std::uint32_t material_offset = read_u32(bytes + 32);
    const std::uint32_t material_stride = read_u32(bytes + 36);
    if (textures == 0 || textures > 96 ||
        materials == 0 || materials > 96 ||
        texture_stride != 48 || material_stride != 32 ||
        !range_valid(texture_offset, textures, texture_stride, size) ||
        !range_valid(material_offset, materials, material_stride, size)) {
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
            item[12] > 5 || offset > size ||
            bytes_count != stored_width * stored_height * 2 ||
            bytes_count > size - offset) {
            return PackageError::Texture;
        }
        if (bytes_count > 720000 ||
            total > 720000 - bytes_count) {
            return PackageError::EdramBudget;
        }
        total += bytes_count;
    }
    if (read_u32(bytes + 48) != total) {
        return PackageError::Size;
    }
    for (std::uint32_t index = 0; index < materials; ++index) {
        const std::uint8_t* item =
            bytes + material_offset + index * material_stride;
        if (read_u16(item) >= textures) {
            return PackageError::Texture;
        }
        if (item[2] > 3) {
            return PackageError::Bucket;
        }
    }
    return PackageError::Ok;
}

PackageError validate_dpcl(
    const void* source, std::uint32_t size, PackageView* view) {
    const PackageError base = validate_base(source, size, "DPCL", view);
    if (base != PackageError::Ok) {
        return base;
    }
    const std::uint8_t* bytes = view->bytes;
    if (read_u16(bytes + 6) != 256 || size < 256) {
        return PackageError::Layout;
    }
    const std::uint32_t vertices = read_u32(bytes + 16);
    const std::uint32_t triangles = read_u32(bytes + 20);
    const std::uint32_t cells_x = read_u32(bytes + 24);
    const std::uint32_t cells_z = read_u32(bytes + 28);
    const std::uint32_t cells = read_u32(bytes + 32);
    const std::uint32_t references = read_u32(bytes + 36);
    if (vertices == 0 || vertices > 75000 ||
        triangles == 0 || triangles > 25000 ||
        vertices != triangles * 3 ||
        cells_x == 0 || cells_z == 0 ||
        cells_x > 256 || cells_z > 256 ||
        cells != cells_x * cells_z || cells > 4096 ||
        references == 0 || references > triangles * cells) {
        return PackageError::Count;
    }
    for (std::uint32_t offset = 40; offset < 68; offset += 4) {
        if (!std::isfinite(read_f32(bytes + offset))) {
            return PackageError::NonFinite;
        }
    }
    if (read_f32(bytes + 40) <= 0.0f) {
        return PackageError::Grid;
    }
    const std::uint32_t vertex_offset = read_u32(bytes + 68);
    const std::uint32_t vertex_stride = read_u32(bytes + 72);
    const std::uint32_t triangle_offset = read_u32(bytes + 76);
    const std::uint32_t triangle_stride = read_u32(bytes + 80);
    const std::uint32_t cell_offset = read_u32(bytes + 84);
    const std::uint32_t cell_stride = read_u32(bytes + 88);
    const std::uint32_t reference_offset = read_u32(bytes + 92);
    const std::uint32_t reference_stride = read_u32(bytes + 96);
    if (vertex_stride != 12 || triangle_stride != 32 ||
        cell_stride != 16 || reference_stride != 2 ||
        !range_valid(vertex_offset, vertices, vertex_stride, size) ||
        !range_valid(triangle_offset, triangles, triangle_stride, size) ||
        !range_valid(cell_offset, cells, cell_stride, size) ||
        !range_valid(
            reference_offset, references, reference_stride, size)) {
        return PackageError::Range;
    }
    for (std::uint32_t index = 0; index < vertices; ++index) {
        const std::uint8_t* vertex =
            bytes + vertex_offset + index * vertex_stride;
        if (!std::isfinite(read_f32(vertex)) ||
            !std::isfinite(read_f32(vertex + 4)) ||
            !std::isfinite(read_f32(vertex + 8))) {
            return PackageError::NonFinite;
        }
    }
    for (std::uint32_t index = 0; index < triangles; ++index) {
        const std::uint8_t* triangle =
            bytes + triangle_offset + index * triangle_stride;
        if (read_u16(triangle) >= vertices ||
            read_u16(triangle + 2) >= vertices ||
            read_u16(triangle + 4) >= vertices) {
            return PackageError::Index;
        }
        for (std::uint32_t component = 8; component <= 20; component += 4) {
            if (!std::isfinite(read_f32(triangle + component))) {
                return PackageError::NonFinite;
            }
        }
    }
    for (std::uint32_t index = 0; index < cells; ++index) {
        const std::uint8_t* cell =
            bytes + cell_offset + index * cell_stride;
        const std::uint32_t first = read_u32(cell);
        const std::uint32_t count = read_u32(cell + 4);
        if (first > references || count > references - first ||
            read_u32(cell + 8) >= cells_x ||
            read_u32(cell + 12) >= cells_z) {
            return PackageError::Grid;
        }
        for (std::uint32_t item = 0; item < count; ++item) {
            if (read_u16(
                    bytes + reference_offset +
                    (first + item) * reference_stride) >= triangles) {
                return PackageError::Grid;
            }
        }
    }
    return PackageError::Ok;
}

PackageError validate_dpsc(
    const void* source, std::uint32_t size, PackageView* view) {
    const PackageError base = validate_base(source, size, "DPSC", view, 4);
    if (base != PackageError::Ok) {
        return base;
    }
    const std::uint8_t* bytes = view->bytes;
    const std::uint16_t version = read_u16(bytes + 4);
    if (read_u16(bytes + 6) != 256 || size < 256 ||
        read_u32(bytes + 20) > 63 ||
        read_u32(bytes + 24) > 15 ||
        read_u32(bytes + 28) == 0 ||
        read_u32(bytes + 32) == 0 ||
        read_u32(bytes + 36) == 0) {
        return PackageError::Scene;
    }
    for (std::uint32_t offset = 40; offset <= 48; offset += 4) {
        if (!std::isfinite(read_f32(bytes + offset))) {
            return PackageError::NonFinite;
        }
    }
    if (read_u32(bytes + 60) != 1) {
        return PackageError::Spawn;
    }
    for (std::uint32_t offset = 96; offset <= 116; offset += 4) {
        if (!std::isfinite(read_f32(bytes + offset))) {
            return PackageError::NonFinite;
        }
    }
    const float spawn_x = read_f32(bytes + 40);
    const float spawn_y = read_f32(bytes + 44);
    const float spawn_z = read_f32(bytes + 48);
    if (spawn_x < read_f32(bytes + 96) ||
        spawn_y < read_f32(bytes + 100) ||
        spawn_z < read_f32(bytes + 104) ||
        spawn_x > read_f32(bytes + 108) ||
        spawn_y > read_f32(bytes + 112) ||
        spawn_z > read_f32(bytes + 116)) {
        return PackageError::Spawn;
    }
    const std::uint32_t rubies = read_u32(bytes + 120);
    const std::uint32_t ruby_offset = read_u32(bytes + 124);
    const std::uint32_t exits = read_u32(bytes + 128);
    const std::uint32_t exit_offset = read_u32(bytes + 132);
    const std::uint32_t actors = read_u32(bytes + 136);
    const std::uint32_t actor_offset = read_u32(bytes + 140);
    const std::uint32_t actor_stride =
        version >= 2 ? read_u32(bytes + 168) : 32;
    if (rubies != 5 ||
        !range_valid(ruby_offset, rubies, 16, size) ||
        !range_valid(exit_offset, exits, 32, size) ||
        actors > (version >= 3 ? 1024u : 16u) ||
        (version >= 2 && actor_stride != 64) ||
        !range_valid(actor_offset, actors, actor_stride, size)) {
        return PackageError::Range;
    }
    for (std::uint32_t index = 0; index < rubies; ++index) {
        const std::uint8_t* ruby = bytes + ruby_offset + index * 16;
        if (!std::isfinite(read_f32(ruby)) ||
            !std::isfinite(read_f32(ruby + 4)) ||
            !std::isfinite(read_f32(ruby + 8))) {
            return PackageError::NonFinite;
        }
    }
    bool source_indices[1024] = {};
    std::uint32_t supported = 0;
    for (std::uint32_t index = 0; index < actors; ++index) {
        const std::uint8_t* actor =
            bytes + actor_offset + index * actor_stride;
        const std::uint32_t position_offset = version >= 2 ? 20 : 12;
        if (!std::isfinite(read_f32(actor + position_offset)) ||
            !std::isfinite(read_f32(actor + position_offset + 4)) ||
            !std::isfinite(read_f32(actor + position_offset + 8))) {
            return PackageError::NonFinite;
        }
        if (version == 1) {
            continue;
        }
        if (!std::isfinite(read_f32(actor + 44)) ||
            !std::isfinite(read_f32(actor + 48)) ||
            !std::isfinite(read_f32(actor + 52)) ||
            read_f32(actor + 44) <= 0.0f ||
            read_f32(actor + 48) <= 0.0f ||
            read_f32(actor + 52) <= 0.0f) {
            return PackageError::NonFinite;
        }
        const std::uint16_t source_index = read_u16(actor + 42);
        if (source_index >= 1024 ||
            (version < 4 && source_indices[source_index])) {
            return PackageError::Duplicate;
        }
        if (version < 4) {
            source_indices[source_index] = true;
        }
        if (version >= 3) {
            continue;
        }
        const bool geyser = std::memcmp(actor, "geyser\0\0", 8) == 0;
        const bool camera = std::memcmp(actor, "CamArea\0", 8) == 0;
        if (!geyser && !camera) {
            return PackageError::ActorType;
        }
        if (read_u32(actor + 8) !=
                (geyser ? 0xc856c722u : 0x8647067du) ||
            read_u16(actor + 14) != 1 ||
            read_u32(actor + 56) != 0x26f1d042u) {
            return PackageError::ActorType;
        }
        const std::uint16_t process = read_u16(actor + 12);
        if ((geyser && process != 0x0167) ||
            (camera && process != 0x02cf)) {
            return PackageError::Process;
        }
        if (geyser) {
            const std::uint8_t arg0 =
                static_cast<std::uint8_t>(read_u32(actor + 16));
            if (arg0 != 0 && arg0 != 1 && arg0 != 0xff) {
                return PackageError::Parameters;
            }
            const std::int16_t rotation_x =
                static_cast<std::int16_t>(read_u16(actor + 32));
            if (actor[38] !=
                static_cast<std::uint8_t>((rotation_x >> 8) & 0x0f) ||
                actor[41] != 1) {
                return PackageError::ActorType;
            }
            ++supported;
        } else if (actor[41] != 0) {
            return PackageError::ActorType;
        }
        if (actor[39] != 2 || actor[40] != 0) {
            return PackageError::Scene;
        }
    }
    if (version == 2 &&
        (actors != 3 || supported != 2 || read_u32(bytes + 172) != 2)) {
        return PackageError::Count;
    }
    if (version >= 3) {
        const std::uint32_t exit_stride = read_u32(bytes + 156);
        const std::uint32_t trigger_stride = read_u32(bytes + 160);
        const std::uint32_t spawn_stride = read_u32(bytes + 164);
        const std::uint32_t typed_exit_offset = read_u32(bytes + 208);
        const std::uint32_t typed_exits = read_u32(bytes + 212);
        const std::uint32_t trigger_offset = read_u32(bytes + 216);
        const std::uint32_t triggers = read_u32(bytes + 220);
        const std::uint32_t spawn_offset = read_u32(bytes + 224);
        const std::uint32_t spawns = read_u32(bytes + 228);
        if (exit_stride != 64 || trigger_stride != 80 ||
            spawn_stride != 64 || typed_exits == 0 || typed_exits > 64 ||
            triggers > 64 ||
            spawns == 0 || spawns > 256 ||
            read_u32(bytes + 232) != typed_exits ||
            read_u32(bytes + 236) == 0 ||
            read_u32(bytes + 240) == 0 ||
            !range_valid(
                typed_exit_offset, typed_exits, exit_stride, size) ||
            (triggers != 0 &&
             !range_valid(
                 trigger_offset, triggers, trigger_stride, size)) ||
            !range_valid(
                spawn_offset, spawns, spawn_stride, size)) {
            return PackageError::Range;
        }
        bool exit_indices[64] = {};
        bool spawn_indices[256] = {};
        for (std::uint32_t index = 0; index < typed_exits; ++index) {
            const std::uint8_t* exit =
                bytes + typed_exit_offset + index * exit_stride;
            const std::uint16_t source_index = read_u16(exit);
            if (source_index >= typed_exits ||
                exit_indices[source_index] ||
                static_cast<std::int8_t>(exit[2]) < -1 ||
                static_cast<std::int8_t>(exit[2]) > 63 ||
                (static_cast<std::int8_t>(exit[3]) < -1 ||
                 static_cast<std::int8_t>(exit[3]) > 14)) {
                return PackageError::Duplicate;
            }
            exit_indices[source_index] = true;
            bool terminated = false;
            for (std::uint32_t character = 0; character < 8; ++character) {
                const std::uint8_t value = exit[12 + character];
                if (value == 0) {
                    terminated = true;
                    continue;
                }
                if (terminated ||
                    !((value >= 'A' && value <= 'Z') ||
                      (value >= '0' && value <= '9') ||
                      value == '_')) {
                    return PackageError::Scene;
                }
            }
            if (exit[12] == 0 ||
                (read_u16(exit + 8) != 0xffff &&
                 read_u16(exit + 8) >= triggers)) {
                return PackageError::Scene;
            }
        }
        for (std::uint32_t index = 0; index < triggers; ++index) {
            const std::uint8_t* trigger =
                bytes + trigger_offset + index * trigger_stride;
            for (std::uint32_t component = 12;
                 component <= 52; component += 4) {
                if (component == 24 || component == 28) {
                    continue;
                }
                if (!std::isfinite(read_f32(trigger + component))) {
                    return PackageError::NonFinite;
                }
            }
            if (trigger[57] >= typed_exits ||
                read_u16(trigger + 2) != 0x030c ||
                read_u16(trigger + 30) != 1 ||
                read_f32(trigger + 44) <= 0.0f ||
                read_f32(trigger + 48) <= 0.0f ||
                read_f32(trigger + 52) <= 0.0f) {
                return PackageError::Scene;
            }
        }
        for (std::uint32_t index = 0; index < spawns; ++index) {
            const std::uint8_t* spawn =
                bytes + spawn_offset + index * spawn_stride;
            if (spawn_indices[spawn[0]]) {
                return PackageError::Duplicate;
            }
            spawn_indices[spawn[0]] = true;
            for (std::uint32_t component : {4u, 8u, 12u, 32u, 36u, 40u, 44u}) {
                if (!std::isfinite(read_f32(spawn + component))) {
                    return PackageError::NonFinite;
                }
            }
        }
    }
    if (version >= 4) {
        const std::uint32_t environment_offset = read_u32(bytes + 248);
        const std::uint32_t environments = read_u32(bytes + 252);
        if (environments != 1 ||
            !range_valid(environment_offset, environments, 128, size)) {
            return PackageError::Range;
        }
        const std::uint8_t* environment = bytes + environment_offset;
        if (std::memcmp(environment, "DENV", 4) != 0 ||
            read_u16(environment + 4) != 1 ||
            read_u16(environment + 6) != 128 ||
            read_u32(environment + 8) != read_u32(bytes + 16) ||
            read_u32(environment + 12) != read_u32(bytes + 20) ||
            read_u32(environment + 12) > 63 ||
            environment[18] >= 64 ||
            environment[19] >= 8 ||
            read_u32(environment + 24) == 0 ||
            read_u32(environment + 24) > 0x0fu) {
            return PackageError::Scene;
        }
        for (std::uint32_t offset = 52; offset <= 104; offset += 4) {
            if (!std::isfinite(read_f32(environment + offset))) {
                return PackageError::NonFinite;
            }
        }
        const float key_length =
            read_f32(environment + 52) * read_f32(environment + 52) +
            read_f32(environment + 56) * read_f32(environment + 56) +
            read_f32(environment + 60) * read_f32(environment + 60);
        const float shadow_length =
            read_f32(environment + 92) * read_f32(environment + 92) +
            read_f32(environment + 96) * read_f32(environment + 96) +
            read_f32(environment + 100) * read_f32(environment + 100);
        if (key_length < 0.5f || key_length > 1.5f ||
            shadow_length < 0.5f || shadow_length > 1.5f ||
            read_f32(environment + 76) < 0.0f ||
            read_f32(environment + 84) <= read_f32(environment + 80) ||
            read_f32(environment + 88) < 0.0f ||
            read_f32(environment + 88) > 1.0f ||
            read_u32(environment + 108) > 30) {
            return PackageError::Scene;
        }
    }
    return PackageError::Ok;
}

PackageError read_dpsc_exit_v3(
    const PackageView& view,
    std::uint32_t index,
    SceneExitV3* result) {
    if (result == nullptr || view.bytes == nullptr ||
        view.size < kHeaderSize ||
        read_u16(view.bytes + 4) < 3 ||
        index >= read_u32(view.bytes + 212)) {
        return PackageError::Range;
    }
    const std::uint8_t* item =
        view.bytes + read_u32(view.bytes + 208) + index * 64;
    result->source_exit_index = read_u16(item);
    std::memcpy(result->destination_stage, item + 12, 8);
    result->destination_stage[8] = '\0';
    result->destination_room = static_cast<std::int8_t>(item[2]);
    result->destination_layer = static_cast<std::int8_t>(item[3]);
    result->destination_start = item[4];
    result->wipe = item[5];
    result->source_flags = read_u16(item + 6);
    result->trigger_index = read_u16(item + 8);
    result->return_exit_index = read_u16(item + 10);
    return PackageError::Ok;
}

PackageError read_dpsc_trigger_v3(
    const PackageView& view,
    std::uint32_t index,
    SceneTriggerV3* result) {
    if (result == nullptr || view.bytes == nullptr ||
        view.size < kHeaderSize ||
        read_u16(view.bytes + 4) < 3 ||
        index >= read_u32(view.bytes + 220)) {
        return PackageError::Range;
    }
    const std::uint8_t* item =
        view.bytes + read_u32(view.bytes + 216) + index * 80;
    result->source_type = read_u16(item);
    result->process_id = read_u16(item + 2);
    result->name_hash = read_u32(item + 4);
    result->parameters = read_u32(item + 8);
    for (std::uint32_t component = 0; component < 3; ++component) {
        result->position[component] =
            read_f32(item + 12 + component * 4);
        result->rotation[component] = static_cast<std::int16_t>(
            read_u16(item + 24 + component * 2));
        result->source_scale[component] =
            read_f32(item + 32 + component * 4);
        result->dimensions[component] =
            read_f32(item + 44 + component * 4);
    }
    result->shape = read_u16(item + 30);
    result->automatic = item[56];
    result->exit_index = item[57];
    result->visual_fallback = item[58];
    result->logic_fallback = item[59];
    result->flags = read_u32(item + 60);
    return PackageError::Ok;
}

PackageError read_dpsc_spawn_v3(
    const PackageView& view,
    std::uint32_t index,
    SceneSpawnV3* result) {
    if (result == nullptr || view.bytes == nullptr ||
        view.size < kHeaderSize ||
        read_u16(view.bytes + 4) < 3 ||
        index >= read_u32(view.bytes + 228)) {
        return PackageError::Range;
    }
    const std::uint8_t* item =
        view.bytes + read_u32(view.bytes + 224) + index * 64;
    result->start_index = item[0];
    result->type = item[1];
    for (std::uint32_t component = 0; component < 3; ++component) {
        result->position[component] =
            read_f32(item + 4 + component * 4);
        result->rotation[component] = static_cast<std::int16_t>(
            read_u16(item + 16 + component * 2));
        result->floor_normal[component] =
            read_f32(item + 36 + component * 4);
    }
    result->layer = static_cast<std::int8_t>(item[22]);
    result->parameters = read_u32(item + 24);
    result->floor_valid = item[28] != 0;
    result->floor_height = read_f32(item + 32);
    return PackageError::Ok;
}

PackageError find_dpsc_spawn_v3(
    const PackageView& view,
    std::uint8_t start_index,
    SceneSpawnV3* spawn) {
    if (view.bytes == nullptr || view.size < kHeaderSize ||
        read_u16(view.bytes + 4) < 3) {
        return PackageError::Version;
    }
    const std::uint32_t count = read_u32(view.bytes + 228);
    for (std::uint32_t index = 0; index < count; ++index) {
        SceneSpawnV3 candidate = {};
        const PackageError error =
            read_dpsc_spawn_v3(view, index, &candidate);
        if (error != PackageError::Ok) {
            return error;
        }
        if (candidate.start_index == start_index) {
            if (spawn != nullptr) {
                *spawn = candidate;
            }
            return PackageError::Ok;
        }
    }
    return PackageError::Spawn;
}

PackageError read_dpsc_actor_v3(
    const PackageView& view,
    std::uint32_t index,
    SceneActorV3* result) {
    if (result == nullptr || view.bytes == nullptr ||
        view.size < kHeaderSize ||
        read_u16(view.bytes + 4) < 3 ||
        index >= read_u32(view.bytes + 136)) {
        return PackageError::Range;
    }
    const std::uint8_t* item =
        view.bytes + read_u32(view.bytes + 140) + index * 64;
    std::memcpy(result->source_name, item, 8);
    result->source_name[8] = '\0';
    result->name_hash = read_u32(item + 8);
    result->process_id = read_u16(item + 12);
    result->mapping_version = read_u16(item + 14);
    result->parameters = read_u32(item + 16);
    for (std::uint32_t component = 0; component < 3; ++component) {
        result->position[component] =
            read_f32(item + 20 + component * 4);
        result->rotation[component] = static_cast<std::int16_t>(
            read_u16(item + 32 + component * 2));
        result->scale[component] =
            read_f32(item + 44 + component * 4);
    }
    result->room = item[39];
    result->layer = item[40];
    result->supported = item[41];
    result->source_index = read_u16(item + 42);
    result->table_hash = read_u32(item + 56);
    return PackageError::Ok;
}

PackageError read_dpsc_environment_v4(
    const PackageView& view,
    EnvironmentRecordV4* result) {
    if (result == nullptr || view.bytes == nullptr ||
        view.size < kHeaderSize ||
        read_u16(view.bytes + 4) < 4 ||
        read_u32(view.bytes + 252) != 1) {
        return PackageError::Version;
    }
    const std::uint8_t* item =
        view.bytes + read_u32(view.bytes + 248);
    result->stage_hash = read_u32(item + 8);
    result->room_index = read_u32(item + 12);
    result->environment_id = read_u16(item + 16);
    result->pattern = item[18];
    result->schedule_slot = item[19];
    result->pselect_id = read_u16(item + 20);
    result->palette_id = read_u16(item + 22);
    result->flags = read_u32(item + 24);
    result->ambient_room = read_u32(item + 28);
    result->ambient_actor = read_u32(item + 32);
    result->key_light_color = read_u32(item + 36);
    result->fog_color = read_u32(item + 40);
    result->clear_color = read_u32(item + 44);
    result->local_light_color = read_u32(item + 48);
    for (std::uint32_t component = 0; component < 3; ++component) {
        result->key_light_direction[component] =
            read_f32(item + 52 + component * 4);
        result->local_light_position[component] =
            read_f32(item + 64 + component * 4);
        result->shadow_direction[component] =
            read_f32(item + 92 + component * 4);
    }
    result->local_light_power = read_f32(item + 76);
    result->fog_near = read_f32(item + 80);
    result->fog_far = read_f32(item + 84);
    result->shadow_density = read_f32(item + 88);
    result->transition_rate = read_f32(item + 104);
    result->local_light_count = read_u32(item + 108);
    result->source_counts = read_u32(item + 112);
    return PackageError::Ok;
}

const char* package_error_name(PackageError error) {
    static constexpr const char* names[] = {
        "ok", "missing", "truncated", "magic", "version", "size",
        "crc", "count", "range", "layout", "index", "material",
        "texture", "bucket", "non_finite", "edram_budget",
        "collision", "grid",
        "scene", "spawn", "actor_type", "process", "parameters",
        "duplicate", "capacity",
    };
    const std::uint32_t index = static_cast<std::uint32_t>(error);
    return index < sizeof(names) / sizeof(names[0])
        ? names[index] : "unknown";
}

}  // namespace dusk::psp::room
