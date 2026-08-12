# Scene and actor matrix current-trace refresh

## Result

P4.5 classification: `DONE_WITH_UNALIGNED_NATIVE_PSP_EVIDENCE`.

The generated matrices now distinguish current native PSP traces from stale or
missing evidence. No row is promoted to `MATCH` without an aligned desktop
counterpart.

## Inventory

- 40 declared scene scenarios;
- 10 current desktop DTRC scenarios;
- 2 current PSP DTRC scenarios: D_MN10 R09 and R02 actor runs;
- 748 actor placement rows;
- 18 `ORIGINAL_SOURCE` placements;
- 10 placements with current native PSP evidence: seven in R09 and three in
  R02;
- five of eight original actor source families represented on PSP;
- zero scene or actor `MATCH` classifications;
- zero fabricated traces.

The actor lifecycle rows carrying current PSP events are classified
`native_psp_unaligned`. This records real execution without confusing it with
cross-platform equivalence.

## Validation

- scene matrix host test: pass;
- actor matrix host test: pass;
- original native trace coverage inventory: `PARTIAL`, five of eight sources;
- whitespace and patch validation: pass.

Build identity:
`sha256:5d2f240e28ae54c5e1d493c7ec5ab1e0809f9c400dc98f1f795ee96d191b3cff`.

No production source, desktop oracle, package or network resource was changed.
