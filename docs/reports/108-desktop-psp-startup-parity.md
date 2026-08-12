# Parité oracle desktop et startup PSP v2

## Classification

`READY_DUSKLIGHT_DESKTOP_PSP_STARTUP_PARITY_FOUNDATION_WITH_DOCUMENTED_PLATFORM_DIFFERENCES`

Les trois profils startup v2 ont été réellement exécutés via
`launchservices_gui`, dans le profil PPSSPP isolé. Les six requêtes ont observé
le boot et validé leurs marqueurs.

## Matrice de comparaison

| Élément | desktop_value | psp_value | classification | correction | platform_difference |
| --- | --- | --- | --- | --- | --- |
| ordre startup | logo → opening → F_SP102 → titre → file select → New Game | même ordre DPST, 11 segments | MATCH | aucune | non |
| gate titre | `key_wait`, START à PAD 2100, non bloqué | `InputRequired`, input valide | MATCH | aucune | non |
| attente titre | animation à 1785, prompt à 1924 | source event puis prompt | MATCH_WITH_TOLERANCE | conserver l’événement, calibrage visuel futur | oui |
| file select | vraie `NAME_SCENE`, trois slots | trois slots, textures source, layout borné | EXPECTED_PLATFORM_DIFFERENCE | BLO complet futur | layout PSP |
| création fichier | états card 13→25→13→14→15→16→17→2 | initialisation New Game déterministe | MATCH_WITH_TOLERANCE | aucune destination | stockage PSP |
| noms | Link puis cheval, états 16→17→18→19→46 | New Game automatisé | EXPECTED_PLATFORM_DIFFERENCE | UI de nom complète future | interaction réduite |
| destination | `F_SP108`, point 21, room 1, layer 13 | `F_SP108`, start 21, room 1 ; layer export 0 | MATCH_WITH_TOLERANCE | documenter layer scène/source | export room PSP |
| première frame | `PLAY_SCENE` et `ROOM_SCENE` créées | 180 frames, renderer synchronisé | MATCH_WITH_TOLERANCE | aucune | runtime PSP partiel |
| acteurs F_SP108 | acteurs source chargés par le runtime desktop | 599 records, 0 instancié | MISSING_ON_PSP | profiler avant sélection des acteurs essentiels | port incomplet |
| caméra titre | valeur non encore tracée | shim dérivé des limites | MISSING_ON_PSP | attendre la trace caméra exacte | non mesuré |
| opening | runtime desktop réel | événementiel partiel, audio désactivé | MISSING_ON_PSP | port événementiel futur | audio différé |

## Exécutions PPSSPP

| Profil | Renderer | boot_logos | title_flow / New Game |
| --- | --- | --- | --- |
| Functional | software | succès, 362 frames | succès, 787 frames |
| Performance | hardware | succès, 362 frames | succès, 787 frames |
| PSP conservative | hardware | succès, 362 frames | succès, 787 frames |

Le benchmark titre observé donne 45,839 FPS moyens, 24,194 FPS à 1 % low,
20,371 FPS à 0,1 % low, 21,815 ms moyens et 39,502 ms p95. Il atteint 56 draws,
7 080 octets de command list et 1 835 136 octets d’EDRAM. Ces valeurs sont
identiques dans les trois profils et portent `diagnostic_only=true` :
elles sont conservées comme résultat PPSSPP relatif, pas comme preuve de
performance PSP physique. `memory_peak_bytes=0` et les temps GE nuls restent
des métriques incomplètes.

## Non-régression

- tests hôte startup/UI/titre/F_SP108/FrameProfiler : succès ;
- build et smokes historique, core, GU, 3D, asset et bridge : succès ;
- core via GUI : `PSP_EBOOT_STARTED_AND_MARKERS_VALID` ;
- package startup final : succès, EBOOT SHA-256 inchangé
  entre le build et le paquet,
  `1513a8abbf854c31605c44015800f6bca0de6aad975c54f6fb026abf03079453` ;
- les six métriques finales identifient le build
  `3f4d1f47a781d472f7cfb8369cc4984283ff99c6`.

Le smoke canonique GUI a booté mais ses 18 marqueurs ne sont pas apparus en
180 secondes : `PSP_EBOOT_STARTED_MARKER_FAILURE`. La suite historique directe
a aussi échoué plus loin sur le smoke jouable avec `error_code=175`. Ces deux
échecs restent des portes de release ouvertes ; ils ne sont pas présentés comme
des succès ni comme des échecs du startup v2, dont les modes dédiés ont tous
réussi.

## Réseau et livrables

Aucun accès réseau n’a eu lieu. Le paquet est sous
`artifacts/dusklight-startup-v2/PSP/GAME/DUSKLIGHT_PSP`. Le contrat
`DUSKLIGHT_PSP_RENDERING_CONTRACT_V2` est figé séparément du contrat v1.
