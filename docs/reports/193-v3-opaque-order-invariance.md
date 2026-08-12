# V3C — preuve d’invariance de l’ordre opaque F_SP108

Classification : `READY_OPAQUE_ORDER_BEHAVIORAL_EVIDENCE`

## Identité

- commit PSP : `d5f79ffbbfe6f973144077e100ffc4f43e5887e5` ;
- `VISUAL_BUILD_ID` : `sha256:1da9997b1242eed7248ea87d8f5c12c6fd5304161ce294910e0ea363265e9057` ;
- backend : OpenGL ; renderer PPSSPP : software ; profil : `opaque_only` ; scène : F_SP108, room 0, layer 0 ; seed : `0x4455534B` ; réseau : non utilisé ; release complète : non exécutée (`release_runs=0`).

## Protocole borné

Le chemin diagnostique est séparé du rendu canonique. Il fige un unique état de simulation, exécute un frame de chauffe sans mise à jour supplémentaire, puis mesure un frame du même état. Les trois lancements distincts utilisent `source_order`, `reverse_order`, `deterministic_permutation`.

Seules les soumissions opaques avec depth-test et depth-write actifs sont éligibles. Les cinq matériaux opaques sans écriture de profondeur sont exclus et ne sont jamais permutés. Chaque draw réapplique l’état GU complet.

## Résultats

| Mesure | Valeur |
|---|---:|
| draws mesurés | 39 |
| draws room | 12 |
| draws Link | 27 |
| draws sans depth-write exclus | 5 |
| frame de chauffe | 1 |
| frame mesuré | 1 |
| mises à jour de simulation | 1 |
| différences de pixels | 0 |
| mismatches d’occlusion | 0 |
| erreurs | 0 |

Les trois framebuffers ont le même SHA-256 : `e762c3a88f9d3a1f93934add5ecdec6e11b652d4ce787cda76f7fffcedd21cee`.

Le validateur confirme également : même identité visuelle, même caméra, même géométrie, mêmes matériaux et même politique de profondeur.

## Démarrage froid

La première acquisition après reconstruction du bundle GUI a capturé un premier frame hôte transitoire. Une répétition identique à chaud a rejoint exactement les deux autres hashes. Le correctif générique est un frame de chauffe borné sans nouvelle mise à jour de simulation. Les trois acquisitions ont ensuite été régénérées avec le nouveau build et validées dès leur première tentative.

## Portes

- `V3C_OPAQUE_ORDER_INVARIANCE=DONE` ;
- `V3C_DEPTH_THRESHOLD_WITNESSES=WAITING_AGENT_IMPLEMENTATION` ;
- `V3C_HOST_QUANTIZED_DEPTH_ORACLE=WAITING_AGENT_IMPLEMENTATION` ;
- `V3D_DEPTH_VISUAL_VALIDATION` reste ouverte ;
- `DEPTH_PIPELINE.OK` n’est pas émis ;
- `V4C_ALPHA_RUNTIME_INTEGRATION` reste verrouillée.

La prochaine correction autorisée est l’implémentation générique des douze témoins de seuil et de l’oracle hôte quantifié. Aucun changement alpha n’est autorisé avant validation complète de V3D.
