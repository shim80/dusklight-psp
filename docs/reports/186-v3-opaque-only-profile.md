# V3 opaque-only presentation profile

## Result

The repository now has an explicit `opaque_only` presentation profile. In the
real-room path it keeps clear, source camera, room opaque geometry, opaque
static models and Link. It disables alpha-test, alpha-blend, effects/entities,
fog, shadows, debug overlays and UI.

The first GUI request was rejected before PSP boot because the repository-local
runner allow-list had not yet learned the new profile. The worker validation
was updated without restarting the supervisor. A second request booted, but
showed that the F_SP108 render-only fast path returned before presentation
initialization. That ordering defect was corrected independently.

## Validation

Request `20260802T102706Z-parity_trace-1` passed through broker generation 3:

- classification: `MARKERS_VALID_METRICS_VALID`;
- boot observed: true;
- backend: OpenGL host with PSP software renderer;
- parity build ID:
  `sha256:fe833a6f29fe707a482a83f3a340ff9ea54754adf2ac1ac7e51414e095855959`;
- visual build ID:
  `sha256:5eef37a2440041c21182e64fb7b2d7ed04dd627df6668340962cede6ad1126cb`;
- trace SHA-256:
  `e7c157e9ec55d30c5d27608b879accd02826ceae13454fa2bc4881344d355e45`;
- four frames and 176 submissions;
- 68 room opaque submissions and 108 Link opaque submissions;
- zero alpha-test, alpha-blend, fog, UI, or other forbidden submission;
- both source depth-write policies retained.

The permanent verifier is `scripts/test-v3-opaque-trace.sh`.

## Gate

`V3_OPAQUE_PROFILE=DONE`

`V3_DEPTH_VISUALIZATIONS=PENDING`

`DEPTH_PIPELINE.OK` is deliberately not emitted yet. The requested depth raw,
linearized depth, write-mask, test-failure, draw-order and near/far visual
artifacts still require a bounded capture seam. V4 remains locked until that
evidence exists.
