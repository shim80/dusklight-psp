# Build et boot de la référence desktop vanilla

## Classification

`READY_DUSKLIGHT_DESKTOP_VANILLA_BOOT`

Le build desktop vanilla est produit sous :

`reference/dusklight-desktop-vanilla/build/dusklight`

SHA-256 :

`f74a566478574387e72fa12f9befb452249ea38a13c0d6bd0aa6e4eeaeb6cfaa`

Le boot sandboxé avec `GAME_IMAGE=...` :

- construit les deux pipelines Vulkan ;
- charge `F_SP108` ;
- annonce `Loaded F_SP108` ;
- passe le `Vulkan submission #3` ;
- reste vivant jusqu'au timeout borné de 20 secondes ;
- n'écrit aucun fichier hors des racines autorisées.

Le transport visuel est bloqué par l'absence de display X11/Wayland dans le
sandbox Codex ; `Xvfb` et X11VNC ne sont pas installés. La trace runtime reste
utilisable comme oracle de comportement source sans screenshot.
