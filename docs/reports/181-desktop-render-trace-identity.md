# Desktop render-trace identity repair

## Result

The first externally inspected four-frame acquisition was rejected because its
stable tuple collapsed multiple source models under one actor. The trace was
not used as V1 evidence and no PSP renderer change followed from it.

The generic repair keeps packet ownership for the bounded capture lifetime,
assigns a stable ordinal to each real `J3DModel`, and emits per-frame
`submission_id` and `draw_order` fields. It adds no GX call, flush, reorder, or
scenario-specific branch.

## Evidence

Request `visual-v1-fsp108-stable-model-identity` produced:

- four balanced frame begin/end pairs;
- 2,441 events and 233 real render submissions;
- zero actor/profile sentinels;
- zero model sentinels;
- 15 stable model identities;
- identical 60-submission identity sequences in the last three complete
  steady-state frames;
- zero overflow and zero dropped event;
- trace SHA-256
  `21560968f1657882db3fccb5699ad311a6b60b31bd120cc112dcf21322c437c8`.

The first captured frame contains 53 submissions because capture begins at the
first real shape binding. This is expected; frames 92–94 are the three complete
steady-state frames.

## Remaining V1 gate

V1 is not yet closed. The stable trace-local actor/model/material/shape tuple is
now trustworthy, but the required cross-platform source table/record identity,
projection data, and screen bounds are not yet present. A same-scene PSP trace
also remains to be acquired. V3 therefore remains untouched.
