# Audit du pont renderer Dusklight vers PSP

Date : 2026-07-16

## Statut

`READY_FOR_NOD_SOURCE_BRIDGE`

Le renderer PSP ne dépend pas du runtime GPU moderne Dusklight. Le bon point de
coupure est désormais mesuré :

- **entrée du pont** : objets/parsers Dusklight ou fichiers hôte via outils
  offline;
- **sortie du pont** : packages `.dprm`, `.dptx`, `.dpcl`, puis `.dpck`;
- **runtime PSP** : loaders bornés + libGU/libGUM;
- **hors runtime PSP** : shaders modernes, pipelines abstraits, compute,
  ImGui, funchook/capstone, Sentry, réflexion GPU.

## Graphe de rendu observé

`GameState` et les objets `World` font passer les ressources source dans des
objets Aurora :

```text
ResourceManager
  ├─ WorldModel / RoomModel
  ├─ DynamicModel
  ├─ textures / animations / materials
  └─ CollisionMesh
       ↓
IGraphicsBackend
  ├─ graphics pipelines
  ├─ compute pipelines
  ├─ vertex / index / uniform / storage buffers
  ├─ textures / samplers
  └─ draw / dispatch
```

Cette pile suppose des capacités qui ne se traduisent pas proprement en libGU :

- descripteurs et bindings de ressources;
- storage buffers;
- compute skinning;
- réflexion de shaders;
- pipelines précompilés par combinaison shader/matériau;
- formats texture riches;
- debug draw/ImGui.

Il ne faut donc pas essayer d'implémenter `IGraphicsBackend` au complet sur PSP.

## Scène statique

Pour un objet statique, le contrat PSP actuel suffit avec :

```text
source Dusklight
  → positions / normales si requises / UV
  → indices
  → matériau simplifié
  → textures converties
  → collision séparée
  → DPRM + DPTX + DPCL
```

Le loader PSP doit rester aveugle au format source original. Le convertisseur
hôte porte la responsabilité de l'interprétation et de la perte de qualité.

## Modèles animés

`Graphics/DynamicModel.cpp` utilise un compute shader de skinning. Le contrat
PSP ne doit pas reproduire ce compute :

- convertir la hiérarchie de squelette offline;
- conserver bind pose et poids bornés;
- évaluer l'animation sur CPU PSP au début;
- écrire les sommets skinés dans un buffer dynamique;
- mesurer avant toute optimisation VFPU ou précalcul.

Aucune promesse de budget n'est faite tant qu'un acteur réel n'a pas été
profilé.

## Matériaux

Les matériaux desktop sont plus riches que le contrat GU disponible. La
politique recommandée est un classement offline vers un petit enum PSP :

| Classe | Règle PSP |
| --- | --- |
| opaque | texture + depth write |
| alpha-test | texture RGBA + alpha test |
| blended | texture RGBA + blend, depth test sans write si nécessaire |
| unlit | texture/couleur sans lumière |
| lit-simple | lumière diffuse/speculaire fixe GU |
| unsupported | erreur de conversion explicite |

La classification doit être incluse dans le DPRM, pas recalculée au runtime.

## Textures

Le runtime source peut envoyer des formats/tailles inadaptés. Le convertisseur
PSP doit :

- décoder côté hôte;
- appliquer dimensions PSP légales;
- sélectionner 5650 / 5551 / 4444 / CLUT;
- générer directement le layout swizzlé PSP;
- écrire dimensions logiques et dimensions de storage;
- refuser une texture dépassant le budget configurable.

Le runtime PSP ne doit pas disposer d'un décodeur image général.

## Collision

`Collision/Collision.cpp` consomme finalement des triangles et les transforme
en monde. Le DPCL actuel correspond bien à cette frontière :

- triangles bruts convertis offline;
- bounds pré-calculés;
- aucune dépendance au mesh de rendu;
- transforms gameplay validées indépendamment.

Les simplifications de rendu ne doivent jamais simplifier implicitement la
collision.

## Scènes

Le premier bridge réel devrait produire un DPCK de scène :

```text
scene.dpck
├─ room.dprm
├─ room.dptx
├─ room.dpcl
├─ actor_*.dprm
├─ actor_*.dptx
└─ manifest scène
```

Le gestionnaire PSP charge/unload par scène. Aucun worker n'est nécessaire au
premier passage.

## Transforms à verrouiller

Avant conversion réelle, les tests doivent fixer :

- handedness;
- ordre des axes;
- unités monde;
- convention UV V;
- winding/culling;
- matrices acteur/modèle;
- collision vs rendu.

Le pipeline synthétique utilise la même convention numérique de positions et
retourne explicitement les bounds, mais il ne prouve pas la convention source
Dusklight.

## Source manquante pour le vrai bridge

Le checkout `dusklight-main/` n'inclut pas actuellement l'implémentation Nod
référencée par les fichiers `nod/*`. Les headers seuls ne suffisent pas à écrire
un convertisseur fiable des formats source. Il serait risqué de deviner ces
formats depuis les appels renderer.

Le prochain changement utile est donc d'ajouter une source Nod légalement
compatible avec le projet ou de documenter un format intermédiaire officiellement
produit par Dusklight. Sans cela, le renderer PSP peut avancer sur packages
synthétiques mais pas sur un asset source fidèle.

## Ordre recommandé

1. rendre l'implémentation Nod disponible au convertisseur hôte;
2. écrire des tests de lecture source avec petits fixtures autorisés;
3. verrouiller les transforms et matériaux;
4. produire un DPCK source;
5. valider le package sur hôte;
6. charger/rendre sous PPSSPP;
7. mesurer mémoire/FPS;
8. seulement ensuite optimiser.
