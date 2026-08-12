# Matrice de portage PSP

## Cible produit

Le port vise une boucle gameplay représentative avant toute optimisation agressive. La
qualité visuelle peut être réduite, mais les règles de jeu et les collisions doivent
rester les plus proches possibles de la source disponible.

## Matrice sous-système

| Sous-système | Politique | Première étape | Validation | Budget cible initial |
| --- | --- | --- | --- | --- |
| plateforme/fenêtre | remplacer | `sceKernel` + callbacks | EBOOT lance/quitte proprement | < 128 Kio code/data spécifique |
| input | adapter | `sceCtrlReadBufferPositive` | stick + boutons dans smoke gameplay | < 0,1 ms/frame |
| noyau gameplay | conserver/extraire | état joueur, scène, flags | test hôte puis PSP | < 4 ms/frame |
| collision | conserver/adapter | triangles statiques + capsule/point Link | test déterministe | < 2 ms/frame scène simple |
| renderer | remplacer | libGU/GUM opaque + alpha test | scène synthétique | 30 FPS min boucle initiale |
| assets | convertir offline | mesh + texture + collision | hash + loader test | scène de test < 2 Mio |
| textures | convertir offline | 5650/5551/4444/CLUT | capture PPSSPP | EDRAM bornée |
| UI gameplay | adapter | rectangles/sprites simples | barre/compteur visible | < 0,5 ms/frame |
| debug/ImGui | supprimer release | compteurs texte dev | flag build | zéro en release |
| I/O | adapter | manifest + lectures sync | load/unload scène | pas de heap croissant |
| threads | simplifier | aucun worker initial | comportement identique | zéro contention |
| audio | différer/remplacer | silencieux au premier jalon | N/A puis smoke audio | budget après profil |
| shaders | supprimer runtime | équivalent états GU | revue visuelle | zéro compilateur runtime |
| compressions | mesurer | offline d'abord | temps/poids comparés | pas de pic > budget heap |
| sauvegarde | adapter plus tard | chemin Memory Stick | test cycle save/load | après gameplay |
| exceptions | éliminer chemin PSP | codes d'erreur | tests négatifs | `-fno-exceptions` si possible |

## Jalons

### J0 — socle

- toolchain PSP validée;
- smoke GU/GUM/input opérationnel sous PPSSPP;
- profil PPSSPP isolé;
- rapports et budgets initiaux.

### J1 — noyau gameplay hôte

- extraire une bibliothèque sans GPU/window/ImGui;
- test synthétique de mouvement de Link et collision;
- formats de scène minimaux définis;
- aucune dépendance Aurora backend dans la cible PSP.

### J2 — gameplay synthétique PSP

- même test de mouvement/collision sur PSP;
- input réel PSP;
- renderer minimal d'un sol/obstacle/acteur;
- métriques CPU/heap/draws enregistrées.

### J3 — pipeline assets

- convertisseur mesh/collision;
- convertisseur textures;
- manifeste versionné;
- test de corruption et de taille;
- cache simple borné.

### J4 — scène réelle autorisée

- seulement avec des données dont l'usage a été explicitement confirmé;
- comparer logique hôte/PSP sur un échantillon;
- profiler mémoire/FPS;
- corriger les divergences avant l'effet visuel.

### J5 — services console

- sauvegarde Memory Stick;
- audio;
- streaming si nécessaire;
- instrumentation matérielle PSPLINK/`psp-gdb`.

## Critères « done » par adaptation

Une brique n'est pas considérée portée sans :

1. build reproductible;
2. test hôte quand la logique est portable;
3. test PSP/PPSSPP;
4. échec explicite sur entrée invalide;
5. budget mémoire connu;
6. journal ou marqueur de preuve;
7. document de provenance pour toute dépendance ou donnée.

## Garde-fous de fidélité

- Les conversions d'assets ne doivent pas changer les coordonnées gameplay sans
  transformation documentée et inversible.
- La collision est validée indépendamment du rendu.
- Les shaders simplifiés ne doivent pas être utilisés comme justification d'un changement
  de logique.
- Les optimisations à précision réduite sont derrière un drapeau et comparées à la version
  hôte de référence.
- Les scènes tests synthétiques précèdent toute donnée de jeu externe.

## Budgets provisoires

Ces budgets servent d'alarme, pas de promesse :

- EBOOT + code + statiques : viser < 8 Mio;
- heap runtime/ressources : réserver une marge importante sous 32 Mio;
- textures et display buffers : suivre séparément l'EDRAM;
- scène test complète : viser < 4 Mio avant scène réelle;
- 30 FPS comme première cible fonctionnelle; 60 FPS n'est pas une contrainte initiale.

Ils seront remplacés par des mesures dès J2.
