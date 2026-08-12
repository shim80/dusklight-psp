# Résultat — probe de lecture du modèle/animation Link avec le chemin source

## Classification

`READY_FOR_STATIC_LINK_POSE_CONVERTER`

Cette classification concerne la lecture et l'inventaire hôte des ressources Link
avec Nod, Aurora FST et les parseurs J3D du snapshot. Elle ne constitue pas une preuve
de rendu, de skinning, de boucle d'animation ni de gameplay PSP.

## Provenance et versions

| Élément | Valeur |
|---|---|
| Baseline Git | `90263648398960031620f652610e5a2d3fb02ae9` (`Document PSP instance bridge result`) |
| Tag Git local | `psp-dmdl-object-bridge-v1` |
| Dusklight | `1bae8a5e6a812217ca33ba533e707ecfa64b1553` |
| Aurora | `81f12b57ea2566a1c4f85909b74932db21eaf6d0` |
| Nod | `96cd6f275c960986a4ce93c82fe49981701de01a` |
| Source de verrou | `toolchain/loader-sources.lock` |
| Autorisation image | utilisateur, confirmée dans la conversation active |
| Réseau | aucun accès pendant les tests |

Les deux dépendances hôte sont des copies de travail ignorées sous `.cache/deps/source`.
Aucun fichier Aurora/Nod n'est suivi. Le snapshot source historique de Dusklight reste
inchangé et conserve des références cohérentes avec ces deux révisions.

## Blocages de provenance rencontrés

### Nod

Le premier `extern/nod` disponible localement n'était pas la révision demandée :

```text
expected=96cd6f275c960986a4ce93c82fe49981701de01a
actual=e2919c7e79ec8ef1756ad51e043b8d4718d73b1a
```

Cette copie a été rejetée avant compilation. Une archive locale
`nod-96cd6f275c960986a4ce93c82fe49981701de01a.tar.gz`, fournie par l'utilisateur,
a ensuite été installée et vérifiée à la bonne révision. La révision fournie est assez
ancienne pour ne pas contenir la `FileIO::FSeek`/`FTell` exposée dans des Aurora plus
récentes ; le probe utilise donc un FFI C minimal de fermeture dans le fichier test
uniquement, sans modifier Nod.

### Aurora

Le premier snapshot fourni correspondait à une révision récente d'un sous-dossier
partiel et ne satisfaisait pas le contrat :

```text
expected=81f12b57ea2566a1c4f85909b74932db21eaf6d0
actual=786de8cbdbd2cfcb69b3863a2d6901e1a0ceba1f
```

Il a été rejeté. Une archive complète correspondant à `81f12...` a ensuite été fournie.
Le script de vérification recalcule l'identité Git de la révision racine, tandis que
`CMakeLists.txt` recalcule l'identité du sous-arbre Aurora consommé et la compare à la
valeur verrouillée.

## Build hôte

- CMake configure Nod avec les fetches host forcés sur les snapshots locaux déjà
  installés ;
- une copie de travail Aurora est construite sous `.cache/deps/source/aurora` ;
- seuls les TU Aurora nécessaires aux frontières DVD/FST/RARC/J3D sont compilés ;
- `JPEG_Turbo`, `libnod`, `bzip2`, `fmt` et `aurora` proviennent des bundles hôte déjà
  installés ;
- les traductions de types J3D/GX proviennent du snapshot `dusklight-main` ;
- les shims de compatibilité restent dans `tools/dusk_link_loader_probe/compat` ;
- le build reste hors source sous `build/link-loader-probe`.

Un warning Xcode indique que la SDL précompilée cible un SDK macOS plus récent (15.2)
que le SDK 14.4 utilisé par CMake. L'exécutable est néanmoins lié,
exécuté et testé avec succès.

## Chaîne réellement exercée

```text
image brute en lecture seule
  -> Nod FFI épinglé
  -> backend DVD et FST Aurora épinglé
  -> DVDOpen / DVDReadPrio
  -> JKRDecomp (Yaz0)
  -> JKRMemArchive (RARC)
  -> J3DModelLoaderDataBase::load (BMD)
  -> J3DAnmLoaderDataBase::load (BCK)
  -> inventaire des objets J3D et display lists, sans rendu
```

Le probe ne crée aucune fenêtre. Il ne charge aucune texture dans un backend
graphique. Les appels de soumission GX restent des gardes fatales. Les seules
fonctions texture autorisées initialisent les champs de `GXTexObj`/`GXTlutObj`
comme dans Aurora `81f12...`; elles n'émettent aucune commande.

## Données locales lues

| Élément | Observation |
|---|---|
| Image | GameCube brute, 1 459 978 240 octets |
| Disc ID / révision | `GZ2P01` / `00` |
| `/res/Object/Kmdl.arc` | 245 914 octets stockés, Yaz0 puis RARC |
| `/res/Object/AlAnm.arc` | 2 833 856 octets, RARC |
| `waits.bck` | ID `0x026A`, 1 818 octets stockés Yaz0, 3 552 octets décodés |

Aucun octet de ces éléments n'est écrit dans un rapport ou un produit suivi.

## Inventaire BMD mesuré

Tous les modèles ont la magie/version `J3D2bmd3` et les sections
`INF1,VTX1,EVP1,DRW1,JNT1,SHP1,MAT3,TEX1`.

| Ressource | Octets | Joints | Shapes | Mat. | Tex. | Pos. | Norm. | UV | Packets/draws |
|---|---:|---:|---:|---:|---:|---:|---:|---:|---:|
| `al.bmd` | 140 448 | 35 | 18 | 18 | 12 | 1 558 | 1 733 | 1 176 | 39 |
| `al_head.bmd` | 34 400 | 10 | 2 | 2 | 2 | 287 | 325 | 304 | 8 |
| `al_hands.bmd` | 61 056 | 3 | 11 | 11 | 1 | 1 388 | 1 114 | 224 | 11 |
| `al_face.bmd` | 72 576 | 5 | 5 | 5 | 14 | 294 | 261 | 208 | 12 |

Chaque ressource contient trois vertex arrays utilisés : position, normale et
coordonnée UV0. Aucun array couleur n'est présent.

| Ressource | Primitives | Strips | Triangles | Dégénérés | Index max. | Sommets runtime estimés |
|---|---:|---:|---:|---:|---:|---:|
| `al.bmd` | 796 | 796 | 2 777 | 0 | 1 731 | 2 291 |
| `al_head.bmd` | 158 | 158 | 540 | 0 | 322 | 451 |
| `al_hands.bmd` | 798 | 798 | 2 732 | 0 | 1 387 | 2 378 |
| `al_face.bmd` | 141 | 141 | 514 | 0 | 293 | 349 |

Il n'y a ni commande `GX_TRIANGLES` directe ni fan : toute la géométrie mesurée
est encodée en strips. Les triangles logiques sont obtenus par expansion
déterministe ; un triangle est déclaré dégénéré lorsque deux indices de position
sont identiques. L'estimation runtime déduplique le tuple complet des indices
d'attribut par coin.

### Bounds locaux

| Ressource | Minimum | Maximum |
|---|---|---|
| `al.bmd` | `(-93.3886, 0, -15.8344)` | `(93.3886, 163.245, 21.6352)` |
| `al_head.bmd` | `(-67.1081, -27.7472, -13.8674)` | `(17.6486, 7.00164, 14.2209)` |
| `al_hands.bmd` | `(-3.37376, -6.46584, -8.16193)` | `(20.1837, 8.53991, 8.16193)` |
| `al_face.bmd` | `(3.15892, -16.4006, -9.1319)` | `(15.8222, 4.32819, 9.15943)` |

Une bound combinée n'est pas publiée : les bounds sont dans les espaces locaux
de pièces attachées et l'assemblage n'a pas encore été posé.

### Enveloppes et matrices

| Ressource | Enveloppes | Influences | Max/env. | Poids min/max | Somme min/max | Rigides | Enveloppées |
|---|---:|---:|---:|---|---|---:|---:|
| `al.bmd` | 110 | 246 | 5 | 0,05 / 0,95 | 1 / 1 | 24 | 110 |
| `al_head.bmd` | 36 | 74 | 3 | 0,10 / 0,90 | 1 / 1 | 8 | 36 |
| `al_hands.bmd` | 0 | 0 | 0 | 0 / 0 | 0 / 0 | 2 | 0 |
| `al_face.bmd` | 35 | 79 | 3 | 0,02 / 0,90 | 1 / 1 | 4 | 35 |

`EVP1` et `DRW1` sont présents dans les quatre fichiers.

### Formats et quantification

Les attributs utilisés sont :

- position `GX_VA_POS` : `GX_POS_XYZ`, `GX_F32`, fraction 0 ;
- normale `GX_VA_NRM` : `GX_NRM_XYZ`, `GX_S16`, fractions respectives
  14, 15, 15 et 14 ;
- UV0 `GX_VA_TEX0` : `GX_TEX_ST`, `GX_S16`, fractions respectives
  11, 14, 12 et 13.

### Assemblage

- somme brute des positions : 3 527 ;
- somme brute des triangles : 6 563 ;
- sommets runtime estimés après séparation des attributs par coin : 5 469 ;
- triangles runtime hors dégénérés : 6 563 ;
- chunks 16 bits prévisibles : 4, un par ressource ;
- draws sans fusion de matériaux : 70.

Le code réel `daAlink_c::changeLink` sélectionne ensemble `al.bmd`,
`al_head.bmd`, `al_hands.bmd` et `al_face.bmd`. Le calcul de modèle attache tête
et visage à la matrice 4 du corps, et les mains aux matrices 9 et 14. Les trois
pièces sont donc confirmées comme attachées au squelette principal. Les nombres
de joints locaux ne sont pas additionnés comme un squelette unique.

## Animation mesurée

| Champ | Résultat |
|---|---|
| Nom / ID | `waits.bck` / `0x026A` |
| Magie / type | `J3D1bck1` / `ANK1`, `J3DAnmTransformKey` |
| Durée | 45 frames |
| Cadence | non encodée dans le BCK ; dépend de l'horloge d'animation du jeu |
| Boucle | attribut J3D `2` |
| Pistes / joints affectés | 35 / 35 |
| Squelette principal | 35 joints |
| Frame 0 | construite pour les 35 pistes, déterministe et finie |
| NaN / infini | aucun |
| Joints sans piste | 0 |
| Fallback | bind pose pour une piste absente, non utilisé ici |

La compatibilité structurelle avec `al.bmd` est donc confirmée. Aucun skinning
n'est appliqué dans cette phase.

## Tests

- bootstrap/vérification des sources : succès hors ligne après récupération ;
- construction Nod et probe : succès ;
- lecture positive : `LINK_J3D_LOAD_OK` ;
- 13 tests négatifs : succès (variable absente, chemin absent, non-GameCube,
  mauvais ID, mauvaise révision, archives absentes, BMD/BCK absents, fixtures
  tronquée et hors limites, mauvaises révisions Aurora et Nod) ;
- scripts shell : syntaxe valide ;
- smoke tests PSP avant phase : succès ;
- smoke tests PSP après phase : succès global, journal
  `logs/ppsspp/automated-smokes-20260726T081040Z.log`.

Journaux ignorés :

- `logs/link-loader/probe-success.log` ;
- `logs/link-loader/negative-tests.log`.

## Fichiers de la phase

- `toolchain/loader-sources.lock` ;
- `scripts/bootstrap-link-loader-sources.sh` ;
- `scripts/verify-link-loader-sources.sh` ;
- `scripts/build-link-loader-probe.sh` ;
- `scripts/run-link-loader-probe.sh` ;
- `scripts/test-link-loader-probe.sh` ;
- `tools/dusk_link_loader_probe/**` ;
- `docs/decisions/0007-link-loader-source-provenance.md` ;
- ce rapport.

## Prochaine étape

Reprendre uniquement la phase C — Pose statique de la démo Link. La conversion
devra consommer les mesures ci-dessus, conserver les pièces séparées, appliquer
la pose frame 0 prouvée et définir explicitement les attachements. Aurora et Nod
restent des dépendances hôte et ne doivent pas entrer dans l'EBOOT PSP.
