# Dusklight PSP — current status

Updated: 2026-08-12

## Project direction

Gameplay completeness is the dominant priority. The target is end-to-end completion of the game with original mechanics and event behavior. PSP graphics remain intentionally conservative until gameplay systems are substantially complete.

## Gameplay P1 advances

Preserved gameplay work includes source-derived room handoff and locomotion, original `DOOR20` sequencing, source chest/TBOX flow, deferred Demo_Item acquisition semantics, GETA/GETAWAIT resources, Heart Piece source identity/placement and a PPSSPP GETAWAIT visibility proof.

PR #2 (`Make Demo_Item commit source-owned`) remains draft. Its source-owned `dead() -> normal execute -> execItemGet()` semantics compile, but the changed lifecycle still requires an asset-backed PPSSPP replay through item-get, persistence and clean return to locomotion before merge.

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

## Reconstructed canonical PSP target

`test/dusklight-psp/` now exists again as an explicitly reconstructed canonical target. It is not presented as byte-for-byte recovery of the historical launcher. It currently shares the validated startup/save/control entry while the asset-backed game driver is rebuilt from preserved runtime contracts.

The current canonical checkpoint is green on GitHub Actions run `31638286245`, commit `e202afc6bbe2d9afcb6e9f07e34729943142a97c`:

- save/startup/control host tests passed;
- canonical F_SP108 asset contract host test passed;
- pinned PSPDEV bootstrap passed;
- `platform.cpp` and `canonical_assets.cpp` compiled into the Allegrex target;
- `psp-objdump` confirmed `architecture: mips:allegrex`;
- the generated canonical EBOOT booted in pinned PPSSPP 1.20.4.

Canonical EBOOT SHA-256: `7a2f47b8130321829b506dc2f6c816aed81ec8cee1f2eacb13cfbc69f0d0793a`.

This remains executable/boot proof only, not asset-backed gameplay parity and not a release candidate.

## Canonical first-room asset contract

The save handoff now resolves the documented first playable context to the exact packaged paths produced by `scripts/build-dusklight-psp-assets.sh`:

- `data/stages/F_SP108/R01/room.dprm`
- `data/stages/F_SP108/R01/room.dptx`
- `data/stages/F_SP108/R01/room.dpcl`
- `data/stages/F_SP108/R01/room.dpsc`

`canonical_assets.cpp` accepts only `F_SP108 / room 1 / start 21 / layer 0` at this checkpoint and fails closed for unsupported contexts. The platform layer now provides validated relative game paths while rejecting absolute paths, `.`/`..` segments, backslashes and device separators.

Host validation marker:

`CANONICAL_ASSETS_HOST_OK stage=F_SP108 room=1 start=21 layer=0 paths=4 fail_closed=1`

The GitHub runner currently reports no Vulkan physical device, but with Mesa installed PPSSPP reliably falls back to OpenGL and reaches the canonical EBOOT boot marker. This is CI infrastructure behavior only and has no gameplay-parity meaning.

## Screenshot policy

Repository `screenshot/` is reserved for source-faithful game UI or visible asset-backed gameplay. Text-only diagnostics, synthetic/public probes, boot screens and technical framebuffer captures remain CI evidence only and must not be published as project screenshots.

## Active task

Close the first release path in the canonical EBOOT:

`intro/opening -> title -> file select -> create/load persistent slot -> NewGameTransition -> F_SP108 first playable -> PSP controls`

Immediate implementation target:

1. consume the `StartupSaveFlow` gameplay handoff;
2. resolve its canonical room packages;
3. load and validate DPRM/DPTX/DPCL/DPSC from the packaged game directory;
4. initialize `RealRoomRuntime` and spawn start point 21;
5. feed PSP-mapped input into the real room update loop;
6. keep rendering minimal/unlit while gameplay behavior is brought up.

Commercial-derived assets remain local and unversioned, so public CI can compile and prove fail-safe behavior but cannot claim asset-backed first-playable parity.

## Explicitly not closed

- complete asset-backed canonical intro/title/save/F_SP108 one-EBOOT run;
- first-playable control acceptance in the actual F_SP108 gameplay runtime;
- full inventory/pause UI fidelity;
- all chest sizes/items and full item message integration;
- source item BCK/BRK/TEV visual fidelity;
- broad NPC/enemy/boss, dungeon/map and cinematic coverage;
- advanced lighting/post-processing parity.
