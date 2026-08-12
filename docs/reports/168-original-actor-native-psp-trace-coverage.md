# Original actor native PSP trace coverage

## Result

Classification: `ORIGINAL_NATIVE_PSP_DTRC_PARTIAL`.

The current build has real, bounded, broker-acquired PSP DTRC for five of the
eight compiled original-source actor classes. These traces are native PSP
evidence but remain unaligned because no matching desktop actor DTRC exists.
No state row is promoted to `MATCH`.

| Scene | Original classes | Instances | Lifecycle events | State/transform events | Total events |
|---|---|---:|---:|---:|---:|
| D_MN10/R09 | `daLv4HsTarget_c`, `daObjLv4Gear_c`, `daObjSwSpinner_c` | 7 | 42 | 4,200 | 9,946 |
| D_MN10/R02 | `daTbox_c`, `daLv4PoGate_c` | 3 | 18 | 1,800 | 7,522 |

Both acquisitions ran for 300 ticks through broker generation 2, used the
isolated Functional OpenGL/software profile, observed PSP boot, validated
markers and metrics, reported no PSP runtime error, and dropped zero trace
events.

Authoritative build identity:

```text
sha256:5d2f240e28ae54c5e1d493c7ec5ab1e0809f9c400dc98f1f795ee96d191b3cff
```

## Remaining sources

- `daScex_c`: real instances exist, but revealing state belongs to the room
  transition scenario owned by P4.3.
- `daTagPoFire_c`: no placement exists in the canonical R09/R02/F_SP108
  packages; its local source locations are R01/R03/R05/R08.
- `daTboxSw_c`: no canonical direct placement exists; its host test exercises
  dynamic coupling to treasure state.

The earlier scenario metadata incorrectly placed `daTagPoFire_c` in R09 and
`daObjSwSpinner_c` in R02. It now follows the actual DPSC/portability inventory:
spinner is in R09; R02 contains the traced chest and gate placements.

## Evidence boundary

- native PSP actor sources covered: 5/8;
- desktop-aligned actor sources: 0/8;
- host output promoted to trace: no;
- fabricated traces: 0;
- network: none.

P4.1 waits only for transition evidence and additional canonical room packages
or a real dynamic-coupling scenario. Independent P4.2-P4.5 work remains ready.
