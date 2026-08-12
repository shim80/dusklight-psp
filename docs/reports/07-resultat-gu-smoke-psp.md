# Résultat du smoke libGU/GUM

Date : 2026-07-16

## Statut

`READY_FOR_STATIC_ASSET_RENDERING`

Le pipeline libGU/GUM a été validé indépendamment du noyau gameplay. Le test PSP
initialise l'écran 480×272 en 5650, le depth buffer en 16 bits, active depth test,
scissor, culling et projection perspective, puis dessine :

- un cube opaque coloré;
- un plan alpha-testé;
- un triangle transparent derrière;
- un HUD 2D;
- un shader shim non lighting qui active texture + TCC RGBA + alpha test + blend;
- un second mode « lit » qui active light0/diffuse/specular côté GU;
- une texture checker swizzlée hors ligne par le générateur d'assets.

Le même générateur applique deux des formats prévus pour le port :

- 5650 opaque;
- 4444 alpha.

## Build et inspection

La cible `test/gu-psp/` est construite avec `psp-cmake` puis inspectée par
`scripts/inspect-gu-smoke.sh`.

Résultat actuel :

```text
DUSKLIGHT_PSP_GU_SMOKE_STATIC_OK
```

Le PBP contient les chaînes de marqueur et les trois scénarios d'erreur de
configuration du shim.

## Exécution PPSSPP

Les runs initiaux ont exposé deux limites distinctes :

1. le premier runner générique copiait seulement l'EBOOT, donc le test échouait
   sur `opaque_5650.bin` / `alpha_4444.bin` absents;
2. le mode LaunchServices contournant les restrictions de fenêtres ne conserve
   pas les variables de profil du sous-processus.

Le runner supporte maintenant :

- `--game-file source[=dest]` répétable;
- `--launch-mode auto|direct|launchservices`;
- `--profile managed|system`;
- `--script-mode run|request-only`;
- manifeste PPSSPP manuel complet si l'automatisation n'est pas possible.

Le run manuel final, exécuté par l'utilisateur depuis son terminal dans le même
dépôt, a produit :

```text
.test-data/ppsspp/manual-gu/PSP/GAME/DUSKLIGHT_GU/GU.OK
DUSKLIGHT_PSP_GU_OK
```

La capture PPSSPP correspondante est
`.test-data/ppsspp/manual-gu/psp_screen_3d.png`; elle montre le cube 3D texturé,
le plan jaune et la barre HUD à 60 FPS.

Le test GU est donc fonctionnel sous PPSSPP. La restriction restante concerne
l'automatisation graphique depuis le runner Codex macOS, pas l'exécution PSP.

## Valeur pour le portage

La pile GU valide désormais les briques nécessaires au premier renderer d'asset
réel :

- vertex format PSP;
- matrices GUM;
- depth test;
- culling;
- textures swizzlées;
- alpha test/blend;
- éclairage fixe optionnel;
- HUD sans ImGui;
- changement contrôlé d'états via shim.

Elle ne prouve pas encore les matériaux ou modèles Dusklight. L'étape suivante
reste un loader de packages convertis avec format versionné et tests négatifs.
