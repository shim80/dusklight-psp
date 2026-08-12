#!/usr/bin/env bash

# Shared confinement helpers. This file is sourced by the project scripts.

COMMON_DIR="$(CDPATH= cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd -P)"
PROJECT_ROOT="$(CDPATH= cd -- "$COMMON_DIR/../.." && pwd -P)"
MANIFEST_PATH="$PROJECT_ROOT/toolchain/manifest.lock"

die() {
  printf 'Erreur : %s\n' "$*" >&2
  exit 1
}

require_project_root() {
  command -v git >/dev/null 2>&1 || die "Git est requis pour établir la frontière du projet."
  local git_root
  git_root="$(git -C "$PROJECT_ROOT" rev-parse --show-toplevel 2>/dev/null)" ||
    die "aucune racine Git ne contient $PROJECT_ROOT"
  git_root="$(CDPATH= cd -- "$git_root" && pwd -P)"
  [ "$git_root" = "$PROJECT_ROOT" ] ||
    die "la racine calculée ($PROJECT_ROOT) diffère de la racine Git ($git_root)"
}

assert_project_path() {
  local input="$1" path probe relative part
  local -a project_parts
  case "$input" in
    /*) path="$input" ;;
    *) path="$PROJECT_ROOT/$input" ;;
  esac

  case "/$path/" in
    */../*|*/./*) die "composant de chemin relatif interdit : $input" ;;
  esac
  case "$path" in
    "$PROJECT_ROOT"|"$PROJECT_ROOT"/*) ;;
    *) die "chemin extérieur à la racine : $input" ;;
  esac

  relative="${path#"$PROJECT_ROOT"}"
  relative="${relative#/}"
  probe="$PROJECT_ROOT"
  IFS='/' read -r -a project_parts <<< "$relative"
  for part in "${project_parts[@]}"; do
    [ -n "$part" ] || continue
    probe="$probe/$part"
    [ ! -L "$probe" ] || die "lien symbolique refusé dans le chemin : $probe"
  done

  if [ -e "$path" ]; then
    probe="$(realpath "$path")" || die "impossible de résoudre : $path"
    case "$probe" in
      "$PROJECT_ROOT"|"$PROJECT_ROOT"/*) ;;
      *) die "résolution extérieure à la racine : $path -> $probe" ;;
    esac
  fi

  printf '%s\n' "$path"
}

safe_mkdir() {
  local path
  path="$(assert_project_path "$1")"
  mkdir -p -- "$path"
  assert_project_path "$path" >/dev/null
}

sha256_file() {
  local file="$1"
  if command -v sha256sum >/dev/null 2>&1; then
    sha256sum "$file" | awk '{print $1}'
  elif command -v shasum >/dev/null 2>&1; then
    shasum -a 256 "$file" | awk '{print $1}'
  else
    die "aucun outil SHA-256 (sha256sum ou shasum) n'est disponible"
  fi
}

toml_tool_value() {
  local tool_id="$1" key="$2"
  awk -v wanted="$tool_id" -v key="$key" '
    /^\[\[tool\]\]$/ { active = 0; next }
    $0 == "id = \"" wanted "\"" { active = 1; next }
    active && index($0, key " = ") == 1 {
      value = substr($0, length(key) + 4)
      if (value ~ /^\".*\"$/) {
        sub(/^\"/, "", value)
        sub(/\"$/, "", value)
      }
      print value
      exit
    }
  ' "$MANIFEST_PATH"
}

host_platform() {
  local os arch
  os="$(uname -s)"
  arch="$(uname -m)"
  case "$os:$arch" in
    Darwin:arm64) printf '%s\n' macos-arm64 ;;
    Darwin:x86_64) printf '%s\n' macos-x86_64 ;;
    Linux:x86_64)
      if [ -n "${WSL_DISTRO_NAME:-}" ]; then
        printf '%s\n' wsl-ubuntu-x86_64
      else
        printf '%s\n' linux-x86_64
      fi
      ;;
    Linux:aarch64|Linux:arm64) printf '%s\n' linux-arm64 ;;
    *) printf 'unsupported:%s:%s\n' "$os" "$arch" ;;
  esac
}

timestamp_utc() {
  date -u '+%Y%m%dT%H%M%SZ'
}

require_project_root
