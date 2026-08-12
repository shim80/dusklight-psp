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
8. `/test/startup-save-integration-host/main.cpp`
9. `/test/psp-controls-host/main.cpp`
10. `/test/dusklight-psp/README.md`
11. `/test/getawait-heart-probe/main.cpp`

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

cmake -S test/psp-controls-host -B .work/build/psp-controls-host -G Ninja -DCMAKE_BUILD_TYPE=RelWithDebInfo
cmake --build .work/build/psp-controls-host -j 4
./.work/build/psp-controls-host/psp_controls_host_test
```

Expected markers:

- `STARTUP_SAVE_FLOW_HOST_OK`
- `STARTUP_SAVE_INTEGRATION_HOST_OK`
- `PSP_CONTROLS_HOST_OK`

Latest fully green public proof before canonical-target reconstruction: GitHub Actions run `31631766479`, commit `5d3cbfe5a928e572a72fda0d0559ca4ab69baabd`. It cross-built and booted the PSP startup/save/control flow in PPSSPP 1.20.4. Development EBOOT SHA-256: `264a15635c56017b794eeea990ad00715b4db88b863f9a23f3775bff4efdeed0`.

The PSP probe itself now consumes `StartupSaveFlow` and maps real `SceCtrlData` through `psp_controls`; it no longer bypasses the startup/save boundary by calling `FileSelectRuntime` directly.

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

Historical scripts `scripts/build-canonical-existing-assets.sh` and `scripts/package-dusklight-psp.sh` expect a PSP project at `test/dusklight-psp` producing `dusklight_psp.elf` and `EBOOT.PBP`, but that historical launcher is absent from the reconstructed public tree.

`test/dusklight-psp` has therefore been restored as an explicitly **reconstructed build/package boundary**, not as a claim of exact historical recovery. At the current checkpoint it builds the same validated startup/save/control entry source as `test/startup-save-psp` so there is only one implementation of the release-path save/input semantics.

Build it with:

```sh
psp-cmake -S test/dusklight-psp -B .work/build/dusklight-psp -G Ninja -DCMAKE_BUILD_TYPE=RelWithDebInfo
cmake --build .work/build/dusklight-psp -j 4
$PSPDEV/bin/psp-objdump -f .work/build/dusklight-psp/dusklight_psp.elf
```

The active CI now gates both the public probe and this reconstructed canonical target and boots the canonical `EBOOT.PBP` in PPSSPP. Check the newest `PSP startup save flow` run before claiming this reconstruction is green.

## 6. Next implementation target — asset-backed release path

Replace the shared public probe entry beneath the reconstructed canonical target with the asset-backed game driver using preserved contracts rather than guessed historical source.

Required route:

`intro/opening -> title -> file select -> create/load slot -> NewGameTransition -> F_SP108 first playable -> PSP controls`

Preserve the historical boundary marker expectation `NEW_GAME_TRANSITION.OK=DUSKLIGHT_PSP_F_SP108_FIRST_PLAYABLE_OK` and `source_stage=F_SP108`, but make the selected persistent `StartContext` authoritative for the handoff.

`test/canonical-runtime/startup_first_playable_host_test.cpp` proves the existing F_SP108 runtime contract: model/textures/collision/scene packages validate, spawn 21 initializes `RealRoomRuntime`, and the expected first room contains 599 source actors with 9 essential actors instantiated by the current PSP actor system.

Full validation needs legally supplied startup/game-derived packages. Do not fabricate those packages or convert a synthetic fixture into a claim of asset-backed startup parity.

## 7. Asset contract for GETAWAIT Heart Piece proof

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

## 8. Pull-request gates

PR #2 (`Make Demo_Item commit source-owned`) remains draft/unmerged. It may only be promoted after one asset-backed PPSSPP run proves the complete chest lifecycle after the source-owned commit: chest visible/closed, OPEN, BOXOP/GETA/GETAWAIT, Heart Piece visibility, inventory unchanged before acknowledgement, `dead()` followed by exactly one source `execItemGet()`, persistence/recreation and clean Link locomotion, with marker and real screenshot.

The startup/save/control branch may only be promoted once the reconstructed canonical target is wired to the asset-backed intro/title/F_SP108 route and replayed in PPSSPP. Public synthetic-fixture proof is necessary but not sufficient.

## 9. Release gate

Do not publish the first public EBOOT release until all are ready together:

- intro cinematic/startup route;
- main/title menu;
- persistent save creation/loading;
- beginning of the game in F_SP108;
- PSP-adapted controls sufficient for user testing.

Rendering polish remains secondary until gameplay completion.
