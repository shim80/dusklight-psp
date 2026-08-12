# PSP import-stub link ordering

Date: 2026-08-12
Issue: `#3`
Branch: `agent/fix-psp-import-order`
Base checkpoint: `agent/source-demo-item-commit`

## Problem

The source chest checkpoint cross-compiled and produced `EBOOT.PBP`, but both
`psp-fixup-imports` passes reported:

```text
Warning: could not fixup imports, stubs out of order.
Ensure the SDK libraries are linked in last to correct this.
```

The warning is significant because an apparently successful link can still leave
an EBOOT with invalid PSP import ordering.

## Root cause

`dusk_psp_runtime` is an INTERFACE aggregation target. CMake emitted its
transitive Dusklight archives after the executable's direct link dependencies.
When the target listed `dusk_psp_runtime` beside `pspge`, `pspdisplay`,
`pspdebug`, and `pspctrl`, the SDK import-stub libraries appeared before the
project archives in the final link command.

Removing the SDK libraries was not a fix: the link then failed with unresolved
`sceGe*`, `sceDisplay*`, and `pspDebug*` symbols. The libraries were required,
but their position was wrong.

## Fix

`test/chest-source-full/CMakeLists.txt` now:

1. lists every Dusklight static archive explicitly;
2. encloses those project archives in one GNU linker rescan group;
3. appends `pspgum`, `pspgu`, `pspge`, `pspdisplay`, `pspdebug`, and `pspctrl`
   after the group.

The final causal shape is:

```text
main.o
-Wl,--start-group
  dusk_psp_game ... dusk_psp_resources
-Wl,--end-group
-lpspgum -lpspgu -lpspge -lpspdisplay -lpspdebug -lpspctrl
```

The workflow records the verbose linker command and fails if the original
warning ever reappears.

## Regression coverage

A new asset-independent host target uses the real upstream
`d_a_demo_item.cpp` with the PSP compatibility path and verifies the stacked
source-owned acquisition semantics:

```text
DEMO_ITEM_COMMIT_HOST_OK item=0x21 source_owned=true commit_frames=2 acquisitions=1 duplicate_commits=0
```

This proves that the link-only correction did not replace or bypass the
`dead() -> actionEvent() -> execItemGet()` lifecycle introduced by the base
checkpoint.

## PSP and PPSSPP validation

GitHub Actions run `31633775215` completed successfully with the pinned
PSPDEV/PSPSDK and PPSSPP versions.

Validation results:

- host `Demo_Item` regression passed;
- Allegrex compilation produced `EBOOT.PBP`;
- the verbose build log contained no import-stub ordering warning;
- final EBOOT SHA-256:
  `ca162d1d48f8b595bd15d4b845ff2c97e6524759b14ac90e9d779826a9e45a18`;
- PPSSPP booted the EBOOT and reached the controlled
  `CHEST_SOURCE_FULL.FAIL` marker with `code=10`.

PPSSPP v1.20.4 initializes SDL audio before loading an EBOOT and provides no
no-audio command-line mode. The headless workflow therefore starts a local
PulseAudio null sink. This is host infrastructure only and does not modify PSP
runtime behavior.

The `code=10` marker is the expected public-CI boundary: proprietary-derived
assets are deliberately absent, so the EBOOT exits at its first asset-loading
check. Reaching that marker proves that PSP initialization and import resolution
completed without an import crash; it is not a substitute for the separate
asset-backed full chest gameplay validation.

## Scope boundary

No gameplay timing, item placement, message acknowledgement, inventory,
treasure persistence, graphics, or sound behavior was changed. Issue `#1`
(BCK/BRK/TEV Heart Piece presentation) remains separate.
