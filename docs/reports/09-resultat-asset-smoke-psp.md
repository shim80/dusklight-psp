# Résultat du smoke asset PSP

Date : 2026-07-16

## Statut

`READY_FOR_DUSKLIGHT_FORMAT_CONVERTER`

Un pipeline asset indépendant du GPU desktop existe maintenant et est validé sur
hôte et PSP. Les packages générés sous `.test-data/assets/asset-smoke/` sont :

- `synthetic.dprm` : modèle statique, 24 sommets, 36 indices, 12 triangles;
- `synthetic.dptx` : deux textures 64×64 swizzlées, opaque 5650 et alpha 4444;
- `synthetic.dpcl` : collision, 4 triangles;
- `synthetic.dpck` : conteneur aligné regroupant les trois packages.

Les formats utilisent :

- magic + version + taille totale + CRC32;
- sections nommées avec offset, taille, count et stride;
- alignement 16 octets;
- offsets relatifs, jamais pointeurs hôte;
- vertex layout PSP invariant : `u`, `v`, `ABGR`, `x`, `y`, `z`;
- collision séparée du modèle de rendu.

## Tests hôte

`tools/asset-converter/convert_asset.py` convertit un asset JSON synthétique en
packages PSP. `inspect_asset.py` et `test_asset_pipeline.py` valident :

- headers et CRC;
- bounds;
- types de section;
- offsets/alignement;
- indices;
- tailles textures;
- corruption CRC;
- erreur de magic;
- index hors limites;
- box collision invalide;
- conteneur tronqué;
- entry offset mal aligné;
- transforms limites via round-trip float32.

Le résultat hôte est :

```text
DUSKLIGHT_PSP_ASSET_HOST_TESTS_OK
```

## Loader PSP

`test/asset-psp/` lit `synthetic.dpck` depuis le Memory Stick, valide le
conteneur et chaque entrée, vérifie le payload du modèle, les textures et les
triangles de collision, puis écrit :

```text
DUSKLIGHT_PSP_ASSET_OK
```

dans `ASSET.OK`.

Le dernier run PPSSPP LaunchServices a :

- démarré PPSSPP;
- produit le marker exact;
- quitté avec `request_complete=1`;
- laissé `runner_config` vide parce que LaunchServices macOS ne propage pas le
  HOME/XDG du sous-processus.

Le script classe maintenant ce transport comme succès PSP avec dérive de profil
explicite, au lieu de prétendre un faux profil isolé.

## Provenance

Les entrées test sont synthétiques et générées dans le dépôt. Aucun asset du jeu
n'a été lu, inspecté ou converti.

## Suite

Le prochain jalon doit analyser les formats Dusklight source sans modifier le
renderer PSP :

1. définir les entrées autorisées du convertisseur;
2. documenter les transforms Dusklight → PSP;
3. convertir un modèle/texture/collision simple;
4. valider le même package sur hôte et PSP;
5. seulement ensuite l'intégrer au rendu.
