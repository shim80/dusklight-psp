# Link state-initialization classification

## Result

`STATE_INITIALIZATION_CLOSED`

The eight cached mobile Link causal summaries for build
`sha256:8264de147b259b5f918960afc8a817252800c073f750e2a8dd33c72716ce75d3`
were inspected without running PPSSPP again.

All eight first numerical causal boundaries are joint reference fields mapped
to `GROUNDING`, at ticks 30, 31, or 36. No initialized procedure, speed,
velocity, current/target yaw, camera yaw, clip identity, or actor origin field
crosses its numerical tolerance before those boundaries.

The first later `procedure_state` differences occur at ticks 35–42, depending
on scenario. The first later actor-world-transform differences occur at ticks
32–38. They cannot be classified as initialization defects because they are
downstream of the earlier leg-chain boundary.

The comparator also reports a PSP-only `input_change` event at tick 0. This is
an additive emission-policy difference: the PSP trace records its initialized
neutral input while the desktop trace emits only a change. It carries no
numerical state mismatch and is not a gameplay divergence.

## Per-scenario first boundary

| Scenario | Tick | First subsystem |
|---|---:|---|
| `link_walk` | 31 | feet / grounding |
| `link_run` | 31 | feet / grounding |
| `link_turn_90` | 30 | feet / grounding |
| `link_turn_180` | 36 | feet / grounding |
| `link_stop` | 31 | feet / grounding |
| `link_slope` | 31 | feet / grounding |
| `link_camera_follow` | 31 | feet / grounding |
| `link_collision_wall` | 31 | feet / grounding |

No code correction is justified for P1.1. The next independent classification
is whether actor-world-transform drift is its own defect or a consequence of
the earlier grounding/pose boundary.
