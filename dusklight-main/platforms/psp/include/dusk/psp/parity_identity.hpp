#ifndef DUSK_PSP_PARITY_IDENTITY_HPP
#define DUSK_PSP_PARITY_IDENTITY_HPP

#include <cstddef>
#include <cstdint>

namespace dusk::psp::parity {

struct ParitySceneId {
    std::uint32_t scene_process_id;
    std::uint32_t stage_hash;
    std::int16_t room;
    std::int16_t layer;
    std::uint32_t generation;
};

struct ParityActorId {
    std::uint32_t stage_hash;
    std::int16_t room;
    std::int16_t layer;
    std::uint32_t source_table_hash;
    std::uint16_t source_index;
    std::uint16_t process_id;
    std::uint32_t generation;
};

struct ParityModelId {
    ParityActorId actor;
    std::uint32_t archive_hash;
    std::uint32_t resource_id;
    std::uint16_t model_slot;
    std::uint16_t generation;
};

bool operator==(const ParitySceneId& left, const ParitySceneId& right);
bool operator==(const ParityActorId& left, const ParityActorId& right);
bool operator==(const ParityModelId& left, const ParityModelId& right);

std::uint64_t stable_hash(const ParitySceneId& id);
std::uint64_t stable_hash(const ParityActorId& id);
std::uint64_t stable_hash(const ParityModelId& id);

bool format_id(
    const ParitySceneId& id, char* output, std::size_t capacity);
bool format_id(
    const ParityActorId& id, char* output, std::size_t capacity);
bool format_id(
    const ParityModelId& id, char* output, std::size_t capacity);

}  // namespace dusk::psp::parity

#endif
