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
23. `/docs/reports/205-psp-pc-fidelity-startup-checkpoint.md`
24. `/dusklight-main/platforms/psp/include/dusk/psp/room_package.hpp`
25. `/tools/fsp102_environment_export.py`
26. `/tools/startup_sequence_export.py`

Do not infer that a historical report's Git SHA exists in the reconstructed repository. The historical ledger distinguishes provenance from reconstructed commits.

## 2. Asset safety

Bring your own legally obtained Twilight Princess game image when original assets must be regenerated. Never commit extracted or derived commercial game packages.

An authorized workspace asset bundle was successfully located on 2026-08-13 and used locally for the first-playable proof. It contains the canonical F_SP108/Link/HUD packages and packaged startup assets. The assets themselves were not uploaded to GitHub.

## 3. Latest public checkpoint

Latest fully green canonical public proof before the current fidelity branch: GitHub Actions run `31679685111`, commit `3318caf88e8ec2cdf84cebc54408dac5fc01cdea`.

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

The authorized workspace also contains packaged startup data. Never upload the commercial-derived package bytes to GitHub.

## 7. Rendering profile history

The first-playable route was deliberately conservative while gameplay coverage was incomplete:

- `presentation::Profile::OpaqueOnly`;
- `RenderProfile::KnownGoodUnlit`;
- lighting off;
- fog off;
- shadows off.

The current fidelity branch does not claim those old conservative settings are visually sufficient. Visual work should now improve source fidelity without regressing the proven gameplay/save/control route.

## 7a. Previous F_SP102 startup environment checkpoint

PR #12 (`Render complete F_SP102 startup environment`) is the base intro branch for the current fidelity work. It keeps commercial-derived output local and adds deterministic exporter/bootstrap/build tooling plus the runtime asset-path contract.

Previous validated export metrics were 24,263 vertices, 21,513 triangles, 46 submeshes, 46 materials, 34 textures and 565,888 texture bytes. Public checkpoint run `31776948473` on `f5dac2ed771b5979a123a80b9ee2b37e6c18e495` was green with EBOOT SHA-256 `5ce0e73926752643670ab0a25b3d0fc872772883d365aaeeb33180061df1fb96`.

## 7b. Current PSP PC-fidelity/startup checkpoint

Work is on `agent/psp-pc-fidelity-startup`, stacked on the F_SP102 branch.

Key code checkpoint commit: `a39a280fb1876f0995d357a53a63ff70eaee5f98` (`Improve PSP startup and material fidelity`).

Rendering changes:

- DPTX v3 adds `MPV1` bounded material plans;
- maximum two PSP GU passes per material;
- per-pass texture identity, texture effect, blend policy and depth-write state;
- explicit Exact/Approximate/Unsupported fidelity status rather than silent TEV equivalence;
- source Item3D title camera replaces the arbitrary 3000-unit billboard placement;
- title camera uses FOV 45, eye `(0,0,-1000)`, model translation `(0,0,-430)` and mirrored X.

Recovered F_SP102 export checkpoint:

- 24,348 vertices;
- 21,513 triangles;
- 46 submeshes/materials;
- 40 textures;
- 656,128 texture bytes;
- 48 planned passes;
- 2 exact, 41 approximate, 3 unsupported materials.

Startup is intentionally reduced to:

`Dusklight team logo -> F_SP102 title -> START -> file select/save -> gameplay`

The new exporter does not emit warning, Nintendo, Dolby, progressive or realtime-opening replay segments. The pre-title team card is port-owned/generated and does not require commercial logo assets.

PSP controls remain the existing native mapping documented in section 5.

Validation run `31819052956` passed MPV1 host tests, reduced-startup host/export tests, pinned PSPDEV/PPSSPP bootstrap and the full Allegrex link. EBOOT SHA-256:

`9f3ec5f9a937c694ae1e3b4be1a37468037652c4e54b86f8c95d9f9278345eca`

Proof artifact ID: `9226218542`.

Important: this is a compile/contract checkpoint, not a new asset-backed visual acceptance. The current execution environment did not expose the Twilight Princess ISO or Dusklight PC runtime/assets. Do not claim PC parity from this run.

Still open:

- asset-backed F_SP102/title/F_SP108 replay against Dusklight PC captures;
- water, fog, far background and scene layers;
- UV/clamp/wrap and visible pass-order defects;
- alpha-test/blend/depth fixes driven by real foliage/water captures;
- source BPK/BRK/BTK material animation for title and affected scenes;
- improving the 41 approximate and 3 unsupported F_SP102 materials where they are visibly wrong.

Detailed report: `docs/reports/205-psp-pc-fidelity-startup-checkpoint.md`.

## 7c. Asset-backed reduced-startup replay

Local request `startup-title-fidelity-v12` validates the complete reduced route in
pinned PPSSPP 1.20.4 with OpenGL and the hardware PSP renderer:

`team logo -> F_SP102/title -> START -> file select -> F_SP108/R01/start21`

The request completed in 15,208 ms and produced six framebuffers, valid metrics and
`DUSKLIGHT_PSP_STARTUP_ROUTE_CAPTURE_OK`. Final EBOOT SHA-256:

`eb8d4412a674f18a0885ec659c681d1fc71c187ef637797b606c880bc6ad09e1`

The file-select screen now uses its source-derived DPSU panels/cursor. The startup
loader uses the actual `title_room` package names and no longer requires the
nonexistent `title_camera.dpcm`. The F_SP108 MPV1 water upgrade is applied by the
reproducible asset build.

Two bounded title experiments regressed the image (white rectangle, then missing
logo), so both were discarded. The final title is the recognizable baseline and still
needs source-backed TEV/UV/BPK/BRK/BTK work. Do not claim desktop parity.

Resume from `docs/reports/206-startup-route-file-select-ppsspp.md`, then obtain an
exact desktop capture for the same checkpoints before the next visual pass.

## 7d. Safe Link wrapped-lighting checkpoint

Resume from `docs/reports/207-safe-link-wrapped-lighting-ppsspp.md` for the latest
renderer state. `RenderProfile::CandidateGame` now selects
`SafeWrappedDiffuse`; `SourceApprox` remains diagnostic-only because its source
magnitudes made Link almost black.

Accepted constants are ambient `0.58`, key `0.32`, wrap bias `0.35` and minimum
illumination `0.52`. The renderer converts the F_SP108 world light into Link model
space with inverse yaw, uses already-normalized skinned normals and modulates the source
texture with a 64-level/27-material color LUT. PPSSPP first-frame cost was 4,339 us;
physical-PSP timing is open. The subtle rim variant cost 8,196 us and was rejected for
insufficient visible benefit.

Local A/B images and commercial-derived packages remain ignored. The complete reduced
route passed with team logo, title, file select and F_SP108 gameplay captures. MPV1,
alpha foliage, source fog and the accepted alpha water second pass were not changed.
No water animation, bloom or global composite is implemented yet.

## 8. Immediate next work

1. design an append-only, versioned and bounds-checked compact material-animation binding sourced from BTK;
2. use it for slow multi-direction F_SP108 water UV motion while preserving the accepted alpha second pass;
3. capture the same F_SP102/title/F_SP108 views in PSP and Dusklight PC and rank visible deltas;
4. fix the largest remaining title/material/depth/UV errors without exceeding two regular passes;
5. rerun actual PPSSPP control acceptance and physical-PSP lighting timing.

## 9. Heart Piece / chest track

PR #2 (`Make Demo_Item commit source-owned`) remains draft/unmerged. It may only be promoted after one asset-backed PPSSPP run proves the complete chest lifecycle after the source-owned commit: chest visible/closed, OPEN, BOXOP/GETA/GETAWAIT, Heart Piece visibility, inventory unchanged before acknowledgement, `dead()` followed by exactly one source `execItemGet()`, persistence/recreation and clean Link locomotion, with marker and real screenshot.

PR #4 (`Fix PSP import stub ordering`) was independently validated and merged into `agent/source-demo-item-commit` as `76e3dbedb56970283bfcee4225bf41cd4106836d`.

PR #5 (`Add BRK runtime plumbing for source item effects`) remains draft. Its compile/smoke proof is green, but it still lacks a dedicated functional BRK gate and asset-backed Heart Piece BRK/TEV rendering proof.

## 10. Release gate

Do not publish the first public EBOOT release until all are ready together:

- reduced startup route with the Dusklight team logo and source-faithful title;
- persistent save creation/loading;
- beginning of the game in F_SP108;
- PSP-adapted controls sufficient for user testing;
- an asset-backed visual pass confirming that the major material/alpha/water/background defects are acceptable for the target build.

Do not describe the current bounded material implementation as full TEV or PC rendering parity.
