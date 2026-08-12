# Rapport 136 — Staging de la revue globale

## Résultat

Le paquet intermédiaire est généré sous
`artifacts/dusklight-psp-global-parity-review/`. Il contient les matrices
scène/acteur, les rapports 124 à 139, les métriques de provenance performance
et les répertoires attendus pour les futures captures.

Il ne contient aucun des quatre marqueurs finaux. Son statut est
`PENDING_GUI_EXECUTION`.

Le transport Functional reste fixé à OpenGL + software. Le dernier essai
borné est `HOST_LAUNCHSERVICES_FAILED` avec `boot_observed=false`. L’essai
précédent a exposé `HOST_GRAPHICS_INIT_FAILED`. Aucun défaut runtime EBOOT
n'est donc établi.

Les lancements LaunchServices par scénario sont abandonnés. Le transport
requis est désormais `persistent_gui_broker`; son implémentation et ses tests
hôte passent, mais son démarrage unique depuis le Terminal propriétaire reste
`pending`.

## Portes encore ouvertes

- 40 traces PSP courantes ;
- comparaisons DTRC scène par scène ;
- fermeture comportementale Link ;
- captures Functional desktop/PSP ;
- overlays, heatmaps et landmarks réels ;
- runs Performance et PSP conservative du nouvel EBOOT ;
- vérification de fuite/allocation/corruption sur les runs complets ;
- un unique passage release complet ;
- revue manuelle du propriétaire.

Le staging n’est pas une classification
`READY_DUSKLIGHT_PSP_GLOBAL_PARITY_REVIEW_WITH_DOCUMENTED_PLATFORM_DIFFERENCES`.
Il prépare cette revue sans falsifier ses preuves.

```text
global_review_staged=true
global_review_ready=false
final_markers=0
ppsspp_execution=PENDING_GUI_EXECUTION
ppsspp_transport_required=persistent_gui_broker
ppsspp_gui_broker_manual_start=pending
ppsspp_eboot_runtime_failure_observed=false
user_manual_direction_validation=pending
user_manual_acceptance=pending
```
