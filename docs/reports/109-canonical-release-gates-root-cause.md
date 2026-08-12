# Rapport 109 — Cause racine des portes de release canoniques

## Résultat

Classification : `READY_DUSKLIGHT_PSP_CANONICAL_RELEASE_GATES`.

Le run LaunchServices `20260731T044950Z-smoke-1` a booté l’EBOOT, exécuté
502 frames et deux transitions, puis produit les 18 marqueurs fonctionnels
avec leur contenu exact. `CONTINUOUS.METRICS` termine avec `error_code=0`.

## Cause

Le package union avait 36 entrées valides. `PspResourceManager` n’en acceptait
que 32. Le frontend startup ne traversant pas ce gestionnaire, les six runs
startup réussissaient tandis que le gameplay canonique retournait avant toute
scène.

Les marqueurs diagnostiques ont borné la panne entre
`CANONICAL_MODE.SELECTED` et `CANONICAL_MANIFEST.LOADED`. Après passage de la
capacité à 48, le même manifeste de 3 089 octets est chargé et le smoke
termine normalement.

## Non-régression

- core smoke GUI : marqueur `CORE.OK` exact ;
- host canonical core et FrameProfiler : réussis ;
- startup Functional : réussi ;
- smoke canonique : 18/18, sortie propre ;
- aucune donnée startup supprimée et aucun accès réseau.

