# Desktop render-state trace V1

## Result

The source-derived desktop instrumentation is implemented and validated in
commit `6b697b2`. It is neutral: it observes real `J3DMatPacket` submissions,
does not add a GX call, does not flush, and does not alter draw-buffer order.

The trace preserves actor identity at packet registration and emits the final
material and shape identity at actual draw time. It records frame, pass,
bucket, submission, texture, depth, alpha-test, blend, cull, fog and lighting
state. Capture starts on the first real shape and is bounded to four frames,
8192 shape bindings per frame and 128 draw buffers per frame.

## Validation

- reproducible patch application and reverse-application: PASS;
- patch whitespace check: PASS;
- host JSONL contract test: PASS, 15 event kinds, one fixture frame;
- targeted compilation of `J3DPacket.cpp`, `J3DDrawBuffer.cpp`,
  `m_Do_graphic.cpp` and `f_op_actor.cpp`: PASS;
- desktop application link against the already pinned local dependencies:
  PASS;
- network downloads: zero.

The ordinary aggregate build attempted to refresh the Cargo index because its
local sparse index lacks the `aes` entry. It was stopped, then the affected C++
units and final link were validated directly against the existing pinned
`libnod.a`. No dependency was downloaded or replaced.

## GUI acquisition

The isolated `launchservices_gui` attempt for `F_SP108,1,13` returned macOS
error `-10810` before a Dusklight process was observed. Therefore no desktop
render trace is claimed and no oracle file was changed. The acquisition node is
classified `PENDING_GUI_EXECUTION`; V1 is `SUSPENDED_INFRASTRUCTURE`, while V2
continues independently.
