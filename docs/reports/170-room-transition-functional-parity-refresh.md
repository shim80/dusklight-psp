# Room and transition Functional parity refresh

## Result

P4.3 classification: `FUNCTIONAL_PARITY_WITH_DOCUMENTED_DIFFERENCES`.

The current build completed the canonical bidirectional D_MN10 transition
smoke through the isolated broker:

```text
R09 -> black fade -> R02 -> black fade -> R09
```

The source exit records, destination rooms, spawn records and floor mappings
remain authoritative. The PSP black fade and translucent debug trigger marker
are documented platform representations, not pixel-identical source effects.

## Current-build evidence

- two transitions: one R09→R02 and one R02→R09;
- three room loads, two unloads, three activations and two deactivations;
- actor lifecycle: three create waves, two destroy waves, zero duplicates;
- room generation advanced to 3 with zero stale room, actor or texture handles;
- R09: 18,000 vertices, 11,361 triangles, 23 draws, 5,878 collision
  triangles, seven instantiated actors;
- R02: 23,953 vertices, 13,152 triangles, 19 draws, 6,241 collision
  triangles, nine instantiated actors;
- no simultaneous double-room EDRAM residency;
- zero allocations during Playing, zero leaked bytes, zero non-finite values;
- transition, resource, collision, camera and actor lifecycle validations pass;
- PPSSPP classification `MARKERS_VALID_METRICS_VALID`, boot observed and no
  PSP runtime error.

R09/R02 model/collision origins were already closed in P2.2. Current native
actor traces now cover five real classes across both rooms. F_SP102 and F_SP108
are covered by the current Functional startup pass; F_SP110 geyser behavior
remains the P3.5 procedural `PARTIAL_PARITY` / `EXPECTED_PLATFORM_DIFFERENCE`
classification.

## Evidence boundary

This closes the functional room/transition matrix, not scene-exit desktop/PSP
DTRC alignment. `daScex_c` native state tracing remains a P4.1 dependency. No
new renderer, grounding, desktop oracle or proprietary package was modified.

Build identity:
`sha256:5d2f240e28ae54c5e1d493c7ec5ab1e0809f9c400dc98f1f795ee96d191b3cff`.

Network and fabricated traces: none.
