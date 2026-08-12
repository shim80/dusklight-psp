# Local BCK Hermite extraction

## Result

The existing exact J3D loader now supports an opt-in, repository-local dump of
the ANK1 curve tables used by the four playable Link clips. Extraction was
performed offline from the already authorized local image. No asset or raw
curve value is versioned.

Source identity:

- disc ID: `GZ2P01`;
- revision: `0`;
- image size: `1,459,978,240` bytes;
- image SHA-256: `ab8a811eb484ab5db065d69dde4a343bbebd897c5d74ad447632aff4722cc652`.

The ignored exact dump has schema `dusklight.bck.ank1.curves.v1` and SHA-256
`97f8c04255361cce8cc3474f2344a82fd0d50f6714ad18c8d53bc8d866143a25`.
It records resource identity, duration, loop mode, rotation decimal shift,
joint/component/axis, key times, values, incoming/outgoing tangents and the
J3D interpolation mode.

## Aggregate evidence

| Clip | Resource | Duration | Loop | Tracks | Keys | Hermite tracks |
|---|---:|---:|---:|---:|---:|---:|
| `waits.bck` | 618 | 45 | 2 | 315 | 470 | 48 |
| `walks.bck` | 631 | 24 | 2 | 315 | 673 | 75 |
| `dashs.bck` | 205 | 24 | 2 | 315 | 734 | 76 |
| `stepl.bck` | 563 | 20 | 2 | 315 | 466 | 66 |

All four clips contain 35 joints and use rotation decimal shift 1. The total is
1,260 component/axis tracks, including 265 multi-key Hermite tracks.

All multi-key tracks in these four resources use ANK1 tangent type 0: one
tangent value per key is used as both incoming and outgoing tangent. Therefore
the unresolved DPAN v1 issue is not loss of distinct in/out tangents. It is the
replacement of the continuous Hermite curve by integer-frame transform samples
and runtime interpolation between those samples.

## Validation

- exact loader build: `LINK_LOADER_PROBE_BUILD_OK`;
- image identity and revision validation: passed;
- all four fixed resource IDs and names: passed;
- output JSON parsing and schema check: passed;
- finite J3D frame-zero evaluation: passed;
- network: not used;
- Allegrex: not applicable to this host-only extractor change.

This evidence establishes that Hermite data exists, but not yet that it causes
the first PSP divergence. DPAN v2 remains unauthorized until the exact
J3D-versus-DPAN evaluation in P0.3 proves causality.
