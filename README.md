# Dusklight PSP

A gameplay-first Sony PSP port of **Dusklight / Twilight Princess decompilation work**.

The goal is simple: make the game playable from beginning to end on PSP while keeping gameplay, event flow and source behavior as close as practical to the pinned Dusklight source. Visual fidelity is deliberately secondary for now; conservative/unlit rendering is preferred over delaying gameplay progress.

> **Status:** active development. No public gameplay release is considered ready yet.

## What is being ported first

The current priority is complete game flow rather than rendering polish:

- intro and scripted events;
- main menu and PSP-friendly controls;
- save creation/loading;
- player movement and room transitions;
- doors, chests and item-get sequences;
- inventory and pause/UI systems;
- NPCs, enemies and bosses;
- maps, dungeons and cinematics;
- persistence across room recreation and saves.

Advanced lighting, post-processing and exact material presentation remain intentionally deferred until gameplay is substantially complete.

## Current gameplay checkpoint

The preserved PSP/PPSSPP work has already demonstrated the original GETAWAIT Link animation (`0x16A`) presenting the Heart Piece in the real `D_MN10/R02` gameplay context with source-derived placement and item-get camera geometry.

The active integration checkpoint is the complete source-driven chest lifecycle:

```text
interaction
  -> DEFAULT_TREASURE_NORMAL
  -> BOXOP
  -> GETA
  -> Demo_Item show
  -> GETAWAIT
  -> item message
  -> Demo_Item dead
  -> source-owned execItemGet()
  -> inventory / treasure bit
  -> persistence
  -> locomotion
```

The current branch also cross-compiles this checkpoint to a real PSP `EBOOT.PBP` in GitHub Actions. CI artifacts are development checkpoints only; they are **not** public releases and may still require local game-derived assets to run correctly.

## Screenshots

Real PSP/PPSSPP screenshots will be shown here once they are captured from validated gameplay checkpoints.

No screenshots are currently committed to the repository, and this project intentionally does not use fabricated or desktop-only images as PSP evidence.

When the next asset-backed PPSSPP validation is complete, this section will be updated with actual PSP captures from the intro/menu/gameplay path.

## Releases

A public EBOOT release will be created only after the first playable milestone is ready:

**intro cinematic + main menu + save management + beginning of the game + PSP-adapted controls**.

Until then, development EBOOTs may appear as temporary GitHub Actions artifacts for testing, but they should not be treated as stable releases.

## Reproducing the development environment

The PSP compatibility code, tests, scripts and recovered reports are versioned in this repository. Commercial Nintendo data is intentionally excluded.

For the exact validated Linux x86_64 environment:

```sh
./scripts/verify-handoff.sh
./scripts/bootstrap-repro.sh
```

`bootstrap-repro.sh` downloads and verifies the pinned PSPDEV/PSPSDK and PPSSPP versions, then assembles the pinned upstream Dusklight/Aurora/Nod source tree with the PSP compatibility layer under `.work/assembled/`.

A contributor must provide a legally obtained supported game image locally whenever original game assets need to be regenerated.

## Repository map

- `AGENTS.md` — project rules and gameplay-first priority order.
- `docs/STATUS.md` — authoritative current checkpoint and open work.
- `docs/RESUME.md` — exact restart protocol for a fresh development session.
- `docs/ROADMAP.md` — gameplay-first phase ordering.
- `docs/EXTERNAL_HANDOFF.md` — clean-room handoff and known boundaries.
- `docs/REPRODUCIBILITY.md` — source, toolchain and asset policy.
- `docs/COMMIT_LEDGER.md` — historical/reconstructed provenance.
- `docs/reports/` — recovered causal campaign reports and PSP evidence notes.
- `test/getawait-heart-probe/` — preserved GETAWAIT Heart Piece visibility proof.
- `test/chest-source-full/` — full source-driven chest-event integration harness.
- `CONTRIBUTING.md` — contribution and evidence requirements.

## Development rule

Never fabricate traces, screenshots, metrics, assets or unsupported source behavior. A gameplay checkpoint is only considered validated once its evidence actually exists.
