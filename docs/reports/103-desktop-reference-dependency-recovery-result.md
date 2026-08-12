# Résultat — dépendances desktop de référence

## Classification

`READY_DUSKLIGHT_DESKTOP_REFERENCE_VANILLA_DEPS`

Dépendances récupérées/vérifiées :

- Dawn : archive officielle exacte, provenance prouvée ;
- SDL 3.2.22 : archive officielle téléchargée, compilée et installée localement ;
- abseil-cpp 20240722.0 : archive fournie, provenance primaire GitHub vérifiée ;
- fmt 12.1.0 : archive fournie, provenance primaire GitHub vérifiée ;
- freetype 2.14.3 : archive fournie, provenance primaire SourceForge vérifiée ;
- JPEG Turbo : préinstallé dans l'environnement et réutilisé ;
- Glaze et miniz : présents dans l'archive source exacte de Dusklight.

La construction complète est désormais bloquée sur la résolution de miniz CMake,
pas sur une dépendance système hôte manquante.
