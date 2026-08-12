# Campagne autonome de parité globale — résultat

## Classification

`READY_UNATTENDED_PPSSPP_PARITY_CAMPAIGN`

Cette classification porte sur l'orchestration : la campagne PPSSPP peut être
exécutée dans la session Aqua sans Terminal persistant, redémarrage manuel ou
confirmation entre les requêtes. Elle ne signifie pas que la parité de contenu
desktop/PSP est atteinte.

## Identité canonique

- commit de build PSP : `deed00eb696e3a6ed8660993cf75bfdd004d07cb` ;
- EBOOT SHA-256 :
  `f40b89d1ec86677bc5dd3ec0e852e6c5dcb91290098d9e432eb396c00f89041b` ;
- `PARITY_BUILD_ID` :
  `sha256:51b668fc77b0b328c3e7223c70f5427d9ccbfd5cee0f7446acb0d119b5f1ff00` ;
- schéma de trace : `DTRC_V3` ;
- contrat : `DUSKLIGHT_DESKTOP_PSP_PARITY_CONTRACT_V1`.

Le même EBOOT est présent dans le build de travail et le paquet final. La
finalisation lit désormais le commit depuis `PARITY_BUILD_ID.metrics`, au lieu
d'attribuer rétroactivement le binaire au `HEAD` d'infrastructure courant.

## Exécution de campagne

La file `global-parity` contient 43 éléments et termine avec :

- `succeeded=43` ;
- `failed=0` ;
- `pending=0`.

Cela couvre 40 acquisitions de scénario, cinq scènes Performance, cinq scènes
PSP conservative et la release automatisée. Les deux profils benchmark sont
liés à l'EBOOT canonique. Les 26 captures de fidélité et les 28 captures de
revue rendu sont présentes, soit 54 images de revue.

Le smoke historique, le core smoke, le smoke canonique à 18 marqueurs, les
revues, le replay, les 100 transitions de stress et le mode interactif ont été
validés. L'exécution Performance emploie le backend accéléré Vulkan ; PSP
conservative emploie OpenGL. Ces mesures PPSSPP ne remplacent pas une mesure
sur PSP physique.

## Résultat de parité

Les dix scénarios Link disposent de traces PSP natives DTRC v3 et se comparent
à l'oracle desktop. Les dix sont classés `PARTIAL_PARITY` :

- événements desktop : 52 661 ;
- événements PSP : 49 141 ;
- événements alignés : 49 093 ;
- première divergence causale commune : tick 0, `actor_transform.procedure` ;
- valeur desktop : 3 ;
- valeur PSP : 0.

Cette divergence n'a pas été masquée et l'oracle desktop n'a pas été modifié.

Les trente autres scénarios sont classés `MISSING_OR_NOT_PORTED`. Chaque
acquisition indique `dtrc_native=false` et `fabricated_trace=false` : aucune
trace artificielle n'est présentée comme une preuve PSP.

Aucun des quatre marqueurs finaux de parité n'est créé :

- `TRANSFORM_PARITY.OK` ;
- `LINK_BEHAVIOR_PARITY.OK` ;
- `SCENE_BEHAVIOR_PARITY.OK` ;
- `GLOBAL_PARITY_REVIEW.OK`.

## Autonomie du broker

Le heartbeat final observé rapporte le label
`com.dusklight.ppsspp-gui-broker`, la génération 2, le protocole 2 et l'état
`READY`. Les métriques de campagne rapportent notamment :

- `manual_restart_count=0` ;
- `unplanned_restart_count=0` ;
- `user_confirmation_prompts=0` ;
- `duplicate_requests=0` ;
- `lost_requests=0` ;
- `workers_spawned=85` ;
- `artifact_only_recoveries=2` ;
- `error_code=0`.

Les 12 requêtes en échec du compteur cumulé correspondent aux diagnostics et
essais corrigés pendant la stabilisation ; une requête valide a continué après
ces échecs. Les correctifs du worker et du collecteur ont été chargés sans
redémarrage manuel. Le rechargement rare du superviseur a utilisé le
`kickstart -k` autorisé du LaunchAgent local au dépôt.

## Incidents corrigés

Trois défauts d'infrastructure ont été observés et corrigés :

1. le jeton de reload local était bloqué par la frontière TCC ; le reload passe
   maintenant directement par le job launchd autorisé ;
2. les répertoires de captures vides n'étaient pas transportés jusqu'au worker ;
3. les fichiers de contrôle des modes stress et interactif n'étaient pas inclus
   dans le paquet de requête GUI.

Le finaliseur vérifie aussi que le hash EBOOT du paquet correspond exactement à
l'identité de build. Aucun accès réseau n'a été utilisé.

## Livrable

Le paquet `artifacts/dusklight-psp-global-parity-review/` contient les matrices,
les traces et comparaisons courantes, les benchmarks, les captures, l'identité
de build et les métriques du broker. Il est volontairement dépourvu des
marqueurs de succès de parité tant que les divergences ci-dessus subsistent.

## Suite technique

La prochaine correction de contenu doit partir de la divergence
`actor_transform.procedure` au tick 0, puis réacquérir seulement les scénarios
Link affectés. Les trente scénarios non portés nécessitent une implémentation
réelle avant toute trace PSP. La validation de performance sur matériel PSP
reste requise.
