# Alpha regression and first EBOOT release gate

Date: 2026-08-13

## Asset-backed gameplay status

The authorized workspace asset bundle now produces real visible gameplay in pinned PPSSPP 1.20.4 using the canonical PSP EBOOT. The accepted capture is committed at `screenshot/dusklight-psp-f-sp108-gameplay.jpg` and shows Link inside `F_SP108 / R01 / start 21`.

Analog locomotion was exercised after the first visible frame and visibly changed Link's runtime pose/orientation. Full camera/action/pause acceptance remains open.

## Alpha-texture regression

The accepted F_SP108 capture also exposes a renderer regression: background foliage/plants with cutout textures are being rendered as opaque geometry instead of respecting their alpha-tested coverage.

Tracked as issue #6: `Restore alpha-tested foliage rendering in F_SP108`.

This is a targeted renderer correctness bug, not a request for broad visual polish. Acceptance requires a new asset-backed F_SP108 screenshot where foliage cutouts are correct without regressing opaque room geometry or Link.

## Mandatory first public EBOOT gate

The first public EBOOT must be published as a GitHub Release asset immediately once one EBOOT proves the complete user-facing milestone:

`cinematic/opening -> start/title menu -> save UI menu -> create/load persistent slot -> gameplay in F_SP108`

The release must also retain PSP-adapted controls sufficient for user testing.

Do not treat a GitHub Actions artifact as the public release. The user-facing EBOOT must be attached to a GitHub Release.

Do not publish before the complete chain above works in one EBOOT. Once it does, publishing that EBOOT is part of closing the milestone, not a later optional task.

## Immediate order of work

1. keep GitHub updated before moving to each new milestone;
2. fix/verify issue #6 alpha-tested foliage regression;
3. finish deterministic asset-backed controls acceptance;
4. replace the synthetic startup fixture with the packaged source-faithful cinematic/title/save assets already present in the workspace;
5. replay the full chain in one EBOOT;
6. publish that EBOOT as a GitHub Release asset as soon as the milestone passes.

Commercial-derived assets remain local and must never be committed or attached to the release.
