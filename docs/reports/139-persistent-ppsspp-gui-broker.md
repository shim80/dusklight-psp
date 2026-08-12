# Broker GUI PPSSPP persistant

> Note de succession : ce rapport conserve l'historique du broker initial.
> Son démarrage manuel au premier plan est remplacé par le LaunchAgent borné
> décrit dans `141-autonomous-ppsspp-gui-broker.md`, conformément à l'exception
> étroite désormais présente dans `AGENTS.md`.

## Résultat hôte

`DusklightPpssppGuiBroker` remplace les lancements LaunchServices par test.
Le broker est une application hôte locale, sans lien avec l'EBOOT, qui :

- est lancée une fois dans la session graphique du propriétaire ;
- publie un heartbeat et un PID ;
- revendique atomiquement une requête ;
- exécute les requêtes séquentiellement ;
- importe le validateur et l'exécuteur PPSSPP déjà durcis ;
- rejette tout `PARITY_BUILD_ID` différent ;
- utilise un profil isolé par requête ;
- écrit une réponse portant `transport=persistent_gui_broker` ;
- ne fait aucun accès réseau.

Le transport ne contient plus aucun appel LaunchServices, y compris lors du
démarrage manuel du broker.

## Démarrage propriétaire requis

Le broker ne doit pas être lancé par un processus Codex. La commande exacte à
exécuter depuis un Terminal macOS normal est :

```sh
cd "/Users/shimonalbo/Documents/Projects/dusklight-psp-codex-starter"
scripts/start-ppsspp-gui-broker.sh
```

Une fois le heartbeat `READY`, les runners existants placent leurs requêtes
dans la file du broker. S'il est absent, le résultat reste
`PENDING_GUI_EXECUTION` sans prétendre observer un boot PSP.

## Validation courante

```text
broker_host_build=true
request_schema_valid=true
response_schema_valid=true
foreign_parity_build_id_rejected=true
per_test_launchservices_calls=0
broker_manual_start=pending
psp_execution=PENDING_GUI_EXECUTION
```

## Durcissement du démarrage

La première ouverture propriétaire a laissé un PID sans publier de heartbeat.
Aucune requête n'a été consommée. Le broker publie désormais son heartbeat
avant de charger le moteur PPSSPP, accepte l'argument macOS `-psn_…` et écrit
toute exception fatale dans `logs/broker.log`. Le statut repose sur la
fraîcheur du heartbeat plutôt que sur `kill -0`, qui peut être refusé à un
processus Codex isolé même lorsque le processus GUI appartient au même
utilisateur.

La seconde ouverture a fourni la preuve TCC suivante avant toute requête PSP :

```text
BROKER_FATAL PermissionError: Operation not permitted:
build/reports/PARITY_BUILD_ID.metrics
```

LaunchServices créait une application distincte qui ne bénéficiait pas de
l'autorisation Documents du Terminal. Le script exécute donc désormais le
broker directement au premier plan depuis le Terminal propriétaire.

Le mode arrière-plan détaché a été refusé par la politique permanente du
dépôt. Le Terminal reste ouvert pendant la campagne et `Ctrl-C` arrête le
broker. Aucun service, agent de connexion ou processus caché n'est créé.

La première requête canonique a ensuite révélé une frontière de confinement
historique : le runner n'autorisait que `.test-data/ppsspp/`. Elle a été
étendue exclusivement à `.test-data/ppsspp-gui-broker/`, toujours sous la
racine Git. La requête a été rejetée avant PPSSPP avec
`boot_observed=false`; ce n'est pas un défaut EBOOT.

Après extension, le smoke canonique a produit 18 marqueurs valides avec
`PSP_EBOOT_STARTED_AND_MARKERS_VALID`. Le wrapper a toutefois cherché les
métriques dans son chemin de compatibilité historique : seuls les marqueurs y
étaient recopiés. Le submitter réplique désormais tous les outputs générés,
mais pas l'EBOOT, les packages ni les entrées de scénario. Un ancien fichier
`FailedGraphicsBackends.txt` n'est en outre plus rapporté comme erreur quand
les marqueurs de la requête courante sont valides.
