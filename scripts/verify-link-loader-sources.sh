#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR="$(CDPATH= cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd -P)"
# shellcheck source=lib/common.sh
. "$SCRIPT_DIR/lib/common.sh"

require_project_root

DUSKLIGHT_COMMIT="1bae8a5e6a812217ca33ba533e707ecfa64b1553"
DUSKLIGHT_TREE="dbd0de5c47808b5a78daa24c391166936bd6acc5"
AURORA_COMMIT="${DUSKLIGHT_TEST_AURORA_COMMIT:-81f12f31d23ec822d8bde2031c91e94c470911eb}"
AURORA_TREE="5330068f39c624dc85c20533eb0c13a40cca5570"
NOD_COMMIT="${DUSKLIGHT_TEST_NOD_COMMIT:-dc18d2ff129f05228b8510ea092d8b24c290a49a}"
NOD_TREE="96204981e7a5916f8cff2d1c730b4f1bde7aa922"
VENDOR_SHA256="e482b6733351add58e711af9880a3ccd066a0eaaac17bf7ffb07306a6925a995"
VENDOR_SIZE="13317589"

PROVENANCE_DIR="$PROJECT_ROOT/.cache/provenance"
SOURCE_DIR="$PROJECT_ROOT/.tools/sources"
VENDOR_ARCHIVE="$PROJECT_ROOT/.cache/downloads/vendored-crates-v2.0.0-alpha.10.tar.zst"
VENDOR_DIR="$SOURCE_DIR/nod/vendor/$NOD_COMMIT"

verify_object() {
  local mirror="$1" commit="$2" tree="$3"
  git --git-dir="$mirror" cat-file -e "$commit^{commit}" ||
    die "commit absent : $commit"
  [ "$(git --git-dir="$mirror" show -s --format=%T "$commit")" = "$tree" ] ||
    die "tree inattendu pour $commit"
}

verify_checkout() {
  local checkout="$1" commit="$2"
  [ "$(git -C "$checkout" rev-parse HEAD)" = "$commit" ] ||
    die "checkout incorrect : $checkout"
  [ -z "$(git -C "$checkout" status --short)" ] ||
    die "checkout externe modifié : $checkout"
}

verify_object "$PROVENANCE_DIR/dusklight.git" "$DUSKLIGHT_COMMIT" "$DUSKLIGHT_TREE"
verify_object "$PROVENANCE_DIR/aurora.git" "$AURORA_COMMIT" "$AURORA_TREE"
verify_object "$PROVENANCE_DIR/nod.git" "$NOD_COMMIT" "$NOD_TREE"
verify_checkout "$SOURCE_DIR/aurora/$AURORA_COMMIT" "$AURORA_COMMIT"
verify_checkout "$SOURCE_DIR/nod/$NOD_COMMIT" "$NOD_COMMIT"

[ "$(wc -c < "$VENDOR_ARCHIVE" | tr -d ' ')" = "$VENDOR_SIZE" ] ||
  die "taille inattendue pour le bundle de crates"
[ "$(sha256_file "$VENDOR_ARCHIVE")" = "$VENDOR_SHA256" ] ||
  die "SHA-256 inattendu pour le bundle de crates"
[ "$(cat "$VENDOR_DIR/.verified-source")" = "$VENDOR_SHA256" ] ||
  die "bundle de crates non marqué comme vérifié"

printf '%s\n' \
  "LINK_LOADER_SOURCES_OK" \
  "dusklight_commit=$DUSKLIGHT_COMMIT" \
  "aurora_commit=$AURORA_COMMIT" \
  "nod_commit=$NOD_COMMIT"
