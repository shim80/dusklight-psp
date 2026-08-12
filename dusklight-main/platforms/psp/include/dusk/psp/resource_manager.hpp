#ifndef DUSK_PSP_RESOURCE_MANAGER_HPP
#define DUSK_PSP_RESOURCE_MANAGER_HPP

#include <cstdint>

namespace dusk::psp::resources {

enum class PspResourceType : std::uint8_t {
    SkinnedModel,
    TextureArchive,
    AnimationArchive,
    UiArchive,
    RoomModel,
    StaticModel,
    RoomCollision,
    Scene,
    Invalid,
};

struct PspResourceHandle {
    std::uint16_t slot;
    std::uint16_t generation;
    PspResourceType type;
};

using ReadResource = bool (*)(
    void* user, const char* path, void* output,
    std::uint32_t capacity, std::uint32_t* size);

struct Metrics {
    std::uint32_t manifest_entries;
    std::uint32_t load_calls;
    std::uint32_t release_calls;
    std::uint32_t generation_changes;
    std::uint32_t stale_handles;
    std::uint32_t wrong_types;
    std::uint32_t crc_failures;
    std::uint32_t errors;
};

class PspResourceManager {
public:
    static constexpr std::uint16_t kEntryCapacity = 48;
    static constexpr std::uint16_t kSlotCapacity = 24;

    bool initialize(
        const char* root, const void* manifest,
        std::uint32_t manifest_size,
        ReadResource reader, void* reader_user);
    void shutdown();

    bool load(
        const char* symbolic_id, PspResourceType expected_type,
        void* output, std::uint32_t capacity,
        PspResourceHandle* handle, std::uint32_t* size);
    bool release(PspResourceHandle handle);
    bool handle_valid(PspResourceHandle handle) const;
    const void* data(PspResourceHandle handle) const;
    std::uint32_t size(PspResourceHandle handle) const;
    PspResourceType type(PspResourceHandle handle) const;

    bool initialized() const;
    Metrics metrics = {};

private:
    struct Entry {
        char id[96];
        char path[128];
        PspResourceType type;
        std::uint32_t crc;
    };

    struct Slot {
        const void* data;
        std::uint32_t size;
        std::uint16_t generation;
        std::uint16_t references;
        PspResourceType type;
        bool active;
    };

    const Entry* find(const char* symbolic_id) const;

    Entry entries_[kEntryCapacity] = {};
    Slot slots_[kSlotCapacity] = {};
    char root_[128] = {};
    ReadResource reader_ = nullptr;
    void* reader_user_ = nullptr;
    std::uint16_t entry_count_ = 0;
    bool initialized_ = false;
};

const char* resource_type_name(PspResourceType type);
PspResourceType parse_resource_type(const char* text);

}  // namespace dusk::psp::resources

#endif
