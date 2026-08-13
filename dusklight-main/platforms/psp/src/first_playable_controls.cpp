#include "dusk/psp/first_playable_controls.hpp"

#include <cmath>

namespace dusk::psp::game {
namespace {

constexpr float kMovementProofDistance = 1.0f;
constexpr float kAnalogInputThreshold = 0.05f;

float horizontal_distance_squared(
    const room::Vec3& a, const room::Vec3& b) {
    const float dx = a.x - b.x;
    const float dz = a.z - b.z;
    return dx * dx + dz * dz;
}

}  // namespace

void arm_first_playable_control_proof(
    FirstPlayableControlProof* proof,
    const room::RealRoomState& state) {
    if (proof == nullptr) {
        return;
    }
    *proof = {};
    proof->origin = state.position;
    proof->armed = true;
}

void observe_first_playable_controls(
    FirstPlayableControlProof* proof,
    const playable::Input& input,
    const room::RealRoomState& before,
    const room::RealRoomState& after) {
    if (proof == nullptr || !proof->armed) {
        return;
    }

    const float analog_magnitude_squared =
        input.analog_x * input.analog_x + input.analog_y * input.analog_y;
    if (analog_magnitude_squared >=
        kAnalogInputThreshold * kAnalogInputThreshold) {
        proof->movement_input_seen = true;
    }
    if (proof->movement_input_seen &&
        horizontal_distance_squared(after.position, proof->origin) >=
            kMovementProofDistance * kMovementProofDistance) {
        proof->movement_proven = true;
    }

    if ((input.camera_left || input.camera_right) &&
        after.camera_manual_frames > 0) {
        proof->camera_proven = true;
    }

    if (input.action_pressed && after.actions > before.actions) {
        proof->action_proven = true;
    }

    if (input.pause_pressed &&
        before.mode == room::LoadState::Playing &&
        after.mode == room::LoadState::Paused &&
        after.pause_entries > before.pause_entries) {
        proof->pause_proven = true;
    }

    if (proof->pause_proven &&
        before.mode == room::LoadState::Paused &&
        after.mode == room::LoadState::Playing &&
        (input.action_pressed || input.cancel_pressed)) {
        proof->resume_proven = true;
    }
}

bool first_playable_controls_complete(
    const FirstPlayableControlProof& proof) {
    return proof.armed &&
           proof.movement_proven &&
           proof.camera_proven &&
           proof.action_proven &&
           proof.pause_proven &&
           proof.resume_proven;
}

}  // namespace dusk::psp::game
