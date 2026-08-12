# 197 — GETAWAIT Heart Piece visibility

Classification: `P1_GETAWAIT_HEART_VISIBLE`

The preserved PSP harness is `test/getawait-heart-probe/`.

Evidence established during PPSSPP testing:

- Link source clip GETAWAIT, resource `0x16A`;
- source animation 30 frames / 35 joints;
- Heart Piece source BMD identity `o_gd_hutk.bmd`;
- PSP proof mesh 484 triangles;
- actual R02 Heart Piece chest selected from the scene actor table;
- source-derived large-chest Link placement;
- item placement from Link joint 21 plus Demo_Item offset;
- framebuffer differences prove actual item visibility.

This closes the earlier hypothesis that the generic renderer could not submit a third actor model. The next task is event integration, not another static rendering workaround.
