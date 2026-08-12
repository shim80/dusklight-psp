# Rapport 124 — Périmètre de parité PSP actuel

## Périmètre prouvé

La campagne mesure maintenant cinq axes de fidélité :

1. **startup** — logos, ouverture partielle, titre, file select borné, New Game ;
2. **premier gameplay** — F_SP108, Link idle, room/collision et acteurs essentiels ;
3. **caméra** — checkpoints F_SP102 et caméra gameplay existante ;
4. **rendu** — room/Link/ombres sur backend GE fixed-function ;
5. **benchmark** — profils PPSSPP calibrés v2.1.

## Classification courante

| Domaine | Statut | Commentaire |
|---|---|---|
| startup sequence | `MATCH_WITH_TOLERANCE` | ordre et transitions couverts |
| title camera | `MATCH_WITH_TOLERANCE` | checkpoints source partagés |
| file select | `EXPECTED_PLATFORM_DIFFERENCE` | UX bornée, pas J2D complet |
| F_SP108 essential actors | `MATCH_WITH_TOLERANCE` | 9/9 frontière prouvée |
| Link idle | `MATCH_WITH_TOLERANCE` | position/procédure initiale |
| Link mobile procedures | `MISSING_ON_PSP` | non fermées par cette trace |
| room geometry/collision | `MATCH_WITH_TOLERANCE` | packages source convertis |
| renderer/materials | `EXPECTED_PLATFORM_DIFFERENCE` | GE simplifié, pas GX exact |
| projected shadow | `EXPECTED_PLATFORM_DIFFERENCE` | géométrie/projection simplifiée |
| actor original behavior | `MISSING_ON_PSP` | adaptateurs sans procédures originales |
| opening event/audio | `MISSING_ON_PSP` | partiel/silencieux |
| performance hardware | `NOT_RUN` | PSP physique différée |

## Gates ouverts

- smoke canonique 18 marqueurs à refaire sur le build actuel ;
- playable legacy à refaire après correction de l’erreur 175 ;
- Link walk/run/turn/stop/slope à instrumenter côté source ;
- comportements originaux des acteurs à porter ou classer explicitement ;
- validation matérielle PSP physique à effectuer lorsque l’utilisateur l’autorisera.

## Gates fermés

- oracle desktop v2 reproductible ;
- caméra titre ;
- frontend startup v2 ;
- file select borné ;
- première frame F_SP108 ;
- neuf acteurs essentiels instanciés ;
- package union canonique ;
- FrameProfiler v2.1 calibré côté PPSSPP.
