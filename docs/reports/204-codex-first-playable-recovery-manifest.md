# Codex first-playable recovery manifest

Date: 2026-08-14

This file records the exact local-only Codex checkpoint recovered after the Codex session ran out of approval credits before staging/committing/pushing its worktree.

## Git base

- base commit: `e1bae54b0886d90101e9795006c6e62e97ac1bd6`
- local branch at recovery time: `codex/playable-milestone`
- upstream at recovery time: `origin/main`
- recovery GitHub branch: `agent/recover-codex-first-playable`
- recovery tracking issue: #20
- recovery draft PR: #21

## Recovery archives

These archives remain local/private. They are listed here only by digest so a future reconstruction can prove byte identity.

- `untracked-files-LOCAL-ONLY.tar.gz`: `815960c985829c3421d15483b3d7e3e3c7216289767124c15a613207482546bc`
- `full-worktree-LOCAL-ONLY.tar.gz`: `98a38cbf43203f6dc4681787655ee07be8c3a261adb2cbd5c8b853ef4837f139`

The untracked archive was inspected before use. It contains only source/document files plus macOS AppleDouble sidecars; no commercial game asset extensions were present in the source-only recovery payload.

## Locally revalidated checkpoint

- `MATERIAL_PASS_PLAN_HOST_OK plans=1 passes=2 negative_cases=2`
- `STARTUP_CAMERA_TRACK_HOST_OK source_fps=30 source_frames=2 samples=3 crc_fail_closed=1`
- `DEMO38_SOURCE_CAMERA_HOST_OK source_fps=30 source_frames=2400 samples=2401 desktop_trace_offset=2`
- `STARTUP_TITLE_ASSET_HOST_OK room=DPRM,DPTX title=DPRM,DPTX local_vertices=24 blend_submeshes=6`
- `CANONICAL_STARTUP_ASSETS_HOST_OK paths=10 sequence=DPST ui=DPSU title=DPRM,DPTX,DPAN camera=DPCM fail_closed=1`
- `CANONICAL_ASSETS_HOST_OK stage=F_SP108 room=1 start=21 layer=0 paths=4 fail_closed=1`
- Python exporters pass `py_compile`.
- modified shell scripts pass `bash -n`.

Private generated packages were used only for local validation and remain uncommitted.

## Binary proof retained locally

- Allegrex EBOOT SHA-256: `ca75544bea82b549d06ae4a8232cbeac67d7f31efc4f517bb45dfd437b58b6ff`
- 128-byte DPSV save SHA-256: `4b9a979ac4f544b721b42fb996253821d834b692540e02d4ab016a0b282ad1b7`
- save magic: `DPSV`
- F_SP102 DPRM SHA-256: `7af40bc7d9957c29d5bef2b9bca8ec11b0d89270a2118177354ab96dd8368b3c`
- F_SP102 DPTX SHA-256: `066cf74114945a6b6c35f4fb2a900f26b4dec8acc5bb6a6ae30d621ab20e37a5`
- title DPRM SHA-256: `9bc1a7440c43d2c595a3c6ca50bdbabc54dde72ee1f87b0b41909f511f397689`
- title DPTX SHA-256: `ca144711f564c8c057e1651468f90dc9fd889f943068fc9016213f36aa34e977`
- title DPAN SHA-256: `3cdb38faa1711ed3352020ef341115ddc43c721b4a97da8cd87cf31bdc8ca537`
- generated DPCM SHA-256: `ae4630366f6c6599813674b6c79929fcec0ad2d6b747ea9a4eb1f8dd68be438f`

## Exact recovered source/worktree files

The final worktree version of every path below was hashed after extracting the full local worktree archive.

| SHA-256 | Path |
|---|---|
| `f7327b735bd7167ae0ac83f3db85718272df4f8d6522412ea56a86e3d80524da` | `.github/workflows/psp-intro-fsp102-environment.yml` |
| `fd8c0cc41fd521348141d5e86dadb4a8cd724aec125e501d29b6cab882df7cb5` | `docs/RESUME.md` |
| `71fb36813ac53cee48a74ebc20bca22a7a4f7efb95bd379dee8783e0adafe2c1` | `docs/STATUS.md` |
| `b2cbc50a9f1ca198d05ed16b4cda3fdd6631b43bd2819fdfa74d485dc1c1bf7d` | `docs/reports/203-fsp102-material-pass-title-item3d-checkpoint.md` |
| `f7aacdc86141d9ef54112cfa2b5b96576d8c520d60434cc371b8a2a6b9b35950` | `dusklight-main/platforms/psp/include/dusk/psp/playable_render.hpp` |
| `d4ad77c50289ce1064864864219fa28cc6e5ac366eda62aa5fbca4f3f5c4dbcd` | `dusklight-main/platforms/psp/include/dusk/psp/room_package.hpp` |
| `55c60e545459054f76111557bade571c8c16862bd54dafd30e310b3f31eb66c0` | `dusklight-main/platforms/psp/src/canonical_assets.cpp` |
| `cd51f726ba882609fa6fc9c8a965f33b99c86f48d649847bc2769e1cf35d749e` | `dusklight-main/platforms/psp/src/canonical_game.cpp` |
| `47150406f616fec1d9f668bee0a35efcdaaac12b9bc4daf8f7675649d577467c` | `dusklight-main/platforms/psp/src/canonical_startup_entry.cpp` |
| `d8b816c7c4d0e5587184c71859ffd958410918eb8c5982258c5c248e3955169a` | `dusklight-main/platforms/psp/src/playable_render.cpp` |
| `80bda22d9ab6764dcec68086b71124292d2f7866f7afcce6ffbdf2520c45e517` | `dusklight-main/platforms/psp/src/room_package.cpp` |
| `c806028cbd585e7ac044c9d1311ff9cd7325272cbca29b5f89aa89726b5bc992` | `scripts/bootstrap-fsp102-exporter.sh` |
| `b6db9490422b87c900650a8632d71bc273ada42542cfa6ecb438db62e516c79f` | `scripts/build-demo38-camera-assets.sh` |
| `a7ed64ccbf7b8be76da7dd16144b71bbce01ac679a30786aae1509c16fdad053` | `scripts/build-dusklight-psp-assets.sh` |
| `4d8ba43916ed96aa360d70afe02b3069c6b1062e90052fab650603a4bf52b540` | `scripts/build-dusklight-startup-assets.sh` |
| `5eca9168a5c658bee4cf6fea7f34f914d0e063256110e46ae7f8741ec1f7dbbb` | `scripts/build-fsp102-environment-assets.sh` |
| `dd761fa495099e537397cfeee6af5874c08ff210603f28c796413b032df4c319` | `test/canonical-assets-host/main.cpp` |
| `50a99b057b52b11bfd8dc02c021ff69ce9068f9100524bc68140046f6cb7fc9c` | `test/canonical-runtime/CMakeLists.txt` |
| `5582799c8450401e58a1bb4277b396ab4ce964d3a68f0ae0f1fb534dc019eb31` | `test/canonical-runtime/material_pass_plan_host_test.cpp` |
| `7221f9b7764d6224df1587af4acf7cb5907980a7e163d618a9e3599a174e5348` | `test/canonical-runtime/startup_title_asset_host_test.cpp` |
| `e6b67caee746d03e4c8df6cfaf9d9aff58c35b225ec3b2ca2025f75dff12215b` | `test/material-pass-plan-host/CMakeLists.txt` |
| `1832e21cd62652e77a87a719493664fa82e47ec91ffc83c6d8d4709f7e67ec0e` | `test/startup-camera-host/main.cpp` |
| `a149099053d6c329a1207297d9af193be1f03f4dc0d9f95c85ced3b93dacc24b` | `toolchain/manifest.lock` |
| `6074002f1172df1080e03e35127f4cd891b85b5da56d8cd486186369a3758c91` | `tools/demo38_camera_export.py` |
| `60c6408df914046e47ff251163194e983d15487dfd3fd2736f5141116b3fb24c` | `tools/fsp102_environment_export.py` |
| `aa572c10d6ccb44aa1be838aa0ab1c82a3bcc4ff548c6818bb38e4ee16cf4ffb` | `tools/startup_logo_ui_export.py` |
| `863d9234a29e61bce789f52a3e43f0ace94582a9fe49edc33a5eb58ca79e9672` | `tools/title_logo_export.py` |
| `95828d9f28ebc1c6237091019c268260eda4dd782ad4eb60669e3ed5751cd1a4` | `tools/title_prompt_ui_export.py` |

## Recovery boundary

This manifest does **not** claim that the recovered source changes are already represented in `main` or in PR #12. The local checkpoint is newer than those branches. PR #21 intentionally remains a recovery/draft surface until the recovered source is replayed on a clean GitHub base and the Allegrex + one-EBOOT proof is rerun.

No commercial-derived package, ISO, ARC/BMD/BTI/BCK/STB resource, EBOOT, save file, or local framebuffer is embedded in this manifest.
