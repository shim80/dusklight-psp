# Rapport 134 — Pipeline de revue visuelle

## Résultat

`tools/dusk_visual_parity_review/dusk_visual_parity_review.py` produit, pour
deux captures de mêmes dimensions :

- `desktop.png` ;
- `psp.png` ;
- `side_by_side.png` ;
- `overlay_50.png` ;
- `difference_heatmap.png` ;
- `landmarks.csv` ;
- `screen_bounds.csv`.

Il ne redimensionne pas silencieusement une capture et refuse les landmarks
hors viewport. Le self-test utilise une image générée localement, vérifie une
différence nulle sur une auto-comparaison et rejette des dimensions
incompatibles.

Le heatmap est un diagnostic brut. Les décisions de parité doivent se fonder
sur les landmarks, silhouettes, bounds écran, visibilité et classification
des différences de plateforme ; une variation TEV/couleur ne devient pas une
erreur de transform.

## État d’exécution

L’outil est prêt, mais aucune nouvelle paire desktop/PSP n’a été fabriquée à
partir d’anciennes captures. Les captures Functional du build courant restent
`PENDING_GUI_EXECUTION`.

```text
visual_pipeline_ready=true
visual_derivatives=7
synthetic_self_test=true
real_scenario_pairs_current=0
functional_capture=PENDING_GUI_EXECUTION
global_review_package_ready=false
user_manual_direction_validation=pending
user_manual_acceptance=pending
```
