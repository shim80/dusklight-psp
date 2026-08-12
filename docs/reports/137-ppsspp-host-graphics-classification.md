# Classification du défaut graphique hôte PPSSPP

## Résultat

Le lancement canonique du core smoke du 31 juillet 2026 n'établit pas un
défaut de l'EBOOT. Il est classé comme défaut de transport hôte :

`HOST_GRAPHICS_INIT_FAILED`

## Preuves observées

- transport : `launchservices_gui` ;
- profil PPSSPP : profil isolé propre à la requête ;
- backend demandé : OpenGL ;
- renderer PSP demandé : software ;
- PPSSPP a écrit une ligne `Booted` avant l'initialisation graphique ;
- stderr contient `OpenGL 2.0 or higher.` ;
- le profil isolé contient
  `PSP/SYSTEM/FailedGraphicsBackends.txt` avec `OPENGL` ;
- aucun marqueur PSP n'a été produit.

La ligne `Booted` prouve la sélection du payload, pas l'exécution réussie du
runtime après initialisation du backend graphique.

## Correction

Le runner reconnaît désormais :

- le diagnostic stderr OpenGL minimal ;
- le fichier PPSSPP `FailedGraphicsBackends.txt` du profil isolé ;
- la priorité du défaut graphique hôte sur un échec runtime ou marqueur quand
  aucun marqueur valide n'a été observé.
- une attente LaunchServices bornée au timeout PSP augmenté de 15 secondes ;
  l'application GUI est lancée par LaunchServices sans attente système
  indéfinie, puis le client attend la réponse structurée.

Un marqueur valide conserve toujours la priorité et ferme le test avec succès.

## Portée

Ce correctif ne modifie ni l'EBOOT, ni les sources PSP, ni le profil demandé.
Les validations Functional restent strictement OpenGL + software. Tant que ce
backend hôte est indisponible, les nœuds dépendant de PPSSPP restent
`PENDING_GUI_EXECUTION` et non `BLOCKED_PPSSPP_BOOT`.

La première tentative de confirmation après correction du classifieur est
restée bloquée dans l'attente LaunchServices avant le démarrage du runner :
aucun stdout, stderr ou résultat de runner n'a été créé. Cette observation a
motivé le bornage du client ; elle ne constitue pas un signal de boot PSP.
