# Rapport 110 — Cause racine de l’erreur jouable 175

`175` est exactement `170 + PackageError::Size`, au site
`load_packages()` de `test/link-playable/main.cpp`, pendant
`validate_dpui`.

Le DPUI v2 légal mesure 132 192 octets et porte le SHA-256
`4e3fd2da93150ecdee0cf10c50067486764c6faf580a8d0a412462ac7137a795`.
Le tampon historique `g_dpui` ne réservait que 24 000 octets. La lecture
réussissait avec 24 000 octets, puis le champ de taille déclaré par le package
ne correspondait pas à la taille lue.

La correction porte le tampon à 160 000 octets, valeur déjà utilisée par le
runtime canonique. La numérotation n’est pas remappée. Les métriques exposent
désormais `error_name`, `error_subsystem` et `error_detail`.

Le run GUI `20260731T-link-playable-after-175-fix-valid-mode` réutilise les
mêmes quatre packages : il boote, progresse jusqu’à Run, rend Link et le HUD,
écrit `PLAYABLE.OK`, sort proprement et termine avec `error_code=0`.

