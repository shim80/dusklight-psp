# Link actor world-transform classification

## Result

`ACTOR_WORLD_TRANSFORM_DOWNSTREAM`

The cached causal chains show the same ordering in all eight mobile scenarios:
the first leg-chain/grounding divergence occurs before any actor transform
divergence. The actor transform and model base matrix then diverge together.

| Scenario | Grounding tick | Actor tick | Model-base tick |
|---|---:|---:|---:|
| `link_walk` | 31 | 32 | 32 |
| `link_run` | 31 | 32 | 32 |
| `link_turn_90` | 30 | 33 | 33 |
| `link_turn_180` | 36 | 38 | 38 |
| `link_stop` | 31 | 32 | 32 |
| `link_slope` | 31 | 32 | 32 |
| `link_camera_follow` | 31 | 32 | 32 |
| `link_collision_wall` | 31 | 32 | 32 |

The first actor field is normally `current_position[2]`; the 90-degree turn
first exposes `current_position[0]`. This axis change follows the scenario
heading and does not indicate an initialization or constant-origin defect.

Because model base translation changes on exactly the actor tick, it is derived
from the actor world transform rather than an independent model-origin source.
No actor transform, model base, collision origin, speed, or yaw correction is
justified before the earlier grounding boundary is resolved.

No code was changed and no PPSSPP run was performed for this classification.
