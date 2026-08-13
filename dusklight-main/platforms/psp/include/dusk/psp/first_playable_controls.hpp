#ifndef DUSK_PSP_FIRST_PLAYABLE_CONTROLS_HPP
#define DUSK_PSP_FIRST_PLAYABLE_CONTROLS_HPP

#include "dusk/psp/playable_runtime.hpp"
#include "dusk/psp/real_room_runtime.hpp"

namespace dusk::psp::game {

struct FirstPlayableControlProof {
    room::Vec3 origin;
    bool armed;
    bool movement_input_seen;
    bool movement_proven;
    bool camera_proven;
    bool action_proven;
    bool pause_proven;
    bool resume_proven;
};

void arm_first_playable_control_proof(
    FirstPlayableControlProof* proof,
    const room::RealRoomState& state);

void observe_first_playable_controls(
    FirstPlayableControlProof* proof,
    const playable::Input& input,
    const room::RealRoomState& before,
    const room::RealRoomState& after);

bool first_playable_controls_complete(
    const FirstPlayableControlProof& proof);

}  // namespace dusk::psp::game

#endif
