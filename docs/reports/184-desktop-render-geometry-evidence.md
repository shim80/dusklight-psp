# Desktop render geometry evidence

## Result

V1 now records neutral geometric evidence at each real J3D shape submission:

- the 3x4 model matrix from the submitting `J3DModel`;
- the 3x4 active J3D view matrix;
- the reconstructed 4x4 active GX projection matrix;
- camera-space depth bounds;
- normalized screen bounds derived from the real J3D shape bounds.

The instrumentation issues no GX draw, state-setting, flush, or synchronization
call. `GXGetProjectionv` only observes the current projection. The patch was
regenerated from the pinned vanilla snapshot and its reverse-application check
passes, repairing the stale unified-diff hunk counts at the same time.

## Validation

The targeted RelWithDebInfo desktop build and link completed offline. Request
`visual-v1-fsp108-geometry-evidence-final` then passed through broker generation
3 with:

- `DESKTOP_TRACE_VALID`;
- direct Mach-O Aqua transport, without LaunchServices;
- process, window, Dawn, Metal, disc, scene, and first draw observed;
- four completed frames;
- 3,373 events;
- 233 render submissions;
- 233 model matrices;
- 233 view matrices;
- 233 projection matrices;
- 233 camera-depth/screen-bound records;
- finite values and exact one-to-one submission coverage;
- zero overflow and zero dropped event;
- controlled clean exit;
- trace SHA-256
  `8b1841c0d8b61148b4bb2cfc2aa06387b50b2616886ea0353d8acbe4916dfd33`.

The adapter now rejects a trace if any submission lacks one of these records,
if an array has the wrong cardinality, or if a value is non-finite. Positive
and missing-bounds negative host tests pass.

## Gate result

`V1_DESKTOP_RENDER_TRACE=DONE`

Together with the exact 20-record F_SP108 room source join from report 183,
this satisfies the V1 trace and geometry gate for the room pipeline. Link's
multi-model/material visibility mapping remains partial and must not be joined
by draw order; it is not used to diagnose the room opaque-depth pipeline.

`V3_OPAQUE_DEPTH=READY`
