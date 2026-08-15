# Exact resume protocol

This file is designed for a fresh agent/session with no conversational memory.

## 1. Read first

Read, in order:

1. `/AGENTS.md`
2. `/docs/STATUS.md`
3. `/docs/COMMIT_LEDGER.md`
4. `/docs/reports/202-daedalus-x64-psp-rendering-performance-research.md`
5. the newest remaining reports in `/docs/reports/`
6. `/dusklight-main/platforms/psp/include/dusk/psp/canonical_assets.hpp`
7. `/dusklight-main/platforms/psp/include/dusk/psp/canonical_startup_loader.hpp`
8. `/dusklight-main/platforms/psp/src/canonical_startup_entry.cpp`
9. `/dusklight-main/platforms/psp/src/playable_render.cpp`
10. `/dusklight-main/platforms/psp/include/dusk/psp/startup_save_flow.hpp`
11. `/dusklight-main/platforms/psp/src/startup_save_flow.cpp`
12. `/dusklight-main/platforms/psp/include/dusk/psp/psp_controls.hpp`
13. `/dusklight-main/platforms/psp/include/dusk/psp/first_playable_controls.hpp`
14. `/dusklight-main/platforms/psp/src/canonical_game.cpp`
15. `/scripts/build-dusklight-startup-assets.sh`
16. `/scripts/run-canonical-first-playable.sh`
17. relevant host tests for the subsystem being changed.

Do not infer that a historical report's Git SHA exists in the reconstructed repository. The historical ledger distinguishes provenance from reconstructed commits.

## 2. Asset safety

Bring your own legally obtained Twilight Princess game image when original assets must be regenerated. Never commit extracted or derived commercial game packages.

An authorized local asset workflow has been used to validate the first playable and the startup path. The assets themselves were not uploaded to GitHub.

## 3. Proven gameplay checkpoint

The canonical asset-backed route reaches visible F_SP108 gameplay at `F_SP108 / room 1 / start 21 / layer 0`. A real PPSSPP framebuffer is committed at:

`screenshot/dusklight-psp-f-sp108-gameplay.jpg`

The gameplay renderer remains intentionally conservative while broader gameplay coverage is incomplete.

## 4. Startup / title checkpoint

The current EBOOT was exercised locally in one isolated PPSSPP run through:

`French warning -> startup logos/progressive -> full source DPCM/F_SP102 opening -> title prompt -> START -> persistent file creation -> F_SP108 first frame`

The F_SP102 opening loads all nine known environment layers. The two known source-side defects now have candidate fixes:

1. DPTX v3 stores bounded, explicitly classified material plans and the runtime executes up to two GU passes;
2. the title actor uses the source Item3D view (FOV 45, eye Z -1000) and source model transform (Z -430, mirrored X), not a guessed billboard distance.

These fixes are Allegrex-compiled, host-tested and PPSSPP-replayed. The title exporter now preserves joint-local geometry and source blend classification; this removed the double-transform/opaque-rectangle defect. Remaining complex F_SP102 plans and title BPK/BRK/BTK material animation are still partial. Read `docs/reports/203-fsp102-material-pass-title-item3d-checkpoint.md` before changing them.

Do not commit a startup screenshot until these defects are corrected enough to satisfy the repository screenshot policy.

## 5. DaedalusX64 PSP research checkpoint

The PSP rendering/performance study is:

`docs/reports/202-daedalus-x64-psp-rendering-performance-research.md`

Reference repository/commit:

`DaedalusX64/daedalus@4f5c6fb045358044b64173fac619db5496cc2328`

The important adoption order is:

1. explicit bounded J3D/TEV -> PSP GU material pass plans, with `Exact / Approximate / Unsupported` classification;
2. instrumentation for texture residency/uploads, GU state calls, material passes, package I/O and GE waits;
3. deterministic host-generated PSP-native/pre-swizzled textures;
4. reclaimable EDRAM texture residency with explicit pin/lifetime policy and measured RAM fallback;
5. applied GU-state cache;
6. source-informed package prefetch;
7. only after profiling: double command lists and narrow VFPU kernels;
8. Media Engine only as an optional job queue with a CPU fallback, preferably audio/pure-data first.

Do not copy Daedalus implementation code casually. Reimplement techniques against Dusklight interfaces and review attribution/license compatibility if code is ever intentionally adapted.

Media Engine availability must never be required for gameplay, save/event logic or render correctness. PPSSPP can validate plumbing, but real PSP hardware is required before claiming an ME or VFPU performance gain dependent on hardware behavior.

## 6. Startup/save/control semantics that must not regress

- exactly three save slots;
- up/down clamp and do not wrap;
- Cross or START confirms on PSP;
- empty slot creates `F_SP108`, room 1, start 21, layer 0 and persists before handoff;
- occupied slot restores exact persisted `stage/room/start/layer` and metadata;
- persistence is `DPSV` v1 + CRC32 and fails closed;
- analog movement has a centered deadzone and normalized output;
- Cross=action, L/R=camera, Triangle/Square=zoom, START=pause, Circle=cancel, D-pad=menu, SELECT=debug;
- one-shot controls are edge-triggered.

## 7. Rendering rules

Keep the gameplay path conservative:

- `presentation::Profile::OpaqueOnly`;
- `RenderProfile::KnownGoodUnlit`;
- lighting off;
- fog off;
- shadows off.

Material-pass work may improve source texture composition where needed for correctness, but do not turn this checkpoint into general graphical polish.

Opaque submissions may only be reordered where existing order/depth tests prove invariance. Alpha-test and alpha-blend ordering remains source-derived unless separately proven equivalent.

**Do not work on issue #6.** That F_SP108 alpha-tested foliage regression is separately owned.

## 7a. Current startup cinematic checkpoint

PR #12 (`Render complete F_SP102 startup environment`) is the current intro branch. It keeps commercial-derived output local and adds only the deterministic exporter/bootstrap/build tooling plus the runtime asset-path contract.

Current validated local export metrics: 24,348 vertices, 21,513 triangles, 46 submeshes/materials, 40 textures, 656,128 texture bytes and 48 passes. Classification is 2 exact, 41 approximate and 3 unsupported. DPRM SHA-256 `7af40bc7d9957c29d5bef2b9bca8ec11b0d89270a2118177354ab96dd8368b3c`; DPTX SHA-256 `066cf74114945a6b6c35f4fb2a900f26b4dec8acc5bb6a6ae30d621ab20e37a5`.

Public checkpoint run `31776948473` on `f5dac2ed771b5979a123a80b9ee2b37e6c18e495` is fully green. Exact EBOOT SHA-256: `5ce0e73926752643670ab0a25b3d0fc872772883d365aaeeb33180061df1fb96`.

The branch expects `data/startup/fsp102_environment.dprm/.dptx` under the source `demo38_01` DPCM camera. The exact local camera is generated from source STB SHA-256 `e335d6d44c002dd25881aedd2f053a226be18cdd254d2049e0d78f2aa88b735d`: 30 Hz, 2,400 ticks, 2,401 samples, DPCM SHA-256 `ae4630366f6c6599813674b6c79929fcec0ad2d6b747ea9a4eb1f8dd68be438f`. The current local Allegrex EBOOT SHA-256 is `ca75544bea82b549d06ae4a8232cbeac67d7f31efc4f517bb45dfd437b58b6ff`. That exact EBOOT completed the full startup/save/gameplay route in pinned PPSSPP v1.20.4. Keep PR #12 draft until the remaining explicitly partial title/material surfaces are corrected enough for repository screenshot acceptance.

## 8. Immediate next work

1. port only the source title BPK/BRK/BTK state required to reproduce the final title composition;
2. refine only visibly wrong approximate/unsupported F_SP102 plans without touching issue #6;
3. replay `intro/opening -> title -> file select -> save -> F_SP108` in one EBOOT after each correction;
4. preserve the strict opt-in automation request and normal physical-input behavior;
5. add only source-faithful visual proof;
6. then evaluate the startup branch for merge/release readiness.

Performance work after that should follow the independent issues created from report 202. Each optimization must show a measurable bottleneck and preserve behavioral/render equivalence.

## 9. Release gate

Do not publish the first public EBOOT release until all are ready together:

- intro cinematic/startup route;
- main/title menu;
- persistent save creation/loading;
- beginning of the game in F_SP108;
- PSP-adapted controls sufficient for user testing.

The release-ready EBOOT must be published as a GitHub Release asset, not merely as a CI ZIP.
