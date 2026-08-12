# Rapport 118 — Référence Link v2

`desktop_link_reference_v2.csv` contient les états Link observés dans F_SP108 :
position, rotation, procédure et vitesse, complétés par animation, joints et
collision dans la trace JSONL.

L’idle initial est `MATCH_WITH_TOLERANCE` avec le runtime PSP existant. Walk,
run, turn, stop et slope ne sont pas fermés par cette exécution automatique et
restent `MISSING_ON_PSP` dans le comparateur, sans extrapolation visuelle.
