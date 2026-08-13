#include "dusk/psp/first_playable_controls.hpp"

#include <cstdio>

namespace {

bool require(bool condition, const char* message) {
    if (!condition) {
        std::fprintf(stderr, "FIRST_PLAYABLE_CONTROLS_HOST_FAIL %s\n", message);
        return false;
    }
    return true;
}

}  // namespace

int main() {
    using dusk::psp::game::FirstPlayableControlProof;
    using dusk::psp::game::arm_first_playable_control_proof;
    using dusk::psp::game::first_playable_controls_complete;
    using dusk::psp::game::observe_first_playable_controls;
    using dusk::psp::playable::Input;
    using dusk::psp::room::LoadState;
    using dusk::psp::room::RealRoomState;

    FirstPlayableControlProof proof = {};
    RealRoomState before = {};
    RealRoomState after = {};
    before.mode = LoadState::Playing;
    after = before;
    before.position = {10.0f, 20.0f, 30.0f};
    after.position = before.position;

    Input input = {};
    input.action_pressed = true;
    observe_first_playable_controls(&proof, input, before, after);
    if (!require(!first_playable_controls_complete(proof),
                 "unarmed tracker accepted input")) {
        return 1;
    }

    arm_first_playable_control_proof(&proof, before);

    input = {};
    input.analog_y = -1.0f;
    observe_first_playable_controls(&proof, input, before, after);
    if (!require(!proof.movement_proven,
                 "analog input alone counted as movement")) {
        return 2;
    }
    after.position.x = 11.1f;
    input = {};
    observe_first_playable_controls(&proof, input, before, after);
    if (!require(proof.movement_proven,
                 "runtime displacement was not accepted")) {
        return 3;
    }

    before = after;
    input = {};
    input.camera_left = true;
    after = before;
    observe_first_playable_controls(&proof, input, before, after);
    if (!require(!proof.camera_proven,
                 "camera button alone counted as camera response")) {
        return 4;
    }
    after.camera_manual_frames = 59;
    observe_first_playable_controls(&proof, input, before, after);
    if (!require(proof.camera_proven,
                 "runtime manual-camera state was not accepted")) {
        return 5;
    }

    before = after;
    input = {};
    input.action_pressed = true;
    after = before;
    observe_first_playable_controls(&proof, input, before, after);
    if (!require(!proof.action_proven,
                 "Cross without source action counted as action")) {
        return 6;
    }
    ++after.actions;
    observe_first_playable_controls(&proof, input, before, after);
    if (!require(proof.action_proven,
                 "source action counter increment was not accepted")) {
        return 7;
    }

    before = after;
    before.mode = LoadState::Playing;
    after = before;
    input = {};
    input.pause_pressed = true;
    observe_first_playable_controls(&proof, input, before, after);
    if (!require(!proof.pause_proven,
                 "START without pause transition counted as pause")) {
        return 8;
    }
    after.mode = LoadState::Paused;
    ++after.pause_entries;
    observe_first_playable_controls(&proof, input, before, after);
    if (!require(proof.pause_proven,
                 "Playing-to-Paused transition was not accepted")) {
        return 9;
    }

    before = after;
    after = before;
    input = {};
    input.cancel_pressed = true;
    observe_first_playable_controls(&proof, input, before, after);
    if (!require(!proof.resume_proven,
                 "cancel without resume transition counted as resume")) {
        return 10;
    }
    after.mode = LoadState::Playing;
    observe_first_playable_controls(&proof, input, before, after);
    if (!require(proof.resume_proven,
                 "Paused-to-Playing transition was not accepted")) {
        return 11;
    }

    if (!require(first_playable_controls_complete(proof),
                 "complete runtime evidence did not close tracker")) {
        return 12;
    }

    std::printf(
        "FIRST_PLAYABLE_CONTROLS_HOST_OK "
        "movement=displacement camera=manual_runtime "
        "action=source_prompt pause=enter_resume fail_closed=1\n");
    return 0;
}
