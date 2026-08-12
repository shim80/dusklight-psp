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

## PSP link and source-owned commit checkpoint

The stacked gameplay branch now has an asset-independent host regression for the source-owned `Demo_Item` commit boundary. It verifies that inventory remains unchanged before `dead()`, commits once on the following normal source process frame, and remains exactly one on a second frame.

The PSP chest target's final link order is also corrected: every Dusklight archive is enclosed in one linker rescan group, followed by the PSPSDK/GU import libraries. GitHub Actions fails if `psp-fixup-imports` ever emits the former `stubs out of order` warning.

Validated in GitHub Actions run `31633775215`:

- host marker: `DEMO_ITEM_COMMIT_HOST_OK`;
- Allegrex `EBOOT.PBP` built without the import-order warning;
- EBOOT SHA-256: `ca162d1d48f8b595bd15d4b845ff2c97e6524759b14ac90e9d779826a9e45a18`;
- PPSSPP reached the controlled missing-assets boundary `CHEST_SOURCE_FULL.FAIL` with `code=10`.

That PPSSPP marker proves boot and import resolution in public CI. It does not replace the asset-backed gameplay run required to close the complete chest lifecycle. See `docs/reports/201-psp-import-stub-order.md`.

## Active task

Integrate and validate the complete original chest event lifecycle with the source-owned `Demo_Item` commit path. Do not spend the next gameplay cycle on broad lighting or post-processing.

Desired closure sequence:

`OPEN interaction -> DEFAULT_TREASURE_NORMAL -> BOXOP -> GETA -> Demo_Item show -> GETAWAIT -> item message -> Demo_Item dead -> execItemGet -> inventory -> treasure bit -> event cleanup -> locomotion -> room recreation persistence`

After that gameplay checkpoint is integrated, the separate visual issue remains the source BCK/BRK/TEV presentation for `O_gD_hutk`.

## Explicitly not closed

- asset-backed full chest lifecycle validation for the current source-owned commit branch;
- source Heart Piece BCK/BRK/TEV presentation on PSP;
- full inventory/pause UI fidelity;
- all chest sizes/items;
- broad NPC/enemy/boss coverage;
- all dungeon/map systems;
- full cinematic coverage;
- advanced lighting/post-processing parity.
