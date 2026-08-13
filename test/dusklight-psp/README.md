# Reconstructed canonical PSP entry target

The original historical `test/dusklight-psp` launcher referenced by the canonical build/package scripts is not present in the reconstructed public Git history.

This directory intentionally restores the **canonical build target and packaging boundary**, not a claim of byte-for-byte historical recovery.

## Current entry path

`main.cpp` is now a minimal PSP bootstrap that calls `dusk::psp::game::run_canonical_game()`.

The canonical driver owns the persistent startup/save handoff and, for the current first-playable checkpoint, accepts exactly:

```text
F_SP108 / room 1 / start 21 / layer 0
```

A successful handoff requires these local, derived and untracked packages:

```text
data/common/link.dpsk
data/common/link.dptx
data/common/link.dpan
data/common/hud.dpui
data/stages/F_SP108/R01/room.dprm
data/stages/F_SP108/R01/room.dptx
data/stages/F_SP108/R01/room.dpcl
data/stages/F_SP108/R01/room.dpsc
```

The runtime validates the room/Link packages, requires the preserved 599-source-actor scene contract, start point 21, nine essential instantiated actors, a consistent `RealRoomRuntime`, and a valid Link animation/skin runtime before rendering.

The first-playable renderer intentionally stays conservative:

- opaque room + Link path;
- known-good unlit profile;
- lighting off;
- fog off;
- shadows off.

## Authorized local first-playable run

With the derived assets present under `build/assets/dusklight-psp/data`, run:

```sh
bash scripts/run-canonical-first-playable.sh
```

The script:

1. bootstraps the pinned PSPDEV/PPSSPP tools only if needed;
2. validates the exact eight-file first-playable asset boundary;
3. builds the canonical Allegrex target;
4. packages the EBOOT and local data tree;
5. launches PPSSPP from the packaged game directory;
6. records `first-playable-ppsspp.log`;
7. after PPSSPP exits, fails unless the game emitted the first-render proof marker.

The required marker is:

```text
DUSKLIGHT_PSP_FIRST_PLAYABLE_RENDER_OK stage=F_SP108 room=1 start=21 actors=9 render=opaque_unlit frame=1
```

That marker is emitted only after a successful `render_real_room_frame()` following valid room, actor and Link updates. Public CI deliberately has no commercial-derived assets and is required to **not** emit this marker.

A marker alone is not a visual proof. The first-playable checkpoint is accepted only with both the marker and a real asset-backed gameplay framebuffer showing F_SP108 + Link. Technical boot captures and synthetic/public probe screens do not count as project gameplay screenshots.

Commercial-derived packages remain local and untracked.
