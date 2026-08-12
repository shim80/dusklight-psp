# Pre-ground pose and DPAN Hermite blocker

## Result

The original `link_turn_180` tick-35 divergence was **not** proven to be a
grounding-solver defect. It was preceded by two source-state mismatches:

1. wait-turn exit acceleration;
2. the animated-foot displacement contribution to movement speed.

After both generic corrections, the first reported joint mismatch moved to
tick 36 and total divergences fell from 3432 to 1050 for this scenario.
Grounding production code was not modified.

The remaining tick-36 difference is classified
`UNRESOLVED_PRE_GROUND_POSE_OR_GROUNDING`, with the local blocker
`DPAN_V1_HERMITE_DATA_LOSS`. This is deliberately narrower than claiming
either an animation-conversion defect or a true terrain-solver defect.

## Evidence from ticks 30–40

For `link_turn_180`, source clip identity, animation frame, playback speed,
actor/root position, pelvis, and upper-leg pose agree through the transition
within the current trace tolerances after the state corrections. At the first
fractional frame after double-controller entry, the residual grows down the
leg chain through knees, ankles, feet, and sole reference points.

That shape is compatible with lost curve information before grounding:

- the desktop path uses `J3DAnmTransformKey::calcTransform()`;
- BCK transform keys include time, value, tangent, and interpolation data;
- DPAN v1 retains sampled integer/quaternion poses, not the original Hermite
  key tables and tangents;
- the mismatch appears on fractional animation frames, where tangent data can
  affect the evaluated pose.

It is not sufficient proof that DPAN is causal. The same immutable desktop
DTRC v3 trace contains neither `pose_build_exit` nor `grounding_enter` events.
Its joint reference points are emitted at `frame_present` and mapped to the
`GROUNDING` layer, after both pose evaluation and grounding can have acted.

## Missing evidence

The following evidence is not present locally:

- desktop `pose_build_exit` samples for actor transform, root, pelvis, hips,
  knees, ankles, feet, and sole references at ticks 30–40;
- desktop `grounding_enter` state for the same ticks;
- source BCK key tables, values, tangents, and interpolation modes for the
  affected moving clips;
- a configured legal-local `DUSKLIGHT_GAME_IMAGE` from which those source
  tables could be converted.

The trace schema mentions `animation_update_enter`, `animation_update_exit`,
`grounding_enter`, and `grounding_exit`, but the accepted desktop reference
does not contain them. The desktop oracle was not modified or reacquired.

## Corrections validated in this pass

| Commit | Generic source semantic | Revealing result |
|---|---|---|
| `8d48771` | wait-turn exit acceleration order | frontier leaves tick 35 |
| `e315ee0` | animated-foot speed contribution | `3431 -> 1468`, tick 36 |
| `b18ce18` | stick plus controlled-camera angle | turn-90 frontier advances |
| `c3061c7` | manifest-bounded causal comparison | idle and ground-contact close |
| `4bbf648` | JUT stick `atan2f` angle | slope `20096 -> 19833` |
| `7849fc4` | integer `cLib_addCalcAngleS` yaw chase | slope `19833 -> 19823` |

Every runtime correction passed its targeted host tests and the Allegrex
build before a fresh broker trace. No tick 35 branch, scenario branch, desktop
sample, constant Y offset, expanded tolerance, model displacement, capsule
displacement, or grounding disable was introduced.

## Ten-scenario Link state

Build:
`sha256:8264de147b259b5f918960afc8a817252800c073f750e2a8dd33c72716ce75d3`

| Scenario | Result | First reported event |
|---|---:|---|
| `link_idle_full_cycle` | match within tolerance | none |
| `link_ground_contact` | match within tolerance | none |
| `link_walk` | 5735 differences | joint Z, tick 31 |
| `link_run` | 9051 differences | joint Z, tick 31 |
| `link_turn_90` | 12601 differences | joint Z, tick 30 |
| `link_turn_180` | 1050 differences | joint Z, tick 36 |
| `link_stop` | 7528 differences | joint Z, tick 31 |
| `link_slope` | 19823 differences | joint Z, tick 31 |
| `link_camera_follow` | 13964 differences | joint X, tick 31 |
| `link_collision_wall` | 19130 differences | joint Z, tick 31 |

All ten traces booted through broker generation 2 and returned valid markers
and metrics. The causal tool filters desktop events at or beyond the scenario's
exclusive `maximum_ticks` boundary.

## Safe resume condition

Do not modify floor query, capsule, grounding offset, penetration correction,
or update order on the current evidence. Resume at the same frontier only when
one of these inputs already exists locally and can be validated without
altering the oracle:

1. complete desktop pre-ground checkpoints for ticks 30–40; or
2. legal-local source BCK key/tangent data sufficient to build and test a
   generic backward-compatible DPAN v2.

No network access is required or authorized by this blocker. Without one of
those inputs, implementing DPAN v2 would invent missing source data, while
changing grounding would violate the required pre-ground comparison order.
