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
7. `/test/startup-save-integration-host/main.cpp`
8. `/test/getawait-heart-probe/main.cpp`

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

## 3. Current public save/startup sanity

Run the two host layers first:

```sh
cmake -S test/startup-save-host -B .work/build/startup-save-host -G Ninja -DCMAKE_BUILD_TYPE=RelWithDebInfo
cmake --build .work/build/startup-save-host -j 4
./.work/build/startup-save-host/startup_save_host_test

cmake -S test/startup-save-integration-host -B .work/build/startup-save-integration-host -G Ninja -DCMAKE_BUILD_TYPE=RelWithDebInfo
cmake --build .work/build/startup-save-integration-host -j 4
./.work/build/startup-save-integration-host/startup_save_integration_host_test
```

Expected markers:

- `STARTUP_SAVE_FLOW_HOST_OK`
- `STARTUP_SAVE_INTEGRATION_HOST_OK`

The integration marker must report the route `intro-title-file-select-transition-gameplay`, immediate new-game persistence and continuation from the recreated save context.

Build the public PSP save/startup executable with:

```sh
psp-cmake -S test/startup-save-psp -B .work/build/startup-save-psp -G Ninja -DCMAKE_BUILD_TYPE=RelWithDebInfo
cmake --build .work/build/startup-save-psp -j 4
```

GitHub Actions run `31630808657` on commit `a0fb49ab3f3e8587385160a95f5ef3b02e8a19db` passed both host tests, the Allegrex build and a real PPSSPP 1.20.4 boot. Development EBOOT SHA-256: `d12c2edfafcfb09dde1651c9459df131f42524439af2a19b00d0da26ba8afb51`.

## 4. Startup/save semantics that must not regress

- exactly three file slots;
- selected slot starts at 0;
- up/down clamp and do not wrap;
- Cross or START confirms on PSP;
- an empty slot creates the source-derived first-playable context `F_SP108`, room 1, start 21, layer 0;
- the new slot is persisted before gameplay handoff;
- an occupied slot returns its persisted `stage/room/start/layer` context;
- persistence uses the fixed `DPSV` v1 bank plus CRC32;
- corrupt/read-failed saves fail closed;
- failed writes restore the in-memory pre-write bank and enter the error state;
- gameplay handoff is not consumable until the startup runtime has crossed `NewGameTransition`;
- after handoff, saving updates the selected slot and survives object destruction/recreation.

## 5. Next implementation target — release path

Wire `StartupSaveFlow` into the canonical asset-backed startup executable rather than leaving the public `test/startup-save-psp` probe as the only PSP consumer.

Required end-to-end route:

`intro/opening -> title -> file select -> create/load slot -> NewGameTransition -> F_SP108 first playable -> PSP controls`

The historical canonical startup harness already expected `NEW_GAME_TRANSITION.OK=DUSKLIGHT_PSP_F_SP108_FIRST_PLAYABLE_OK` and `source_stage=F_SP108`. Preserve that boundary, but make the selected persistent `StartContext` the authority for the gameplay handoff.

Full validation needs the legally supplied startup/game-derived packages. Do not fabricate those packages or convert a synthetic host fixture into a claim of asset-backed startup parity.

## 6. Asset contract for GETAWAIT Heart Piece proof

The chest/item-get harness still expects local, untracked derived packages:

- Link DPSK/DPTX;
- DPAN containing `GETAWAIT` resource `0x16A`, `GETA` `0x169` and BOXOP for full integration;
- R02 DPRM/DPTX/DPSC;
- Heart Piece DPRM/DPTX derived from `O_gD_hutk/o_gd_hutk.bmd`;
- HUD DPUI.

Known source identities:

- `AlAnm.arc` / GETA resource `0x169`, 30 frames, 35 joints;
- `AlAnm.arc` / GETAWAIT resource `0x16A`, 30 frames, 35 joints;
- `O_gD_hutk.arc` / BMD resource `0x0008`, `o_gd_hutk.bmd`;
- Heart Piece item number `0x21`;
- TBOX name hash in the preserved R02 package: `0x2A0E83C6`.

Do not commit these extracted commercial files.

## 7. Chest PR merge gate

The source-owned Demo_Item commit is cross-built but the full asset-backed chest lifecycle has not yet been replayed after that semantic change. Keep its pull request draft/unmerged until one PPSSPP run proves:

- original chest visible closed;
- OPEN starts the source treasure event;
- source BOXOP/GETA/GETAWAIT run;
- Heart Piece visible in gameplay;
- inventory unchanged while merely visible;
- message acknowledgement calls `dead()` and source execution commits exactly once;
- chest state survives recreation;
- Link returns to clean locomotion;
- marker and real gameplay screenshot exist.

## 8. Release gate

Do not publish the first public EBOOT release until all are ready together:

- intro cinematic/startup route;
- main/title menu;
- persistent save creation/loading;
- beginning of the game in F_SP108;
- PSP-adapted controls sufficient for user testing.

Rendering polish remains secondary until gameplay completion.
