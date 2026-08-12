# Dusklight PSP — current status

Updated: 2026-08-12

## Project direction

Gameplay completeness is the dominant priority. The target is end-to-end completion of the game with original mechanics and event behavior. PSP graphics remain intentionally conservative until gameplay systems are substantially complete.

## Historical parity foundation retained in this snapshot

The repository preserves reports from the earlier camera/input/animation, actor, room-transition and rendering campaigns. Historical SHAs in those reports are provenance: some original Git objects did not survive workspace reconstruction and must not be presented as current reachable commits.

## Gameplay P1 advances

Preserved gameplay work includes source-derived room handoff and locomotion, original `DOOR20` sequencing, source chest/TBOX flow, deferred Demo_Item acquisition semantics, GETA/GETAWAIT resources, Heart Piece source identity/placement and a PPSSPP GETAWAIT visibility proof.

The source-owned Demo_Item commit remains a draft merge candidate until the full asset-backed chest lifecycle is replayed after the `dead() -> normal execute -> execItemGet()` change.

## Startup/save/control checkpoint

Branch `agent/startup-save-flow` now contains a public PSP route that exercises the same runtime boundaries required by the release path without committing commercial assets.

Validated behavior:

- three file slots; initial selection 0; up/down clamp without wrap;
- Cross or START confirms a file;
- new game context is `F_SP108`, room 1, start point 21, layer 0;
- `DPSV` v1 persistence uses CRC32 and restores exact slot metadata;
- occupied files resume their persisted `stage/room/start/layer` context;
- `StartupSaveFlow` binds `StartupRuntime` file selection and `NewGameTransition` to persistence;
- new files are written before gameplay handoff;
- read/write failures fail closed;
- destruction/recreation resumes the persisted gameplay checkpoint;
- PSP gameplay controls are mapped through a host-testable mapper: analog stick movement with deadzone, Cross action, L/R camera, Triangle/Square zoom, START pause, Circle cancel, D-pad menu navigation and SELECT debug;
- one-shot actions are edge-triggered so held buttons cannot double-fire.

The PSP probe now consumes `StartupSaveFlow` directly and reads real `SceCtrlData` through the PSP control mapper instead of calling `FileSelectRuntime` directly. Its public DPST fixture drives the non-commercial startup state machine to file selection, then selection/confirmation reaches `NewGameTransition` and exposes the selected gameplay handoff context.

GitHub Actions run `31631766479` on commit `5d3cbfe5a928e572a72fda0d0559ca4ab69baabd` passed:

- save host semantics;
- startup/save integration semantics;
- PSP control semantics;
- pinned PSPDEV Allegrex cross-build including `startup_save_flow.cpp` and `psp_controls.cpp`;
- real PPSSPP 1.20.4 boot of the generated EBOOT;
- framebuffer capture and artifact publication.

Development EBOOT SHA-256: `264a15635c56017b794eeea990ad00715b4db88b863f9a23f3775bff4efdeed0`.

Host markers:

- `STARTUP_SAVE_FLOW_HOST_OK`
- `STARTUP_SAVE_INTEGRATION_HOST_OK flow=intro-title-file-select-transition-gameplay new_game=F_SP108/R01/start21 immediate_persist=true continue_context=true gameplay_checkpoint=true`
- `PSP_CONTROLS_HOST_OK analog=deadzone+normalized move=stick action=cross camera=L/R zoom=triangle/square pause=start cancel=circle menu=dpad debug=select edges=debounced`

This is real PSP/PPSSPP executable proof of the public startup/save/control boundary. It is **not** proof of the complete original asset-backed intro/title/file-select/F_SP108 route.

## Current preserved proof

- `test/startup-save-host/` — persistent slot semantics;
- `test/startup-save-integration-host/` — startup -> save -> transition -> gameplay handoff and recreation;
- `test/psp-controls-host/` — PSP control mapping and edge semantics;
- `test/startup-save-psp/` — PSP executable using `StartupSaveFlow` and the control mapper;
- `test/getawait-heart-probe/` — latest asset-backed item-presentation proof.

Commercial game data is intentionally not versioned.

## Active task

Close the first public-release path in the canonical EBOOT:

`intro/opening -> title -> file select -> create/load persistent slot -> NewGameTransition -> F_SP108 first playable -> PSP controls`

The public save/control boundary is now implemented and PPSSPP-booted. The next required proof is the canonical asset-backed executable using the same `StartupSaveFlow` handoff and PSP mapper with real startup packages and the first playable runtime.

A reconstructability issue remains: `scripts/build-canonical-existing-assets.sh` references the historical `test/dusklight-psp` target, which is absent from the current public overlay. Do not invent that executable from memory; recover its implementation/provenance or rebuild the canonical entry point from preserved source/runtime contracts with explicit validation.

## Explicitly not closed

- complete asset-backed canonical intro/title/save/F_SP108 one-EBOOT run;
- first-playable control acceptance in the actual F_SP108 gameplay runtime;
- full inventory/pause UI fidelity;
- all chest sizes/items and full item message integration;
- source item BCK/BRK/TEV visual fidelity;
- broad NPC/enemy/boss, dungeon/map and cinematic coverage;
- advanced lighting/post-processing parity.
