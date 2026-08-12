# Startup and file-select Functional parity refresh

## Result

P4.2 classification: `FUNCTIONAL_PARITY_WITH_DOCUMENTED_DIFFERENCES`.

The current PSP build completed both bounded startup v2 Functional scenes
through the isolated GUI broker:

- `benchmark_boot_logos_v2`: warning/Nintendo/Dolby timeline reached the
  opening-stage load boundary;
- `benchmark_title_v2`: title, START acceptance, file select and New Game
  reached `new_game_transition` in the same EBOOT;
- New Game selected `F_SP108/R01/start21`, instantiated all nine essential
  source records and reported coverage `1.000`.

Both runs used OpenGL plus the software renderer, observed PSP boot, validated
all requested markers and metrics, used no fallback, and ended with
`error_code=0` and no PSP runtime error.

## Desktop/PSP comparison

The immutable desktop oracle and current host evaluators support:

| Surface | Classification |
|---|---|
| startup segment order and source timers | `MATCH_WITH_TOLERANCE` |
| F_SP102 title camera checkpoints 0/900/1800 | `MATCH_WITH_TOLERANCE` |
| START gate and scripted file cursor | `MATCH_WITH_TOLERANCE` |
| New Game destination and same-EBOOT handoff | `MATCH` |
| complete opening event runtime/audio | `PARTIAL_PARITY` |
| free name entry and full J2D file-select layout | `EXPECTED_PLATFORM_DIFFERENCE` / `PARTIAL_PARITY` |

This closes P4.2 behaviorally. It does not claim pixel parity or pane-level
desktop/PSP DTRC; captures, screen landmarks and pane transforms remain P4.4.

## Validation

- startup host package/runtime/camera/UI/title/first-playable suite: PASS;
- DPUI v2 host test: PASS, 31 records and 20 original assets;
- Functional boot logos: PASS;
- Functional title/file-select/New Game: PASS;
- startup parity v2 validator: PASS, camera
  `MATCH_WITH_TOLERANCE`, New Game `MATCH`, F_SP108 essential actors 9/9;
- parity build identity:
  `sha256:5d2f240e28ae54c5e1d493c7ec5ab1e0809f9c400dc98f1f795ee96d191b3cff`;
- network and fabricated traces: none.
