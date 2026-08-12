# UI pane parity evidence boundary

## Result

P4.4 classification: `BLOCKED_LOCAL_UI_SCREEN_EVIDENCE`.

The structural contracts pass for all 14 startup/HUD surfaces: DPST timing,
DPSU anchors, title model origins, title camera, file-select cursor, DPUI v2
records and New Game handoff. The current Functional boot-logos and title-flow
runs also validate their markers and metrics.

That evidence is insufficient for screen-space parity. Both current broker
responses report:

```text
capture_files_collected=0
trace_files_collected=0
```

There is therefore no current PSP pane DTRC, framebuffer capture, screen
landmark set or overlay to compare with the desktop oracle. The six existing
`PARTIAL_PARITY` rows remain partial; no pixel or pane-screen `MATCH` is
claimed.

## Available evidence

- startup/UI host surfaces: 14/14 structurally valid;
- DPUI v2: 31 records, 20 original assets, deterministic visible output;
- model and pane recentering: none;
- current Functional startup behavior: PASS;
- source pivot/anchor policy: `MATCH_WITH_TOLERANCE` locally.

## Missing evidence

- bounded startup/HUD `ui_pane_transform` DTRC events on PSP;
- corresponding desktop pane events;
- current framebuffer captures and screen landmarks;
- overlay/heatmap comparison for title logo, prompt, file cursor, HUD and
  pause panes.

This is a local instrumentation dependency, not a broker outage. P4.5 and
other independent work may continue. Network and fabricated captures: none.
