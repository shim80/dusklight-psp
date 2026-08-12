# P1 GETAWAIT Heart Piece checkpoint

Date: 2026-08-12
Owner/publication identity: `shim80`

## Purpose

Close the long-running ambiguity around why the Heart Piece was logically present during `011get_item` but not clearly visible in the PSP gameplay frame. This checkpoint separates renderer submission, source camera geometry, Link item-get pose, Demo_Item placement and acquisition timing into independently testable layers.

## Source facts recovered

- Link GETA animation resource: `0x169`, 30 frames, 35 joints, recovered from `AlAnm.arc`.
- Link GETAWAIT animation resource: `0x16A`, 30 frames, 35 joints, recovered from `AlAnm.arc`.
- Heart Piece archive/model: `O_gD_hutk.arc` / `o_gd_hutk.bmd`, model resource `0x0008`.
- Source Heart Piece geometry used by the PSP proof: 484 triangles.
- Heart Piece item number: `0x21`.
- Large chest Link setup follows the human branch of `procCoOpenTreasureInit()`: Link faces `partnerYaw + 0x8000` and is positioned 111 units from the chest.
- Demo_Item placement uses Link left-foot joint 21 and source offset `(0,115,54)` rotated by Link yaw.
- `DEFAULT_TREASURE_NORMAL` item acquisition semantics are deferred: create != acquire; show occurs during GETA; GETAWAIT/message precedes `dead()`; inventory commit follows actor death through `execItemGet()`.

## Renderer isolation

A dedicated PPSSPP third-model probe rendered the same real-room path with two static actor models and then three. Adding the third model increased actor draw calls by six and changed 1,403 displayed pixels. This closed the hypothesis that `render_real_room_frame_with_models()` ignored the third model.

Conclusion: generic third-model submission is not the current blocker and should not be reopened without new contradictory evidence.

## Camera isolation

A source GETITEM camera helper reproduced the human type-3 path selected from event Type=2, including the 17-frame transition, source relative positions and side selection. Independent projection showed that the source-derived Heart Piece position can lie inside the PSP viewport. The camera should therefore not be adjusted by eye to compensate for a missing item pose.

## Root cause and visible closure

The earlier visibility probe still rendered Link in ordinary Idle while placing the item according to source Demo_Item rules. Framebuffer comparison showed zero Heart Piece pixels in that state: the item was fully occluded. Replacing Idle with the real GETAWAIT source pose changed Link geometry enough for the source-positioned item to become visible.

The successful PPSSPP proof used:

- real R02 source-derived chest placement;
- real GETAWAIT resource `0x16A`;
- source-derived Link/world yaw;
- joint-21 item placement;
- source Heart Piece geometry;
- source item-get camera side/frame search;
- a real Allegrex EBOOT executed by PPSSPP.

Recorded successful run values from the campaign:

```text
DUSKLIGHT_PSP_GETAWAIT_HEART_VISIBLE_OK
source_clip=GETAWAIT
resource_id=0x16A
source_bmd=o_gd_hutk.bmd
source_triangles=484
chest=1300.000,62.500,-3012.500
link=1300.000,62.500,-2901.500
yaw_s16=-32768
best_frame=21
camera_side=1
left_foot_y=67.062
item=1300.000,182.062,-2955.500
pixel_differences=491
diff_bounds=17,12,295,134
```

Proof EBOOT SHA-256 recorded in the campaign:

`d6d5602e845e1884a94aec034b1682157d0eca984ca6e4945ad473184e9f3499`

## What this does not close

This proof is not by itself the complete chest lifecycle. It establishes the source-correct visual presentation ingredients. The next gameplay checkpoint must put them back under event ownership rather than a probe selecting GETAWAIT/frame/camera state directly.

Still open:

- full `interaction -> DEFAULT_TREASURE_NORMAL -> BOXOP -> GETA -> GETAWAIT` automatic cut ownership in one gameplay run;
- visible item message UI driven by the source event/message system;
- exact one-time `dead() -> execItemGet()` commit timing in that same visible run;
- chest/treasure persistence after actor/room recreation in that integrated run;
- clean return from item-get animation to ordinary controllable locomotion;
- source Heart Piece BCK/BRK/TEV visual channels; the preserved PSP proof intentionally allows simplified unlit material treatment.

## Next action

Do not make another standalone visibility probe unless integration produces contradictory evidence. Integrate the validated GETA/GETAWAIT animation resource path and Demo_Item placement into the source treasure-event runtime, then validate the entire stateful chest sequence in one PPSSPP run with logical markers and one genuine gameplay screenshot.
