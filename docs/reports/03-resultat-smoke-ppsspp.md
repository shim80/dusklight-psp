# Résultat du premier test PPSSPP

Date : 2026-07-16

## Statut

Le premier smoke test PSP a été compilé avec succès avec la toolchain PSPDEV
épinglée et inspecté statiquement. Les tentatives de lancement PPSSPP ont
échoué au niveau de l'hôte avant l'exécution du programme PSP. La boucle est
donc classée `HOST_WINDOW_BLOCKED` et non `PSP_RUNTIME_FAILED`.

Une relance manuelle de PPSSPP par l'utilisateur a ensuite été observée. Cette
exécution a écrit `.test-data/ppsspp/smoke/SMOKE.OK` avec le contenu exact
`DUSKLIGHT_PSP_SMOKE_OK` et a produit `ppsspp-smoke-00000.png`. Cela valide le
homebrew et le montage du Memory Stick sous PPSSPP lorsque l'application est
lancée depuis une session graphique utilisateur. Le lanceur automatisé reste
néanmoins bloqué par l'impossibilité du runner de prendre la main sur la fenêtre
GUI macOS.

Une capture fournie par l'utilisateur montre en outre le homebrew en exécution
avec le fond bleu, le triangle coloré et la texture damier, ainsi que l'overlay
PPSSPP de debug actif. Cette preuve visuelle confirme le rendu GU et la présence
de la texture factice. L'automatisation de la fenêtre macOS reste distinctement
non résolue.

## Installation locale effectuée

- PSPDEV : `.tools/pspdev`
  - archive officielle GitHub `v20260701`, arm64 macOS;
  - SHA-256 vérifié avant extraction;
  - `psp-gcc` 15.1.0, `psp-cmake` 3.31.6, `psp-objdump` 2.44;
  - PSPSDK sous `.tools/pspdev/psp/sdk`.
- PPSSPP : `.tools/ppsspp/PPSSPPSDL.app`
  - archive officielle GitHub `v1.20.4`;
  - SHA-256 vérifié avant extraction;
  - binaire arm64 signé ad-hoc par l'archive amont.

Le bundle PSPDEV extrait occupe environ 1,9 Gio. PPSSPP occupe environ 22 Mio.

## Build du smoke test

Commandes exécutées :

```bash
source scripts/env.sh
scripts/build-smoke.sh
scripts/inspect-smoke.sh
```

Le build produit :

- `build/smoke/smoke.elf` : 438 004 octets;
- `build/smoke/EBOOT.PBP` : 486 884 octets;
- `PARAM.SFO`, `smoke.prx` et l'asset factice.

Inspection :

- ELF32 little-endian MIPS, machine MIPS R3000;
- ELF non stripé;
- présence de `.text`, `.data`, `.rodata`, `.bss`;
- symbole `main` présent;
- sections PSP `.lib.stub*` présentes;
- le PBP contient la chaîne `DUSKLIGHT_PSP_SMOKE_OK`.

## Tentatives PPSSPP

### 1. Lancement direct du bundle

Échec immédiat de l'application hôte : `NSOSStatusErrorDomain Code=-10810`.
Aucun processus PPSSPP durable, aucun journal et aucun `SMOKE.OK`.

### 2. Lancement via LaunchServices

PPSSPP apparaît dans la table de processus sous la session utilisateur, mais la
fenêtre ne peut pas être contrôlée depuis le terminal automatisé. Le processus
consomme 0 % CPU et le marqueur n'apparaît pas.

L'accès aux fenêtres via Accessibility renvoie `-25211` (`kAXErrorAPIDisabled`).
L'ajout à la base `tcc.db` est interdit par SIP. Aucune tentative de désactiver
SIP, TCC ou une protection système n'a été faite.

## Modifications apportées au lanceur

`scripts/run-ppsspp-smoke.sh` est passé à un mode bloquant par défaut :

- `--launch-mode auto|direct|launchservices`;
- `--profile managed|system`;
- timeout par défaut de 20 secondes, plafonné à 300;
- cleanup garanti sur timeout, SIGINT et SIGTERM;
- détection et terminaison des processus PPSSPP démarrés par le script;
- classification `HOST_WINDOW_BLOCKED` si PPSSPP démarre sans atteindre le
  marqueur;
- manifeste de demande manuelle si l'environnement interdit l'automatisation de
  la fenêtre.

Le mode `managed` garde `HOME` et `XDG_CONFIG_HOME` sous
`.test-data/ppsspp/`. Le mode `system` est disponible uniquement sur demande
explicite pour diagnostiquer les restrictions macOS; il ne modifie pas les
préférences utilisateur de PPSSPP avec les runs effectués ici.

Une interruption manuelle au milieu du timeout a aussi vérifié que le nouveau
handler de signal tue PPSSPP immédiatement et nettoie les fichiers temporaires.

## Conclusion

La compilation et l'empaquetage PSP sont opérationnels. L'EBOOT est
structurellement conforme pour une première boucle. L'étape suivante
recommandée est d'exécuter PPSSPP depuis une session graphique réellement
contrôlable, ou d'ajouter un runner PPSSPP source/headless si une exécution
entièrement automatisée est indispensable.

Mise à jour : le homebrew s'est effectivement exécuté sous PPSSPP lorsque le
lancement GUI a été effectué manuellement par l'utilisateur. Le statut runtime
PSP n'est donc plus bloqué; seul le contrôle automatique de PPSSPP par le runner
reste classé `HOST_WINDOW_BLOCKED`.
