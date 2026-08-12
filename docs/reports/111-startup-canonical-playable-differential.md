# Rapport 111 — Différentiel startup, canonique et jouable

| Chemin | Mode | Ressources | Échec initial | Correction |
|---|---|---|---|---|
| Startup dédié | benchmark startup | chargement direct `data/startup` | aucun | inchangé |
| Smoke canonique | smoke | gestionnaire + manifeste union | capacité 32 pour 36 entrées | capacité 48 |
| Jouable historique | smoke | quatre fichiers directs | DPUI 132 192 dans tampon 24 000 | tampon 160 000 |

Les trois chemins utilisaient le profil isolé mais pas la même frontière de
chargement. Le succès startup ne prouvait donc ni la capacité du manifeste
gameplay, ni le budget du tampon DPUI historique.

Après correction, le même EBOOT canonique gère explicitement ses modes via
`PspRuntimeModeDescriptor`; les tests LaunchServices écrivent toujours le
mode. Le package utilisateur est l’union startup/gameplay et démarre en
`startup`. Aucun marqueur diagnostique ne remplace les marqueurs fonctionnels.

