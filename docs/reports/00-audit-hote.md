# Audit de l'hôte et du dépôt

Date de l'audit : 2026-07-16. Cette passe n'a téléchargé ni archive ni binaire,
n'a installé aucun paquet et n'a compilé aucune dépendance.

## Réserve de conformité

Pendant la validation finale, une redirection de test a créé par erreur le fichier
`/tmp/dusklight-path-guard.out`, qui contenait uniquement le message de refus d'un
chemin hors racine. Il a été supprimé immédiatement et son absence a ensuite été
vérifiée. Aucun téléchargement, outil, secret ou fichier de projet n'y a transité.
Il ne subsiste donc aucune écriture extérieure, mais le critère littéral « aucun fichier
n'a été écrit hors du dépôt » a été transitoirement enfreint. Le contrôle corrigé ne
crée plus de fichier et conserve sa sortie en mémoire.

## Faits observés

| Élément | Observation |
| --- | --- |
| Système | macOS 26.5, build 25F71, noyau Darwin 25.5.0 |
| Architecture | arm64 (Apple Silicon) |
| Shell | `/bin/zsh`; scripts de projet en Bash |
| WSL | absent et non pertinent sur cet hôte |
| Racine canonique | dossier `dusklight-psp-codex-starter` actuellement ouvert |
| Git | Git 2.53.0; dépôt initialisé sur `main` après autorisation explicite; aucun commit |
| Chemin racine | ASCII, sans espace; le dépôt contient toutefois des sous-chemins avec espaces |
| Liens symboliques | aucun lien symbolique détecté dans les destinations de travail auditées |
| Disque | 124 976 900 Kio disponibles, soit environ 119,2 Gio; volume utilisé à 87 % |

### Outils hôte

| Outil | État observé |
| --- | --- |
| CMake | 3.30.4 dans le `PATH`; répond au minimum 3.25 de Dusklight |
| Ninja | 1.11.1 |
| GNU Make | 3.81, fourni par macOS |
| Python | 3.12.2; `tomllib` disponible pour valider le manifeste |
| Compilateur | Apple Clang 15.0.0, cible arm64-apple-darwin |
| Xcode | Xcode et SDK macOS détectés sous `/Applications/Xcode.app` |
| Archives | bsdtar 3.5.3, unzip 6.00, p7zip 17.06 |
| Sommes | `shasum` et `sha256sum` disponibles |
| Réseau futur | `curl` système et `wget` présents; aucun téléchargement effectué |
| Java | absent; non requis pour PSPDEV, PPSSPP desktop ou le smoke test |
| PowerShell | absent; les scripts `.ps1` ne peuvent pas être exécutés localement sur cet hôte |
| Homebrew | présent mais non utilisé par cette mission |

Certaines commandes du `PATH` proviennent déjà de Homebrew ou de Miniconda. L'audit ne
les a ni mises à jour ni modifiées. Le bootstrap proposé ne doit pas dupliquer CMake,
Ninja ou Python tant que les versions existantes conviennent.

### PSPDEV et PPSSPP

- Aucun `psp-config`, `psp-gcc`, `psp-objdump`, `pack-pbp`, `psp-gdb` ou PSPLINK n'a
  été trouvé dans le `PATH` ou dans les emplacements globaux usuels audités.
- Aucun PPSSPP ou PPSSPPHeadless n'a été trouvé dans le `PATH`, `/Applications` ou le
  dossier Applications utilisateur.
- `.tools/`, `.cache/downloads/`, `build/`, `.test-data/ppsspp/`, `logs/` et
  `artifacts/` étaient vides hors fichiers `.gitkeep` au début de l'audit.

### Source Dusklight

- `dusklight-main/` est présent : environ 151 Mio et 19 175 fichiers.
- Le projet utilise CMake 3.25+, des presets CMake et une source sous licence CC0.
- `.gitmodules` déclare `extern/aurora`, mais ce sous-module n'est pas présent.
- Le CMake hôte ajoute Aurora et Freeverb et déclare notamment cxxopts, nlohmann/json,
  miniz, funchook/capstone et, selon la plateforme, libjpeg-turbo et Sentry.
- Le backend hôte attend D3D, Vulkan ou Metal. Il ne constitue pas un backend PSP et
  ne doit pas être compilé naïvement pour la console.
- Aucun portage du moteur ni modification de `dusklight-main/` n'a été entrepris.

### Donnée à statut juridique non confirmé

Un fichier de 1 459 978 240 octets nommé comme une image commerciale existe déjà dans
`game iso/`. Il n'a pas été ouvert, haché, copié ou utilisé. Il reste exclu de tout test
automatisé jusqu'à confirmation par l'utilisateur qu'il est légalement autorisé à
l'utiliser. Le smoke test créé dans `test/smoke/` n'en dépend pas.

## Informations vérifiées auprès des sources officielles

- La publication officielle [PSPDEV v20260701](https://github.com/pspdev/pspdev/releases/tag/v20260701)
  propose un bundle macOS arm64 de 148 Mo. GitHub publie le SHA-256 enregistré dans
  `toolchain/manifest.lock`.
- PSPDEV recommande ses bundles complets plutôt que `psptoolchain` seul pour installer
  tout l'environnement. La compilation de `psptoolchain` depuis les sources réclame
  une chaîne importante de dépendances hôte et impose un chemin PSPDEV absolu sans
  espace ni caractère spécial.
- [PSPSDK](https://github.com/pspdev/pspsdk) fournit libGU/libGUM, newlib glue,
  `psp-config`, les outils PRX/PBP et les bibliothèques nécessaires au test.
- [PPSSPP v1.20.4](https://github.com/hrydgard/ppsspp/releases/tag/v1.20.4), commit
  `fa50bb1976065c4f8b1b47af227d367fe9771555`, est la version stable épinglée.
- La documentation macOS de PPSSPP situe son état utilisateur sous `.config/ppsspp`.
  Le lanceur redéfinit donc `HOME` et `XDG_CONFIG_HOME` vers `.test-data/ppsspp/`.
- PSPDEV documente [PSPLINK et psp-gdb](https://pspdev.github.io/debugging.html) pour
  le débogage ultérieur sur console physique.

## Hypothèses et choix proposés

- Le bundle PSPDEV officiel est préférable à un build source pour la première boucle :
  moins de dépendances hôte, empreinte connue et installation locale simple.
- Le PPSSPP SDL officiel suffit au test fonctionnel initial. Une construction source
  ou headless n'est justifiée que si le processus GUI ne fournit pas une automatisation
  assez stable.
- La réussite PPSSPP est déterminée par un fichier-marqueur écrit par le homebrew, pas
  par le code de sortie de l'interface graphique.
- Les outils de conversion d'assets seront de petits outils maison sous `tools/`,
  choisis après que les formats PSP requis auront été mesurés.

## Bloqueurs et limites

- PSPDEV et PPSSPP ne sont pas encore installés; aucune compilation PSP n'est possible.
- `extern/aurora` manque pour un build hôte complet de Dusklight. Cela ne bloque pas le
  test PSPSDK autonome.
- PowerShell est absent : seule la cohérence statique des scripts `.ps1` peut être
  examinée sur ce Mac.
- La contrainte amont PSPDEV sur les chemins spéciaux limite la portabilité réelle de
  la toolchain. Les scripts restent sûrs avec ces chemins, mais refusent l'installation
  PSPDEV quand l'amont ne peut pas la supporter.
- PPSSPP n'est pas une preuve de parité du Media Engine, des caches, du DMA ou du timing.

## Espace disque proposé

| Usage | Estimation prudente |
| --- | ---: |
| Archives PSPDEV + PPSSPP | 0,25 Gio |
| PSPDEV extrait et bibliothèques | 1,5 à 2,5 Gio |
| PPSSPP et profil de test | 0,2 Gio |
| Builds PSP, symboles et journaux initiaux | 0,5 à 1 Gio |
| Réserve minimale recommandée | 4 Gio |
| Avec builds PPSSPP/toolchain depuis les sources | 8 à 12 Gio |

L'espace actuel est suffisant, mais le volume est déjà rempli à 87 %; les builds source
volumineux doivent rester différés.

## Opérations exigeant une nouvelle autorisation

1. Exécuter `scripts/bootstrap-tools.sh --download-only` ou son équivalent PowerShell.
2. Extraire les archives vérifiées avec `--install`.
3. Exécuter pour la première fois `psp-gcc`, `psp-cmake`, PPSSPP ou tout autre binaire
   nouvellement téléchargé.
4. Utiliser l'image de jeu existante ou toute autre donnée au statut juridique incertain.
5. Installer une éventuelle dépendance système; aucune n'est indispensable au plan
   préconisé sur l'hôte audité.
