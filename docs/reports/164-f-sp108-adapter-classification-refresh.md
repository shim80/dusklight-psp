# F_SP108 adapter classification refresh

## Result

Classification: `F_SP108_ADAPTERS_LIFECYCLE_ONLY`.

All nine essential F_SP108 records still instantiate the bounded PSP type
`dusk::psp::actor::EssentialSourceActor`. Their DPSC identity, parameters and
transforms match the desktop activation oracle exactly, but none of the six
real source classes is compiled through this path. Consequently all nine rows
remain `ADAPTER_EXACT_LIFECYCLE_ONLY` with `MISSING_BEHAVIOR`.

| Source class | Records | Transform matches | Original behavior in path |
|---|---:|---:|---|
| `kytag14_class` | 2 | 2 | no |
| `daObjDigpl_c` | 1 | 1 | no |
| `daTag_Cam_c` | 1 | 1 | no |
| `daTag_Event_c` | 1 | 1 | no |
| `daSwc00_c` | 3 | 3 | no |
| `daTagAtkItem_c` | 1 | 1 | no |

The adapter is not presented as a Dusklight source type, draws no placeholder
geometry, and cannot receive a behavior or pixel `MATCH` from transform
equality alone.

## Validation

- exact DPSC/oracle audit: PASS, 599 source records, nine adapters and zero
  transform error;
- three negative classification cases: PASS;
- first-playable host runtime: PASS, nine instantiated actors;
- missing behavior: 9; original sources ported through this path: 0;
- production changes, PPSSPP acquisition, network: none.

P3.4 is unblocked to determine whether any whole original class is immediately
portable. Replacement remains forbidden until that class-level dependency and
behavior proof exists.
