# Rapport 128 — Parité géométrie/collision room

## Résultat

Le pipeline room canonique n’applique plus de recentrage implicite. Les modèles
DMDL room sont convertis avec `origin_policy=preserve` et les points de
collision conservent leur espace source.

Pour chaque room canonique, `room_model_collision_parity.csv` compare :

- bounds modèle source ;
- bounds DPSM reconstruites ;
- bounds collision source ;
- bounds DPSC/DPCL ;
- translation modèle ;
- translation collision ;
- delta de centre modèle/collision ;
- empreinte XZ.

## Cas couverts

- F_SP108 ;
- R02 ;
- R09.

Les trois rooms respectent la relation source mesurée entre géométrie visible et
collision. Aucun offset « à l’œil » n’est accepté.

## Corrections

- suppression du `center_vertices` implicite du chemin room ;
- conservation explicite des positions source dans DPSM v2 ;
- validation de la même transformation pour le rendu et la collision ;
- refus des packages mélangeant ancienne et nouvelle politique sans migration.

## Tests

`scripts/test-room-model-collision-parity.sh` vérifie :

- round-trip du modèle ;
- collision transform ;
- bounds ;
- absence de double translation ;
- fixtures négatives décalées artificiellement.

La comparaison est spatiale/numérique. Une capture seule n’est jamais utilisée
comme preuve de parité collision.
