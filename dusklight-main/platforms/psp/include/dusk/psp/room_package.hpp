#ifndef DUSK_PSP_ROOM_PACKAGE_HPP
#define DUSK_PSP_ROOM_PACKAGE_HPP

#include <cstdint>

namespace dusk::psp::room {

enum class PackageError : std::uint32_t {
    Ok,
    Missing,
    Truncated,
    Magic,
    Version,
    Size,
    Crc,
    Count,
    Range,
    Layout,
    Index,
    Material,
    Texture,
    Bucket,
    NonFinite,
    EdramBudget,
    Collision,
    Grid,
    Scene,
    Spawn,
    ActorType,
    Process,
    Parameters,
    Duplicate,
    Capacity,
};

struct PackageView {
    const std::uint8_t* bytes;
    std::uint32_t size;
    std::uint32_t expected_crc;
    std::uint32_t actual_crc;
};

struct SceneExitV3 {
    std::uint16_t source_exit_index;
    char destination_stage[9];
    std::int8_t destination_room;
    std::int8_t destination_layer;
    std::uint8_t destination_start;
    std::uint8_t wipe;
    std::uint16_t source_flags;
    std::uint16_t trigger_index;
    std::uint16_t return_exit_index;
};

struct SceneTriggerV3 {
    std::uint16_t source_type;
    std::uint16_t process_id;
    std::uint32_t name_hash;
    std::uint32_t parameters;
    float position[3];
    std::int16_t rotation[3];
    std::uint16_t shape;
    float source_scale[3];
    float dimensions[3];
    std::uint8_t automatic;
    std::uint8_t exit_index;
    std::uint8_t visual_fallback;
    std::uint8_t logic_fallback;
    std::uint32_t flags;
};

struct SceneSpawnV3 {
    std::uint8_t start_index;
    std::uint8_t type;
    float position[3];
    std::int16_t rotation[3];
    std::int8_t layer;
    std::uint32_t parameters;
    bool floor_valid;
    float floor_height;
    float floor_normal[3];
};

struct SceneActorV3 {
    char source_name[9];
    std::uint32_t name_hash;
    std::uint16_t process_id;
    std::uint16_t mapping_version;
    std::uint32_t parameters;
    float position[3];
    std::int16_t rotation[3];
    std::uint8_t room;
    std::uint8_t layer;
    std::uint8_t supported;
    std::uint16_t source_index;
    float scale[3];
    std::uint32_t table_hash;
};

struct EnvironmentRecordV4 {
    std::uint32_t stage_hash;
    std::uint32_t room_index;
    std::uint16_t environment_id;
    std::uint8_t pattern;
    std::uint8_t schedule_slot;
    std::uint16_t pselect_id;
    std::uint16_t palette_id;
    std::uint32_t flags;
    std::uint32_t ambient_room;
    std::uint32_t ambient_actor;
    std::uint32_t key_light_color;
    std::uint32_t fog_color;
    std::uint32_t clear_color;
    std::uint32_t local_light_color;
    float key_light_direction[3];
    float local_light_position[3];
    float local_light_power;
    float fog_near;
    float fog_far;
    float shadow_density;
    float shadow_direction[3];
    float transition_rate;
    std::uint32_t local_light_count;
    std::uint32_t source_counts;
};

std::uint16_t read_u16(const std::uint8_t* bytes);
std::uint32_t read_u32(const std::uint8_t* bytes);
float read_f32(const std::uint8_t* bytes);
std::uint32_t package_crc32(
    const std::uint8_t* bytes, std::uint32_t size);
PackageError validate_dprm(
    const void* bytes, std::uint32_t size, PackageView* view);
PackageError validate_room_dptx(
    const void* bytes, std::uint32_t size, PackageView* view);
PackageError validate_dpcl(
    const void* bytes, std::uint32_t size, PackageView* view);
PackageError validate_dpsc(
    const void* bytes, std::uint32_t size, PackageView* view);
PackageError read_dpsc_exit_v3(
    const PackageView& view,
    std::uint32_t index,
    SceneExitV3* exit);
PackageError read_dpsc_trigger_v3(
    const PackageView& view,
    std::uint32_t index,
    SceneTriggerV3* trigger);
PackageError read_dpsc_spawn_v3(
    const PackageView& view,
    std::uint32_t index,
    SceneSpawnV3* spawn);
PackageError find_dpsc_spawn_v3(
    const PackageView& view,
    std::uint8_t start_index,
    SceneSpawnV3* spawn);
PackageError read_dpsc_actor_v3(
    const PackageView& view,
    std::uint32_t index,
    SceneActorV3* actor);
PackageError read_dpsc_environment_v4(
    const PackageView& view,
    EnvironmentRecordV4* environment);
const char* package_error_name(PackageError error);

}  // namespace dusk::psp::room

#endif
