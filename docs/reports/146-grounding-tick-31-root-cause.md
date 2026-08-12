# Tick 31 Link turn divergence — root-cause classification

## Result

The first reported `link_turn_180` mismatch at tick 31 is not a grounding
failure. It is a root-pose difference caused by the PSP old-frame animation
morph implementation.

Classification: `ROOT_POSE_DIFFERENCE / OLD_FRAME_MORPH_SEMANTICS`.

## Evidence

- Desktop and PSP actor transforms, floor identity, collision origin, floor
  height, and model base matrix agree at tick 31.
- The local root and pelvis already differ before the left-hip reference point
  crosses the comparison tolerance.
- `daAlink_c::procWaitTurnInit()` selects `ANM_STEP_TURN` with
  `mBasicInterpolation`.
- The preserved source snapshot defines `mBasicInterpolation` as `4.0f` in
  `d_a_alink_HIO_data.inc`.
- `mDoExt_MtxCalcOldFrame::initOldFrameMorf()` and
  `decOldFrameMorfCounter()` produce new-pose contributions of `1/4`, `1/3`,
  `1/2`, then `1` for that four-frame morph.
- `mDoExt_MtxCalcAnmBlendTblOld::calc()` stores every blended result back into
  the old-frame arrays. The next frame therefore blends recursively from the
  preceding output pose.
- The PSP runtime instead used six frames, retained one fixed transition
  snapshot, and contributed zero percent of the new pose on entry.

The causal comparison labelled the first out-of-tolerance hip value as
`GROUNDING` because `joint_reference_point` is mapped to that layer. The raw
event ordering proves that the responsible subsystem is earlier.

## Minimal correction boundary

Only the playable animation transition semantics are changed:

1. use the source HIO duration of four frames;
2. blend recursively from the previously produced pose;
3. consume one old-frame morph step per fixed runtime update.

No desktop reference, grounding solver, floor value, actor transform,
scenario-specific value, comparison tolerance, or tick-specific branch is
modified.

## Targeted validation

`link_turn_pose_checkpoint_host_test` exercises ticks 24 through 40 and emits
`checkpoint,value,error,classification` rows. It verifies the source-derived
old-frame counter sequence at the turn entry without embedding any desktop
trace value as a runtime oracle.

## Causal validation

- correction commit: `ee2e730ca03e9ff919b3e055e6a62e157329d932`;
- parity build: `sha256:29354ce2de6d6dbfb7f7049e09cba42c6a725ab1c728ea9849c023c18f9d3ad4`;
- broker request: `20260731T161051Z-parity_trace-1`, generation 2;
- boot, marker, and all 13 collected metrics files valid;
- divergence count: 7,280 before, 7,225 after;
- the tick 31 joint-reference mismatch is absent;
- new earliest frontier: `ACTOR_TRANSFORM`, tick 33,
  `actor_state.camera_yaw`.

This is causal progress. The ten-scenario Link reacquisition remains required
before declaring the animation layer closed again.
