# Dusklight PSP agent directives

These directives are mandatory for automated agents and human contributors working on the PSP port.

## Mission and priority

The primary objective is a 100% finishable PSP version with source-faithful gameplay. Prioritize, in order:

1. progression blockers and save/persistent state;
2. player procedures, interactions and scripted events;
3. room/dungeon transitions and collision;
4. objects, chests, item-get and inventory;
5. UI, pause/inventory/map/dungeon interfaces;
6. NPC/enemy/boss behavior and combat;
7. cinematics and gameplay effects;
8. visual refinement only after gameplay systems are dependable.

For graphics, stay within PSP limits: conservative unlit/vertex-lit rendering, simple alpha test/blend, lightweight particles, minimal post-process. Do not trade gameplay correctness for expensive graphics.

## Source fidelity

The desktop/source project is the behavior oracle. Port the actual source rules, constants, event dependencies, animation resource identities and persistence semantics. Do not replace missing behavior with magic timers or actor-specific hacks when the source data can be read.

When a branch is not implemented or not proven, fail closed and document it as incomplete. Never label an approximation as parity.

## Validation discipline

For meaningful changes:

- run targeted host tests;
- compile for Allegrex;
- execute a real EBOOT in PPSSPP when the change reaches PSP runtime behavior;
- use marker files/metrics for deterministic checks;
- use screenshots only for source-faithful game UI or visible asset-backed gameplay advances;
- never publish or commit text-only diagnostics, synthetic/public probes, boot screens or technical framebuffer captures under `screenshot/`;
- keep diagnostic captures in CI artifacts/logs only and never present them as product screenshots;
- keep visual proof separate from logical proof;
- for stateful features, destroy/recreate/re-enter and verify persistence.

A successful boot is not a successful feature test.

## Commercial data

Never commit proprietary game images or extracted Nintendo assets. Keep ISO/RARC/BMD/BCK and converted packages local. Repository scripts may describe how a contributor can reproduce derived data from their own legally obtained image.

## Git/recovery

- Repository owner/publication identity: `shim80`.
- Keep `main` resumable at every commit.
- Commit reports alongside behavior changes.
- Do not amend historical evidence to make a newer hypothesis look proven.
- Update `docs/STATUS.md` and `docs/RESUME.md` at every material checkpoint.
- Record blockers honestly, including infrastructure/remount/quota failures.
- Before destructive cleanup, preserve the current worktree state and proof hashes.

## Current P1 rule

Do not reopen the renderer's generic three-model submission bug: it was isolated and proven to render an added model. The active integration task is the full chest/item-get lifecycle using the validated GETA/GETAWAIT Link poses and Demo_Item placement.
