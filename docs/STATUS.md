# Dusklight PSP — current status

Updated: 2026-08-13

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

## Latest canonical public checkpoint

`test/dusklight-psp/` is an explicitly reconstructed canonical target, not a byte-for-byte recovery of the lost historical launcher. Its minimal PSP bootstrap calls `dusk::psp::game::run_canonical_game()`.

Latest fully green public proof: GitHub Actions run `31677276452`, commit `7e01a72a51760c118a86ed0ea4d1fcff5b9e518e`.

That run proves:

- syntax validity of the canonical asset-backed helper scripts;
- save/startup/control host semantics;
- canonical F_SP108 asset-path contract;
- strict eight-file first-playable asset preflight;
- fail-closed preflight when `room.dpsc` is removed;
- pinned PSPDEV/PPSSPP bootstrap;
- canonical driver + Link runtime + room/collision/movebg + PSP playable renderer compile and final link for Allegrex;
- `psp-objdump` reports `architecture: mips:allegrex`;
- the generated canonical EBOOT boots in pinned PPSSPP 1.20.4;
- public PPSSPP does not emit an asset-backed gameplay proof marker when commercial-derived assets are absent.

Canonical EBOOT SHA-256: `59a6be284bca810f8d22fbf8b65f7eeb87f8b82b2805d4059040cc5894677b9c`.

CI artifact ID: `9172054577`.

Relevant guards:

- `DUSKLIGHT_PSP_FIRST_PLAYABLE_ASSETS_OK files=8 stage=F_SP108 room=1 start=21 layer=0`
- `DUSKLIGHT_PSP_FIRST_PLAYABLE_ASSETS_FAIL_CLOSED_OK`
- `DUSKLIGHT_PSP_PUBLIC_BOOT_NO_ASSET_PROOF_OK`

The public diagnostic framebuffer remains technical boot evidence only. It is not a gameplay screenshot.

## Canonical first-room handoff

The selected persistent `StartContext` is consumed by the canonical driver. The currently supported first-playable contract is exactly `F_SP108 / room 1 / start 21 / layer 0`, resolving to:

- `data/stages/F_SP108/R01/room.dprm`
- `data/stages/F_SP108/R01/room.dptx`
- `data/stages/F_SP108/R01/room.dpcl`
- `data/stages/F_SP108/R01/room.dpsc`

Global Link/HUD resources resolve to:

- `data/common/link.dpsk`
- `data/common/link.dptx`
- `data/common/link.dpan`
- `data/common/hud.dpui`

The canonical loaders measure each file, allocate exact-sized owned storage, validate the packages and retain the buffers for the lifetime of their runtime consumers.

Before entering gameplay, `run_canonical_game()` requires:

- 599 source scene actors;
- start point 21 present;
- successful `RealRoomRuntime` initialization and spawn;
- 9 essential source actors instantiated;
- 9 active actors and 9 create calls;
- consistent room and actor runtime state;
- valid Link source-animation/skin update;
- successful PSP room+Link frame submission.

Once these checks pass, PSP-mapped input drives `update_real_room()` and the actor-system update loop at 30 Hz.

## Rendering and proof protocol

The canonical driver uses the existing PSP playable renderer with the deliberately conservative profile:

- `presentation::Profile::OpaqueOnly`;
- `RenderProfile::KnownGoodUnlit`;
- lighting off;
- fog off;
- shadows off.

A one-shot runtime proof is emitted only after the first successful asset-backed gameplay frame:

`DUSKLIGHT_PSP_FIRST_PLAYABLE_RENDER_OK stage=F_SP108 room=1 start=21 actors=9 render=opaque_unlit frame=1`

The public workflow explicitly fails if that marker ever appears without local game assets, preventing a boot-only run from being misreported as gameplay proof.

The authorized local path is now one command when the derived assets are present under `build/assets/dusklight-psp/data`:

```sh
bash scripts/run-canonical-first-playable.sh
```

The runner preflights the eight files, builds/packages the EBOOT, launches PPSSPP from the packaged game directory, captures `first-playable-ppsspp.log`, and fails after PPSSPP exits if the first-render marker was never emitted.

A marker alone is not visual proof. Acceptance still requires a real asset-backed framebuffer showing F_SP108 + Link. No usable proprietary F_SP108/Link asset bundle is available in the current execution environment, so this final visual/runtime proof must be produced in an authorized local environment.

## Screenshot policy

Repository `screenshot/` is reserved for source-faithful game UI or visible asset-backed gameplay. Text-only diagnostics, synthetic/public probes, boot screens and technical framebuffer captures remain CI evidence only.

## Active task

Close the first release path in the canonical EBOOT:

`intro/opening -> title -> file select -> create/load persistent slot -> NewGameTransition -> F_SP108 first playable -> PSP controls`

Immediate gameplay-first target:

1. instrument deterministic first-playable PSP control acceptance after the first rendered frame without claiming success from synthetic/public CI;
2. validate the tracker semantics on the host and its integration in the Allegrex EBOOT;
3. run `scripts/run-canonical-first-playable.sh` in an authorized asset-backed environment and capture the first-render proof plus real gameplay framebuffer;
4. prove movement, camera, action and pause/resume in actual F_SP108;
5. only after that replace the synthetic/public startup presentation with packaged source-faithful startup assets.

## Explicitly not closed

- complete asset-backed canonical intro/title/save/F_SP108 one-EBOOT run;
- visible F_SP108 gameplay rendering proof in the canonical driver;
- first-playable control acceptance in the actual F_SP108 gameplay runtime;
- source-faithful packaged intro/title presentation in the canonical EBOOT;
- full inventory/pause UI fidelity;
- all chest sizes/items and full item message integration;
- source item BCK/BRK/TEV visual fidelity;
- broad NPC/enemy/boss, dungeon/map and cinematic coverage;
- advanced lighting/post-processing parity.
