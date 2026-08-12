# Référence desktop exacte de Dusklight

Cette arborescence décrit l’oracle comportemental desktop utilisé par le port
PSP. Les sources matérialisées, builds, profils, traces et captures restent
ignorés par Git.

La référence principale est le commit Dusklight épinglé dans
`reference-source.lock`. Elle n’est ni la cible du port ni une source de
dépendances pour l’EBOOT PSP.

Les scripts refusent le réseau et ne lisent ou n’écrivent que sous la racine du
projet. `source-vanilla` reste strictement non modifié. L’instrumentation est
appliquée uniquement à `source-trace` à partir du même commit.

Les patches numérotés ajoutent uniquement le schéma
`dusklight.desktop.reference.v1` et l’injection déterministe à la frontière
PAD. `scripts/apply-dusklight-desktop-reference-trace.sh` vérifie les révisions
avant application. La trace startup validée et son manifeste portable sont
versionnés sous `reference/desktop/traces/`.

Le scénario de référence est :

```text
title key_wait
→ START
→ création du fichier de sauvegarde
→ premier emplacement
→ noms Link et cheval
→ PLAY_SCENE
→ F_SP108 / point 21 / room 1 / layer 13
```

Le profil initial injecté correspond au preset `Classic` exact de la révision
épinglée. Il évite seulement que la fenêtre de premier lancement bloque le PAD ;
il ne modifie ni les états du jeu ni les sources vanilla.
