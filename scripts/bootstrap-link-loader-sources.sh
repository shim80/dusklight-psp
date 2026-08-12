#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR="$(CDPATH= cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd -P)"
# shellcheck source=lib/common.sh
. "$SCRIPT_DIR/lib/common.sh"

require_project_root

DUSKLIGHT_URL="https://github.com/TwilitRealm/dusklight.git"
DUSKLIGHT_COMMIT="1bae8a5e6a812217ca33ba533e707ecfa64b1553"
AURORA_URL="https://github.com/encounter/aurora.git"
AURORA_COMMIT="81f12f31d23ec822d8bde2031c91e94c470911eb"
NOD_URL="https://github.com/encounter/nod.git"
NOD_COMMIT="dc18d2ff129f05228b8510ea092d8b24c290a49a"
VENDOR_URL="https://github.com/encounter/nod/releases/download/v2.0.0-alpha.10/vendored-crates-v2.0.0-alpha.10.tar.zst"
VENDOR_SHA256="e482b6733351add58e711af9880a3ccd066a0eaaac17bf7ffb07306a6925a995"
VENDOR_SIZE="13317589"

PROVENANCE_DIR="$(assert_project_path ".cache/provenance")"
DOWNLOAD_DIR="$(assert_project_path ".cache/downloads")"
SOURCE_DIR="$(assert_project_path ".tools/sources")"
VENDOR_ARCHIVE="$DOWNLOAD_DIR/vendored-crates-v2.0.0-alpha.10.tar.zst"
VENDOR_DIR="$SOURCE_DIR/nod/vendor/$NOD_COMMIT"

safe_mkdir "$PROVENANCE_DIR"
safe_mkdir "$DOWNLOAD_DIR"
safe_mkdir "$SOURCE_DIR/aurora"
safe_mkdir "$SOURCE_DIR/nod"
safe_mkdir "$PROJECT_ROOT/.tmp"

ensure_mirror() {
  local url="$1" mirror="$2" commit="$3"
  if [ ! -d "$mirror" ]; then
    git clone --mirror "$url" "$mirror"
  fi
  git --git-dir="$mirror" remote get-url origin |
    grep -Fx "$url" >/dev/null ||
    die "URL inattendue pour $mirror"
  if ! git --git-dir="$mirror" cat-file -e "$commit^{commit}" 2>/dev/null; then
    git --git-dir="$mirror" fetch --prune --tags origin
  fi
  git --git-dir="$mirror" cat-file -e "$commit^{commit}" ||
    die "commit absent après récupération : $commit"
}

ensure_checkout() {
  local mirror="$1" checkout="$2" commit="$3"
  if [ -e "$checkout/.git" ]; then
    [ "$(git -C "$checkout" rev-parse HEAD)" = "$commit" ] ||
      die "checkout présent à une autre révision : $checkout"
    [ -z "$(git -C "$checkout" status --short)" ] ||
      die "checkout externe modifié : $checkout"
    return
  fi
  [ ! -e "$checkout" ] ||
    die "destination non vide sans checkout Git : $checkout"
  git --git-dir="$mirror" worktree add --detach "$checkout" "$commit"
}

ensure_mirror "$DUSKLIGHT_URL" "$PROVENANCE_DIR/dusklight.git" "$DUSKLIGHT_COMMIT"
ensure_mirror "$AURORA_URL" "$PROVENANCE_DIR/aurora.git" "$AURORA_COMMIT"
ensure_mirror "$NOD_URL" "$PROVENANCE_DIR/nod.git" "$NOD_COMMIT"

ensure_checkout \
  "$PROVENANCE_DIR/aurora.git" \
  "$SOURCE_DIR/aurora/$AURORA_COMMIT" \
  "$AURORA_COMMIT"
ensure_checkout \
  "$PROVENANCE_DIR/nod.git" \
  "$SOURCE_DIR/nod/$NOD_COMMIT" \
  "$NOD_COMMIT"

if [ -f "$VENDOR_ARCHIVE" ]; then
  [ "$(wc -c < "$VENDOR_ARCHIVE" | tr -d ' ')" = "$VENDOR_SIZE" ] ||
    die "taille inattendue pour $VENDOR_ARCHIVE"
  [ "$(sha256_file "$VENDOR_ARCHIVE")" = "$VENDOR_SHA256" ] ||
    die "SHA-256 inattendu pour $VENDOR_ARCHIVE"
else
  command -v curl >/dev/null 2>&1 || die "curl est requis"
  curl --fail --location --proto '=https' --tlsv1.2 \
    --output "$VENDOR_ARCHIVE.part" "$VENDOR_URL"
  [ "$(wc -c < "$VENDOR_ARCHIVE.part" | tr -d ' ')" = "$VENDOR_SIZE" ] ||
    die "taille téléchargée inattendue"
  [ "$(sha256_file "$VENDOR_ARCHIVE.part")" = "$VENDOR_SHA256" ] ||
    die "SHA-256 téléchargé inattendu"
  mv -- "$VENDOR_ARCHIVE.part" "$VENDOR_ARCHIVE"
fi

if [ ! -f "$VENDOR_DIR/.verified-source" ]; then
  command -v zstd >/dev/null 2>&1 || die "zstd est requis pour le bundle Nod"
  temp_dir="$PROJECT_ROOT/.tmp/nod-vendor-$NOD_COMMIT"
  [ ! -e "$temp_dir" ] || die "répertoire temporaire déjà présent : $temp_dir"
  safe_mkdir "$temp_dir"
  zstd --decompress --stdout "$VENDOR_ARCHIVE" | tar -xf - -C "$temp_dir"
  safe_mkdir "$(dirname -- "$VENDOR_DIR")"
  mv -- "$temp_dir" "$VENDOR_DIR"
  printf '%s\n' "$VENDOR_SHA256" > "$VENDOR_DIR/.verified-source"
fi

"$SCRIPT_DIR/verify-link-loader-sources.sh"

