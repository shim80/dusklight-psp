#ifndef DUSK_PSP_ORIGINAL_DYNAMIC_ACTOR_BRIDGE_HPP
#define DUSK_PSP_ORIGINAL_DYNAMIC_ACTOR_BRIDGE_HPP

#include "dusk/psp/process_runtime.hpp"
#include "dusk/psp/room_package.hpp"

#include <cstdint>

namespace dusk::psp::compat {

struct OriginalDynamicActorDescriptor {
    const char* selection_classification;
    const char* source_table_type;
    std::uint16_t source_record_index;
    const char* source_name;
    std::uint32_t source_name_hash;
    std::uint16_t source_process_id;
    const char* source_process_symbol;
    const char* source_profile_symbol;
    const char* source_class_name;
    const char* source_header;
    const char* source_implementation;
    const char* source_archive;
    std::uint32_t source_params;
    float source_position[3];
    std::int16_t source_rotation[3];
    float source_scale[3];
    std::uint8_t source_room;
    std::uint8_t source_layer;
};

struct OriginalDynamicActorMetrics {
    std::uint32_t context_activations;
    std::uint32_t context_deactivations;
    std::uint32_t companion_records;
    std::uint32_t input_updates;
    std::uint32_t rotation_samples;
    std::uint32_t rotation_updates;
    std::uint32_t state_transitions;
    std::uint32_t pause_samples;
    std::uint32_t pause_violations;
    std::int16_t initial_rotation;
    std::int16_t rotation;
    std::int16_t speed;
    std::int16_t target;
    std::uint16_t state_count;
    bool record_mapping_valid;
    bool params_preserved;
    bool matrix_valid;
    bool companion_active;
    bool moving;
};

bool register_original_dynamic_actor_profile(
    process::PspProcessManager* manager);
bool activate_original_dynamic_actor_context(
    process::PspProcessManager* manager,
    const room::PackageView& scene,
    std::int8_t room_number);
void deactivate_original_dynamic_actor_context(
    std::int8_t room_number);
void update_original_dynamic_actor_context(
    std::uint32_t fixed_update, bool paused);
bool sample_original_dynamic_actor(
    const process::PspProcessManager& manager, bool paused);
bool original_dynamic_actor_profile_valid();
const process::Metrics* original_dynamic_actor_process_metrics(
    const process::PspProcessManager& manager);
const OriginalDynamicActorDescriptor&
original_dynamic_actor_descriptor();
const OriginalDynamicActorMetrics&
original_dynamic_actor_metrics();

}  // namespace dusk::psp::compat

#endif
