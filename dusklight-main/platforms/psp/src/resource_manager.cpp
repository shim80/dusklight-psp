#include "dusk/psp/resource_manager.hpp"

#include "dusk/psp/room_package.hpp"

#include <cstdio>
#include <cstring>

namespace dusk::psp::resources {
namespace {

constexpr char kHeader[] = "DUSKLIGHT_RESOURCE_MANIFEST_V1\n";

bool copy_field(
    const char* begin, const char* end,
    char* output, std::uint32_t capacity) {
    if (begin == nullptr || end == nullptr || end <= begin ||
        output == nullptr || capacity == 0) {
        return false;
    }
    const std::uint32_t length =
        static_cast<std::uint32_t>(end - begin);
    if (length >= capacity) {
        return false;
    }
    std::memcpy(output, begin, length);
    output[length] = '\0';
    return true;
}

bool parse_hex(const char* text, std::uint32_t* value) {
    if (text == nullptr || value == nullptr || std::strlen(text) != 8) {
        return false;
    }
    std::uint32_t result = 0;
    for (std::uint32_t index = 0; index < 8; ++index) {
        const char digit = text[index];
        const std::uint32_t nibble =
            digit >= '0' && digit <= '9' ? digit - '0' :
            digit >= 'A' && digit <= 'F' ? digit - 'A' + 10 :
            digit >= 'a' && digit <= 'f' ? digit - 'a' + 10 : 16;
        if (nibble > 15) {
            return false;
        }
        result = (result << 4) | nibble;
    }
    *value = result;
    return true;
}

}  // namespace

const char* resource_type_name(PspResourceType type) {
    switch (type) {
    case PspResourceType::SkinnedModel: return "SkinnedModel";
    case PspResourceType::TextureArchive: return "TextureArchive";
    case PspResourceType::AnimationArchive: return "AnimationArchive";
    case PspResourceType::UiArchive: return "UiArchive";
    case PspResourceType::RoomModel: return "RoomModel";
    case PspResourceType::StaticModel: return "StaticModel";
    case PspResourceType::RoomCollision: return "RoomCollision";
    case PspResourceType::Scene: return "Scene";
    case PspResourceType::Invalid: return "Invalid";
    }
    return "Invalid";
}

PspResourceType parse_resource_type(const char* text) {
    if (text == nullptr) return PspResourceType::Invalid;
    for (std::uint8_t raw = 0;
         raw < static_cast<std::uint8_t>(PspResourceType::Invalid);
         ++raw) {
        const auto type = static_cast<PspResourceType>(raw);
        if (std::strcmp(text, resource_type_name(type)) == 0) {
            return type;
        }
    }
    return PspResourceType::Invalid;
}

bool PspResourceManager::initialize(
    const char* root, const void* manifest,
    std::uint32_t manifest_size,
    ReadResource reader, void* reader_user) {
    shutdown();
    if (root == nullptr || manifest == nullptr || reader == nullptr ||
        manifest_size <= sizeof(kHeader) - 1 ||
        std::strncmp(
            static_cast<const char*>(manifest), kHeader,
            sizeof(kHeader) - 1) != 0 ||
        std::snprintf(root_, sizeof(root_), "%s", root) <= 0) {
        ++metrics.errors;
        return false;
    }
    const char* cursor =
        static_cast<const char*>(manifest) + sizeof(kHeader) - 1;
    const char* limit = static_cast<const char*>(manifest) + manifest_size;
    while (cursor < limit) {
        const char* newline = static_cast<const char*>(
            std::memchr(cursor, '\n', limit - cursor));
        const char* line_end = newline != nullptr ? newline : limit;
        if (line_end == cursor) {
            cursor = newline != nullptr ? newline + 1 : limit;
            continue;
        }
        if (entry_count_ >= kEntryCapacity) {
            ++metrics.errors;
            shutdown();
            return false;
        }
        const char* first = static_cast<const char*>(
            std::memchr(cursor, '|', line_end - cursor));
        const char* second = first == nullptr ? nullptr :
            static_cast<const char*>(
                std::memchr(first + 1, '|', line_end - first - 1));
        const char* third = second == nullptr ? nullptr :
            static_cast<const char*>(
                std::memchr(second + 1, '|', line_end - second - 1));
        char type_text[32] = {};
        char crc_text[9] = {};
        Entry entry = {};
        if (first == nullptr || second == nullptr || third == nullptr ||
            !copy_field(cursor, first, entry.id, sizeof(entry.id)) ||
            !copy_field(first + 1, second, type_text, sizeof(type_text)) ||
            !copy_field(second + 1, third, entry.path, sizeof(entry.path)) ||
            !copy_field(third + 1, line_end, crc_text, sizeof(crc_text)) ||
            (entry.type = parse_resource_type(type_text)) ==
                PspResourceType::Invalid ||
            !parse_hex(crc_text, &entry.crc) ||
            entry.path[0] == '/' || std::strstr(entry.path, "..") != nullptr ||
            std::strchr(entry.path, ':') != nullptr ||
            find(entry.id) != nullptr) {
            ++metrics.errors;
            shutdown();
            return false;
        }
        entries_[entry_count_++] = entry;
        cursor = newline != nullptr ? newline + 1 : limit;
    }
    if (entry_count_ == 0) {
        ++metrics.errors;
        shutdown();
        return false;
    }
    reader_ = reader;
    reader_user_ = reader_user;
    initialized_ = true;
    metrics.manifest_entries = entry_count_;
    return true;
}

void PspResourceManager::shutdown() {
    std::memset(entries_, 0, sizeof(entries_));
    std::memset(slots_, 0, sizeof(slots_));
    root_[0] = '\0';
    reader_ = nullptr;
    reader_user_ = nullptr;
    entry_count_ = 0;
    initialized_ = false;
}

const PspResourceManager::Entry*
PspResourceManager::find(const char* symbolic_id) const {
    if (symbolic_id == nullptr) {
        return nullptr;
    }
    for (std::uint16_t index = 0; index < entry_count_; ++index) {
        if (std::strcmp(entries_[index].id, symbolic_id) == 0) {
            return &entries_[index];
        }
    }
    return nullptr;
}

bool PspResourceManager::load(
    const char* symbolic_id, PspResourceType expected_type,
    void* output, std::uint32_t capacity,
    PspResourceHandle* handle, std::uint32_t* output_size) {
    const Entry* entry = find(symbolic_id);
    if (!initialized_ || entry == nullptr || output == nullptr ||
        capacity == 0 || handle == nullptr || output_size == nullptr) {
        ++metrics.errors;
        return false;
    }
    if (entry->type != expected_type) {
        ++metrics.wrong_types;
        ++metrics.errors;
        return false;
    }
    std::uint16_t slot_index = 0;
    while (slot_index < kSlotCapacity && slots_[slot_index].active) {
        ++slot_index;
    }
    if (slot_index == kSlotCapacity) {
        ++metrics.errors;
        return false;
    }
    char path[256] = {};
    const int length = std::snprintf(
        path, sizeof(path), "%s/%s", root_, entry->path);
    std::uint32_t loaded_size = 0;
    if (length <= 0 || static_cast<std::uint32_t>(length) >= sizeof(path) ||
        !reader_(
            reader_user_, path, output, capacity, &loaded_size) ||
        loaded_size == 0) {
        ++metrics.errors;
        return false;
    }
    const std::uint32_t actual_crc =
        room::package_crc32(
            static_cast<const std::uint8_t*>(output), loaded_size);
    if (actual_crc != entry->crc) {
        ++metrics.crc_failures;
        ++metrics.errors;
        return false;
    }
    Slot& slot = slots_[slot_index];
    slot.generation = slot.generation == 0xFFFFu
        ? 1u : static_cast<std::uint16_t>(slot.generation + 1u);
    slot.data = output;
    slot.size = loaded_size;
    slot.references = 1;
    slot.type = entry->type;
    slot.active = true;
    *handle = {slot_index, slot.generation, slot.type};
    *output_size = loaded_size;
    ++metrics.load_calls;
    ++metrics.generation_changes;
    return true;
}

bool PspResourceManager::release(PspResourceHandle handle) {
    if (!handle_valid(handle)) {
        ++metrics.stale_handles;
        return false;
    }
    Slot& slot = slots_[handle.slot];
    if (--slot.references == 0) {
        slot.data = nullptr;
        slot.size = 0;
        slot.type = PspResourceType::Invalid;
        slot.active = false;
    }
    ++metrics.release_calls;
    return true;
}

bool PspResourceManager::handle_valid(PspResourceHandle handle) const {
    return initialized_ && handle.slot < kSlotCapacity &&
           slots_[handle.slot].active &&
           slots_[handle.slot].generation == handle.generation &&
           slots_[handle.slot].type == handle.type;
}

const void* PspResourceManager::data(PspResourceHandle handle) const {
    return handle_valid(handle) ? slots_[handle.slot].data : nullptr;
}

std::uint32_t PspResourceManager::size(PspResourceHandle handle) const {
    return handle_valid(handle) ? slots_[handle.slot].size : 0;
}

PspResourceType PspResourceManager::type(
    PspResourceHandle handle) const {
    return handle_valid(handle)
        ? slots_[handle.slot].type : PspResourceType::Invalid;
}

bool PspResourceManager::initialized() const {
    return initialized_;
}

}  // namespace dusk::psp::resources
