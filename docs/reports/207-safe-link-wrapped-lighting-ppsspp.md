# Safe Link wrapped lighting — PPSSPP framebuffer checkpoint

Date: 2026-08-16

## Scope and source evidence

This checkpoint changes Link only. It preserves the accepted F_SP108 room,
alpha-test foliage, alpha-blended water/foam, source fog and bounded MPV1 material
passes.

The audit confirmed that all 3,543 Link normals are skinned and normalized in model
space. The F_SP108 key-light ray is converted from world space with the inverse Link
yaw before evaluation. The earlier `SourceApprox` failure was not a normal-transform
failure: its multiplication of the very dark source material ambient values by the
environment magnitudes collapsed the final texture modulation almost to black.

`SourceApprox` remains available only as a diagnostic and is not selected by the
candidate game profile.

## Accepted approximation

The candidate profile now uses a bounded CPU/vertex-color model:

```text
ambient = normalized_environment_ambient_chroma * 0.58
wrapped = clamp((dot(N, L) + 0.35) / 1.35, 0, 1)
key = normalized_environment_key_chroma * wrapped * 0.32
illumination = max(ambient + key, 0.52)
output = emissive + source_base * illumination
```

The texture remains source-authored and is modulated by the resulting PSP vertex
color. A 64-level per-material lookup table bounds repeated channel work; the 27 Link
material colors are prepared without per-frame heap allocation. The LUT is invalidated
when the renderer is initialized or shut down and is keyed by package identity,
environment colors and lighting variant.

The runtime-only file `DUSKLIGHT.LINK.SHADING` accepts `baseline`, `ambient`,
`wrapped` and `final` for identical-route comparisons. It is absent in the normal
package, where `wrapped` is the default. No debug UI was added.

## PPSSPP A/B results

All requests used the same Allegrex EBOOT, OpenGL backend, hardware PSP renderer,
F_SP108 camera and six-stage startup route. Each request produced valid route markers
and metrics.

| Variant | Vertex luminance min / mean / max | Link visible | PPSSPP cold CPU cost | Decision |
| --- | --- | --- | --- | --- |
| unlit baseline | not applicable | yes | 0 us | preserved as runtime fallback |
| ambient only | 0.5349 / 0.5349 / 0.5349 | yes | 29,725 us before fast-path optimization | rejected: safe but flat |
| wrapped diffuse, unoptimized | 0.5349 / 0.6273 / 0.7855 | yes | 29,804 us | visual model accepted, implementation rejected |
| wrapped + subtle rim | 0.5349 / 0.6545 / 0.8278 | yes | 8,196 us after material preparation | rejected: gain too small for per-vertex view normalization |
| wrapped diffuse, accepted LUT path | 0.5349 / 0.6273 / 0.7863 | yes | 4,339 us | accepted |

The timing is PPSSPP instrumentation from the first gameplay frame, including LUT
construction and the interleaved-vertex copy/cache writeback. It is not a real-PSP FPS
claim. Physical-hardware timing remains required.

Local, ignored framebuffer evidence:

- baseline: `artifacts/validation/link-shading/baseline/startup-gameplay.png`;
- accepted: `artifacts/validation/link-shading/wrapped-v3/startup-gameplay.png`;
- rejected rim: `artifacts/validation/link-shading/final/startup-gameplay.png`.

The accepted image adds readable directional variation to Link without a black
silhouette. Environment pixels, foliage, water, fog and HUD are unchanged at the fixed
camera. There is no exact Dusklight desktop framebuffer at this camera in the current
artifact set, so this is an accepted PSP baseline improvement, not a measured PC-pixel
parity claim.

## Validation

- safe lighting and coordinate-space host contract: pass;
- render profile host contract: pass;
- 14-setup Link lighting preparation matrix: pass;
- first-playable control semantics: pass;
- MPV1 two-pass material contract: pass;
- F_SP108/Link source-provenance checks: pass;
- `git diff --check`: pass;
- full PSP build and EBOOT generation: pass;
- ELF identity: 32-bit little-endian MIPS executable, MIPS-II/Allegrex toolchain;
- complete route captures: team logo, title logo, title prompt, file select,
  F_SP102 scene and F_SP108 gameplay;
- EBOOT SHA-256: `454d09c8fe003f9ecb95d65842a7f2f59bc8f6a1544bc5e266a2ebbb9fd80a03`.

The legacy camera-yaw and move-speed causal scripts still return their documented
nonzero mismatch/conflation status because their historical desktop trace gates are not
closed. The independent first-playable control-state test passes; this checkpoint does
not claim a fresh physical-input PPSSPP control campaign.

## Deliberately unchanged and next task

No MPV1/package revision, BPK/BRK/BTK runtime, water UV animation, bloom, global
composite, vignette or glare was added here. MPV1 currently has no UV-transform or
animation binding. Hard-coding F_SP108 material IDs in the renderer was rejected.

The highest-value next rendering task is an append-only, bounds-checked compact
material-animation section generated from source BTK data (with an explicitly labelled
fallback only where extraction is impossible), followed by an identical-camera
F_SP108 water-motion capture. It must preserve the accepted alpha second pass and the
two-regular-pass ceiling.
