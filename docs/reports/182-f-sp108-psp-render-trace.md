# F_SP108 PSP render-state trace

## Result

The canonical runtime now exposes `f_sp108_first_playable` as a render-only
parity scenario. It reuses the already validated first-playable loader and
renderer, emits no fabricated D_MN10 Link events, derives the scene identity
from the active stage, and stops after the four required frames.

The first request (`20260802T093147Z-parity_trace-1`) observed PSP boot but
failed before `CANONICAL_GAME.READY` with `error_code=120`. The general room
transition path cannot activate F_SP108 and was the wrong route. No renderer
defect was inferred from that failure.

After routing the scenario through the existing F_SP108 path, request
`20260802T093628Z-parity_trace-1` passed with:

- classification `MARKERS_VALID_METRICS_VALID`;
- OpenGL plus PSP software renderer;
- parity build
  `sha256:f8085e3044d14fb5ab3d3d826cd124e5cada9607ddc0b54d8cd2005b45fe33a4`;
- visual build
  `sha256:3fef2b38baa090571e6fcb0713a86c302665fa24ac3e0d9ce354c747c56d7320`;
- four frames and 196 render submissions;
- 88 room submissions and 108 Link submissions;
- 176 opaque and 20 alpha-blend submissions;
- zero dropped events, scenario assists, and transitions;
- trace SHA-256
  `4c94dbfbe25669f7ea678b43846cfdefcf469c36b20ff7627175d713480e638f`.

This creates the required same-scene evidence pair. It does not by itself
close V1 or unlock V3: the source-derived desktop-to-PSP identity join still
has to be proven without draw-order or spatial heuristics.
