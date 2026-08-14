# Gameplay-first roadmap

## P0 — foundation

Persistent game state, save plumbing, room transitions, interaction/event surfaces, generic actor/process compatibility and reliable PPSSPP validation.

## P1 — core adventure loop (current)

Doors, chests, item-get, inventory updates, room handoffs, object persistence, player event procedures and source-driven cinematics needed by normal progression.

Immediate milestone: complete the Heart Piece chest event end-to-end using the validated GETA/GETAWAIT presentation.

### PSP performance lane (does not override gameplay priority)

Performance work is accepted only when it preserves source/gameplay behavior and has a measured bottleneck. The current reference study is `docs/reports/202-daedalus-x64-psp-rendering-performance-research.md`, anchored to DaedalusX64 commit `4f5c6fb045358044b64173fac619db5496cc2328`.

Adoption order:

1. compile supported source J3D/TEV material expressions into explicit bounded PSP GU pass plans; this is also a current F_SP102 correctness need;
2. extend runtime counters for texture residency/uploads, GU state emission, material passes, package I/O and GE wait time;
3. move immutable texture work to deterministic host conversion: target PSP format, swizzle and approved resolution/fallback classification;
4. replace monotonic room-texture EDRAM ownership with a reclaimable texture-residency cache using explicit lifetimes, pinning and measured RAM fallback;
5. add an applied GU-state cache that skips only proven redundant calls while keeping pass-boundary state deterministic;
6. add source-informed package prefetch for known transitions if transition I/O is a measured stall;
7. benchmark double command-list submission and VFPU kernels only after profiling identifies relevant CPU/GE time;
8. experiment with the Media Engine only through an optional job queue with a CPU fallback, preferably for audio or pure data work first. Render correctness and gameplay logic must never depend on ME availability.

Commercial-derived packages remain local. Runtime or host optimizations must keep deterministic package generation, host tests, Allegrex builds and PPSSPP/real-hardware validation where applicable.

## P2 — UI and progression systems

Pause/inventory, item assignment, maps, dungeon state, keys, boss keys, wallets/collectibles, message UI and file/save flow.

## P3 — world actors and combat

NPC interaction, enemies, damage/stun/death, projectiles, switches, moving platforms, dungeon mechanisms, bosses and rewards.

## P4 — full game route

Validate every required room/dungeon/cinematic transition from new game to ending. Maintain a route matrix with blockers and persistence checks.

## P5 — visual refinement

Only after the game route is broadly playable: alpha/vegetation cleanup, lighting profiles, particles/effects, cinematic polish, selective PSP co-processor work where it produces measurable value.
