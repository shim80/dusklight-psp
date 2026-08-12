# Audit du champ `procedure` de Link

## Classification

`RUNTIME_SOURCE_STATE_IMPLEMENTED` — trace PSP fraîche : `MATCH`

La divergence initiale `desktop=3`, `PSP=0` ne compare pas deux champs
équivalents. Le desktop émet le véritable état de procédure de Link. Le PSP
émettait sa phase de mouvement sous le même nom. La valeur PSP ne doit donc pas
être remplacée par 3 : elle doit cesser d'être présentée comme une procédure
source tant que cet état source n'existe pas dans le runtime PSP.

## Champ desktop réel

| Propriété | Preuve |
|---|---|
| Type C/C++ | `u16` |
| Classe | `daAlink_c` |
| Champ | `daAlink_c::mProcID` |
| Enum | `daAlink_c::daAlink_PROC` |
| Symbole pour la valeur 3 | `PROC_WAIT` |
| Déclaration | `.tools/reference/dusklight-desktop/source-vanilla/include/d/actor/d_a_alink.h` |
| Écriture générique | `daAlink_c::commonProcInit(daAlink_PROC)` |
| Initialisation idle | `daAlink_c::procWaitInit()` appelle `commonProcInit(PROC_WAIT)` |
| Lecture/exécution | `(this->*mpProcFunc)()` dans `daAlink_c::execute()` |
| Émetteur instrumenté | `source-trace/src/d/actor/d_a_alink.cpp`, événement `actor_transform` |

`playerInit()` place d'abord `mProcID` à `PROC_MAX`. Dans le chemin de création
normal de D_MN10/R09, `procWaitInit()` sélectionne ensuite `PROC_WAIT` via
`commonProcInit()`. Cette fonction installe à la fois `mpProcFunc`, `mProcID` et
les flags de mode issus de `m_procInitTable`. La valeur observée après création
et au premier `execute()` est donc 3 parce que le chemin source sélectionne
`PROC_WAIT`, pas parce que 3 est une constante de présentation.

## Responsabilités du champ desktop

- cycle de vie : choisi pendant la création puis remplacé par les fonctions
  d'initialisation de procédure ;
- locomotion : `PROC_WAIT`, `PROC_MOVE`, `PROC_WAIT_TURN` et d'autres procédures
  gouvernent les transitions source ;
- animation : `procWaitInit()` appelle `setBlendMoveAnime()`, et de nombreuses
  procédures sélectionnent leurs animations dans leur fonction d'initialisation ;
- callbacks articulaires : ils consomment l'état et les animations résultants,
  mais ne constituent pas la source du champ ;
- caméra : la caméra lit l'état transformé du joueur ; elle n'écrit pas
  `mProcID` dans le chemin audité ;
- collision : plusieurs branches de correction mur/sol conditionnent leur
  comportement sur `mProcID`, mais la collision n'est pas la source du champ.

## État PSP porté après l'audit

| Propriété | Valeur |
|---|---|
| Type C++ | `dusk::psp::room::LinkProcedure` |
| Champ | `RealRoomState::link_procedure` |
| Valeur idle brute | 3 |
| Symbole | `PROC_WAIT` |
| Construction | `construct_link_procedure_state()` sélectionne `PROC_MAX` invalide |
| Création | `initialize_link_wait_procedure()` sélectionne `PROC_WAIT` |
| Mise à jour | `update_link_procedure()` choisit une procédure depuis l'état runtime |
| Émetteur | lit uniquement `RealRoomState::link_procedure` |
| Checkpoint de trace | `frame_present` |

`MotionPhase` demeure un état PSP distinct. Le sous-ensemble de procédures
nécessaire au périmètre Link existant est maintenant un état du runtime, avec
les valeurs et symboles vérifiés dans le snapshot local exact. Le traceur ne
dérive plus jamais `procedure` de `MotionPhase`.

## Chronologie vérifiée par le harness hôte

Le test `scripts/test-link-procedure-field.sh` exécute le vrai
`RealRoomRuntime` hôte avec les packages D_MN10/R09 et produit :

- `build/reports/link-procedure/psp-checkpoints.csv` ;
- `build/reports/link-procedure/procedure-timeline.csv` ;
- `build/reports/link-procedure/procedure-audit.json`.

Treize checkpoints sont enregistrés, de `actor_constructed` à
`frame_present`. Les deux premiers contiennent `PROC_MAX`, invalide. La sortie
de création et les onze checkpoints suivants contiennent `PROC_WAIT`, valide.
Cette chronologie correspond au chemin `playerInit()` → `procWaitInit()` →
`commonProcInit(PROC_WAIT)` de l'oracle.

## Correction de trace

DTRC v3.1 conserve la lecture de v3 et ajoute :

- `schema_revision=1` ;
- `lifecycle_checkpoint` ;
- `procedure_raw` ;
- `procedure_symbol` ;
- `procedure_enum_type` ;
- `procedure_source_field` ;
- `procedure_source_symbol` ;
- `procedure_trace_checkpoint` ;
- `procedure_valid` ;
- `procedure_initialization_phase`.

L'étape d'audit a d'abord écrit `procedure=null`, `procedure_valid=false` afin
de retirer le faux mapping. Après portage de l'état runtime, l'émetteur écrit la
valeur brute, le symbole, le champ source et `procedure_valid=true`. La trace
native de `link_idle_full_cycle` liée au build
`sha256:f8f14b9a818327139f061bc0a49eadd9463f08089ac996afe26d6a8e67ce356a`
confirme `PROC_WAIT=3` au tick 0.

## Tests négatifs

Le harness refuse :

1. `motion_phase` déclaré comme procédure valide ;
2. checkpoints desktop/PSP non alignés ;
3. valeur codée en dur ;
4. injection depuis `main` ;
5. injection par scénario ;
6. injection dans l'émetteur ;
7. valeur inconnue classée `MATCH`.

Il vérifie aussi par scan source qu'aucune affectation `procedure = 3` ni ancien
formatage `procedure:%u` ne subsiste dans le runtime PSP.

## Conclusion

Le faux mapping est corrigé et l'état source-compatible existe désormais dans
le runtime. Sur la nouvelle acquisition idle, la divergence `procedure` est
fermée. La première divergence causale restante est `actor_state.animation_id`
au tick 0 (`desktop=618`, `PSP=0`). Aucun marqueur final Link n'est autorisé :
la parité globale du scénario reste fausse.
