# Current PSP conservative validation

## Result

P5.3 classification: `CURRENT_PSP_CONSERVATIVE_SCOPE_VALID`.

The isolated broker completed the five benchmark v1 scenes and both startup v2
segments with the current EBOOT. Every run observed PSP boot, validated its
markers and metrics, and exited without a PSP runtime or host graphics error.

## Effective profile

- graphics backend: OpenGL;
- PSP renderer: hardware;
- native resolution: 480x272;
- configured CPU clock: 222 MHz;
- fast memory: disabled;
- conservative settings: effective;
- network: unused.

Startup v2 reports these settings explicitly. Its boot-logos segment measures
60.066 FPS average with a 16,681 us p95 and 1,359,872 bytes peak EDRAM. The
title-flow segment measures 59.937 FPS average with a 16,682 us p95, 24 draws
and 1,581,056 bytes peak EDRAM. New Game reaches source stage F_SP108.

The five v1 metrics validate as current and complete. Their numerical timing is
identical to the Performance profile despite a different backend/configuration;
because v1 labels itself diagnostic and PPSSPP is not physical PSP timing, this
is recorded as an emulator limitation rather than a hardware-performance
claim.

Build identity:
`sha256:5d2f240e28ae54c5e1d493c7ec5ab1e0809f9c400dc98f1f795ee96d191b3cff`.

Real PSP validation and human visual acceptance remain pending.
