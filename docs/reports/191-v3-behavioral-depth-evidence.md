# Rapport 191 — preuve comportementale de profondeur V3

## État courant

```text
V3C_RAW_DEPTH_EDRAM_READBACK=BLOCKED_LOCAL_EMULATOR_OBSERVABILITY
V3C_BEHAVIORAL_DEPTH_EVIDENCE=IN_PROGRESS
V3C_SYNTHETIC_GE_FIXTURES=DONE
V3C_OPAQUE_ORDER_INVARIANCE=READY
V3D_DEPTH_VISUAL_VALIDATION=WAITING(V3C_BEHAVIORAL_DEPTH_EVIDENCE)
DEPTH_PIPELINE.OK=NOT_EMITTED
```

La première famille d'exécution PSP du contrat `DUSKLIGHT_PSP_BEHAVIORAL_DEPTH_VALIDATION_V1` est acquise. Elle ne lit jamais le Z-buffer brut. Elle observe le résultat des états de profondeur dans le color buffer, puis vérifie les pixels, hashes, états et compteurs produits par le même EBOOT canonique.

## Correction du transport

La première soumission a été rejetée avant tout boot par la liste blanche du runner GUI. Ce résultat était un défaut hôte, pas un défaut de l'EBOOT. Les commits suivants l'ont corrigé sans élargir le transport à des modes arbitraires :

| Commit | Résultat |
|---|---|
| `b94eefb` | `depth_behavior_fixture` ajouté à la liste blanche et au schéma ; mode arbitraire toujours rejeté |
| `b7bd8bd` | assertion littérale du test du broker corrigée |

Le bundle autonome a ensuite été reconstruit et signé localement. Le test hôte a confirmé un superviseur minimal, des workers frais, deux applications autorisées et la sérialisation. Le fichier `runner.py` embarqué était identique au fichier versionné avant l'acquisition.

## Acquisition PSP

```text
request_id=20260802T120018Z-depth_behavior_fixture-1
classification=MARKERS_VALID_METRICS_VALID
boot_observed=true
graphics_backend_used=opengl
psp_renderer=software
ppsspp_exit_status=0
synthetic_depth_cases=16
synthetic_depth_failures=0
allocations=0
depth_state_leaks=0
render_target_state_leaks=0
near_far_valid=true
depth_monotonic=true
reversed_depth_mapping_valid=true
order_invariant=true
error_code=0
```

Identités de l'artefact exécuté :

```text
PARITY_BUILD_ID=sha256:fdb4acc511b62e12cc3a6ca6635b849054c8ee8c2c1098a2ecd52d030a0e93a4
VISUAL_BUILD_ID=sha256:6fa23228c2dca9d26eae0bb7c323c6224aa37694a9479ec2757aa3e4734d1fae
EBOOT_SHA256=4fc7eec0675050f6133b4dbffeb560339b4c84c4fc402717e1fba32483d80ad9
```

Les paires `Z1/Z2` et `Z3/Z4` ont des hashes framebuffer identiques entre ordre direct et ordre inverse. Les cas write on/off, LESS/LEQUAL, égalité, near/far, mapping inversé, clear puis draw et restauration d'état ont tous produit `PASS`. La trace contient exactement seize événements et le marqueur contient exactement, sans saut de ligne final :

```text
DUSKLIGHT_PSP_DEPTH_BEHAVIOR_FIXTURE_OK
```

## Frontière de la preuve

Ce résultat ferme uniquement les fixtures synthétiques B. Il ne ferme pas encore V3D : l'invariance d'ordre de la scène réelle, les témoins par seuil, l'oracle hôte, les visualisations de rejets/ordre et les paires d'occlusion desktop/PSP restent requises.

La lecture brute demeure honnêtement déclarée indisponible :

```text
raw_depth_readback_available=false
raw_depth_readback_status=UNOBSERVABLE_IN_CURRENT_PPSSPP
raw_depth_readback_required_for_acceptance=false
validation_method=behavioral_depth_witnesses
real_psp_raw_depth_validation=pending
```

Aucun bucket alpha, ordre canonique du jeu, oracle desktop ou profil Performance n'a été modifié par cette étape.
