# Selftest du broker GUI PPSSPP autonome

## Classification

`READY_AUTONOMOUS_PPSSPP_GUI_BROKER`

Le selftest post-bootstrap a exécuté sept workers sous un superviseur unique :

1. core smoke historique ;
2. smoke canonique ;
3. récupération de métriques sans PPSSPP ;
4. second smoke canonique ;
5. trace DTRC `link_idle_full_cycle` ;
6. requête invalide contrôlée ;
7. récupération valide après l'échec.

## Résultat observé

```text
supervisor_pid=97181
supervisor_pid_stable=true
workers_loaded_fresh=true
worker_fixture_a=true
worker_fixture_b=true
markers_18_valid=true
metrics_retrieved=true
dtrc_retrieved=true
request_after_failure_valid=true
manual_restart_count=0
user_confirmation_prompts_after_bootstrap=0
```

Le smoke canonique produit 18 marqueurs valides et douze fichiers
`.METRICS`. Le core smoke produit `DUSKLIGHT_PSP_CORE_OK`. Le worker suivant
l'échec contrôlé retourne de nouveau `result_code=0`.

## Défaut de transport macOS corrigé

Le LaunchAgent Aqua ne pouvait pas revendiquer les fichiers créés par Codex
sous `Documents` à cause de leur provenance TCC. Le transport définitif garde
tout sous la racine du dépôt :

- mailbox structurée créée par le superviseur ;
- requête atomique matérialisée par le superviseur ;
- worker, collecteur et runner chargés à neuf depuis le bundle signé ;
- PPSSPP épinglé copié une seule fois dans ce bundle ;
- EBOOT, configuration et packages préparés sous les Resources du bundle ;
- HOME, XDG, cache, TMP et Memory Stick isolés par requête sous les Resources
  de PPSSPP ;
- preuves recopiées dans les chemins canoniques par le client après la réponse.

Les empreintes PPSSPP, EBOOT, configuration et packages sont toujours
validées avant exécution. Aucun profil PPSSPP personnel, accès réseau ou
emplacement extérieur au dépôt n'est utilisé.

Les redémarrages automatiques visibles avant le selftest correspondent aux
itérations de réparation du superviseur. Aucun redémarrage manuel ni nouvelle
intervention propriétaire n'a été nécessaire après le bootstrap initial.
