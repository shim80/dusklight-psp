# Matrice de parité des acteurs — état intermédiaire

La matrice `build/reports/PARITY_ACTOR_MATRIX.csv` contient une ligne par
placement DPSC actif dans les trois rooms canoniques, plus les deux geysers de
la verticale historique.

| implémentation | placements | interprétation |
|---|---:|---|
| `ORIGINAL_SOURCE` | 18 | cinq profils originaux présents dans les DPSC canoniques |
| `SOURCE_COMPATIBLE_ADAPTER` | 0 | aucune revendication |
| `TRANSFORM_ONLY_ADAPTER` | 9 | frontière essentielle F_SP108 |
| `PROCEDURAL_FALLBACK` | 2 | geysers historiques |
| `MISSING` | 719 | records conservés mais non instanciés, dont `CamArea` historique |
| total | 748 | 745 canoniques + 3 historiques |

Chaque ligne utilise un `ParityActorId` composé du stage, de la room, de la
layer, de la table, de l’index source, du process ID et de la génération. Aucun
alignement par proximité ou ordre de draw n’est utilisé.

Les 18 lignes `ORIGINAL_SOURCE` ne sont pas classées `MATCH` : elles prouvent
la source et le cycle hôte. Dix placements disposent désormais d’événements
PSP natifs courants : sept en R09 et trois en R02, couvrant cinq familles
originales. Sans événements desktop alignés, ils restent
`native_psp_unaligned` et non `MATCH`. Les neuf adaptateurs n’exposent aucun
nom de classe source dans la colonne `class`. Les 719 placements absents sont
explicitement `MISSING_ON_PSP`.

```text
actor_matrix_status=INTERMEDIATE
actors_in_scope=748
actors_original_source_placements=18
actors_source_compatible_adapter=0
actors_transform_only_adapter=9
actors_procedural_fallback=2
actors_missing=719
actors_psp_trace_current=10
actors_match=0
stable_identity_collisions=0
user_manual_direction_validation=pending
user_manual_acceptance=pending
```
