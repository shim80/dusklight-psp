# Rapport 135 — Provenance des benchmarks de parité

## Résultat

Les quinze runs benchmark v1 et le contrat v2.1 sont présents et conservent
leur valeur de baseline historique. Ils ne correspondent pas à l’EBOOT de
parité courant :

```text
current_eboot_sha256=6bca91b86dfe30fff863b8f3b742aecb52d24f65d4bbc87bb3262472b16d21ec
packaged_eboot_sha256=8525c36b5ae49753438fa8486eb913e51556786e9247484f82731199084c3b06
benchmark_v1_recorded_eboot_sha256=d3bbfe9221a331a09ca6ee04a4a437fab4e4c0e66fa242f7b9e2851ea67254cd
```

Les profils historiques restent correctement séparés :

- Functional : OpenGL + software ;
- Performance : OpenGL + hardware ;
- PSP conservative : OpenGL + hardware, réglages conservateurs ;
- transport : `launchservices_gui` ;
- profil isolé et réseau désactivé.

Ils ne sont pas réétiquetés comme mesures « après » des corrections actuelles.

## Porte performance

`scripts/test-parity-performance-provenance.sh` valide les trois manifests, les
quinze fichiers métriques, le transport et la provenance. Il produit
`build/reports/parity-performance-status.metrics`.

La décision de régression doit attendre trois nouvelles campagnes sur l’EBOOT
courant :

```text
baseline_profiles=3
baseline_scene_runs=15
current_profile_runs=0
benchmark_v1_regression=PENDING_GUI_EXECUTION
benchmark_v2_1_regression=PENDING_GUI_EXECUTION
ram_delta=PENDING_GUI_EXECUTION
edram_delta=PENDING_GUI_EXECUTION
draw_delta=PENDING_GUI_EXECUTION
command_list_delta=PENDING_GUI_EXECUTION
performance_judgement=PENDING_GUI_EXECUTION
user_manual_direction_validation=pending
user_manual_acceptance=pending
```

Cette attente n’est pas un défaut de boot. Aucun lancement PPSSPP direct n’a
été effectué.
