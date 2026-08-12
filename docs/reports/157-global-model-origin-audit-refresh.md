# Global model-origin audit refresh

The existing reproducible origin audit was rerun over the current canonical
package at commit `65ce7c0`.

Results:

- packages audited: 37;
- DPSK/DPRM models audited: 11;
- source pivots preserved: 26 packages;
- not applicable textures: 11 packages;
- pivot divergences: 0;
- forbidden vertex recentering, bounds-center pivot, minimum-Y pivot, DPSC
  floor substitution, and runtime floor-as-spawn patterns: all absent.

This refresh confirms the correction and format policies already documented in
report 125 remain intact. It authorizes the more specific room, rigid actor,
MoveBG, startup-model and UI-pivot audits; it does not by itself declare their
behavioral or visual parity.
