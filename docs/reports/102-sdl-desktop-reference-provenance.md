# Provenance — SDL desktop reference

## Source

- Archive officielle :
  https://github.com/libsdl-org/SDL/releases/download/release-3.2.22/SDL3-3.2.22.tar.gz
- Version : `3.2.22`.
- SHA-256 obtenu :
  `b52ca20aafcfc134ec2e14d23d2e7f990192ad06ea172af8c16f014f2f7b58a2`.
- Extraction locale : `.cache/deps/source/SDL3-3.2.22`.

Le runtime desktop Linux avait besoin de `libSDL3.so.0`; la copie locale de la
bibliothèque est conservée dans `.cache/deps/host/lib` pour l'exécution et pour
les outils de conversion host-side qui lient `libnod`.
