# Geyser behavioral parity refresh

## Result

Classification remains:

```text
implementation=PROCEDURAL_FALLBACK
logic=PARTIAL_PARITY
representation=EXPECTED_PLATFORM_DIFFERENCE
overall=PARTIAL_PARITY
```

The two historical F_SP110/R02 records preserve identity, parameters and a
bounded PSP state-machine translation. The original `daObjGeyser_c` is not
compiled, and its BMD, MoveBG collision, JPA emitters, point wind and audio are
not represented by their source implementations.

## Behavioral evidence

- both reactive and periodic parameter forms are decoded;
- off, warning/on-wait, on and disappear transitions are exercised;
- pause, reset and delete remain stable;
- player-volume contacts and bounded impulse behavior are tested;
- allocation-free update, capacity and deterministic particle budgets pass.

This local behavior is not aligned to desktop action ticks, capsule geometry,
or impulse values. It therefore remains `PARTIAL_PARITY`.

## Visual evidence

The PSP column and particles are a deliberate procedural representation. They
remain `EXPECTED_PLATFORM_DIFFERENCE` and provide no evidence for source BMD,
MoveBG, JPA, wind, lighting, or audio parity.

## Validation

- real actor runtime host: PASS for v1/v2, two actors, 96 particles, zero
  allocations and 20 negative cases;
- source and PSP state-symbol audit: PASS;
- Allegrex actor/renderer libraries: PASS;
- original geyser source compiled: no;
- PPSSPP acquisition, network, production changes: none.

P3.5 is complete as a behavioral/representation classification. Native
scenario DTRC and eventual visual review remain P4/P5 evidence tasks; the
procedural fallback must never be relabeled as full source parity.
