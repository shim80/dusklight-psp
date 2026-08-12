# Desktop pose checkpoint instrumentation

## Result

The trace-only Dusklight desktop worktree now exposes neutral lifecycle
checkpoints around Link animation update, both grounding phases, and model pose
construction. The vanilla reference checkout remains clean and unchanged.

The instrumentation is reproducible as
`reference/desktop/patches/0008-dusklight-reference-pose-checkpoints.patch`.
It is applied after the already validated DTRC v2, v3 and Link behavior patches.
No game state, branch, transform, animation frame, floor result, or update order is
changed by the patch.

## Observed source order

For every complete sampled tick in the revealing `link_turn_180` run, the event
order is:

1. `animation_update_enter` / `animation_update_exit`;
2. `grounding_enter` / `grounding_exit` for actor collision;
3. `grounding_enter` / `grounding_exit` for `footBgCheck`;
4. `pose_build_enter` / `pose_build_exit` around `modelCalc`.

This disproves the working assumption that the final pose for a tick is built
before that same tick's grounding work. A causal comparison must align the pose
consumed by grounding with the previously completed pose and must not modify the
grounding solver until that alignment is proven.

The trace includes actor state and transform, animation clip/frame/rate, prior
morph buffer state, model base matrix, global matrices for root/pelvis/hips/
knees/ankles/feet, and left/right sole reference points. Fields named
`old_frame_local_*` are explicitly the prior morph-buffer transforms; they are
not presented as final evaluated local transforms.

## Validation

- trace-only translation unit: compiled successfully;
- desktop application: linked and ad-hoc signed successfully;
- DTRC host/schema test: `PARITY_TRACE_V3_1_TEST_OK`;
- revealing trace: 157 sampled ticks and 4,567 valid events;
- lifecycle order: verified programmatically by the mobile-only runner;
- vanilla desktop checkout: clean;
- network: not used.

The raw trace and extracted tick window remain under ignored build paths and are
not versioned. The first revealing trace is complete. Seven additional mobile
desktop acquisitions are `SUSPENDED_INFRASTRUCTURE`: the GUI authorization layer
timed out twice before process creation, so no application failure was observed.

## Causal consequence

`DPAN_V1_HERMITE_DATA_LOSS` is still a hypothesis, not a selected fix. The next
independent step is exact local BCK/ANK1 extraction. Grounding remains frozen.
