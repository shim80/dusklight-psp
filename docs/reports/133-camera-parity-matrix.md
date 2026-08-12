# Rapport 133 — Matrice de parité caméra

## Résultat

Dix usages caméra sont inventoriés dans
`reference/parity/camera-parity-matrix.csv`. La caméra titre est la seule
fermée cross-platform avec tolérance grâce aux checkpoints source
0/900/1800. Les neuf autres restent `PARTIAL_PARITY`.

Les tests hôte démontrent :

- la conversion des angles et du heading Link ;
- la cible d’attention source de Link ;
- l’initialisation de l’œil sur l’orbite, sans interpolation depuis l’origine
  monde ;
- la finitude et les round-trips des espaces caméra ;
- la collision segment/DPCL des trois rooms canoniques ;
- la caméra titre à `0.02` unité, avec aspect et near explicitement adaptés.

Ils ne démontrent pas que les trajectoires eye/center des scènes gameplay
suivent encore le desktop tick par tick.

## Matrice résumée

| Groupe | scénarios | fermé | partiel | raison principale |
|---|---:|---:|---:|---|
| startup | 3 | 1 | 2 | opening/file select incomplets |
| F_SP108 et Link | 2 | 0 | 2 | DTRC v3 PSP non rejouée |
| D_MN10 | 3 | 0 | 3 | trajectoires et transitions non alignées |
| pause | 1 | 0 | 1 | trace de gel caméra absente |
| collision | 1 | 0 | 1 | DPCL PSP prouvé, échantillons desktop absents |

Les différences d’aspect, near/far, résolution et collision simplifiée sont
`EXPECTED_PLATFORM_DIFFERENCE`. Une caméra générique n’est pas déclarée
équivalente à une caméra source.

## Validation

`scripts/test-camera-parity-matrix.sh` réexécute les contrats caméra startup,
Link et room. Les nœuds qui nécessitent PPSSPP restent
`PENDING_GUI_EXECUTION`.

```text
camera_scenarios=10
camera_match=0
camera_match_with_tolerance=1
camera_partial=9
camera_collision_rooms_host=3
camera_dtrc_psp_runs=0
classification=PARTIAL_PARITY
user_manual_direction_validation=pending
user_manual_acceptance=pending
```
