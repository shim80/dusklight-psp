# Rapport 130 — Matrice des huit sources originales

## Résultat

Les huit fichiers sources annoncés sont bien liés dans l’ELF Allegrex et leurs
profils réels sont exercés par les tests hôte. Dix-huit placements de cinq
profils existent dans les trois DPSC canoniques. `poFire` et `tboxSw` sont
exercés par leurs surfaces hôte mais n’ont pas de placement autonome dans ces
trois tables ; `tboxSw` est couplé au cycle du coffre.

Cette preuve ferme la présence binaire et le cycle hôte. Elle ne ferme pas la
parité comportementale cross-platform : aucune séquence DTRC v3 desktop/PSP
alignée n’existe encore pour leurs états dynamiques. Les huit lignes restent
donc `PARTIAL_PARITY`, même lorsque la source originale est compilée.

La matrice machine est
`reference/parity/original-source-actor-matrix.csv`.

| Source | process | classe | placements canoniques | source PSP | cycle hôte | états desktop/PSP |
|---|---|---|---:|---|---|---|
| `scnChg` | `0x030c` | `daScex_c` | 2 | originale | exercé | `PARTIAL_PARITY` |
| `L4hmato` | `0x009f` | `daLv4HsTarget_c` | 7 | originale | exercé | `PARTIAL_PARITY` |
| `spnGear` | `0x0183` | `daObjLv4Gear_c` | 4 | originale | exercé | `PARTIAL_PARITY` |
| `poFire` | `0x017a` | `daTagPoFire_c` | 0 | originale | exercé | `PARTIAL_PARITY` |
| `tboxSw` | `0x016e` | `daTboxSw_c` | 0 | originale | exercé | `PARTIAL_PARITY` |
| `tboxB0` | `0x00fb` | `daTbox_c` | 2 | originale | exercé | `PARTIAL_PARITY` |
| `L4Pgate` | `0x009d` | `daLv4PoGate_c` | 1 | originale | exercé | `PARTIAL_PARITY` |
| `swspin` | `0x00b3` | `daObjSwSpinner_c` | 2 | originale | exercé | `PARTIAL_PARITY` |

## Ce que les tests démontrent

`scripts/test-original-actor-parity-matrix.sh` vérifie :

- le fichier source réel et son symbole de profil ;
- la présence du fichier source dans la map Allegrex ;
- les 18 placements canoniques attendus ;
- les cycles create/execute/draw/delete hôte ;
- les ressources modèle, texture, animation et MoveBG pertinentes ;
- les matrices modèle/collision pour les objets couverts par l’audit pivot ;
- l’absence de statut `MATCH` sans trace comparable.

Il réutilise le test pivot des objets et les tests dédiés à `SCENE_EXIT`,
`Tag_poFire` et `TBOX_SW`. Aucun replay de trace n’est utilisé comme logique
PSP.

## Première divergence encore ouverte

La première divergence causale n’est pas un pivot ni une absence binaire. Elle
est l’absence d’une observation desktop et PSP alignée par `ParityActorId` pour
les états et leurs ticks :

- rotation de `spnGear` ;
- ouverture/fermeture de `Lv4PoGate` ;
- activation de `SwSpinner` ;
- switch et animation de `tbox` ;
- destination de `SCENE_EXIT` ;
- cycle de `poFire`.

Ces nœuds PSP restent `PENDING_GUI_EXECUTION`. En conséquence :

```text
original_source_files_compiled=8
original_profiles_host_exercised=8
canonical_original_actor_placements=18
original_actor_desktop_aligned_scenarios=0
original_actor_state_match=0
original_actor_state_partial=8
classification=PARTIAL_PARITY
user_manual_direction_validation=pending
user_manual_acceptance=pending
```
