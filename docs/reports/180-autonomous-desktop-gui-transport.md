# Autonomous desktop GUI transport

## Result

Classification: `READY_AUTONOMOUS_DUSKLIGHT_DESKTOP_GUI_TRANSPORT`.

The existing repository LaunchAgent now accepts exactly two application types:
the pinned PPSSPP build and the exact pinned Dusklight desktop reference. A
fresh adapter validates the bundle, executable, source lock, trace patch, local
game-image identity, isolated profile, outputs, scenario, and timeout. It then
launches `Dusklight.app/Contents/MacOS/Dusklight` directly in the Aqua worker;
LaunchServices is not used.

The historical PSP core smoke remained valid after the protocol upgrade. Its
marker still contains exactly `DUSKLIGHT_PSP_CORE_OK`.

## D0–D9

The bundle and request contract passed static validation. Vanilla and trace
builds initialized a window, Dawn, Metal, and disc `GZ2P01`. The startup trace,
direct F_SP108 reach, one-frame trace, four-frame transport trace, artifact-only
recollection, invalid executable-hash rejection, and valid request after the
rejection all behaved as specified.

The supervisor PID remained stable across the accepted D1–D9 evidence. No
manual restart or user confirmation was used. The invalid request was rejected
as `HOST_DESKTOP_EXECUTABLE_INVALID` before process creation.

## Build correction

The first render-trace selftest exposed a stale local
`libJSystem_J3DGraphBase.a`: its instrumented objects were current, but the
archive linked into the app predated them. The local pinned archive and final
application were rebuilt. No dependency was downloaded. One aggregate build
attempt asked Cargo to refresh its index and was stopped by the disabled
network; the already pinned local `libnod.a` was then reused.

## Render-trace transport evidence

Request `visual-v1-fsp108-four-frame-fixed` reached F_SP108 room 1, spawn 21,
layer 13 and produced four closed frames, 2,441 render-state events, no
overflow, no dropped events, and trace SHA-256
`80a589a3dba684c67b285ebc839075a055cfeb8ccc2b2e6490577ad030a207c9`.
Artifact-only recollection validated the same evidence without relaunching the
application.

This closes the GUI transport only. It does not close V1: 180 of 233 draw
submissions still carry the explicit unowned-actor sentinel and the desktop
schema does not yet expose every source/model identity required for a faithful
V1/V2 join. V1 therefore remains locally blocked on render-trace identity; V3
remains waiting and no depth state has been changed.
