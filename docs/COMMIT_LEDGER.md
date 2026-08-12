# Commit and provenance ledger

## Historical campaign commits

These SHAs were reported and validated during earlier workspaces. The reconstructed publication repository may not contain their Git objects because the original `.git` database was repeatedly lost/remounted. Reports preserved in the publication overlay retain that causal history.

| Commit | Summary |
|---|---|
| `b572ab1` | Camera source divergence pushed later. |
| `20d8fde` | CAMERA layer closed. |
| `a62238d` | INPUT layer closed. |
| `7629fbe` | Source turn animation integration. |
| `a4e18fd` | DPAN package capability work. |
| `5fffb95` | Controller input ordering. |
| `5a986f2` | Causal closure documentation. |
| `a69b05b` | Final 10/10 animation validation. |
| `ee2e730` | Source recursive old-frame morph correction. |
| `a856982` | Follow-up animation/runtime validation. |
| `741ca9d` | Animation closure evidence. |
| `5315bf1` | Double-controller correction. |
| `3fff002` | Causal state/evidence checkpoint. |
| `9fbd1f1a3275c45454ba97e85e6c20ca40b0fe7c` | Pre-ground pose and DPAN Hermite blocker. |
| `1fe7c35` | PSP render-state trace gate closure. |
| `2c9621d` ... `3543c8e` | V3 depth-state/source alignment sequence. |
| `e8c40f3` | Alpha inventory and decoder tests. |
| `00dbefb` | UI source audit. |
| `fbda917` | Effective lighting audit. |
| `290be7e` | Depth observability blocker/state matrix. |
| `7b8fff4` | Synthetic PSP depth fixtures + UI inventory checkpoint. |

## Reconstructed publication lineage

The `dusklight-psp` repository intentionally starts a fresh Git lineage under `shim80`. Its publication commits preserve the recovered PSP source overlay, the current P1 gameplay checkpoint, and restart documentation. Do not manufacture the historical SHAs above in this reconstructed lineage.

The local publication bundle from which this GitHub repository was prepared contained these logical checkpoints:

- `9c10897` — Import recovered Dusklight PSP source baseline.
- `acb8cba` — Preserve P1 GETAWAIT Heart Piece gameplay checkpoint.
- `5dc6610` — Add complete project resume and publication directives.
- `00b3cc7` — Preserve causal iteration ledger.

Those identifiers refer to the preserved local publication lineage; GitHub-side commit SHAs differ because this repository was reconstructed through the GitHub object/contents APIs after repeated workspace remounts.
