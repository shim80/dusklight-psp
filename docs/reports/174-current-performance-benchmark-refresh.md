# Current Performance benchmark refresh

## Result

P5.2 classification: `CURRENT_PERFORMANCE_SCOPE_VALID`.

All runs use the current EBOOT identity, the isolated PPSSPP profile, the
persistent GUI broker, hardware rendering and Vulkan. Markers and metrics are
valid, with no PSP runtime error.

## Benchmark v1 current results

| scene | frames | FPS average | 1% low | frame p95 (us) | draws | RAM peak | EDRAM peak |
|---|---:|---:|---:|---:|---:|---:|---:|
| boot intro | 600 | 31.440 | 2.518 | 29,989 | 71 | 9,005,816 | 1,900,672 |
| title | 600 | 35.709 | 34.491 | 28,785 | 71 | 9,005,816 | 1,900,672 |
| Link idle | 1,800 | 35.713 | 34.489 | 28,793 | 71 | 9,005,816 | 1,900,672 |
| room stress | 600 | 22.156 | 21.702 | 45,833 | 72 | 9,005,816 | 1,900,672 |
| transition | 600 | 31.440 | 2.518 | 29,989 | 71 | 9,005,816 | 1,900,672 |

Compared with the explicitly historical Performance baseline, FPS deltas range
from -0.010 to +0.028 and average frame-time deltas from -28 to +7 us. RAM peak
is +57,324 bytes in every scene; EDRAM peak is unchanged. These baseline files
are comparison inputs only, not current measurements.

## Startup v2 current results

| segment | FPS average | frame p95 (us) | draws | EDRAM peak |
|---|---:|---:|---:|---:|
| boot logos | 60.066 | 16,681 | 1 | 1,359,872 |
| title flow | 59.937 | 16,682 | 24 | 1,581,056 |

The title-flow run validates the New Game marker and reaches source stage
F_SP108. Its v2.1 profiler correctly reports RAM peak as unavailable instead
of inventing a value.

Benchmark v1 remains a diagnostic source subset and does not supersede startup
v2 visual fidelity. PPSSPP timing is provisional until real PSP validation.

Build identity:
`sha256:5d2f240e28ae54c5e1d493c7ec5ab1e0809f9c400dc98f1f795ee96d191b3cff`.

Network use: none.
