# F_SP102 material-pass and title Item3D checkpoint

Date: 2026-08-14

## Scope

This checkpoint applies the first Daedalus-inspired rendering architecture to
the startup environment without changing the conservative gameplay profile or
touching issue #6. It also replaces the arbitrary title-logo billboard with
the dedicated source Item3D camera contract.

No extracted or converted commercial asset is committed. Local package
measurements below come from the authorized game image and are recorded only
as reproducibility evidence.

## Bounded material-pass contract

DPTX v3 adds an `MPV1` table with one fail-closed plan per source material and
at most two GU passes. Each plan records:

- `Exact`, `Approximate` or `Unsupported` fidelity;
- an explicit fallback reason;
- texture identity and texture/color operation per pass;
- blend and depth-write policy per pass.

DPTX v1/v2 remains accepted. A malformed v3 plan, out-of-range texture,
invalid pass count or invalid enum is rejected before rendering.

The current F_SP102 export is deterministic across two independent runs:

- 24,348 vertices;
- 21,513 triangles;
- 46 submeshes/materials;
- 40 textures, 656,128 texture bytes;
- 48 planned passes;
- 2 exact, 41 approximate and 3 unsupported material plans;
- DPRM SHA-256 `7af40bc7d9957c29d5bef2b9bca8ec11b0d89270a2118177354ab96dd8368b3c`;
- DPTX SHA-256 `066cf74114945a6b6c35f4fb2a900f26b4dec8acc5bb6a6ae30d621ab20e37a5`.

The large majority remains explicitly approximate: this is a correctness
foundation, not a claim of complete J3D TEV equivalence. In particular, the
three unsupported plans must remain visible in metrics and must not be
silently promoted to exact.

## Title-logo source contract

The original `daTitle_c::Draw` path sets the model translation to
`(0, 0, -430)` and mirrors X. The actor is submitted to the Item3D list, whose
renderer uses:

- perspective FOV 45 degrees;
- aspect ratio of the framebuffer;
- near/far planes 1 and 100000;
- eye `(0, 0, -1000)`;
- target `(0, 0, 0)` and up `(0, 1, 0)`.

The PSP path now switches from the F_SP102 cinematic camera to this dedicated
view before drawing the title model. The previous camera-facing placement at
an arbitrary distance of 3000 units has been removed. The title exporter also
retains the six shapes in joint-local coordinates. The previous package baked
their bind transforms into the vertices and the runtime then applied BCK a
second time, producing oversized, separated quads. All six source materials
use GX blending and are now assigned to the alpha-blend bucket instead of
being incorrectly emitted as opaque.

An opt-in repository-local startup capture request writes a raw 480x272,
stride-512 `GU_PSM_5650` framebuffer and material-plan metrics at the first
title-prompt frame. A separate exact `DUSKLIGHT_STARTUP_AUTOMATION_V1`
request supplies only the prompt inputs needed for unattended validation,
captures the first F_SP108 framebuffer and exits after proof. Without that
request, physical PSP input and state-machine behavior are unchanged.

## Validation

Passed locally:

- deterministic F_SP102 export;
- `MATERIAL_PASS_PLAN_HOST_OK plans=1 passes=2 negative_cases=2`;
- `STARTUP_TITLE_ASSET_HOST_OK room=DPRM,DPTX title=DPRM,DPTX` against the
  generated F_SP102 v3 packages and local title packages;
- canonical startup asset paths, camera format, save flow and full
  startup/save integration host tests;
- exact `demo38_01.stb` camera export at 30 Hz: 2,400 source ticks and
  2,401 DPCM samples, byte-identical across two passes;
- DPCM runtime validation and desktop-trace oracle parity with the source
  two-tick `dDemo::start()` offset;
- Allegrex compile and link with `architecture: mips:allegrex`;
- EBOOT SHA-256
  `ca75544bea82b549d06ae4a8232cbeac67d7f31efc4f517bb45dfd437b58b6ff`.

The exact source camera is now materialized locally from the legally supplied
GZ2P01 revision-0 image. Source STB SHA-256 is
`e335d6d44c002dd25881aedd2f053a226be18cdd254d2049e0d78f2aa88b735d`;
generated DPCM SHA-256 is
`ae4630366f6c6599813674b6c79929fcec0ad2d6b747ea9a4eb1f8dd68be438f`.
The commercial-derived DPCM remains ignored and is not committed. The seven
legacy checkpoints are used only as an independent oracle and never as input.

Pinned PPSSPP v1.20.4 was launched directly in the current Aqua session with
HOME, configuration, cache and temporary storage redirected into the isolated
repository profile. The one-shot process completed the exact route and was
then terminated. It produced:

- a 278,528-byte title framebuffer and
  `DUSKLIGHT_PSP_STARTUP_CAPTURE_OK`;
- a 128-byte CRC-protected `DPSV` save;
- a 278,528-byte first-playable framebuffer and
  `DUSKLIGHT_PSP_STARTUP_SAVE_GAMEPLAY_OK` for
  `intro,start,file_select,new_game,F_SP108`.

The corrected deterministic title packages have SHA-256 values DPRM
`9bc1a7440c43d2c595a3c6ca50bdbabc54dde72ee1f87b0b41909f511f397689`,
DPTX `ca144711f564c8c057e1651468f90dc9fd889f943068fc9016213f36aa34e977`
and DPAN `3cdb38faa1711ed3352020ef341115ddc43c721b4a97da8cd87cf31bdc8ca537`.
The title capture is recognizable at the source Item3D scale and the prior
opaque/double-transform defect is removed. The F_SP102 frame still visibly
contains the declared approximate/unsupported material results. Title
BPK/BRK/BTK material animation remains unported, so no global visual parity
claim or repository screenshot acceptance is made.

The earlier launchd broker attempt still fails with `EX_CONFIG`. It remains
unbootstrapped; the successful validation did not install or leave a
persistent process.

## Next gate

Port the bounded source title BPK/BRK/BTK state, then refine only the visibly
wrong approximate/unsupported F_SP102 plans. Replay the already-proven
one-EBOOT route after each causal correction and reserve repository screenshot
acceptance for a source-faithful result.
