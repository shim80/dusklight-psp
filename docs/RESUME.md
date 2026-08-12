# Exact resume protocol

This file is designed for a fresh agent/session with no conversational memory.

## 1. Read first

Read, in order:

1. `/AGENTS.md`
2. `/docs/STATUS.md`
3. `/docs/COMMIT_LEDGER.md`
4. the newest reports in `/docs/reports/`
5. `/test/getawait-heart-probe/main.cpp`
6. the PSP runtime functions referenced by that probe, especially `apply_source_animation_resource_and_skin()`.

Do not infer that a historical report's Git SHA exists in the reconstructed repository. The historical ledger distinguishes provenance from reconstructed commits.

## 2. Required local dependencies

Bring your own legally obtained Twilight Princess game image when original assets must be regenerated. Never commit it.

Install/configure PSPSDK/pspdev and PPSSPP. Set:

```sh
export PSPDEV=/path/to/pspdev
export PSPSDK="$PSPDEV/psp/sdk"
export PATH="$PSPDEV/bin:$PSPSDK/bin:$PATH"
```

## 3. Build sanity

Start with host/canonical tests that cover the code you are changing, then configure/build the PSP probe or gameplay target.

Current probe build:

```sh
psp-cmake -S test/getawait-heart-probe \
  -B build/psp/getawait-heart-probe \
  -G Ninja -DCMAKE_BUILD_TYPE=RelWithDebInfo
cmake --build build/psp/getawait-heart-probe -j 4
```

## 4. Asset contract for GETAWAIT Heart Piece proof

The harness expects local, untracked derived packages next to its EBOOT/memstick setup:

- Link DPSK/DPTX;
- DPAN containing `GETAWAIT` resource `0x16A` (and, for full integration, `GETA` `0x169` and BOXOP);
- R02 DPRM/DPTX/DPSC;
- Heart Piece DPRM/DPTX derived from `O_gD_hutk/o_gd_hutk.bmd`;
- HUD DPUI.

Known source identities:

- `AlAnm.arc` / GETA resource `0x169`, 30 frames, 35 joints;
- `AlAnm.arc` / GETAWAIT resource `0x16A`, 30 frames, 35 joints;
- `O_gD_hutk.arc` / BMD resource `0x0008`, `o_gd_hutk.bmd`;
- Heart Piece item number `0x21`;
- TBOX name hash in the preserved R02 package: `0x2A0E83C6`.

Do not commit these extracted commercial files.

## 5. What the probe proved

The renderer can submit additional static models; do not reopen that as the default hypothesis. The successful GETAWAIT proof required the real Link item-get pose and source-derived Demo_Item placement. Use that evidence to integrate the event; do not replace GETAWAIT with Idle or move the Heart Piece by eye.

## 6. Next implementation target

Restore/complete the source-event treasure runtime so that the harness no longer chooses an item presentation frame manually. Event ownership should advance the Link cuts and Demo_Item lifecycle.

Critical semantics:

- item creation is not acquisition;
- item is hidden on creation;
- Link's GETA/GETAWAIT controls presentation;
- item becomes visible late in GETA;
- message acknowledgement controls the end of item presentation;
- `dead()` precedes `execItemGet()`/commit;
- treasure state persists after actor/room recreation;
- return to ordinary locomotion must clear event animation/root history.

## 7. Definition of done for the next checkpoint

All must be true in one PPSSPP run:

- original chest is visible closed;
- OPEN interaction starts the source treasure event;
- Link uses source BOXOP/GETA/GETAWAIT;
- Heart Piece is visible above/around Link in real gameplay;
- inventory remains unchanged while the Demo_Item is merely visible;
- message acknowledgement kills the Demo_Item and commits exactly once;
- chest stays open after recreation;
- Link returns to a clean locomotion pose;
- marker file states success;
- screenshot is a real gameplay frame, not text/diagnostics.

Then add a report, update `STATUS.md`, and commit.
