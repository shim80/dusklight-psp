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

## Current preserved proof

`test/getawait-heart-probe/` is the latest source-preserved PSP harness in this checkpoint.

It searches the R02 source-derived actor table for the Heart Piece chest, derives Link's large-chest placement and GETAWAIT yaw, applies source animation resource `0x16A`, derives Demo_Item position from Link joint 21, and scans source-correct item-get camera side/frame combinations for visible framebuffer evidence.

Preserved proof artifacts are documented in the publication overlay and evidence manifest. Commercial game data is intentionally not versioned.

## Active task

Integrate the validated GETA/GETAWAIT and Demo_Item presentation into the complete original chest event lifecycle. Do not spend the next cycle on lighting or post-processing.

Desired closure sequence:

`OPEN interaction -> DEFAULT_TREASURE_NORMAL -> BOXOP -> GETA -> Demo_Item show -> GETAWAIT -> item message -> Demo_Item dead -> execItemGet -> inventory -> treasure bit -> event cleanup -> locomotion -> room recreation persistence`

## Explicitly not closed

- full inventory/pause UI fidelity;
- all chest sizes/items;
- complete item message UI and message database integration;
- all source item visual animation channels (BCK/BRK/TEV) on PSP;
- broad NPC/enemy/boss coverage;
- all dungeon/map systems;
- full cinematic coverage;
- advanced lighting/post-processing parity.
