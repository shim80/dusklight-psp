# Object pivot fixes

## Corrections appliquées

| objet | cause | ancienne transformation | transformation source | nouvelle transformation | tests |
|---|---|---|---|---|---|
| spawn `D_MN10/R09` | le header DPSC substituait le sol à l’origine source | `(0, 450.000031, -4650)` | `(0, 462.5, -4650)` | position source conservée; sol séparé | audit DPSC, runtime hôte, CRC package |
| spawn `F_SP108/R01/start21` | même substitution générique | `(-16922.418, 3.689293, -4467.296)` | `(-16922.418, 3.686285, -4467.296)` | position source conservée; sol séparé | audit DPSC, runtime hôte, CRC package |

Ce correctif concerne les origines de scène et non les vertices des modèles.
Il a été appliqué dans le convertisseur DPSC et dans le runtime de spawn, puis
les packages affectés ont été régénérés.

## Objets rigides

Aucun offset de pivot spécifique n’a été ajouté ou retiré pour les sept
modèles rigides actuels. Leurs vertices locaux étaient déjà conformes à la
politique source.

```text
model_pivots_corrected=0
scene_spawn_pivots_corrected=2
unproven_actor_specific_offsets=0
desktop_psp_object_trace_comparison=pending
```
