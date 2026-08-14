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

The real EBOOT has been exercised locally through:

`French warning -> startup logos/progressive -> F_SP102 opening -> title model`

The F_SP102 opening now loads all nine known environment layers. Two blockers remain before the startup integration PR can leave draft:

1. multi-texture/TEV materials can render as large white/brown flats because the current PSP material fallback collapses source material composition too aggressively;
2. the source title-logo model/animation loads but is displayed too small; recover the source/export transform contract rather than selecting an arbitrary visual scale.

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

## 8. Immediate next work

1. implement/validate the F_SP102 material-pass plan foundation without touching issue #6;
2. identify the exact affected F_SP102 source materials and classify each planned PSP representation;
3. rerun the real startup EBOOT and confirm the large flat material artifacts are gone;
4. recover/fix the title-logo transform/scale contract;
5. replay `intro/opening -> title -> file select -> save -> F_SP108` in one EBOOT;
6. add only source-faithful visual proof;
7. then evaluate the startup branch for merge/release readiness.

Performance work after that should follow the independent issues created from report 202. Each optimization must show a measurable bottleneck and preserve behavioral/render equivalence.

## 9. Release gate

Do not publish the first public EBOOT release until all are ready together:

- intro cinematic/startup route;
- main/title menu;
- persistent save creation/loading;
- beginning of the game in F_SP108;
- PSP-adapted controls sufficient for user testing.

The release-ready EBOOT must be published as a GitHub Release asset, not merely as a CI ZIP.
