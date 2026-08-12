# Reproducible validated environment

Exact environment used by the validated Linux x86_64 PSP campaign:

- PSPDEV/PSPSDK `v20260701`
- `pspdev-ubuntu-latest-x86_64.tar.gz`
- SHA-256 `f8f2f2235995836188e5fce2e6225c4b17a47232ea82dd850dbf7a5d99c90587`
- PPSSPP `1.20.4`
- `PPSSPP-v1.20.4-anylinux-x86_64.AppImage`
- SHA-256 `661c098e6b7f7610171a57b7c533ce8bba6f2312b71e76d61e850461973eba21`
- Dusklight `1bae8a5e6a812217ca33ba533e707ecfa64b1553`
- Aurora `81f12f31d23ec822d8bde2031c91e94c470911eb`
- Nod `dc18d2ff129f05228b8510ea092d8b24c290a49a` (`v2.0.0-alpha.10`)

Run `scripts/bootstrap-repro.sh`. It verifies official binary downloads by SHA-256 and calls `scripts/assemble-source.sh` to combine the pinned upstream source with this repository's PSP overlay.

Nintendo game images and extracted/converted commercial assets are intentionally not downloaded or versioned. Asset-generation scripts consume a legally obtained local game image.
