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
8. `/dusklight-main/platforms/psp/include/dusk/psp/first_playable_controls.hpp`
9. `/dusklight-main/platforms/psp/include/dusk/psp/canonical_assets.hpp`
10. `/dusklight-main/platforms/psp/include/dusk/psp/canonical_room_loader.hpp`
11. `/dusklight-main/platforms/psp/include/dusk/psp/canonical_common_loader.hpp`
12. `/dusklight-main/platforms/psp/src/canonical_game.cpp`
13. `/dusklight-main/platforms/psp/src/playable_render.cpp`
14. `/test/canonical-runtime/startup_first_playable_host_test.cpp`
15. `/test/startup-save-integration-host/main.cpp`
16. `/test/psp-controls-host/main.cpp`
17. `/test/first-playable-controls-host/main.cpp`
18. `/test/dusklight-psp/CMakeLists.txt`
19. `/test/dusklight-psp/README.md`
20. `/scripts/validate-canonical-first-playable-assets.sh`
21. `/scripts/run-canonical-first-playable.sh`
22. `/test/getawait-heart-probe/main.cpp`

Do not infer that a historical report's Git SHA exists in the reconstructed repository. The historical ledger distinguishes provenance from reconstructed commits.

## 2. Asset safety

Bring your own legally obtained Twilight Princess game image when original assets must be regenerated. Never commit extracted or derived commercial game packages.

An authorized workspace asset bundle was successfully located on 2026-08-13 and used locally for the first-playable proof. It contains the canonical F_SP108/Link/HUD packages and packaged startup assets. The assets themselves were not uploaded to GitHub.

## 3. Latest public checkpoint

Latest fully green canonical public proof: GitHub Actions run `31679685111`, commit `3318caf88e8ec2cdf84cebc54408dac5fc01cdea`.

Canonical EBOOT SHA-256: `9fce67ba95f3ff6c3be7ce674be324711ff03e17f1986b4a7993fe8f4129d5a2`.

Artifact ID: `9172983520`.

The run proves host save/startup/control semantics, first-playable control tracker semantics, canonical asset preflight, Allegrex compile/link, `mips:allegrex` ELF identity, PPSSPP boot and public fail-closed behavior without proprietary assets.

## 4. Asset-backed proof now achieved

Using the exact run-82 EBOOT with pinned PPSSPP 1.20.4 (AppImage SHA-256 `661c098e6b7f7610171a57b7c533ce8bba6f2312b71e76d61e850461973eba21`), the local asset-backed route now reaches visible F_SP108 gameplay.

Observed route:

`file select -> slot creation/persist -> NewGameTransition -> F_SP108/R01/start21 -> room/actor/Link initialization -> PSP gameplay renderer`

A real PPSSPP framebuffer shows Link inside F_SP108 and is committed at:

`screenshot/dusklight-psp-f-sp108-gameplay.jpg`

Analog input was exercised after the first visible frame and produced an observable Link orientation/locomotion change. This is actual asset-backed gameplay, not the synthetic startup probe.

The first local run exposed an old/new HUD-package contract mismatch. The preserved workspace `hud.dpui` is a valid compact DPUI v2 with original gameplay/pause sprites but without the later complete printable-ASCII glyph set. Commit `3318caf88e8ec2cdf84cebc54408dac5fc01cdea` adds a canonical-only validator that still checks package CRC/size/ranges/atlas and requires sprite IDs `0–3`, `10–20`, `30`, `40–43`; the general `validate_dpui()` remains strict.

The PSP runtime marker remains:

`DUSKLIGHT_PSP_FIRST_PLAYABLE_RENDER_OK stage=F_SP108 room=1 start=21 actors=9 render=opaque_unlit frame=1`

In the current local headless PPSSPP setup, the PSP platform `log()` stream is not mirrored into the host PPSSPP stdout. Do not invent a marker observation; the accepted proof is the actual framebuffer after all fail-closed initialization gates passed.

## 5. Startup/save/control semantics that must not regress

- exactly three save slots;
- up/down clamp and do not wrap;
- Cross or START confirms on PSP;
- empty slot creates `F_SP108`, room 1, start 21, layer 0 and persists before handoff;
- occupied slot restores exact persisted `stage/room/start/layer` and metadata;
- persistence is `DPSV` v1 + CRC32 and fails closed;
- analog movement has a centered deadzone and normalized output;
- Cross=action, L/R=camera, Triangle/Square=zoom, START=pause, Circle=cancel, D-pad=menu, SELECT=debug;
- one-shot controls are edge-triggered.

The first-playable control tracker counts effects, not raw input: movement requires displacement, camera requires runtime manual-camera state, action requires the source-prompt action counter to increment, and pause/resume require actual `Playing <-> Paused` transitions.

Full asset-backed control acceptance is not closed yet. Analog locomotion is visually proven; camera/action/pause/resume still need deterministic acceptance in actual F_SP108.

## 6. Canonical first-playable asset contract

At this checkpoint the gameplay handoff accepts exactly `F_SP108 / room 1 / start 21 / layer 0`.

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

The workspace also contains packaged startup data, including `data/startup/startup.dpst`, `title_ui.dpsu`, `file_select.dpsu`, startup logos and title model/texture/animation packages. These should be used next to replace the synthetic/public startup presentation.

## 7. Rendering profile

Keep the first-playable renderer deliberately conservative while gameplay is incomplete:

- `presentation::Profile::OpaqueOnly`;
- `RenderProfile::KnownGoodUnlit`;
- lighting off;
- fog off;
- shadows off.

Do not spend the next checkpoint on graphical polish.

## 8. Immediate next work

1. finish deterministic asset-backed control acceptance in F_SP108 for camera, source-prompt Cross action, START pause and resume;
2. keep the proven room+Link framebuffer path intact;
3. wire the packaged source-faithful startup assets into the canonical driver instead of the synthetic fixture;
4. replay `intro/opening -> title -> file select -> save -> F_SP108` in one EBOOT;
5. add only real source-faithful UI/gameplay screenshots to `screenshot/`;
6. then evaluate the startup branch for merge/release readiness.

## 9. Heart Piece / chest track

PR #2 (`Make Demo_Item commit source-owned`) remains draft/unmerged. It may only be promoted after one asset-backed PPSSPP run proves the complete chest lifecycle after the source-owned commit: chest visible/closed, OPEN, BOXOP/GETA/GETAWAIT, Heart Piece visibility, inventory unchanged before acknowledgement, `dead()` followed by exactly one source `execItemGet()`, persistence/recreation and clean Link locomotion, with marker and real screenshot.

PR #4 (`Fix PSP import stub ordering`) was independently validated and merged into `agent/source-demo-item-commit` as `76e3dbedb56970283bfcee4225bf41cd4106836d`.

PR #5 (`Add BRK runtime plumbing for source item effects`) remains draft. Its compile/smoke proof is green, but it still lacks a dedicated functional BRK gate and asset-backed Heart Piece BRK/TEV rendering proof.

## 10. Release gate

Do not publish the first public EBOOT release until all are ready together:

- intro cinematic/startup route;
- main/title menu;
- persistent save creation/loading;
- beginning of the game in F_SP108;
- PSP-adapted controls sufficient for user testing.

Rendering polish remains secondary until gameplay completion.
