# Rapport 123 — Benchmark v2.1

FrameProfiler v2.1 sépare simulation, soumission, attente sync, compteur GE
indisponible et mémoire de pic indisponible. Functional demande un readback
périodique réellement exécuté ; Performance et PSP conservative gardent le
renderer matériel.

Le marker final `BENCHMARK_V2_1.OK` est généré par le validateur seulement
après contrôle de trois profils, des configs hachées, du readback Functional,
des champs indisponibles et de la cohérence avec les temps LaunchServices.

Les performances matérielles et le coût du GPU réel restent différés jusqu’à
une PSP physique.
