# Résultat du smoke gameplay PSP

Date : 2026-07-16

## Statut

`READY_FOR_GAMEPLAY_ASSET_BRIDGE`

Un noyau gameplay C++ sans fenêtre, GPU moderne, ImGui, filesystem desktop ni
threading a été ajouté sous `dusklight-main/platforms/psp/core/` et validé à la
fois sur l'hôte et dans un EBOOT PSP. Le noyau gère une scène synthétique avec :

- Link;
- collision avec obstacle;
- ramassage d'objets;
- drapeau d'événement;
- transition vers une seconde scène;
- compteurs de pas, clamps collision, collectibles, transitions et high-water
  de l'arène.

Les règles de mouvement sont partagées entre le test hôte et le smoke PSP. Aucun
résultat du moteur desktop n'est revendiqué comme preuve de parité; il s'agit
d'une base de portage indépendante et déterministe.

## Tests exécutés

### Tests hôte

`test/core-host/core_host_tests.cpp` couvre :

- déplacement libre;
- normalisation diagonale;
- blocage/clamp par obstacle;
- anti-tunneling avec grand pas;
- collecte unique et drapeau associé;
- transition de scène après prérequis;
- reset contrôlé de l'arène;
- déterminisme sur deux répétitions de la même séquence.

Dernier résultat :

```text
DUSKLIGHT_PSP_CORE_HOST_TESTS_OK
```

### Build PSP

Le build `test/core-psp/` partage `gameplay_core.cpp` et produit :

```text
build/core-psp/core_smoke.elf
build/core-psp/EBOOT.PBP
```

`psp-objdump` confirme :

- ELF32 little-endian MIPS;
- machine MIPS R3000;
- ELF non stripé;
- sections PSP attendues;
- symbole `main` présent;
- chaînes de succès et erreur présentes dans le PBP.

### Exécution PPSSPP

Le premier essai après le port gameplay a échoué parce que le runner continuait
à lancer `DUSKLIGHT_SMOKE`. Le script `run-ppsspp-smoke.sh` a été rendu générique
avec options `--eboot`, `--game-id`, `--marker`, `--expected`, `--run-label` et
`--timeout`.

La relance `core-valid` a écrit le marqueur :

```text
DUSKLIGHT_PSP_CORE_OK
```

dans :

```text
.test-data/ppsspp/memstick/PSP/GAME/DUSKLIGHT_CORE/CORE.OK
```

Le smoke PSP a donc réellement exécuté le noyau gameplay partagé et validé la
séquence synthétique complète.

## Mesures

Dernières tailles observées :

| Élément | Taille |
| --- | ---: |
| `gameplay_core.cpp` | 13 306 octets |
| `core_smoke.elf` | 1 909 268 octets |
| `EBOOT.PBP` | 791 407 octets |
| arène du test | 1 024 octets |
| high-water observé | 192 octets |
| simulation smoke | 77 pas |
| collectibles | 1 |
| transitions | 1 |
| collisions/clamps | 49 |

## Dépendances

La cible PSP du noyau utilise uniquement :

- STL minimale (`array`, `cstdint`, `cstring`, `cmath`, `type_traits`);
- PSPSDK pour le wrapper test;
- le noyau sous `dusklight-main/platforms/psp/core/`.

Elle n'utilise pas :

- Aurora;
- ImGui;
- Vulkan/Metal/D3D;
- exceptions;
- RTTI;
- filesystem;
- threads;
- assets commerciaux.

## Analyse de fidélité

Le noyau n'est pas encore la logique originale de Dusklight. Il a pour but de
fixer la frontière PSP : simulation, collision, allocations et format de scène.
Les prochains changements doivent remplacer progressivement les comportements
synthétiques par les données et règles source, avec tests différentiels explicites.

Le risque principal reste l'absence d'un pont asset : `Application` et les
parseurs desktop restent liés à Aurora et aux formats/runtime hôte. Le prochain
jalon doit donc définir un format converti pour une scène petite, sans importer
toute la pile graphique desktop.

## Régressions et erreurs observées

- Aucun nouvel artefact hors dépôt n'a été créé.
- Aucun asset externe n'a été lu.
- Une erreur de sélection de l'EBOOT du runner a été corrigée et couverte par le
  lancement final.
- Le marker PPSSPP reste la preuve d'exécution; le code de sortie GUI n'est pas
  utilisé comme verdict.
