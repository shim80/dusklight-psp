# Reproducibility and non-redistributable data

## What is versioned

The repository directly versions the recovered PSP compatibility source under `dusklight-main/platforms/psp/`, PSP tests, build/validation scripts, reference patches, causal reports and restart documentation. No publication-time source-overlay transport is required.

## What is not versioned

Nintendo game images and extracted game resources are intentionally absent. This includes ISO/GCM/RVZ/WBFS files and extracted RARC/BMD/BCK/BRK/BTP/BTK/BTI data. Converted packages derived from those assets are also excluded. Contributors regenerate them locally from their own legally obtained supported game image.

## Upstream pins

- Dusklight: `1bae8a5e6a812217ca33ba533e707ecfa64b1553`
- Aurora: `81f12f31d23ec822d8bde2031c91e94c470911eb`
- Nod: `dc18d2ff129f05228b8510ea092d8b24c290a49a`

These same pins are enforced by `scripts/assemble-source.sh`.

## Exact validated environment

The validated campaign baseline is Linux x86_64:

- PSPDEV/PSPSDK `v20260701`, SHA-256 `f8f2f2235995836188e5fce2e6225c4b17a47232ea82dd850dbf7a5d99c90587`;
- PPSSPP `1.20.4`, SHA-256 `661c098e6b7f7610171a57b7c533ce8bba6f2312b71e76d61e850461973eba21`.

Run:

```sh
./scripts/verify-handoff.sh
./scripts/bootstrap-repro.sh
```

The bootstrap requires ordinary host utilities (`bash`, `git`, `curl`, `tar`, `xz`, `sha256sum`, `cmake`, `python3`, `ninja`) and keeps PSPDEV/PPSSPP inside `.tools/`. Cross-platform tool definitions are pinned in `toolchain/manifest.lock` and consumed by `scripts/bootstrap-tools.sh` / `.ps1`.

## Source assembly

`scripts/assemble-source.sh` clones the exact upstream commits, injects the PSP-specific tree from this repository and produces `.work/assembled/`. This is the canonical clean-room reconstruction path for a new contributor.

## Current Heart Piece proof provenance

Relevant source identities used by the P1 proof:

- `res/Object/AlAnm.arc`: GETA `0x169`, GETAWAIT `0x16A`, both 30 frames / 35 joints;
- `res/Object/O_gD_hutk.arc`: `o_gd_hutk.bmd`, model resource `0x0008`; proof geometry 484 triangles;
- `D_MN10/R02`: Heart Piece chest discovered from the source-derived scene actor table.

The PSP material treatment remains intentionally simplified/unlit. Gameplay timing, pose selection, placement, event ownership and persistence are the fidelity targets at this stage.

## Historical boundary

The original Git object database for many historical campaign commits did not survive workspace remounts. Their SHAs, reports and causal ledgers are preserved in `docs/COMMIT_LEDGER.md` and `docs/reports/`, but those lost Git objects cannot be recreated honestly. The current repository is a reconstructed publication lineage.
