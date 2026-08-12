# Réconciliation du build de parité

## Résultat

Le build PSP autoritaire est reconstruit une seule fois depuis l'état de
staging `0dae447ce65c2b0861d8dbab2dc6c571745b10ab`.

```text
commit=0dae447ce65c2b0861d8dbab2dc6c571745b10ab
elf_sha256=8a88a4e32daf77783a0b6d3d1ef9e76720edfba9d30ec34607503894f389b4b7
eboot_sha256=d2d00406b491d3229613ca883def49bc9bc36b781fc10c68a9bd48c7e7e7ba52
resource_manifest_sha256=52de50bdace19a3b282a236f2731e1210987188c480a968d400922b8bfc0e68a
trace_schema_version=DTRC_V3
parity_contract_version=DUSKLIGHT_DESKTOP_PSP_PARITY_CONTRACT_V1
```

Le `PARITY_BUILD_ID` est le SHA-256 de la sérialisation canonique des six
champs demandés : commit, EBOOT, manifest, ensemble de packages, schéma de
trace et contrat de parité. Le fichier généré ajoute les empreintes ELF et des
deux configurations PPSSPP autorisées.

## Explication de la divergence précédente

- `6bca91b8…` était l'EBOOT de travail relinké après les corrections Link ;
- `8525c36b…` était l'EBOOT plus ancien resté dans le paquet canonique ;
- le cache CMake conservait encore `d67459b9…` comme macro de provenance.

Ces trois identités ne pouvaient pas soutenir une acquisition commune. Le
paquet canonique contient maintenant exactement l'EBOOT `d2d00406…` et le
même `PARITY.BUILD` que le répertoire de rapports.

Les caches de tests restent invalidés naturellement par les nouvelles
empreintes d'entrée. Aucune trace, capture ou mesure PSP antérieure n'est
reclassée comme courante.
