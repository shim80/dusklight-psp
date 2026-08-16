# Bounded F_SP108 new-game intro checkpoint

Date: 2026-08-16
Branch: `agent/psp-pc-fidelity-startup`

## Result

The canonical new-game route no longer hands off directly from Epona naming to
playable F_SP108. It now traverses:

`team logo -> F_SP102/title -> START -> file select -> Link name -> Epona name -> F_SP108 intro -> F_SP108 gameplay`

The new intro has a wide shot and a Link close-up. Each shot displays one source
opening line in a dark, gold-edged message pane using the source Rodan font. Cross
advances, Start skips, and each shot also advances after 270 frames so automated and
unattended routes cannot stall. Existing saves continue to bypass name entry and the
new-game intro.

The HUD is hidden in both intro shots and restored on the first gameplay frame. The
handoff reuses the initialized F_SP108 room, Link runtime, source environment, fog,
alpha foliage, bounded MPV1 water composition and accepted safe wrapped Link lighting.

## Fidelity boundary

The desktop reference captures
`comparisons/06-fsp108-demo-wide-pc-only.png` and
`comparisons/07-fsp108-demo-closeup-pc-only.png` define the intended composition.
This checkpoint restores the missing narrative beat and its visible hierarchy, but is
not exact `demo01_01` event parity.

Source-backed elements:

- the real exported F_SP108 room, Link model, animation runtime and environment;
- the accepted foliage/water/material passes and Link lighting;
- Rodan glyphs extracted from the source font;
- the two opening dialogue lines and wide/close-up shot structure.

Bounded PSP approximations still present:

- Rusl is not instantiated or rendered;
- Link remains in the standing idle runtime rather than following the full source
  cutscene animation;
- camera positions and message-pane effects are PSP-specific approximations;
- source event timing, facial animation and all intermediate `demo01_01` staging are
  not implemented.

Entered Link and Epona names remain runtime-only and are not serialized in the save
bank.

## PSP texture-storage correction

The first intro replay exposed corrupted message glyphs even though host-side atlas
validation passed. The canonical HUD atlas is logically 512x192 RGBA4444; binding its
non-power-of-two height directly to the PSP Graphics Engine caused invalid sampling in
the lower glyph rows. The renderer now allocates a transparent 512x256 EDRAM surface,
copies the unchanged swizzled 512x192 package into it, and binds the power-of-two
storage dimensions. UVs and package bytes remain unchanged.

The HUD allocation therefore rises from 196,608 to 262,144 bytes. Combined Link
textures plus HUD use 665,728 bytes, below the existing 1,150,000-byte texture budget.
The same storage helper is used by the standalone startup renderer; its existing
512x512 atlas allocation is unchanged.

## Final PPSSPP evidence

Final request: `startup-intro-v4`

- PPSSPP 1.20.4, OpenGL, hardware PSP renderer;
- classification: `MARKERS_VALID_METRICS_VALID`;
- result code: `0`;
- duration: 15,663 ms;
- route marker: `DUSKLIGHT_PSP_STARTUP_ROUTE_CAPTURE_OK`;
- ten RGB565 route captures;
- route: `team_logo,fsp102_scene,title_logo,title_prompt,file_select,player_name,horse_name,intro_wide,intro_closeup,fsp108`;
- Allegrex EBOOT SHA-256:
  `d88e29b0bb06192da9d49ae9bc8c3f02500892f0ae21702dfc4e1384ac291f4e`;
- parity build ID:
  `sha256:af718181132508dab42a45972ec04d1691ac21101dfa74a21c1dcae2e002826b`;
- visual build ID:
  `sha256:1fc2c23540541ea61139c62c05fa2d8ac50c405b32f2eff4044d44b998c7158a`;
- source commit embedded in the build:
  `b3e63265c34fe71c1c6afca182cfd453ee9a5edc`.

Local converted evidence is under `artifacts/validation/startup-intro-v4/`. The wide
and close-up captures show readable Rodan text; the gameplay capture proves HUD
restoration and a clean handoff. These ignored images and all commercial-derived
packages remain local.

## Validation

- startup runtime/name-entry/intro/UI host suite: pass;
- full PSP compile and link: pass;
- ten-stage PPSSPP route marker and metrics: pass;
- visual inspection of wide, close-up and gameplay captures: pass;
- broker cleanup: `BROKER_NOT_BOOTSTRAPPED`;
- no ROM, extracted package, save or captured commercial framebuffer committed.

## Next fidelity slices

The largest visible startup gap is still the F_SP102 `demo38`/animated-title
composition. Exact `demo01_01` parity requires Rusl, source event staging and cutscene
animation. Name persistence and the full source name-entry BLO also remain open.
