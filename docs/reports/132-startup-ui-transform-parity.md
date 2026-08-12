# Rapport 132 — Parité des transforms startup et UI

## Résultat

Quatorze surfaces startup/UI sont inventoriées dans
`reference/parity/startup-ui-parity.csv`. Les tests hôte ferment l’ordre DPST,
les segments logos, les packages DPSU, les modèles et textures du titre, le
BCK converti, les trois checkpoints caméra, la porte START, le file select,
New Game et le layout DPUI du HUD.

Le statut global reste `PARTIAL_PARITY` : les nouvelles captures Functional,
les landmarks écran et les traces de panes desktop/PSP ne peuvent pas être
produits tant que le runner GUI est indisponible.

## Pivots et transforms

| Surface | origine/pivot | état |
|---|---|---|
| warning, Nintendo, Dolby | anchor/pivot du pane DPSU issu de la source | `MATCH_WITH_TOLERANCE` local |
| décor titre | origine locale du modèle J3D converti | `MATCH_WITH_TOLERANCE` local |
| logo titre | origine locale et base matrix source, jamais centre AABB | `MATCH_WITH_TOLERANCE` local |
| animation titre | SRT local du DPAN/BCK | `MATCH_WITH_TOLERANCE` local |
| prompt, file select, curseur | anchor/pivot de pane | `MATCH_WITH_TOLERANCE` local |
| HUD et pause | anchor/pivot DPUI | `MATCH_WITH_TOLERANCE` local |
| caméra titre | checkpoints source 0/900/1800 | `MATCH_WITH_TOLERANCE` |

Les différences de résolution, de filtre, de composition J2D simplifiée et de
texte PSP restent `EXPECTED_PLATFORM_DIFFERENCE`. Aucun bounds de modèle ou de
pane n’est utilisé comme pivot runtime.

## Comportement

| Flux | résultat |
|---|---|
| ordre logos → opening → titre | `MATCH_WITH_TOLERANCE` |
| porte `Appuyez sur START` | `MATCH_WITH_TOLERANCE` |
| curseur file select | `MATCH_WITH_TOLERANCE` |
| New Game → `F_SP108/R01/start21` | `MATCH` pour la destination |
| opening complet | `PARTIAL_PARITY`, event runtime/audio incomplets |
| saisie libre des noms | `PARTIAL_PARITY` |
| HUD/pause comparés au desktop | `PARTIAL_PARITY`, trace pane absente |

Le `MATCH` de New Game porte uniquement sur la destination et le maintien du
même EBOOT. Il ne propage pas un statut `MATCH` aux acteurs F_SP108, classés
séparément.

## Validation

`scripts/test-startup-ui-transform-parity.sh` exécute :

- `startup_runtime_host_test` ;
- `startup_ui_host_test` ;
- `startup_title_asset_host_test` ;
- `startup_camera_parity_host_test` ;
- `startup_first_playable_host_test` ;
- `frame_profiler_host_test` ;
- `dpui_v2_host_test`.

```text
startup_ui_surfaces=14
startup_ui_partial=6
title_model_recentered=false
title_logo_bounds_used_as_pivot=false
pane_bounds_used_as_pivot=false
new_game_destination_match=true
functional_capture=PENDING_GUI_EXECUTION
performance_capture=PENDING_GUI_EXECUTION
psp_conservative_capture=PENDING_GUI_EXECUTION
classification=PARTIAL_PARITY
user_manual_direction_validation=pending
user_manual_acceptance=pending
```
