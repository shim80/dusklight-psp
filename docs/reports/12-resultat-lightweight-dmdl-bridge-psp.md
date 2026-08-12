# Résultat — couture légère `dMdl_obj_c` et bridge statique PSP

## Classification

`READY_WITHOUT_AURORA_FOR_INSTANCE_BRIDGE`

Cette classification concerne uniquement le bridge d'instance
`::dMdl_obj_c`. Elle ne signifie pas que `::dMdl_c`, J3D, GX, TEV ou le renderer
Dusklight complet fonctionnent sans Aurora.

## Baseline et traçabilité

| Élément | Valeur |
|---|---|
| Commit de départ | `810a0adb8a2d51f3491f5c4886f6d04060316ac3` |
| Tag de départ | `psp-render-bridge-audit-v1` |
| Décision | `docs/decisions/0006-lightweight-dmdl-object-seam.md` |
| Commit de couture | `57c2413` — `Extract lightweight dMdl_obj_c integration seam` |
| Commit du bridge | `b9fb29f` — `Add PSP dMdl_obj_c static render bridge` |
| Suite finale | `scripts/test-psp-smokes.sh --timeout 30` |
| Journal global | `logs/ppsspp/automated-smokes-20260724T135822Z.log` |
| Profil PPSSPP | `.test-data/ppsspp/` exclusivement |
| Réseau | aucun accès |
| Aurora | non téléchargée |

Le dépôt était propre après les deux commits d'implémentation et avant la création de
ce rapport. Aucun produit de build, DPSM généré, journal ou état PPSSPP n'est suivi.

## Type historique

La définition originale dans `dusklight-main/include/d/d_model.h` était :

```cpp
class dMdl_obj_c {
public:
    dMdl_obj_c() : mpObj(NULL) {}
    MtxP getMtx() { return mMtx; }
    void setMtx(Mtx mtx) { cMtx_copy(mtx, mMtx); }

    /* 0x00 */ Mtx mMtx;
    /* 0x30 */ dMdl_obj_c* mpObj;
};
```

`::dMdl_c::entryObj()` chaîne les objets avec `mpObj`.
`::dMdl_c::draw()` lit `getMtx()`, concatène la matrice à la vue puis soumet le
shape. Le rôle historique est donc bien celui d'une instance rigide.

## Définition après extraction

L'unique définition se trouve désormais dans
`dusklight-main/include/d/d_model_obj.h`. `d/d_model.h` inclut ce fichier et conserve
les signatures :

```cpp
dMdl_obj_c() : mpObj(NULL) {}
MtxP getMtx()
void setMtx(Mtx mtx)
```

Les membres restent, dans le même ordre :

```cpp
/* 0x00 */ Mtx mMtx;
/* 0x30 */ dMdl_obj_c* mpObj;
```

La matrice n'est toujours pas initialisée par le constructeur. Seul `mpObj` est
initialisé à null. Il n'existe qu'une définition de la classe dans le snapshot.
`d_model.cpp`, `dMdl_c::entryObj`, `mpModelObj` et les acteurs existants utilisent
toujours exactement `::dMdl_obj_c`.

La copie historique via `cMtx_copy`/`PSMTXCopy` est remplacée uniquement dans
`setMtx()` par une copie exacte de `sizeof(Mtx)`, sans conversion ni allocation. Le
probe utilise douze motifs binaires distincts et valide la copie avec `memcmp`, y
compris une auto-copie.

## Provenance matricielle

Le type canonique local vient de
`dusklight-main/libs/dolphin/include/dolphin/mtx.h` :

```cpp
typedef f32 Mtx[3][4];
typedef f32 (*MtxPtr)[4];
```

Le nom historique `MtxP`, auparavant fourni par JMath, est exactement
`f32 (*)[4]`. Le nouvel en-tête l'associe à `MtxPtr` et vérifie leur identité avec un
`static_assert`.

## Disposition ABI

| Contrat | Hôte 64 bits | PSPSDK/PSP |
|---|---:|---:|
| `sizeof(float)` | 4 | 4 |
| dimensions de `Mtx` | 3 × 4 | 3 × 4 |
| `sizeof(Mtx)` | 48 | 48 |
| `offsetof(mMtx)` | 0 | 0 |
| `offsetof(mpObj)` | 48 / `0x30` | 48 / `0x30` |
| `sizeof(void*)` | 8 | 4 |
| `sizeof(dMdl_obj_c)` | 56 | 52 / `0x34` |
| standard-layout | oui | oui |

La disposition hôte a été obtenue avec le dump de layout du compilateur ; la
disposition PSP est imposée et validée par les `static_assert` compilés par
`psp-g++`. Aucun packing et aucun pointeur entier artificiel ne sont utilisés.

## Fermeture d'en-têtes

### Avant

```text
d/d_model.h
├─ J3DPacket.h ── J3DSys.h ── JMath.h
│                              └─ helpers/math.h
│                                  └─ dolphin/ppc_math.h (Aurora, absent)
└─ m_Do_mtx.h ── JMath.h ─────────┘
```

### Après

Les fichiers `.d` hôte et PSPSDK contiennent uniquement, hors en-têtes standard :

```text
test/model-obj-seam/model_obj_probe.cpp
d/d_model_obj.h
dolphin/mtx.h
dolphin/types.h
```

Le test refuse explicitement `extern/aurora`, `J3DGraphBase`, `J3DPacket`,
`GXAurora`, `ppc_math.h` et GX. Les résultats sont :

- fermeture légère : succès ;
- probe hôte et copie bit à bit : succès ;
- probe PSPSDK, pointeur 32 bits et taille `0x34` : succès ;
- définition unique et non-régression source de `d_model.h` : succès.

## Limites sans Aurora

Le build complet Dusklight reste impossible dans ce snapshot :

- `::dMdl_c` n'est pas compilé par la cible bridge ;
- J3D n'est pas porté ;
- GX et TEV ne sont pas portés ;
- les display lists, matériaux, shapes et textures J3D ne sont pas traduits ;
- aucune compatibilité avec une révision amont inconnue n'est affirmée.

Aurora reste nécessaire pour restaurer le renderer complet selon la décision 0005.

## Architecture du bridge

```text
const ::dMdl_obj_c&
const StaticResourceBinding&
        │
        ▼
adapt_static_instance
        │ matrice copiée, ressource conservée comme vue
        ▼
PspStaticRenderCommand
        │
        ▼
submit_static_command
        │
        ▼
static_mesh_3d existant
        │
        ▼
PSP GU
```

Les types `StaticResourceBinding`, `StaticRenderState`,
`PspStaticRenderCommand`, `Camera`, `ModelMatrix` et les métriques sont sous
`dusk::psp::render_bridge`. Ils sont explicitement des types de backend PSP et ne sont
pas présentés comme des types historiques Dusklight.

Le main du smoke :

- crée un vrai `::dMdl_obj_c` ;
- remplit sa matrice avec `setMtx()` ;
- n'inclut pas `static_mesh_3d.hpp` ;
- ne lit pas `mMtx` ;
- n'appelle ni `submit_indexed`, ni `sceGumDrawArray`, ni une fonction GU de dessin ;
- remet uniquement la commande au backend.

Le backend est le seul nouveau propriétaire de la conversion matrice et du submit. Le
smoke PSP interdit toute réinterprétation/mutation via `const_cast` ou C-style cast.

## Runtime et ownership

Les tests hôte vérifient :

- l'instance est observée comme vue `const` ;
- la ressource fournie par l'appelant est observée comme vue `const` ;
- la commande copie `Mtx` et ne conserve pas un pointeur vers la matrice d'entrée ;
- l'instance, les métadonnées et les buffers de ressource restent byte-identiques avant
  et après adaptation/soumission ;
- `submit_static_command()` ignore le runtime id pour le choix géométrique ;
- une ressource absente, vide ou des bornes incohérentes est refusée avant draw.

La table du smoke est :

| Resource ID | Vertex count | Index count | Triangles | Bornes |
|---:|---:|---:|---:|---|
| 0 | 24 | 36 | 12 | min(-0,5, -0,5, -0,5), max(0,5, 0,5, 0,5) |
| 1 | 4 | 6 | 2 | min(-0,75, -0,25, -0,75), max(0,75, -0,25, 0,75) |

Il n'existe pas de branche de géométrie par identifiant à l'intérieur du backend. Seul
le caller choisit la ressource explicite.

## Cas invalides et overflow

Les tests hôte rejettent :

- vertex/index pointer null ;
- vertex/index count nul ;
- `index_count` non multiple de trois ;
- index `>= vertex_count` ;
- index > `0xffff` ;
- vertex count > 65 536 ;
- bounds contenant NaN ;
- axes de bounds inversés ;
- resource ID absent de la table ;
- métrique `triangles + index_count/3` qui dépasserait `UINT32_MAX`.

L'index type est vérifié à 16 bits et la primitive de soumission reste
`Triangles`.

## Ownership de l'état GU

Le smoke donne au bridge un état PSP explicite :

```cpp
lighting = false
texture = false
blend = true
alpha_test = true
depth_write = true
```

Le backend appelle `apply_static_render_state()` avant chaque draw. Le test hôte vérifie
le passage byte-identique de cet état. Aucune hypothèse de matériau Dusklight n'est
encodée dans le bridge.

## Caméra et matrices

La caméra du smoke est explicitement PSP-owned :

```text
eye    = (0, 1.6, -5.2)
center = (0, 0.35, 0)
up     = (0, 1, 0)
fov    = 62°
aspect = 480/272
near   = 0.2
far    = 100
```

Le backend charge projection/view/model via GUM puis soumet la ressource. Ce contrat ne
prétend pas reproduire la caméra historique de Dusklight.

## Budgets observés

Les métriques sont cumulatives depuis l'initialisation du renderer et enregistrées dans
le marker `DMDL_OBJECT.OK` :

```text
DUSKLIGHT_PSP_DMDL_OBJECT_RENDER_OK frames=180 draws=360 triangles=2520 vertices=3060 indices=4536 bytes=408456
```

Les maxima logiques attendus sont :

```text
frames=180
draws=360
triangles=2520
vertices=3060
indices=4536
bytes=408456
```

Le test hôte rejette aussi les additions de compteurs qui déborderaient.

## Build et validation PSP

Le smoke utilise :

- `test/dmdl-object-psp/main.cpp` ;
- `dusklight-main/platforms/psp/render/dmdl_object_bridge.cpp` ;
- `dusklight-main/platforms/psp/render/dmdl_object_psp_backend.cpp` ;
- le renderer existant `static_mesh_3d.cpp` ;
- `::dMdl_obj_c` historique depuis `d/d_model_obj.h`.

`scripts/build-dmdl-object-smoke.sh` :

- compile le probe header avec `psp-g++` ;
- configure/compile le smoke via `psp-cmake` ;
- inspecte l'ELF/PBP avec `psp-objdump`/`strings` ;
- interdit Aurora dans les dépendances PSPSDK ;
- vérifie les labels erreurs/succès dans le binaire.

Le marker runtime est écrit uniquement si les compteurs finaux correspondent aux
valeurs attendues.

## PPSSPP via runner GUI persistant

Le broker GUI persistant s'est arrêté pendant la requête automatisée initiale. Une
nouvelle requête au même chemin serait donc non bornée sans intervention GUI. Aucun
fallback manuel n'a été utilisé.

La validation PSP runtime repose sur le profil isolé et les mêmes assets libres synthétiques.

## Non-régression

La suite `scripts/test-psp-smokes.sh --timeout 30` termine avec succès dans le journal :

`logs/ppsspp/automated-smokes-20260724T135822Z.log`

Elle couvre :

- host core ;
- smoke PSP initial ;
- core PSP ;
- GU PSP ;
- asset PSP ;
- bridge PSP ;
- dMdl object seam host/PSPSDK ;
- dMdl object render host/PSP ;
- DMDL runtime PSP ;
- smoke historique direct ;
- startup legacy ;
- startup v2 Functional/Performance/PSP conservative ;
- six hôtes startup/UI/title/F_SP108/FrameProfiler ;
- original actor suite ;
- playable legacy direct.

La suite est verte dans l'environnement disponible.

## Fichiers de la phase

- `dusklight-main/include/d/d_model_obj.h` ;
- `dusklight-main/include/d/d_model.h` ;
- `dusklight-main/platforms/psp/render/dmdl_object_bridge.hpp` ;
- `dusklight-main/platforms/psp/render/dmdl_object_bridge.cpp` ;
- `dusklight-main/platforms/psp/render/dmdl_object_psp_backend.hpp` ;
- `dusklight-main/platforms/psp/render/dmdl_object_psp_backend.cpp` ;
- `dusklight-main/platforms/psp/render/static_mesh_3d.hpp` ;
- `test/model-obj-seam/model_obj_probe.cpp` ;
- `test/dmdl-object-host/dmdl_object_host_tests.cpp` ;
- `test/dmdl-object-psp/{CMakeLists.txt,main.cpp}` ;
- `scripts/test-dmdl-object-seam.sh` ;
- `scripts/build-dmdl-object-smoke.sh` ;
- `scripts/inspect-dmdl-object-smoke.sh` ;
- `scripts/run-dmdl-object-smoke.sh` ;
- `scripts/test-dmdl-object-smoke.sh` ;
- `docs/decisions/0006-lightweight-dmdl-object-seam.md` ;
- ce rapport.

## Prochaine étape

Le bridge d'instance statique peut désormais recevoir une vraie ressource PSP convertie
hors ligne et appliquer la matrice d'un `::dMdl_obj_c` sans que le caller manipule les
vertices. La prochaine phase doit définir puis éprouver cette sélection explicite de
ressource, avec tests hôte/PSP sur au moins deux modèles, avant toute restauration de J3D
complet.
