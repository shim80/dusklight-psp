# Résultat — configuration desktop de référence

## Classification

`READY_DUSKLIGHT_DESKTOP_REFERENCE_CONFIGURED`

Le snapshot exact fourni est configuré hors ligne sans modification du source :

- cache : `reference/dusklight-desktop-vanilla/build` ;
- source exact : `reference/dusklight-desktop-vanilla/source/dusklight-main` ;
- source Aurora exacte : `source/dusklight-main/extern/aurora` ;
- miniz officiel : `3.0.2`, injecté par `FETCHCONTENT_SOURCE_DIR_MINIZ` ;
- SDL 3.2.22, Dawn, fmt, freetype et abseil résolus localement ;
- téléchargements CMake forcés hors ligne ;
- `cmake --build ... --target help` confirme la cible `dusklight`.

Les étapes restent confinées à `.cache`, `.tools`, `reference` et `docs`.
