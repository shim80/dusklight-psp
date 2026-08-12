#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR="$(CDPATH= cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd -P)"
# shellcheck source=lib/common.sh
. "$SCRIPT_DIR/lib/common.sh"

with_trace=false
while [ "$#" -gt 0 ]; do
  case "$1" in
    --with-trace)
      with_trace=true
      shift
      ;;
    *)
      die "argument inconnu : $1"
      ;;
  esac
done

LOCK="$(assert_project_path "reference/desktop/reference-source.lock")"
AUTHORITY="$(assert_project_path "toolchain/loader-sources.lock")"
ROOT="$(assert_project_path ".tools/reference/dusklight-desktop")"
VANILLA="$ROOT/source-vanilla"
TRACE="$ROOT/source-trace"
WORKTREES="$ROOT/worktrees"
NOD="$WORKTREES/nod-dc18d2ff129f05228b8510ea092d8b24c290a49a"
MANIFESTS="$ROOT/manifests"
DUSKLIGHT_MIRROR="$(assert_project_path ".cache/provenance/dusklight.git")"
AURORA_MIRROR="$(assert_project_path ".cache/provenance/aurora.git")"
NOD_MIRROR="$(assert_project_path ".cache/provenance/nod.git")"
DUSKLIGHT_COMMIT=1bae8a5e6a812217ca33ba533e707ecfa64b1553
DUSKLIGHT_TREE=dbd0de5c47808b5a78daa24c391166936bd6acc5
AURORA_COMMIT=81f12f31d23ec822d8bde2031c91e94c470911eb
AURORA_TREE=5330068f39c624dc85c20533eb0c13a40cca5570
NOD_COMMIT=dc18d2ff129f05228b8510ea092d8b24c290a49a
NOD_TREE=96204981e7a5916f8cff2d1c730b4f1bde7aa922

[ -f "$LOCK" ] && [ -f "$AUTHORITY" ] ||
  die "verrou de provenance absent"
for exact in \
  "commit = \"$DUSKLIGHT_COMMIT\"" \
  "tree = \"$DUSKLIGHT_TREE\"" \
  "commit = \"$AURORA_COMMIT\"" \
  "tree = \"$AURORA_TREE\"" \
  "commit = \"$NOD_COMMIT\"" \
  "tree = \"$NOD_TREE\""; do
  grep -Fq "$exact" "$AUTHORITY" ||
    die "valeur absente du manifeste autoritaire : $exact"
done
grep -Fq 'https://github.com/TwilitRealm/dusklight.git' "$AUTHORITY" &&
grep -Fq 'https://github.com/encounter/aurora.git' "$AUTHORITY" &&
grep -Fq 'https://github.com/encounter/nod.git' "$AUTHORITY" ||
  die "URL épinglée absente"

verify_object() {
  local mirror="$1" commit="$2" tree="$3" name="$4"
  [ -d "$mirror" ] || die "mirror local absent : $name"
  [ "$(git --git-dir="$mirror" rev-parse "$commit^{commit}")" = "$commit" ] ||
    die "commit local invalide : $name"
  [ "$(git --git-dir="$mirror" rev-parse "$commit^{tree}")" = "$tree" ] ||
    die "tree local invalide : $name"
}
verify_object "$DUSKLIGHT_MIRROR" "$DUSKLIGHT_COMMIT" \
  "$DUSKLIGHT_TREE" dusklight
verify_object "$AURORA_MIRROR" "$AURORA_COMMIT" "$AURORA_TREE" aurora
verify_object "$NOD_MIRROR" "$NOD_COMMIT" "$NOD_TREE" nod

gitlinks="$(git --git-dir="$DUSKLIGHT_MIRROR" ls-tree -r \
  "$DUSKLIGHT_COMMIT" | awk '$1 == "160000" {print $3 "|" $4}')"
[ "$gitlinks" = "$AURORA_COMMIT|extern/aurora" ] ||
  die "ensemble de gitlinks Dusklight inattendu"
git --git-dir="$DUSKLIGHT_MIRROR" show \
  "$DUSKLIGHT_COMMIT:.gitmodules" |
  grep -Fq 'url = https://github.com/encounter/aurora.git' ||
  die "URL gitlink Aurora invalide"

safe_mkdir ".tools/reference/dusklight-desktop"
safe_mkdir ".tools/reference/dusklight-desktop/mirrors"
safe_mkdir ".tools/reference/dusklight-desktop/worktrees"
safe_mkdir ".tools/reference/dusklight-desktop/manifests"

materialize_worktree() {
  local mirror="$1" target="$2" commit="$3" label="$4"
  if [ -d "$target" ] && [ ! -e "$target/.git" ]; then
    [ -z "$(find "$target" -mindepth 1 -maxdepth 1 -print -quit)" ] ||
      die "$label existe sans métadonnées Git et n'est pas vide"
    rmdir -- "$target"
  fi
  if [ -e "$target" ]; then
    [ -d "$target" ] || die "$label existe mais n'est pas un répertoire"
    [ "$(git -C "$target" rev-parse HEAD 2>/dev/null)" = "$commit" ] ||
      die "$label existe à une révision différente"
  else
    git --git-dir="$mirror" worktree add --detach "$target" "$commit"
  fi
}

materialize_worktree \
  "$DUSKLIGHT_MIRROR" "$VANILLA" "$DUSKLIGHT_COMMIT" source-vanilla
materialize_worktree \
  "$AURORA_MIRROR" "$VANILLA/extern/aurora" \
  "$AURORA_COMMIT" source-vanilla-aurora
if [ "$with_trace" = true ]; then
  materialize_worktree \
    "$DUSKLIGHT_MIRROR" "$TRACE" "$DUSKLIGHT_COMMIT" source-trace
  materialize_worktree \
    "$AURORA_MIRROR" "$TRACE/extern/aurora" \
    "$AURORA_COMMIT" source-trace-aurora
fi
materialize_worktree \
  "$NOD_MIRROR" "$NOD" "$NOD_COMMIT" nod

[ "$(git -C "$VANILLA" rev-parse HEAD^{tree})" = "$DUSKLIGHT_TREE" ] ||
  die "tree vanilla différent"
[ "$(git -C "$VANILLA/extern/aurora" rev-parse HEAD^{tree})" = \
    "$AURORA_TREE" ] || die "tree Aurora matérialisé différent"
[ "$(git -C "$NOD" rev-parse HEAD^{tree})" = "$NOD_TREE" ] ||
  die "tree Nod matérialisé différent"
if [ "$with_trace" = true ]; then
  [ "$(git -C "$TRACE" rev-parse HEAD^{tree})" = "$DUSKLIGHT_TREE" ] ||
    die "tree trace initial différent"
  [ "$(git -C "$TRACE/extern/aurora" rev-parse HEAD^{tree})" = \
    "$AURORA_TREE" ] || die "tree Aurora trace initial différent"
fi
[ -f "$VANILLA/LICENSE.md" ] &&
[ -f "$VANILLA/extern/aurora/LICENSE" ] &&
{ [ -f "$NOD/LICENSE-APACHE" ] || [ -f "$NOD/LICENSE-APACHE.md" ]; } ||
  die "preuve de licence locale absente"

vanilla_status="$(git -C "$VANILLA" status --porcelain \
  --untracked-files=all --ignore-submodules=none)"
[ -z "$vanilla_status" ] || die "checkout vanilla sale"
if [ "$with_trace" = true ]; then
  trace_status="$(git -C "$TRACE" status --porcelain \
    --untracked-files=all --ignore-submodules=none)"
  [ -z "$trace_status" ] || die "checkout trace initial sale"
fi

manifest="$MANIFESTS/BOOTSTRAP.MANIFEST"
{
  printf 'format=DUSKLIGHT_DESKTOP_REFERENCE_BOOTSTRAP_V1\n'
  printf 'dusklight_commit=%s\n' "$DUSKLIGHT_COMMIT"
  printf 'dusklight_tree=%s\n' "$DUSKLIGHT_TREE"
  printf 'aurora_commit=%s\n' "$AURORA_COMMIT"
  printf 'aurora_tree=%s\n' "$AURORA_TREE"
  printf 'nod_commit=%s\n' "$NOD_COMMIT"
  printf 'nod_tree=%s\n' "$NOD_TREE"
  printf 'gitlink_count=1\n'
  printf 'gitlink_0=extern/aurora:%s\n' "$AURORA_COMMIT"
  printf 'vanilla_source_modified_files=0\n'
  printf 'vanilla_source_untracked_files=0\n'
  printf 'vanilla_tree_matches_expected=true\n'
  printf 'network_access_performed=false\n'
  printf 'source_trace_materialized=%s\n' \
    "$([ -d "$TRACE" ] && echo true || echo false)"
} >"$manifest"
printf 'DUSKLIGHT_DESKTOP_REFERENCE_BOOTSTRAP_OK vanilla=%s nod=%s\n' \
  "${VANILLA#"$PROJECT_ROOT"/}" "${NOD#"$PROJECT_ROOT"/}"
