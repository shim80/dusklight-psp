# Link old-frame morph causal closure

## Classification

`ANIMATION_RUNTIME_CLOSED`

The tick 31 `link_turn_180` joint-reference mismatch was an upstream pose
transition error, not a grounding error. Commit
`ee2e730ca03e9ff919b3e055e6a62e157329d932` reproduces the preserved source
snapshot's four-frame recursive old-frame morph.

## Validation boundary

- host runtime and ticks 24–40 checkpoint harness: pass;
- host grounding and root-anchor regressions: pass;
- Allegrex build and architecture check: pass;
- parity build:
  `sha256:29354ce2de6d6dbfb7f7049e09cba42c6a725ab1c728ea9849c023c18f9d3ad4`;
- revealing PSP trace: valid, 7,280 → 7,225 divergences;
- ten Link PSP traces reacquired through broker generation 2;
- ten traces have the same parity build identity;
- ten causal summaries generated against unchanged desktop traces;
- zero of ten first causal divergences are in `ANIMATION_RUNTIME`.

The earliest remaining Link frontier is `link_turn_180`, tick 33,
`ACTOR_TRANSFORM`, on `actor_state.camera_yaw`. The other nine scenario
frontiers are in `STATE_INITIALIZATION`.

## Preserved invariants

- no desktop reference modification;
- no network access;
- no tick-specific or oracle-value runtime branch;
- no tolerance change;
- no fake trace or fabricated Dusklight type;
- repository-isolated PPSSPP profile and canonical GUI broker only.
