# Matrice de parité des scènes — état intermédiaire

La matrice machine `build/reports/PARITY_SCENE_MATRIX.csv` est générée depuis
les 40 scénarios déclarés, les traces réellement présentes et la date du build
PSP courant.

## Résumé

| mesure | valeur |
|---|---:|
| scénarios déclarés | 40 |
| traces DTRC v3 desktop courantes | 10 |
| traces PSP valides pour l’EBOOT courant | 2 |
| scènes `MATCH` | 0 |
| scènes `PARTIAL_PARITY` | 40 |

Les deux traces PSP courantes sont les acquisitions natives bornées
`d_mn10_r09_actors` et `d_mn10_r02_actors`. Elles couvrent dix placements
réels, mais aucune trace desktop alignée ne permet encore de promouvoir une
scène en `MATCH`.

## Politique de classification

Chaque ligne conserve le stage, la room et le scénario, les traces réellement présentes et leur caractère courant/périmé, la caméra et le nombre d’acteurs attendus, l’identité obligatoire `ParityActorId`, les statuts transform/comportement/collision/animation/UI/environnement/rendu/performance, la preuve hôte et le blocage local.

Une cohérence hôte de package ne devient pas automatiquement une parité desktop/PSP. De même, une différence de rendu attendue ne masque jamais une trace de transform absente.

## Suite requise

Les scénarios encore dépourvus de preuve courante devront être exécutés par le transport LaunchServices GUI avec l’EBOOT courant. Le comparateur devra ensuite fermer la première divergence causale de chaque scénario avant toute promotion.

```text
scene_matrix_status=INTERMEDIATE
scenes_in_scope=40
scenes_traced_desktop=10
scenes_traced_psp_current=2
scenes_match=0
scenes_partial=40
ppsspp_execution=PARTIAL_CURRENT_NATIVE_TRACE
global_marker_created=false
user_manual_direction_validation=pending
user_manual_acceptance=pending
```
