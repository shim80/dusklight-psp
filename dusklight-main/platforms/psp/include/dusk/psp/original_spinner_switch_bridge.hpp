#ifndef DUSK_PSP_ORIGINAL_SPINNER_SWITCH_BRIDGE_HPP
#define DUSK_PSP_ORIGINAL_SPINNER_SWITCH_BRIDGE_HPP

#include "dusk/psp/interaction_runtime.hpp"
#include "dusk/psp/process_runtime.hpp"
#include "dusk/psp/room_package.hpp"
#include "dusk/psp/switch_runtime.hpp"

#include <cstdint>

namespace dusk::psp::compat {

struct OriginalSpinnerSwitchMetrics {
    std::uint32_t context_activations;
    std::uint32_t context_deactivations;
    std::uint32_t input_frames;
    std::uint32_t prompt_frames;
    std::uint32_t accepted_frames;
    std::uint32_t original_execute_samples;
    std::uint32_t switch_activations;
    std::uint32_t mechanism_changes;
    std::uint32_t post_switch_frames;
    std::uint32_t spinner_in_frames;
    std::uint32_t rotation_frames;
    std::uint16_t source_record_index;
    std::uint32_t source_parameters;
    std::int16_t published_angle;
    bool record_mapping_valid;
    bool profile_valid;
    bool facade_active;
    bool source_switch_active;
};

bool activate_original_spinner_switch(
    process::PspProcessManager* manager,
    interaction::PspInteractionContext* interactions,
    switches::PspSwitchSurface* switches,
    const room::PackageView& scene,
    std::int8_t room_number);
void deactivate_original_spinner_switch(std::int8_t room_number);
bool update_original_spinner_switch(
    bool paused, bool action_pressed);
bool sample_original_spinner_switch();
bool original_spinner_switch_profile_valid();
const OriginalSpinnerSwitchMetrics&
original_spinner_switch_metrics();

}  // namespace dusk::psp::compat

#endif
