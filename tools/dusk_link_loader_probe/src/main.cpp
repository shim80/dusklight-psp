#include <aurora/dvd.h>
#include <dolphin/dvd.h>
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
#include <JSystem/J3DGraphLoader/J3DModelLoader.h>
#include <JSystem/J3DGraphBase/J3DTransform.h>
#include <JSystem/JKernel/JKRExpHeap.h>
#include <JSystem/JKernel/JKRDecomp.h>
#include <JSystem/JKernel/JKRMemArchive.h>
#include <JSystem/JUtility/JUTNameTab.h>

#include <array>
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <iterator>
#include <limits>
#include <memory>
#include <set>
#include <stdexcept>
#include <string>
#include <sstream>
#include <unordered_set>
#include <unordered_map>
#include <utility>
#include <vector>

#include "playable_export.hpp"

namespace {

constexpr std::array<const char*, 2> kArchivePaths = {
    "/res/Object/Kmdl.arc",
    "/res/Object/AlAnm.arc",
};

constexpr std::array<const char*, 4> kModelNames = {
    "al.bmd",
    "al_head.bmd",
    "al_hands.bmd",
    "al_face.bmd",
};

const char* environment_or(const char* name, const char* fallback) {
    const char* value = std::getenv(name);
    return value != nullptr && value[0] != '\0' ? value : fallback;
}

bool ascii_equal_ignore_case(const char* left, const char* right) {
    while (*left != '\0' && *right != '\0') {
        const unsigned char a = static_cast<unsigned char>(*left++);
        const unsigned char b = static_cast<unsigned char>(*right++);
        const unsigned char folded_a =
            a >= 'A' && a <= 'Z' ? static_cast<unsigned char>(a + 32) : a;
        const unsigned char folded_b =
            b >= 'A' && b <= 'Z' ? static_cast<unsigned char>(b + 32) : b;
        if (folded_a != folded_b) {
            return false;
        }
    }
    return *left == *right;
}

class DvdSession {
public:
    explicit DvdSession(const char* image_path) {
        if (!aurora_dvd_open(image_path)) {
            throw std::runtime_error("unable to open disc through Aurora/Nod");
        }
    }

    DvdSession(const DvdSession&) = delete;
    DvdSession& operator=(const DvdSession&) = delete;

    ~DvdSession() {
        aurora_dvd_close();
    }
};

std::vector<std::uint8_t> read_dvd_file(const char* path) {
    DVDFileInfo file = {};
    if (!DVDOpen(path, &file)) {
        throw std::runtime_error(std::string("DVDOpen failed: ") + path);
    }
    if (file.length == 0 ||
        file.length > static_cast<std::uint32_t>(std::numeric_limits<std::int32_t>::max())) {
        DVDClose(&file);
        throw std::runtime_error(std::string("invalid archive size: ") + path);
    }

    std::vector<std::uint8_t> bytes(file.length);
    const std::int32_t read = DVDReadPrio(
        &file,
        bytes.data(),
        static_cast<std::int32_t>(bytes.size()),
        0,
        2);
    const bool closed = DVDClose(&file);
    if (read != static_cast<std::int32_t>(bytes.size()) || !closed) {
        throw std::runtime_error(std::string("DVDReadPrio failed: ") + path);
    }
    return bytes;
}

struct DvdEntry {
    std::string path;
    std::uint32_t bytes;
};

void collect_dvd_tree(
    const std::string& path,
    std::vector<DvdEntry>* files,
    bool print) {
    DVDDir directory = {};
    if (!DVDOpenDir(path.c_str(), &directory)) {
        throw std::runtime_error("DVDOpenDir failed: " + path);
    }
    DVDDirEntry entry = {};
    while (DVDReadDir(&directory, &entry)) {
        if (entry.name == nullptr ||
            std::strcmp(entry.name, ".") == 0 ||
            std::strcmp(entry.name, "..") == 0) {
            continue;
        }
        const std::string child =
            path == "/" ? path + entry.name : path + "/" + entry.name;
        if (entry.isDir) {
            collect_dvd_tree(child, files, print);
            continue;
        }
        DVDFileInfo file = {};
        if (!DVDFastOpen(static_cast<s32>(entry.entryNum), &file)) {
            DVDCloseDir(&directory);
            throw std::runtime_error("DVDFastOpen failed: " + child);
        }
        const std::uint32_t length = file.length;
        DVDClose(&file);
        if (files != nullptr) {
            files->push_back({child, length});
        }
        if (print) {
            std::cout << "DVD_FILE path=" << child
                      << " bytes=" << length << '\n';
        }
    }
    DVDCloseDir(&directory);
}

void list_dvd_tree(const std::string& path) {
    collect_dvd_tree(path, nullptr, true);
}

std::string disc_id(const DVDDiskID& id) {
    std::string value;
    value.append(id.gameName, sizeof(id.gameName));
    value.append(id.company, sizeof(id.company));
    return value;
}

std::vector<std::uint8_t> decompress_archive(
    std::vector<std::uint8_t> bytes) {
    if (bytes.size() < 16 || std::memcmp(bytes.data(), "Yaz0", 4) != 0) {
        return bytes;
    }
    const u32 expanded_size = JKRDecompExpandSize(bytes.data());
    if (expanded_size == 0) {
        throw std::runtime_error("invalid Yaz0 expanded size");
    }
    std::vector<std::uint8_t> expanded(expanded_size);
    JKRDecomp::decode(
        bytes.data(),
        expanded.data(),
        expanded_size,
        0);
    if (expanded.size() < 4 ||
        std::memcmp(expanded.data(), "RARC", 4) != 0) {
        throw std::runtime_error("Yaz0 payload is not RARC");
    }
    return expanded;
}

void validate_rarc(
    const std::vector<std::uint8_t>& archive,
    const char* label);

std::vector<std::uint8_t> copy_expanded_resource(
    JKRMemArchive& archive,
    const char* name) {
    void* source = nullptr;
    for (std::uint32_t index = 0;
         index < archive.countFile(); ++index) {
        JKRArchive::SDirEntry entry = {};
        if (archive.getDirEntry(&entry, index) &&
            entry.name != nullptr &&
            std::strcmp(entry.name, name) == 0) {
            source = archive.getResource(entry.id);
            break;
        }
    }
    if (source == nullptr) {
        throw std::runtime_error(
            std::string("RARC resource missing: ") + name);
    }
    const u32 stored_size = archive.getResSize(source);
    if (stored_size < 4 ||
        std::memcmp(source, "Yaz0", 4) != 0) {
        return std::vector<std::uint8_t>(
            static_cast<const std::uint8_t*>(source),
            static_cast<const std::uint8_t*>(source) + stored_size);
    }
    auto* compressed = static_cast<std::uint8_t*>(source);
    const u32 expanded_size = JKRDecompExpandSize(compressed);
    if (expanded_size == 0) {
        throw std::runtime_error(
            std::string("invalid Yaz0 resource: ") + name);
    }
    std::vector<std::uint8_t> expanded(expanded_size);
    JKRDecomp::decode(
        compressed, expanded.data(), expanded_size, 0);
    return expanded;
}

std::vector<HudSourceResource> load_hud_resources(
    const char* archive_path,
    const std::vector<const char*>& names,
    std::vector<std::uint8_t>* layout) {
    std::vector<std::uint8_t> bytes =
        decompress_archive(read_dvd_file(archive_path));
    validate_rarc(bytes, archive_path);
    JKRMemArchive archive(
        bytes.data(), static_cast<u32>(bytes.size()),
        JKRMEMBREAK_FLAG_UNKNOWN0);
    std::vector<HudSourceResource> resources;
    for (const char* name : names) {
        std::vector<std::uint8_t> resource =
            copy_expanded_resource(archive, name);
        if (layout != nullptr &&
            std::strcmp(name, "zelda_game_image.blo") == 0) {
            *layout = std::move(resource);
        } else {
            resources.push_back({name, std::move(resource)});
        }
    }
    return resources;
}

void export_original_hud_from_disc() {
    std::vector<std::uint8_t> layout;
    const std::vector<const char*> main_names = {
        "zelda_game_image.blo",
        "tt_heart_00.bti", "tt_heart_01.bti",
        "tt_heart_02.bti", "tt_heart_03.bti",
        "tt_rupy_green_icon2.bti",
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
        "tt_zelda_button_ab_maru.bti",
        "tt_select_square_4i_00.bti",
    };
    std::vector<HudSourceResource> resources =
        load_hud_resources(
            "/res/Layout/main2D.arc", main_names, &layout);
    const std::vector<const char*> pause_names = {
        "tt_horiwaku_lu.bti",
        "tt_horiwaku_top_rr.bti",
        "im_newwindow_try03_02_64x16_gre.bti",
    };
    std::vector<HudSourceResource> pause =
        load_hud_resources(
            "/res/Layout/clctres.arc", pause_names, nullptr);
    resources.insert(
        resources.end(),
        std::make_move_iterator(pause.begin()),
        std::make_move_iterator(pause.end()));
    const std::vector<const char*> font_names = {
        "rodan_b_24_22.bfn",
    };
    std::vector<HudSourceResource> fonts =
        load_hud_resources(
            "/res/Fonteu/fontres.arc", font_names, nullptr);
    if (fonts.size() != 1) {
        throw std::runtime_error("HUD source font inventory invalid");
    }
    export_original_hud_package(layout, resources, fonts[0].bytes);
}

u16 read_be16(const u8* data) {
    return static_cast<u16>((data[0] << 8) | data[1]);
}

u32 read_be32(const u8* data) {
    return (static_cast<u32>(data[0]) << 24) |
           (static_cast<u32>(data[1]) << 16) |
           (static_cast<u32>(data[2]) << 8) |
           static_cast<u32>(data[3]);
}

void validate_rarc(
    const std::vector<std::uint8_t>& archive,
    const char* label) {
    if (archive.size() < 0x40 ||
        std::memcmp(archive.data(), "RARC", 4) != 0) {
        throw std::runtime_error(std::string("invalid RARC header: ") + label);
    }
    const u32 file_length = read_be32(archive.data() + 4);
    const u32 header_length = read_be32(archive.data() + 8);
    const u32 data_offset = read_be32(archive.data() + 0x0c);
    const u32 data_length = read_be32(archive.data() + 0x10);
    if (file_length > archive.size() ||
        header_length > archive.size() ||
        data_offset > archive.size() - header_length ||
        data_length > archive.size() - header_length - data_offset ||
        header_length + 0x20 > archive.size()) {
        throw std::runtime_error(std::string("RARC range outside file: ") + label);
    }
    const u8* info = archive.data() + header_length;
    const u32 node_count = read_be32(info);
    const u32 node_offset = read_be32(info + 4);
    const u32 file_count = read_be32(info + 8);
    const u32 file_offset = read_be32(info + 0x0c);
    const u32 string_length = read_be32(info + 0x10);
    const u32 string_offset = read_be32(info + 0x14);
    const u64 metadata_size = data_offset;
    if (static_cast<u64>(node_offset) + node_count * 0x10 > metadata_size ||
        static_cast<u64>(file_offset) + file_count * 0x14 > metadata_size ||
        static_cast<u64>(string_offset) + string_length > metadata_size) {
        throw std::runtime_error(
            std::string("RARC metadata outside file: ") + label);
    }
}

bool has_suffix(const std::string& text, const char* suffix) {
    const std::size_t length = std::strlen(suffix);
    return text.size() >= length &&
           text.compare(text.size() - length, length, suffix) == 0;
}

struct RoomModelSummary {
    std::uint64_t triangles = 0;
    std::uint64_t degenerate_triangles = 0;
    std::uint64_t runtime_vertices = 0;
    std::uint64_t texture_psp_bytes = 0;
    std::uint32_t packets = 0;
    std::uint32_t materials = 0;
    std::uint32_t textures = 0;
};

RoomModelSummary summarize_room_model(
    const void* resource, std::uint32_t bytes);

struct CollisionSummary {
    std::uint32_t vertices = 0;
    std::uint32_t normals = 0;
    std::uint32_t triangles = 0;
    Vec minimum = {};
    Vec maximum = {};
    bool valid = false;
};

float read_be_float(const std::uint8_t* data) {
    const std::uint32_t bits = read_be32(data);
    float value = 0.0f;
    std::memcpy(&value, &bits, sizeof(value));
    return value;
}

CollisionSummary summarize_kcl(
    const std::uint8_t* bytes, std::uint32_t size) {
    CollisionSummary result;
    if (bytes == nullptr || size < 0x38) {
        return result;
    }
    const std::uint32_t positions = read_be32(bytes);
    const std::uint32_t normals = read_be32(bytes + 4);
    const std::uint32_t prisms = read_be32(bytes + 8);
    const std::uint32_t blocks = read_be32(bytes + 12);
    if (positions < 0x38 || positions > normals ||
        normals > prisms || prisms > blocks || blocks > size ||
        (normals - positions) % 12 != 0 ||
        (blocks - prisms) % 16 != 0) {
        return result;
    }
    result.vertices = (normals - positions) / 12;
    // The normal table is padded before the 16-byte-aligned prism table.
    result.normals = (prisms - normals) / 12;
    const std::uint32_t prism_records = (blocks - prisms) / 16;
    result.triangles = prism_records == 0 ? 0 : prism_records - 1;
    if (result.vertices == 0 || result.triangles == 0) {
        return result;
    }
    const float infinity = std::numeric_limits<float>::infinity();
    result.minimum = {infinity, infinity, infinity};
    result.maximum = {-infinity, -infinity, -infinity};
    for (std::uint32_t index = 0; index < result.vertices; ++index) {
        const std::uint8_t* position = bytes + positions + index * 12;
        const Vec value = {
            read_be_float(position),
            read_be_float(position + 4),
            read_be_float(position + 8),
        };
        if (!std::isfinite(value.x) ||
            !std::isfinite(value.y) ||
            !std::isfinite(value.z)) {
            return {};
        }
        result.minimum.x = std::min(result.minimum.x, value.x);
        result.minimum.y = std::min(result.minimum.y, value.y);
        result.minimum.z = std::min(result.minimum.z, value.z);
        result.maximum.x = std::max(result.maximum.x, value.x);
        result.maximum.y = std::max(result.maximum.y, value.y);
        result.maximum.z = std::max(result.maximum.z, value.z);
    }
    result.valid = true;
    return result;
}

Vec cross_vec(const Vec& left, const Vec& right) {
    return {
        left.y * right.z - left.z * right.y,
        left.z * right.x - left.x * right.z,
        left.x * right.y - left.y * right.x,
    };
}

float dot_vec(const Vec& left, const Vec& right) {
    return left.x * right.x + left.y * right.y + left.z * right.z;
}

bool reconstruct_kcl_triangle(
    const std::uint8_t* bytes,
    std::uint32_t size,
    std::uint32_t triangle_index,
    Vec* first,
    Vec* second,
    Vec* third,
    Vec* face_normal) {
    const CollisionSummary summary = summarize_kcl(bytes, size);
    if (!summary.valid || triangle_index >= summary.triangles) {
        return false;
    }
    const std::uint32_t positions = read_be32(bytes);
    const std::uint32_t normals = read_be32(bytes + 4);
    const std::uint32_t prisms = read_be32(bytes + 8);
    const std::uint8_t* prism =
        bytes + prisms + (triangle_index + 1) * 16;
    const float height = read_be_float(prism);
    const std::uint16_t position_index = read_be16(prism + 4);
    const std::uint16_t face_index = read_be16(prism + 6);
    const std::uint16_t edge1_index = read_be16(prism + 8);
    const std::uint16_t edge2_index = read_be16(prism + 10);
    const std::uint16_t edge3_index = read_be16(prism + 12);
    if (position_index >= summary.vertices ||
        face_index >= summary.normals ||
        edge1_index >= summary.normals ||
        edge2_index >= summary.normals ||
        edge3_index >= summary.normals) {
        return false;
    }
    auto read_vec = [&](std::uint32_t offset, std::uint32_t index) {
        const std::uint8_t* value = bytes + offset + index * 12;
        return Vec{
            read_be_float(value),
            read_be_float(value + 4),
            read_be_float(value + 8),
        };
    };
    *first = read_vec(positions, position_index);
    *face_normal = read_vec(normals, face_index);
    const Vec edge1 = read_vec(normals, edge1_index);
    const Vec edge2 = read_vec(normals, edge2_index);
    const Vec edge3 = read_vec(normals, edge3_index);
    const Vec first_direction = cross_vec(*face_normal, edge1);
    const Vec second_direction = cross_vec(edge2, *face_normal);
    const float first_denominator = dot_vec(first_direction, edge3);
    const float second_denominator = dot_vec(second_direction, edge3);
    if (!std::isfinite(height) ||
        std::fabs(first_denominator) < 1.0e-6f ||
        std::fabs(second_denominator) < 1.0e-6f) {
        return false;
    }
    const float first_scale = height / first_denominator;
    const float second_scale = height / second_denominator;
    *third = {
        first->x + first_direction.x * first_scale,
        first->y + first_direction.y * first_scale,
        first->z + first_direction.z * first_scale,
    };
    *second = {
        first->x + second_direction.x * second_scale,
        first->y + second_direction.y * second_scale,
        first->z + second_direction.z * second_scale,
    };
    return std::isfinite(second->x) &&
           std::isfinite(second->y) &&
           std::isfinite(second->z) &&
           std::isfinite(third->x) &&
           std::isfinite(third->y) &&
           std::isfinite(third->z);
}

void print_exit_collision_geometry(
    const std::uint8_t* kcl,
    std::uint32_t kcl_size,
    const std::uint8_t* plc,
    std::uint32_t plc_size,
    const std::string& stage,
    const std::string& room,
    const std::string& layer) {
    const CollisionSummary summary = summarize_kcl(kcl, kcl_size);
    if (!summary.valid || plc == nullptr || plc_size < 8) {
        return;
    }
    const std::uint16_t code_size = read_be16(plc + 4);
    const std::uint16_t attribute_count = read_be16(plc + 6);
    if (read_be32(plc) != 0x53504c43 ||
        code_size != 0x14 ||
        attribute_count > (plc_size - 8) / code_size) {
        return;
    }
    const std::uint32_t prisms = read_be32(kcl + 8);
    std::array<std::uint32_t, 64> counts = {};
    std::array<Vec, 64> minimum = {};
    std::array<Vec, 64> maximum = {};
    const float infinity = std::numeric_limits<float>::infinity();
    for (std::size_t index = 0; index < counts.size(); ++index) {
        minimum[index] = {infinity, infinity, infinity};
        maximum[index] = {-infinity, -infinity, -infinity};
    }
    for (std::uint32_t index = 0; index < summary.triangles; ++index) {
        const std::uint8_t* prism = kcl + prisms + (index + 1) * 16;
        const std::uint16_t attribute = read_be16(prism + 14);
        if (attribute >= attribute_count) {
            continue;
        }
        const std::uint32_t code0 =
            read_be32(plc + 8 + attribute * code_size);
        const std::uint32_t exit_index = code0 & 0x3f;
        if (exit_index == 0x3f) {
            continue;
        }
        Vec points[3] = {};
        Vec normal = {};
        if (!reconstruct_kcl_triangle(
                kcl, kcl_size, index,
                &points[0], &points[1], &points[2], &normal)) {
            continue;
        }
        for (const Vec& point : points) {
            minimum[exit_index].x =
                std::min(minimum[exit_index].x, point.x);
            minimum[exit_index].y =
                std::min(minimum[exit_index].y, point.y);
            minimum[exit_index].z =
                std::min(minimum[exit_index].z, point.z);
            maximum[exit_index].x =
                std::max(maximum[exit_index].x, point.x);
            maximum[exit_index].y =
                std::max(maximum[exit_index].y, point.y);
            maximum[exit_index].z =
                std::max(maximum[exit_index].z, point.z);
        }
        ++counts[exit_index];
    }
    for (std::size_t exit_index = 0; exit_index < counts.size();
         ++exit_index) {
        if (counts[exit_index] == 0) {
            continue;
        }
        std::cout << "ROOM_EXIT_GEOMETRY"
                  << " stage=" << stage
                  << " room=" << room
                  << " layer=" << layer
                  << " exit=" << exit_index
                  << " source=room.plc/kcl"
                  << " triangles=" << counts[exit_index]
                  << " bounds="
                  << minimum[exit_index].x << ','
                  << minimum[exit_index].y << ','
                  << minimum[exit_index].z << ':'
                  << maximum[exit_index].x << ','
                  << maximum[exit_index].y << ','
                  << maximum[exit_index].z
                  << '\n';
    }
}

bool kcl_floor_at(
    const std::uint8_t* bytes,
    std::uint32_t size,
    const Vec& position,
    float* floor_height) {
    const CollisionSummary summary = summarize_kcl(bytes, size);
    bool found = false;
    float best = -std::numeric_limits<float>::infinity();
    for (std::uint32_t index = 0; index < summary.triangles; ++index) {
        Vec a = {};
        Vec b = {};
        Vec c = {};
        Vec normal = {};
        if (!reconstruct_kcl_triangle(
                bytes, size, index, &a, &b, &c, &normal) ||
            std::fabs(normal.y) < 0.5f) {
            continue;
        }
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
        const float height = wa * a.y + wb * b.y + wc * c.y;
        if (height <= position.y + 200.0f && height > best) {
            best = height;
            found = true;
        }
    }
    if (found) {
        *floor_height = best;
    }
    return found;
}

struct PlacementSummary {
    std::uint32_t players = 0;
    std::uint32_t exits = 0;
    std::uint32_t actors = 0;
    Vec spawn = {};
    std::int16_t spawn_yaw = 0;
    std::uint32_t spawn_parameters = 0;
    std::string spawn_name;
    bool spawn_valid = false;
};

std::string fixed_ascii(const std::uint8_t* bytes, std::size_t size) {
    std::size_t length = 0;
    while (length < size && bytes[length] != 0) {
        ++length;
    }
    return std::string(
        reinterpret_cast<const char*>(bytes),
        length);
}

void print_placement_details(
    const std::uint8_t* bytes,
    std::uint32_t size,
    const std::string& stage,
    const std::string& room,
    const std::string& layer) {
    if (bytes == nullptr || size < 4) {
        return;
    }
    const std::uint32_t chunks = read_be32(bytes);
    if (chunks > 128 || chunks > (size - 4) / 12) {
        return;
    }
    for (std::uint32_t chunk = 0; chunk < chunks; ++chunk) {
        const std::uint8_t* node = bytes + 4 + chunk * 12;
        const std::string tag(
            reinterpret_cast<const char*>(node), 4);
        const std::uint32_t count = read_be32(node + 4);
        const std::uint32_t offset = read_be32(node + 8);
        if (offset > size) {
            return;
        }
        const bool player =
            tag == "PLYR" ||
            (tag.size() == 4 && tag.compare(0, 3, "PLY") == 0);
        const bool scene_exit =
            tag == "SCLS" ||
            (tag.size() == 4 && tag.compare(0, 3, "SCL") == 0);
        const bool actor =
            tag == "ACTR" || tag == "TGOB" || tag == "TRES" ||
            tag == "TGSC" || tag == "SCOB" || tag == "TGDR" ||
            (tag.size() == 4 &&
             (tag.compare(0, 3, "ACT") == 0 ||
              tag.compare(0, 3, "TGO") == 0 ||
              tag.compare(0, 3, "TRE") == 0 ||
              tag.compare(0, 3, "SCO") == 0));
        if (scene_exit) {
            if (count > (size - offset) / 13) {
                return;
            }
            for (std::uint32_t index = 0; index < count; ++index) {
                const std::uint8_t* entry = bytes + offset + index * 13;
                const std::uint8_t packed_a = entry[10];
                const std::uint8_t packed_b = entry[11];
                std::cout
                    << "ROOM_SCLS"
                    << " stage=" << stage
                    << " room=" << room
                    << " layer=" << layer
                    << " table=" << tag
                    << " index=" << index
                    << " destination_stage=" << fixed_ascii(entry, 8)
                    << " destination_start="
                    << static_cast<unsigned>(entry[8])
                    << " destination_room="
                    << static_cast<int>(static_cast<std::int8_t>(entry[9]))
                    << " destination_layer="
                    << static_cast<unsigned>(packed_b & 0x0f)
                    << " time_h="
                    << static_cast<unsigned>(
                           ((packed_a >> 4) & 0x0f) |
                           (packed_b & 0x10))
                    << " wipe_time="
                    << static_cast<unsigned>((packed_b >> 5) & 7)
                    << " wipe=" << static_cast<unsigned>(entry[12])
                    << " raw_a=0x" << std::hex
                    << static_cast<unsigned>(packed_a)
                    << " raw_b=0x" << static_cast<unsigned>(packed_b)
                    << std::dec << '\n';
            }
        } else if (player) {
            if (count > (size - offset) / 32) {
                return;
            }
            for (std::uint32_t index = 0; index < count; ++index) {
                const std::uint8_t* entry = bytes + offset + index * 32;
                std::cout
                    << "ROOM_SPAWN"
                    << " stage=" << stage
                    << " room=" << room
                    << " layer=" << layer
                    << " table=" << tag
                    << " index=" << index
                    << " name=" << fixed_ascii(entry, 8)
                    << " params=0x" << std::hex << read_be32(entry + 8)
                    << std::dec
                    << " position=" << read_be_float(entry + 12) << ','
                    << read_be_float(entry + 16) << ','
                    << read_be_float(entry + 20)
                    << " rotation="
                    << static_cast<std::int16_t>(read_be16(entry + 24)) << ','
                    << static_cast<std::int16_t>(read_be16(entry + 26)) << ','
                    << static_cast<std::int16_t>(read_be16(entry + 28))
                    << " start_index="
                    << static_cast<unsigned>(entry[29])
                    << '\n';
            }
        } else if (actor) {
            const std::uint32_t stride =
                tag == "TGSC" || tag == "TGDR" ||
                tag.compare(0, 3, "SCO") == 0
                    ? 36 : 32;
            if (count > (size - offset) / stride) {
                return;
            }
            for (std::uint32_t index = 0; index < count; ++index) {
                const std::uint8_t* entry = bytes + offset + index * stride;
                std::cout
                    << "ROOM_ACTOR"
                    << " stage=" << stage
                    << " room=" << room
                    << " layer=" << layer
                    << " table=" << tag
                    << " index=" << index
                    << " name=" << fixed_ascii(entry, 8)
                    << " params=0x" << std::hex << read_be32(entry + 8)
                    << std::dec
                    << " position=" << read_be_float(entry + 12) << ','
                    << read_be_float(entry + 16) << ','
                    << read_be_float(entry + 20)
                    << " rotation="
                    << static_cast<std::int16_t>(read_be16(entry + 24)) << ','
                    << static_cast<std::int16_t>(read_be16(entry + 26)) << ','
                    << static_cast<std::int16_t>(read_be16(entry + 28));
                if (stride == 36) {
                    std::cout
                        << " scale="
                        << static_cast<unsigned>(entry[32]) / 10.0f << ','
                        << static_cast<unsigned>(entry[33]) / 10.0f << ','
                        << static_cast<unsigned>(entry[34]) / 10.0f;
                }
                std::cout << '\n';
            }
        }
    }
}

PlacementSummary summarize_placement(
    const std::uint8_t* bytes, std::uint32_t size) {
    PlacementSummary result;
    if (bytes == nullptr || size < 4) {
        return result;
    }
    const std::uint32_t chunks = read_be32(bytes);
    if (chunks > 128 || chunks > (size - 4) / 12) {
        return result;
    }
    for (std::uint32_t index = 0; index < chunks; ++index) {
        const std::uint8_t* node = bytes + 4 + index * 12;
        const std::string tag(
            reinterpret_cast<const char*>(node), 4);
        const std::uint32_t count = read_be32(node + 4);
        const std::uint32_t offset = read_be32(node + 8);
        if (offset > size) {
            return {};
        }
        const bool player = tag == "PLYR" ||
            (tag.size() == 4 && tag.compare(0, 3, "PLY") == 0);
        const bool actor =
            tag == "ACTR" || tag == "TGOB" || tag == "TRES" ||
            tag == "TGSC" || tag == "SCOB" || tag == "TGDR" ||
            (tag.size() == 4 &&
             (tag.compare(0, 3, "ACT") == 0 ||
              tag.compare(0, 3, "TGO") == 0 ||
              tag.compare(0, 3, "TRE") == 0 ||
              tag.compare(0, 3, "SCO") == 0));
        if (player) {
            if (count > (size - offset) / 32) {
                return {};
            }
            result.players += count;
            if (!result.spawn_valid && count != 0) {
                const std::uint8_t* entry = bytes + offset;
                result.spawn_name.assign(
                    reinterpret_cast<const char*>(entry),
                    strnlen(reinterpret_cast<const char*>(entry), 8));
                result.spawn_parameters = read_be32(entry + 8);
                result.spawn = {
                    read_be_float(entry + 12),
                    read_be_float(entry + 16),
                    read_be_float(entry + 20),
                };
                result.spawn_yaw =
                    static_cast<std::int16_t>(read_be16(entry + 26));
                result.spawn_valid =
                    std::isfinite(result.spawn.x) &&
                    std::isfinite(result.spawn.y) &&
                    std::isfinite(result.spawn.z);
            }
        } else if (
            tag == "SCLS" ||
            (tag.size() == 4 && tag.compare(0, 3, "SCL") == 0)) {
            if (count > (size - offset) / 13) {
                return {};
            }
            result.exits += count;
        } else if (actor) {
            const std::uint32_t stride =
                tag == "TGSC" || tag == "SCOB" || tag == "TGDR"
                    ? 36 : 32;
            if (count > (size - offset) / stride) {
                return {};
            }
            result.actors += count;
        }
    }
    return result;
}

void inventory_room_archives() {
    std::vector<DvdEntry> disc_files;
    collect_dvd_tree("/res/Stage", &disc_files, false);
    std::sort(
        disc_files.begin(), disc_files.end(),
        [](const DvdEntry& left, const DvdEntry& right) {
            return left.bytes < right.bytes;
        });
    std::uint32_t room_count = 0;
    for (const DvdEntry& disc_file : disc_files) {
        const std::size_t slash = disc_file.path.find_last_of('/');
        if (slash == std::string::npos) {
            continue;
        }
        const std::string filename = disc_file.path.substr(slash + 1);
        if (filename.size() != 10 ||
            filename[0] != 'R' ||
            filename[3] != '_' ||
            !has_suffix(filename, ".arc")) {
            continue;
        }
        const std::size_t stage_slash =
            disc_file.path.find_last_of('/', slash - 1);
        if (stage_slash == std::string::npos) {
            continue;
        }
        const std::string stage =
            disc_file.path.substr(stage_slash + 1, slash - stage_slash - 1);
        std::vector<std::uint8_t> compressed =
            read_dvd_file(disc_file.path.c_str());
        std::vector<std::uint8_t> archive =
            decompress_archive(std::move(compressed));
        validate_rarc(archive, disc_file.path.c_str());
        JKRMemArchive mounted(
            archive.data(),
            static_cast<u32>(archive.size()),
            JKRMEMBREAK_FLAG_UNKNOWN0);
        std::vector<std::string> geometry;
        std::vector<std::string> collision;
        std::vector<std::string> placement;
        std::vector<std::string> animations;
        std::vector<std::string> other;
        std::uint64_t geometry_bytes = 0;
        std::uint64_t collision_bytes = 0;
        std::uint64_t placement_bytes = 0;
        for (u32 index = 0; index < mounted.countFile(); ++index) {
            JKRArchive::SDirEntry entry = {};
            if (!mounted.getDirEntry(&entry, index) ||
                entry.name == nullptr) {
                continue;
            }
            void* resource = entry.id != 0xffff
                ? mounted.getResource(entry.id)
                : mounted.getResource(entry.name);
            if (resource == nullptr) {
                continue;
            }
            const std::uint32_t bytes = mounted.getResSize(resource);
            const std::string name = entry.name;
            if (has_suffix(name, ".bmd") || has_suffix(name, ".bdl")) {
                geometry.push_back(name);
                geometry_bytes += bytes;
            } else if (has_suffix(name, ".dzb") ||
                       has_suffix(name, ".kcl")) {
                collision.push_back(name);
                collision_bytes += bytes;
            } else if (has_suffix(name, ".dzr") ||
                       has_suffix(name, ".dzs")) {
                placement.push_back(name);
                placement_bytes += bytes;
            } else if (has_suffix(name, ".btk") ||
                       has_suffix(name, ".brk") ||
                       has_suffix(name, ".btp")) {
                animations.push_back(name);
            } else if (name != "." && name != "..") {
                other.push_back(name);
            }
        }
        auto names = [](const std::vector<std::string>& values) {
            std::ostringstream stream;
            for (std::size_t index = 0; index < values.size(); ++index) {
                if (index != 0) {
                    stream << ',';
                }
                stream << values[index];
            }
            return stream.str();
        };
        RoomModelSummary model_summary;
        CollisionSummary collision_summary;
        PlacementSummary placement_summary;
        bool spawn_floor_valid = false;
        float spawn_floor_height = 0.0f;
        static std::uint32_t detailed = 0;
        auto resource_named = [&](const std::string& wanted) -> void* {
            for (u32 index = 0; index < mounted.countFile(); ++index) {
                JKRArchive::SDirEntry entry = {};
                if (!mounted.getDirEntry(&entry, index) ||
                    entry.name == nullptr || wanted != entry.name) {
                    continue;
                }
                return entry.id != 0xffff
                    ? mounted.getResource(entry.id)
                    : mounted.getResource(entry.name);
            }
            return nullptr;
        };
        if (!placement.empty()) {
            void* dzr = resource_named(placement.front());
            placement_summary = summarize_placement(
                static_cast<const std::uint8_t*>(dzr),
                mounted.getResSize(dzr));
            print_placement_details(
                static_cast<const std::uint8_t*>(dzr),
                mounted.getResSize(dzr),
                stage,
                filename.substr(1, 2),
                filename.substr(4, 2));
        }
        if (!collision.empty()) {
            void* kcl = resource_named(collision.front());
            void* plc = resource_named("room.plc");
            if (kcl != nullptr && plc != nullptr) {
                print_exit_collision_geometry(
                    static_cast<const std::uint8_t*>(kcl),
                    mounted.getResSize(kcl),
                    static_cast<const std::uint8_t*>(plc),
                    mounted.getResSize(plc),
                    stage,
                    filename.substr(1, 2),
                    filename.substr(4, 2));
            }
        }
        const bool inspect =
            !geometry.empty() && !collision.empty() &&
            !placement.empty() && detailed < 20;
        if (inspect) {
            for (const std::string& name : geometry) {
                void* resource = resource_named(name);
                const RoomModelSummary part = summarize_room_model(
                    resource, mounted.getResSize(resource));
                model_summary.triangles += part.triangles;
                model_summary.degenerate_triangles +=
                    part.degenerate_triangles;
                model_summary.runtime_vertices += part.runtime_vertices;
                model_summary.texture_psp_bytes += part.texture_psp_bytes;
                model_summary.packets += part.packets;
                model_summary.materials += part.materials;
                model_summary.textures += part.textures;
            }
            void* kcl = resource_named(collision.front());
            collision_summary = summarize_kcl(
                static_cast<const std::uint8_t*>(kcl),
                mounted.getResSize(kcl));
            if (placement_summary.spawn_valid) {
                spawn_floor_valid = kcl_floor_at(
                    static_cast<const std::uint8_t*>(kcl),
                    mounted.getResSize(kcl),
                    placement_summary.spawn,
                    &spawn_floor_height);
            }
            ++detailed;
        }
        const int score = inspect
            ? 1000 -
                static_cast<int>(model_summary.triangles / 100) -
                static_cast<int>(model_summary.runtime_vertices / 100) -
                static_cast<int>(model_summary.texture_psp_bytes / 4096) -
                static_cast<int>(model_summary.materials * 2) -
                static_cast<int>(animations.size() * 10) -
                static_cast<int>(placement_summary.actors * 2) +
                (collision_summary.valid ? 100 : 0) +
                (placement_summary.spawn_valid ? 100 : 0)
            : -1;
        std::cout << "ROOM_ARCHIVE"
                  << " stage=" << stage
                  << " room=" << filename.substr(1, 2)
                  << " layer=" << filename.substr(4, 2)
                  << " path=" << disc_file.path
                  << " stored_bytes=" << disc_file.bytes
                  << " expanded_bytes=" << archive.size()
                  << " file_count=" << mounted.countFile()
                  << " geometry_count=" << geometry.size()
                  << " geometry_bytes=" << geometry_bytes
                  << " geometry=" << names(geometry)
                  << " dzb_count=" << collision.size()
                  << " dzb_bytes=" << collision_bytes
                  << " dzb=" << names(collision)
                  << " placement_count=" << placement.size()
                  << " placement_bytes=" << placement_bytes
                  << " placement=" << names(placement)
                  << " material_animation_count=" << animations.size()
                  << " material_animations=" << names(animations)
                  << " other=" << names(other)
                  << " detailed=" << inspect
                  << " triangles=" << model_summary.triangles
                  << " degenerate_triangles="
                  << model_summary.degenerate_triangles
                  << " runtime_vertices=" << model_summary.runtime_vertices
                  << " packets=" << model_summary.packets
                  << " materials=" << model_summary.materials
                  << " textures=" << model_summary.textures
                  << " texture_psp_bytes="
                  << model_summary.texture_psp_bytes
                  << " collision_vertices=" << collision_summary.vertices
                  << " collision_triangles=" << collision_summary.triangles
                  << " collision_bounds="
                  << collision_summary.minimum.x << ','
                  << collision_summary.minimum.y << ','
                  << collision_summary.minimum.z << ':'
                  << collision_summary.maximum.x << ','
                  << collision_summary.maximum.y << ','
                  << collision_summary.maximum.z
                  << " players=" << placement_summary.players
                  << " spawn_valid=" << placement_summary.spawn_valid
                  << " spawn_name=" << placement_summary.spawn_name
                  << " spawn_parameters=0x" << std::hex
                  << placement_summary.spawn_parameters << std::dec
                  << " spawn=" << placement_summary.spawn.x << ','
                  << placement_summary.spawn.y << ','
                  << placement_summary.spawn.z
                  << " spawn_yaw=" << placement_summary.spawn_yaw
                  << " spawn_floor_valid=" << spawn_floor_valid
                  << " spawn_floor_height=" << spawn_floor_height
                  << " exits=" << placement_summary.exits
                  << " actors=" << placement_summary.actors
                  << " score=" << score
                  << '\n';
        ++room_count;
    }
    std::cout << "ROOM_ARCHIVE_INVENTORY_OK count=" << room_count << '\n';
}

struct GeometryMetrics {
    u32 primitive_commands = 0;
    u32 direct_triangle_commands = 0;
    u32 strip_commands = 0;
    u32 fan_commands = 0;
    u64 logical_triangles = 0;
    u64 degenerate_triangles = 0;
    u32 maximum_index = 0;
    std::unordered_set<std::string> corner_keys;
};

void measure_display_list(
    const J3DShapeDraw& draw,
    const GXVtxDescList* descriptions,
    GeometryMetrics& metrics);

void report_hand_shape_metrics(J3DModelData& model) {
    JUTNameTab* material_names = model.getMaterialName();
    for (u16 shape_index = 0; shape_index < model.getShapeNum(); ++shape_index) {
        J3DShape* shape = model.getShapeNodePointer(shape_index);
        GeometryMetrics geometry;
        std::set<u16> draw_matrices;
        std::set<u16> rigid_joints;
        for (u16 group = 0; group < shape->getMtxGroupNum(); ++group) {
            J3DShapeMtx* shape_mtx = shape->getShapeMtx(group);
            for (u16 slot = 0; slot < shape_mtx->getUseMtxNum(); ++slot) {
                const u16 draw_matrix = shape_mtx->getUseMtxIndex(slot);
                if (draw_matrix == 0xffff) {
                    continue;
                }
                draw_matrices.insert(draw_matrix);
                if (model.getDrawMtxFlag(draw_matrix) == 0) {
                    rigid_joints.insert(model.getDrawMtxIndex(draw_matrix));
                }
            }
            measure_display_list(
                *shape->getShapeDraw(group),
                shape->getVtxDesc(),
                geometry);
        }
        const Vec minimum = *shape->getMin();
        const Vec maximum = *shape->getMax();
        const Vec center = {
            (minimum.x + maximum.x) * 0.5f,
            (minimum.y + maximum.y) * 0.5f,
            (minimum.z + maximum.z) * 0.5f,
        };
        J3DMaterial* material = shape->getMaterial();
        const u16 material_index =
            material == nullptr ? 0xffff : material->getIndex();
        const char* material_name =
            material_names != nullptr && material_index != 0xffff
                ? material_names->getName(material_index)
                : nullptr;
        std::ostringstream draw_matrix_text;
        for (u16 value : draw_matrices) {
            if (draw_matrix_text.tellp() > 0) {
                draw_matrix_text << ',';
            }
            draw_matrix_text << value;
        }
        std::ostringstream rigid_joint_text;
        for (u16 value : rigid_joints) {
            if (rigid_joint_text.tellp() > 0) {
                rigid_joint_text << ',';
            }
            rigid_joint_text << value;
        }
        const char* probable_side =
            rigid_joints.size() == 1 && *rigid_joints.begin() == 1
                ? "left"
                : rigid_joints.size() == 1 && *rigid_joints.begin() == 2
                    ? "right"
                    : "undetermined";
        std::cout << "HAND_SHAPE_METRICS"
                  << " index=" << shape_index
                  << " material_index=" << material_index
                  << " name=" << (material_name != nullptr ? material_name : "unnamed")
                  << " packets=" << shape->getMtxGroupNum()
                  << " primitives=" << geometry.primitive_commands
                  << " triangles=" << geometry.logical_triangles
                  << " degenerate_triangles=" << geometry.degenerate_triangles
                  << " runtime_vertices=" << geometry.corner_keys.size()
                  << " draw_matrices=" << draw_matrix_text.str()
                  << " rigid_joints=" << rigid_joint_text.str()
                  << " bounds=" << minimum.x << ',' << minimum.y << ',' << minimum.z
                  << ':' << maximum.x << ',' << maximum.y << ',' << maximum.z
                  << " center=" << center.x << ',' << center.y << ',' << center.z
                  << " dimensions="
                  << maximum.x - minimum.x << ','
                  << maximum.y - minimum.y << ','
                  << maximum.z - minimum.z
                  << " winding=gx_strip_alternating"
                  << " probable_side=" << probable_side
                  << '\n';
    }
    std::cout << "NEUTRAL_HAND_SELECTION"
              << " animation=ANM_WAIT"
              << " animation_enum=0x019"
              << " left_hand_shape_index=4"
              << " right_hand_shape_index=10"
              << " decision_path=m_anmDataTable[ANM_WAIT]"
                 "->setHandIndex->setDrawHand"
              << " source_symbols=ANM_WAIT,m_anmDataTable,setHandIndex,setDrawHand"
              << " source_constants=0x4,0xA,0xFF"
              << " unresolved_fields=none"
              << '\n';
}

struct AssemblyMetrics {
    u64 positions = 0;
    u64 triangles = 0;
    u64 runtime_vertices = 0;
    u64 nondegenerate_triangles = 0;
    u32 chunks16 = 0;
    u32 draws = 0;
    u16 main_joints = 0;
};

AssemblyMetrics assembly_metrics;
std::array<J3DModelData*, 4> loaded_models = {};
J3DAnmTransformKey* loaded_animation = nullptr;
std::string loaded_animation_name;
u16 loaded_animation_id = 0;

struct AttributeLayout {
    GXAttr attr;
    GXAttrType type;
    u8 offset;
    u8 size;
};

std::vector<AttributeLayout> make_layout(const GXVtxDescList* descriptions) {
    std::vector<AttributeLayout> layout;
    u8 offset = 0;
    for (const GXVtxDescList* description = descriptions;
         description->attr != GX_VA_NULL;
         ++description) {
        if (description->type == GX_NONE) {
            continue;
        }
        u8 size = 0;
        if (description->type == GX_INDEX8) {
            size = 1;
        } else if (description->type == GX_INDEX16) {
            size = 2;
        } else if (
            description->type == GX_DIRECT &&
            description->attr >= GX_VA_PNMTXIDX &&
            description->attr <= GX_VA_TEX7MTXIDX) {
            size = 1;
        } else {
            throw std::runtime_error(
                "unsupported direct display-list attribute in inventory");
        }
        layout.push_back({description->attr, description->type, offset, size});
        offset += size;
    }
    return layout;
}

u16 vertex_index(
    const u8* vertex,
    const std::vector<AttributeLayout>& layout,
    GXAttr wanted,
    bool* present = nullptr) {
    for (const AttributeLayout& attribute : layout) {
        if (attribute.attr != wanted) {
            continue;
        }
        if (present != nullptr) {
            *present = true;
        }
        if (attribute.type == GX_INDEX16) {
            return read_be16(vertex + attribute.offset);
        }
        return vertex[attribute.offset];
    }
    if (present != nullptr) {
        *present = false;
    }
    return 0;
}

std::string corner_key(
    const u8* vertex,
    const std::vector<AttributeLayout>& layout) {
    std::string key;
    key.reserve(layout.size() * 3);
    for (const AttributeLayout& attribute : layout) {
        if (attribute.attr < GX_VA_POS || attribute.type == GX_DIRECT) {
            continue;
        }
        const u16 index = attribute.type == GX_INDEX16
            ? read_be16(vertex + attribute.offset)
            : vertex[attribute.offset];
        key.push_back(static_cast<char>(attribute.attr));
        key.push_back(static_cast<char>(index >> 8));
        key.push_back(static_cast<char>(index & 0xff));
    }
    return key;
}

void record_triangle(
    const u8* vertices,
    u32 stride,
    const std::vector<AttributeLayout>& layout,
    u16 a,
    u16 b,
    u16 c,
    GeometryMetrics& metrics) {
    bool has_position = false;
    const u16 ia = vertex_index(
        vertices + stride * a,
        layout,
        GX_VA_POS,
        &has_position);
    const u16 ib = vertex_index(vertices + stride * b, layout, GX_VA_POS);
    const u16 ic = vertex_index(vertices + stride * c, layout, GX_VA_POS);
    ++metrics.logical_triangles;
    if (has_position && (ia == ib || ib == ic || ia == ic)) {
        ++metrics.degenerate_triangles;
    }
}

void measure_display_list(
    const J3DShapeDraw& draw,
    const GXVtxDescList* descriptions,
    GeometryMetrics& metrics) {
    const std::vector<AttributeLayout> layout = make_layout(descriptions);
    u32 stride = 0;
    for (const AttributeLayout& attribute : layout) {
        stride += attribute.size;
    }
    const u8* data = draw.getDisplayList();
    const u32 size = draw.getDisplayListSize();
    for (u32 cursor = 0; cursor < size;) {
        const u8 command = data[cursor++];
        if (command == 0) {
            continue;
        }
        const u8 primitive = command & 0xf8;
        if (primitive != GX_TRIANGLES &&
            primitive != GX_TRIANGLESTRIP &&
            primitive != GX_TRIANGLEFAN &&
            primitive != GX_QUADS) {
            throw std::runtime_error("unsupported GX command in shape display list");
        }
        if (cursor + 2 > size) {
            throw std::runtime_error("truncated GX primitive header");
        }
        const u16 count = read_be16(data + cursor);
        cursor += 2;
        const u64 payload_size = static_cast<u64>(count) * stride;
        if (payload_size > size - cursor) {
            throw std::runtime_error("truncated GX primitive payload");
        }
        const u8* vertices = data + cursor;
        ++metrics.primitive_commands;
        metrics.direct_triangle_commands += primitive == GX_TRIANGLES;
        metrics.strip_commands += primitive == GX_TRIANGLESTRIP;
        metrics.fan_commands += primitive == GX_TRIANGLEFAN;

        for (u16 vertex = 0; vertex < count; ++vertex) {
            const u8* current = vertices + stride * vertex;
            metrics.corner_keys.insert(corner_key(current, layout));
            for (const AttributeLayout& attribute : layout) {
                if (attribute.type == GX_DIRECT) {
                    continue;
                }
                const u16 index = attribute.type == GX_INDEX16
                    ? read_be16(current + attribute.offset)
                    : current[attribute.offset];
                metrics.maximum_index = std::max<u32>(
                    metrics.maximum_index,
                    index);
            }
        }
        if (primitive == GX_TRIANGLES) {
            for (u16 vertex = 0; vertex + 2 < count; vertex += 3) {
                record_triangle(
                    vertices,
                    stride,
                    layout,
                    vertex,
                    vertex + 1,
                    vertex + 2,
                    metrics);
            }
        } else if (primitive == GX_TRIANGLESTRIP) {
            for (u16 vertex = 2; vertex < count; ++vertex) {
                const u16 a = (vertex & 1) == 0 ? vertex - 2 : vertex - 1;
                const u16 b = (vertex & 1) == 0 ? vertex - 1 : vertex - 2;
                record_triangle(
                    vertices,
                    stride,
                    layout,
                    a,
                    b,
                    vertex,
                    metrics);
            }
        } else if (primitive == GX_TRIANGLEFAN) {
            for (u16 vertex = 2; vertex < count; ++vertex) {
                record_triangle(
                    vertices,
                    stride,
                    layout,
                    0,
                    vertex - 1,
                    vertex,
                    metrics);
            }
        } else {
            for (u16 vertex = 0; vertex + 3 < count; vertex += 4) {
                record_triangle(
                    vertices,
                    stride,
                    layout,
                    vertex,
                    vertex + 1,
                    vertex + 2,
                    metrics);
                record_triangle(
                    vertices,
                    stride,
                    layout,
                    vertex + 2,
                    vertex + 3,
                    vertex,
                    metrics);
            }
        }
        cursor += static_cast<u32>(payload_size);
    }
}

std::string section_list(const u8* resource, u32 resource_size) {
    const u32 block_count = read_be32(resource + 0x0c);
    u32 offset = 0x20;
    std::ostringstream sections;
    for (u32 block = 0; block < block_count; ++block) {
        if (offset + 8 > resource_size) {
            throw std::runtime_error("truncated J3D section header");
        }
        if (block != 0) {
            sections << ',';
        }
        sections.write(reinterpret_cast<const char*>(resource + offset), 4);
        const u32 block_size = read_be32(resource + offset + 4);
        if (block_size < 8 || block_size > resource_size - offset) {
            throw std::runtime_error("invalid J3D section size");
        }
        offset += block_size;
    }
    return sections.str();
}

std::string attribute_formats(J3DVertexData& vertices) {
    std::ostringstream formats;
    bool first = true;
    for (const GXVtxAttrFmtList* format = vertices.getVtxAttrFmtList();
         format != nullptr && format->attr != GX_VA_NULL;
         ++format) {
        if (!first) {
            formats << ',';
        }
        first = false;
        formats << static_cast<unsigned>(format->attr)
                << ':' << static_cast<unsigned>(format->cnt)
                << ':' << static_cast<unsigned>(format->type)
                << ':' << static_cast<unsigned>(format->frac);
    }
    return formats.str();
}

void report_model_metrics(
    const char* name,
    const u8* resource,
    u32 resource_size,
    J3DModelData& model) {
    GeometryMetrics geometry;
    Vec minimum = {INFINITY, INFINITY, INFINITY};
    Vec maximum = {-INFINITY, -INFINITY, -INFINITY};
    u32 draw_groups = 0;
    for (u16 shape_index = 0; shape_index < model.getShapeNum(); ++shape_index) {
        J3DShape* shape = model.getShapeNodePointer(shape_index);
        minimum.x = std::min(minimum.x, shape->getMin()->x);
        minimum.y = std::min(minimum.y, shape->getMin()->y);
        minimum.z = std::min(minimum.z, shape->getMin()->z);
        maximum.x = std::max(maximum.x, shape->getMax()->x);
        maximum.y = std::max(maximum.y, shape->getMax()->y);
        maximum.z = std::max(maximum.z, shape->getMax()->z);
        draw_groups += shape->getMtxGroupNum();
        for (u16 group = 0; group < shape->getMtxGroupNum(); ++group) {
            measure_display_list(
                *shape->getShapeDraw(group),
                shape->getVtxDesc(),
                geometry);
        }
    }

    u32 vertex_array_count = 0;
    u32 color_count = 0;
    u32 uv_count = 0;
    J3DVertexData& vertices = model.getVertexData();
    for (GXAttr attribute = GX_VA_POS;
         attribute <= GX_VA_TEX7;
         attribute = static_cast<GXAttr>(attribute + 1)) {
        const u32 count = vertices.getVtxArrNum(attribute);
        vertex_array_count += count != 0;
        if (attribute == GX_VA_CLR0 || attribute == GX_VA_CLR1) {
            color_count += count;
        }
        if (attribute >= GX_VA_TEX0 && attribute <= GX_VA_TEX7) {
            uv_count += count;
        }
    }

    u32 maximum_influences = 0;
    u32 influence_count = 0;
    f32 minimum_weight = INFINITY;
    f32 maximum_weight = -INFINITY;
    f32 minimum_weight_sum = INFINITY;
    f32 maximum_weight_sum = -INFINITY;
    for (u16 envelope = 0; envelope < model.getWEvlpMtxNum(); ++envelope) {
        const u8 count = model.getWEvlpMixMtxNum(envelope);
        maximum_influences = std::max<u32>(maximum_influences, count);
        f32 sum = 0.0f;
        for (u8 influence = 0; influence < count; ++influence) {
            const f32 weight = model.getWEvlpMixWeight()[influence_count++];
            minimum_weight = std::min(minimum_weight, weight);
            maximum_weight = std::max(maximum_weight, weight);
            sum += weight;
        }
        minimum_weight_sum = std::min(minimum_weight_sum, sum);
        maximum_weight_sum = std::max(maximum_weight_sum, sum);
    }
    if (influence_count == 0) {
        minimum_weight = maximum_weight = 0.0f;
        minimum_weight_sum = maximum_weight_sum = 0.0f;
    }

    u32 rigid_matrices = 0;
    u32 enveloped_matrices = 0;
    for (u16 matrix = 0; matrix < model.getDrawMtxNum(); ++matrix) {
        if (model.getDrawMtxFlag(matrix) == 0) {
            ++rigid_matrices;
        } else {
            ++enveloped_matrices;
        }
    }
    const u16 texture_count =
        model.getTexture() == nullptr ? 0 : model.getTexture()->getNum();
    JUTNameTab* texture_names = model.getTextureName();
    for (u16 texture_index = 0; texture_index < texture_count; ++texture_index) {
        const ResTIMG* texture =
            model.getTexture()->getResTIMG(texture_index);
        std::ostringstream users;
        for (u16 material_index = 0;
             material_index < model.getMaterialNum();
             ++material_index) {
            J3DMaterial* material =
                model.getMaterialNodePointer(material_index);
            bool uses_texture = false;
            for (u32 slot = 0; slot < 8; ++slot) {
                uses_texture |= material->getTexNo(slot) == texture_index;
            }
            if (uses_texture) {
                if (users.tellp() > 0) {
                    users << ',';
                }
                users << material_index;
            }
        }
        std::cout << "TEXTURE_METRICS"
                  << " model=" << name
                  << " index=" << texture_index
                  << " name="
                  << (texture_names != nullptr
                          ? texture_names->getName(texture_index)
                          : "unnamed")
                  << " width=" << static_cast<u16>(texture->width)
                  << " height=" << static_cast<u16>(texture->height)
                  << " gx_format=" << static_cast<unsigned>(texture->format)
                  << " alpha=" << static_cast<unsigned>(texture->alphaEnabled)
                  << " indexed=" << static_cast<unsigned>(texture->indexTexture)
                  << " palette_format="
                  << static_cast<unsigned>(texture->colorFormat)
                  << " colors=" << static_cast<u16>(texture->numColors)
                  << " mipmaps=" << static_cast<unsigned>(texture->mipmapCount)
                  << " min_filter=" << static_cast<unsigned>(texture->minFilter)
                  << " mag_filter=" << static_cast<unsigned>(texture->magFilter)
                  << " material_users="
                  << (users.tellp() > 0 ? users.str() : "none")
                  << '\n';
    }
    JUTNameTab* material_names = model.getMaterialName();
    for (u16 material_index = 0;
         material_index < model.getMaterialNum();
         ++material_index) {
        J3DMaterial* material = model.getMaterialNodePointer(material_index);
        std::ostringstream textures;
        u32 texture_slots = 0;
        for (u32 slot = 0; slot < 8; ++slot) {
            const u16 texture_index = material->getTexNo(slot);
            if (texture_index == 0xffff || texture_index >= texture_count) {
                continue;
            }
            if (texture_slots++ != 0) {
                textures << ',';
            }
            textures << texture_index;
        }
        std::cout << "MATERIAL_METRICS"
                  << " model=" << name
                  << " index=" << material_index
                  << " name="
                  << (material_names != nullptr
                          ? material_names->getName(material_index)
                          : "unnamed")
                  << " texture_slots=" << texture_slots
                  << " texture_indices="
                  << (texture_slots != 0 ? textures.str() : "none")
                  << " psp_candidate="
                  << (texture_slots == 0
                          ? "FlatColorFallback"
                          : texture_slots == 1
                              ? "OpaqueOrAlphaTexture"
                              : "PrimaryTextureFallback")
                  << '\n';
    }
    const std::string sections = section_list(resource, resource_size);
    assembly_metrics.positions += model.getVtxNum();
    assembly_metrics.triangles += geometry.logical_triangles;
    assembly_metrics.runtime_vertices += geometry.corner_keys.size();
    assembly_metrics.nondegenerate_triangles +=
        geometry.logical_triangles - geometry.degenerate_triangles;
    assembly_metrics.chunks16 +=
        static_cast<u32>((geometry.corner_keys.size() + 65534) / 65535);
    assembly_metrics.draws += draw_groups;
    if (std::strcmp(name, "al.bmd") == 0) {
        assembly_metrics.main_joints = model.getJointNum();
    }
    std::cout << "MODEL_METRICS"
              << " name=" << name
              << " magic=" << std::string(
                     reinterpret_cast<const char*>(resource),
                     8)
              << " sections=" << sections
              << " file_bytes=" << read_be32(resource + 8)
              << " joints=" << model.getJointNum()
              << " shapes=" << model.getShapeNum()
              << " materials=" << model.getMaterialNum()
              << " textures=" << texture_count
              << " vertex_arrays=" << vertex_array_count
              << " positions=" << model.getVtxNum()
              << " normals=" << model.getNrmNum()
              << " colors=" << color_count
              << " uvs=" << uv_count
              << " packets=" << draw_groups
              << " primitives=" << geometry.primitive_commands
              << " direct_triangle_commands="
              << geometry.direct_triangle_commands
              << " strips=" << geometry.strip_commands
              << " fans=" << geometry.fan_commands
              << " logical_triangles=" << geometry.logical_triangles
              << " degenerate_triangles=" << geometry.degenerate_triangles
              << " max_index=" << geometry.maximum_index
              << " runtime_vertices=" << geometry.corner_keys.size()
              << " bounds=" << minimum.x << ',' << minimum.y << ',' << minimum.z
              << ':' << maximum.x << ',' << maximum.y << ',' << maximum.z
              << " evp1=" << (sections.find("EVP1") != std::string::npos)
              << " drw1=" << (sections.find("DRW1") != std::string::npos)
              << " envelopes=" << model.getWEvlpMtxNum()
              << " influences=" << influence_count
              << " max_influences=" << maximum_influences
              << " weight_range=" << minimum_weight << ':' << maximum_weight
              << " weight_sum_range="
              << minimum_weight_sum << ':' << maximum_weight_sum
              << " rigid_matrices=" << rigid_matrices
              << " enveloped_matrices=" << enveloped_matrices
              << " draws_without_material=" << draw_groups
              << " chunks16=" << (geometry.corner_keys.size() + 65534) / 65535
              << " formats=" << attribute_formats(vertices)
              << '\n';
    if (std::strcmp(name, "al_hands.bmd") == 0) {
        report_hand_shape_metrics(model);
    }
}

RoomModelSummary summarize_room_model(
    const void* resource, std::uint32_t bytes) {
    if (resource == nullptr || bytes < 32) {
        throw std::runtime_error("room model resource absent");
    }
    J3DModelData* model =
        J3DModelLoaderDataBase::load(resource, 0x59020030);
    if (model == nullptr) {
        throw std::runtime_error("room J3D model load failed");
    }
    RoomModelSummary result;
    GeometryMetrics geometry;
    for (u16 shape_index = 0;
         shape_index < model->getShapeNum(); ++shape_index) {
        J3DShape* shape = model->getShapeNodePointer(shape_index);
        for (u16 group = 0; group < shape->getMtxGroupNum(); ++group) {
            measure_display_list(
                *shape->getShapeDraw(group),
                shape->getVtxDesc(),
                geometry);
            ++result.packets;
        }
    }
    result.triangles =
        geometry.logical_triangles - geometry.degenerate_triangles;
    result.degenerate_triangles = geometry.degenerate_triangles;
    result.runtime_vertices = geometry.corner_keys.size();
    result.materials = model->getMaterialNum();
    J3DTexture* textures = model->getTexture();
    result.textures = textures != nullptr ? textures->getNum() : 0;
    for (std::uint16_t index = 0; index < result.textures; ++index) {
        const ResTIMG* image = textures->getResTIMG(index);
        if (image == nullptr || image->width == 0 || image->height == 0 ||
            image->width > 512 || image->height > 512) {
            throw std::runtime_error("invalid room TEX1 dimensions");
        }
        const std::uint32_t stored_width =
            (static_cast<std::uint32_t>(image->width) + 7u) & ~7u;
        const std::uint32_t stored_height =
            (static_cast<std::uint32_t>(image->height) + 7u) & ~7u;
        result.texture_psp_bytes +=
            stored_width * stored_height * 2u;
    }
    return result;
}

void export_selected_real_room() {
    const char* stage = environment_or("DUSKLIGHT_ROOM_STAGE", "F_SP110");
    const unsigned long room = std::strtoul(
        environment_or("DUSKLIGHT_ROOM_INDEX", "2"), nullptr, 10);
    if (stage[0] == '\0' || std::strlen(stage) > 8 || room > 63) {
        throw std::runtime_error("selected room identity invalid");
    }
    for (const char* cursor = stage; *cursor != '\0'; ++cursor) {
        if (!((*cursor >= 'A' && *cursor <= 'Z') ||
              (*cursor >= '0' && *cursor <= '9') ||
              *cursor == '_')) {
            throw std::runtime_error("selected stage identifier unsafe");
        }
    }
    char archive_path[64] = {};
    char stage_archive_path[64] = {};
    if (std::snprintf(
            archive_path, sizeof(archive_path),
            "/res/Stage/%s/R%02lu_00.arc", stage, room) <= 0 ||
        std::snprintf(
            stage_archive_path, sizeof(stage_archive_path),
            "/res/Stage/%s/STG_00.arc", stage) <= 0) {
        throw std::runtime_error("selected room path formatting failed");
    }
    std::vector<std::uint8_t> archive = decompress_archive(
        read_dvd_file(archive_path));
    std::vector<std::uint8_t> stage_archive = decompress_archive(
        read_dvd_file(stage_archive_path));
    validate_rarc(archive, archive_path);
    validate_rarc(stage_archive, stage_archive_path);
    JKRMemArchive mounted(
        archive.data(),
        static_cast<u32>(archive.size()),
        JKRMEMBREAK_FLAG_UNKNOWN0);
    JKRMemArchive stage_mounted(
        stage_archive.data(),
        static_cast<u32>(stage_archive.size()),
        JKRMEMBREAK_FLAG_UNKNOWN0);
    auto resource_named = [](JKRMemArchive& source, const char* wanted) -> void* {
        for (u32 index = 0; index < source.countFile(); ++index) {
            JKRArchive::SDirEntry entry = {};
            if (!source.getDirEntry(&entry, index) ||
                entry.name == nullptr ||
                !ascii_equal_ignore_case(entry.name, wanted)) {
                continue;
            }
            return entry.id != 0xffff
                ? source.getResource(entry.id)
                : source.getResource(entry.name);
        }
        return nullptr;
    };
    void* model_resource = resource_named(mounted, "model.bmd");
    void* collision_resource = resource_named(mounted, "room.kcl");
    void* placement_resource = resource_named(mounted, "room.dzr");
    void* stage_resource = resource_named(stage_mounted, "stage.dzs");
    if (model_resource == nullptr ||
        collision_resource == nullptr ||
        placement_resource == nullptr ||
        stage_resource == nullptr) {
        throw std::runtime_error("selected real room resource missing");
    }
    if (std::getenv("DUSKLIGHT_ENVIRONMENT_AUDIT") != nullptr) {
        auto print_chunks = [](const char* source, const std::uint8_t* bytes,
                               std::uint32_t size) {
            if (size < 4) {
                throw std::runtime_error("environment audit source truncated");
            }
            const std::uint32_t count =
                (static_cast<std::uint32_t>(bytes[0]) << 24) |
                (static_cast<std::uint32_t>(bytes[1]) << 16) |
                (static_cast<std::uint32_t>(bytes[2]) << 8) |
                bytes[3];
            if (count > 128 || count > (size - 4) / 12) {
                throw std::runtime_error(
                    "environment audit chunk table invalid");
            }
            for (std::uint32_t index = 0; index < count; ++index) {
                const std::uint8_t* node = bytes + 4 + index * 12;
                const std::uint32_t entries =
                    (static_cast<std::uint32_t>(node[4]) << 24) |
                    (static_cast<std::uint32_t>(node[5]) << 16) |
                    (static_cast<std::uint32_t>(node[6]) << 8) |
                    node[7];
                const std::uint32_t offset =
                    (static_cast<std::uint32_t>(node[8]) << 24) |
                    (static_cast<std::uint32_t>(node[9]) << 16) |
                    (static_cast<std::uint32_t>(node[10]) << 8) |
                    node[11];
                if (offset > size) {
                    throw std::runtime_error(
                        "environment audit chunk offset invalid");
                }
                std::cout << "ENVIRONMENT_CHUNK source=" << source
                          << " tag="
                          << std::string(
                              reinterpret_cast<const char*>(node), 4)
                          << " count=" << entries
                          << " offset=" << offset << '\n';
            }
        };
        print_chunks(
            "stage.dzs",
            static_cast<const std::uint8_t*>(stage_resource),
            stage_mounted.getResSize(stage_resource));
        print_chunks(
            "room.dzr",
            static_cast<const std::uint8_t*>(placement_resource),
            mounted.getResSize(placement_resource));
    }
    J3DModelData* model =
        J3DModelLoaderDataBase::load(model_resource, 0x59020030);
    if (model == nullptr) {
        throw std::runtime_error("selected real room J3D load failed");
    }
    J3DTexture* textures = model->getTexture();
    JUTNameTab* texture_names = model->getTextureName();
    if (textures == nullptr || texture_names == nullptr) {
        throw std::runtime_error("selected real room TEX1 table missing");
    }
    std::uint32_t external_textures = 0;
    for (std::uint16_t index = 0; index < textures->getNum(); ++index) {
        if (textures->getResTIMG(index)->imageOffset != 0) {
            continue;
        }
        char name[96] = {};
        const int count = std::snprintf(
            name, sizeof(name), "%s.bti", texture_names->getName(index));
        if (count <= 0 || static_cast<std::size_t>(count) >= sizeof(name)) {
            throw std::runtime_error("selected room BTI name overflow");
        }
        void* external = resource_named(stage_mounted, name);
        if (external == nullptr ||
            stage_mounted.getResSize(external) < sizeof(ResTIMG)) {
            throw std::runtime_error(
                std::string("selected room external BTI missing: ") + name);
        }
        textures->setResTIMG(index, *static_cast<ResTIMG*>(external));
        ++external_textures;
    }
    export_real_room_packages(
        model,
        static_cast<const std::uint8_t*>(collision_resource),
        mounted.getResSize(collision_resource),
        static_cast<const std::uint8_t*>(placement_resource),
        mounted.getResSize(placement_resource),
        static_cast<const std::uint8_t*>(stage_resource),
        stage_mounted.getResSize(stage_resource));
    std::cout << "REAL_ROOM_EXPORT_OK"
              << " stage=" << stage
              << " room=" << std::setfill('0') << std::setw(2) << room
              << std::setfill(' ')
              << " layer=00"
              << " archive=" << archive_path
              << " stage_archive=" << stage_archive_path
              << " external_textures=" << external_textures
              << " model=model.bmd collision=room.kcl placement=room.dzr"
              << '\n';
}

void export_selected_original_actor() {
    constexpr char archive_path[] = "/res/Object/L4HsMato.arc";
    std::vector<std::uint8_t> archive = decompress_archive(
        read_dvd_file(archive_path));
    validate_rarc(archive, archive_path);
    JKRMemArchive mounted(
        archive.data(),
        static_cast<u32>(archive.size()),
        JKRMEMBREAK_FLAG_UNKNOWN0);
    void* model_resource = mounted.getResource(static_cast<u16>(4));
    void* collision_resource = mounted.getResource(static_cast<u16>(7));
    if (model_resource == nullptr || collision_resource == nullptr) {
        throw std::runtime_error(
            "L4HsMato source resource IDs 4/7 are absent");
    }
    J3DModelData* model =
        J3DModelLoaderDataBase::load(model_resource, 0x59020030);
    if (model == nullptr) {
        throw std::runtime_error("L4HsMato J3D model load failed");
    }
    export_real_room_packages(model, nullptr, 0, nullptr, 0, nullptr, 0);
    const char* collision_output =
        std::getenv("DUSKLIGHT_ACTOR_DPCL_OUTPUT");
    if (collision_output != nullptr && collision_output[0] != '\0') {
        export_movebg_collision_package(
            static_cast<const std::uint8_t*>(collision_resource),
            mounted.getResSize(collision_resource),
            collision_output);
    }
    std::cout << "ORIGINAL_ACTOR_EXPORT_OK"
              << " source_name=L4hmato"
              << " archive=" << archive_path
              << " archive_bytes=" << archive.size()
              << " model_resource_id=4"
              << " model_resource_bytes="
              << mounted.getResSize(model_resource)
              << " collision_resource_id=7"
              << " collision_resource_bytes="
              << mounted.getResSize(collision_resource)
              << " models=1 animations=0 collision=1"
              << '\n';
}

void export_selected_dynamic_actor() {
    constexpr char archive_path[] = "/res/Object/P_Gear.arc";
    std::vector<std::uint8_t> archive = decompress_archive(
        read_dvd_file(archive_path));
    validate_rarc(archive, archive_path);
    JKRMemArchive mounted(
        archive.data(),
        static_cast<u32>(archive.size()),
        JKRMEMBREAK_FLAG_UNKNOWN0);
    const std::array<u16, 2> resource_ids = {4, 3};
    const std::array<const char*, 2> dprm_variables = {
        "DUSKLIGHT_GEAR_SMALL_DPRM_OUTPUT",
        "DUSKLIGHT_GEAR_LARGE_DPRM_OUTPUT",
    };
    const std::array<const char*, 2> dptx_variables = {
        "DUSKLIGHT_GEAR_SMALL_DPTX_OUTPUT",
        "DUSKLIGHT_GEAR_LARGE_DPTX_OUTPUT",
    };
    std::uint32_t total_source_bytes = 0;
    for (std::size_t index = 0; index < resource_ids.size(); ++index) {
        void* resource = mounted.getResource(resource_ids[index]);
        if (resource == nullptr) {
            throw std::runtime_error("P_Gear source model absent");
        }
        J3DModelData* model =
            J3DModelLoaderDataBase::load(resource, 0x59020030);
        if (model == nullptr) {
            throw std::runtime_error("P_Gear J3D model load failed");
        }
        export_static_model_packages(
            model,
            environment_or(dprm_variables[index], ""),
            environment_or(dptx_variables[index], ""));
        total_source_bytes += mounted.getResSize(resource);
    }
    std::cout << "ORIGINAL_DYNAMIC_ACTOR_EXPORT_OK"
              << " source_name=spnGear"
              << " archive=" << archive_path
              << " archive_bytes=" << archive.size()
              << " small_model_resource_id=4"
              << " large_model_resource_id=3"
              << " model_source_bytes=" << total_source_bytes
              << " models=2 animations=0 collision=0"
              << '\n';
}

void export_selected_original_door() {
    constexpr char archive_path[] = "/res/Object/L4R02Gate.arc";
    std::vector<std::uint8_t> archive = decompress_archive(
        read_dvd_file(archive_path));
    validate_rarc(archive, archive_path);
    JKRMemArchive mounted(
        archive.data(),
        static_cast<u32>(archive.size()),
        JKRMEMBREAK_FLAG_UNKNOWN0);
    void* model_resource = mounted.getResource(static_cast<u16>(4));
    void* collision_resource = mounted.getResource(static_cast<u16>(7));
    if (model_resource == nullptr || collision_resource == nullptr) {
        throw std::runtime_error(
            "L4R02Gate source resource IDs 4/7 are absent");
    }
    J3DModelData* model =
        J3DModelLoaderDataBase::load(model_resource, 0x59020030);
    if (model == nullptr) {
        throw std::runtime_error("L4R02Gate J3D model load failed");
    }
    export_static_movebg_packages(
        model,
        static_cast<const std::uint8_t*>(collision_resource),
        mounted.getResSize(collision_resource),
        environment_or("DUSKLIGHT_DOOR_DPRM_OUTPUT", ""),
        environment_or("DUSKLIGHT_DOOR_DPTX_OUTPUT", ""),
        environment_or("DUSKLIGHT_DOOR_DPCL_OUTPUT", ""));
    std::cout << "ORIGINAL_DOOR_EXPORT_OK"
              << " source_name=L4Pgate"
              << " archive=" << archive_path
              << " archive_bytes=" << archive.size()
              << " model_resource_id=4"
              << " model_resource_bytes="
              << mounted.getResSize(model_resource)
              << " collision_resource_id=7"
              << " collision_resource_bytes="
              << mounted.getResSize(collision_resource)
              << " formats=DPRM,DPTX,DPCL"
              << '\n';
}

void export_selected_spinner_switch() {
    constexpr char archive_path[] = "/res/Object/P_Sswitch.arc";
    std::vector<std::uint8_t> archive = decompress_archive(
        read_dvd_file(archive_path));
    validate_rarc(archive, archive_path);
    JKRMemArchive mounted(
        archive.data(),
        static_cast<u32>(archive.size()),
        JKRMEMBREAK_FLAG_UNKNOWN0);
    const std::array<u16, 2> model_ids = {4, 5};
    const std::array<u16, 2> collision_ids = {9, 8};
    const std::array<const char*, 2> dprm_variables = {
        "DUSKLIGHT_SPINNER_SWITCH_BASE_DPRM_OUTPUT",
        "DUSKLIGHT_SPINNER_SWITCH_TOP_DPRM_OUTPUT",
    };
    const std::array<const char*, 2> dptx_variables = {
        "DUSKLIGHT_SPINNER_SWITCH_BASE_DPTX_OUTPUT",
        "DUSKLIGHT_SPINNER_SWITCH_TOP_DPTX_OUTPUT",
    };
    const std::array<const char*, 2> dpcl_variables = {
        "DUSKLIGHT_SPINNER_SWITCH_BASE_DPCL_OUTPUT",
        "DUSKLIGHT_SPINNER_SWITCH_TOP_DPCL_OUTPUT",
    };
    for (std::size_t index = 0; index < model_ids.size(); ++index) {
        void* model_resource = mounted.getResource(model_ids[index]);
        void* collision_resource =
            mounted.getResource(collision_ids[index]);
        if (model_resource == nullptr || collision_resource == nullptr) {
            throw std::runtime_error(
                "P_Sswitch source resource is absent");
        }
        J3DModelData* model =
            J3DModelLoaderDataBase::load(model_resource, 0x59020030);
        if (model == nullptr) {
            throw std::runtime_error(
                "P_Sswitch J3D model load failed");
        }
        export_static_movebg_packages(
            model,
            static_cast<const std::uint8_t*>(collision_resource),
            mounted.getResSize(collision_resource),
            environment_or(dprm_variables[index], ""),
            environment_or(dptx_variables[index], ""),
            environment_or(dpcl_variables[index], ""));
    }
    std::cout << "SPINNER_SWITCH_EXPORT_OK"
              << " source_name=swspin"
              << " archive=" << archive_path
              << " archive_bytes=" << archive.size()
              << " model_resource_ids=4,5"
              << " collision_resource_ids=9,8"
              << " formats=DPRM,DPTX,DPCL"
              << '\n';
}

void export_selected_treasure_chest() {
    constexpr char archive_path[] = "/res/Object/Dalways.arc";
    constexpr u16 model_id = 13;
    constexpr u16 animation_id = 8;
    constexpr u16 closed_collision_id = 27;
    constexpr u16 open_collision_id = 28;
    std::vector<std::uint8_t> archive = decompress_archive(
        read_dvd_file(archive_path));
    validate_rarc(archive, archive_path);
    JKRMemArchive mounted(
        archive.data(),
        static_cast<u32>(archive.size()),
        JKRMEMBREAK_FLAG_UNKNOWN0);
    void* model_resource = mounted.getResource(model_id);
    void* animation_resource = mounted.getResource(animation_id);
    void* closed_collision =
        mounted.getResource(closed_collision_id);
    void* open_collision =
        mounted.getResource(open_collision_id);
    if (model_resource == nullptr || animation_resource == nullptr ||
        closed_collision == nullptr || open_collision == nullptr) {
        throw std::runtime_error(
            "Dalways large chest source resource is absent");
    }
    J3DModelData* model =
        J3DModelLoaderDataBase::load(model_resource, 0x59020030);
    if (model == nullptr) {
        throw std::runtime_error(
            "Dalways large chest J3D model load failed");
    }
    export_static_model_packages(
        model,
        environment_or("DUSKLIGHT_TBOX_DPRM_OUTPUT", ""),
        environment_or("DUSKLIGHT_TBOX_DPTX_OUTPUT", ""));
    export_static_bck_package(
        archive, animation_id,
        environment_or("DUSKLIGHT_TBOX_DPAN_OUTPUT", ""));
    export_movebg_collision_package(
        static_cast<const std::uint8_t*>(closed_collision),
        mounted.getResSize(closed_collision),
        environment_or("DUSKLIGHT_TBOX_CLOSED_DPCL_OUTPUT", ""));
    export_movebg_collision_package(
        static_cast<const std::uint8_t*>(open_collision),
        mounted.getResSize(open_collision),
        environment_or("DUSKLIGHT_TBOX_OPEN_DPCL_OUTPUT", ""));
    std::cout << "TREASURE_CHEST_EXPORT_OK"
              << " source_name=tboxB0"
              << " archive=" << archive_path
              << " model_resource_id=" << model_id
              << " animation_resource_id=" << animation_id
              << " closed_collision_resource_id="
              << closed_collision_id
              << " open_collision_resource_id="
              << open_collision_id
              << " formats=DPRM,DPTX,DPAN,DPCL"
              << '\n';
}

void export_original_startup_title() {
    constexpr char archive_path[] = "/res/Object/TitlePal.arc";
    constexpr u16 model_id = 10;
    constexpr u16 bck_id = 7;
    std::vector<std::uint8_t> archive = decompress_archive(
        read_dvd_file(archive_path));
    validate_rarc(archive, archive_path);
    JKRMemArchive mounted(
        archive.data(),
        static_cast<u32>(archive.size()),
        JKRMEMBREAK_FLAG_UNKNOWN0);
    void* model_resource = mounted.getResource(model_id);
    void* animation_resource = mounted.getResource(bck_id);
    if (model_resource == nullptr || animation_resource == nullptr) {
        throw std::runtime_error(
            "TitlePal source resource IDs 10/7 are absent");
    }
    J3DModelData* model =
        J3DModelLoaderDataBase::load(model_resource, 0x59020030);
    if (model == nullptr) {
        throw std::runtime_error("TitlePal J3D model load failed");
    }
    export_static_model_packages(
        model,
        environment_or("DUSKLIGHT_TITLE_DPRM_OUTPUT", ""),
        environment_or("DUSKLIGHT_TITLE_DPTX_OUTPUT", ""));
    export_static_bck_package(
        archive, bck_id,
        environment_or("DUSKLIGHT_TITLE_DPAN_OUTPUT", ""));
    std::cout << "ORIGINAL_STARTUP_TITLE_EXPORT_OK"
              << " archive=" << archive_path
              << " model_resource_id=" << model_id
              << " model_resource_bytes="
              << mounted.getResSize(model_resource)
              << " bck_resource_id=" << bck_id
              << " bck_resource_bytes="
              << mounted.getResSize(animation_resource)
              << " unported_animation_ids=13,16,19"
              << " formats=DPRM,DPTX,DPAN"
              << '\n';
}

void export_demo01_startup_actors() {
    constexpr char archive_path[] = "/res/Object/Demo01_01.arc";
    constexpr u16 rusl_model_id = 47;
    constexpr u16 rusl_wide_bck_id = 18;
    constexpr u16 rusl_closeup_bck_id = 19;
    constexpr u16 link_wide_bck_id = 3;
    constexpr u16 link_closeup_bck_id = 4;
    // BCK 18 uses J3DFrameCtrl::EMode_LOOP with an end frame of 120.
    // At STB frame 270, the source runtime therefore samples local frame 30.
    constexpr float rusl_wide_frame = 30.0f;
    constexpr float rusl_wide_elapsed_frame = 270.0f;
    constexpr float rusl_closeup_frame = 92.0f;
    std::vector<std::uint8_t> archive = decompress_archive(
        read_dvd_file(archive_path));
    validate_rarc(archive, archive_path);
    JKRMemArchive mounted(
        archive.data(),
        static_cast<u32>(archive.size()),
        JKRMEMBREAK_FLAG_UNKNOWN0);
    void* model_resource = mounted.getResource(rusl_model_id);
    void* wide_resource = mounted.getResource(rusl_wide_bck_id);
    void* closeup_resource = mounted.getResource(rusl_closeup_bck_id);
    if (model_resource == nullptr || wide_resource == nullptr ||
        closeup_resource == nullptr ||
        mounted.getResource(link_wide_bck_id) == nullptr ||
        mounted.getResource(link_closeup_bck_id) == nullptr) {
        throw std::runtime_error(
            "Demo01_01 startup actor source resource is absent");
    }
    J3DModelData* rusl_model =
        J3DModelLoaderDataBase::load(model_resource, 0x59020030);
    auto* rusl_wide = dynamic_cast<J3DAnmTransform*>(
        J3DAnmLoaderDataBase::load(wide_resource));
    auto* rusl_closeup = dynamic_cast<J3DAnmTransform*>(
        J3DAnmLoaderDataBase::load(closeup_resource));
    if (rusl_model == nullptr || rusl_wide == nullptr ||
        rusl_closeup == nullptr) {
        throw std::runtime_error(
            "Demo01_01 Rusl model/BCK load failed");
    }
    export_animated_static_model_package(
        rusl_model,
        rusl_wide,
        rusl_wide_frame,
        environment_or("DUSKLIGHT_DEMO01_RUSL_WIDE_DPRM_OUTPUT", ""));
    export_animated_static_model_package(
        rusl_model,
        rusl_closeup,
        rusl_closeup_frame,
        environment_or(
            "DUSKLIGHT_DEMO01_RUSL_CLOSEUP_DPRM_OUTPUT", ""));
    export_static_model_texture_package(
        rusl_model,
        environment_or("DUSKLIGHT_DEMO01_RUSL_DPTX_OUTPUT", ""));
    export_static_bck_package(
        archive,
        link_wide_bck_id,
        environment_or("DUSKLIGHT_DEMO01_LINK_WIDE_DPAN_OUTPUT", ""));
    export_static_bck_package(
        archive,
        link_closeup_bck_id,
        environment_or("DUSKLIGHT_DEMO01_LINK_CLOSEUP_DPAN_OUTPUT", ""));
    std::cout << "DEMO01_STARTUP_ACTOR_EXPORT_OK"
              << " archive=" << archive_path
              << " rusl_model_resource_id=" << rusl_model_id
              << " rusl_joints=" << rusl_model->getJointNum()
              << " rusl_wide_bck_id=" << rusl_wide_bck_id
              << " rusl_wide_duration=" << rusl_wide->getFrameMax()
              << " rusl_wide_loop_mode="
              << static_cast<unsigned>(rusl_wide->getAttribute())
              << " rusl_wide_elapsed_frame="
              << rusl_wide_elapsed_frame
              << " rusl_wide_frame=" << rusl_wide_frame
              << " rusl_closeup_bck_id=" << rusl_closeup_bck_id
              << " rusl_closeup_duration=" << rusl_closeup->getFrameMax()
              << " rusl_closeup_loop_mode="
              << static_cast<unsigned>(rusl_closeup->getAttribute())
              << " rusl_closeup_frame=" << rusl_closeup_frame
              << " link_bck_ids=" << link_wide_bck_id << ','
              << link_closeup_bck_id
              << " formats=DPRM,DPTX,DPAN"
              << '\n';
}

void export_original_startup_ui() {
    auto mount_resources = [](
        const char* archive_path,
        const std::vector<const char*>& names) {
        std::vector<std::uint8_t> bytes =
            decompress_archive(read_dvd_file(archive_path));
        validate_rarc(bytes, archive_path);
        JKRMemArchive mounted(
            bytes.data(), static_cast<u32>(bytes.size()),
            JKRMEMBREAK_FLAG_UNKNOWN0);
        std::vector<HudSourceResource> resources;
        for (const char* name : names) {
            resources.push_back({
                name, copy_expanded_resource(mounted, name)});
        }
        return resources;
    };
    const std::vector<HudSourceResource> logos = mount_resources(
        "/res/Object/LogoPal.arc",
        {"nintendo_376x104.bti", "dolby_p2_232_112.bti"});
    const std::vector<HudSourceResource> warning = mount_resources(
        "/res/Layout/LogoPalFr.arc",
        {"warning_fr.bti", "warning_pstart_fr.bti"});
    std::vector<HudSourceResource> fonts = mount_resources(
        "/res/Fonteu/fontres.arc", {"rodan_b_24_22.bfn"});
    export_original_startup_ui_packages(
        logos, warning, fonts[0].bytes);
}

void export_original_file_select_ui() {
    constexpr const char* names[] = {
        "tt_3setu_w_l.bti",
        "tt_gold_uzu_long2.bti",
        "tt_spot_square3.bti",
        "tt_zelda_button_a_8ia.bti",
    };
    std::vector<std::uint8_t> bytes = decompress_archive(
        read_dvd_file("/res/Object/fileSel.arc"));
    validate_rarc(bytes, "/res/Object/fileSel.arc");
    JKRMemArchive mounted(
        bytes.data(), static_cast<u32>(bytes.size()),
        JKRMEMBREAK_FLAG_UNKNOWN0);
    std::vector<HudSourceResource> resources;
    for (const char* name : names) {
        resources.push_back({
            name, copy_expanded_resource(mounted, name)});
    }
    export_original_file_select_ui_package(
        resources,
        environment_or(
            "DUSKLIGHT_FILE_SELECT_DPSU_OUTPUT", ""));
}

void load_models(std::vector<std::uint8_t>& kmdl_bytes) {
    JKRMemArchive archive(
        kmdl_bytes.data(),
        static_cast<u32>(kmdl_bytes.size()),
        JKRMEMBREAK_FLAG_UNKNOWN0);

    for (u32 model_index = 0; model_index < kModelNames.size(); ++model_index) {
        const char* name = model_index == 0
            ? environment_or("DUSKLIGHT_TEST_MODEL_NAME", kModelNames[0])
            : kModelNames[model_index];
        JKRArchive::SDirEntry selected = {};
        bool found = false;
        for (u32 index = 0; index < archive.countFile(); ++index) {
            JKRArchive::SDirEntry entry = {};
            if (archive.getDirEntry(&entry, index) &&
                entry.name != nullptr &&
                std::strcmp(entry.name, name) == 0) {
                selected = entry;
                found = true;
                break;
            }
        }
        if (!found) {
            throw std::runtime_error(std::string("missing BMD resource: ") + name);
        }
        void* resource = archive.getResource(selected.id);
        if (resource == nullptr) {
            throw std::runtime_error(
                std::string("unable to access BMD resource: ") + name);
        }
        J3DModelData* model = J3DModelLoaderDataBase::load(resource, 0x59020030);
        if (model == nullptr) {
            throw std::runtime_error(std::string("J3D BMD load failed: ") + name);
        }
        loaded_models[model_index] = model;
        std::cout << "model name=" << name
                  << " id=" << selected.id
                  << " bytes=" << archive.getResSize(resource)
                  << " joints=" << model->getJointNum()
                  << " shapes=" << model->getShapeNum()
                  << " materials=" << model->getMaterialNum()
                  << " vertices=" << model->getVtxNum()
                  << " normals=" << model->getNrmNum()
                  << '\n';
        if (model_index == 0) {
            JUTNameTab* joint_names = model->getJointName();
            for (u16 joint = 0; joint < model->getJointNum(); ++joint) {
                const J3DTransformInfo& bind =
                    model->getJointNodePointer(joint)->getTransformInfo();
                std::cout << "LINK_JOINT_METRICS"
                          << " index=" << joint
                          << " name="
                          << (joint_names != nullptr
                                  ? joint_names->getName(joint)
                                  : "unnamed")
                          << " bind_translation="
                          << bind.mTranslate.x << ','
                          << bind.mTranslate.y << ','
                          << bind.mTranslate.z
                          << " bind_rotation="
                          << bind.mRotation.x << ','
                          << bind.mRotation.y << ','
                          << bind.mRotation.z
                          << '\n';
            }
            JUTNameTab* material_names = model->getMaterialName();
            for (u16 shape_index = 0;
                 shape_index < model->getShapeNum();
                 ++shape_index) {
                J3DShape* shape =
                    model->getShapeNodePointer(shape_index);
                J3DMaterial* material =
                    shape != nullptr ? shape->getMaterial() : nullptr;
                const u16 material_index =
                    material != nullptr ? material->getIndex() : 0xffff;
                std::cout << "LINK_BODY_SHAPE_METRICS"
                          << " shape_index=" << shape_index
                          << " material_index=" << material_index
                          << " material_name="
                          << (material_names != nullptr &&
                                      material_index != 0xffff
                                  ? material_names->getName(material_index)
                                  : "unnamed")
                          << '\n';
            }
        }
        report_model_metrics(
            name,
            static_cast<const u8*>(resource),
            archive.getResSize(resource),
            *model);
    }
    std::cout << "ASSEMBLY_METRICS"
              << " raw_positions=" << assembly_metrics.positions
              << " raw_triangles=" << assembly_metrics.triangles
              << " runtime_vertices=" << assembly_metrics.runtime_vertices
              << " runtime_triangles="
              << assembly_metrics.nondegenerate_triangles
              << " chunks16=" << assembly_metrics.chunks16
              << " draws_without_material=" << assembly_metrics.draws
              << " combined_bounds=unproven"
              << " attached=al_head.bmd,al_hands.bmd,al_face.bmd"
              << " uncertain=none\n";
}

struct AnimationResource {
    std::unique_ptr<std::uint8_t, decltype(&std::free)> bytes{
        nullptr,
        &std::free};
    u32 size = 0;
    u32 stored_size;
};

AnimationResource extract_animation(
    std::vector<std::uint8_t>& animation_archive_bytes) {
    JKRMemArchive archive(
        animation_archive_bytes.data(),
        static_cast<u32>(animation_archive_bytes.size()),
        JKRMEMBREAK_FLAG_UNKNOWN0);
    const u16 animation_resource_id = static_cast<u16>(std::strtoul(
        environment_or("DUSKLIGHT_TEST_ANIMATION_ID", "0x26a"),
        nullptr,
        0));
    const char* expected_name =
        environment_or("DUSKLIGHT_TEST_ANIMATION_NAME", "waits.bck");
    const char* resource_name = nullptr;
    for (u32 index = 0; index < archive.countFile(); ++index) {
        JKRArchive::SDirEntry entry = {};
        if (archive.getDirEntry(&entry, index) &&
            entry.id == animation_resource_id) {
            resource_name = entry.name;
            break;
        }
    }
    if (resource_name == nullptr ||
        std::strcmp(resource_name, expected_name) != 0) {
        throw std::runtime_error(
            std::string("animation resource name mismatch: expected ") +
            expected_name);
    }
    void* resource = archive.getResource(animation_resource_id);
    if (resource == nullptr) {
        throw std::runtime_error("missing BCK resource");
    }
    loaded_animation_name = resource_name;
    loaded_animation_id = animation_resource_id;
    const u32 stored_size = archive.getResSize(resource);
    const auto* first = static_cast<const std::uint8_t*>(resource);
    u32 expanded_size = stored_size;
    const bool compressed =
        stored_size >= 16 && std::memcmp(first, "Yaz0", 4) == 0;
    if (compressed) {
        expanded_size =
            JKRDecompExpandSize(const_cast<std::uint8_t*>(first));
    }
    auto* expanded = static_cast<std::uint8_t*>(std::malloc(expanded_size));
    if (expanded == nullptr) {
        throw std::runtime_error("unable to allocate BCK extraction buffer");
    }
    if (compressed) {
        JKRDecomp::decode(
            const_cast<std::uint8_t*>(first),
            expanded,
            expanded_size,
            0);
    } else {
        std::memcpy(expanded, first, expanded_size);
    }
    return {
        std::unique_ptr<std::uint8_t, decltype(&std::free)>(
            expanded,
            &std::free),
        expanded_size,
        stored_size};
}

void load_animation(AnimationResource& resource) {
    J3DAnmBase* animation = J3DAnmLoaderDataBase::load(resource.bytes.get());
    auto* transform = dynamic_cast<J3DAnmTransformKey*>(animation);
    if (transform == nullptr) {
        throw std::runtime_error("resource is not a J3DAnmTransformKey");
    }
    loaded_animation = transform;
    bool frame_zero_finite = true;
    transform->setFrame(0.0f);
    for (u16 joint = 0; joint < transform->field_0x1e; ++joint) {
        J3DTransformInfo value = {};
        transform->getTransform(joint, &value);
        frame_zero_finite &=
            std::isfinite(value.mScale.x) &&
            std::isfinite(value.mScale.y) &&
            std::isfinite(value.mScale.z) &&
            std::isfinite(value.mTranslate.x) &&
            std::isfinite(value.mTranslate.y) &&
            std::isfinite(value.mTranslate.z);
    }
    if (!frame_zero_finite) {
        throw std::runtime_error("non-finite frame-zero BCK transform");
    }
    std::cout << "animation name=" << loaded_animation_name
              << " id=0x" << std::hex << loaded_animation_id << std::dec
              << " stored_bytes=" << resource.stored_size
              << " bytes=" << resource.size
              << " kind=J3DAnmTransformKey\n";
    std::cout << "ANIMATION_METRICS"
              << " name=" << loaded_animation_name
              << " id=0x" << std::hex << loaded_animation_id << std::dec
              << " magic=" << std::string(
                     reinterpret_cast<const char*>(resource.bytes.get()),
                     8)
              << " type=ANK1"
              << " duration=" << transform->getFrameMax()
              << " cadence=not_encoded"
              << " loop_mode="
              << static_cast<unsigned>(transform->getAttribute())
              << " tracks=" << transform->field_0x1e
              << " affected_joints=" << transform->field_0x1e
              << " frame0_valid=1"
              << " finite=1"
              << " main_skeleton_joints=" << assembly_metrics.main_joints
              << " compatible="
              << (transform->field_0x1e == assembly_metrics.main_joints)
              << " joints_without_track="
              << (assembly_metrics.main_joints > transform->field_0x1e
                      ? assembly_metrics.main_joints - transform->field_0x1e
                      : 0)
              << " fallback=bind_pose_for_untracked"
              << '\n';
}

struct BakedVertex {
    u32 color;
    f32 x;
    f32 y;
    f32 z;
};

static_assert(sizeof(BakedVertex) == 16);
static_assert(offsetof(BakedVertex, color) == 0);
static_assert(offsetof(BakedVertex, x) == 4);
static_assert(offsetof(BakedVertex, y) == 8);
static_assert(offsetof(BakedVertex, z) == 12);

struct VertexKey {
    u16 shape;
    u16 position;
    u16 normal;
    u16 uv;
    u16 draw_matrix;

    bool operator==(const VertexKey&) const = default;
};

struct VertexKeyHash {
    std::size_t operator()(const VertexKey& key) const {
        std::size_t value = key.shape;
        value = value * 65537u + key.position;
        value = value * 65537u + key.normal;
        value = value * 65537u + key.uv;
        return value * 65537u + key.draw_matrix;
    }
};

struct BakedChunk {
    u32 part;
    const char* name;
    u32 source_hash;
    std::vector<BakedVertex> vertices;
    std::vector<u16> indices;
    Vec minimum = {INFINITY, INFINITY, INFINITY};
    Vec maximum = {-INFINITY, -INFINITY, -INFINITY};
};

u32 fnv1a(const char* text) {
    u32 hash = 2166136261u;
    for (; *text != '\0'; ++text) {
        hash ^= static_cast<u8>(*text);
        hash *= 16777619u;
    }
    return hash;
}

Vec transform_position(MtxP matrix, const Vec& value) {
    return {
        matrix[0][0] * value.x + matrix[0][1] * value.y +
            matrix[0][2] * value.z + matrix[0][3],
        matrix[1][0] * value.x + matrix[1][1] * value.y +
            matrix[1][2] * value.z + matrix[1][3],
        matrix[2][0] * value.x + matrix[2][1] * value.y +
            matrix[2][2] * value.z + matrix[2][3],
    };
}

Vec transform_normal(MtxP matrix, const Vec& value) {
    const f32 a = matrix[0][0];
    const f32 b = matrix[0][1];
    const f32 c = matrix[0][2];
    const f32 d = matrix[1][0];
    const f32 e = matrix[1][1];
    const f32 f = matrix[1][2];
    const f32 g = matrix[2][0];
    const f32 h = matrix[2][1];
    const f32 i = matrix[2][2];
    const f32 determinant =
        a * (e * i - f * h) -
        b * (d * i - f * g) +
        c * (d * h - e * g);
    if (!std::isfinite(determinant) || std::fabs(determinant) < 1.0e-12f) {
        throw std::runtime_error("non-invertible normal matrix");
    }
    const f32 inverse = 1.0f / determinant;
    Vec result = {
        ((e * i - f * h) * value.x +
         (c * h - b * i) * value.y +
         (b * f - c * e) * value.z) * inverse,
        ((f * g - d * i) * value.x +
         (a * i - c * g) * value.y +
         (c * d - a * f) * value.z) * inverse,
        ((d * h - e * g) * value.x +
         (b * g - a * h) * value.y +
         (a * e - b * d) * value.z) * inverse,
    };
    const f32 length =
        std::sqrt(result.x * result.x + result.y * result.y +
                  result.z * result.z);
    if (!std::isfinite(length) || length < 1.0e-8f) {
        throw std::runtime_error("invalid transformed normal");
    }
    result.x /= length;
    result.y /= length;
    result.z /= length;
    return result;
}

u32 diagnostic_color(const Vec& normal) {
    constexpr f32 lx_unscaled = 0.35f;
    constexpr f32 ly_unscaled = 0.80f;
    constexpr f32 lz_unscaled = 0.48f;
    constexpr f32 length = 0.99644368f;
    const f32 dot =
        normal.x * (lx_unscaled / length) +
        normal.y * (ly_unscaled / length) +
        normal.z * (lz_unscaled / length);
    const f32 raw = 0.25f + 0.75f * std::max(dot, 0.0f);
    const f32 quantized =
        0.25f + std::round((raw - 0.25f) / 0.1875f) * 0.1875f;
    const u32 gray = static_cast<u32>(std::lround(quantized * 255.0f));
    return gray | (gray << 8) | (gray << 16) | 0xff000000u;
}

MtxP source_matrix(
    J3DModel& model,
    J3DModelData& data,
    u16 draw_matrix) {
    if (draw_matrix >= data.getDrawMtxNum()) {
        throw std::runtime_error("draw matrix index outside DRW1");
    }
    const u16 index = data.getDrawMtxIndex(draw_matrix);
    if (data.getDrawMtxFlag(draw_matrix) == 0) {
        if (index >= data.getJointNum()) {
            throw std::runtime_error("rigid joint index outside JNT1");
        }
        return model.getAnmMtx(index);
    }
    if (index >= data.getWEvlpMtxNum()) {
        throw std::runtime_error("envelope index outside EVP1");
    }
    return model.getWeightAnmMtx(index);
}

Vec source_position(J3DVertexData& data, u16 index) {
    if (data.getVtxPosType() != GX_F32 || index >= data.getVtxNum()) {
        throw std::runtime_error("unsupported or invalid position");
    }
    return static_cast<Vec*>(data.getVtxPosArray())[index];
}

Vec source_normal(J3DVertexData& data, u16 index) {
    if (index >= data.getNrmNum()) {
        throw std::runtime_error("normal index outside VTX1");
    }
    if (data.getVtxNrmType() == GX_F32) {
        return static_cast<Vec*>(data.getVtxNrmArray())[index];
    }
    if (data.getVtxNrmType() != GX_S16) {
        throw std::runtime_error("unsupported normal component type");
    }
    const s16* values = static_cast<s16*>(data.getVtxNrmArray()) + index * 3;
    const f32 scale = 1.0f / static_cast<f32>(1u << data.getVtxNrmFrac());
    return {values[0] * scale, values[1] * scale, values[2] * scale};
}

u16 append_corner(
    const u8* vertex,
    const std::vector<AttributeLayout>& layout,
    u16 shape_index,
    const std::array<u16, 10>& resolved_matrices,
    u16 resolved_matrix_count,
    J3DModel& model,
    BakedChunk& chunk,
    std::unordered_map<VertexKey, u16, VertexKeyHash>& deduplicated) {
    bool has_position = false;
    bool has_normal = false;
    bool has_uv = false;
    bool has_matrix = false;
    const u16 position =
        vertex_index(vertex, layout, GX_VA_POS, &has_position);
    const u16 normal =
        vertex_index(vertex, layout, GX_VA_NRM, &has_normal);
    const u16 uv =
        vertex_index(vertex, layout, GX_VA_TEX0, &has_uv);
    const u16 matrix_register =
        vertex_index(vertex, layout, GX_VA_PNMTXIDX, &has_matrix);
    if (!has_position || !has_normal) {
        throw std::runtime_error("selected corner lacks position or normal");
    }
    if (has_matrix && matrix_register % 3 != 0) {
        throw std::runtime_error("invalid GX position matrix register");
    }
    const u16 matrix_slot = has_matrix ? matrix_register / 3 : 0;
    if (matrix_slot >= resolved_matrix_count) {
        throw std::runtime_error("position matrix slot outside shape table");
    }
    const u16 draw_matrix = resolved_matrices[matrix_slot];
    if (draw_matrix == 0xffff) {
        throw std::runtime_error("unresolved inherited shape matrix");
    }
    const VertexKey key = {
        shape_index,
        position,
        normal,
        has_uv ? uv : static_cast<u16>(0xffff),
        draw_matrix,
    };
    const auto found = deduplicated.find(key);
    if (found != deduplicated.end()) {
        return found->second;
    }
    if (chunk.vertices.size() >= 65535) {
        throw std::runtime_error("DPMD chunk exceeds 16-bit vertex limit");
    }
    J3DVertexData& data = model.getModelData()->getVertexData();
    MtxP matrix = source_matrix(model, *model.getModelData(), draw_matrix);
    const Vec position_value =
        transform_position(matrix, source_position(data, position));
    const Vec normal_value =
        transform_normal(matrix, source_normal(data, normal));
    if (!std::isfinite(position_value.x) ||
        !std::isfinite(position_value.y) ||
        !std::isfinite(position_value.z)) {
        throw std::runtime_error("non-finite baked position");
    }
    chunk.minimum.x = std::min(chunk.minimum.x, position_value.x);
    chunk.minimum.y = std::min(chunk.minimum.y, position_value.y);
    chunk.minimum.z = std::min(chunk.minimum.z, position_value.z);
    chunk.maximum.x = std::max(chunk.maximum.x, position_value.x);
    chunk.maximum.y = std::max(chunk.maximum.y, position_value.y);
    chunk.maximum.z = std::max(chunk.maximum.z, position_value.z);
    const u16 result = static_cast<u16>(chunk.vertices.size());
    chunk.vertices.push_back({
        diagnostic_color(normal_value),
        position_value.x,
        position_value.y,
        position_value.z,
    });
    deduplicated.emplace(key, result);
    return result;
}

void append_triangle(
    const u8* vertices,
    u32 stride,
    const std::vector<AttributeLayout>& layout,
    u16 shape_index,
    const std::array<u16, 10>& resolved_matrices,
    u16 resolved_matrix_count,
    J3DModel& model,
    BakedChunk& chunk,
    std::unordered_map<VertexKey, u16, VertexKeyHash>& deduplicated,
    u16 a,
    u16 b,
    u16 c) {
    const u16 ia = append_corner(
        vertices + stride * a, layout, shape_index, resolved_matrices,
        resolved_matrix_count,
        model, chunk, deduplicated);
    const u16 ib = append_corner(
        vertices + stride * b, layout, shape_index, resolved_matrices,
        resolved_matrix_count,
        model, chunk, deduplicated);
    const u16 ic = append_corner(
        vertices + stride * c, layout, shape_index, resolved_matrices,
        resolved_matrix_count,
        model, chunk, deduplicated);
    if (ia == ib || ib == ic || ia == ic) {
        throw std::runtime_error("unexpected degenerate selected triangle");
    }
    chunk.indices.push_back(ia);
    chunk.indices.push_back(ib);
    chunk.indices.push_back(ic);
}

void bake_shape(
    J3DModel& model,
    u16 shape_index,
    BakedChunk& chunk,
    std::unordered_map<VertexKey, u16, VertexKeyHash>& deduplicated) {
    J3DShape* shape =
        model.getModelData()->getShapeNodePointer(shape_index);
    const std::vector<AttributeLayout> layout =
        make_layout(shape->getVtxDesc());
    u32 stride = 0;
    for (const AttributeLayout& attribute : layout) {
        stride += attribute.size;
    }
    std::array<u16, 10> resolved_matrices;
    resolved_matrices.fill(0xffff);
    for (u16 group = 0; group < shape->getMtxGroupNum(); ++group) {
        J3DShapeMtx& shape_mtx = *shape->getShapeMtx(group);
        const u16 matrix_count = shape_mtx.getUseMtxNum();
        if (matrix_count > resolved_matrices.size()) {
            throw std::runtime_error("shape matrix table exceeds GX slots");
        }
        for (u16 slot = 0; slot < matrix_count; ++slot) {
            const u16 candidate = shape_mtx.getUseMtxIndex(slot);
            if (candidate != 0xffff) {
                resolved_matrices[slot] = candidate;
            } else if (resolved_matrices[slot] == 0xffff) {
                throw std::runtime_error(
                    "shape matrix inheritance lacks a preceding value");
            }
        }
        J3DShapeDraw& draw = *shape->getShapeDraw(group);
        const u8* data = draw.getDisplayList();
        const u32 size = draw.getDisplayListSize();
        for (u32 cursor = 0; cursor < size;) {
            const u8 command = data[cursor++];
            if (command == 0) {
                continue;
            }
            const u8 primitive = command & 0xf8;
            if (cursor + 2 > size) {
                throw std::runtime_error("truncated selected primitive");
            }
            const u16 count = read_be16(data + cursor);
            cursor += 2;
            const u64 payload = static_cast<u64>(count) * stride;
            if (payload > size - cursor) {
                throw std::runtime_error("selected primitive outside display list");
            }
            const u8* vertices = data + cursor;
            if (primitive == GX_TRIANGLES) {
                for (u16 vertex = 0; vertex + 2 < count; vertex += 3) {
                    append_triangle(vertices, stride, layout, shape_index,
                                    resolved_matrices, matrix_count, model,
                                    chunk, deduplicated,
                                    vertex, vertex + 1, vertex + 2);
                }
            } else if (primitive == GX_TRIANGLESTRIP) {
                for (u16 vertex = 2; vertex < count; ++vertex) {
                    const u16 a = (vertex & 1) == 0 ? vertex - 2 : vertex - 1;
                    const u16 b = (vertex & 1) == 0 ? vertex - 1 : vertex - 2;
                    append_triangle(vertices, stride, layout, shape_index,
                                    resolved_matrices, matrix_count, model,
                                    chunk, deduplicated,
                                    a, b, vertex);
                }
            } else if (primitive == GX_TRIANGLEFAN) {
                for (u16 vertex = 2; vertex < count; ++vertex) {
                    append_triangle(vertices, stride, layout, shape_index,
                                    resolved_matrices, matrix_count, model,
                                    chunk, deduplicated,
                                    0, vertex - 1, vertex);
                }
            } else if (primitive == GX_QUADS) {
                for (u16 vertex = 0; vertex + 3 < count; vertex += 4) {
                    append_triangle(vertices, stride, layout, shape_index,
                                    resolved_matrices, matrix_count, model,
                                    chunk, deduplicated,
                                    vertex, vertex + 1, vertex + 2);
                    append_triangle(vertices, stride, layout, shape_index,
                                    resolved_matrices, matrix_count, model,
                                    chunk, deduplicated,
                                    vertex + 2, vertex + 3, vertex);
                }
            } else {
                throw std::runtime_error("unsupported selected GX primitive");
            }
            cursor += static_cast<u32>(payload);
        }
    }
}

BakedChunk bake_chunk(
    u32 part,
    const char* name,
    J3DModel& model,
    const std::vector<u16>& shapes) {
    BakedChunk chunk;
    chunk.part = part;
    chunk.name = name;
    chunk.source_hash = fnv1a(name);
    std::unordered_map<VertexKey, u16, VertexKeyHash> deduplicated;
    for (u16 shape : shapes) {
        if (shape >= model.getModelData()->getShapeNum()) {
            throw std::runtime_error("selected shape outside model");
        }
        bake_shape(model, shape, chunk, deduplicated);
    }
    if (chunk.vertices.empty() ||
        chunk.indices.empty() ||
        chunk.indices.size() % 3 != 0) {
        throw std::runtime_error("empty or malformed baked chunk");
    }
    return chunk;
}

void write_u16_le(std::vector<u8>& bytes, std::size_t offset, u16 value) {
    bytes[offset] = static_cast<u8>(value);
    bytes[offset + 1] = static_cast<u8>(value >> 8);
}

void write_u32_le(std::vector<u8>& bytes, std::size_t offset, u32 value) {
    bytes[offset] = static_cast<u8>(value);
    bytes[offset + 1] = static_cast<u8>(value >> 8);
    bytes[offset + 2] = static_cast<u8>(value >> 16);
    bytes[offset + 3] = static_cast<u8>(value >> 24);
}

void write_f32_le(std::vector<u8>& bytes, std::size_t offset, f32 value) {
    u32 bits = 0;
    std::memcpy(&bits, &value, sizeof(bits));
    write_u32_le(bytes, offset, bits);
}

std::size_t align16(std::size_t value) {
    return (value + 15u) & ~std::size_t(15u);
}

u32 crc32_bytes(const std::vector<u8>& bytes) {
    u32 crc = 0xffffffffu;
    for (u8 byte : bytes) {
        crc ^= byte;
        for (int bit = 0; bit < 8; ++bit) {
            crc = (crc >> 1) ^ (0xedb88320u & (0u - (crc & 1u)));
        }
    }
    return ~crc;
}

std::vector<u8> serialize_dpmd(const std::vector<BakedChunk>& chunks) {
    constexpr std::size_t header_size = 128;
    constexpr std::size_t chunk_size = 96;
    constexpr u32 flags = 0x3;
    const std::size_t table_offset = header_size;
    const std::size_t table_bytes = chunk_size * chunks.size();
    std::size_t cursor = align16(table_offset + table_bytes);
    struct Layout {
        std::size_t vertex_offset;
        std::size_t vertex_bytes;
        std::size_t index_offset;
        std::size_t index_bytes;
    };
    std::vector<Layout> layouts;
    u32 total_vertices = 0;
    u32 total_indices = 0;
    Vec minimum = {INFINITY, INFINITY, INFINITY};
    Vec maximum = {-INFINITY, -INFINITY, -INFINITY};
    for (const BakedChunk& chunk : chunks) {
        const std::size_t vertex_bytes =
            chunk.vertices.size() * sizeof(BakedVertex);
        const std::size_t index_bytes =
            chunk.indices.size() * sizeof(u16);
        const std::size_t vertex_offset = cursor;
        cursor = align16(cursor + vertex_bytes);
        const std::size_t index_offset = cursor;
        cursor = align16(cursor + index_bytes);
        layouts.push_back(
            {vertex_offset, vertex_bytes, index_offset, index_bytes});
        total_vertices += static_cast<u32>(chunk.vertices.size());
        total_indices += static_cast<u32>(chunk.indices.size());
        minimum.x = std::min(minimum.x, chunk.minimum.x);
        minimum.y = std::min(minimum.y, chunk.minimum.y);
        minimum.z = std::min(minimum.z, chunk.minimum.z);
        maximum.x = std::max(maximum.x, chunk.maximum.x);
        maximum.y = std::max(maximum.y, chunk.maximum.y);
        maximum.z = std::max(maximum.z, chunk.maximum.z);
    }
    if (cursor > std::numeric_limits<u32>::max()) {
        throw std::runtime_error("DPMD size overflow");
    }
    std::vector<u8> bytes(cursor, 0);
    std::memcpy(bytes.data(), "DPMD", 4);
    write_u16_le(bytes, 4, 1);
    write_u16_le(bytes, 6, header_size);
    write_u32_le(bytes, 8, static_cast<u32>(bytes.size()));
    write_u32_le(bytes, 12, flags);
    write_u32_le(bytes, 16, static_cast<u32>(chunks.size()));
    write_u32_le(bytes, 20, total_vertices);
    write_u32_le(bytes, 24, total_indices);
    write_u32_le(bytes, 28, total_indices / 3);
    write_u32_le(bytes, 32, 1);
    write_u32_le(bytes, 36, 1);
    write_u32_le(bytes, 40, sizeof(BakedVertex));
    write_f32_le(bytes, 48, minimum.x);
    write_f32_le(bytes, 52, minimum.y);
    write_f32_le(bytes, 56, minimum.z);
    write_f32_le(bytes, 60, maximum.x);
    write_f32_le(bytes, 64, maximum.y);
    write_f32_le(bytes, 68, maximum.z);
    const Vec center = {
        (minimum.x + maximum.x) * 0.5f,
        (minimum.y + maximum.y) * 0.5f,
        (minimum.z + maximum.z) * 0.5f,
    };
    write_f32_le(bytes, 72, center.x);
    write_f32_le(bytes, 76, center.y);
    write_f32_le(bytes, 80, center.z);
    f32 radius_squared = 0.0f;
    for (const BakedChunk& chunk : chunks) {
        for (const BakedVertex& vertex : chunk.vertices) {
            const f32 dx = vertex.x - center.x;
            const f32 dy = vertex.y - center.y;
            const f32 dz = vertex.z - center.z;
            radius_squared = std::max(
                radius_squared, dx * dx + dy * dy + dz * dz);
        }
    }
    write_f32_le(bytes, 84, std::sqrt(radius_squared));
    write_u32_le(bytes, 88, table_offset);
    write_u32_le(bytes, 92, static_cast<u32>(table_bytes));
    write_u32_le(bytes, 96, static_cast<u32>(align16(table_offset + table_bytes)));
    write_u32_le(
        bytes, 100,
        static_cast<u32>(bytes.size() - align16(table_offset + table_bytes)));
    write_u32_le(bytes, 108, fnv1a("al.bmd+al_head.bmd+al_hands.bmd+al_face.bmd"));
    write_u32_le(bytes, 116, fnv1a("J3D_PURE_WAITS_FRAME0_NEUTRAL_HANDS_V1"));
    for (std::size_t index = 0; index < chunks.size(); ++index) {
        const BakedChunk& chunk = chunks[index];
        const Layout& layout = layouts[index];
        const std::size_t entry = table_offset + index * chunk_size;
        write_u32_le(bytes, entry, chunk.part);
        write_u32_le(bytes, entry + 4, chunk.source_hash);
        write_u32_le(bytes, entry + 12, static_cast<u32>(layout.vertex_offset));
        write_u32_le(bytes, entry + 16, static_cast<u32>(chunk.vertices.size()));
        write_u32_le(bytes, entry + 20, static_cast<u32>(layout.vertex_bytes));
        write_u32_le(bytes, entry + 24, static_cast<u32>(layout.index_offset));
        write_u32_le(bytes, entry + 28, static_cast<u32>(chunk.indices.size()));
        write_u32_le(bytes, entry + 32, static_cast<u32>(layout.index_bytes));
        write_u32_le(bytes, entry + 36, static_cast<u32>(chunk.indices.size() / 3));
        write_f32_le(bytes, entry + 40, chunk.minimum.x);
        write_f32_le(bytes, entry + 44, chunk.minimum.y);
        write_f32_le(bytes, entry + 48, chunk.minimum.z);
        write_f32_le(bytes, entry + 52, chunk.maximum.x);
        write_f32_le(bytes, entry + 56, chunk.maximum.y);
        write_f32_le(bytes, entry + 60, chunk.maximum.z);
        write_u32_le(bytes, entry + 64, 1);
        for (std::size_t item = 0; item < chunk.vertices.size(); ++item) {
            const BakedVertex& vertex = chunk.vertices[item];
            const std::size_t target =
                layout.vertex_offset + item * sizeof(BakedVertex);
            write_u32_le(bytes, target, vertex.color);
            write_f32_le(bytes, target + 4, vertex.x);
            write_f32_le(bytes, target + 8, vertex.y);
            write_f32_le(bytes, target + 12, vertex.z);
        }
        for (std::size_t item = 0; item < chunk.indices.size(); ++item) {
            write_u16_le(
                bytes,
                layout.index_offset + item * sizeof(u16),
                chunk.indices[item]);
        }
    }
    write_u32_le(bytes, 104, 0);
    write_u32_le(bytes, 104, crc32_bytes(bytes));
    return bytes;
}

void write_binary(const char* path, const std::vector<u8>& bytes) {
    std::ofstream output(path, std::ios::binary | std::ios::trunc);
    if (!output ||
        !output.write(
            reinterpret_cast<const char*>(bytes.data()),
            static_cast<std::streamsize>(bytes.size()))) {
        throw std::runtime_error(std::string("unable to write output: ") + path);
    }
}

void write_manifest(
    const char* path,
    const std::vector<BakedChunk>& chunks,
    const std::vector<u8>& dpmd) {
    std::ofstream output(path, std::ios::trunc);
    if (!output) {
        throw std::runtime_error("unable to write Link manifest");
    }
    u32 vertices = 0;
    u32 triangles = 0;
    for (const BakedChunk& chunk : chunks) {
        vertices += static_cast<u32>(chunk.vertices.size());
        triangles += static_cast<u32>(chunk.indices.size() / 3);
    }
    output << "{\n"
           << "  \"pose_contract\": "
              "\"J3D_PURE_WAITS_FRAME0_NEUTRAL_HANDS_V1\",\n"
           << "  \"diagnostic_pose\": true,\n"
           << "  \"gameplay_exact_pose\": false,\n"
           << "  \"left_hand_shape_index\": 4,\n"
           << "  \"right_hand_shape_index\": 10,\n"
           << "  \"source_inventory_triangle_count\": 6563,\n"
           << "  \"selected_visible_triangle_count\": " << triangles << ",\n"
           << "  \"runtime_triangle_count\": " << triangles << ",\n"
           << "  \"runtime_vertex_count\": " << vertices << ",\n"
           << "  \"runtime_chunk_count\": " << chunks.size() << ",\n"
           << "  \"inactive_hand_variant_triangle_count\": 2234,\n"
           << "  \"dpmd_bytes\": " << dpmd.size() << ",\n"
           << "  \"chunks\": [\n";
    for (std::size_t index = 0; index < chunks.size(); ++index) {
        const BakedChunk& chunk = chunks[index];
        output << "    {\"name\":\"" << chunk.name
               << "\",\"vertices\":" << chunk.vertices.size()
               << ",\"triangles\":" << chunk.indices.size() / 3
               << ",\"bounds\":[" << chunk.minimum.x << ','
               << chunk.minimum.y << ',' << chunk.minimum.z << ','
               << chunk.maximum.x << ',' << chunk.maximum.y << ','
               << chunk.maximum.z << "]}"
               << (index + 1 == chunks.size() ? "\n" : ",\n");
    }
    output << "  ]\n}\n";
}

void write_obj(const char* path, const std::vector<BakedChunk>& chunks) {
    std::ofstream output(path, std::ios::trunc);
    if (!output) {
        throw std::runtime_error("unable to write Link OBJ preview");
    }
    std::size_t base = 1;
    for (const BakedChunk& chunk : chunks) {
        output << "o " << chunk.name << '\n';
        for (const BakedVertex& vertex : chunk.vertices) {
            output << "v " << vertex.x << ' ' << vertex.y << ' ' << vertex.z << '\n';
        }
        for (std::size_t index = 0; index < chunk.indices.size(); index += 3) {
            output << "f "
                   << base + chunk.indices[index] << ' '
                   << base + chunk.indices[index + 1] << ' '
                   << base + chunk.indices[index + 2] << '\n';
        }
        base += chunk.vertices.size();
    }
}

void bake_link_diagnostic_pose() {
    const char* output_path = std::getenv("DUSKLIGHT_DPMD_OUTPUT");
    if (output_path == nullptr || output_path[0] == '\0') {
        return;
    }
    if (loaded_animation == nullptr) {
        throw std::runtime_error("waits.bck unavailable to pose baker");
    }
    for (J3DModelData* model : loaded_models) {
        if (model == nullptr) {
            throw std::runtime_error("BMD unavailable to pose baker");
        }
    }
    J3DModel body(loaded_models[0], J3DMdlFlag_None, 1);
    J3DModel head(loaded_models[1], J3DMdlFlag_None, 1);
    J3DModel hands(loaded_models[2], J3DMdlFlag_None, 1);
    J3DModel face(loaded_models[3], J3DMdlFlag_None, 1);
    using BodyCalc = J3DMtxCalcAnimation<
        J3DMtxCalcAnimationAdaptorDefault<J3DMtxCalcCalcTransformMaya>,
        J3DMtxCalcJ3DSysInitMaya>;
    loaded_animation->setFrame(0.0f);
    BodyCalc body_calc(loaded_animation);
    loaded_models[0]->getJointNodePointer(0)->setMtxCalc(&body_calc);
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

    std::vector<u16> all_body_shapes;
    for (u16 index = 0; index < loaded_models[0]->getShapeNum(); ++index) {
        all_body_shapes.push_back(index);
    }
    std::vector<u16> all_head_shapes;
    for (u16 index = 0; index < loaded_models[1]->getShapeNum(); ++index) {
        all_head_shapes.push_back(index);
    }
    std::vector<u16> all_face_shapes;
    for (u16 index = 0; index < loaded_models[3]->getShapeNum(); ++index) {
        all_face_shapes.push_back(index);
    }
    std::vector<BakedChunk> chunks;
    chunks.push_back(bake_chunk(1, "body", body, all_body_shapes));
    chunks.push_back(bake_chunk(2, "head", head, all_head_shapes));
    chunks.push_back(bake_chunk(3, "face", face, all_face_shapes));
    chunks.push_back(bake_chunk(4, "left_hand", hands, {4}));
    chunks.push_back(bake_chunk(5, "right_hand", hands, {10}));
    u32 triangles = 0;
    for (const BakedChunk& chunk : chunks) {
        triangles += static_cast<u32>(chunk.indices.size() / 3);
        std::cout << "BAKED_CHUNK"
                  << " name=" << chunk.name
                  << " vertices=" << chunk.vertices.size()
                  << " triangles=" << chunk.indices.size() / 3
                  << " bounds=" << chunk.minimum.x << ',' << chunk.minimum.y << ','
                  << chunk.minimum.z << ':' << chunk.maximum.x << ','
                  << chunk.maximum.y << ',' << chunk.maximum.z
                  << '\n';
    }
    if (triangles != 4329 || chunks.size() != 5) {
        throw std::runtime_error("diagnostic pose triangle or chunk count mismatch");
    }
    const std::vector<u8> dpmd = serialize_dpmd(chunks);
    write_binary(output_path, dpmd);
    const char* manifest = std::getenv("DUSKLIGHT_DPMD_MANIFEST");
    if (manifest != nullptr && manifest[0] != '\0') {
        write_manifest(manifest, chunks, dpmd);
    }
    const char* preview = std::getenv("DUSKLIGHT_DPMD_PREVIEW");
    if (preview != nullptr && preview[0] != '\0') {
        write_obj(preview, chunks);
    }
    const char* hand_preview = std::getenv("DUSKLIGHT_HAND_PREVIEW");
    if (hand_preview != nullptr && hand_preview[0] != '\0') {
        std::vector<BakedChunk> hand_chunks;
        static constexpr std::array<const char*, 11> names = {
            "hand_shape_0", "hand_shape_1", "hand_shape_2",
            "hand_shape_3", "hand_shape_4_selected_left",
            "hand_shape_5", "hand_shape_6", "hand_shape_7",
            "hand_shape_8", "hand_shape_9",
            "hand_shape_10_selected_right",
        };
        for (u16 shape = 0; shape < names.size(); ++shape) {
            hand_chunks.push_back(
                bake_chunk(100 + shape, names[shape], hands, {shape}));
        }
        write_obj(hand_preview, hand_chunks);
    }
    std::cout << "LINK_DIAGNOSTIC_POSE_OK"
              << " triangles=" << triangles
              << " chunks=" << chunks.size()
              << " bytes=" << dpmd.size()
              << " crc=0x" << std::hex << std::uppercase
              << (static_cast<u32>(dpmd[104]) |
                  (static_cast<u32>(dpmd[105]) << 8) |
                  (static_cast<u32>(dpmd[106]) << 16) |
                  (static_cast<u32>(dpmd[107]) << 24))
              << std::dec
              << '\n';
}

}  // namespace

int main() {
    const char* fixture_path = std::getenv("DUSKLIGHT_TEST_RARC_FIXTURE");
    if (fixture_path != nullptr && fixture_path[0] != '\0') {
        try {
            std::ifstream stream(fixture_path, std::ios::binary);
            std::vector<std::uint8_t> fixture{
                std::istreambuf_iterator<char>(stream),
                std::istreambuf_iterator<char>()};
            validate_rarc(fixture, "synthetic fixture");
            std::cout << "RARC_FIXTURE_OK\n";
            return 0;
        } catch (const std::exception& error) {
            std::cerr << "LINK_PROBE_ERROR code=rarc_fixture message="
                      << error.what() << '\n';
            return 4;
        }
    }
    const char* image_path = std::getenv("DUSKLIGHT_GAME_IMAGE");
    if (image_path == nullptr || image_path[0] == '\0') {
        std::cerr << "LINK_PROBE_ERROR code=missing_game_image\n";
        return 2;
    }

    try {
        DvdSession session(image_path);
        const DVDDiskID* id = DVDGetCurrentDiskID();
        if (id == nullptr ||
            disc_id(*id) != environment_or(
                "DUSKLIGHT_TEST_EXPECT_DISC_ID",
                "GZ2P01")) {
            throw std::runtime_error("unexpected disc ID");
        }
        const u8 expected_revision = static_cast<u8>(std::strtoul(
            environment_or("DUSKLIGHT_TEST_EXPECT_REVISION", "0"),
            nullptr,
            0));
        if (id->gameVersion != expected_revision) {
            throw std::runtime_error("unexpected disc revision");
        }

        std::cout << "disc_id=" << disc_id(*id) << '\n';
        std::cout << "disc_revision=" << static_cast<unsigned>(id->gameVersion) << '\n';
        const char* list_path = std::getenv("DUSKLIGHT_LIST_DVD_PATH");
        if (list_path != nullptr && list_path[0] != '\0') {
            list_dvd_tree(list_path);
            std::cout << "DVD_LIST_OK root=" << list_path << '\n';
            return 0;
        }
        const char* list_rarc_path =
            std::getenv("DUSKLIGHT_LIST_RARC_PATH");
        if (list_rarc_path != nullptr && list_rarc_path[0] != '\0') {
            std::vector<std::uint8_t> bytes =
                decompress_archive(read_dvd_file(list_rarc_path));
            validate_rarc(bytes, list_rarc_path);
            if (JKRExpHeap::createRoot(1, true) == nullptr) {
                throw std::runtime_error(
                    "unable to initialize JKR root heap");
            }
            JKRMemArchive archive(
                bytes.data(), static_cast<u32>(bytes.size()),
                JKRMEMBREAK_FLAG_UNKNOWN0);
            const char* wanted =
                std::getenv("DUSKLIGHT_RARC_RESOURCE_NAME");
            const char* output =
                std::getenv("DUSKLIGHT_RARC_RESOURCE_OUTPUT");
            bool wrote_wanted = false;
            for (std::uint32_t index = 0;
                 index < archive.countFile(); ++index) {
                JKRArchive::SDirEntry entry = {};
                if (!archive.getDirEntry(&entry, index) ||
                    entry.name == nullptr) {
                    continue;
                }
                const void* resource = archive.getResource(entry.id);
                if (wanted != nullptr && output != nullptr &&
                    std::strcmp(entry.name, wanted) == 0 &&
                    resource != nullptr) {
                    const u32 resource_size =
                        archive.getResSize(resource);
                    write_binary(
                        output,
                        std::vector<u8>(
                            static_cast<const u8*>(resource),
                            static_cast<const u8*>(resource) +
                                resource_size));
                    wrote_wanted = true;
                }
                std::cout << "RARC_FILE"
                          << " id=" << entry.id
                          << " name=" << entry.name
                          << " bytes="
                          << (resource == nullptr
                                  ? 0
                                  : archive.getResSize(resource))
                          << '\n';
            }
            std::cout << "RARC_LIST_OK path=" << list_rarc_path
                      << " count=" << archive.countFile() << '\n';
            if (wanted != nullptr && output != nullptr &&
                !wrote_wanted) {
                throw std::runtime_error(
                    std::string("RARC resource not found: ") + wanted);
            }
            return 0;
        }
        if (std::getenv("DUSKLIGHT_INVENTORY_ROOMS") != nullptr) {
            if (JKRExpHeap::createRoot(1, true) == nullptr) {
                throw std::runtime_error("unable to initialize JKR root heap");
            }
            inventory_room_archives();
            return 0;
        }
        if (std::getenv("DUSKLIGHT_ORIGINAL_ACTOR_EXPORT") != nullptr) {
            if (JKRExpHeap::createRoot(1, true) == nullptr) {
                throw std::runtime_error("unable to initialize JKR root heap");
            }
            export_selected_original_actor();
            return 0;
        }
        if (std::getenv("DUSKLIGHT_DYNAMIC_ACTOR_EXPORT") != nullptr) {
            if (JKRExpHeap::createRoot(1, true) == nullptr) {
                throw std::runtime_error("unable to initialize JKR root heap");
            }
            export_selected_dynamic_actor();
            return 0;
        }
        if (std::getenv("DUSKLIGHT_ORIGINAL_DOOR_EXPORT") != nullptr) {
            if (JKRExpHeap::createRoot(1, true) == nullptr) {
                throw std::runtime_error("unable to initialize JKR root heap");
            }
            export_selected_original_door();
            return 0;
        }
        if (std::getenv("DUSKLIGHT_SPINNER_SWITCH_EXPORT") != nullptr) {
            if (JKRExpHeap::createRoot(1, true) == nullptr) {
                throw std::runtime_error("unable to initialize JKR root heap");
            }
            export_selected_spinner_switch();
            return 0;
        }
        if (std::getenv("DUSKLIGHT_TBOX_EXPORT") != nullptr) {
            if (JKRExpHeap::createRoot(1, true) == nullptr) {
                throw std::runtime_error("unable to initialize JKR root heap");
            }
            export_selected_treasure_chest();
            return 0;
        }
        if (std::getenv("DUSKLIGHT_STARTUP_TITLE_EXPORT") != nullptr) {
            if (JKRExpHeap::createRoot(1, true) == nullptr) {
                throw std::runtime_error(
                    "unable to initialize JKR root heap");
            }
            export_original_startup_title();
            return 0;
        }
        if (std::getenv("DUSKLIGHT_DEMO01_STARTUP_EXPORT") != nullptr) {
            if (JKRExpHeap::createRoot(1, true) == nullptr) {
                throw std::runtime_error(
                    "unable to initialize JKR root heap");
            }
            export_demo01_startup_actors();
            return 0;
        }
        if (std::getenv("DUSKLIGHT_STARTUP_UI_EXPORT") != nullptr) {
            if (JKRExpHeap::createRoot(1, true) == nullptr) {
                throw std::runtime_error(
                    "unable to initialize JKR root heap");
            }
            export_original_startup_ui();
            return 0;
        }
        if (std::getenv("DUSKLIGHT_FILE_SELECT_UI_EXPORT") != nullptr) {
            if (JKRExpHeap::createRoot(1, true) == nullptr) {
                throw std::runtime_error(
                    "unable to initialize JKR root heap");
            }
            export_original_file_select_ui();
            return 0;
        }
        if (std::getenv("DUSKLIGHT_DPRM_OUTPUT") != nullptr ||
            std::getenv("DUSKLIGHT_ROOM_DPTX_OUTPUT") != nullptr ||
            std::getenv("DUSKLIGHT_DPCL_OUTPUT") != nullptr ||
            std::getenv("DUSKLIGHT_DPSC_OUTPUT") != nullptr) {
            if (JKRExpHeap::createRoot(1, true) == nullptr) {
                throw std::runtime_error("unable to initialize JKR root heap");
            }
            export_selected_real_room();
            return 0;
        }
        std::vector<std::uint8_t> kmdl_compressed =
            read_dvd_file(environment_or(
                "DUSKLIGHT_TEST_KMDL_PATH",
                kArchivePaths[0]));
        std::vector<std::uint8_t> animations_compressed =
            read_dvd_file(environment_or(
                "DUSKLIGHT_TEST_ANIMATION_PATH",
                kArchivePaths[1]));
        std::cout << "archive path=" << kArchivePaths[0]
                  << " bytes=" << kmdl_compressed.size()
                  << " magic=" << std::string(
                         reinterpret_cast<const char*>(kmdl_compressed.data()),
                         4)
                  << '\n';
        std::cout << "archive path=" << kArchivePaths[1]
                  << " bytes=" << animations_compressed.size()
                  << " magic=" << std::string(
                         reinterpret_cast<const char*>(animations_compressed.data()),
                         4)
                  << '\n';
        std::vector<std::uint8_t> kmdl =
            decompress_archive(std::move(kmdl_compressed));
        std::vector<std::uint8_t> animations =
            decompress_archive(std::move(animations_compressed));
        validate_rarc(kmdl, kArchivePaths[0]);
        validate_rarc(animations, kArchivePaths[1]);
        if (JKRExpHeap::createRoot(1, true) == nullptr) {
            throw std::runtime_error("unable to initialize JKR root heap");
        }
        if (std::getenv("DUSKLIGHT_DPUI_OUTPUT") != nullptr) {
            export_original_hud_from_disc();
        }
        AnimationResource animation = extract_animation(animations);
        load_models(kmdl);
        load_animation(animation);
        bake_link_diagnostic_pose();
        export_playable_model_and_animations(loaded_models, animations);
        std::cout << "LINK_J3D_LOAD_OK\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << "LINK_PROBE_ERROR code=archive_access message="
                  << error.what() << '\n';
        return 3;
    }
}
