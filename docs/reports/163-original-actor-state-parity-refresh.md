# Original actor state parity refresh

## Result

Classification: `ORIGINAL_ACTOR_STATE_PARTIAL_PARITY`.

The eight original source implementations remain present in the Allegrex
binary and pass create/execute/draw/delete host contracts. Their source-facing
state transitions are implemented, but none can be classified desktop/PSP
`MATCH` before native DTRC is emitted and aligned by `ParityActorId`.

| Actor | Source behavior exercised locally | State classification |
|---|---|---|
| `daScex_c` | destination/transition lifecycle | `PARTIAL_PARITY` |
| `daLv4HsTarget_c` | create, render, MoveBG lifecycle | `PARTIAL_PARITY` |
| `daObjLv4Gear_c` | dynamic rotation and pause stability | `PARTIAL_PARITY` |
| `daTagPoFire_c` | lifecycle and source self-delete | `PARTIAL_PARITY` |
| `daTboxSw_c` | treasure/switch persistence and deferred delete | `PARTIAL_PARITY` |
| `daTbox_c` | interaction, open event and item creation | `PARTIAL_PARITY` |
| `daLv4PoGate_c` | switch-driven open/close and matrix transport | `PARTIAL_PARITY` |
| `daObjSwSpinner_c` | interaction, rotation and switch activation | `PARTIAL_PARITY` |

## First missing evidence

The first shared gap is observation, not a proven runtime mismatch:

- native actor lifecycle/state event schema;
- PSP events emitted by the real executable;
- desktop events for the same scenarios;
- identity and tick alignment;
- first-divergence classification per actor.

Host metrics are retained as regression guards and are not serialized as DTRC.
No actor-specific offset, state hardcode, or copied desktop value is selected.

## Validation

- original actor parity matrix: PASS, eight sources and 18 canonical
  placements;
- five rigid/MoveBG actor hosts plus scene-exit, poFire and tbox-switch hosts:
  PASS;
- Allegrex compatibility library: PASS;
- aligned desktop actor scenarios: 0;
- aligned PSP actor scenarios: 0;
- state `MATCH`: 0; state `PARTIAL_PARITY`: 8;
- production changes, PPSSPP acquisitions, network, fabricated traces: none.

P4.1 owns the next causal step: add bounded generic instrumentation first,
then acquire only scenarios whose behavior exists. P3.2 is complete as an
honest state classification, not as a parity closure.
