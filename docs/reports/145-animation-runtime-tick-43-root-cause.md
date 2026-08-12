# Fermeture causale de `ANIMATION_RUNTIME`

## Résultat

`ANIMATION_RUNTIME_CLOSED`

La divergence initiale de `link_idle_full_cycle` au tick 43 et la divergence
réouverte par `link_turn_180` aux ticks 30–31 sont fermées sans modifier la
référence desktop et sans coder en dur un tick ou une valeur de l’oracle.

## Cause du tick 43

Le contrôleur J3D desktop est observé après une avance effectuée pendant la
création de l’acteur puis après l’avance de `execute()`. Le sampler PSP partait
directement de sa première mise à jour. La correction `0f99b6c` expose le
checkpoint source depuis l’état réel du contrôleur et boucle selon la période
lue dans la table DPAN active. Les dix scénarios Link ont ensuite confirmé
qu’aucune première divergence ne restait dans cette couche.

## Cause du demi-tour

Le scénario `link_turn_180` a révélé une seconde frontière honnête : le paquet
DPAN ne contenait pas la ressource source 563, déclarée par
`daAlink_c::ANM_STEP_TURN` et résolue en `stepl.bck` (`0x233`). Le commit
`7629fbe` exporte cette ressource réelle et la relie à l’état runtime
`TurnInPlace`. Le commit `5fffb95` reproduit ensuite l’ordre de
`procWaitTurnInit()` : frame 0 au tick de sélection, puis avance 0.7 à la mise
à jour suivante.

Le DPAN passe de 134688 à 164144 octets. Le buffer PSP historique de 160000
octets a donc refusé le paquet avant l’initialisation de la scène. Le commit
`a4e18fd` porte uniquement cette capacité à 192 KiB. Le signal de boot était
présent et aucune erreur PPSSPP n’était signalée ; après correction, la même
trace termine en 14.2 secondes avec marqueur et métriques valides.

## Preuves

- ressource desktop : `ANM_STEP_TURN`, archive ID `0x233`, `stepl.bck` ;
- DPAN : quatre clips déterministes, ID final 563 ;
- checkpoints hôte : frame 0 lors de la sélection, frame 0.7 ensuite ;
- compilation : hôte et Allegrex ;
- build causal :
  `sha256:32aa35132e31db2d404eab51af9828ea43ad05968598c9ced8ba37a618597c15` ;
- acquisition : `20260731T154956Z-parity_trace-1`, marqueurs et métriques
  valides, broker génération 2 ;
- causal : 7289 → 7280 divergences, nouvelle frontière `GROUNDING` tick 31.
- réacquisition finale : 10/10 traces natives et 10/10 résumés causaux
  valides sous le même build ; aucune première divergence dans
  `ANIMATION_RUNTIME`.

## Invariants

- référence desktop inchangée ;
- aucun accès réseau ;
- aucun tick 43 ou tick 30 dans le correctif runtime ;
- aucun type Dusklight artificiel ;
- animation issue du snapshot local légalement fourni ;
- profil PPSSPP isolé et transport LaunchAgent canonique.
