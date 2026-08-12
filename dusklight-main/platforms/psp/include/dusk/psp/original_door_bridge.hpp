#ifndef DUSK_PSP_ORIGINAL_DOOR_BRIDGE_HPP
#define DUSK_PSP_ORIGINAL_DOOR_BRIDGE_HPP

#include "dusk/psp/process_runtime.hpp"
#include "dusk/psp/room_package.hpp"

#include <cstdint>

namespace dusk::psp::model {
class PspStaticModelRuntime;
}

namespace dusk::psp::movebg {
class PspMoveBgWorld;
}

namespace dusk::psp::switches {
class PspSwitchSurface;
}

namespace dusk::psp::compat {

struct OriginalDoorDescriptor {
    const char* table;
    std::uint16_t source_record_index;
    const char* source_name;
    std::uint32_t source_name_hash;
    std::uint16_t process_id;
    const char* process_symbol;
    const char* profile_symbol;
    const char* class_name;
    const char* header;
    const char* source;
    const char* archive;
    std::uint32_t parameters;
    float position[3];
    std::int16_t rotation[3];
    std::uint8_t room;
    std::uint8_t switch_number;
    std::uint8_t model_resource;
    std::uint8_t collision_resource;
};

struct OriginalDoorMetrics {
    std::uint32_t context_activations;
    std::uint32_t context_deactivations;
    std::uint32_t source_samples;
    std::uint32_t switch_on_requests;
    std::uint32_t switch_off_requests;
    std::uint32_t doors_closed;
    std::uint32_t doors_opened;
    std::uint32_t completed_cycles;
    std::uint32_t matrix_parity_samples;
    std::uint32_t matrix_mismatches;
    bool record_mapping_valid;
    bool parameters_preserved;
    bool active;
    bool visit_complete;
};

bool register_original_door_profile(
    process::PspProcessManager* manager);
bool original_door_profile_valid();
const OriginalDoorDescriptor& original_door_descriptor();
bool activate_original_door_validation(
    process::PspProcessManager* manager,
    switches::PspSwitchSurface* switch_surface,
    const room::PackageView& scene,
    std::int8_t room_number);
bool sample_original_door_validation(
    process::PspProcessManager* manager,
    model::PspStaticModelRuntime* models,
    movebg::PspMoveBgWorld* world,
    std::int8_t room_number,
    bool paused);
void deactivate_original_door_validation(std::int8_t room_number);
bool original_door_transition_ready(std::int8_t room_number);
const OriginalDoorMetrics& original_door_metrics();

}  // namespace dusk::psp::compat

#endif
