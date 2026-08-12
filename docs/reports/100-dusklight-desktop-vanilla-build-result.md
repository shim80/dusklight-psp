# Résultat — build vanilla Dusklight desktop

## Classification

`READY_DUSKLIGHT_DESKTOP_VANILLA_BUILD`

Le snapshot desktop de référence est maintenant construit sans instrumentation
et sans réutiliser les deux caches CMake fournis par l'utilisateur. Les
corrections nécessaires sont confinées au workspace de référence :

- le header système X11 manquant localement `X11/extensions/XShm.h` est fourni
  dans `reference/dusklight-desktop-vanilla/compat-include` ;
- le compilateur de shaders a un fallback local `posix_spawn` lorsque
  `/bin/sh` échoue avec EPERM dans l'environnement hôte ;
- les dépendances desktop sont résolues par symlinks vers
  `.cache/deps/source/dusklight-main/extern` ;
- l'exécutable final est `reference/dusklight-desktop-vanilla/build/dusklight` ;
- SHA-256 : `f74a566478574387e72fa12f9befb452249ea38a13c0d6bd0aa6e4eeaeb6cfaa`.

Le workspace source PSP `dusklight-main/` n'est pas modifié par ces corrections.
