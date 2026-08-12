# Rapport 114 — Caméra titre desktop

Sept checkpoints F_SP102 (frames 0 à 1 800) fournissent eye, center, up, FOV,
aspect, near/far et matrices. Le PSP reprend eye/center/FOV et adapte seulement
l’aspect 480/272 et le near à 20.

Le test hôte exécute le même code que l’EBOOT aux frames 0, 900 et 1 800 avec
une tolérance de 0,02. Résultat :
`STARTUP_CAMERA_PARITY_HOST_OK`.

Parité : `MATCH_WITH_TOLERANCE`.
