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

`test/dusklight-psp/` is an explicitly reconstructed canonical target, not a byte-for-byte recovery of the lost historical launcher. It now has its own minimal PSP bootstrap and calls `dusk::psp::game::run_canonical_game()` instead of sharing the synthetic probe entry.

The current canonical checkpoint is green on GitHub Actions run `31672752146`, commit `d7eebf2c5bb8be75d8c47c507dd5dea58ff1c213`:

- save/startup/control host tests passed;
- canonical F_SP108 asset-contract host test passed;
- pinned PSPDEV bootstrap passed;
- `canonical_game.cpp`, canonical room loading, `RealRoomRuntime`, collision and actor runtime compiled for Allegrex;
- `psp-objdump` confirmed `architecture: mips:allegrex`;
- the generated canonical EBOOT booted in pinned PPSSPP 1.20.4;
- CI artifact ID: `9170341638`.

Canonical EBOOT SHA-256: `bd7374ed26c054f8bd7b27191daa3b8d591682ad8e8af6c3182a0492f9fbd072`.

This public CI run proves compilation and boot of the real canonical driver. It does **not** prove execution of the asset-backed first room because commercial-derived game packages are intentionally absent from public CI.

## Canonical first-room handoff

The selected persistent `StartContext` is now consumed by the canonical driver. The currently supported first-playable contract is exactly `F_SP108 / room 1 / start 21 / layer 0`, resolving to:

- `data/stages/F_SP108/R01/room.dprm`
- `data/stages/F_SP108/R01/room.dptx`
- `data/stages/F_SP108/R01/room.dpcl`
- `data/stages/F_SP108/R01/room.dpsc`

The canonical loader measures each file, allocates exact-sized owned storage, validates DPRM/DPTX/DPCL/DPSC and keeps all four buffers alive for the lifetime of the room runtime.

Before entering gameplay, `run_canonical_game()` requires the preserved first-room invariants:

- 599 source scene actors;
- start point 21 present;
- successful `RealRoomRuntime` initialization and spawn;
- 9 essential source actors instantiated;
- 9 active actors and 9 create calls;
- consistent room and actor runtime state.

Once these checks pass, PSP-mapped input drives both `update_real_room()` and the actor-system update loop at 30 Hz. Any unsupported context, missing/corrupt package, failed spawn or inconsistent runtime fails closed and explicitly does not claim gameplay parity.

## Rendering checkpoint

The canonical driver currently uses debug-screen telemetry after a successful room handoff. This is **not** visible gameplay proof. The repository already contains the PSP render stack (`graphics.cpp`, `playable_render.cpp`, `actor_render.cpp`, `static_render_backend.cpp`, `static_render_bridge.cpp`), and the next implementation step is to reuse that existing pipeline for minimal/unlit F_SP108 rendering rather than invent a new renderer.

## Screenshot policy

Repository `screenshot/` is reserved for source-faithful game UI or visible asset-backed gameplay. Text-only diagnostics, synthetic/public probes, boot screens and technical framebuffer captures remain CI evidence only and must not be published as project screenshots.

## Active task

Close the first release path in the canonical EBOOT:

`intro/opening -> title -> file select -> create/load persistent slot -> NewGameTransition -> F_SP108 first playable -> PSP controls`

Immediate implementation target:

1. reuse the existing PSP graphics/playable/static render stack for minimal room rendering;
2. draw the loaded F_SP108 room and player using the canonical runtime state;
3. retain PSP-mapped movement/camera/action/pause updates;
4. execute one authorized asset-backed PPSSPP run of the complete handoff;
5. only then treat the first playable room as proven.

Commercial-derived assets remain local and unversioned, so public CI can compile and prove fail-safe behavior but cannot claim asset-backed first-playable parity.

## Explicitly not closed

- complete asset-backed canonical intro/title/save/F_SP108 one-EBOOT run;
- visible F_SP108 gameplay rendering in the canonical driver;
- first-playable control acceptance in the actual F_SP108 gameplay runtime;
- full inventory/pause UI fidelity;
- all chest sizes/items and full item message integration;
- source item BCK/BRK/TEV visual fidelity;
- broad NPC/enemy/boss, dungeon/map and cinematic coverage;
- advanced lighting/post-processing parity.
