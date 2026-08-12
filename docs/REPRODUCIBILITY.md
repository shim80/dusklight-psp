# Reproducibility and non-redistributable data

## What is versioned

Source code, compatibility layers, tests, causal reports, small EBOOT proof builds and screenshots produced by the PSP port may be versioned.

## What is not versioned

Nintendo game images and extracted game resources are not stored in this repository. This includes ISO/GCM/RVZ/WBFS files and extracted RARC/BMD/BCK/BRK/BTP/BTK/BTI/etc. Converted packages derived from those assets are also intentionally ignored.

## Current Heart Piece proof provenance

The successful P1 probe was derived from a legally supplied PAL game image identified in the local campaign as GZ2P01.

Relevant source resource identities:

- `res/Object/AlAnm.arc`
  - GETA: resource `0x169`, 30 frames / 35 joints
  - GETAWAIT: resource `0x16A`, 30 frames / 35 joints
- `res/Object/O_gD_hutk.arc`
  - `o_gd_hutk.bmd`, model resource `0x0008`
  - source geometry used by the proof: 484 triangles
- `D_MN10/R02`
  - Heart Piece chest discovered from the source-derived scene actor table, not a hard-coded arbitrary position.

The proof's PSP material treatment remains intentionally simplified/unlit. Gameplay timing, pose selection, placement and camera rules are the fidelity target at this stage.

## Reproducible source overlay

`source-overlay/dusklight-psp-code-overlay.tar.gz.b64` contains the PSP-specific source/tests/scripts snapshot used for this publication. Decode and extract it with:

```sh
base64 -d source-overlay/dusklight-psp-code-overlay.tar.gz.b64 > /tmp/dusklight-psp-code-overlay.tar.gz
sha256sum /tmp/dusklight-psp-code-overlay.tar.gz
mkdir -p /tmp/dusklight-psp-overlay
tar -xzf /tmp/dusklight-psp-code-overlay.tar.gz -C /tmp/dusklight-psp-overlay
```

Expected SHA-256 of the decoded archive:

`c9ef715d967052200484480643594128640d9e92bc5ecf395808423559528b51`

The archive contains only redistributable project source/tests/scripts; commercial game data is excluded.
