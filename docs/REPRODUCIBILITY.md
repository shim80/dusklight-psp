# Reproducibility and non-redistributable data

## What is versioned

Source code, compatibility layers, tests, causal reports and port-generated proof material may be versioned.

## What is not versioned

Nintendo game images and extracted game resources are not stored in this repository. This includes ISO/GCM/RVZ/WBFS files and extracted RARC/BMD/BCK/BRK/BTP/BTK/BTI data. Converted packages derived from those assets are also excluded.

## Upstream pins

- Dusklight: `1bae8a5e6a812217ca33ba533e707ecfa64b1553`
- Aurora: `81f12f31d23ec822d8bde2031c91e94c470911eb`
- Nod: `dc18d2ff129f05228b8510ea092d8b24c290a49a`

## PSP source overlay

`source-overlay/code-overlay-b64/` contains the redistributable PSP-specific source/tests/scripts snapshot. Reconstruct it with:

```sh
./source-overlay/reconstruct.sh /tmp/dusklight-psp-code-overlay.tar.xz
mkdir -p /tmp/dusklight-psp-overlay
tar -xJf /tmp/dusklight-psp-code-overlay.tar.xz -C /tmp/dusklight-psp-overlay
```

Expected decoded archive:

- size: 179556 bytes
- SHA-256: `ce56f4d674c2faad781dfd53ae1ff3a5e7110d29a3ecfab756947c099a582527`
- paths: 204

`reconstruct.sh` verifies both SHA-256 and XZ integrity before the archive should be trusted.

## Current Heart Piece proof provenance

Relevant source identities used by the P1 proof:

- `res/Object/AlAnm.arc`: GETA `0x169`, GETAWAIT `0x16A`, both 30 frames / 35 joints.
- `res/Object/O_gD_hutk.arc`: `o_gd_hutk.bmd`, model resource `0x0008`; proof geometry 484 triangles.
- `D_MN10/R02`: Heart Piece chest discovered from the source-derived scene actor table.

The PSP material treatment remains intentionally simplified/unlit. Gameplay timing, pose selection, placement and camera rules are the fidelity target at this stage.
