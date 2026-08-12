# F_SP108 immediate portability

## Result

Classification: `NO_IMMEDIATELY_PORTABLE_F_SP108_ADAPTER`.

None of the six original classes behind the nine lifecycle-only adapters
passes both the host and PSPSDK source-closure probes. No adapter is replaced.

| Original source | Host first blocker | PSPSDK first blocker |
|---|---|---|
| `d_a_kytag14.cpp` | missing `dolphin/ppc_math.h` | missing `dolphin/ppc_math.h` |
| `d_a_obj_digplace.cpp` | missing `fopAcStts_NOEXEC_e` | missing `fopAcStts_NOEXEC_e` |
| `d_a_tag_camera.cpp` | missing `dolphin/ppc_math.h` | missing `dolphin/ppc_math.h` |
| `d_a_tag_event.cpp` | missing `dolphin/ppc_math.h` | missing `dolphin/ppc_math.h` |
| `d_a_swc00.cpp` | conflicting `Vec` definition | missing `dolphin/ppc_math.h` |
| `d_a_tag_attack_item.cpp` | conflicting `Vec` definition | missing `dolphin/ppc_math.h` |

These are closure failures, not proof that adding a header alone would deliver
behavioral parity. Each class also requires its switch, event, camera,
collision, or actor-status dependencies to be audited before integration.

## Validation

- reproducible class-level probes: 6 host and 6 PSPSDK, all failed before
  source-compatible integration was established;
- existing nine-adapter DPSC/oracle audit: PASS;
- existing F_SP108 first-playable host runtime: PASS;
- replacements performed: 0;
- production changes, PPSSPP acquisitions, network: none.

P3.4 is complete because its replacement condition is false. The lifecycle
adapters remain the honest implementation until a separate class-closure task
proves one of the original sources portable.
