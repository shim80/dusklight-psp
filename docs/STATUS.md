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

The startup route now has a real asset-backed PPSSPP proof through the French warning, startup logos/progressive presentation, the F_SP102 opening environment and the source title model. The F_SP102 environment uses all nine known room/sky layers, but two visual defects still keep PR #12 in draft:

1. some multi-texture/TEV materials render as large flat regions under the current simplified PSP material path;
2. the source title logo model/animation loads, but its world/display scale is still too small and requires recovery of the source/export transform contract.

No flawed F_SP102/title screenshot is accepted under `screenshot/` yet.

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

## Active task

Close the first release path in the canonical EBOOT:

`intro/opening -> title -> file select -> create/load persistent slot -> NewGameTransition -> F_SP108 first playable -> PSP controls`

Immediate order:

1. fix F_SP102 multi-texture/TEV material composition using explicit source-derived PSP pass plans;
2. recover the correct title-logo source/export transform scale;
3. replay the complete startup/title/save/F_SP108 route in one EBOOT;
4. preserve existing source gameplay, save and control semantics;
5. introduce texture residency/performance optimizations only behind measurements and equivalence gates.

## Explicitly not closed

- complete asset-backed canonical intro/title/save/F_SP108 one-EBOOT run with source-faithful startup presentation;
- full first-playable control acceptance for camera/action/pause/resume in actual F_SP108;
- full inventory/pause UI fidelity;
- all chest sizes/items and full item message integration;
- source item BCK/BRK/TEV visual fidelity;
- broad NPC/enemy/boss, dungeon/map and cinematic coverage;
- advanced lighting/post-processing parity;
- texture residency cache, VFPU acceleration or Media Engine acceleration (research only until measured and implemented).
