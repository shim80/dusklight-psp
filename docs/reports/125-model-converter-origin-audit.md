# Rapport 125 — Audit de l’origine modèle du convertisseur PSP

## Résultat

L’hypothèse « recentrer tout modèle source autour de l’origine » était bien
présente dans le convertisseur DMDL et pouvait déplacer des objets rigides dont
le pivot du BMD fait partie de la transformation gameplay.

Le format DPSM passe en v2 et transporte désormais `source_origin[3]`. Le
convertisseur accepte explicitement `--origin-policy preserve|center` et exige
une politique au lieu d’appliquer un recentrage implicite.

## Contrat

- `preserve` : positions locales source inchangées, `source_origin=(0,0,0)` ;
- `center` : positions décalées de `-center`, avec `source_origin=center` ;
- reconstruction source : `stored_position + source_origin` ;
- les loaders PSP v2 valident les deux politiques et refusent les valeurs non
  finies.

Les assets startup dont la position écran est intentionnellement centrée
conservent explicitement `--origin-policy center`. Les modèles monde/acteurs
restent `preserve` tant qu’une preuve source contraire n’existe pas.

## Tests

`scripts/test-model-origin-parity.sh` construit des fixtures off-origin et
vérifie :

- round-trip exact des positions en mode `preserve` ;
- reconstruction exacte en mode `center` ;
- refus d’une politique absente ;
- parsing PSP v2 des deux formes ;
- absence de `center_vertices` dans la conversion room canonique.

Le test hôte DPSM est étendu au même contrat.

## Portée

Cette correction ferme le transform de conversion générique. Elle ne prouve
pas encore que chaque classe d’acteur utilise le bon pivot : chaque famille doit
être auditée face au BMD source et à la matrice collision avant de modifier sa
transformation.
