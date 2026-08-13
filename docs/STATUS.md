# Dusklight PSP — current status

Updated: 2026-08-13

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

`test/dusklight-psp/` is an explicitly reconstructed canonical target, not a byte-for-byte recovery of the lost historical launcher. It has its own minimal PSP bootstrap and calls `dusk::psp::game::run_canonical_game()` instead of sharing the synthetic probe entry.

The current canonical renderer checkpoint is fully green on GitHub Actions run `31676401364`, commit `13aa52b4167bee6d987f80b3d609ae12f331d7ce`:

- save/startup/control host tests passed;
- canonical F_SP108 asset-contract host test passed;
- pinned PSPDEV bootstrap passed;
- the canonical driver, Link runtime, room/collision/movebg runtime and the existing PSP playable renderer all compiled and linked for Allegrex;
- `psp-objdump` confirmed `architecture: mips:allegrex`;
- the generated canonical EBOOT booted in pinned PPSSPP 1.20.4;
- CI artifact ID: `9171725278`.

Canonical EBOOT SHA-256: `6098a95f1b45d3b48cfa69740565e26bca460c193812fea390d28a03b02eed6d`.

This public CI run proves compilation, final link and PPSSPP boot of the canonical driver **with the room+Link renderer path present in the EBOOT**. It does **not** prove execution of the asset-backed first room because commercial-derived game packages are intentionally absent from public CI.

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

Before entering gameplay, `run_canonical_game()` requires the preserved first-room invariants:

- 599 source scene actors;
- start point 21 present;
- successful `RealRoomRuntime` initialization and spawn;
- 9 essential source actors instantiated;
- 9 active actors and 9 create calls;
- consistent room and actor runtime state.

Once these checks pass, PSP-mapped input drives `update_real_room()` and the actor-system update loop at 30 Hz. Any unsupported context, missing/corrupt package, failed spawn or inconsistent runtime fails closed and explicitly does not claim gameplay parity.

## Rendering checkpoint

The successful room handoff no longer uses debug-screen gameplay telemetry. The canonical driver now initializes the existing PSP playable renderer and Link animation runtime, feeds it the canonical room buffers and live `RealRoomRuntime` camera/player state, and submits a minimal gameplay frame using:

- `presentation::Profile::OpaqueOnly`;
- `RenderProfile::KnownGoodUnlit`;
- lighting off;
- fog off;
- shadows off.

The reconstructed standalone target mirrors the official PSP renderer source set and excludes the separate upstream-dependent static render bridge. `movebg_runtime.cpp` is linked because `shadow_runtime` references its receiver collection path even though shadows remain disabled in the current presentation profile.

Public CI has now proven that this renderer-containing EBOOT cross-compiles, links and boots. The missing proof is an authorized asset-backed PPSSPP run that actually crosses file select, loads F_SP108 and displays room+Link.

## Screenshot policy

Repository `screenshot/` is reserved for source-faithful game UI or visible asset-backed gameplay. Text-only diagnostics, synthetic/public probes, boot screens and technical framebuffer captures remain CI evidence only and must not be published as project screenshots.

## Active task

Close the first release path in the canonical EBOOT:

`intro/opening -> title -> file select -> create/load persistent slot -> NewGameTransition -> F_SP108 first playable -> PSP controls`

Immediate implementation target:

1. make the authorized local asset-backed build/run path deterministic and one-command;
2. execute that path with the eight canonical room/Link/HUD packages present;
3. prove visible F_SP108 room+Link rendering and PSP controls in PPSSPP;
4. then replace the synthetic/public startup presentation with the packaged source-faithful startup assets;
5. only then treat the first playable startup path as merge-ready.

Commercial-derived assets remain local and unversioned, so public CI can compile and prove fail-safe behavior but cannot claim asset-backed first-playable parity.

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
