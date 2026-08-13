# Exact resume protocol

This file is designed for a fresh agent/session with no conversational memory.

## 1. Read first

Read, in order:

1. `/AGENTS.md`
2. `/docs/STATUS.md`
3. `/docs/COMMIT_LEDGER.md`
4. the newest reports in `/docs/reports/`
5. `/dusklight-main/platforms/psp/include/dusk/psp/startup_save_flow.hpp`
6. `/dusklight-main/platforms/psp/src/startup_save_flow.cpp`
7. `/dusklight-main/platforms/psp/include/dusk/psp/psp_controls.hpp`
8. `/dusklight-main/platforms/psp/include/dusk/psp/canonical_assets.hpp`
9. `/dusklight-main/platforms/psp/include/dusk/psp/canonical_room_loader.hpp`
10. `/dusklight-main/platforms/psp/src/canonical_game.cpp`
11. `/test/canonical-runtime/startup_first_playable_host_test.cpp`
12. `/test/startup-save-integration-host/main.cpp`
13. `/test/psp-controls-host/main.cpp`
14. `/test/dusklight-psp/README.md`
15. `/test/getawait-heart-probe/main.cpp`

Do not infer that a historical report's Git SHA exists in the reconstructed repository. The historical ledger distinguishes provenance from reconstructed commits.

## 2. Required local dependencies

Bring your own legally obtained Twilight Princess game image when original assets must be regenerated. Never commit it.

For the pinned public Linux x86_64 toolchain/PPSSPP setup:

```sh
./scripts/bootstrap-repro.sh
export PSPDEV="$PWD/.tools/pspdev"
export PSPSDK="$PSPDEV/psp/sdk"
export PATH="$PSPDEV/bin:$PSPSDK/bin:$PATH"
```

## 3. Public release-path sanity

Run the host semantics first:

```sh
cmake -S test/startup-save-host -B .work/build/startup-save-host -G Ninja -DCMAKE_BUILD_TYPE=RelWithDebInfo
cmake --build .work/build/startup-save-host -j 4
./.work/build/startup-save-host/startup_save_host_test

cmake -S test/startup-save-integration-host -B .work/build/startup-save-integration-host -G Ninja -DCMAKE_BUILD_TYPE=RelWithDebInfo
cmake --build .work/build/startup-save-integration-host -j 4
./.work/build/startup-save-integration-host/startup_save_integration_host_test

cmake -S test/canonical-assets-host -B .work/build/canonical-assets-host -G Ninja -DCMAKE_BUILD_TYPE=RelWithDebInfo
cmake --build .work/build/canonical-assets-host -j 4
./.work/build/canonical-assets-host/canonical_assets_host_test

cmake -S test/psp-controls-host -B .work/build/psp-controls-host -G Ninja -DCMAKE_BUILD_TYPE=RelWithDebInfo
cmake --build .work/build/psp-controls-host -j 4
./.work/build/psp-controls-host/psp_controls_host_test
```

Expected markers:

- `STARTUP_SAVE_FLOW_HOST_OK`
- `STARTUP_SAVE_INTEGRATION_HOST_OK`
- `CANONICAL_ASSETS_HOST_OK`
- `PSP_CONTROLS_HOST_OK`

Latest fully green canonical public proof: GitHub Actions run `31672752146`, commit `d7eebf2c5bb8be75d8c47c507dd5dea58ff1c213`. It cross-builds the canonical gameplay driver for Allegrex and boots its EBOOT in PPSSPP 1.20.4. Canonical EBOOT SHA-256: `bd7374ed26c054f8bd7b27191daa3b8d591682ad8e8af6c3182a0492f9fbd072`. Artifact ID: `9170341638`.

This is compilation/boot proof. Public CI does not contain the derived game packages and therefore does not prove that F_SP108 was loaded or rendered.

The separate startup/save PSP probe remains synthetic/public and is kept only as a diagnostic boundary. The canonical target no longer shares its entry point.

## 4. Startup/save/control semantics that must not regress

- exactly three file slots;
- selected slot starts at 0;
- up/down clamp and do not wrap;
- Cross or START confirms on PSP;
- an empty slot creates `F_SP108`, room 1, start 21, layer 0;
- the new slot is persisted before gameplay handoff;
- an occupied slot returns its persisted `stage/room/start/layer` context;
- persistence uses fixed `DPSV` v1 plus CRC32 and exact metadata restoration;
- corrupt/read-failed saves fail closed;
- failed writes restore the in-memory pre-write bank and enter the error state;
- gameplay handoff is not consumable until `NewGameTransition` completes;
- after handoff, saving updates the selected slot and survives object destruction/recreation;
- analog movement has a PSP-centered deadzone and normalized output;
- Cross=action, L/R=camera, Triangle/Square=zoom, START=pause, Circle=cancel, D-pad=menu, SELECT=debug;
- one-shot controls are edge-triggered and cannot double-fire while held.

## 5. Reconstructed canonical target

Historical scripts `scripts/build-canonical-existing-assets.sh` and `scripts/package-dusklight-psp.sh` expect a PSP project at `test/dusklight-psp` producing `dusklight_psp.elf` and `EBOOT.PBP`, but the historical launcher is absent from the reconstructed public tree.

`test/dusklight-psp` is therefore an explicitly **reconstructed build/package boundary**, not a claim of exact historical recovery. It now has a small PSP bootstrap in `test/dusklight-psp/main.cpp` that calls `dusk::psp::game::run_canonical_game()`.

Build it with:

```sh
psp-cmake -S test/dusklight-psp -B .work/build/dusklight-psp -G Ninja -DCMAKE_BUILD_TYPE=RelWithDebInfo
cmake --build .work/build/dusklight-psp -j 4
$PSPDEV/bin/psp-objdump -f .work/build/dusklight-psp/dusklight_psp.elf
```

The canonical target currently compiles `canonical_game.cpp`, `canonical_room_loader.cpp`, `actor_runtime.cpp`, `real_room_runtime.cpp`, `room_collision.cpp`, `room_package.cpp`, startup/save/control sources and platform support.

## 6. Canonical first-playable handoff

Required release route:

`intro/opening -> title -> file select -> create/load slot -> NewGameTransition -> F_SP108 first playable -> PSP controls`

The selected persistent `StartContext` is authoritative. At the current checkpoint only `F_SP108 / room 1 / start 21 / layer 0` is accepted and resolves to:

- `data/stages/F_SP108/R01/room.dprm`
- `data/stages/F_SP108/R01/room.dptx`
- `data/stages/F_SP108/R01/room.dpcl`
- `data/stages/F_SP108/R01/room.dpsc`

`canonical_room_loader` measures each file, allocates exact-sized owned storage, validates DPRM/DPTX/DPCL/DPSC and retains those buffers for the lifetime of all `PackageView` consumers.

Before gameplay activation, `run_canonical_game()` requires the preserved historical first-playable contract:

- scene actor count 599;
- start point 21 exists;
- `RealRoomRuntime` initializes and spawns successfully;
- room state is consistent;
- actor system initializes with 9 essential source actors;
- active count and create-call count are both 9;
- actor-system state is consistent.

After activation, mapped PSP input drives `update_real_room()` and `update_actor_system()` at 30 Hz. Unsupported context, missing/corrupt packages, failed spawn or inconsistent state fails closed.

The startup presentation inside the current canonical driver is still a clearly marked synthetic/public fixture so public CI can boot without commercial-derived startup assets. Never describe that as source-faithful intro/title presentation.

## 7. Next implementation target — visible first playable

Replace the canonical driver's debug-screen gameplay telemetry with the already-existing PSP render stack, preserving minimal/unlit presentation while gameplay comes first.

Inspect and reuse rather than rewrite:

- `dusklight-main/platforms/psp/include/dusk/psp/graphics.hpp`
- `dusklight-main/platforms/psp/src/graphics.cpp`
- `dusklight-main/platforms/psp/src/playable_render.cpp`
- `dusklight-main/platforms/psp/src/actor_render.cpp`
- `dusklight-main/platforms/psp/src/static_render_backend.cpp`
- `dusklight-main/platforms/psp/src/static_render_bridge.cpp`

Determine the exact additional packaged Link/HUD/model paths from the current asset builder/manifest before modifying the canonical asset contract. Do not guess filenames.

Then perform one authorized asset-backed PPSSPP run proving the save handoff loads F_SP108, renders the room/player, accepts PSP controls and remains stable. Public synthetic-fixture proof is necessary but not sufficient.

## 8. Asset contract for GETAWAIT Heart Piece proof

The chest/item-get harness expects local, untracked derived packages:

- Link DPSK/DPTX;
- DPAN containing GETAWAIT `0x16A`, GETA `0x169` and BOXOP for full integration;
- R02 DPRM/DPTX/DPSC;
- Heart Piece DPRM/DPTX from `O_gD_hutk/o_gd_hutk.bmd`;
- HUD DPUI.

Known identities:

- `AlAnm.arc` / GETA `0x169`, 30 frames, 35 joints;
- `AlAnm.arc` / GETAWAIT `0x16A`, 30 frames, 35 joints;
- `O_gD_hutk.arc` / BMD `0x0008`, `o_gd_hutk.bmd`;
- Heart Piece item `0x21`;
- TBOX hash in preserved R02 package `0x2A0E83C6`.

Never commit extracted commercial files.

## 9. Pull-request gates

PR #2 (`Make Demo_Item commit source-owned`) remains draft/unmerged. It may only be promoted after one asset-backed PPSSPP run proves the complete chest lifecycle after the source-owned commit: chest visible/closed, OPEN, BOXOP/GETA/GETAWAIT, Heart Piece visibility, inventory unchanged before acknowledgement, `dead()` followed by exactly one source `execItemGet()`, persistence/recreation and clean Link locomotion, with marker and real screenshot.

The startup/save/control branch may only be promoted once the reconstructed canonical target is wired to the asset-backed intro/title/F_SP108 route and replayed in PPSSPP. Public synthetic-fixture proof is necessary but not sufficient.

## 10. Release gate

Do not publish the first public EBOOT release until all are ready together:

- intro cinematic/startup route;
- main/title menu;
- persistent save creation/loading;
- beginning of the game in F_SP108;
- PSP-adapted controls sufficient for user testing.

Rendering polish remains secondary until gameplay completion.
