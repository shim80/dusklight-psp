# Startup model and UI pivot refresh

## Result

Classification: `STARTUP_UI_LOCAL_PIVOTS_CLOSED`.

The canonical title-room and title-logo DPRM packages preserve their source
model origins. Startup DPSU and HUD DPUI records preserve pane anchors/pivots;
no model bounds or pane bounds is used as a runtime pivot. No production
correction is selected.

| Surface | Evidence | Local classification |
|---|---|---|
| title room | 24,799 DPRM vertices; stored bounds equal computed bounds; room model identity | `MATCH_WITH_TOLERANCE` |
| title logo | 24 DPRM vertices; stored bounds equal computed bounds; explicit identity instance matrix | `MATCH_WITH_TOLERANCE` |
| warning/Nintendo/Dolby | DPSU pane anchors, valid package | `MATCH_WITH_TOLERANCE` |
| title prompt/file select/cursor | DPSU pane anchors, valid packages | `MATCH_WITH_TOLERANCE` locally |
| HUD/pause | DPUI v2, 31 records and 20 original assets | `MATCH_WITH_TOLERANCE` locally |

The title room bounds are large because they remain in the original F_SP102
model coordinate system; they are metadata, not evidence for recentering. The
title-logo model retains its asymmetric source Y bounds and is likewise not
centered at its AABB midpoint.

## Validation

- startup runtime, profiler, title camera, DPSU, title assets and first
  playable host tests: PASS;
- DPUI v2 host render: PASS, 31 records, 20 assets, 19,411 visible pixels;
- startup/UI matrix: 14 surfaces, six intentionally `PARTIAL_PARITY`;
- Allegrex resource, startup runtime and renderer libraries: PASS;
- production sources or packages changed: none;
- PPSSPP acquisitions and network: none.

The remaining `PARTIAL_PARITY` rows are not pivot failures. They require
Functional captures, screen landmarks, pane traces, or behavior not yet
implemented (full opening event/audio and free name entry). P4.4 is therefore
unblocked for evidence acquisition, but no global desktop/PSP `MATCH` is
claimed by this local audit.
