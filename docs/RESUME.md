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
10. `/dusklight-main/platforms/psp/include/dusk/psp/canonical_common_loader.hpp`
11. `/dusklight-main/platforms/psp/src/canonical_game.cpp`
12. `/dusklight-main/platforms/psp/src/playable_render.cpp`
13. `/test/canonical-runtime/startup_first_playable_host_test.cpp`
14. `/test/startup-save-integration-host/main.cpp`
15. `/test/psp-controls-host/main.cpp`
16. `/test/dusklight-psp/CMakeLists.txt`
17. `/test/dusklight-psp/README.md`
18. `/scripts/validate-canonical-first-playable-assets.sh`
19. `/scripts/run-canonical-first-playable.sh`
20. `/test/getawait-heart-probe/main.cpp`

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

## 3. Latest public checkpoint

Latest fully green canonical public proof: GitHub Actions run `31677276452`, commit `7e01a72a51760c118a86ed0ea4d1fcff5b9e518e`.

Canonical EBOOT SHA-256: `59a6be284bca810f8d22fbf8b65f7eeb87f8b82b2805d4059040cc5894677b9c`.

Artifact ID: `9172054577`.

The run proves:

- shell syntax for the asset-backed preflight/build/run scripts;
- save/startup/integration/control host semantics;
- F_SP108 canonical asset-path contract;
- the exact eight-file first-playable preflight;
- fail-closed behavior when `room.dpsc` is missing;
- pinned PSPDEV/PPSSPP bootstrap;
- Allegrex compilation and final link of the canonical driver including Link runtime, room/collision/movebg runtime and PSP playable renderer;
- `architecture: mips:allegrex` from `psp-objdump`;
- PPSSPP boot of the canonical EBOOT;
- explicit absence of asset-backed first-render proof in public CI.

Expected public markers include:

- `STARTUP_SAVE_FLOW_HOST_OK`
- `STARTUP_SAVE_INTEGRATION_HOST_OK`
- `CANONICAL_ASSETS_HOST_OK`
- `PSP_CONTROLS_HOST_OK`
- `DUSKLIGHT_PSP_FIRST_PLAYABLE_ASSETS_OK files=8 stage=F_SP108 room=1 start=21 layer=0`
- `DUSKLIGHT_PSP_FIRST_PLAYABLE_ASSETS_FAIL_CLOSED_OK`
- `DUSKLIGHT_PSP_PUBLIC_BOOT_NO_ASSET_PROOF_OK`

This is compile/link/boot/guard proof. Public CI does not contain the derived game packages and therefore does not prove that F_SP108 or Link were actually loaded or rendered.

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

Historical scripts expect a PSP project at `test/dusklight-psp`, but the historical launcher is absent from the reconstructed public tree.

`test/dusklight-psp` is therefore an explicitly reconstructed build/package boundary, not a claim of exact historical recovery. `main.cpp` is a minimal PSP bootstrap that calls `dusk::psp::game::run_canonical_game()`.

The standalone target mirrors the official PSP renderer source set instead of linking the full platform runtime, because the latter also expects compatibility actors from the complete pinned upstream source tree. The separate upstream-dependent static render bridge is intentionally excluded.

Build it with:

```sh
psp-cmake -S test/dusklight-psp -B .work/build/dusklight-psp -G Ninja -DCMAKE_BUILD_TYPE=RelWithDebInfo
cmake --build .work/build/dusklight-psp -j 4
$PSPDEV/bin/psp-objdump -f .work/build/dusklight-psp/dusklight_psp.elf
```

## 6. Canonical first-playable handoff

Required release route:

`intro/opening -> title -> file select -> create/load slot -> NewGameTransition -> F_SP108 first playable -> PSP controls`

At the current checkpoint only `F_SP108 / room 1 / start 21 / layer 0` is accepted.

Room packages:

```text
data/stages/F_SP108/R01/room.dprm
data/stages/F_SP108/R01/room.dptx
data/stages/F_SP108/R01/room.dpcl
data/stages/F_SP108/R01/room.dpsc
```

Global playable packages:

```text
data/common/link.dpsk
data/common/link.dptx
data/common/link.dpan
data/common/hud.dpui
```

Before gameplay activation, `run_canonical_game()` requires:

- scene actor count 599;
- start point 21 exists;
- `RealRoomRuntime` initializes and spawns successfully;
- room state is consistent;
- actor system initializes with 9 essential source actors;
- active count and create-call count are both 9;
- Link source animation and skin update remains valid.

After activation, mapped PSP input drives `update_real_room()` and `update_actor_system()` at 30 Hz. Unsupported context, missing/corrupt packages, failed spawn or inconsistent state fails closed.

The startup presentation inside the current canonical driver is still a clearly marked synthetic/public fixture so public CI can boot without commercial-derived startup assets. Never describe that as source-faithful intro/title presentation.

## 7. Renderer and asset-backed proof

The canonical gameplay renderer uses:

- `presentation::Profile::OpaqueOnly`;
- `RenderProfile::KnownGoodUnlit`;
- lighting off;
- fog off;
- shadows off.

The canonical driver emits this one-shot marker only after the first successful real-room frame submission following valid room, actor and Link updates:

```text
DUSKLIGHT_PSP_FIRST_PLAYABLE_RENDER_OK stage=F_SP108 room=1 start=21 actors=9 render=opaque_unlit frame=1
```

Public CI is required to fail if that marker appears without local game assets.

With authorized derived assets present at `build/assets/dusklight-psp/data`, run exactly:

```sh
bash scripts/run-canonical-first-playable.sh
```

The runner:

1. bootstraps pinned tools only if needed;
2. validates the eight canonical files and manifest boundary;
3. builds and packages the canonical EBOOT;
4. launches PPSSPP from the packaged game directory;
5. records `first-playable-ppsspp.log`;
6. after PPSSPP exits, fails unless the first-render marker was observed.

The marker is necessary but not sufficient. Acceptance also requires a real asset-backed gameplay framebuffer showing F_SP108 + Link. No usable proprietary F_SP108/Link asset bundle is available in the current agent execution environment, so that proof must be produced in an authorized local environment.

Only a real asset-backed gameplay screenshot may be promoted to repository `screenshot/`.

## 8. Next gameplay-first implementation target

Do not spend the next checkpoint on lighting or visual polish.

Instrument deterministic first-playable PSP control acceptance after gameplay is active. The runtime proof must remain impossible to trigger from public boot-only CI and should cover source-visible effects of:

- analog movement;
- camera L/R input;
- Cross action edge;
- START pause and resume.

First inspect the exact `playable::Input` and `RealRoomRuntime` state transitions before implementing the tracker. Add host semantics for the tracker, compile it into the canonical Allegrex EBOOT, and reserve the final acceptance marker for an asset-backed run with real PSP-mapped input.

After F_SP108 rendering + controls are proven asset-backed, replace the synthetic/public startup presentation with the packaged source-faithful startup assets.

## 9. Heart Piece / chest track

PR #2 (`Make Demo_Item commit source-owned`) remains draft/unmerged. It may only be promoted after one asset-backed PPSSPP run proves the complete chest lifecycle after the source-owned commit: chest visible/closed, OPEN, BOXOP/GETA/GETAWAIT, Heart Piece visibility, inventory unchanged before acknowledgement, `dead()` followed by exactly one source `execItemGet()`, persistence/recreation and clean Link locomotion, with marker and real screenshot.

PR #4 (`Fix PSP import stub ordering`) was independently validated and merged into `agent/source-demo-item-commit` as `76e3dbedb56970283bfcee4225bf41cd4106836d`.

PR #5 (`Add BRK runtime plumbing for source item effects`) remains draft. Its current compile/smoke proof is green, but it is foundational only and does not yet have a dedicated functional BRK CI gate or asset-backed Heart Piece BRK/TEV rendering proof.

The chest/item-get harness expects local, untracked derived packages including Link DPSK/DPTX/DPAN, R02 room packages, Heart Piece DPRM/DPTX and HUD DPUI. Never commit extracted commercial files.

## 10. Release gate

Do not publish the first public EBOOT release until all are ready together:

- intro cinematic/startup route;
- main/title menu;
- persistent save creation/loading;
- beginning of the game in F_SP108;
- PSP-adapted controls sufficient for user testing.

Rendering polish remains secondary until gameplay completion.
