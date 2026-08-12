# Link model-origin classification

## Result

`MODEL_ORIGIN_CLOSED_BEFORE_GROUNDING_BOUNDARY`

For every mobile Link scenario, model-base translation was compared with actor
`current_position` on both desktop and PSP for all ticks strictly before the
scenario's first causal boundary.

The maximum Euclidean difference is exactly `0.0` for both platforms in all
eight scenarios. There is no constant offset, axis swap, scale conversion, or
startup-origin defect to correct.

Later in several desktop traces, model-base Y can differ from actor-origin Y.
Those samples occur after the first grounding/leg-chain divergence and reflect
source vertical pose/terrain correction. The PSP model base and actor origin
remain coupled. This later behavior must be revisited only after the earlier
grounding field is closed; changing model origin now would hide causality.

No model, actor, capsule, collision, or grounding code was modified. No new
PPSSPP run was performed.
