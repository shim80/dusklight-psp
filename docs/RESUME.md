# Exact resume protocol

This file is designed for a fresh agent/session with no conversational memory.

## 1. Read first

Read, in order:

1. `/AGENTS.md`
2. `/docs/STATUS.md`
3. `/docs/COMMIT_LEDGER.md`
4. the newest reports in `/docs/reports/`, including `201-psp-import-stub-order.md`
5. `/test/chest-source-full/main.cpp`
6. `/test/canonical-runtime-demo-item/demo_item_commit_host_test.cpp`
7. `/test/getawait-heart-probe/main.cpp`
8. the PSP runtime functions referenced by those harnesses, especially `apply_source_animation_resource_and_skin()`.

Do not infer that a historical report's Git SHA exists in the reconstructed repository. The historical ledger distinguishes provenance from reconstructed commits.

The current link-order work is stacked on `agent/source-demo-item-commit`. Keep that dependency explicit until the gameplay PR is integrated.

## 2. Required local dependencies

Bring your own legally obtained Twilight Princess game image when original assets must be regenerated. Never commit it.

Install/configure PSPSDK/pspdev and PPSSPP. Set:

```sh
export PSPDEV=/path/to/pspdev
export PSPSDK="$PSPDEV/psp/sdk"
export PATH="$PSPDEV/bin:$PSPSDK/bin:$PATH"
```

## 3. Build sanity

Start with the asset-independent source-owned acquisition regression:

```sh
cmake -S test/canonical-runtime-demo-item \
  -B build/host/canonical-runtime-demo-item \
  -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build build/host/canonical-runtime-demo-item \
  --target demo_item_commit_host_test -j 4
./build/host/canonical-runtime-demo-item/demo_item_commit_host_test
```

Expected marker:

```text
DEMO_ITEM_COMMIT_HOST_OK item=0x21 source_owned=true commit_frames=2 acquisitions=1 duplicate_commits=0
```

Then configure and build the integrated PSP chest target:

```sh
psp-cmake -S test/chest-source-full \
  -B build/psp/chest-source-full \
  -G Ninja -DCMAKE_BUILD_TYPE=RelWithDebInfo
cmake --build build/psp/chest-source-full -j 4 --verbose \
  2>&1 | tee build/psp/chest-source-full/build.log
```

The build is invalid if the log contains:

```text
Warning: could not fixup imports, stubs out of order.
```

The required final link invariant is: all Dusklight archives first inside one
`--start-group/--end-group`, followed by the PSP GU/system libraries. Do not
reintroduce `dusk_psp_runtime` beside direct SDK libraries on this executable;
CMake would place the INTERFACE target's transitive archives after those stubs.

The preserved standalone visual probe can still be built with:

```sh
psp-cmake -S test/getawait-heart-probe \
  -B build/psp/getawait-heart-probe \
  -G Ninja -DCMAKE_BUILD_TYPE=RelWithDebInfo
cmake --build build/psp/getawait-heart-probe -j 4
```

## 4. Public-CI boot boundary

GitHub Actions run `31633775215` produced an EBOOT with SHA-256
`ca162d1d48f8b595bd15d4b845ff2c97e6524759b14ac90e9d779826a9e45a18` and booted it in PPSSPP.

Because proprietary-derived assets are absent in public CI, the expected smoke-test marker is:

```text
CHEST_SOURCE_FULL.FAIL
code=10
```

That marker proves PSP initialization and import resolution reached the first asset-loading check. It is not gameplay completion evidence. PPSSPP v1.20.4 initializes SDL audio before loading the EBOOT, so a headless runner must provide a working host audio device; the workflow uses a local PulseAudio null sink.

## 5. Asset contract for GETAWAIT Heart Piece proof

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

## 6. What the probes proved

The renderer can submit additional static models; do not reopen that as the default hypothesis. The successful GETAWAIT proof required the real Link item-get pose and source-derived Demo_Item placement. Use that evidence to integrate the event; do not replace GETAWAIT with Idle or move the Heart Piece by eye.

The host regression additionally proves the source-owned commit boundary without game assets: acknowledgement only sets `dead()`, the following normal source process frame executes `execItemGet()`, and a second frame does not duplicate the acquisition.

## 7. Next implementation target

Run the integrated source-event treasure target with legal local assets and validate the complete sequence. Event ownership must advance the Link cuts and Demo_Item lifecycle.

Critical semantics:

- item creation is not acquisition;
- item is hidden on creation;
- Link's GETA/GETAWAIT controls presentation;
- item becomes visible late in GETA;
- message acknowledgement controls the end of item presentation;
- `dead()` precedes `execItemGet()`/commit;
- treasure state persists after actor/room recreation;
- return to ordinary locomotion must clear event animation/root history.

After the integrated gameplay checkpoint is closed, return to the separate Heart Piece BCK/BRK/TEV presentation issue. Do not fake those channels with an arbitrary color pulse or timer.

## 8. Definition of done for the next gameplay checkpoint

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
