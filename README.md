# dusklight-psp

Gameplay-first PSP port of **Dusklight / Twilight Princess decompilation work**, maintained as a source-faithful compatibility effort for Sony PSP.

The project goal is not to reproduce the desktop renderer first. The priority is to make the game finishable from beginning to end with gameplay behavior matching the source game: player procedures, room transitions, doors, chests, item-get, inventory, UI, NPCs, enemies, bosses, maps, dungeons, scripted events and cinematics. Rendering is intentionally conservative on PSP: unlit where needed, simple alpha/blending, bounded particles and minimal post-processing until gameplay parity is substantially complete.

## Current checkpoint

The latest preserved P1 checkpoint proves under PPSSPP that the original **GETAWAIT** Link animation (`resource 0x16A`) can present the Heart Piece in the real `D_MN10/R02` gameplay context using source animation data, source Heart Piece model identity, source-derived large-chest placement, Demo_Item placement from Link joint 21, and source item-get camera geometry.

The next integration step is **not another visual probe**. It is to put the validated GETA/GETAWAIT pose and Demo_Item visibility back into the complete source-driven chest lifecycle:

`interaction -> DEFAULT_TREASURE_NORMAL -> BOXOP -> GETA -> Demo_Item show -> GETAWAIT -> message -> dead() -> execItemGet() -> inventory/treasure bit -> persistence -> locomotion`.

Read [`docs/STATUS.md`](docs/STATUS.md), [`docs/RESUME.md`](docs/RESUME.md), and [`AGENTS.md`](AGENTS.md) before changing code.

## Publication format

Because this repository was reconstructed after repeated ephemeral-workspace remounts, the PSP-specific source/test/script snapshot is preserved as a verified redistributable overlay:

`source-overlay/dusklight-psp-code-overlay.tar.gz.b64`

Decode and extract it as documented in [`docs/REPRODUCIBILITY.md`](docs/REPRODUCIBILITY.md). The decoded archive SHA-256 is:

`c9ef715d967052200484480643594128640d9e92bc5ecf395808423559528b51`

Commercial Nintendo data is intentionally excluded. Contributors must supply their own legally obtained game image when original resources must be regenerated.

## Repository guide

- `AGENTS.md` — mandatory project/agent rules.
- `docs/STATUS.md` — authoritative current checkpoint and open work.
- `docs/RESUME.md` — exact restart protocol for a fresh session/agent.
- `docs/ROADMAP.md` — gameplay-first phase ordering.
- `docs/COMMIT_LEDGER.md` — historical and reconstructed commit provenance.
- `docs/REPRODUCIBILITY.md` — source overlay and non-redistributable asset policy.
- `docs/reports/200-p1-getawait-heart-checkpoint.md` — detailed latest P1 report.
- `CONTRIBUTING.md` — contribution and evidence discipline.

## Validation rule

A feature is not considered closed because it compiles. Prefer this evidence order:

1. source behavior identified;
2. host regression test where meaningful;
3. Allegrex build;
4. PPSSPP execution with explicit success marker;
5. real gameplay visual evidence when the change is visual;
6. persistence/re-entry test for stateful gameplay.

Never fabricate traces, screenshots, metrics, assets or unsupported source behavior.
