# V3 depth capture observability

## Result

```text
V3A_DEPTH_STATE=DONE
V3B_OPAQUE_PROFILE=DONE
V3C_RAW_DEPTH_EDRAM_READBACK=BLOCKED_LOCAL_EMULATOR_OBSERVABILITY
V3C_BEHAVIORAL_DEPTH_EVIDENCE=READY
V3D_DEPTH_VISUAL_VALIDATION=WAITING(V3C_BEHAVIORAL_DEPTH_EVIDENCE)
V3_DEPTH_VISUALIZATIONS=PENDING
DEPTH_PIPELINE.OK=NOT_EMITTED
```

The bounded PSP seam is present and disabled outside `parity_trace` with the
`opaque_only` presentation. It captures after exactly four rendered frames,
uses the same EBOOT and isolated PPSSPP profile as the accepted opaque run,
and writes a 512-by-272 16-bit color target plus the nominal 16-bit depth
target. It does not change simulation input, update count, material buckets,
the default presentation, Performance, or the V1/V2 benchmarks.

The raw seam cannot be used as the required V3D proof. Both permitted host graphics
transports booted and completed successfully, but PPSSPP's software PSP
renderer exposed the nominal depth EDRAM range as 278,528 zero bytes. A
zero-only source cannot support honest raw, linearized, write-mask,
depth-test-failure, or near/far images. Draw-order events remain available in
the render trace, but they are not a spatial depth visualization and are not
substituted for one.

## Changes already isolated in V3C

| Commit | Change |
|---|---|
| `4ffabb0` | bounded opaque color and nominal depth capture |
| `1d27aa1` | copy source changed from a relative GU address to the absolute EDRAM depth target |
| `3e820ef` | direct synchronized EDRAM read used to exclude a GU image-copy interpretation issue |

The public PSP function rejects an uninitialized renderer, null output, and a
buffer smaller than 278,528 bytes. The caller writes only
`V3_OPAQUE_COLOR.5650` and `V3_DEPTH_RAW.u16`, and only at the bounded end of
the F_SP108 opaque parity run.

## Allegrex and runtime validation

The canonical Allegrex target compiled after each change. Both broker runs
reported:

- `boot_observed=true`;
- `classification=MARKERS_VALID_METRICS_VALID`;
- `markers_valid=true`;
- `metrics_valid=true`;
- `psp_renderer=software`;
- four render frames and the accepted opaque-only submission trace;
- no PPSSPP runtime or host graphics error.

The current identities are:

```text
PARITY_BUILD_ID=sha256:99eaac40e0773f10f3b50edad41dae091c21b96abe4ccda7f124daee4ea57846
VISUAL_BUILD_ID=sha256:a7a0d11496de780b1a885b9510a9f1e34e26fc00ea87116244beb7bf89276d7f
```

## Two-transport evidence

| Request | Host backend | PSP renderer | Color SHA-256 | Depth SHA-256 | Depth values |
|---|---|---|---|---|---|
| `20260802T105005Z-parity_trace-1` | OpenGL | software | `d78f1e647f1460e8d95a1abcbdd2749f90bea3688fcdcdec4ed1f1b195490cf2` | `79095a8a9c81e1d95e90442adcddeae93061fac4611d4c1f768850eaf6787c69` | one unique value, min/max 0, nonzero 0 |
| `20260802T105051Z-parity_trace-1` | Vulkan | software | `d78f1e647f1460e8d95a1abcbdd2749f90bea3688fcdcdec4ed1f1b195490cf2` | `79095a8a9c81e1d95e90442adcddeae93061fac4611d4c1f768850eaf6787c69` | one unique value, min/max 0, nonzero 0 |

The color and depth hashes differ, so the collector did not alias or rename
the color capture. OpenGL and Vulkan produce the same depth bytes. This is a
local observability defect after the PSP program has booted and completed; it
is not `BLOCKED_PPSSPP_BOOT` and it is not evidence that depth testing failed.

## Revised consequence for the task graph

The bounded raw seam is retained as optional evidence for a future real PSP or
diagnostic PPSSPP run. It is no longer a mandatory V3D dependency. V3D now
depends on the independent behavioral-depth contract, which is allowed to
prove the same depth semantics through color-buffer witnesses, synthetic GE
fixtures, order invariance, host prediction, occlusion pairs and state
isolation. Therefore:

- `DEPTH_PIPELINE.OK` is deliberately absent;
- `V3C_BEHAVIORAL_DEPTH_EVIDENCE` is ready;
- `V4C_ALPHA_RUNTIME_INTEGRATION` remains waiting only on V3D;
- no alpha bucket or canonical renderer behavior was changed;
- V4A, V4B, the UI source audit, and the effective lighting audit proceeded
  independently and are complete;
- Performance, PSP conservative, and the one allowed final release run remain
  unopened.

The next honest depth proof is the behavioral validation contract. No network
access is required. A color-only reconstruction, a constant mask, or a
draw-order timeline alone must not be labeled as sufficient depth proof.

```text
raw_depth_readback_available=false
raw_depth_readback_backend=PPSSPP
raw_depth_readback_status=UNOBSERVABLE
raw_depth_readback_required_for_acceptance=false
real_psp_raw_depth_validation=pending
```
