# MoveBG model/collision parity

## Result

Classification: `MOVEBG_LOCAL_MATRIX_PARITY_CLOSED`.

The five canonical MoveBG model resources and their six collision resources
remain in source-local space. Original actor code produces the render and
collision matrices, and the PSP runtime commits the returned collision matrix
to `PspMoveBgWorld` during the same `move_bg_execute()` call. No constant
offset, recentering, bounds pivot, or frame-delay correction is justified.

| Actor | Model resource(s) | Collision resource(s) | Matrix relationship | Local result |
|---|---|---|---|---|
| `daLv4HsTarget_c` | `L4HsMato:4` | `L4HsMato:7` | source base matrix returned to MoveBG | MATCH, lag 0 |
| `daLv4PoGate_c` | `L4R02Gate:4` | `L4R02Gate:7` | source base matrix returned to MoveBG | MATCH, lag 0 |
| `daObjSwSpinner_c` | `P_Sswitch:4`, `:5` | `P_Sswitch:9`, `:8` | separate source base/top matrices | MATCH locally |
| `daTbox_c` | `Dalways:13` | `Dalways:27`, `:28` | model matrix copied to `mBgMtx`; collision swaps preserve it | MATCH locally |

`P_Gear` is rigid but not MoveBG and is therefore outside this subtask.

## Runtime path

1. The original actor computes its model matrix in `setBaseMtx()`.
2. The same source matrix is stored in or returned as its MoveBG matrix.
3. `PspStaticModelRuntime::move_bg_execute()` calls the original `Execute()`.
4. The returned `Mtx*` is copied unchanged into `movebg::Matrix34`.
5. `PspMoveBgWorld::create()` or `update()` receives it before the execute call
   returns.

The spinner's second collision uses the source actor's explicit secondary
`dBgW` path. The chest's closed/open collision swap likewise retains the
source-owned `mBgMtx`; neither path invents a platform presentation transform.

## Evidence and validation

- canonical object audit: five MoveBG models, all collision resources present,
  none recentered;
- target host test: eight instances, model/collision matrix parity, lag 0;
- gate host test: open and close paths, matrix parity, lag 0;
- spinner host test: 400 frames, 384 mechanism changes, balanced MoveBG
  create/delete, zero runtime errors;
- chest host test: two placements, animation/open event path, zero model
  errors;
- Allegrex actor and collision libraries: PASS;
- production sources changed: none;
- network and PPSSPP acquisitions: none.

Desktop/PSP behavioral `MATCH` is not claimed here. Aligned DTRC remains the
later P3/P4 gate; this task closes only the local origin and matrix transport
contract required before those traces are interpreted.
