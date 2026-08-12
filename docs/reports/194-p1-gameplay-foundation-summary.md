# 194 — P1 gameplay foundation summary

Classification: `P1_GAMEPLAY_FOUNDATION_RECONSTRUCTED`

This report consolidates the gameplay-first direction adopted after the earlier visual-parity campaign. The PSP port now prioritizes an end-to-end playable game over expensive renderer parity.

Proven/implemented in prior P1 workspaces included room transition/handoff architecture, original door actor/event work, original chest actor/event work, item lifecycle separation and source Link event-animation work. Some intermediate Git object history was lost to workspace remounts; source/report evidence that survived is preserved in the reconstructed repository.

Graphics policy for P1: conservative unlit/simple blend/alpha/particles; advanced lighting/post-processing is deferred unless required for gameplay comprehension.

Next closure target: full source-driven treasure lifecycle with visible Heart Piece, deferred commit and persistence.
