#ifndef DUSK_PSP_ORIGINAL_RENDERED_ACTOR_BRIDGE_HPP
#define DUSK_PSP_ORIGINAL_RENDERED_ACTOR_BRIDGE_HPP

#include "dusk/psp/process_runtime.hpp"
#include "dusk/psp/room_package.hpp"

#include <cstdint>

namespace dusk::psp::compat {

struct OriginalRenderedActorDescriptor {
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
    std::uint8_t candidate_count;
    std::uint8_t selected_score;
};

bool register_original_rendered_actor_profiles(
    process::PspProcessManager* manager);
bool create_registered_room_actors(
    process::PspProcessManager* manager,
    const room::PackageView& scene,
    std::int8_t room_number,
    std::uint16_t* created_count);
bool original_rendered_actor_profile_valid();
const process::Metrics* original_rendered_actor_process_metrics(
    const process::PspProcessManager& manager);
const OriginalRenderedActorDescriptor&
original_rendered_actor_descriptor();

}  // namespace dusk::psp::compat

#endif
