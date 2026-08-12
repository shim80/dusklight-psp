#!/usr/bin/env bash

SCRIPT_DIR="$(CDPATH= cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd -P)"
# shellcheck source=lib/common.sh
. "$SCRIPT_DIR/lib/common.sh"

export DUSKLIGHT_PSP_ROOT="$PROJECT_ROOT"
export PSPDEV="$PROJECT_ROOT/.tools/pspdev"
export PSP="$PSPDEV/psp"
export PSPSDK="$PSP/sdk"
export PPSSPP_STATE_ROOT="$PROJECT_ROOT/.test-data/ppsspp"
export PPSSPP_HOME="$PPSSPP_STATE_ROOT/home"
export XDG_CONFIG_HOME="$PPSSPP_HOME/.config"
export XDG_CACHE_HOME="$PPSSPP_STATE_ROOT/xdg-cache"
export TMPDIR="$PROJECT_ROOT/.tmp"

case ":$PATH:" in
  *":$PSPDEV/bin:"*) ;;
  *) export PATH="$PSPDEV/bin:$PROJECT_ROOT/.tools/bin:$PATH" ;;
esac

if [ -d "$PSP/lib/pkgconfig" ]; then
  export PKG_CONFIG_PATH="$PSP/lib/pkgconfig${PKG_CONFIG_PATH:+:$PKG_CONFIG_PATH}"
fi

printf '%s\n' "Environnement local défini pour ce processus : $PROJECT_ROOT"
