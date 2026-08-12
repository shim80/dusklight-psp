# Succès du smoke canonique à 18 marqueurs

## Preuve préservée

La requête `20260731T105530Z-smoke-1` a exécuté l'EBOOT de parité courant par
le broker GUI avec le résultat suivant :

```text
classification=PSP_EBOOT_STARTED_AND_MARKERS_VALID
result_code=0
boot_observed=true
markers_valid=true
marker_count=18
ppsspp_exit_status=0
transport=persistent_gui_broker
parity_build_id=sha256:28e8358a579e5d0fed12945a4a369f2e040775263c5e44681bf779dde9e40a03
eboot_sha256=d2d00406b491d3229613ca883def49bc9bc36b781fc10c68a9bd48c7e7e7ba52
```

Les 18 entrées de `marker_results` existent et ont toutes
`content_valid=true`. Douze fichiers `.METRICS` existent encore sur le Memory
Stick isolé de cette requête.

L'erreur postérieure du wrapper ne remet pas cette preuve en cause : elle est
survenue lors de la copie hôte des métriques, après la réponse PSP réussie.
La récupération autonome doit donc employer `collect_existing_artifacts=true`
avant d'envisager un nouveau smoke.
