# Dusklight PSP — current status

Updated: 2026-08-14

## Project direction

Gameplay completeness is the dominant priority. The target is end-to-end completion of the game with original mechanics and event behavior. PSP graphics remain intentionally conservative until gameplay systems are substantially complete.

## DaedalusX64 PSP research checkpoint — 2026-08-14

A PSP-specific performance/rendering study of `DaedalusX64/daedalus` is now recorded in `docs/reports/202-daedalus-x64-psp-rendering-performance-research.md`, anchored to Daedalus commit `4f5c6fb045358044b64173fac619db5496cc2328`.

The directly transferable architecture is:

- separate logical texture identity from physical PSP residency;
- generate PSP-native texture format and swizzled payloads on the host whenever assets are immutable;
- treat EDRAM as a reclaimable cache with explicit lifetimes rather than monotonic scene ownership;
- compile complex source texture/TEV expressions into bounded PSP GU pass plans instead of collapsing them to one guessed texture state;
- cache derived/applied GU state and invalidate it only when source state changes;
- keep filesystem/package streaming separate from texture residency and use source-known transition destinations for prefetch;
- benchmark double display-list submission and VFPU kernels only after profiling identifies a real bottleneck;
- keep Media Engine support optional, job-based and backed by a CPU fallback. Daedalus's concrete ME usage studied is asynchronous audio work; Dusklight render correctness and gameplay logic must never depend on ME availability.

The immediate runtime application is the current F_SP102 startup material defect: some multi-texture/TEV materials produce large white/brown flats despite correct geometry. The first performance-lane implementation should therefore be a source-derived bounded material-pass compiler plus instrumentation. This work must not touch issue #6, which remains separately owned.

No Daedalus source code or proprietary Nintendo-derived assets were added by this research checkpoint. The study records architecture and PSP techniques for original implementation in Dusklight.

## Startup / intro integration checkpoint

The complete canonical route now has a real, single-run PPSSPP proof through the French warning, startup logos/progressive presentation, all 2,400 source camera ticks over the nine-layer F_SP102 environment, the title prompt, automated START, persistent slot creation and the first rendered F_SP108 gameplay frame. The opt-in automation is activated only by the exact repository-local request `DUSKLIGHT_STARTUP_AUTOMATION_V1`; normal PSP input semantics are unchanged.

1. DPTX v3 now carries bounded, classified material-pass plans instead of collapsing every multi-texture/TEV material to one texture;
2. the title model now uses the original Item3D camera and model transform instead of an arbitrary camera-facing placement, and its exporter retains joint-local vertices instead of applying bind transforms twice.

The captured title is recognizable and no longer dominated by the former oversized opaque rectangles. F_SP102 still visibly exposes the declared approximate/unsupported material plans, and title BPK/BRK/BTK material animation is not yet ported. No partial-parity capture is accepted under `screenshot/`.

## Startup/save/control checkpoint

The persistent startup/save/control boundary includes:

- exactly three save slots with clamped, non-wrapping selection;
- Cross or START confirms a file;
- default new game is `F_SP108`, room 1, start point 21, layer 0;
- `DPSV` v1 persistence with CRC32 restores exact metadata and gameplay context;
- new files persist before gameplay handoff;
- read/write failures fail closed;
- recreation resumes the persisted context;
- PSP controls use analog movement, Cross action, L/R camera, Triangle/Square zoom, START pause, Circle cancel, D-pad menu navigation and SELECT debug;
- one-shot actions are edge-triggered.

## Authorized asset-backed first-playable proof

A real PPSSPP framebuffer visibly shows Link rendered inside F_SP108. The approved project capture is committed at:

`screenshot/dusklight-psp-f-sp108-gameplay.jpg`

The first-playable contract remains `F_SP108 / room 1 / start 21 / layer 0` with source scene actors and the canonical room/Link/HUD package path. Commercial-derived packages remain local and are never published to GitHub.

## Rendering profile

The canonical gameplay driver keeps the deliberately conservative profile while gameplay coverage remains incomplete:

- `presentation::Profile::OpaqueOnly`;
- `RenderProfile::KnownGoodUnlit`;
- lighting off;
- fog off;
- shadows off.

The Daedalus-derived material-pass work is a correctness extension to this conservative renderer, not a request to begin expensive visual polish.

## Screenshot policy

Repository `screenshot/` is reserved for source-faithful game UI or visible asset-backed gameplay. Text-only diagnostics, synthetic/public probes, boot screens and technical framebuffer captures remain CI evidence only.

## F_SP102 startup environment checkpoint

Draft PR #12 (`Render complete F_SP102 startup environment`) remains intentionally unmerged until asset-backed visual acceptance. Its final source changes have been reapplied on a clean branch based on current `main` for validation alongside the DaedalusX64 research checkpoint.

The source-safe exporter combines the five F_SP102 room BMDs and four stage/sky BMDs. Its new DPTX v3 contract stores at most two passes per source material with explicit `Exact / Approximate / Unsupported` fidelity and fail-closed validation. Two independent local exports are byte-identical: 24,348 vertices, 21,513 triangles, 46 submeshes/materials, 40 textures, 656,128 texture bytes and 48 passes. Classification is 2 exact, 41 approximate and 3 unsupported plans. The generated commercial-derived packages remain local.

Current local package hashes are DPRM `7af40bc7d9957c29d5bef2b9bca8ec11b0d89270a2118177354ab96dd8368b3c` and DPTX `066cf74114945a6b6c35f4fb2a900f26b4dec8acc5bb6a6ae30d621ab20e37a5`.

GitHub Actions run `31776948473` on commit `f5dac2ed771b5979a123a80b9ee2b37e6c18e495` is green for exporter/contract checks, pinned PSP toolchain bootstrap, Allegrex build, `mips:allegrex` verification and pinned PPSSPP smoke. Exact EBOOT SHA-256: `5ce0e73926752643670ab0a25b3d0fc872772883d365aaeeb33180061df1fb96`.

The canonical startup room paths now resolve to `data/startup/fsp102_environment.dprm` and `data/startup/fsp102_environment.dptx`. The progressive-scan startup segment also uses DPSU channel 3 instead of reusing the warning channel 0.

The current EBOOT compiles as `mips:allegrex` with SHA-256 `ca75544bea82b549d06ae4a8232cbeac67d7f31efc4f517bb45dfd437b58b6ff`. Host gates pass for material plans, title assets, canonical asset paths and startup/save integration. The source `demo38_01.stb` is evaluated directly into a deterministic 30 Hz, 2,400-tick/2,401-sample DPCM package (SHA-256 `ae4630366f6c6599813674b6c79929fcec0ad2d6b747ea9a4eb1f8dd68be438f`); the legacy checkpoints are oracle-only.

A direct Aqua launch of pinned PPSSPP v1.20.4 used an isolated repository-local profile and completed the full route. It produced a 278,528-byte title framebuffer, a 128-byte `DPSV` save, a 278,528-byte F_SP108 framebuffer and `DUSKLIGHT_PSP_STARTUP_SAVE_GAMEPLAY_OK`. The corrected deterministic title packages are DPRM `9bc1a7440c43d2c595a3c6ca50bdbabc54dde72ee1f87b0b41909f511f397689`, DPTX `ca144711f564c8c057e1651468f90dc9fd889f943068fc9016213f36aa34e977` and DPAN `3cdb38faa1711ed3352020ef341115ddc43c721b4a97da8cd87cf31bdc8ca537`. The broker remains unbootstrapped after launchd's earlier `EX_CONFIG`; the one-shot emulator process was terminated after validation. Details are in `docs/reports/203-fsp102-material-pass-title-item3d-checkpoint.md`.

## Active task

Close the first release path in the canonical EBOOT:

`intro/opening -> title -> file select -> create/load persistent slot -> NewGameTransition -> F_SP108 first playable -> PSP controls`

Immediate order:

1. port the bounded source title BPK/BRK/BTK material animation needed to close the remaining title-composition gap;
2. refine only the explicitly approximate/unsupported F_SP102 plans that remain visibly wrong;
3. replay the complete startup/title/save/F_SP108 route after each causal visual correction;
4. preserve existing source gameplay, save and control semantics;
5. introduce texture residency/performance optimizations only behind measurements and equivalence gates.

## Explicitly not closed

- source-faithful startup presentation beyond the now-proven one-EBOOT route, especially title BPK/BRK/BTK and the declared approximate/unsupported F_SP102 plans;
- full first-playable control acceptance for camera/action/pause/resume in actual F_SP108;
- full inventory/pause UI fidelity;
- all chest sizes/items and full item message integration;
- source item BCK/BRK/TEV visual fidelity;
- broad NPC/enemy/boss, dungeon/map and cinematic coverage;
- advanced lighting/post-processing parity;
- texture residency cache, VFPU acceleration or Media Engine acceleration (research only until measured and implemented).
