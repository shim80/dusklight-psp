# Rapport 129 — Parité des neuf adaptateurs F_SP108

## Résultat

Les neuf adaptateurs du premier chargement F_SP108 sont maintenant classés par
identité source, transformation, état initial et cycle lifecycle.

`f_sp108_adapter_parity.csv` contient pour chaque acteur :

- process id ;
- paramètres ;
- room/layer ;
- position/rotation source ;
- ressource associée ;
- état initial ;
- nombre d’execute/draw observés ;
- classification.

## Statut

- identité/transform : `MATCH_WITH_TOLERANCE` pour 9/9 ;
- lifecycle create/execute/draw/delete : `MATCH_WITH_TOLERANCE` ;
- logique métier complète : `MISSING_ON_PSP` pour les adaptateurs qui ne
  compilent pas encore leur classe originale.

Le rapport sépare donc désormais explicitement « adaptateur correct » de « acteur
original porté ». Les adaptateurs ne sont pas promus en parité comportementale
sans exécution du code original correspondant.

## Tests

`scripts/test-f-sp108-adapter-parity.sh` :

- reconstruit les placements depuis DPSC ;
- compare toutes les transforms ;
- contrôle le nombre exact d’acteurs ;
- rejette un process id, room, layer ou paramètre modifié ;
- interdit toute classification `MATCH` si `original_behavior=false`.
