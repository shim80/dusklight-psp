#include "dusk/psp/parity_trace.hpp"

#include <cstdio>
#include <cstring>

namespace dusk::psp::parity {
namespace {

constexpr std::uint64_t kFnvOffset = 14695981039346656037ull;
constexpr std::uint64_t kFnvPrime = 1099511628211ull;

void hash_bytes(
    std::uint64_t* hash, const void* bytes, std::size_t size) {
    const auto* input = static_cast<const std::uint8_t*>(bytes);
    for (std::size_t index = 0; index < size; ++index) {
        *hash ^= input[index];
        *hash *= kFnvPrime;
    }
}

template <typename T>
void hash_value(std::uint64_t* hash, const T& value) {
    hash_bytes(hash, &value, sizeof(value));
}

bool valid_string(const char* value) {
    return value != nullptr && std::strchr(value, '"') == nullptr &&
           std::strchr(value, '\\') == nullptr &&
           std::strchr(value, '\n') == nullptr;
}

}  // namespace

bool operator==(const ParitySceneId& left, const ParitySceneId& right) {
    return left.scene_process_id == right.scene_process_id &&
           left.stage_hash == right.stage_hash &&
           left.room == right.room && left.layer == right.layer &&
           left.generation == right.generation;
}

bool operator==(const ParityActorId& left, const ParityActorId& right) {
    return left.stage_hash == right.stage_hash &&
           left.room == right.room && left.layer == right.layer &&
           left.source_table_hash == right.source_table_hash &&
           left.source_index == right.source_index &&
           left.process_id == right.process_id &&
           left.generation == right.generation;
}

bool operator==(const ParityModelId& left, const ParityModelId& right) {
    return left.actor == right.actor &&
           left.archive_hash == right.archive_hash &&
           left.resource_id == right.resource_id &&
           left.model_slot == right.model_slot &&
           left.generation == right.generation;
}

std::uint64_t stable_hash(const ParitySceneId& id) {
    std::uint64_t hash = kFnvOffset;
    hash_value(&hash, id.scene_process_id);
    hash_value(&hash, id.stage_hash);
    hash_value(&hash, id.room);
    hash_value(&hash, id.layer);
    hash_value(&hash, id.generation);
    return hash;
}

std::uint64_t stable_hash(const ParityActorId& id) {
    std::uint64_t hash = kFnvOffset;
    hash_value(&hash, id.stage_hash);
    hash_value(&hash, id.room);
    hash_value(&hash, id.layer);
    hash_value(&hash, id.source_table_hash);
    hash_value(&hash, id.source_index);
    hash_value(&hash, id.process_id);
    hash_value(&hash, id.generation);
    return hash;
}

std::uint64_t stable_hash(const ParityModelId& id) {
    std::uint64_t hash = stable_hash(id.actor);
    hash_value(&hash, id.archive_hash);
    hash_value(&hash, id.resource_id);
    hash_value(&hash, id.model_slot);
    hash_value(&hash, id.generation);
    return hash;
}

bool format_id(
    const ParitySceneId& id, char* output, std::size_t capacity) {
    const int count = std::snprintf(
        output, capacity, "scene:%08lx:%d:%d:%lu:%lu",
        static_cast<unsigned long>(id.stage_hash),
        static_cast<int>(id.room), static_cast<int>(id.layer),
        static_cast<unsigned long>(id.scene_process_id),
        static_cast<unsigned long>(id.generation));
    return count >= 0 && static_cast<std::size_t>(count) < capacity;
}

bool format_id(
    const ParityActorId& id, char* output, std::size_t capacity) {
    const int count = std::snprintf(
        output, capacity, "actor:%08lx:%d:%d:%08lx:%u:%u:%lu",
        static_cast<unsigned long>(id.stage_hash),
        static_cast<int>(id.room), static_cast<int>(id.layer),
        static_cast<unsigned long>(id.source_table_hash),
        static_cast<unsigned int>(id.source_index),
        static_cast<unsigned int>(id.process_id),
        static_cast<unsigned long>(id.generation));
    return count >= 0 && static_cast<std::size_t>(count) < capacity;
}

bool format_id(
    const ParityModelId& id, char* output, std::size_t capacity) {
    char actor[128] = {};
    if (!format_id(id.actor, actor, sizeof(actor))) {
        return false;
    }
    const int count = std::snprintf(
        output, capacity, "model:%s:%08lx:%lu:%u:%u", actor,
        static_cast<unsigned long>(id.archive_hash),
        static_cast<unsigned long>(id.resource_id),
        static_cast<unsigned int>(id.model_slot),
        static_cast<unsigned int>(id.generation));
    return count >= 0 && static_cast<std::size_t>(count) < capacity;
}

const char* event_type_name(EventType type) {
    static constexpr const char* kNames[] = {
        "actor_transform", "actor_origin", "actor_state",
        "actor_collision", "actor_interaction", "actor_culling",
        "actor_shadow", "model_instance", "model_base_matrix",
        "model_local_origin", "model_bounds", "model_draw_matrix",
        "joint_local_matrix", "joint_global_matrix",
        "joint_reference_point",
        "animation_clip", "animation_frame", "animation_root",
        "collision_matrix", "collision_bounds", "movebg_matrix",
        "camera_state", "ui_pane_transform", "resource_lifecycle",
        "render_submission", "scene_checkpoint",
        "behavior_checkpoint",
        "input_change", "locomotion_state_change", "animation_change",
        "turn_start", "turn_end", "stop", "floor_contact",
        "lifecycle_checkpoint",
    };
    const auto index = static_cast<std::size_t>(type);
    if (index >= sizeof(kNames) / sizeof(kNames[0])) {
        return "invalid";
    }
    return kNames[index];
}

const char* lifecycle_checkpoint_name(LifecycleCheckpoint checkpoint) {
    static constexpr const char* kNames[] = {
        "actor_constructed",
        "actor_create_enter",
        "actor_create_exit",
        "actor_first_execute_enter",
        "actor_first_execute_exit",
        "animation_update_enter",
        "animation_update_exit",
        "grounding_enter",
        "grounding_exit",
        "camera_update",
        "draw_prepare",
        "draw_submit",
        "frame_present",
    };
    const auto index = static_cast<std::size_t>(checkpoint);
    if (index >= sizeof(kNames) / sizeof(kNames[0])) {
        return "invalid";
    }
    return kNames[index];
}

bool DtrcV3Writer::initialize(TraceSink sink, void* user) {
    sink_ = sink;
    user_ = user;
    events_written_ = 0;
    dropped_events_ = 0;
    initialized_ = sink != nullptr;
    return initialized_;
}

bool DtrcV3Writer::emit(
    const EventHeader& header, EventType type,
    const char* payload_json) {
    if (!initialized_ || !valid_string(header.build_identity) ||
        !valid_string(header.run_id) ||
        !valid_string(header.scenario_id) ||
        !valid_string(header.stage) || payload_json == nullptr ||
        std::strcmp(event_type_name(type), "invalid") == 0 ||
        std::strcmp(
            lifecycle_checkpoint_name(header.lifecycle_checkpoint),
            "invalid") == 0) {
        ++dropped_events_;
        return false;
    }
    char scene_id[96] = {};
    if (!format_id(header.scene_id, scene_id, sizeof(scene_id))) {
        ++dropped_events_;
        return false;
    }
    char line[kTraceLineCapacity] = {};
    const int count = std::snprintf(
        line, sizeof(line),
        "{\"schema\":\"dusklight.parity.dtrc.v3.1\","
        "\"schema_version\":3,\"schema_revision\":1,"
        "\"build_identity\":\"%s\","
        "\"lifecycle_checkpoint\":\"%s\",\"run_id\":\"%s\","
        "\"scenario_id\":\"%s\",\"frame\":%lu,\"game_tick\":%lu,"
        "\"scene_generation\":%lu,\"scene_id\":\"%s\","
        "\"stage\":\"%s\",\"room\":%d,\"layer\":%d,"
        "\"source_table\":%lu,\"source_index\":%u,"
        "\"source_name_hash\":%lu,\"process_id\":%u,"
        "\"profile_id\":%u,\"actor_generation\":%lu,"
        "\"model_resource_id\":%lu,\"model_instance_id\":%u,"
        "\"event_type\":\"%s\",\"payload\":%s}\n",
        header.build_identity,
        lifecycle_checkpoint_name(header.lifecycle_checkpoint),
        header.run_id, header.scenario_id,
        static_cast<unsigned long>(header.frame),
        static_cast<unsigned long>(header.game_tick),
        static_cast<unsigned long>(header.scene_id.generation), scene_id,
        header.stage, static_cast<int>(header.room),
        static_cast<int>(header.layer),
        static_cast<unsigned long>(header.source_table),
        static_cast<unsigned int>(header.source_index),
        static_cast<unsigned long>(header.source_name_hash),
        static_cast<unsigned int>(header.process_id),
        static_cast<unsigned int>(header.profile_id),
        static_cast<unsigned long>(header.actor_generation),
        static_cast<unsigned long>(header.model_resource_id),
        static_cast<unsigned int>(header.model_instance_id),
        event_type_name(type), payload_json);
    if (count <= 0 || static_cast<std::size_t>(count) >= sizeof(line) ||
        !sink_(user_, line, static_cast<std::size_t>(count))) {
        ++dropped_events_;
        return false;
    }
    ++events_written_;
    return true;
}

bool DtrcV3Writer::healthy() const {
    return initialized_ && dropped_events_ == 0;
}

std::uint32_t DtrcV3Writer::events_written() const {
    return events_written_;
}

std::uint32_t DtrcV3Writer::dropped_events() const {
    return dropped_events_;
}

}  // namespace dusk::psp::parity
