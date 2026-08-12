# 196 — Chest / Demo_Item lifecycle

Classification: `P1_TREASURE_LIFECYCLE_TARGET`

The source behavior makes an important distinction that must remain true on PSP: creating the item partner is not equivalent to granting the inventory item.

Required order:

1. TBOX starts `DEFAULT_TREASURE_NORMAL`.
2. Link opens the chest with the source BOXOP path.
3. a Demo_Item partner exists hidden.
4. GETA advances the item presentation.
5. the item becomes visible near the end of GETA.
6. GETAWAIT holds the presentation while the message is active.
7. message completion causes the partner to be killed.
8. Demo_Item event action commits `execItemGet()` exactly once.
9. treasure state remains persistent after actor/room recreation.
10. Link returns to ordinary locomotion with no stale event-root pose.

Any implementation that increments inventory at item creation is incorrect.
