# Rapport 192 — inventaire hôte des layouts UI BLO2

## Résultat

```text
UI_LAYOUT_INVENTORY_HOST=DONE
UI_LAYOUT_RUNTIME_INTEGRATION=NOT_STARTED
CANONICAL_RENDERER_MODIFIED=NO
DESKTOP_ORACLE_MODIFIED=NO
NETWORK_ACCESS=NO
```

La phase UI-B demandée après l'audit 188 est fermée côté acquisition, format et tests hôte. La phase d'intégration runtime reste volontairement non ouverte.

## Livrables

- `tools/dusk_ui_layout_inventory/dusk_ui_layout_inventory.py` : parseur et validateur BLO2/MAT1 bornés ;
- `test/ui-layout-inventory/test_ui_layout_inventory.py` : fixtures synthétiques et cas négatifs ;
- `scripts/test-ui-layout-inventory.sh` : acquisition locale des neuf layouts, validation et agrégation ;
- `docs/design/psp-ui-layout-inventory-v1.md` : contrat du format et frontière runtime.

## Corpus réel validé

| Surface | Archive | Layout exact | Panes | Matériaux | Textures | Fonts | Text boxes |
|---|---|---|---:|---:|---:|---:|---:|
| Title | `/res/Layout/Title2D.arc` | `zelda_press_start.blo` | 10 | 7 | 0 | 1 | 7 |
| File select | `/res/Object/fileSel.arc` | `zelda_file_select.blo` | 291 | 251 | 30 | 2 | 28 |
| File copy | `/res/Object/fileSel.arc` | `zelda_file_select_copy_select.blo` | 93 | 78 | 15 | 0 | 0 |
| File yes/no | `/res/Object/fileSel.arc` | `zelda_file_select_yes_no_window.blo` | 27 | 20 | 5 | 1 | 4 |
| File menu | `/res/Object/fileSel.arc` | `zelda_file_select_3menu_window.blo` | 39 | 30 | 5 | 1 | 6 |
| File details | `/res/Object/fileSel.arc` | `zelda_file_select_details.blo` | 47 | 32 | 14 | 0 | 0 |
| HUD principal | `/res/Layout/main2D.arc` | `zelda_game_image.blo` | 346 | 216 | 25 | 1 | 35 |
| HUD lanterne | `/res/Layout/main2D.arc` | `zelda_game_image_kantera.blo` | 17 | 10 | 3 | 0 | 0 |
| HUD pikari | `/res/Layout/main2D.arc` | `zelda_icon_pikari.blo` | 5 | 3 | 1 | 0 | 0 |
| **Total** |  |  | **875** | **647** | **98** | **6** | **80** |

Répartition des panes : 228 `PAN2`, 567 `PIC2`, 80 `TBX2` et aucun `WIN2` dans ce corpus borné. Les 875 identités `(archive, layout, tag_hex, path)` sont uniques.

## Preuve hôte

Commande exécutée localement avec l'image déjà fournie au projet :

```text
DUSKLIGHT_GAME_IMAGE=<image-locale> ./scripts/test-ui-layout-inventory.sh
```

Résultat :

```text
Ran 7 tests in 0.003s
OK
UI_LAYOUT_INVENTORY_HOST_OK layouts=9 panes=875 materials=647 textures=98 fonts=6 text_boxes=80 negative_tests=5
```

Chaque JSON a ensuite passé `UI_BLO2_INVENTORY_VALIDATE_OK`. Le résumé agrégé ignoré est produit dans `build/host/ui-layout-inventory/UI_LAYOUT_INVENTORY_SUMMARY.json`.

## Couverture des tests

Les tests positifs vérifient hiérarchie parent/enfants, extraction matériaux/textures/fonts/texte, visibilité/alpha statiques séparés, transforms/bounds et identité stable. Les cinq rejets ciblés couvrent offset MAT1 hors bloc, taille tronquée/hors fichier, tags frères ambigus, cycle parent/enfant et altération des identités.

## Observations importantes

- Le title réel est un arbre de 10 panes et sept text boxes utilisant `rodan_24_22.bfn`; ce n'est pas un sprite unique générique.
- File select et HUD exposent des hiérarchies riches qui exigent une conversion pilotée par identité source.
- Les valeurs produites sont statiques ; une animation J2D pourra modifier transform, couleur, visibilité, texture ou matériau et devra être acquise séparément.
- Les outputs extraits restent ignorés. Aucun octet BLO2, BTI, BFN ou autre asset propriétaire n'est ajouté aux fichiers suivis.

## Conclusion

```text
UI_SOURCE_AUDIT=DONE
UI_LAYOUT_INVENTORY_FORMAT=DONE
UI_LAYOUT_INVENTORY_HOST_TESTS=DONE
UI_RUNTIME_READY_FOR_MAPPING_AUDIT=YES
UI_RUNTIME_INTEGRATION_AUTHORIZED_BY_THIS_REPORT=NO
```

Le prochain travail UI peut s'appuyer sur un mapping stable des sources title, file select et HUD, en conservant la séparation acquisition/conversion, intégration runtime et preuve visuelle.
