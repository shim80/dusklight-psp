# Résultat du smoke 3D corrigé

Date : 2026-07-16

## Statut

`READY_FOR_DUSKLIGHT_ASSET_BRIDGE`

Le smoke 3D initial était techniquement valide mais visuellement trompeur : la
caméra regardait exactement le plan `z=0` et les objets avaient une profondeur
de ±1 unité, donc la vue s'effondrait pratiquement sur une face. Cette passe a
corrigé le smoke synthétique avant de toucher aux formats Dusklight.

## Corrections

- caméra déplacée vers `z=-6`, centre sur `z=0`, up `+Y`;
- depth test libGU conservé (`GU_GEQUAL` avec `sceGuDepthRange(65535, 0)`);
- texture checker rendue en 64×64 puis swizzlée avec vérification des tuiles;
- damier agrandi à 8×8 pixels par case pour rendre le contrôle visuel clair;
- coupe GUM non utilisée, clipping PSP laissé dans le chemin normal;
- aucun changement du noyau gameplay.

## Validation hôte

`test/gu-host/gu_math_host_tests` vérifie :

- matrice look-at;
- matrice perspective avec aspect 480/272;
- ordre world → view → projection;
- clipping proche/lointain;
- profondeur normalisée de trois points de test;
- convention de profondeur PSP attendue.

Résultat :

```text
DUSKLIGHT_PSP_GU_MATH_HOST_TESTS_OK
```

## Build et inspection PSP

Le build PSP reste basé sur libGU/libGUM, sans backend Aurora. L'inspection
statique vérifie maintenant également la présence des appels perspective et
look-at.

Résultat :

```text
DUSKLIGHT_PSP_GU_SMOKE_STATIC_OK
```

## Run PPSSPP

Le runner ne juge le succès qu'après apparition de `GU.OK` avec le jeton exact.
Le dernier run manuel utilisateur montre la scène synthétique en vraie
perspective 3D et le damier attendu sur le cube.

L'automatisation graphique du terminal contrôlé reste limitée par macOS, mais
le marqueur et la capture utilisateur confirment l'exécution PSP/PPSSPP.

## Suite autorisée

Le prochain travail peut porter sur des packages convertis Dusklight :

1. format de modèle statique PSP;
2. format texture PSP;
3. format collision;
4. loader borné avec validation structurelle;
5. scène synthétique chargée depuis ces packages;
6. seulement ensuite ressources Dusklight autorisées.
