# PSP render-state trace V2

## Result

V2 is closed on parity build
`sha256:88be78511640ecd6102e14488a96ea767cd0fc33db01e0fee99af52cefa84247`
and VISUAL_BUILD_ID
`sha256:d4426a6e902b7a824e9f400fa80939a9e6ee44a70b98ca0d0b374e8e24684a70`.

The production renderer now exposes a disabled-by-default trace sink at the
actual room, static-actor, Link and startup-UI submission points. The trace
records stable source, actor, material, shape and texture identities together
with depth-test/write, alpha-test/reference, blend, cull, fog and lighting
concepts. It does not query GU state and does not change submission order.

The generic boundary is capped at four distinct frames and 8192 submissions.
Its host test proves both limits. Allegrex compilation and final linking pass.

## PPSSPP evidence

One revealing Functional scenario was acquired through broker generation 2:

- request: `20260802T081042Z-parity_trace-1`;
- scenario: `d_mn10_r09_actors`;
- backend: OpenGL with PSP software renderer;
- boot observed: true;
- classification: `MARKERS_VALID_METRICS_VALID`;
- trace events: 256 over frames 0-3;
- sources: room 92, static actors 56, Link 108;
- buckets: opaque 252, alpha blend 4;
- DTRC stable-identity self-compare: PASS;
- captures: none requested or claimed.

The first acquisition exposed a missing `submission_id` semantic key. Commit
`0c192cd` corrected that generic identity field; the second acquisition passes.
No scenario- or tick-specific branch was added.

## Gate

V3 remains `WAITING_DEPENDENCY`. The PSP evidence is valid, but desktop V1 is
still `PENDING_GUI_EXECUTION`; opaque depth will not be modified without the
aligned source states.
