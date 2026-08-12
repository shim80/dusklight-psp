#ifndef DUSK_PSP_ORIGINAL_TBOX_BRIDGE_HPP
#define DUSK_PSP_ORIGINAL_TBOX_BRIDGE_HPP

#include "dusk/psp/event_runtime.hpp"
#include "dusk/psp/interaction_runtime.hpp"
#include "dusk/psp/item_runtime.hpp"
#include "dusk/psp/process_runtime.hpp"
#include "dusk/psp/room_package.hpp"

#include <cstdint>

namespace dusk::psp::compat {

struct OriginalTboxMetrics {
    std::uint32_t placements_seen;
    std::uint32_t placements_created;
    std::uint32_t interaction_requests;
    std::uint32_t interactions_accepted;
    std::uint32_t items_created;
    std::uint32_t items_shown;
    std::uint32_t items_killed;
    std::uint32_t items_committed;
    std::uint32_t events_completed;
    std::uint32_t collision_swaps;
    std::uint32_t unsupported_boss_placements;
    bool source_profile_valid;
    bool parameters_preserved;
};

bool register_original_tbox_profile(
    process::PspProcessManager* manager);
bool original_tbox_profile_valid();
bool bind_original_tbox_context(
    process::PspProcessManager* manager,
    events::PspEventContext* events,
    interaction::PspInteractionContext* interactions,
    items::PspItemContext* items);
void unbind_original_tbox_context();
bool create_original_tboxes(
    process::PspProcessManager* manager,
    const room::PackageView& scene,
    std::int8_t room_number,
    process::ProcessHandle* handles,
    std::uint16_t handle_capacity,
    std::uint16_t* created);
void deactivate_original_tboxes(std::int8_t room_number);
void set_original_tbox_player_position(const float position[3]);
bool set_original_tbox_validation_player_position();
bool sample_original_tbox_interaction(
    process::PspProcessManager* manager,
    bool press_open);
const OriginalTboxMetrics& original_tbox_metrics();

// Source Demo_Item presentation lifecycle used while the full original
// Demo_Item process is progressively connected. When a SourceEventScript is
// bound, chest creation only creates a hidden partner. Acquisition happens
// exclusively after show -> message -> kill/commit.
bool original_tbox_demo_item_pending(
    std::uint32_t process_id, std::uint8_t* item_no);
bool original_tbox_demo_item_visible(std::uint32_t process_id);
bool original_tbox_demo_item_show(std::uint32_t process_id);
bool original_tbox_demo_item_kill_and_commit(std::uint32_t process_id);

}  // namespace dusk::psp::compat

#endif
