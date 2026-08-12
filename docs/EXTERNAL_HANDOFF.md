# External clean-room handoff

This document answers one question: can a contributor with no prior conversation reconstruct the environment, understand the current state and continue in the intended direction?

## Start here

Read in this order:

1. `AGENTS.md`
2. `docs/STATUS.md`
3. `docs/RESUME.md`
4. `docs/ROADMAP.md`
5. `docs/COMMIT_LEDGER.md`
6. the newest files in `docs/reports/`, especially `200-p1-getawait-heart-checkpoint.md`
7. `test/getawait-heart-probe/main.cpp`
8. `test/chest-source-full/main.cpp`

Then run `./scripts/verify-handoff.sh`.

## Exact validated environment

For the environment used by the preserved PSP campaign, use Linux x86_64 and run:

```sh
./scripts/bootstrap-repro.sh
```

This pins and verifies PSPDEV/PSPSDK `v20260701`, PPSSPP `1.20.4`, Dusklight, Aurora and Nod. It assembles a clean source tree under `.work/assembled/` without modifying the checked-in source snapshot.

The cross-platform bootstrap (`bootstrap-tools.sh` / `.ps1`) is preserved for macOS/Linux/WSL workflows through `toolchain/manifest.lock`, but the strongest reproducibility claim applies to the Linux x86_64 baseline actually used for the latest campaign.

## Commercial data boundary

The repository deliberately does not contain Nintendo game data. When original assets are required, provide a legally obtained supported game image locally and set `DUSKLIGHT_GAME_IMAGE`. Asset-generation scripts write generated packages under ignored build/test paths. Never commit the game image, extracted archives or converted packages.

## Build and run the preserved P1 probe

With PSPDEV active:

```sh
psp-cmake -S test/getawait-heart-probe -B build/psp/getawait-heart-probe -G Ninja -DCMAKE_BUILD_TYPE=RelWithDebInfo
cmake --build build/psp/getawait-heart-probe -j 4
```

The probe needs locally regenerated Link/R02/Heart Piece/HUD packages documented in `docs/RESUME.md`. The latest validated runtime proof is PPSSPP, not a fresh physical-PSP hardware certification.

## Current direction

Do not reopen generic renderer/depth work without a new gameplay-visible regression. The active P1 task is the source-owned chest sequence:

`OPEN -> DEFAULT_TREASURE_NORMAL -> BOXOP -> GETA -> Demo_Item show -> GETAWAIT -> message -> dead() -> execItemGet() -> inventory/treasure bit -> cleanup -> locomotion -> recreation persistence`.

The next checkpoint is valid only if the whole sequence succeeds in one PPSSPP gameplay run with deterministic markers and a genuine gameplay screenshot.

## Historical fidelity

The repository contains the recovered source snapshot, causal reports, matrices and ledgers needed to understand why previous decisions were made. It does **not** pretend that lost historical Git objects still exist. `docs/COMMIT_LEDGER.md` explicitly separates historical SHAs from the reconstructed Git lineage.

## Known limits

- Full game completion is not claimed; `docs/STATUS.md` lists systems still open.
- Commercial assets are not redistributable and must be regenerated locally.
- The most recent execution proof is PPSSPP running a real Allegrex EBOOT; physical PSP hardware still deserves a fresh validation pass before claiming hardware certification.
- Cross-platform bootstrap paths are preserved, while Linux x86_64 is the exact validated environment.
