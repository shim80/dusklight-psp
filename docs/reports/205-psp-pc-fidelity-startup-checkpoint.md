# PSP PC-fidelity/startup checkpoint

Date: 2026-08-14
Branch: `agent/psp-pc-fidelity-startup`
Validated code commit: `a39a280fb1876f0995d357a53a63ff70eaee5f98`

## Scope

This checkpoint resumes the recovered F_SP102 material-pass/title work and changes the priority from simple route completion to bounded PSP rendering fidelity.

Implemented:

- DPTX v3 `MPV1` material plans with a maximum of two PSP GU passes per source material;
- explicit material fidelity/fallback reporting instead of silently claiming TEV parity;
- preservation of source texture identities in PEV1 and pass-specific texture selection;
- per-pass texture effect, blend policy and depth-write state in `playable_render.cpp`;
- source Item3D title camera semantics: 45 degree FOV, eye `(0,0,-1000)`, model translation `(0,0,-430)` and mirrored X;
- title alpha rendering after the F_SP102 environment instead of the previous arbitrary 3000-unit billboard placement;
- startup reduced to `Dusklight team logo -> F_SP102 title -> START -> file select/save -> gameplay`;
- Nintendo, Dolby, warning, progressive prompt and realtime opening replay removed from the generated startup sequence;
- original port-owned Dusklight PSP team card generated without commercial logo assets;
- existing PSP controls preserved: analog movement, Cross action, L/R camera, Triangle/Square zoom, START pause, Circle cancel, D-pad menu and SELECT debug.

## F_SP102 material export checkpoint

Recovered/exported contract:

- 24,348 vertices;
- 21,513 triangles;
- 46 submeshes/materials;
- 40 textures;
- 656,128 texture bytes;
- 48 planned PSP passes;
- fidelity classification: 2 exact, 41 approximate, 3 unsupported.

The two-pass ceiling is deliberate. It follows the PSP/Daedalus-style bounded-pass approach for predictable GU cost, while retaining explicit fallback metadata for source material behavior that cannot be represented faithfully.

## Validation

GitHub Actions run `31819052956` passed:

- `MATERIAL_PASS_PLAN_HOST_OK plans=1 passes=2 negative_cases=2`;
- `STARTUP_RUNTIME_HOST_OK`;
- `DUSKLIGHT_STARTUP_SEQUENCE_OK segments=5 flow=team_logo,title,start,file_select`;
- pinned PSPDEV bootstrap;
- pinned PPSSPP 1.20.4 bootstrap;
- full 35-object PSP target compile/link;
- `psp-objdump` identity `architecture: mips:allegrex`;
- EBOOT generation.

EBOOT SHA-256:

`9f3ec5f9a937c694ae1e3b4be1a37468037652c4e54b86f8c95d9f9278345eca`

Proof artifact ID: `9226218542`.

## Explicit limitations / next visual work

This is not a claim of PC visual parity. The current execution environment did not expose the user's Twilight Princess ISO or the Dusklight PC runtime/assets, so an asset-backed PPSSPP versus Dusklight PC screenshot comparison could not be replayed here.

Still open and prioritized:

1. asset-backed F_SP102/F_SP108 replay and side-by-side visual acceptance against Dusklight PC;
2. water, fog, far-background and scene-layer fidelity, especially F_SP102/F_SP108;
3. alpha-test/blend/depth ordering fixes driven by actual foliage/water captures;
4. UV/clamp/wrap and pass-order corrections for surfaces that still break visually;
5. source BPK/BRK/BTK material animation coverage for title and affected scenes;
6. expand bounded material compilation where the current 41 approximate / 3 unsupported F_SP102 materials show visible errors.

Do not promote this checkpoint as full rendering parity until those asset-backed comparisons are completed.
