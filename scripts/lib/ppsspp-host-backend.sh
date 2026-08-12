#!/usr/bin/env bash

ppsspp_validate_backend() {
  case "$1" in auto|opengl|vulkan) return 0 ;; esac
  return 1
}

ppsspp_gui_session_available() {
  if [ "$(uname -s)" != Darwin ]; then
    return 0
  fi
  pgrep -x WindowServer >/dev/null 2>&1
}

ppsspp_opengl_available() {
  if [ "$(uname -s)" != Darwin ]; then
    return 0
  fi
  [ -d /System/Library/Frameworks/OpenGL.framework/Versions/Current/Resources ] ||
    [ -d /System/Library/Frameworks/OpenGL.framework/Versions/A/Resources ]
}

ppsspp_metal_available() {
  [ "$(uname -s)" = Darwin ] || return 1
  command -v system_profiler >/dev/null 2>&1 || return 1
  system_profiler SPDisplaysDataType 2>/dev/null |
    grep -Eq 'Metal Support:|Metal Family:'
}

ppsspp_resolve_backend() {
  local requested="$1" runtime_mode="$2"
  if [ "$requested" != auto ]; then
    printf '%s\n' "$requested"
    return
  fi
  case "$runtime_mode" in
    smoke|replay) printf '%s\n' opengl ;;
    *)
      if ppsspp_gui_session_available && ppsspp_metal_available; then
        printf '%s\n' vulkan
      else
        printf '%s\n' opengl
      fi
      ;;
  esac
}

ppsspp_write_backend_config() {
  local base="$1" backend="$2" destination="$3" backend_value
  case "$backend" in
    opengl) backend_value='0 (OPENGL)' ;;
    vulkan) backend_value='3 (VULKAN)' ;;
    *) return 1 ;;
  esac
  [ -f "$base" ] || return 1
  {
    awk '
      !/^GraphicsBackend = / &&
      !/^DisabledGraphicsBackends = / {
        print
      }
    ' "$base"
    printf 'GraphicsBackend = %s\n' "$backend_value"
    printf 'DisabledGraphicsBackends = \n'
  } >"$destination"
}

ppsspp_graphics_argument() {
  local backend="$1" software_renderer="$2"
  if [ "$software_renderer" = true ]; then
    printf '%s\n' software
  elif [ "$backend" = vulkan ]; then
    printf '%s\n' vulkan
  else
    printf '%s\n' opengl3.1
  fi
}

ppsspp_boot_observed() {
  local stderr_log="$1" eboot="$2"
  [ -f "$stderr_log" ] &&
    grep -Fq "Booted $eboot" "$stderr_log"
}

ppsspp_host_graphics_failure() {
  local stderr_log="$1"
  [ -f "$stderr_log" ] &&
    grep -Eqi \
      'VK_ERROR_INCOMPATIBLE_DRIVER|Metal, which is not available|Failed to create (vulkan|OpenGL|GL) (instance|context|device)|Could not create.*(OpenGL|Vulkan|window)|No available video device' \
      "$stderr_log"
}

ppsspp_failure_classification() {
  local stderr_log="$1" eboot="$2" timed_out="$3"
  if ppsspp_boot_observed "$stderr_log" "$eboot"; then
    if [ "$timed_out" = true ]; then
      printf '%s\n' PSP_MARKER_TIMEOUT
    else
      printf '%s\n' PSP_EBOOT_FAILED
    fi
  elif ppsspp_host_graphics_failure "$stderr_log"; then
    printf '%s\n' HOST_GRAPHICS_INIT_FAILED
  elif ! ppsspp_gui_session_available; then
    printf '%s\n' PPSSPP_GUI_SESSION_REQUIRED
  else
    printf '%s\n' HOST_GRAPHICS_INIT_FAILED
  fi
}

ppsspp_write_preflight() {
  local destination="$1" requested="$2" selected="$3" binary="$4"
  local molten="${binary%/MacOS/PPSSPPSDL}/Frameworks/libMoltenVK.dylib"
  {
    printf 'host_os=%s\n' "$(uname -s)"
    printf 'backend_requested=%s\n' "$requested"
    printf 'backend_selected=%s\n' "$selected"
    if ppsspp_gui_session_available; then
      printf 'gui_session_available=true\n'
    else
      printf 'gui_session_available=false\n'
    fi
    if ppsspp_opengl_available; then
      printf 'opengl_framework_available=true\n'
    else
      printf 'opengl_framework_available=false\n'
    fi
    if ppsspp_metal_available; then
      printf 'metal_device_visible=true\n'
    else
      printf 'metal_device_visible=false\n'
    fi
    printf 'ppsspp_binary=%s\n' "$binary"
    if [ -f "$binary" ]; then
      printf 'ppsspp_sha256=%s\n' \
        "$(shasum -a 256 "$binary" | awk '{print $1}')"
    fi
    if [ -f "$molten" ]; then
      printf 'moltenvk=%s\n' "$molten"
      printf 'moltenvk_sha256=%s\n' \
        "$(shasum -a 256 "$molten" | awk '{print $1}')"
    else
      printf 'moltenvk=absent\n'
    fi
  } >"$destination"
}
