# dusklight-psp

Gameplay-first PSP port of **Dusklight / Twilight Princess decompilation work**, maintained as a source-faithful compatibility effort for Sony PSP.

The priority is a finishable game with source-matching gameplay: player procedures, room transitions, doors, chests, item-get, inventory, UI, NPCs, enemies, bosses, maps, dungeons, scripted events and cinematics. Rendering stays conservative until gameplay parity is substantially complete.

## Current checkpoint

The preserved P1 checkpoint proves under PPSSPP that the original GETAWAIT Link animation (`0x16A`) can present the Heart Piece in the real `D_MN10/R02` gameplay context using source animation data, source model identity, source-derived large-chest placement, Demo_Item placement from Link joint 21 and source item-get camera geometry.

The next integration step is the complete source-driven chest lifecycle:

`interaction -> DEFAULT_TREASURE_NORMAL -> BOXOP -> GETA -> Demo_Item show -> GETAWAIT -> message -> dead() -> execItemGet() -> inventory/treasure bit -> persistence -> locomotion`.

Read `AGENTS.md`, `docs/STATUS.md`, `docs/RESUME.md` and `docs/EXTERNAL_HANDOFF.md` before changing code.

## Reproduce the validated development environment

The PSP-specific compatibility source, tests, scripts and recovered reports are **directly versioned in this repository**. The old publication-time `source-overlay/` transport is no longer part of `main` and is not required.

For the exact validated Linux x86_64 environment:

```sh
./scripts/verify-handoff.sh
./scripts/bootstrap-repro.sh
```

`bootstrap-repro.sh` downloads the pinned PSPDEV/PSPSDK and PPSSPP releases, verifies their SHA-256 values, and assembles the pinned upstream Dusklight/Aurora/Nod source with this repository's PSP tree under `.work/assembled/`.

For the older cross-platform tool bootstrap, `scripts/bootstrap-tools.sh` / `scripts/bootstrap-tools.ps1` use `toolchain/manifest.lock`. Those paths are useful for development, but the exact validated campaign baseline is Linux x86_64 as documented in `toolchain/REPRODUCIBLE_ENVIRONMENT.md`.

Commercial Nintendo data is intentionally excluded. A contributor must supply a legally obtained supported game image locally when original assets need regeneration.

## Repository guide

- `AGENTS.md` — mandatory project/agent rules and priority order.
- `docs/STATUS.md` — authoritative checkpoint and open work.
- `docs/RESUME.md` — exact restart protocol for a fresh session/agent.
- `docs/EXTERNAL_HANDOFF.md` — clean-room handoff instructions and known boundaries.
- `docs/ROADMAP.md` — gameplay-first phase ordering.
- `docs/COMMIT_LEDGER.md` — historical/reconstructed provenance.
- `docs/REPRODUCIBILITY.md` — source, toolchain and asset policy.
- `docs/reports/` — recovered causal campaign reports and P1 evidence.
- `test/getawait-heart-probe/` — latest preserved visible GETAWAIT Heart Piece proof harness.
- `test/chest-source-full/` — full chest-event integration harness under active development.
- `CONTRIBUTING.md` — contribution and evidence discipline.

Never fabricate traces, screenshots, metrics, assets or unsupported source behavior.
