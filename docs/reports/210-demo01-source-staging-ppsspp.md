# Source-derived demo01_01 staging checkpoint

Date: 2026-08-16
Branch: `agent/psp-pc-fidelity-startup`

## Result

The first two F_SP108 new-game shots now use source-derived `demo01_01` staging
instead of the former gameplay-camera approximation. The wide shot renders Link and
Rusl together in their authored seated cutscene poses. The close-up subject is Rusl,
not Link. Both shots use the source actor origin, yaw, camera eye/center, corrected
widescreen FOV, dialogue identity and body-animation moment.

The bounded startup route remains:

`team logo -> F_SP102/title -> START -> file select -> Link name -> Epona name -> demo01_01 wide -> demo01_01 Rusl close-up -> F_SP108 gameplay`

Cross advances, Start skips, unattended playback remains bounded, the HUD stays
hidden during the cinematic and is restored on the first gameplay frame.

## Source evidence and compact conversion

The offline `demo01_timeline_extract.py` tool reads the local STB and BMG and emits a
deterministic, bounds-checked JSON description of the two represented shots. The
generated output is validation evidence only and is not committed.

- `demo01_01.stb` SHA-256:
  `79ae83a66188ac6fa485e4a54dec0bf40c557238db3a46703ee162f46a4bb8f2`;
- `zel_00.bmg` SHA-256:
  `daa1ede7a86ca93b31f3f489a39725e9e8e019f3cdfca37fcb7438b6cc15cb86`;
- compact timeline SHA-256:
  `1747f476dcde1c93720b4bae3dc01971a2c63f3362397bed9c2ef49d662ff170`;
- source archive: `/res/Object/Demo01_01.arc`;
- Rusl model ID: `47` (`demo01_moi_cut00_gp_1.bmd`);
- Rusl body BCK IDs: `18` wide, `19` close-up;
- Link body BCK IDs: `3` wide, `4` close-up;
- Link close-up face BCK ID: `5` (identified, not yet played on PSP);
- Rusl BTK ID: `31` and BTP ID: `41` (identified, not yet played on PSP);
- common actor translation: `(-17320, -62, -5100)`;
- common actor yaw: `-115` degrees;
- wide source frame `270`, looped body sample frame `30`, STB message index
  `1512`, BMG message ID `3002`;
- close-up source frame `482`, local body sample frame `92`, STB message index
  `1514`, BMG message ID `3004`.

Wide camera:

- eye `(-16537.84375, 230.79931640625, -4335.28515625)`;
- center `(-16656.85546875, 204.42723083496094, -4432.3349609375)`;
- source FOV `30.128679275512695` degrees.

Rusl close-up camera:

- eye `(-17422.09765625, -33.586063385009766, -5202.60205078125)`;
- center `(-17379.376953125, 19.296863555908203, -5062.423828125)`;
- source FOV `23.482006072998047` degrees.

The PSP applies the same 4:3-to-480:272 tangent-space FOV correction used by the
desktop camera path. The old hand-authored Link-relative camera offsets are no longer
used for these shots.

## Actor packages and rendering

The local exporter now produces two pre-skinned Rusl DPRM poses, one shared Rusl DPTX
texture package, and the exact Link wide/close-up DPAN tracks. Weighted envelope
vertices are baked with the animated weight matrices; this fixes the deformation
found during framebuffer validation.

The startup loader owns the five intro packages separately from the regular gameplay
packages. Link swaps to the appropriate cutscene animation for each shot and restores
the gameplay animation at handoff. Rusl uses the existing bounded static-model
renderer, with source-derived warm precomputed vertex lighting for the two known
poses. No per-frame heap allocation or generic event VM was added.

This is a compact two-shot runtime, not full `demo01_01` playback. Rusl is currently
represented by two deterministic pre-skinned poses rather than a live skeleton.

## Dialogue presentation

The opaque cyan placeholder pane has been replaced by a lower translucent cinematic
vignette and a compact top confirmation strip. Source Rodan glyphs, tighter margins,
the PSP green confirm prompt and the exact source strings are used. The BMG emphasis
tags for message `3004` are represented by a colored `hour of twilight` run.

The pane ornaments remain a bounded geometric approximation rather than converted
source UI textures. Some glyph spacing/coverage defects remain visible in the PSP
DPUI atlas.

## Rejected framebuffer experiments

- v5 baked animated positions with joint matrices only. Rusl's weighted-envelope
  geometry stretched across the frame, so the result was rejected and the exporter
  was corrected to use animated weight matrices.
- v7 passed raw 4:3 source FOV values directly to the 480:272 PSP projection. Both
  shots were too wide relative to the desktop oracle, so the result was rejected and
  the desktop tangent-space widescreen correction was reproduced.

No rejected visual variant remains enabled in product source.

## Final PPSSPP evidence

Final request: `startup-intro-fidelity-v10`

- PPSSPP 1.20.4, OpenGL, hardware PSP renderer;
- classification: `MARKERS_VALID_METRICS_VALID`;
- result code: `0`;
- duration: `16,681 ms`;
- route marker: `DUSKLIGHT_PSP_STARTUP_ROUTE_CAPTURE_OK`;
- Allegrex EBOOT SHA-256:
  `cb7bbba55c3a421cdc4de71e954692605ac60039cc6e62a3c50ad2652e05aaa3`;
- parity build ID:
  `sha256:b7fdf34860fe02bca8f936af920e50ef63aef903404ef26bd8255b41612d50e1`;
- visual build ID:
  `sha256:1fc2c23540541ea61139c62c05fa2d8ac50c405b32f2eff4044d44b998c7158a`.

Local ignored evidence is under `artifacts/demo01-fidelity-v10/`:

- `startup-intro-wide.png` SHA-256:
  `e450501bba224bd5ff068466bee13a9989dfd515ffa98d976ea1b81a8b2f6051`;
- `startup-intro-closeup.png` SHA-256:
  `4d7fc2bef851bfae693ed67ed6fdae357f8b29f971b2dfc786914eaf876b71a6`;
- `ab-wide.png` SHA-256:
  `31b192d899d757b5ea292dcb016cc9edd69c442bcfaceb13fb6cd5a48b28dfac`;
- `ab-rusl-closeup.png` SHA-256:
  `e9a39dc4cf43efb05cdcbeb16f1639d9150a0a85c0c98de6a8859d2a4fdb4a48`.

The PC-left/PSP-right review confirms equivalent subjects, source shot direction,
seated composition, dialogue state and approximate framing. The GUI broker was
unbootstrapped after capture.

## Validation

- deterministic compact STB/BMG extraction: pass;
- deterministic 18-file startup asset export: pass;
- deterministic 37-entry PSP asset package: pass;
- startup runtime/name-entry/intro/UI host suite: pass;
- PSP compile, Allegrex link and EBOOT generation: pass;
- PPSSPP marker and metrics validation: pass;
- PC-left/PSP-right framebuffer review: pass for the two targeted structural shots;
- no ROM, extracted model/texture, commercial framebuffer or EBOOT is staged.

## Remaining fidelity boundary

- Rusl BTK `31` and BTP `41` are identified but not compiled into runtime tracks;
- Link face BCK `5` and face texture animation are not played;
- Rusl's PSP eye/mouth state therefore differs visibly from the PC close-up;
- water and cascade UV/material animation remain static and cooler than the PC;
- room materials and character lighting are still flatter than the desktop renderer;
- dialogue ornaments are approximated and the font atlas has visible spacing holes;
- no additional intermediate `demo01_01` shot is represented yet;
- F_SP102 `demo38`/title was not improved in this slice;
- no post-process was added.

Estimated full startup fidelity after this checkpoint:

- flow fidelity: `92%`;
- visual fidelity: `50%`.
