# dusklight-psp

Gameplay-first PSP port of **Dusklight / Twilight Princess decompilation work**, maintained as a source-faithful compatibility effort for Sony PSP.

The priority is to make the game finishable from beginning to end with gameplay behavior matching the source game: player procedures, room transitions, doors, chests, item-get, inventory, UI, NPCs, enemies, bosses, maps, dungeons, scripted events and cinematics. Rendering stays conservative on PSP until gameplay parity is substantially complete.

## Current checkpoint

The preserved P1 checkpoint proves under PPSSPP that the original GETAWAIT Link animation (`0x16A`) can present the Heart Piece in the real `D_MN10/R02` gameplay context using source animation data, source model identity, source-derived large-chest placement, Demo_Item placement from Link joint 21 and source item-get camera geometry.

The next integration step is the complete source-driven chest lifecycle:

`interaction -> DEFAULT_TREASURE_NORMAL -> BOXOP -> GETA -> Demo_Item show -> GETAWAIT -> message -> dead() -> execItemGet() -> inventory/treasure bit -> persistence -> locomotion`.

Read `AGENTS.md`, `docs/STATUS.md` and `docs/RESUME.md` before changing code.

## Reconstruct the PSP source snapshot

The PSP-specific source/tests/scripts snapshot is preserved under `source-overlay/code-overlay-b64/`. Run:

```sh
./source-overlay/reconstruct.sh /tmp/dusklight-psp-code-overlay.tar.xz
mkdir -p /tmp/dusklight-psp-overlay
tar -xJf /tmp/dusklight-psp-code-overlay.tar.xz -C /tmp/dusklight-psp-overlay
```

Canonical archive SHA-256:

`ce56f4d674c2faad781dfd53ae1ff3a5e7110d29a3ecfab756947c099a582527`

See `docs/REPRODUCIBILITY.md` for upstream pins and commercial-data rules.

## Repository guide

- `AGENTS.md` — mandatory project/agent rules.
- `docs/STATUS.md` — authoritative checkpoint and open work.
- `docs/RESUME.md` — restart protocol for a fresh session/agent.
- `docs/ROADMAP.md` — gameplay-first phase ordering.
- `docs/COMMIT_LEDGER.md` — historical/reconstructed provenance.
- `docs/REPRODUCIBILITY.md` — source reconstruction and asset policy.
- `docs/reports/200-p1-getawait-heart-checkpoint.md` — detailed P1 report.
- `CONTRIBUTING.md` — contribution and evidence discipline.

Commercial Nintendo data is intentionally excluded. Never fabricate traces, screenshots, metrics, assets or unsupported source behavior.
