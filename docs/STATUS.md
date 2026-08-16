# Dusklight PSP — current status

Updated: 2026-08-16

## Project direction

Gameplay completeness is the dominant priority. The target is end-to-end completion of the game with original mechanics and event behavior. PSP graphics remain intentionally conservative until gameplay systems are substantially complete.

## Gameplay P1 advances

Preserved gameplay work includes source-derived room handoff and locomotion, original `DOOR20` sequencing, source chest/TBOX flow, deferred Demo_Item acquisition semantics, GETA/GETAWAIT resources, Heart Piece source identity/placement and a PPSSPP GETAWAIT visibility proof.

PR #2 (`Make Demo_Item commit source-owned`) remains draft. Its source-owned `dead() -> normal execute -> execItemGet()` semantics compile, but the changed lifecycle still requires an asset-backed PPSSPP replay through item-get, persistence and clean return to locomotion before merge.

PR #4 (`Fix PSP import stub ordering`) was validated in its own reproducible scope and merged into `agent/source-demo-item-commit` as merge commit `76e3dbedb56970283bfcee4225bf41cd4106836d`. That closes the linker-order implementation on the stacked source branch; it does not relax PR #2's asset-backed gameplay gate.

PR #5 (`Add BRK runtime plumbing for source item effects`) remains draft. Its current PSP compile/smoke path is green, but it is foundational only and still lacks a dedicated functional BRK CI gate plus asset-backed Heart Piece render integration. Gameplay-first work remains higher priority.

## Startup/save/control checkpoint

Branch `agent/startup-save-flow` contains the persistent startup/save/control boundary required by the release path:

- exactly three save slots with clamped, non-wrapping selection;
- Cross or START confirms a file;
- default new game is `F_SP108`, room 1, start point 21, layer 0;
- `DPSV` v1 persistence with CRC32 restores exact metadata and gameplay context;
- new files persist before gameplay handoff;
- read/write failures fail closed;
- recreation resumes the persisted context;
- PSP controls use analog movement, Cross action, L/R camera, Triangle/Square zoom, START pause, Circle cancel, D-pad menu navigation and SELECT debug;
- one-shot actions are edge-triggered.

Host markers retained by CI:

- `STARTUP_SAVE_FLOW_HOST_OK`
- `STARTUP_SAVE_INTEGRATION_HOST_OK flow=intro-title-file-select-transition-gameplay new_game=F_SP108/R01/start21 immediate_persist=true continue_context=true gameplay_checkpoint=true`
- `PSP_CONTROLS_HOST_OK analog=deadzone+normalized move=stick action=cross camera=L/R zoom=triangle/square pause=start cancel=circle menu=dpad debug=select edges=debounced`
- `FIRST_PLAYABLE_CONTROLS_HOST_OK movement=displacement camera=manual_runtime action=source_prompt pause=enter_resume fail_closed=1`

## Latest canonical public checkpoint

`test/dusklight-psp/` is an explicitly reconstructed canonical target, not a byte-for-byte recovery of the lost historical launcher. Its minimal PSP bootstrap calls `dusk::psp::game::run_canonical_game()`.

Latest fully green public proof: GitHub Actions run `31679685111`, commit `3318caf88e8ec2cdf84cebc54408dac5fc01cdea`.

That run proves:

- save/startup/control and first-playable-control host semantics;
- canonical F_SP108 asset-path contract and strict eight-file preflight;
- pinned PSPDEV/PPSSPP bootstrap;
- canonical driver + Link runtime + room/collision/movebg + PSP playable renderer compile and final link for Allegrex;
- `psp-objdump` reports `architecture: mips:allegrex`;
- the generated canonical EBOOT boots in pinned PPSSPP 1.20.4;
- public PPSSPP does not emit asset-backed gameplay/control proof markers when commercial-derived assets are absent.

Canonical EBOOT SHA-256: `9fce67ba95f3ff6c3be7ce674be324711ff03e17f1986b4a7993fe8f4129d5a2`.

CI artifact ID: `9172983520`.

## Authorized asset-backed first-playable proof

The workspace asset bundle was located and used locally without uploading commercial-derived data to GitHub. It contains the exact F_SP108/Link/HUD first-playable packages plus packaged startup assets.

The run used the run-82 EBOOT above and the pinned PPSSPP 1.20.4 AppImage whose SHA-256 is `661c098e6b7f7610171a57b7c533ce8bba6f2312b71e76d61e850461973eba21`.

The first asset-backed attempt exposed a real compatibility boundary: preserved `hud.dpui` is a valid CRC-checked compact DPUI v2 with 31 records, while the later general validator requires a complete printable-ASCII glyph set. Commit `3318caf88e8ec2cdf84cebc54408dac5fc01cdea` keeps the general DPUI validator strict and adds a canonical-only compatibility validator that still requires the complete binary structure, CRC, atlas bounds and original gameplay/pause sprite IDs `0–3`, `10–20`, `30`, `40–43`.

After that fix, the same local asset set successfully crosses:

`file select -> persistent slot creation -> NewGameTransition -> F_SP108/R01/start21 -> RealRoomRuntime -> actor runtime -> Link skin/runtime -> PSP renderer`

A real PPSSPP framebuffer visibly shows Link rendered inside F_SP108. The approved project capture is committed at:

`screenshot/dusklight-psp-f-sp108-gameplay.jpg`

The full native-resolution local evidence is retained separately from the repository JPEG. Analog gameplay input was also exercised after the first visible frame and produced an observable Link orientation/locomotion change. This closes the previous blocker “visible F_SP108 gameplay rendering proof”.

The PSP-side one-shot marker remains encoded to fire only after a successful frame:

`DUSKLIGHT_PSP_FIRST_PLAYABLE_RENDER_OK stage=F_SP108 room=1 start=21 actors=9 render=opaque_unlit frame=1`

PPSSPP's host stdout in this headless local configuration does not relay the platform `log()` channel, so the accepted visual proof is the actual framebuffer plus the same EBOOT's fail-closed initialization path, not a fabricated host log marker.

## Canonical first-room handoff

The selected persistent `StartContext` is consumed by the canonical driver. The currently supported first-playable contract is exactly `F_SP108 / room 1 / start 21 / layer 0`.

Required room packages:

- `data/stages/F_SP108/R01/room.dprm`
- `data/stages/F_SP108/R01/room.dptx`
- `data/stages/F_SP108/R01/room.dpcl`
- `data/stages/F_SP108/R01/room.dpsc`

Required global Link/HUD resources:

- `data/common/link.dpsk`
- `data/common/link.dptx`
- `data/common/link.dpan`
- `data/common/hud.dpui`

Before entering gameplay, `run_canonical_game()` requires 599 source scene actors, start point 21, successful room spawn, 9 essential active source actors/create calls, valid Link animation/skin state and a successful PSP room+Link frame submission.

## Rendering profile

The canonical driver uses the deliberately conservative profile:

- `presentation::Profile::OpaqueOnly`;
- `RenderProfile::KnownGoodUnlit`;
- lighting off;
- fog off;
- shadows off.

This is intentional while gameplay coverage remains incomplete.

## Screenshot policy

Repository `screenshot/` is reserved for source-faithful game UI or visible asset-backed gameplay. Text-only diagnostics, synthetic/public probes, boot screens and technical framebuffer captures remain CI evidence only.

The first accepted asset-backed gameplay capture is now `screenshot/dusklight-psp-f-sp108-gameplay.jpg`.

## F_SP102 startup environment checkpoint

Draft PR #12 (`Render complete F_SP102 startup environment`) is based directly on current `main` and remains intentionally unmerged until asset-backed visual acceptance.

The source-safe exporter combines the five F_SP102 room BMDs and four stage/sky BMDs into the existing DPRM/DPTX v2 runtime contract. Two independent local exports are byte-identical: 24,263 vertices, 21,513 triangles, 46 submeshes, 46 materials, 34 textures and 565,888 texture bytes. The generated commercial-derived packages remain local.

GitHub Actions run `31776948473` on commit `f5dac2ed771b5979a123a80b9ee2b37e6c18e495` is green for exporter/contract checks, pinned PSP toolchain bootstrap, Allegrex build, `mips:allegrex` verification and pinned PPSSPP smoke. Exact EBOOT SHA-256: `5ce0e73926752643670ab0a25b3d0fc872772883d365aaeeb33180061df1fb96`.

The canonical startup room paths now resolve to `data/startup/fsp102_environment.dprm` and `data/startup/fsp102_environment.dptx`. The progressive-scan startup segment also uses DPSU channel 3 instead of reusing the warning channel 0.

Visual acceptance is still open because the previously materialized startup asset bundle is not present in the current local runtime. Do not merge PR #12 or publish a release based only on the public smoke.

## PSP PC-fidelity/startup checkpoint

Branch `agent/psp-pc-fidelity-startup` resumes the recovered F_SP102 material-pass/title checkpoint on top of the PR #12 branch and changes the rendering direction toward bounded PSP fidelity rather than a single-texture approximation.

The DPTX contract is now v3 with `MPV1` material plans, capped at two GU passes per source material. The renderer applies per-pass texture selection, texture effect, blend policy and depth-write state. Recovered F_SP102 export metrics are 24,348 vertices, 21,513 triangles, 46 submeshes/materials, 40 textures, 656,128 texture bytes and 48 planned passes, classified as 2 exact, 41 approximate and 3 unsupported.

The title no longer uses the arbitrary 3000-unit billboard placement. It uses the source Item3D camera path with 45-degree FOV, eye `(0,0,-1000)`, title translation `(0,0,-430)` and mirrored X.

Generated startup is reduced to:

`Dusklight team logo -> F_SP102 title -> START -> file select/save -> gameplay`

Nintendo, Dolby, warning, progressive and realtime opening replay are no longer emitted by the new startup exporter. PSP controller mapping remains unchanged.

GitHub Actions run `31819052956` is green for MPV1 host validation, reduced startup runtime/export validation, pinned PSPDEV/PPSSPP bootstrap and a full Allegrex link. EBOOT SHA-256: `9f3ec5f9a937c694ae1e3b4be1a37468037652c4e54b86f8c95d9f9278345eca`. Proof artifact ID: `9226218542`.

This checkpoint does **not** close visual parity. The current execution environment does not expose the Twilight Princess ISO or Dusklight PC runtime/assets, so no new asset-backed PSP-vs-PC screenshot comparison was performed. Water/fog/background/F_SP108 tuning, UV/clamp/pass-order fixes driven by captures, and BPK/BRK/BTK material animation remain open.

Detailed report: `docs/reports/205-psp-pc-fidelity-startup-checkpoint.md`.

## Asset-backed reduced-startup replay

The branch now has an opt-in PPSSPP route proof for the complete intended sequence:

`Dusklight PSP team logo -> F_SP102 title -> START -> source-derived file select -> F_SP108/R01/start21`

Final local request `startup-title-fidelity-v12` completed in 15,208 ms with six
RGB565 captures, valid route metrics and marker
`DUSKLIGHT_PSP_STARTUP_ROUTE_CAPTURE_OK`. The EBOOT is an Allegrex binary with
SHA-256 `eb8d4412a674f18a0885ec659c681d1fc71c187ef637797b606c880bc6ad09e1`.

The canonical startup paths now match the packaged `title_room` assets, the
nonexistent camera package dependency is removed, and file select uses its DPSU
panels/cursor rather than debug text. The F_SP108 MPV1 upgrade is reproducible in the
asset build and the final gameplay capture retains the improved bounded water pass.

Two title-material passes were tested and rejected because they produced a white
rectangle and then a missing logo. Their code was removed and the recognizable
baseline restored. Title composition and source-derived file-select typography remain
open; this checkpoint is route/visual evidence, not PC parity.

Detailed report: `docs/reports/206-startup-route-file-select-ppsspp.md`.

## Active task

Close the first release path in the canonical EBOOT while increasing visible fidelity where the source assets are available:

`Dusklight logo -> title -> START -> file select/save -> F_SP108 first playable -> PSP controls`

Immediate targets:

1. capture the exact desktop reference for the same six route checkpoints;
2. address the remaining title TEV/UV/material-animation defects without exceeding the bounded PSP pass budget;
3. improve source-faithful file-select typography/content, then fog, far-background and scene layers;
4. preserve the already-proven save/control/gameplay route;
5. extend title/scene material animation only with source-backed BPK/BRK/BTK behavior.

## Explicitly not closed

- asset-backed visual acceptance of the reduced startup and title against Dusklight PC;
- full water/fog/background/scene-layer parity for F_SP102/F_SP108;
- complete TEV parity beyond the bounded two-pass PSP approximation;
- title and scene BPK/BRK/BTK material animation coverage;
- full first-playable control acceptance for camera/action/pause/resume in actual F_SP108;
- full inventory/pause UI fidelity;
- all chest sizes/items and full item message integration;
- source item BCK/BRK/TEV visual fidelity;
- broad NPC/enemy/boss, dungeon/map and cinematic coverage;
- advanced lighting/post-processing parity.
