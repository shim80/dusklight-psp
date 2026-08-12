# Audit de l'architecture Dusklight pour un port PSP

## Résumé

Le frontend desktop actuel est intentionnellement très mince. `src/main/main.cpp`
crée une fenêtre 800×600 et trois interfaces via Aurora (`IWindow`,
`IGraphicsBackend`, `IInputBackend`), branche les contrôleurs, puis appelle
`Application::Update`, `GraphicsBackend::Update` et l'UI ImGui à chaque frame.

Cette forme est utile pour le port : le cœur `Application` n'est pas le même objet
que le backend de fenêtre/GPU, et la logique de scène ne dépend pas directement
d'ImGui. En revanche, le CMake racine et `extern/aurora` restent desktop-centric
et doivent être contournés plutôt que forcés à compiler sur PSP.

## Sous-systèmes et dépendances

| Sous-système | Source principale | Dépendances desktop observées | Statut PSP proposé |
| --- | --- | --- | --- |
| boucle et orchestration | `src/main/main.cpp`, `src/main/Application.*` | Aurora window/input/graphics, ImGui | **adapter** : boucle PSP native, réutiliser `Application` par morceaux |
| progression et gameplay | `Application.cpp`, `GameState.cpp`, `World/Actors.*`, `Collision/Collision.cpp` | surtout Aurora math/input/graphics abstrait | **conserver / adapter** : priorité gameplay |
| gestion ressources | `ResourceManager.*`, `Util/Asset.cpp`, `Util/BinaryReader.hpp` | `std::filesystem`, `std::ifstream`, workers CPU, `lzo` | **remplacer/adapter** : VFS PSP, I/O synchrone ou file bornée |
| décompression | `Util/Compression.cpp` | `lzo` | **conserver si utile**, mais via un cache/offline quand possible |
| rendu monde | `World/Models.*`, `Graphics/DynamicModel.*`, `GameState.cpp` | `IGraphicsBackend`, pipelines, buffers, samplers, shaders | **remplacer** par libGU/GUM + formats convertis |
| shaders | `lib/shadercompiler`, `assets/shaders`, `static.shaders`, `AnimatedShaders` | compilateur hôte, réflexion shaders, API GPU moderne | **retirer du runtime PSP**; convertir offline |
| textures | `GameState.cpp`, `ResourceManager.cpp`, `Graphics*` | textures GPU modernes | **adapter** : swizzle/CLUT/4444/5650 offline |
| input | `main.cpp`, `GameState.cpp`, `Graphics/Camera.cpp` | `IInputBackend`, `EKey`, `EGamepadAxis` | **adapter** vers `sceCtrlReadBufferPositive` |
| audio | aucune boucle audio trouvée dans `src/main` | pas de backend audio Aurora utilisé directement ici | **différer**, remplacer par backend PSP ultérieur |
| debug/UI | `GameState.cpp`, `Graphics/Camera.cpp`, `main.cpp` | ImGui, debug draw, DebugMenu | **retirer/derrière drapeau** pour release PSP |
| OS/fichiers | `main.cpp`, `ResourceManager.cpp`, `Util/Asset.cpp` | `std::filesystem`, chemins host, exec dir | **adapter** vers `ms0:/`, `disc0:/` ou VFS local |
| threading | `ResourceManager.*` | `std::jthread`, mutex, CV, jobs async | **simplifier** : synchrone d'abord, workers PSP seulement si mesurés |
| exceptions | `Util/Asset.cpp`, `Compression.cpp`, `Util.hpp` | `throw std::runtime_error` | **éliminer du chemin PSP** ou compiler le sous-ensemble avec politique explicite |
| dépendances racine | `CMakeLists.txt`, `extern/aurora/CMakeLists.txt` | cxxopts, nlohmann, miniz, funchook, capstone, sentry, freetype, png, etc. | **ne pas lier en bloc**; importer uniquement les briques mesurées |

## Ce que `Application` réutilise vraiment

`Application` sait :

- appliquer les données de scène chargées;
- construire `WorldActor` / `WorldModel`;
- ajouter les meshes de collision;
- mettre à jour Link via `UpdateLinkActors`;
- exposer la position de Link et les polygones de collision;
- gérer flags d'événements, compteur d'objets et variables debug.

Ce code est la cible de portage initiale, mais il reste lié aux types
`IGraphicsBackend` et aux objets GPU dans ses signatures. Le premier vrai module PSP doit
donc extraire un noyau de scène/gameplay ou fournir un petit adaptateur compatible, sans
embarquer le backend desktop complet.

## GPU et shaders

Les appels observés dans `World/Models`, `Graphics/DynamicModel`, `GameState` et
`DynamicSceneParser` demandent des concepts modernes :

- pipelines graphiques et compute;
- buffers storage/uniform;
- descriptors/bindings;
- samplers et textures structurées;
- dispatch compute;
- shaders statiques et animés compilés offline.

Il n'est pas rationnel d'émuler cette API sur PSP. Le backend PSP doit aplatir les
états vers :

- listes de commandes GU;
- matrices GUM;
- vertex/index buffers statiques en mémoire adaptée;
- quelques états de matériau groupés;
- textures swizzlées et éventuellement CLUT;
- alpha test/blend limité;
- lumière et fog simplifiés.

Les matériaux et meshes doivent être convertis sur l'hôte. Les shaders desktop ne
doivent pas être compilés ou interprétés sur PSP.

## Ressources et I/O

`ResourceManager` charge en asynchrone via `std::filesystem::path` et `std::ifstream`,
puis garde des maps de `shared_ptr` de ressources. Cette approche est souple sur desktop
mais coûteuse pour un premier port PSP.

Ordre recommandé :

1. manifest d'assets convertis;
2. lecture synchrone bornée vers buffers préalloués;
3. cache LRU simple par scène;
4. seulement après mesure, un worker I/O PSP.

Pour les données compressées, la PSP ne doit pas refaire les conversions qui peuvent
être faites sur l'hôte. Si LZO reste nécessaire pour une ressource dynamique, l'intégrer
comme bibliothèque PSP mesurée, pas comme dépendance du CMake desktop complet.

## Input

L'input actuel lit des axes gamepad et plusieurs raccourcis clavier de debug. Pour PSP :

- stick analogique → vitesse/direction de Link;
- D-pad / boutons → actions principales;
- clavier/debug menu → supprimé en release;
- caméra libre debug → drapeau dev seulement.

La première étape de gameplay peut donc être validée sans reproduire tout le mapping
desktop.

## Audio

Aucun moteur audio actif n'a été observé dans `src/main` pendant cet audit. Il faut
confirmer lors de l'intégration réelle si l'audio arrive par un chemin externe. Le port
initial peut rester silencieux et concentrer les budgets sur gameplay/rendu. Le backend
audio PSP doit être conçu seulement après mesure du format source et des besoins de
streaming.

## CMake et Aurora

Le CMake racine :

- détecte Windows/Linux/macOS;
- ajoute `extern/aurora`;
- compile `lib/shadercompiler`;
- attend des packages/deps desktop;
- active Metal/Vulkan/D3D et des helpers debug.

Le CMake PSP doit donc être un projet séparé, par exemple `platforms/psp/CMakeLists.txt`,
appelé via `psp-cmake`, avec uniquement :

- le noyau portable explicitement listé;
- PSPSDK/libGU/libGUM;
- les loaders convertis;
- la plateforme PSP.

Il ne doit pas exécuter le CMake racine en mode PSP.

## Plan incrémental proposé

1. **Boucle PSP + contrôleur + GU** : déjà validé par le smoke test.
2. **Premier noyau gameplay** : Link, collision simple, changement de scène sans renderer moderne.
3. **Assets offline** : format de mesh/texture minimal et scène synthétique.
4. **Renderer monde** : opaque + alpha test, pas de shaders modernes.
5. **Scène réelle autorisée** : seulement après le pipeline synthétique.
6. **Mesure mémoire/FPS** : au premier gameplay réel.
7. **Audio/streaming/threads** : seulement après données de profiling.

## Performance et mémoire

Les décisions de budget doivent être instrumentées :

- high-water mark du heap;
- taille EDRAM et command list;
- nombre de draws et changements texture;
- mémoire par scène;
- temps CPU simulation / rendu / I/O;
- frames > 16,7/33,3 ms.

Avant ces mesures, aucune promesse 30/60 FPS n'est crédible.

## Risques de portabilité prioritaires

1. hiérarchie de dépendances desktop via Aurora;
2. API GPU moderne non transposable;
3. concurrence et `std::jthread`;
4. exceptions et STL dynamique sur une machine à mémoire limitée;
5. taille des assets et formats texture;
6. dépendance cachée à l'UI/debug desktop;
7. formats et streaming audio encore inconnus.

## Conclusion

Le moteur n'est pas « prêt PSP », mais sa séparation frontend/application rend une
migration par sous-systèmes crédible. La meilleure stratégie reste un **port PSP dédié**
qui réutilise le noyau gameplay mesuré et remplace explicitement fenêtre, GPU, input,
I/O et debug, plutôt qu'une tentative de compiler le runtime desktop complet.
