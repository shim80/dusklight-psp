# Rapport 116 — Référence file select v2

L’oracle conserve 24 états file select et 24 états de panes UI. Le PSP fournit
trois slots, curseur, sélection vide, New Game et validation déterministe des
noms Link/cheval.

Le layout J2D complet, la saisie libre et l’annulation exhaustive ne sont pas
encore portés. Ils sont classés `EXPECTED_PLATFORM_DIFFERENCE`, sans saut de
l’état New Game dans le parcours automatique.
