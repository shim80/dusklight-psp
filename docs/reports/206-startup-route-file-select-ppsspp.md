# Reduced startup route and PPSSPP visual checkpoint

Date: 2026-08-16
Branch: `agent/psp-pc-fidelity-startup`
PR: #22 (draft)

## Result

The asset-backed canonical EBOOT now completes the intended route:

`Dusklight PSP team logo -> F_SP102 title -> START -> file select -> F_SP108/R01/start21`

The old Nintendo, Dolby, warning, progressive-scan and original boot presentation
remain absent. The file-select screen is rendered through the source-derived DPSU
package and its slot cursor instead of the PSP debug console.

An exact opt-in route token, `DUSKLIGHT.ROUTE.CAPTURE=route_v1`, drives the
otherwise unchanged player flow and records six RGB565 framebuffers. Normal builds
remain controller-driven.

## Final PPSSPP evidence

Final request: `startup-title-fidelity-v12`

- PPSSPP: pinned 1.20.4, OpenGL, hardware PSP renderer;
- classification: `MARKERS_VALID_METRICS_VALID`;
- result code: `0`;
- duration: 15,208 ms;
- route marker: `DUSKLIGHT_PSP_STARTUP_ROUTE_CAPTURE_OK`;
- captured frames: team logo, F_SP102 scene, title logo, title prompt, file select,
  and F_SP108 gameplay;
- Allegrex EBOOT SHA-256:
  `eb8d4412a674f18a0885ec659c681d1fc71c187ef637797b606c880bc6ad09e1`;
- parity build ID:
  `sha256:ebe1a7c501c0a4e9c0a145dddf8a8d58a9d91471ffd54bc2ae2650573c4b9a97`;
- visual build ID:
  `sha256:15b68827e9a22b470bc009069f6b686cdff3a3ed8ce5fcc026970cdb31d78caa`.

Local PNG conversions are under
`artifacts/validation/startup-title-fidelity-v12/`. They are validation evidence,
not redistributable game assets, and are not committed.

## Retained changes

- corrected the canonical startup asset contract to the packaged
  `title_room.dprm/.dptx` names;
- removed the nonexistent `title_camera.dpcm` dependency and used the already
  source-validated Item3D camera checkpoints;
- rendered the file-select package and selection cursor as player-facing UI;
- added deterministic route capture and marker generation for PPSSPP validation;
- made the F_SP108 MPV1 material-pass upgrade part of the reproducible asset build,
  retaining the improved bounded water composition;
- embedded and validated the build commit identity in the PSP target;
- added a reusable RGB565-to-PNG inspection tool.

## Bounded title experiments

Two source-backed title material experiments were evaluated and rejected:

1. multi-texture sampler/matrix pass: the logo became a large white rectangle;
2. corrected secondary-texture packing: the logo disappeared.

Both experimental runtime/export changes were removed. The final run deliberately
restores the recognizable branch baseline instead of committing a visual regression.
The baseline still has obvious title compositing/UV defects and is not PC parity.

## Validation

Passed:

- deterministic full startup/PSP asset build;
- startup runtime, camera, UI, title-package and first-playable host tests;
- full Allegrex compile/link and `mips:allegrex` object identity;
- parity-build identity host test;
- PPSSPP asset-backed route with six captures, valid marker and valid metrics;
- Python syntax checks and `git diff --check`.

`scripts/test-f-sp108-adapter-parity.sh` could not be replayed from this reconstructed
worktree because its untracked audit helper is absent. The full deterministic asset
pipeline still executed the F_SP108 alpha-state patch and MPV1 upgrade successfully.

## Open visual work

- title TEV composition, UV transforms and BPK/BRK/BTK animation;
- source-faithful file-select typography/content beyond the converted panels/cursor;
- F_SP102 exposure, fog and far-background parity;
- a same-build PSP-versus-exact-desktop screenshot comparison;
- full F_SP108 water, fog and scene-layer acceptance on physical PSP.

No ROM, extracted package, commercial asset, save file or captured commercial image
is included in the commit.
