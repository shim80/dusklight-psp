# Broker GUI PPSSPP autonome

## État avant bootstrap

Le transport est prêt côté dépôt, mais le LaunchAgent n'est pas encore
enregistré. La classification reste donc `BROKER_NOT_BOOTSTRAPPED`. Aucune
exécution PPSSPP de cette phase n'est interprétée comme une preuve de boot PSP.

Le composant persistant est limité au superviseur
`com.dusklight.ppsspp-gui-broker`, dans le domaine `gui/$UID`. Son plist est
généré sous `.test-data/ppsspp-gui-broker/`; rien n'est copié dans un dossier
LaunchAgents et aucun autre service n'est créé.

## Architecture validée sur l'hôte

- Le superviseur ne charge ni runner, ni worker, ni collecteur.
- Chaque requête atomique démarre un worker Python neuf depuis les sources
  courantes du dépôt.
- Le worker charge à neuf le runner et le collecteur, puis valide les chemins,
  les liens symboliques, l'exécutable PPSSPP et les empreintes attendues.
- Un seul worker est actif à la fois ; une requête interrompue est remise dans
  la file au redémarrage du superviseur.
- Le heartbeat expose le PID, la génération, le protocole, l'état et la requête
  active.
- Les compteurs distinguent bootstrap manuel, redémarrage automatique,
  workers, lancements PPSSPP et artefacts collectés.
- `collect_existing_artifacts=true` réutilise le Memory Stick isolé sans
  lancer PPSSPP.
- Les core smokes historiques portent
  `identity_scope=historical_core_smoke` et ne revendiquent pas le
  `PARITY_BUILD_ID` courant.

## Résultats pré-bootstrap

```text
plist_repository_local=true
plist_aqua_session=true
run_at_load=true
keep_alive=true
supervisor_imports_mutable_worker=false
worker_loaded_per_request=true
artifact_collector_loaded_per_request=true
request_paths_confined=true
runner_hash_validation_on_recovery=true
artifact_only_classification=MARKERS_VALID_METRICS_VALID
artifact_only_metrics_files=12
ppsspp_launched_for_artifact_recovery=false
campaign_items=43
launchagent_registered=false
```

Le test dynamique borné du worker a récupéré les douze fichiers `.METRICS`
du smoke canonique existant avec `result_code=0`. Le smoke canonique antérieur
reste la preuve des 18 marqueurs ; il n'a pas été relancé pendant cette passe.

## Selftest après le bootstrap unique

`scripts/test-ppsspp-gui-broker-autonomy.sh` enchaînera, avec le même PID de
superviseur : core smoke, deux smokes canoniques, récupération des métriques,
trace DTRC, échec contrôlé puis requête valide. La classification
`READY_AUTONOMOUS_PPSSPP_GUI_BROKER` ne sera produite qu'après validation de
ces sept workers et des compteurs sans redémarrage manuel ni nouvelle demande
utilisateur.

La campagne `global-parity` est persistée sous le profil isolé et comporte 43
tâches reprenables : 40 scénarios, Performance, PSP conservative et une seule
release. Son exécution reste en attente du bootstrap initial.

## Nettoyage

Après collecte complète des rapports et artefacts, le service se retire avec
`scripts/unbootstrap-ppsspp-gui-broker.sh`. Le plist et tous les états restent
dans le dépôt ; aucun nettoyage externe n'est nécessaire.
