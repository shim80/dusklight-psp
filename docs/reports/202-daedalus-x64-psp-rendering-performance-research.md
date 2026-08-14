# 202 — DaedalusX64 PSP rendering and performance research

Date: 2026-08-14

## Purpose

This report studies the PSP backend of DaedalusX64 as an architectural reference for Dusklight PSP. The objective is not to transplant an N64 emulator renderer into Twilight Princess. The useful part is the accumulated PSP-specific engineering around texture representation, GU state, VRAM pressure, streaming, VFPU acceleration, command submission and optional Media Engine work.

The study is deliberately gameplay-first. Any optimization adopted by Dusklight must preserve source-derived game behavior and must not weaken the existing render/state validation gates. The current immediate visual problem — incomplete F_SP102 multi-texture/TEV material rendering — is a correctness issue and therefore may use the material-pass ideas below before the broader performance work.

## Reference provenance

Repository: `DaedalusX64/daedalus`

Reference branch: `master`

Reference commit: `4f5c6fb045358044b64173fac619db5496cc2328`

Daedalus files studied include:

- `Source/HLEGraphics/TextureCache.cpp`
- `Source/HLEGraphics/CachedTexture.cpp`
- `Source/SysPSP/Graphics/NativeTexturePSP.cpp`
- `Source/SysPSP/Graphics/VideoMemoryManager.cpp`
- `Source/Utility/MemoryHeap.cpp`
- `Source/RomFile/ROMBuffer.cpp`
- `Source/RomFile/RomFileCache.cpp`
- `Source/SysPSP/HLEGraphics/RendererPSP.cpp`
- `Source/SysPSP/HLEGraphics/Combiner/RenderSettings.cpp`
- `Source/HLEGraphics/RDPStateManager.cpp`
- `Source/SysPSP/Graphics/GraphicsContextPSP.cpp`
- `Source/SysPSP/HLEGraphics/ConvertVertices.S`
- `Source/SysPSP/HLEGraphics/TransformWithLighting.S`
- `Source/SysPSP/Utility/FastMemcpyPSP.cpp`
- `Source/SysPSP/Utility/CacheUtil.h`
- `Source/SysPSP/Utility/VolatileMemPSP.cpp`
- `Source/SysPSP/PRX/MediaEngine/main.c`
- `Source/SysPSP/PRX/MediaEngine/me.c`
- `Source/HLEAudio/Plugin/PSP/AudioPluginPSP.cpp`
- `Source/Utility/Profiler.cpp`
- `Options.cmake`

The analysis is based on behavior and architecture observed at that exact commit. Future Daedalus changes must not silently change the assumptions in this report.

## Licensing boundary

Daedalus source files are GPL v2-or-later. Dusklight PSP is GPLv3. This report records concepts, contracts and PSP techniques. Implementation in Dusklight should be written against our own interfaces and data formats. Do not paste Daedalus implementation code into Dusklight merely because a similar algorithm is desired. If code is ever intentionally adapted, preserve attribution and review license compatibility explicitly.

## Executive conclusions

The strongest lessons for Dusklight are not isolated micro-optimizations. They are separations of responsibility:

1. **logical texture identity must be separate from PSP residency**;
2. **PSP-native texture representation should be produced offline whenever possible**;
3. **EDRAM must be treated as a cache, not as a monotonic scene-owned blob**;
4. **complex source material logic should compile to a bounded GU pass plan** rather than collapse to one guessed texture state;
5. **derived state must be cached and invalidated only when its source changes**;
6. **GPU command submission, CPU scene preparation and I/O should be allowed to overlap** when profiling proves a stall;
7. **VFPU and Media Engine are optional accelerators with scalar/CPU fallbacks**, never correctness dependencies;
8. **every optimization needs counters and before/after frame evidence** so gains are measurable rather than anecdotal.

For the current F_SP102 intro regression, item 4 is the most directly useful: a material that uses multiple source texture/constant stages should become a small PSP pass program instead of being rendered as `primary texture + simplified alpha`, which is the likely source of the large white/brown flats currently visible.

---

## 1. Texture architecture

### 1.1 Logical cache versus native PSP texture

Daedalus keeps a generic cached-texture object separate from the PSP-native backing texture. Logical lookup, source invalidation and age tracking do not depend on the physical texture being in EDRAM.

That is a strong model for Dusklight. Today several PSP paths effectively assume that a loaded room texture package and its EDRAM residency are one lifetime. Instead, Dusklight should treat these as separate layers:

```text
source/package texture identity
        ↓
validated PSP texture record
        ↓
logical texture cache entry
        ↓
resident allocation (EDRAM or RAM)
        ↓
GU binding
```

Recommended stable key for immutable Dusklight packages:

```text
(package identity / package CRC, texture id, texture-record CRC or content hash)
```

Unlike an emulator reading mutable emulated RAM, our converted static packages do not need to recalculate texture checksums every frame. Package CRC plus immutable texture identity is enough for the common case.

### 1.2 Age-based eviction without frame spikes

Daedalus records the last-use frame and expires old texture entries. Its cleanup is staggered so all stale entries are not destroyed in one frame.

Dusklight adaptation:

- maintain `last_used_frame` per resident texture;
- keep a small pinned set for UI and currently critical actor textures;
- evict stale non-pinned textures by age/LRU;
- spread optional cleanup over multiple frames;
- on room transition, explicitly demote or release the previous room set instead of waiting for arbitrary heap pressure;
- record hits, misses, uploads, evictions and bytes moved.

This should reduce both EDRAM pressure and transition-time spikes.

### 1.3 PSP-native formats

Daedalus deliberately uses PSP-native pixel formats such as 5650, 5551, 4444 and 8888 depending on content requirements.

Dusklight host export should make the same class of decision from the source texture/material contract:

- **5650**: opaque color textures where alpha is not used;
- **5551**: binary alpha/cutout textures when one alpha bit is source-compatible;
- **4444**: translucent textures where 4-bit channels are visually acceptable;
- **8888**: reserve for cases where the source/fidelity test proves lower precision unacceptable;
- indexed formats can be evaluated later only if they reduce total cost after palette/state overhead is measured.

This is preferable to treating all textures as one generic representation.

### 1.4 Pre-swizzled data

Daedalus swizzles texture storage into a layout friendly to PSP texture fetch. Dusklight should move this work to the host exporter.

A DPTX vNext record can carry:

- PSP target format;
- logical width/height;
- stored width/height;
- mip count and offsets when applicable;
- swizzled flag/layout version;
- swizzled payload size;
- optional source precision/fallback classification.

Runtime responsibility should then be limited to validation, allocation, copy, cache maintenance and `sceGuTexImage`/state binding.

**Desired invariant:** no expensive color conversion or swizzle should occur on PSP during normal room entry for static converted assets.

### 1.5 Graceful size fallback

Daedalus can retry a native texture with reduced dimensions when allocation fails. Dusklight should not automatically downscale source assets at runtime because source fidelity is more controlled in our offline pipeline. However, the concept is useful as a build-time policy:

- assign texture importance classes;
- generate only approved dimensions in the host pipeline;
- fail closed if a required texture cannot fit its declared class;
- optionally provide an explicitly generated lower-resolution fallback record for known memory-pressure scenes.

This keeps runtime deterministic and avoids silently changing visuals based on heap fragmentation.

---

## 2. EDRAM, volatile memory and residency

### 2.1 EDRAM is a cache

Daedalus uses a reclaimable video-memory allocator rather than assuming one scene can permanently own the remaining EDRAM. It also falls back to a block of volatile RAM when video memory is exhausted.

Dusklight should introduce a `PspTextureResidency` layer with at least:

```text
allocate(size, alignment, priority)
release(handle)
touch(handle, frame)
pin(handle)
unpin(handle)
evict_until(bytes_needed)
resident_location(handle) -> EDRAM | RAM
```

Textures in normal RAM are still usable by the GU, although slower. That makes RAM a useful overflow tier rather than an immediate fatal error. The exact RAM pool size must be measured on our target models/build and should not be hard-coded from Daedalus.

### 2.2 Explicit reclaim instead of monotonic cursor growth

The current Dusklight renderer uses fixed EDRAM regions and sequential texture upload cursors. That is simple and appropriate for first proof, but it makes multi-scene/title/gameplay composition fragile.

A reclaimable allocator enables:

- title textures to disappear when gameplay begins;
- previous-room textures to be freed as the new room becomes authoritative;
- UI and Link textures to remain pinned;
- transient actor/item textures to have bounded lifetimes;
- shadow buffers to remain reserved without forcing every room texture resident simultaneously.

### 2.3 Keep command lists and transient CPU buffers out of valuable texture EDRAM

Daedalus uses volatile memory for large command-list buffers. Dusklight should benchmark the same principle: reserve EDRAM primarily for frame/depth buffers and high-value texture residency, while command lists, transient converted vertices and staging data live in aligned system/volatile memory where appropriate.

---

## 3. Source material / TEV to PSP GU pass plans

This is the highest-value near-term finding.

### 3.1 Why a single GU texture function is not enough

The PSP GU can directly express useful operations (`MODULATE`, `DECAL`, `BLEND`, `REPLACE`, `ADD`) but a GameCube J3D TEV material may combine multiple textures, constants, vertex colors and alpha rules.

Collapsing that to one texture and one `sceGuTexFunc` can produce catastrophically wrong output even when geometry, UVs and individual textures are correct. The F_SP102 large solid flats are consistent with this class of failure.

### 3.2 Daedalus strategy worth adapting

Daedalus does not assume every source combiner maps to one PSP state. It builds render settings, tracks texture-0/texture-1 dependencies and can execute more than one color pass. When multiple passes are needed it preserves working vertex data so each pass can apply its own color expression/state.

Dusklight equivalent:

```cpp
struct PspMaterialPass {
    uint16_t texture_id;
    TextureEffect tfx;      // replace/modulate/blend/add/decal
    TextureComponent tcc;   // RGB/RGBA
    uint32_t texture_factor;
    BlendState blend;
    AlphaState alpha;
    DepthState depth;
    CullState cull;
    VertexColorOp vertex_color;
};

struct PspMaterialPassPlan {
    uint8_t pass_count;
    PspMaterialPass pass[kMaxMaterialPasses];
    FidelityClass fidelity; // Exact / Approximate / Unsupported
    FallbackReason reason;
};
```

The host converter should compile the source J3D material into this bounded plan where possible. The PSP runtime should execute the plan, not rediscover TEV semantics every frame.

### 3.3 Exact / approximate / unsupported must be explicit

Daedalus tracks when a combiner representation is inexact. Dusklight should do the same and expose it in package/runtime metrics.

Rules:

- `Exact`: PSP pass plan is proven equivalent for the supported source expression;
- `Approximate`: intentionally simplified but visually bounded and documented;
- `Unsupported`: fail closed or use a named conservative fallback, never report parity.

For our current gameplay-first renderer, a named unlit approximation is acceptable where needed, but it must not accidentally turn into a giant solid polygon because the source requires a second texture/pass.

### 3.4 Draw-order constraints

Opaque passes may be batched/reordered only where existing depth/order tests prove invariance. Alpha-test and alpha-blend submissions keep source-derived ordering unless a dedicated equivalence test proves otherwise.

No performance optimization may weaken the current alpha regression work or interfere with issue #6 ownership.

---

## 4. GU state cache and submission cost

Daedalus uses dirty-state tracking heavily. Derived texture descriptors and renderer state are recomputed only after their source state changes.

Dusklight should introduce a small applied-state cache covering expensive/redundant calls such as:

- bound texture + texture format/size;
- `sceGuTexFunc` and texture env color;
- alpha-test enable/function/reference;
- blend enable/equation/factors;
- depth enable/function/write mask;
- cull enable/front face;
- lighting/fog toggles;
- projection/view change generation.

The renderer still emits complete, deterministic state when entering a pass boundary; the cache only removes calls proven redundant within the same well-defined context.

Metrics to add:

- `gu_state_change_requests`;
- `gu_state_changes_emitted`;
- `texture_binds`;
- `texture_bind_reuses`;
- `material_plan_cache_hits/misses`.

This gives an objective measure of whether state sorting/caching is actually useful.

---

## 5. Streaming and package I/O

Daedalus has a separate ROM cache using fixed chunks and MRU/LRU replacement. It also performs fixed loading in bounded blocks rather than relying on one huge synchronous operation.

Dusklight does not need an N64 ROM cache, but it does need the same separation:

```text
filesystem/package streaming cache  !=  texture EDRAM residency cache
```

Recommended use:

- stream/validate large packages in bounded reads;
- prefetch the next known room/startup package while current gameplay/cutscene still has spare frame budget;
- stage data in RAM before promoting hot textures to EDRAM;
- maintain a small package cache across immediately adjacent transitions;
- emit loading metrics (`read_bytes`, `read_us`, `prefetch_hits`, `prefetch_misses`).

Because Twilight Princess transitions are source-driven, prefetch hints should come from known stage/door/event destinations rather than speculative random I/O.

---

## 6. Display-list pipelining

Daedalus uses two sizeable command-list buffers and alternates them. The design allows CPU construction and GU work to overlap rather than forcing a full build/submit/wait cycle on one buffer every frame.

For Dusklight this is a **benchmark-first** optimization.

Proposed experiment:

1. instrument current `sceGuFinish`/`sceGuSync` wait and command-list build time;
2. add two aligned command-list buffers in volatile/system memory;
3. submit completed list N while CPU prepares list N+1;
4. preserve explicit synchronization before touching any resource still referenced by the previous list;
5. compare frame time, 99th-percentile stall and framebuffer identity.

Do not adopt double-list complexity if current GE wait is not a meaningful fraction of the frame.

---

## 7. VFPU fast paths

Daedalus has PSP assembly paths for vertex conversion and transform/lighting. These paths exploit 16-byte alignment and VFPU vector operations.

Potential Dusklight targets, in order:

1. Link skinning matrix/vertex work if profiling shows it is hot;
2. normal transform / simple vertex lighting;
3. vertex repacking/color packing for transient actor geometry;
4. aligned bulk copy only for sizes where measurement beats libc/SDK behavior.

Requirements for every VFPU path:

- keep a scalar reference implementation;
- build-time/runtime feature toggle;
- deterministic test vectors;
- maximum numeric error bound for transforms;
- framebuffer comparison in PPSSPP;
- real PSP timing before declaring a performance win.

VFPU is not a reason to move source behavior into assembly. Only narrow math kernels belong here.

---

## 8. Cache maintenance discipline

Daedalus contains helpers that align D-cache operations to affected ranges. Dusklight still has paths that perform broad cache writeback for convenience.

Recommendation:

- track exact staging/resident ranges;
- use aligned range writeback/invalidate for uploaded texture/vertex blocks;
- retain full-cache operations only at coarse initialization boundaries where simplicity is justified;
- add counters for bytes flushed and number of cache-maintenance calls.

This matters more once textures stream dynamically and command lists overlap.

---

## 9. Media Engine

### 9.1 What Daedalus actually proves

Daedalus packages Media Engine support as an optional PRX/service with a shared job structure and explicit begin/check/wait operations. The concrete active usage studied is asynchronous audio microcode work. The main CPU can launch the job and continue; the consumer synchronizes when the result is needed. A CPU fallback remains available.

This is an important distinction: the Media Engine is **not a replacement PSP renderer**. GU command generation and GPU submission remain main-CPU/GU responsibilities.

### 9.2 Dusklight Media Engine policy

Media Engine work is experimental and optional.

Proposed API shape:

```text
PspMeJobQueue::available()
PspMeJobQueue::submit(job_descriptor)
PspMeJobQueue::poll(ticket)
PspMeJobQueue::wait(ticket)
PspMeJobQueue::run_cpu_fallback(job_descriptor)
```

Job descriptors must be naturally/explicitly aligned, use a documented ownership/cache protocol, and contain no pointers whose lifetime can end before completion.

### 9.3 Candidate jobs

Candidate order:

1. **audio mixing/decoding** once source audio reaches the PSP route;
2. deterministic decompression or package preprocessing if profiling proves it stalls frames;
3. selected pure data transforms with no GU dependency.

Do **not** make render correctness, game simulation, save logic, event sequencing or actor behavior depend on ME availability.

### 9.4 Validation

PPSSPP is useful for functional plumbing but is not sufficient to claim real Media Engine performance. Final ME enablement requires real PSP hardware measurements and a proven CPU fallback with identical output.

Metrics:

- jobs submitted/completed;
- fallback jobs;
- busy time;
- wait time on main CPU;
- bytes processed;
- end-to-end frame/audio improvement.

---

## 10. Profiling before optimization

Daedalus ships an explicit profiler and exposes optional build flags for profiling, VFPU and Media Engine separately.

Dusklight already has useful `RenderMetrics`; extend that instead of introducing an unrelated profiler first.

Minimum counters/timers to add before broad optimization:

```text
frame_cpu_us
command_list_build_us
ge_submit_us
ge_sync_us / ge_wait_us
texture_upload_us
texture_upload_bytes
texture_cache_hits
texture_cache_misses
texture_evictions
edram_texture_resident_bytes
ram_texture_resident_bytes
material_plan_cache_hits
material_plan_cache_misses
material_passes_executed
gu_state_change_requests
gu_state_changes_emitted
package_read_us
package_read_bytes
prefetch_hits
prefetch_misses
vfpu_kernel_us (when enabled)
me_jobs / me_fallback_jobs / me_wait_us (when enabled)
```

Benchmarks should report median and tail behavior, not only one best frame.

---

## 11. Workflow improvements for Dusklight development

The Daedalus study suggests several changes that speed not only runtime but our development loop.

### Host does expensive immutable work

Move these to deterministic host conversion:

- texture format selection;
- swizzle;
- mip generation if used;
- TEV-to-PSP pass-plan compilation;
- material exact/approximate classification;
- bounds/record validation;
- package CRCs and provenance.

The PSP build should consume already PSP-ready data.

### Deterministic conversion remains mandatory

For any vNext package:

- generate twice from the same legal local source image;
- `cmp` outputs;
- record SHA-256 + size + format version;
- run host validator;
- then run PSP/PPSSPP proof.

Derived commercial packages remain local and never go to GitHub.

### Benchmark artifacts

CI/source-safe benchmarks should emit compact JSON/text metrics so performance comparisons survive remounts. Proprietary framebuffer/assets remain local; only source-safe metrics and code are published.

### Small independent optimization PRs

Do not combine TEV correctness, residency, VFPU and ME into one mega-branch. Each layer gets a separate acceptance gate and can be reverted independently.

---

## 12. Prioritized adoption matrix

| Priority | Technique | Dusklight action | Gate |
| --- | --- | --- | --- |
| P0 now | Material pass compiler | Compile supported J3D TEV to bounded PSP GU pass plan | F_SP102 visual correctness + material-plan host tests |
| P0 now | Performance/residency metrics | Extend current metrics before tuning | Stable benchmark JSON + no behavior changes |
| P1 | Host PSP-native textures | Pre-swizzled 5650/5551/4444/8888 DPTX vNext | Deterministic export + package validation + framebuffer parity |
| P1 | Texture residency cache | EDRAM-first, RAM fallback, explicit free/LRU/pinning | Memory-pressure tests + transition proof |
| P1 | GU applied-state cache | Skip redundant state calls | Same render trace/framebuffer + fewer emitted states |
| P1/P2 | Package prefetch | Source-informed staged reads for known transitions | Lower transition stall, same asset contract |
| P2 | Double command lists | Benchmark CPU/GU overlap | Lower GE wait/tail frame time on real PSP |
| P2 | VFPU kernels | Optional narrow transform/lighting/packing paths | Scalar equivalence + measurable PSP gain |
| P3 | Media Engine queue | Optional PRX/job queue, CPU fallback | Real PSP proof; no correctness dependency |

`P0/P1/P2/P3` above are performance-lane priorities and do not replace the gameplay roadmap phases. Gameplay blockers still win scheduling conflicts.

---

## 13. Immediate application to the F_SP102/title branch

Before PR #12 can leave draft:

1. inventory F_SP102 materials that currently render as large solid flats;
2. identify which require more than one texture/constant TEV stage;
3. compile those supported expressions into small pass plans;
4. keep opaque depth state exact and preserve alpha ordering;
5. record exact/approximate status per affected material;
6. rerun the real EBOOT in PPSSPP using the private F_SP102 packages;
7. accept a screenshot only when the visual result is source-faithful enough for the repository screenshot policy.

The Daedalus findings do **not** solve the separate title-logo scale/transform issue automatically; that remains a source-transform/export contract problem.

---

## 14. Acceptance rules for all performance work

An optimization is accepted only when all applicable conditions hold:

- source/gameplay behavior unchanged;
- host tests green;
- Allegrex build green;
- deterministic package generation maintained;
- no proprietary content committed;
- render trace/state invariants remain valid;
- PPSSPP framebuffer does not regress;
- performance counter shows a measurable gain for the claimed bottleneck;
- real PSP measurement is required for VFPU/ME claims that depend on hardware behavior;
- any optional accelerator has a working conservative fallback.

A faster wrong frame is a regression.

## Result

DaedalusX64 validates a direction that fits Dusklight PSP well: do more immutable work offline, make EDRAM a managed cache, compile source material complexity into explicit bounded PSP passes, reduce redundant GU state, stream/prefetch intentionally, and only then add hardware-specific acceleration such as VFPU or Media Engine where measurements justify it.

The first implementation target should be **material pass planning + instrumentation**, because it simultaneously addresses a current F_SP102 correctness defect and creates the foundation required for later texture/state batching without hiding approximations.
