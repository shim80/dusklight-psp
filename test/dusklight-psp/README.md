# Reconstructed canonical PSP entry target

The original `test/dusklight-psp` launcher referenced by the canonical build/package scripts is not present in the reconstructed public Git history.

This directory intentionally restores the **build target and packaging boundary**, not a claim of byte-for-byte historical recovery.

For the current checkpoint it builds the same PSP startup/save/control entry source already validated by `test/startup-save-psp`. This keeps one implementation of the release-path controls and persistent save handoff while restoring the canonical `dusklight_psp` target expected by `scripts/build-canonical-existing-assets.sh` and `scripts/package-dusklight-psp.sh`.

Next step: replace the shared public probe entry with the asset-backed canonical game driver using the preserved runtime contracts (`canonical_game.hpp`, startup packages, F_SP108 room packages, playable runtime) while retaining this target name and output layout.

Commercial-derived packages remain local and untracked.
