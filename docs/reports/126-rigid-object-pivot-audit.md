# Rapport 126 — Audit pivots acteurs rigides

## Résultat

Les objets rigides originaux portés n’ajoutent pas de compensation manuelle de
pivot dans leur classe. Les pivots sont fournis par la géométrie BMD source et
les matrices modèle calculées par la source originale.

L’audit couvre :

- `Obj_Lv4Gear` ;
- `Obj_Lv4PoGate` ;
- `Obj_SwSpinner` ;
- `Obj_Lv4HsTarget` ;
- `Obj_tbox`.

Le spinner possède un chemin spécial d’attache Midna dans la source, mais aucun
des trois placements canoniques R02/R09 ne déclenche cette branche.

## Contrat PSP

Le convertisseur acteur utilise désormais `--origin-policy preserve` et le
runtime reçoit la matrice monde sans correction de centre. Le test
`scripts/test-rigid-actor-pivot-parity.sh` vérifie :

- provenance de chaque BMD ;
- absence de translation centre ajoutée à la matrice acteur ;
- conservation de `source_origin=0` dans les DPSM produits ;
- source de la matrice collision/MoveBG ;
- cohérence spawn/model/collision pour les acteurs couverts.

## Limites

Ce rapport ferme seulement la classe de bug « double translation due au
recentrage du convertisseur ». Il ne prouve pas encore :

- le mouvement dynamique original ;
- la synchronisation rotation/collision sur toutes les frames ;
- les joints animés ;
- les branches conditionnelles non rencontrées.

Ces dimensions restent classées séparément dans la matrice acteur.
