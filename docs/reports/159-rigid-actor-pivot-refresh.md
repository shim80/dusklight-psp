# Rigid actor pivot refresh

## Result

Classification: `RIGID_ACTOR_LOCAL_PIVOTS_CLOSED`.

The seven canonical rigid DPRM resources are source-local, are not recentered,
and receive the original actor matrix at runtime. The five original source
families are compiled into the PSP compatibility library. No production
correction or actor-specific offset is selected.

| Family | Models | Source matrix | Host matrix result | Cross-platform state |
|---|---:|---|---|---|
| target `L4HsMato` | 1 | translate actor × rotate ZXY | model/collision, lag 0 | `PARTIAL_PARITY` |
| gear `P_Gear` | 2 | translate actor × rotate Y dynamic | stable small/large cycles | `PARTIAL_PARITY` |
| gate `L4R02Gate` | 1 | translate actor × rotate ZXY | model/collision, lag 0 | `PARTIAL_PARITY` |
| spinner `P_Sswitch` | 2 | source matrices for base and top | 384 mechanism changes | `PARTIAL_PARITY` |
| chest `Dalways` | 1 | translate actor × rotate ZXY | source open/event path | `PARTIAL_PARITY` |

`daTagPoFire_c` is covered behaviorally but owns no `StaticModel` resource in
the canonical manifest. A geometric pivot classification for fire is therefore
`NOT_APPLICABLE`, not missing evidence.

## Audit correction

The previous landmark reader interpreted DPRM's UV and packed color fields as
XYZ. This produced invalid values, including NaNs. DPRM layout is:

```text
offset +0  UV
offset +8  RGBA8888
offset +12 XYZ
```

The audit now reads XYZ at `+12`. All seven models expose six finite extreme
landmarks, for 42 landmarks total. The seven negative contract cases still
detect recentering, actor/collision matrix drift, MoveBG frame lag, culling
space drift, interaction-origin drift, and shadow-origin drift.

## Validation

- object pivot audit and seven negative cases: PASS;
- five targeted original-actor host executables: PASS;
- Allegrex `dusk_psp_compat` and `dusk_psp_collision`: PASS;
- production sources changed: none;
- PPSSPP acquisitions spent: none;
- network used: no.

The full room-transition executable currently fails independently in its
campaign harness because several review/trace globals are not declared in
`test/room-transition/main.cpp`. This does not invalidate the targeted
Allegrex actor libraries, but it must be resolved before a later full
room-transition acquisition.

Cross-platform status intentionally remains `PARTIAL_PARITY`; local pivot
closure is not promoted to desktop/PSP `MATCH` without aligned real DTRC.
