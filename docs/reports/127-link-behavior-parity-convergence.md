# Rapport 127 — Convergence comportement Link desktop/PSP

## Résultat

Le corpus desktop `desktop_link_movement_v2.csv` contient désormais des runs
dédiés idle, walk, run, turn, stop et slope. Ces traces sont ingérées sans
modification par `compare_link_behavior.py`, qui rejette les scénarios absents,
les frames non monotones et les signatures DPSC incohérentes.

Le runtime PSP émet les mêmes colonnes d’état via
`PspLinkBehaviorRuntimeTrace` : position, rotation, procédure, mode,
`move_angle`, `speed_f`, animation, frame d’animation et bit sol.

## Fermeture par scénario

| Scénario | Comparaison | Statut |
|---|---|---|
| idle | position/procédure/rotation/sol | `MATCH_WITH_TOLERANCE` |
| walk | position/speed/rotation/procédure | `MATCH_WITH_TOLERANCE` |
| run | position/speed/rotation/procédure | `MATCH_WITH_TOLERANCE` |
| turn | `move_angle`, rotation, procédure | `MATCH_WITH_TOLERANCE` |
| stop | speed → 0, procédure | `MATCH_WITH_TOLERANCE` |
| slope | position Y/sol/procédure | `MATCH_WITH_TOLERANCE` |

Les tolérances positionnelles/angulaires sont explicitement calculées dans le
comparateur et publiées dans `link_behavior_parity.csv`.

## Correction

Le seul ajustement PSP introduit dans cette passe est le chemin de comportement
nécessaire pour respecter les transitions de procédure observées ; il n’est pas
appliqué aux autres classes d’acteur.

Aucune modification renderer n’est utilisée pour faire passer la comparaison.

## Tests

- ingestion desktop : réussie ;
- génération PSP trace déterministe : réussie ;
- six scénarios : tous comparés ;
- tests négatifs : scénario manquant, frame regressante, mauvais room/layer,
  mauvais process id ;
- hôte PSP : succès ;
- smoke canonique : à refaire seulement pour non-régression runtime globale.
