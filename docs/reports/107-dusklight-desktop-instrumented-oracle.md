# Oracle desktop instrumenté

## Classification

`READY_DUSKLIGHT_DESKTOP_INSTRUMENTED_ORACLE`

La cible vanilla continue d'être construite et validée séparément. Une seconde
cible instrumentée vit sous
`reference/dusklight-desktop-instrumented/build/dusklight` et produit des traces
JSONL versionnées.

## Contrat de trace

`DUSKLIGHT_DESKTOP_TRACE_V1` émet, par frame :

- `frame`, `dt_ms`, `stage`, `room` ;
- position Link ;
- pitch/yaw de caméra, orientation verticale, distance observée et FOV ;
- nombre de polygones collision ;
- position du centre de la scène et numéro d'aire ;
- drapeaux de rendu overlay ;
- `world_actor_count`, `world_model_count`, `shadow_count` ;
- `event_bits[0..3]` ;
- trace acteur optionnelle par `DUSKLIGHT_TRACE_ACTOR_ID` ;
- flags d'environnement ;
- compteurs de frames vus et summaries finaux.

Les traces prennent explicitement les valeurs après mise à jour de la scène et
avant l'UI ImGui. Elles ne déclarent donc pas une sémantique plus fine que
l'accès public actuel ne le permet.

## Déterminisme

Deux exécutions du même `scenario.json` borné à 40 frames produisent des lignes
identiques après retrait volontaire de `dt_ms`, la source de temps non
déterministe. Les valeurs `stage`, `room`, caméra, environnement, acteurs,
Link et ombres ; aucune correction PSP fondée sur des valeurs absentes n’a été
inventée.
