# F_SP108 source render alignment

## Result

The DPRM and DPSK converters now preserve the original J3D material and shape
identities in their existing reserved records. The PSP renderer emits those
source identities instead of converter-local submesh ordinals.

The permanent provenance guard verifies:

- 22 distinct F_SP108 room source records, covering 22 shapes and 22 materials;
- 27 distinct Link source records, covering 18 shapes and 18 materials;
- no flattened all-zero identity table.

Host package validation, the targeted room/collision suite, DTRC v3.1 tests,
and a non-cached Allegrex build all pass.

## PPSSPP evidence

The first post-fix request, `20260802T094511Z-parity_trace-1`, was valid at
runtime but retained the pre-commit visual identity. It is transport evidence
only and is excluded from the alignment proof.

After rebuilding from commit `45d5253`, request
`20260802T094552Z-parity_trace-1` passed with:

- classification `MARKERS_VALID_METRICS_VALID`;
- OpenGL host backend and PSP software renderer;
- boot observed and result code zero;
- parity build
  `sha256:bb278159b770feaa3dd4784286792beed8782790a7bfecea7bc68ba30d3df2e7`;
- visual build
  `sha256:0d3d7ff7a9a4e6831944dceaf0181c9fd6c7e3aa8d77fb3caba0386e1821cd4f`;
- four frames and 196 real render submissions;
- trace SHA-256
  `ded91c4d14f13ec2cf9d40d50ffbb15935edd16d4817906d8bb1392229ad0fc5`.

## Source-derived join

The alignment tool selects the unique desktop model whose complete
material/shape pair set is a subset of the F_SP108 DPRM source table. This
selects desktop `(profile, actor, model) = (732, 35, 9)` without using draw
order, texture, colour, or spatial proximity.

Twenty of the 22 DPRM pairs have exact desktop submissions. Two source records
are submitted on PSP but absent from the complete desktop steady-state frame.
The resulting first-difference counts are:

- 10 `DEPTH_STATE_MISMATCH`;
- 5 `ALPHA_STATE_MISMATCH`;
- 5 `CULL_STATE_MISMATCH`;
- 2 `UNEXPECTED_ON_PSP`.

The detailed evidence is recorded in
`reference/parity/render-submission-f-sp108/RENDER_SUBMISSION_PARITY.csv` and
its companion Markdown report.

## Classification

`SOURCE_IDENTITY_JOIN_VALID_ROOM_ONLY`

This is real causal progress, but it does not close V1. The current traces
still lack the required model/view/projection matrices and screen bounds. Link
also lacks an unambiguous source-model record, so its repeated local
material/shape numbers cannot yet be joined honestly.

V3 therefore remains waiting. No opaque depth, alpha, culling, fog, or lighting
correction was made in this group.
