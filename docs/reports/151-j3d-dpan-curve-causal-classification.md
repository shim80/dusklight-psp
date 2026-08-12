# J3D/DPAN curve causal classification

## Classification

`DPAN_V1_HERMITE_DATA_LOSS_PRESENT_NOT_PROVEN_CAUSAL`

The exact ANK1 evaluator and the current DPAN v1 runtime were compared at the
fractional frames observed in `link_turn_180`, ticks 30 through 40. Integer
frames were also checked as a converter/runtime consistency guard.

At the initially suspicious tick 35 (`stepl.bck`, frame 3.5), the maximum local
differences over root, pelvis and both leg chains are:

- translation: `0.0134248211`;
- quaternion rotation angle: `0.0064356116` radians;
- scale: `0`.

Thus DPAN v1 demonstrably loses continuous Hermite evaluation. Existence alone
does not establish causality.

## Counterfactual

The tick-35 transition was reconstructed in both representations using the
validated source morph semantics: completed tick-34 `stepl.bck` frame 2.8,
transition to `waits.bck` frame 0, first morph amount 1/3. Both local poses were
propagated through the real DPSK hierarchy and compared with the measured
desktop-minus-PSP global joint vectors.

Results over root, pelvis, hips, knees, ankles and feet:

- predicted Hermite-loss vector norm: `0.0498782`;
- observed vector norm: `0.1953714`;
- predicted/observed norm ratio: `0.2552994`;
- direction cosine: `0.1314513`;
- residual RMSE: `0.0356351`.

The predicted error is too small and points mostly in a different direction.
It cannot honestly be selected as the explanation of the causal boundary.

## Pose/grounding boundary

The new desktop instrumentation proves the within-tick order is animation,
actor collision grounding, foot grounding, then pose build. Therefore the
grounding step consumes the previously completed pose.

For the current build `sha256:8264de…`, the cached causal report places the first
tolerance-breaking divergence at tick 36, `joint_reference_point.position[2]`,
layer `GROUNDING`. The complete tick-35 leg-chain differences remain below the
configured `0.25` position tolerance (maximum Euclidean distance `0.125011`).
The tick-36 maximum becomes `1.25318`.

This supports proceeding to grounding-internal comparison, but does not yet
authorize a solver modification. The first differing grounding field must be
identified in the mandated order.

## Consequences

- DPAN v2: not implemented and locally blocked pending contrary causal proof;
- grounding offsets/tolerances/capsule: unchanged;
- desktop oracle behavior: unchanged;
- network: not used;
- raw BCK values and traces: remain ignored and unversioned.
