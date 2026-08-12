#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR="$(CDPATH= cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd -P)"
# shellcheck source=lib/common.sh
. "$SCRIPT_DIR/lib/common.sh"

MODE=plan
OFFLINE=0
ORIGINAL_ARGS=("$@")

usage() {
  cat <<'EOF'
Usage: bootstrap-tools.sh [--plan|--download-only|--install|--verify] [--offline]

  --plan           Affiche les opérations; aucun accès réseau (mode par défaut).
  --download-only  Télécharge atomiquement les archives officielles et vérifie SHA-256.
  --install        Installe uniquement depuis le cache déjà vérifié; aucun réseau.
  --verify         Vérifie cache, marqueurs d'installation et fichiers attendus sans les exécuter.
  --offline        Interdit le réseau; --download-only échoue si une archive manque.
EOF
}

while [ "$#" -gt 0 ]; do
  case "$1" in
    --plan) MODE=plan ;;
    --download-only) MODE=download ;;
    --install) MODE=install ;;
    --verify) MODE=verify ;;
    --offline) OFFLINE=1 ;;
    -h|--help) usage; exit 0 ;;
    *) die "option inconnue : $1" ;;
  esac
  shift
done

if [ -z "${BOOTSTRAP_LOG_FILE:-}" ]; then
  safe_mkdir logs/bootstrap
  LOG_FILE="$(assert_project_path "logs/bootstrap/$(timestamp_utc)-${MODE}.log")"
  set +e
  BOOTSTRAP_LOG_FILE="$LOG_FILE" "$0" "${ORIGINAL_ARGS[@]}" 2>&1 | tee -a "$LOG_FILE"
  child_status=${PIPESTATUS[0]}
  exit "$child_status"
fi
LOG_FILE="$(assert_project_path "$BOOTSTRAP_LOG_FILE")"

PLATFORM="$(host_platform)"
case "$PLATFORM" in
  macos-arm64) TOOL_IDS=(pspdev-macos-arm64 ppsspp-macos) ;;
  macos-x86_64) TOOL_IDS=(pspdev-macos-x86_64 ppsspp-macos) ;;
  linux-x86_64) TOOL_IDS=(pspdev-ubuntu-x86_64 ppsspp-linux-x86_64) ;;
  linux-arm64) TOOL_IDS=(pspdev-ubuntu-arm64 ppsspp-linux-arm64) ;;
  wsl-ubuntu-x86_64) TOOL_IDS=(pspdev-ubuntu-x86_64) ;;
  *) die "plateforme non prise en charge par le manifeste : $PLATFORM" ;;
esac

printf 'Mode       : %s\n' "$MODE"
printf 'Hors ligne: %s\n' "$OFFLINE"
printf 'Plateforme : %s\n' "$PLATFORM"
printf 'Racine     : %s\n' "$PROJECT_ROOT"
printf 'Manifeste  : %s\n\n' "${MANIFEST_PATH#"$PROJECT_ROOT"/}"

if [[ "$PROJECT_ROOT" =~ [^A-Za-z0-9_./-] ]]; then
  printf '%s\n' "AVERTISSEMENT : PSPDEV amont refuse les chemins avec espaces ou caractères spéciaux."
  [ "$MODE" != install ] || die "installation PSPDEV refusée dans cette racine non compatible avec l'amont"
fi

tool_field() {
  local value
  value="$(toml_tool_value "$1" "$2")"
  [ -n "$value" ] || die "champ '$2' absent pour '$1'"
  printf '%s\n' "$value"
}

verify_archive() {
  local id="$1" archive sha actual
  archive="$(assert_project_path ".cache/downloads/$(tool_field "$id" archive)")"
  sha="$(tool_field "$id" sha256)"
  [ "$sha" != pending ] || die "$id : SHA-256 pending; opération bloquée"
  [ -f "$archive" ] || return 2
  actual="$(sha256_file "$archive")"
  [ "$actual" = "$sha" ] || die "$id : SHA-256 invalide pour ${archive#"$PROJECT_ROOT"/}"
  printf '%s\n' "$archive"
}

plan_tool() {
  local id="$1"
  printf '[%s]\n' "$id"
  printf '  statut       : %s\n' "$(tool_field "$id" status)"
  printf '  version/tag  : %s / %s\n' "$(tool_field "$id" version)" "$(tool_field "$id" tag)"
  printf '  archive      : %s (~%s Mo)\n' "$(tool_field "$id" archive)" "$(tool_field "$id" size_approx_mb)"
  printf '  installation : %s\n' "$(tool_field "$id" install_dir)"
  printf '  source       : %s\n\n' "$(tool_field "$id" source_url)"
}

download_tool() {
  local id="$1" archive url sha partial actual
  safe_mkdir .cache/downloads
  archive="$(assert_project_path ".cache/downloads/$(tool_field "$id" archive)")"
  url="$(tool_field "$id" download_url)"
  sha="$(tool_field "$id" sha256)"
  [ "$sha" != pending ] || die "$id : téléchargement interdit tant que SHA-256 est pending"

  if [ -f "$archive" ]; then
    verify_archive "$id" >/dev/null
    printf '[OK] %s déjà présent et vérifié.\n' "$id"
    return
  fi
  partial="$(assert_project_path ".cache/downloads/.$(tool_field "$id" archive).part")"
  if [ -f "$partial" ]; then
    actual="$(sha256_file "$partial")"
    if [ "$actual" = "$sha" ]; then
      mv -- "$partial" "$archive"
      printf '[OK] %s récupéré depuis un téléchargement interrompu déjà complet.\n' "$id"
      return
    fi
    rm -f -- "$partial"
  fi
  [ "$OFFLINE" -eq 0 ] || die "$id : archive absente en mode --offline"
  printf '[RÉSEAU] %s\n' "$url"
  if command -v curl >/dev/null 2>&1; then
    curl --fail --location --proto '=https' --tlsv1.2 --output "$partial" "$url"
  elif command -v wget >/dev/null 2>&1; then
    wget --https-only --output-document="$partial" "$url"
  else
    die "curl ou wget est requis pour --download-only"
  fi
  actual="$(sha256_file "$partial")"
  if [ "$actual" != "$sha" ]; then
    rm -f -- "$partial"
    die "$id : SHA-256 téléchargé invalide"
  fi
  mv -- "$partial" "$archive"
  printf '[OK] %s téléchargé et vérifié.\n' "$id"
}

validate_archive_paths() {
  local archive="$1" format="$2" destination="$3"
  command -v python3 >/dev/null 2>&1 || die "Python 3 est requis pour valider les chemins d'archive"
  python3 - "$archive" "$format" "$destination" <<'PY'
import pathlib
import stat
import sys
import tarfile
import zipfile

archive, kind, destination = sys.argv[1:]
root = pathlib.Path(destination).resolve()

def checked(name, link_target=None, link_from_root=False):
    pure = pathlib.PurePosixPath(name)
    if pure.is_absolute() or ".." in pure.parts:
        raise SystemExit(f"archive path rejected: {name}")
    target = (root / pathlib.Path(*pure.parts)).resolve(strict=False)
    try:
        target.relative_to(root)
    except ValueError as exc:
        raise SystemExit(f"archive path escapes destination: {name}") from exc
    if link_target:
        link_base = root if link_from_root else target.parent
        resolved = (link_base / link_target).resolve(strict=False)
        try:
            resolved.relative_to(root)
        except ValueError as exc:
            raise SystemExit(f"archive link escapes destination: {name} -> {link_target}") from exc

if kind == "tar.gz":
    with tarfile.open(archive, "r:gz") as bundle:
        for member in bundle.getmembers():
            if not (member.isfile() or member.isdir() or member.issym() or member.islnk()):
                raise SystemExit(f"unsupported tar member type: {member.name}")
            checked(
                member.name,
                member.linkname if member.issym() or member.islnk() else None,
                link_from_root=member.islnk(),
            )
elif kind == "zip":
    with zipfile.ZipFile(archive) as bundle:
        for member in bundle.infolist():
            mode = member.external_attr >> 16
            file_type = stat.S_IFMT(mode)
            if file_type not in (0, stat.S_IFREG, stat.S_IFDIR, stat.S_IFLNK):
                raise SystemExit(f"unsupported zip member type: {member.filename}")
            link = bundle.read(member).decode("utf-8") if file_type == stat.S_IFLNK else None
            checked(member.filename, link)
elif kind != "file":
    raise SystemExit(f"unsupported archive format: {kind}")
PY
}

validate_extracted_tree() {
  local destination="$1"
  python3 - "$destination" <<'PY'
import os
import pathlib
import sys

root = pathlib.Path(sys.argv[1]).resolve()
for directory, names, files in os.walk(root, followlinks=False):
    for name in names + files:
        path = pathlib.Path(directory, name)
        if path.is_symlink():
            resolved = path.resolve(strict=False)
            try:
                resolved.relative_to(root)
            except ValueError as exc:
                raise SystemExit(f"extracted link escapes destination: {path} -> {resolved}") from exc
PY
}

install_tool() {
  local id="$1" archive format archive_root install_rel install_dir version installed_name
  local stage extract payload marker marker_tmp
  archive="$(verify_archive "$id")" || die "$id : archive absente; exécuter d'abord --download-only"
  format="$(tool_field "$id" archive_format)"
  archive_root="$(tool_field "$id" archive_root)"
  install_rel="$(tool_field "$id" install_dir)"
  install_dir="$(assert_project_path "$install_rel")"
  version="$(tool_field "$id" version)"
  marker="$install_dir/.installed-manifest"

  if [ -f "$marker" ] &&
     grep -Fqx "id=$id" "$marker" &&
     grep -Fqx "version=$version" "$marker" &&
     grep -Fqx "sha256=$(tool_field "$id" sha256)" "$marker"; then
    printf '[OK] %s déjà installé.\n' "$id"
    return
  fi
  [ ! -e "$install_dir" ] || die "$id : destination existante non reconnue, refus d'écraser $install_rel"

  safe_mkdir .tmp
  stage="$(assert_project_path ".tmp/install-${id}-$$")"
  [ ! -e "$stage" ] || die "répertoire temporaire déjà présent : $stage"
  mkdir -p -- "$stage/extract"
  extract="$stage/extract"
  validate_archive_paths "$archive" "$format" "$extract"

  case "$format" in
    tar.gz) tar -xzf "$archive" -C "$extract" ;;
    zip) unzip -q "$archive" -d "$extract" ;;
    file)
      installed_name="$(tool_field "$id" installed_filename)"
      [ -n "$installed_name" ] || die "$id : installed_filename manquant"
      mkdir -p -- "$stage/payload"
      cp -- "$archive" "$stage/payload/$installed_name"
      chmod 0755 "$stage/payload/$installed_name"
      ;;
    *) die "$id : format d'archive non pris en charge : $format" ;;
  esac

  if [ "$format" = file ]; then
    payload="$stage/payload"
  elif [ "$archive_root" = . ]; then
    payload="$extract"
  else
    payload="$(assert_project_path "${extract#"$PROJECT_ROOT"/}/$archive_root")"
  fi
  [ -d "$payload" ] || die "$id : racine d'archive attendue absente ($archive_root)"
  validate_extracted_tree "$extract"

  marker_tmp="$payload/.installed-manifest.tmp"
  printf 'id=%s\nversion=%s\nsha256=%s\n' "$id" "$version" "$(tool_field "$id" sha256)" > "$marker_tmp"
  mv -- "$marker_tmp" "$payload/.installed-manifest"
  safe_mkdir "$(dirname -- "$install_rel")"
  mv -- "$payload" "$install_dir"
  rm -rf -- "$stage"
  printf '[OK] %s installé sous %s sans exécuter ses binaires.\n' "$id" "$install_rel"
}

verify_tool() {
  local id="$1" install_dir marker archive_state archive_status version sha
  if archive_state="$(verify_archive "$id" 2>/dev/null)"; then
    printf '[OK] cache %-28s %s\n' "$id" "${archive_state#"$PROJECT_ROOT"/}"
  else
    archive_status=$?
    if [ "$archive_status" -eq 2 ]; then
      printf '[ABSENT] cache %-24s %s\n' "$id" "$(tool_field "$id" archive)"
    else
      die "$id : archive présente mais invalide"
    fi
  fi
  install_dir="$(assert_project_path "$(tool_field "$id" install_dir)")"
  marker="$install_dir/.installed-manifest"
  version="$(tool_field "$id" version)"
  sha="$(tool_field "$id" sha256)"
  if [ -f "$marker" ]; then
    if grep -Fqx "id=$id" "$marker" &&
       grep -Fqx "version=$version" "$marker" &&
       grep -Fqx "sha256=$sha" "$marker"; then
      printf '[OK] installation %-21s %s\n' "$id" "${install_dir#"$PROJECT_ROOT"/}"
    else
      die "$id : marqueur d'installation incohérent"
    fi
  elif [ -e "$install_dir" ]; then
    die "$id : destination présente sans marqueur d'installation valide"
  else
    printf '[ABSENT] installation %-17s %s\n' "$id" "$(tool_field "$id" install_dir)"
  fi
}

case "$MODE" in
  plan)
    for id in "${TOOL_IDS[@]}"; do plan_tool "$id"; done
    printf '%s\n' "Plan terminé : aucun accès réseau, aucune extraction, aucun binaire exécuté."
    ;;
  download)
    for id in "${TOOL_IDS[@]}"; do download_tool "$id"; done
    ;;
  install)
    for id in "${TOOL_IDS[@]}"; do install_tool "$id"; done
    ;;
  verify)
    for id in "${TOOL_IDS[@]}"; do verify_tool "$id"; done
    printf '%s\n' "Vérification terminée sans exécuter les outils installés."
    ;;
esac
