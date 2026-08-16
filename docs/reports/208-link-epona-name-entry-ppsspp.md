# Link and Epona name-entry startup checkpoint

Date: 2026-08-16
Branch: `agent/psp-pc-fidelity-startup`

## Result

The canonical new-game route now restores the two startup states that were entirely
absent from the PSP build:

`team logo -> F_SP102/title -> START -> file select -> Link name -> Epona name -> F_SP108 gameplay`

Selecting an existing save still bypasses name entry. Starting a new game opens an
interactive 13-column grid with uppercase/lowercase letters, digits, space, delete and
end actions. PSP controls are D-pad navigation, Cross selection, Circle deletion,
Triangle case toggle and Start completion. Empty names cannot be accepted. Defaults
remain `Link` and `Epona`.

This checkpoint does not yet implement `demo01_01`; after Epona confirmation the route
still hands off directly to F_SP108 gameplay. Entered names are runtime-only and are not
yet serialized in the save-bank schema.

## Source-backed UI and bounded adaptation

The PC captures
`comparisons/04-player-name-pc-only-missing-on-psp.png` and
`comparisons/05-horse-name-pc-only-missing-on-psp.png` were the visual references.

The PSP screen retains the source-derived file-select background and copies 63 required
glyphs from the already compiled source Rodan DPUI into the startup DPSU. A deterministic
offline merger repacks duplicate file-select textures and glyphs in the existing
512x512 RGBA4444 atlas. The result has 71 validated records and does not increase the
startup texture EDRAM allocation.

The large stone frame, patterned black interior and complete accented character set of
the PC BLO remain approximated. The PSP composition uses a bounded sceGu frame, a
three-row ASCII grid and explicit `SPACE / DELETE / END` buttons. This restores the
flow and visual hierarchy, but it is not claimed as exact BLO or pixel parity.

## Final PPSSPP evidence

Final request: `startup-name-entry-v3`

- PPSSPP 1.20.4, OpenGL, hardware PSP renderer;
- classification: `MARKERS_VALID_METRICS_VALID`;
- result code: `0`;
- duration: 15,689 ms;
- route marker: `DUSKLIGHT_PSP_STARTUP_ROUTE_CAPTURE_OK`;
- eight RGB565 route captures, including `startup-player-name.5650` and
  `startup-horse-name.5650`;
- Allegrex EBOOT SHA-256:
  `54225746baf7711f6e5a5c164b068820b33795a632847806eb101c2c9c28e040`;
- parity build ID:
  `sha256:1d6a7694e9d2ee3edb76736f5875e1d145965378f44be5cfe90c1f0a8e2191e8`;
- visual build ID:
  `sha256:c2e46e76135adb0423d2a70f210a65e2fe9047af2b1b78809af60d9b54d2bcf8`.

Local PNGs are under `artifacts/validation/startup-name-entry-v3/`. They are ignored
validation evidence and are not committed. The F_SP108 capture hash remained stable
across the three name-entry layout iterations, so this slice did not alter the accepted
Link shading, fog, alpha foliage or water composition.

## Validation

- deterministic two-pass canonical asset build: pass;
- source Rodan DPSU merge: 63 glyphs, 71 records, 67 unique atlas items: pass;
- startup runtime and UI host suite: pass;
- name-entry state-machine host test: pass;
- name-entry DPSU identity/range host test: pass;
- full PSP compile/link: pass;
- ELF identity: 32-bit little-endian MIPS, `mips:allegrex`: pass;
- PPSSPP eight-stage route marker and metrics: pass;
- `git diff --check`: pass.

No ROM, extracted source package, captured commercial framebuffer, save file or other
non-redistributable data is included in the commits.

## Next startup slice

The largest remaining flow hole is the missing `demo01_01` sequence between Epona
confirmation and gameplay. Title composition, file-select ornamentation, name
persistence and the full PC accented character grid also remain open.
