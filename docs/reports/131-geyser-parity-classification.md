# Rapport 131 — Classification de parité du geyser historique

## Résultat

Le geyser PSP est une traduction procédurale de deux records historiques
`F_SP110/R02`. Il ne compile pas `daObjGeyser_c` et ne charge pas son BMD, son
MoveBG, ses émetteurs JPA ni son audio. Sa classification est :

```text
implementation_type=PROCEDURAL_FALLBACK
logic_status=PARTIAL_PARITY
representation_status=EXPECTED_PLATFORM_DIFFERENCE
overall_status=PARTIAL_PARITY
```

Il est donc interdit de présenter le geyser complet comme `MATCH`.

## Logique source et traduction PSP

| Élément | Source `daObjGeyser_c` | Runtime PSP | preuve actuelle |
|---|---|---|---|
| identité | process `0x0167`, profil `g_profile_Obj_Geyser` | nom/process DPSC conservés | exact sur 2 records |
| paramètres | type, comportement, switches/timings | `decode_geyser_parameters` | décodage hôte déterministe |
| états réactifs | off, on-wait, on, disappear | off, warning, on, disappear | structure analogue, pas de DTRC alignée |
| états périodiques | off, on-wait, on | off, warning, on | structure analogue, pas de DTRC alignée |
| volume joueur | capsule source | distance segment/rayon PSP | adaptation non encore comparée |
| impulsion | logique source joueur | impulsion PSP bornée | adaptation non encore comparée |
| pause/reset/delete | acteur source | état PSP stable et remis à zéro | test hôte |

Le test hôte couvre les paramètres `0x08040401` et `0x080404ff`, les deux
orientations, les échelles, les transitions réactive/périodique, la pause, le
reset, le delete, les contacts et les budgets. Il ne fournit pas les ticks
desktop de `actionOff*`, `actionOnWait*`, `actionOn*` et
`actionDisappear`.

## Représentation

| Ressource/effet source | État PSP | classification |
|---|---|---|
| BMD du geyser | absent, colonne procédurale | `MISSING_OR_NOT_PORTED` |
| collision MoveBG | absente du fallback | `MISSING_OR_NOT_PORTED` |
| JPA pré/source/colonne/fumée | particules procédurales | `EXPECTED_PLATFORM_DIFFERENCE` |
| vent point | absent | `MISSING_OR_NOT_PORTED` |
| audio bas/haut | absent par gel de périmètre | `EXPECTED_PLATFORM_DIFFERENCE` |
| lumière | approximation renderer PSP | `EXPECTED_PLATFORM_DIFFERENCE` |

Le visuel procédural reste acceptable pour la verticale historique, mais ne
sert pas de preuve de parité de modèle, de MoveBG ou d’effet.

## Validation et blocage local

`scripts/test-geyser-parity.sh` réexécute le test hôte v1/v2 et vérifie les
symboles source et PSP. Le nœud PPSSPP historique demeure
`PENDING_GUI_EXECUTION`; aucune exécution directe de PPSSPP n’est utilisée.

```text
geyser_records=2
geyser_source_class=daObjGeyser_c
geyser_original_source_compiled=false
geyser_logic_desktop_aligned=false
geyser_psp_host_state_machine=true
geyser_bmd_ported=false
geyser_movebg_ported=false
geyser_jpa_ported=false
geyser_audio_ported=false
classification=PARTIAL_PARITY
user_manual_direction_validation=pending
user_manual_acceptance=pending
```
