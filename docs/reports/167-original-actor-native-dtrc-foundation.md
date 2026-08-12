# Original actor native DTRC foundation

## Result

P4.1 has a first executable native trace seam for the revealing
`d_mn10_r09_actors` scenario. It observes the three real original-source
processes present in R09:

- `daLv4HsTarget_c` (`0x009F`);
- `daObjLv4Gear_c` (`0x0183`);
- `daTagPoFire_c` (`0x017A`).

The process manager now retains the DPSC source table hash, record index, name
hash, room and layer supplied at creation. A bounded read-only observation API
exposes those identities together with the real actor base state and lifecycle
metrics. The DTRC writer emits `actor_transform` and `actor_state` only for the
new actor scenario; the ten Link scenario configurations remain unchanged.

## Evidence boundary

This instrumentation now has one real PSP acquisition, but no desktop actor
event has yet been aligned. Consequently all eight actor rows remain
`PARTIAL_PARITY` and none becomes `MATCH`.

The trace values are sampled from live Allegrex objects after their real
create/execute/draw callbacks. Source identities come from DPSC records. Host
test output is not serialized or promoted as PSP evidence, and
`fabricated_traces=0`.

## Validation

- process observation metadata and lifecycle counters: PASS;
- original actor parity host matrix: PASS for eight original sources and 18
  canonical placements;
- DTRC v3.1 contract and negative cases: PASS;
- canonical Allegrex EBOOT: PASS;
- GUI runner plan for `d_mn10_r09_actors`: PASS with isolated profile,
  OpenGL and canonical GUI transport;
- PPSSPP Functional acquisition: PASS through broker generation 2, OpenGL
  software renderer and the isolated profile;
- build identity:
  `sha256:851dac828f0d79fd1d98fb3598543ab032adc65b38f6beebd70fbfedc96e6b53`;
- DTRC: 8,704 events over 300 ticks, zero dropped events;
- original actor DTRC: 3,000 events from five live instances: four
  `daObjLv4Gear_c` records and one `daLv4HsTarget_c` record;
- marker/metrics classification: `MARKERS_VALID_METRICS_VALID`;
- boot observed, PSP runtime error false, allocations during Playing zero;
- network: none.

The `daTagPoFire_c` process is not live at `frame_present`; its lifecycle must
be captured at the process callback boundary rather than inferred from an
absent frame sample. Next, add bounded lifecycle records and prepare R02 actor
coverage before another acquisition.
