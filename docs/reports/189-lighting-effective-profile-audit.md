# Audit du profil lighting effectivement livré

## Résultat

```text
LIGHTING_EFFECTIVE_PROFILE_AUDIT=DONE
CANONICAL_INTERACTIVE_PROFILE=known_good_unlit
CANONICAL_INTERACTIVE_LIGHTING=off
CANONICAL_INTERACTIVE_FOG=off
CANONICAL_INTERACTIVE_SHADOWS=off
PARITY_TRACE_LIGHTING=source_approx_cpu
CANDIDATE_GAME_DELIVERED=false
LIGHTING_MATERIAL_PARITY=NOT_READY
LIGHTING_PHASE_B_HOST_PREPARATION=DONE
```

Le profil du jeu canonique effectivement exposé au joueur reste `known_good_unlit`. Link est dessiné avec `GU_LIGHTING` et `GU_LIGHT0` désactivés, une couleur vertex blanche et `GU_TFX_REPLACE`. La room n'est pas éclairée dynamiquement : ses couleurs vertex ont été précalculées pendant la conversion à partir des normales source.

Le chemin `candidate_game` existe comme contrat mais aucun appelant de production ne le sélectionne. Il ne faut donc pas présenter son éclairage `source_approx` comme le profil actuellement livré.

## Preuve de sélection effective

Le paquet canonique contient `DUSKLIGHT.MODE=startup`, `DUSKLIGHT.PRESENTATION=game`, un EBOOT identique entre `build/psp/dusklight/EBOOT.PBP` et le paquet livré (SHA-256 `de212320a2ad3e55984b03b795c00b03c8f4c2bf03b977fd08be760a60170695`), la chaîne `default_render_profile=known_good_unlit` et l'identité de build `4ffabb071abe1c8bcca1fd3d3e3472d3dbfc3764`.

`test/room-transition/main.cpp` sélectionne `KnownGoodUnlit` pour Interactive, BenchmarkBootIntro, BenchmarkTitle, BenchmarkLinkIdle et BenchmarkTransition. Le contrat `render_profile_config(KnownGoodUnlit)` fixe éclairage, fog et ombres à Off. `draw_real_link()` désactive explicitement `GU_LIGHTING`, `GU_LIGHT0` et `GU_FOG`, puis utilise `GU_TFX_REPLACE`.

Les modes non énumérés prennent `LightingDiagnostics`, puis `SourceApprox`, `Source` et `ProjectedLink`, notamment `BenchmarkRoomStress` et `ParityTrace`. Le profil `OpaqueOnly` neutralise fog et ombres mais ne change ni `render_profile` ni `lighting_mode`. Un scénario `parity_trace` avec `opaque_only` conserve donc le calcul CPU `SourceApprox` sur Link, alors que le jeu interactif utilise Off. Ce calcul ne change ni la pose, ni la profondeur, ni l'ordre des soumissions ; il change seulement les couleurs de Link.

Le booléen `lighting` de la trace PSP vaut vrai uniquement pour `LightingMode::GuCandidate`; il ne signale pas le chemin CPU `SourceApprox`.

## Données de normales disponibles

### Link

`link.dpsk` conserve une normale XYZ flottante par sommet. Le paquet courant contient 3 543 sommets, 27 sous-maillages, stride 64 octets, aucune normale nulle ou non finie, longueur source minimale 0,999908573 et maximale 1,000000079. Le runtime applique la partie 3x3 des matrices de skinning pondérées puis renormalise chaque normale.

```text
NORMAL_PIPELINE_HOST_OK source=3543 runtime=3543 zero=0 non_finite=0
length=1.000000:1.000000 error_mean=0.000000010
error_max=0.000000119 debug_variance=0.028288323
```

### Rooms d'acceptation

Les DPRM des rooms ne conservent pas les normales. Leur sommet de 24 octets contient UV, couleur ABGR précalculée et position XYZ. La conversion appelle `baked_room_color()` avec une direction fixe `(0.30304575, 0.80812204, 0.50507627)` et une composante ambiante de `0.38`.

| Room | Sommets | Sous-maillages | Couleurs précalculées distinctes | Normales runtime |
|---|---:|---:|---:|---|
| F_SP108/R01 | 10 426 | 22 | 379 | absentes |
| D_MN10/R09 | 18 000 | 23 | 314 | absentes |
| D_MN10/R02 | 23 953 | 19 | 346 | absentes |

## États matériaux disponibles

### Link — DPTX

Chaque record de matériau Link conserve texture, bucket provisoire, drapeau lighting, base, ambient, diffuse, emissive, identifiant source, alpha, nombres TEV/texgens, fallback et fonctions diffuse/atténuation compactées. Le runtime contient 27 records pour 18 matériaux source distincts ; les 27 ont lighting activé, base blanche, ambient `0xFF101010`, diffuse blanche, emissive noire et alpha 255. Vingt-cinq ont 1 stage TEV/1 texgen, deux ont 3/3. Ces données sont insuffisantes pour une parité J3D/TEV complète.

La campagne historique a établi que L6 `SourceApprox` donnait une luminance Link de `0.000531994` et un ratio de pixels noirs de `0.996453881`; le profil a été rejeté sans compensation d'exposition arbitraire.

### Rooms — DPRM et DPTX

Le DPRM conserve les identités source shape/material, bucket et Z mode exact, mais ni normale ni état lighting de matériau. Le DPTX room garde numéro de texture, bucket et nombres TEV/texgens ; ses champs lighting sont des shims de conversion constants et ne constituent pas une capture fidèle des couleurs/contrôles de canal J3D.

| Room | Matériaux | TEV observés | Texgens observés |
|---|---:|---|---|
| F_SP108/R01 | 22 | 6 × 1 ; 5 × 2 ; 11 × 3 | 7 × 1 ; 14 × 2 ; 1 × 3 |
| D_MN10/R09 | 23 | 23 × 1 | 23 × 1 |
| D_MN10/R02 | 19 | 19 × 1 | 19 × 1 |

## Environnement disponible

DPSC v4 conserve un record d'environnement source par scène : ambient room/actor, couleur et direction de lumière principale, lumière locale, fog, clear color et ombre.

| Scène | Ambient actor | Key color | Key direction | Lumières locales |
|---|---|---|---|---:|
| F_SP108/R01 | `0xFF3E3539` | `0xFF78615B` | `(0, 0.707107, 0.707107)` | 0 |
| D_MN10/R09 | `0xFF353425` | `0xFF38250E` | `(0.012483, 0.297633, -0.954599)` | 0 |
| D_MN10/R02 | `0xFF353425` | `0xFF38250E` | `(-0.000712, 0.164231, 0.986422)` | 0 |

## Tests ciblés

```text
RENDER_PROFILE_HOST_OK default=known_good_unlit lighting=off fog=off shadows=off
COLOR_PACKING_HOST_OK cases=9 package=ARGB gu=ABGR memory=RGBA
NORMAL_PIPELINE_HOST_OK source=3543 runtime=3543 zero=0 non_finite=0
MATERIAL_LIGHTING_HOST_OK materials=27 lit=27 unlit=0 fallback=0
LIGHT_SPACE_HOST_OK source=world psp=model_cpu normalized=true
GU_STATE_ISOLATION_HOST_OK sentinel=black_caster textured_white_restored=true fields=58
ENVIRONMENT_RUNTIME_HOST_TEST_OK records=2 fog=true local_lights=0 negative_cases=4
HOST_ENVIRONMENT_RUNTIME_OK records=2 transition_regression=true
CANONICAL_PACKAGE_VALID entries=37 sealed=false
```

L'agrégat `test-idle-lighting-host.sh` n'est pas vert dans le worktree courant : `link_idle_foot_slip_host_test` imprime `left_max=0.460888` et `right_max=0.482763`, puis retourne 1 pour un seuil `0.25`. Les cinq tests lighting lancés séparément passent. Cette régression animation/grounding est hors du présent audit et n'a pas été masquée.

## Porte suivante

`LIGHTING_EFFECTIVE_PROFILE_AUDIT` peut être fermé. `LIGHTING_MATERIAL_PARITY` ne doit pas être ouvert comme intégration runtime tant que le profil effectif, la télémétrie CPU SourceApprox, les états de canaux/TEV source, les normales de room éventuelles et une nouvelle preuve visuelle Link ne sont pas acquis.

Aucun renderer, runtime canonique, paquet ou oracle desktop n'a été modifié par cet audit. Aucun accès réseau ni nouvelle suite complète n'a été exécuté.

## Phase B — préparation hôte générique

Un exécutable hôte distinct charge les paquets de Link et des trois rooms et exerce onze setups diagnostics L0–L10. Sur R09, les luminances min/moy/max vont de 1/1/1 pour L0 à 0.025162/0.120967/0.334005 pour L6 ; L8 conserve une variance de 0.028288323. Les égalités L0/L1, L6/L7 et L0/L10 sont vérifiées.

L'inventaire automatisé retrouve 64 records matériau room, 0 normale sérialisée et 1 039 couleurs vertex précalculées distinctes. Un canal `lighting_us` de FrameProfiler est testé séparément avec distribution synthétique bornée. Deux acquisitions historiques performance donnent 7 128 us moyens (Link idle) et 24 129 us (room stress), uniquement comme preuve historique PPSSPP et non comme mesure PSP physique.

Fichiers de préparation : `test/link-playable/host_lighting_preparation_test.cpp`, cible `lighting_preparation_host_test`, `scripts/test-lighting-preparation-host.sh`, `scripts/validate-lighting-benchmark-costs.sh`.
