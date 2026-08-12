# Inventaire des sources UI startup, titre, file select et HUD

## Résultat

`UI_SOURCE_AUDIT=DONE`

L'audit porte uniquement sur le snapshot local, les convertisseurs déjà présents
et les paquets PSP déjà produits. Il n'a ni modifié le renderer canonique, ni
reconverti les ressources, ni accédé au réseau.

Les quatre paquets UI actuels sont valides et déterministes, mais ils ne sont pas
des sérialisations de panes J2D. Ils conservent des images, des glyphes, des
dimensions, des canaux et des positions PSP bornées. Ils ne conservent pas la
hiérarchie, les transforms, les matériaux, les animations ni les identités de
panes des layouts source. Cette distinction est la frontière honnête pour les
futures phases V8/V9.

## Autorités et fichiers inspectés

| Surface | Autorité source dans le snapshot | Conversion actuelle | Paquet PSP courant |
|---|---|---|---|
| avertissement, Nintendo, Dolby | `d_s_logo.cpp`, `LogoPalFr.arc`, `LogoPal.arc` | `export_original_startup_ui_packages` | `data/startup/startup_logos.dpsu` |
| prompt titre | `d_a_title.cpp`, `Title2D.arc:zelda_press_start.blo`, message 100 | `export_original_startup_ui_packages` | `data/startup/title_ui.dpsu` |
| file select | `d_file_select.cpp`, `fileSel.arc` | `export_original_file_select_ui_package` | `data/startup/file_select.dpsu` |
| HUD et pause | `d_meter2_draw.cpp`, `main2D.arc`, `clctres.arc`, `fontres.arc` | `serialize_original_dpui` | `data/common/hud.dpui` |

Les chemins source bruts ci-dessus décrivent les lectures effectuées par le
convertisseur depuis l'image locale autorisée. Aucun ARC, BLO, BTI ou BFN brut
n'est suivi dans Git.

## Inventaire des paquets réellement livrés

| Paquet | Format | Taille | Atlas | Records | Sources représentées | SHA-256 |
|---|---:|---:|---:|---:|---|---|
| `startup_logos.dpsu` | DPSU v1 | 524 512 | 512x512, 4444 | 3 | 4 BTI, dont le prompt d'avertissement fusionné dans le warning | `36e2a62ff7dafea9c564f14c6911e40ea71f5b069c451fd9aee36db9c9b3f152` |
| `title_ui.dpsu` | DPSU v1 | 33 440 | 256x64, 4444 | 17 | 1 BFN, 17 occurrences de glyphes | `51cbd70ecba065ad1bf5fa53683540299be382b7b741aa919cc5885448a0ee82` |
| `file_select.dpsu` | DPSU v1 | 524 672 | 512x512, 4444 | 8 | 4 BTI | `7f9aae08cbbe7883725f78088342676eb7c06ffb4c6484fd8f4e33ae125b2abd` |
| `hud.dpui` | DPUI v2 | 132 192 | 512x128, 4444 | 31 | 20 BTI et 1 BFN | `4e3fd2da93150ecdee0cf10c50067486764c6faf580a8d0a412462ac7137a795` |

Les en-têtes et tables donnent les contenus suivants :

- logos : warning 368x272 à `(56,0)`, Nintendo 376x104 à `(52,84)`, Dolby 232x112 à `(124,80)` ;
- titre : 17 glyphes 24x24, centrés par leurs avances à `y=220` ;
- file select : fond 480x272, trois exemplaires du cadre, trois exemplaires du curseur et un bouton A ;
- HUD : quatre cœurs, dix chiffres, rubis, bouton A, quatre pièces de pause et onze glyphes uniques nécessaires à `Resume`, `Reset Room` et `Exit`.

## Panes et layouts source

### Boot

Les trois écrans boot ne reposent pas sur un `J2DScreen` dans le chemin source inspecté. `dScnLogo_c` construit directement des `dDlst_2D_c` : Nintendo est soumis en 376x104 depuis `(117,154)`, Dolby en 232x112 depuis `(189,150)`, et l'avertissement PAL occupe le framebuffer avec une image d'invite superposée. Le package PSP représente donc honnêtement des sprites, pas des panes. Ses positions 480x272 sont cependant des adaptations du convertisseur et non des transforms source sérialisées.

### Titre

`daTitle_c` charge exactement `zelda_press_start.blo`. Le code source référence le groupe `n_all` et sept text boxes : `t_s_00` à `t_s_05`, puis `t_o`. Chaque text box reçoit la police message et le message 100. Le groupe `n_all` porte translation, scale et animation alpha.

Le DPSU courant ne contient aucune de ces huit identités, aucune relation parent/enfant et aucun état alpha. Il contient les 17 glyphes de la chaîne française, chacun placé directement. Il s'agit d'une preuve d'asset et de lisibilité, pas encore d'une preuve de pane.

### File select

Le chemin réel charge cinq layouts : `zelda_file_select.blo`, `zelda_file_select_copy_select.blo`, `zelda_file_select_yes_no_window.blo`, `zelda_file_select_3menu_window.blo`, `zelda_file_select_details.blo`.

Le fichier d'implémentation contient au moins 125 tags de panes littéraux distincts. Ce nombre est une borne inférieure : des tags supplémentaires sont stockés dans des tables ou créés dynamiquement. Le chemin charge aussi 19 ressources d'animation nommées : quatre BCK, quatre BPK, deux BRK et neuf BTK. Les responsabilités observées couvrent les trois fichiers, les cadres, le curseur, les données de sauvegarde, les menus start/copy/delete, le yes/no, les messages d'erreur, les détails d'équipement et la saisie de nom.

Le DPSU courant ne contient que quatre textures : `tt_3setu_w_l.bti`, `tt_gold_uzu_long2.bti`, `tt_spot_square3.bti`, `tt_zelda_button_a_8ia.bti`.

Le fond est redimensionné à 480x272 et les trois cadres/curseurs utilisent des positions calculées dans le convertisseur. Aucun BLO, BCK/BPK/BRK/BTK, pane, font, message, détail de sauvegarde ou écran secondaire n'est sérialisé. La classification correcte reste donc `partial_source_textures`.

### HUD et pause

`dMeter2Draw_c` charge trois layouts : `zelda_game_image.blo`, `zelda_game_image_kantera.blo` et `zelda_icon_pikari.blo`. Son implémentation référence au moins 251 tags de panes littéraux distincts, notamment les vingt positions de cœurs, rubis/clés, boutons et textes d'action, magie/lanterne, gouttes de lumière et effets Pikari.

Le DPUI v2 courant ne porte qu'un sous-ensemble volontaire : vie, rubis, chiffres, invite A, panneau/cadre/curseur de pause et glyphes des trois choix. Il conserve la taille racine 604x448 du BLO, mais ses 31 positions par défaut sont toutes nulles. Les positions visibles sont actuellement choisies dans `playable_render.cpp`. La taille source du layout n'est donc pas une hiérarchie de panes ni une preuve de leurs transforms.

## Sprites, textures et palettes

Le HUD inventorié contient réellement : IA4 : quatre cœurs, dix chiffres et bouton A ; C8 + TLUT RGB5A3 : icône rubis, 208 couleurs ; I4 : curseur et trois pièces de panneau/cadre pause ; BFN1/GLY1 I4 : police `rodan_b_24_22.bfn`.

Les palettes sont résolues hors ligne avant la conversion en atlas RGBA4444. Le runtime PSP ne reçoit donc plus les identités TLUT ni le format BTI source ; il reçoit des pixels déjà développés et un hash FNV-1a de ressource. Cette chaîne convient à la fidélité des pixels du sous-ensemble, mais elle ne suffit pas à reconstituer un matériau J2D ou ses animations.

## Fonts et messages

| Surface | Dépendance source réelle | État du paquet courant | Manque pour une chaîne générique |
|---|---|---|---|
| titre | font message, message ID 100, sept text boxes | Rodan BFN décodé, texte français sérialisé en 17 glyphes | table message/locale, ID 100, panes et répétition des sept boxes |
| file select | font message + subfont, `dMsgString_c`, messages et contrôles | aucune font et aucun message | les deux fonts, IDs, mise en ligne, écrans secondaires et états |
| HUD | font message pour textes A/B/XY/croix et actions dynamiques | onze glyphes de trois libellés pause | IDs et chaînes HUD dynamiques, métriques, panes de texte |

Pour le titre, le code source prouve l'identité `message 100`, mais le convertisseur courant emploie la chaîne littérale `Appuyez sur START` pour choisir ses glyphes. Il ne lit pas encore une table de messages générique.

Pour le file select, les IDs directement observables incluent au minimum `0x54`, `0x55`, `0x07`, `0x08`, `0x56`, `0x57`, `0x58`, les titres `0x40` à `0x4c`, `0x52`, `0x384`, `0x385`, ainsi que plusieurs messages d'erreur et de carte. Certains IDs sont fournis à des fonctions communes au runtime ; l'inventaire complet doit donc instrumenter ou analyser les tables de messages plutôt que figer cette liste partielle.

## Contrat disponible et données absentes

DPSU v1 et DPUI v2 fournissent : ID local, canal, rectangle écran, rectangle UV, couleur, hash de source et avance. L'ordre des records peut servir à une soumission déterministe.

Ils ne fournissent pas : un `ParityPaneId` dérivé de l'identité J2D ; parent, enfant, groupe ou ordre hiérarchique ; origine/pivot, scale, rotation ou transform global ; alpha hérité, visibilité héritée ou animation de pane ; matériau J2D, sampler, palette source ou animation matériau ; identité de text box, ID de message, locale, ligne ou alignement ; screen bounds et landmarks issus de la référence desktop.

En conséquence, les affirmations historiques « pane anchor » pour ces paquets doivent être lues comme des contrats de placement PSP du sous-ensemble, pas comme une sérialisation exacte des anchors du BLO.

## Découpage recommandé A/B/C/D

### A — acquisition/inventaire
Cette passe ferme l'inventaire statique des chemins source et des paquets courants. La prochaine acquisition doit extraire, sans rendu, les cinq BLO file select, le BLO titre et les trois BLO HUD avec une identité stable par archive, layout, tag et chemin hiérarchique.

### B — tests et conversion
Ajouter un inventaire BLO2 générique qui vérifie au minimum : hiérarchie, type de pane, transform local, bounds, alpha/visibilité, matériau, texture, font/text box et ordre. Les tests doivent refuser les tags dupliqués ambigus, les offsets hors fichier, les cycles et toute identité calculée depuis l'ordre de dessin. La conversion des messages doit préserver `(locale, message_id)` et les métriques de font ; elle ne doit pas créer de format propre à un écran.

### C — intégration runtime
Reste dépendante de la preuve pane V8. Le renderer ne doit pas être modifié à partir des seuls rectangles DPSU/DPUI actuels. L'intégration pourra consommer des panes convertis seulement après stabilisation des identités et tests hôte.

### D — preuve
Produire des événements `ui_pane_transform` desktop/PSP, puis captures, landmarks et overlays pour warning, titre, curseur file select, HUD et pause. Les chaînes et glyphes devront être reliés à leur source, et non seulement à un framebuffer final.

## Validation exécutée

`scripts/test-startup-ui-transform-parity.sh` réussit intégralement :

```text
STARTUP_UI_HOST_OK format=DPSU1 logos=warning,nintendo,dolby title_text=Appuyez_sur_START file_select=source_textures
DPUI_V2_HOST_OK records=31 original_assets=20 atlas_bytes=131072 visible_pixels=19411 unique_content=31
STARTUP_UI_TRANSFORM_PARITY_HOST_OK surfaces=14 partial=6 functional_capture=PENDING_GUI_EXECUTION
```

Cette réussite valide formats, bornes, CRC, diversité du contenu et contrats structurels existants. Elle ne promeut pas les paquets en parité de panes. Aucun test PPSSPP n'était nécessaire pour cet audit indépendant.
