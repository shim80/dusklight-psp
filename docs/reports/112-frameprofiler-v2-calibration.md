# Rapport 112 — Calibration FrameProfiler v2.1

## Résultat

Classification : `READY_DUSKLIGHT_PSP_CALIBRATED_BENCHMARK_V2`.

Trois parcours titre complets ont booté et produit `STARTUP.PARTIAL` et
`NEW_GAME_TRANSITION.OK`. Functional emploie le renderer logiciel ; Performance
et PSP conservative emploient le renderer matériel. Les trois configurations
ont des SHA-256 distincts. Conservative prouve `FastMemory=False`, CPU 222 MHz,
résolution native et modèle PSP 1000 dans sa configuration.

## Corrections

- contrat émis : v2.1 ;
- timer : `sceKernelGetSystemTimeWide` ;
- readback périodique réellement exécuté en Functional ;
- mémoire de pic inconnue : `available=false`, `unavailable` ;
- compteur GE matériel : indisponible ;
- temps de soumission et attente sync séparés ;
- durée profiler comparée à la durée LaunchServices.

Les anciennes métriques v2 identiques ne sont plus une baseline performance.
Le marqueur `BENCHMARK_V2_1.OK` n’est produit que par
`scripts/validate-profiler-calibration.sh`.

La précision et les performances matérielles restent à valider sur PSP
physique ; `hardware_validation=deferred_by_user`.

