# Dusklight PSP — current status

Updated: 2026-08-12

## Project direction

Gameplay completeness is the dominant priority. The target is end-to-end completion of the game with original mechanics and event behavior. PSP graphics remain intentionally conservative until gameplay systems are substantially complete.

## Historical parity foundation retained in this snapshot

The repository contains the prior causal/render campaign reports through the V3 opaque/depth work, including camera/input/animation closures, render-state tracing, actor portability, room transition investigations, UI inventory, lighting audits and depth evidence.

Important historical checkpoints reported during the campaign include:

- `b572ab1` — camera source divergence pushed to tick 7;
- `20d8fde` — camera layer closed;
- `a62238d` — input layer closed;
- `7629fbe`, `a4e18fd`, `5fffb95`, `5a986f2`, `a69b05b` — animation-source/DPAN/controller/closure sequence;
- `ee2e730`, `a856982`, `741ca9d` — old-frame morph animation closure;
- `5315bf1`, `3fff002` — controller double-state correction and causal evidence;
- `9fbd1f1a3275c45454ba97e85e6c20ca40b0fe7c` — pre-ground pose / Hermite blocker report;
- `1fe7c35` — PSP render-state trace gate;
- `2c9621d`, `a4e4d40`, `7c0dce3`, `82bc775`, `9d59255`, `333753e`, `3543c8e` — V3 depth-state convergence and opaque-only profile sequence;
- `e8c40f3`, `00dbefb`, `fbda917`, `290be7e` — alpha/UI/lighting/depth observability audits;
- `7b8fff4` — synthetic PSP depth fixtures and UI inventory checkpoint.

The original Git object database for all of those commits did not survive every workspace remount. Their reports and identifiers are preserved as historical provenance; do not rewrite them as if this reconstructed Git repository still had those objects.

## Gameplay P1 advances

The following gameplay behavior has been demonstrated during the P1 campaign and is the intended integration direction:

- source-derived room handoff architecture and clean Link locomotion handoff;
- original `DOOR20` event sequencing and source door animations in PPSSPP;
- source-derived Link `003n_dash` movement work;
- original TBOX/chest lifecycle, source event ordering and persistence work;
- deferred Demo_Item acquisition semantics: creation/show/message/dead/commit are distinct stages;
- original GETA/GETAWAIT Link animation resources recovered from `AlAnm.arc`;
- Heart Piece source model identity recovered as `o_gd_hutk.bmd`, resource ID 8, 484 triangles;
- source-driven Heart Piece placement from Link joint 21 + item offset;
- GETAWAIT Heart Piece visibility proven under PPSSPP in the preserved probe.

## Startup/save checkpoint

The release-path save layer is now represented by source-controlled PSP runtime code rather than only a standalone UI experiment.

Validated behavior on branch `agent/startup-save-flow`:

- three source-shaped file slots, initial selection 0, up/down clamped without wrap;
- PSP Cross or START confirms the selected file;
- a new file is created at `F_SP108`, room 1, start point 21, layer 0;
- fixed `DPSV` v1 persistence with CRC32 and exact slot metadata restoration;
- an occupied slot resumes its persisted `stage/room/start/layer` context;
- `StartupSaveFlow` binds `StartupRuntime` file-selection and new-game-transition segments to persistence and exposes gameplay handoff only after the transition boundary;
- a new game is persisted immediately before gameplay handoff;
- write/read failures fail closed instead of silently replacing or ignoring save data;
- host integration destroys/recreates the flow and verifies continuation from the persisted gameplay checkpoint.

GitHub Actions run `31630808657` on commit `a0fb49ab3f3e8587385160a95f5ef3b02e8a19db` passed the save host test, startup/save integration test, pinned PSPDEV cross-build and a real PPSSPP 1.20.4 boot/capture. The generated development EBOOT SHA-256 was `d12c2edfafcfb09dde1651c9459df131f42524439af2a19b00d0da26ba8afb51`.

This is logical/runtime proof for the save boundary plus a real PSP executable boot. It is not yet proof of the complete asset-backed canonical intro/title/file-select/F_SP108 route in one PPSSPP run.

## Current preserved proof

`test/getawait-heart-probe/` preserves the latest asset-backed item-presentation proof. `test/startup-save-host/`, `test/startup-save-integration-host/` and `test/startup-save-psp/` preserve the public save-flow logic, recreation/persistence and PSP boot proofs.

Commercial game data is intentionally not versioned.

## Active task

Close the first public-release path in the canonical EBOOT:

`intro/opening -> title -> file select -> create/load persistent slot -> new-game transition -> F_SP108 first playable -> PSP controls`

The save boundary itself is now implemented and publicly tested. The next required proof is to wire it into the canonical asset-backed startup executable and replay the route in PPSSPP with the real startup packages, then continue through the first playable controls.

In parallel, the source-driven chest lifecycle remains a gameplay checkpoint and must not be merged as fully validated until its asset-backed lifecycle is replayed after the source-owned Demo_Item commit change.

## Explicitly not closed

- complete asset-backed canonical intro/title/save/F_SP108 run with the new persistence layer;
- first-playable PSP control acceptance for the release milestone;
- full inventory/pause UI fidelity;
- all chest sizes/items;
- complete item message UI and message database integration;
- all source item visual animation channels (BCK/BRK/TEV) on PSP;
- broad NPC/enemy/boss coverage;
- all dungeon/map systems;
- full cinematic coverage;
- advanced lighting/post-processing parity.
