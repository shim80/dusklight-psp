# F_SP108 source depth-state convergence

## Result

The first V3 source-proven mismatch was caused by two independent generic
export errors. The DPRM bucket was derived from the converter-local material
index rather than the original J3D material index. After that correction,
three opaque submissions still differed because DPRM did not preserve their
source Z-write state.

The source model exposes two real `J3DZMode` IDs:

- `23`: depth test enabled, GX `LEQUAL`, depth write enabled;
- `22`: depth test enabled, GX `LEQUAL`, depth write disabled.

The DPRM submesh record now stores the source test/write flags and GX compare
function in previously reserved bytes. The PSP renderer applies the flags and
maps the GX comparison to the equivalent reversed-depth GE comparison used by
its projection. This is format-generic and contains no F_SP108 shape,
material, frame, or desktop result constant.

## Validation

- deterministic startup and PSP asset conversion: passed;
- source provenance: 22 room records, 22 shapes, 22 materials, two distinct
  depth states, passed;
- room/model/collision host parity for F_SP108, D_MN10/R09 and D_MN10/R02:
  passed;
- Allegrex canonical build: passed;
- PPSSPP GUI broker request `20260802T101829Z-parity_trace-1`: boot and markers
  valid, OpenGL host backend, PSP software renderer;
- parity build ID:
  `sha256:2c7cde890d286af0942020d8cf30e08c4cceefd0df32140075663e986917616a`;
- visual build ID:
  `sha256:f10622ceca5a4d35e560da99b1a307d3eb7c009bc8bd2ccd022eead827d3457a`;
- PSP trace SHA-256:
  `7b89639358fcf29cf44e1ecb532e05dc98e7422042aa0ae8bec5b4962e5cee77`.

Exact source-pair alignment progressed from seven to ten `MATCH` records.
`DEPTH_STATE_MISMATCH` fell from three to zero. The remaining seven alpha and
three blend mismatches belong to V4 and were not corrected here.

## Gate

`V3_SOURCE_DEPTH_STATE=DONE`

The remaining V3 work is the isolated `opaque_only` presentation and its
depth-pipeline evidence. Alpha classification remains locked until that gate
is complete.
