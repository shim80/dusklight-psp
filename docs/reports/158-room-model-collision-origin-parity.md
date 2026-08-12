# Room model/collision origin parity

## Result

Classification: `ROOM_MODEL_COLLISION_ORIGIN_CLOSED`.

The canonical `DPRM` room model and `DPCL` collision packages for
`D_MN10/R09`, `D_MN10/R02`, and `F_SP108/R01` use the same source-local
coordinate space and the same origin `(0, 0, 0)`. No production correction is
selected.

The reproducible check is
`tools/dusk_room_origin_audit/dusk_room_origin_audit.py`; its ignored detailed
result is `build/reports/room-model-collision-origin-audit.json`.

| Room | DPRM vertices | DPCL vertices | DPCL triangles | CRCs | stored/computed bounds | origin/space |
|---|---:|---:|---:|---|---|---|
| `D_MN10/R09` | 18,000 | 17,634 | 5,878 | valid | exact | equal/source-local |
| `D_MN10/R02` | 23,953 | 18,723 | 6,241 | valid | exact | equal/source-local |
| `F_SP108/R01` | 10,426 | 4,395 | 1,465 | valid | exact | equal/source-local |

## Coordinate-space proof

- DPRM conversion evaluates the J3D room with an identity base matrix and
  writes evaluated source-local positions without a bounds/floor recenter.
- DPCL conversion writes the decoded KCL triangle points directly.
- The room renderer calls `model_identity()` before submitting DPRM vertices.
- `CollisionWorld` reads DPCL vertices directly, without an actor or room
  translation.
- Bounds in both formats are validation/culling data. The audit recomputes them
  from every stored vertex; they are not treated as pivots.

DPRM and DPCL bounds are not required to be identical: display geometry and
collision geometry contain different source subsets. Their shared origin and
transform policy are the parity contract.

## Validation

- focused room-origin audit: PASS, three rooms;
- Python syntax check: PASS;
- fresh global origin audit: PASS, 37 packages, 11 models, zero pivot
  divergences;
- host `room_package_host_test` target compilation: PASS;
- Allegrex rebuild: not required because no production or package source was
  changed;
- PPSSPP acquisition: not required and not spent.

The legacy host room replay executable contains an R02-specific historical
floor expectation and is not used as a generic validator for R09 or F_SP108.
Its compile remains protected; the focused audit provides the package-space
evidence required by P2.2.

`DPSC` spawn placement is deliberately outside this task. A fresh global audit
confirmed the current canonical DPSC headers and source spawn records agree;
an older ignored JSON result was discarded.
