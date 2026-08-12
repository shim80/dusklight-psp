# Plan de toolchain PSP

## Matrice de décision

| Composant | Classe | Version proposée | Source/licence | Installation locale et taille | Dépendances, risques et vérification |
| --- | --- | --- | --- | --- | --- |
| Bundle PSPDEV arm64 | obligatoire | `v20260701` | [pspdev/pspdev](https://github.com/pspdev/pspdev), licences mixtes ouvertes | `.tools/pspdev`, archive 148 Mo, environ 1,5–2,5 Gio extrait | SHA-256 officiel puis présence de `psp-config`, `psp-gcc`, `psp-objdump`, `psp-cmake`, `pack-pbp`; chemin amont sans caractères spéciaux |
| Allegrex GCC/binutils/newlib | obligatoire | inclus dans PSPDEV `v20260701` | projets GNU et PSPDEV; GPL/LGPL avec exceptions selon composant | inclus dans le bundle | valider `elf32-littlemips`, symboles et création d'un ELF libre avant tout moteur |
| PSPSDK/libGU/libGUM | obligatoire | inclus dans PSPDEV `v20260701` | [pspdev/pspsdk](https://github.com/pspdev/pspsdk), BSD-compatible; exception GPL-3.0 pour PrxEncrypter | inclus dans `.tools/pspdev` | vérifier les en-têtes, bibliothèques, `psp-config` et l'exemple minimal |
| Outils PSP pack/debug | obligatoire | inclus dans PSPDEV `v20260701` | PSPDEV, majoritairement licences libres | inclus dans `.tools/pspdev` | `pack-pbp`, `psp-prxgen`, `psp-fixup-imports`, `psp-objdump`; PSPLINK et `psp-gdb` préparés mais non exécutés |
| CMake | obligatoire | 3.30.4 hôte | déjà installé; BSD-3-Clause | réutilisation, zéro téléchargement | Dusklight requiert 3.25+ |
| Ninja | recommandé | 1.11.1 hôte | déjà installé; Apache-2.0 | réutilisation | build déterministe et rapide |
| Python | obligatoire pour scripts/assets | 3.12.2 hôte | déjà installé; PSF | réutilisation | `tomllib` présent pour le manifeste |
| PPSSPP SDL | obligatoire | `v1.20.4`, commit `fa50bb1976065c4f8b1b47af227d367fe9771555` | [hrydgard/ppsspp](https://github.com/hrydgard/ppsspp), GPL-2.0+ | `.tools/ppsspp`, archive officielle 5,9 Mo | Metal explicite; config redirigée vers le dépôt; test fonctionnel par marqueur |
| PPSSPPHeadless/source build | optionnel | `v1.20.4` | même source/licence | différé; typiquement 1–3 Gio avec dépendances/build | n'ajouter que si le runner SDL n'est pas assez automatisable |
| GLSLang / SPIR-V | non requis initialement | — | ne pas installer | zéro | libGU/libGUM ne l'exige pas; utile seulement si un pipeline offline spécifique est adopté |
| Pillow | non requis initialement | — | ne pas installer | zéro | les premiers assets test sont générés en Python stdlib |
| PSPLINK + `psp-gdb` | préparé | bundle PSPDEV | libres | zéro supplément notable | réserver au matériel physique après validation PPSSPP |

## Stratégie de dépendances Dusklight

Le CMake desktop de Dusklight n'est pas la première cible PSP. Il ajoute Aurora, Freeverb,
cxxopts, JSON, miniz, funchook/capstone et des bibliothèques desktop. Le plan est donc :

1. compiler d'abord `test/smoke/`, cible PSPSDK autonome sans dépendance moteur;
2. auditer les sous-systèmes Dusklight nécessaires au gameplay;
3. isoler le code portable dans une bibliothèque PSP ou un adaptateur CMake dédié;
4. remplacer les backends GPU/audio/OS par libGU/libGUM, audio PSP et stockage PSP;
5. n'introduire les dépendances externes qu'après mesure du besoin et de l'empreinte.

Cette séparation évite de tenter de compiler Metal, Vulkan, Sentry, funchook ou le runtime
desktop sur PSP.

## Pipeline d'assets prévu

Le pipeline est intentionnellement séparé de `dusklight-main/` :

- entrée : ressources dont l'utilisateur a explicitement le droit d'usage;
- outils : scripts maison sous `tools/`, exécutés sur l'hôte;
- sorties intermédiaires : `build/assets/` ou `.test-data/assets/`;
- sorties PSP : textures quantifiées/palettisées et maillages/animations compactés;
- validation : dimensions, format, poids, hash et chargement par un test PSP minimal;
- cache : local au dépôt et invalidable par version d'outil.

Le smoke test actuel génère ses propres assets libres et ne requiert aucun asset du jeu.

## Règles de chemin et idempotence

- Tous les scripts dérivent la racine via leur propre chemin et `pwd -P`.
- Les téléchargements doivent aller dans `.cache/downloads/` et être hachés avant extraction.
- Les installations doivent rester dans `.tools/`; aucun `sudo`, `/usr/local` ou paquet Homebrew.
- Un second passage doit refuser d'écraser un fichier incompatible et réutiliser les
  archives/outils dont le hash attendu est déjà vérifié.
- PSPDEV est une exception amont : son script de build refuse espaces/caractères spéciaux.
  Le bootstrap contrôle cette précondition avant installation.

## Séquence future proposée

### Téléchargement seulement

```bash
scripts/bootstrap-tools.sh --download-only
```

Cette action, non exécutée pendant l'audit, téléchargerait uniquement les deux archives
épinglées après accord explicite, puis vérifierait les SHA-256.

### Installation locale

```bash
scripts/bootstrap-tools.sh --install
```

Après vérification, cette action extrairait les archives dans `.tools/pspdev` et
`.tools/ppsspp`.

### Smoke test

```bash
source scripts/env.sh
scripts/build-smoke.sh
scripts/inspect-smoke.sh
scripts/run-ppsspp-smoke.sh
```

Le run PPSSPP doit écrire `.test-data/ppsspp/smoke/SMOKE.OK`. Le launcher quitte ensuite
l'émulateur et renvoie un échec si ce marqueur n'existe pas ou ne contient pas le jeton
attendu.

## Vérification actuelle sans installation

`scripts/test-layout.sh` vérifie :

- l'analyse du manifeste et ses champs critiques;
- le refus d'un chemin PSPDEV non compatible;
- l'idempotence du bootstrap simulé avec `--plan`;
- la génération des assets factices libres;
- l'absence de références accidentelles à l'image utilisateur dans le smoke test;
- la cohérence de l'environnement si les outils sont absents.

## Observabilité et symboles

Le profil `RelWithDebInfo` est retenu. Le build doit conserver un ELF non stripé dans
`build/smoke/` et empaqueter un `EBOOT.PBP` normal pour PPSSPP. `psp-objdump -h -t` est
le premier outil d'inspection. Les symboles restent dans le dépôt de build, jamais dans
le PBP release tant que la taille n'est pas maîtrisée.

## Points ouverts après l'audit

- taille exacte du bundle PSPDEV une fois extrait;
- nécessité réelle de construire PPSSPPHeadless;
- format final des assets Dusklight sur PSP;
- budget VRAM/EDRAM du premier rendu moteur;
- politique audio et streaming une fois le backend desktop isolé.
