# Original actor native trace coverage

> Historical pre-instrumentation baseline. Current PSP coverage is documented
> in report 168.

## Result

Classification: `ORIGINAL_NATIVE_DTRC_COVERAGE_MISSING`.

All eight original Dusklight actor sources are compiled for Allegrex and pass
their host lifecycle contracts. None currently emits native DTRC state, and no
actor-specific desktop or PSP DTRC exists for the sixteen mapped revealing
scenario memberships. Host output is not promoted to trace evidence.

| Source | Process | Revealing scenarios | Native emitter | desktop/PSP DTRC |
|---|---|---:|---|---|
| `daScex_c` | `0x030C` | 2 | no | 0 / 0 |
| `daLv4HsTarget_c` | `0x009F` | 2 | no | 0 / 0 |
| `daObjLv4Gear_c` | `0x0183` | 2 | no | 0 / 0 |
| `daTagPoFire_c` | `0x017A` | 2 | no | 0 / 0 |
| `daTboxSw_c` | `0x016E` | 2 | no | 0 / 0 |
| `daTbox_c` | `0x00FB` | 2 | no | 0 / 0 |
| `daLv4PoGate_c` | `0x009D` | 2 | no | 0 / 0 |
| `daObjSwSpinner_c` | `0x00B3` | 2 | no | 0 / 0 |

The current `room-transition` DTRC writer serializes Link state only. Scenario
definitions for original actors exist, but acquiring them now would not yield
the required original state events.

## Metadata correction

Two scenario actor lists used the nonexistent class spelling
`daObjLv4PoGate_c`. They now use the exact compiled type
`daLv4PoGate_c`. This restores both the composite R02 and focused gate scenario
mapping without changing the runtime or desktop oracle.

## Validation

- automated coverage inventory: eight sources, zero native emitters, zero
  desktop traces, zero PSP traces, `fabricated_traces=0`;
- scenario inventory: 40 valid scenarios, 37 resources;
- original actor parity host matrix: PASS, eight lifecycle sources and 18
  canonical placements;
- Allegrex compatibility library: PASS;
- network and PPSSPP acquisitions: none.

P3.2 is unblocked to add generic, versioned original-actor lifecycle/state
events before any acquisition. P4.1 is likewise dependency-ready, but must not
run PPSSPP until the missing emitter is implemented; an empty or Link-only
trace would not be evidence.
