# Rapport 122 — Réconciliation du package canonique

Le script de packaging construit maintenant un package union startup/gameplay,
valide les SHA des ressources critiques et vérifie qu’aucun fichier généré n’est
plus ancien que ses entrées.

Le package utilisateur commence en startup ; les modes smoke et benchmark sont
explicites via `RUNTIME.MODE` et le descripteur `PspRuntimeModeDescriptor`.

Le smoke historique direct n’est plus une preuve canonique suffisante ; le gate
de release exige les marqueurs du package union.
