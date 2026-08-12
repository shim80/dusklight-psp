# Plan de validation PPSSPP

## But

Valider un homebrew PSP libre sans toucher aux données du jeu et sans dépendre du code de
sortie de l'interface PPSSPP. Le homebrew doit fournir sa propre preuve de réussite.

## Couches de validation

1. **Layout hôte** : manifeste, chemins, scripts et génération d'assets libres.
2. **Inspection binaire** : ELF Allegrex/MIPS, sections attendues, symboles et PBP.
3. **Exécution PPSSPP isolée** : profil dédié sous `.test-data/ppsspp/`.
4. **Marqueur applicatif** : le homebrew écrit `SMOKE.OK` avec le jeton attendu.
5. **Journalisation** : copie des journaux pertinents vers `logs/ppsspp/`.
6. **Validation visuelle manuelle** : fenêtre PSP avec triangle coloré et barre UI animée.

## Smoke test prévu

Le projet `test/smoke/` :

- initialise les callbacks de sortie;
- configure GU/GUM en 480×272;
- dessine un fond, un triangle et une barre UI animée;
- charge une texture factice libre générée par `tools/generate-smoke-assets.py`;
- écrit `.test-data/ppsspp/smoke/SMOKE.OK` via `ms0:/PSP/GAME/...`;
- tourne quelques dizaines de frames puis s'arrête proprement.

## Profil PPSSPP isolé

Le launcher réserve :

```text
.test-data/ppsspp/
├── home/
├── config/
├── memstick/
└── smoke/
```

Variables recommandées :

```text
HOME=<repo>/.test-data/ppsspp/home
XDG_CONFIG_HOME=<repo>/.test-data/ppsspp/config
```

Le Memory Stick PPSSPP pointe vers `.test-data/ppsspp/memstick`. Cette isolation doit
empêcher la modification de la configuration PPSSPP utilisateur.

## Critère de succès automatique

Le launcher prépare le marqueur, démarre PPSSPP, surveille le fichier et renvoie :

- `0` si le marqueur apparaît dans le délai et contient le jeton exact;
- `1` si PPSSPP quitte sans marqueur ou si le délai expire;
- un code spécifique si l'EBOOT ou PPSSPP manque.

Cette convention est plus robuste qu'un code de sortie GUI. Le runner doit aussi tuer
PPSSPP après capture du succès pour éviter un processus orphelin.

## Critère visuel manuel

La première preuve fonctionnelle doit montrer :

- couleur de fond stable;
- triangle 3D/libGU visible;
- texture factice correctement orientée;
- petite UI/barre animée;
- absence d'artefact EDRAM évident.

Le critère n'est pas encore automatisé. Une capture d'écran n'est proposée que si PPSSPP
est installé et si une méthode non destructive existe.

## Journaux et artefacts

- `logs/ppsspp/smoke.log` : sortie du launcher et chemin du marqueur;
- `.test-data/ppsspp/` : état jetable de l'émulateur;
- `build/smoke/smoke.elf` : ELF symbolisé;
- `build/smoke/EBOOT.PBP` : paquet test;
- `artifacts/smoke/` : copie facultative du PBP si le test est concluant.

## Tests négatifs

À ajouter dès que la toolchain est disponible :

- asset manquant;
- texture invalide;
- marker path non monté;
- timeout PPSSPP;
- EBOOT absent;
- mémoire insuffisante simulée côté loader;
- réexécution avec profil PPSSPP déjà existant.

## Validation matérielle différée

PPSSPP ne remplace pas une PSP physique pour :

- Media Engine;
- cache CPU et DMA;
- timing audio;
- vitesse Memory Stick;
- comportement thermique/222–333 MHz;
- taille réelle de heap et fragmentation.

Quand un test matériel sera autorisé, PSPLINK + `psp-gdb` et les symboles ELF déjà
conservés fourniront la première boucle de débogage.
