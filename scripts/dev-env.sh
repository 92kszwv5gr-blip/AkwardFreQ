#!/usr/bin/env bash
# AkwardFreQ dev-environment helper for Debian/Ubuntu cloud containers (run as root).
#
#   scripts/dev-env.sh setup                    install or repair everything a session needs (idempotent)
#   scripts/dev-env.sh check                    verify it is all present; exit 1 if anything is missing
#   scripts/dev-env.sh smoke [BINARY] [OUT.png] launch the Standalone app headless, screenshot it, verify it drew
#
# Cloud sessions are reset between conversations: /tmp and installed packages disappear,
# so run `setup` and then `check` at the start of a session that needs to build.
set -u

ORT_VER=1.20.1
ORT_LINUX=/tmp/onnxruntime-linux/onnxruntime-linux-x64-$ORT_VER
ORT_WIN=/tmp/onnxruntime-win/onnxruntime-win-x64-$ORT_VER
ORT_URL=https://github.com/microsoft/onnxruntime/releases/download/v$ORT_VER
MINGW_INC=/usr/x86_64-w64-mingw32/include

failures=0
pass() { printf 'PASS  %s\n' "$1"; }
fail() { printf 'FAIL  %s\n' "$1"; failures=$((failures + 1)); }
step() { printf '\n== %s\n' "$1"; }

cmd_setup() {
  if [ "$(id -u)" -ne 0 ]; then echo "setup needs root (apt-get)"; exit 1; fi

  step "apt packages"
  apt-get update -qq || fail "apt-get update"
  # --no-install-recommends: the Recommends of wine pull in multimedia packages whose
  # versions 404 on stale mirrors and abort the whole install.
  apt-get install -y --no-install-recommends \
    cmake g++ make git curl unzip zip patchelf pkg-config \
    g++-mingw-w64-x86-64 gcc-mingw-w64-x86-64 mingw-w64-tools \
    xvfb xdotool imagemagick x11-utils \
    libxrandr-dev libxinerama-dev libxcursor-dev libxcomposite-dev libfreetype-dev \
    libfontconfig1-dev libasound2-dev libgtk-3-dev >/tmp/dev-env-apt.log 2>&1 \
    && pass "build + headless tools installed" || fail "apt install (see /tmp/dev-env-apt.log)"
  # wine64 alone has no /usr/bin/wine; the `wine` metapackage provides it.
  apt-get install -y --no-install-recommends wine64 wine >>/tmp/dev-env-apt.log 2>&1 \
    && pass "wine installed" || fail "wine install (see /tmp/dev-env-apt.log)"

  step "mingw quirks"
  # Parts of the VST3 SDK include <Windows.h>; mingw ships only windows.h, and the filesystem is case-sensitive.
  [ -e "$MINGW_INC/Windows.h" ] || ln -s windows.h "$MINGW_INC/Windows.h"
  [ -e "$MINGW_INC/Windows.h" ] && pass "Windows.h symlink" || fail "Windows.h symlink"
  # Wrapper that lets CMake run cross-compiled Windows helpers under Wine quietly.
  printf '#!/bin/bash\nexport WINEDEBUG=-all\nexec wine "$@"\n' >/usr/local/bin/wine-quiet
  chmod +x /usr/local/bin/wine-quiet && pass "wine-quiet wrapper"

  step "ONNX Runtime $ORT_VER"
  if [ ! -f "$ORT_LINUX/lib/libonnxruntime.so" ]; then
    mkdir -p /tmp/onnxruntime-linux && curl -fsSL "$ORT_URL/onnxruntime-linux-x64-$ORT_VER.tgz" | tar xz -C /tmp/onnxruntime-linux
  fi
  if [ ! -f "$ORT_WIN/lib/onnxruntime.dll" ]; then
    mkdir -p /tmp/onnxruntime-win && curl -fsSL -o /tmp/onnxruntime-win/ort.zip "$ORT_URL/onnxruntime-win-x64-$ORT_VER.zip" \
      && unzip -q -o /tmp/onnxruntime-win/ort.zip -d /tmp/onnxruntime-win
  fi
  [ -f "$ORT_LINUX/lib/libonnxruntime.so" ] && pass "ONNX Runtime (Linux)" || fail "ONNX Runtime (Linux)"
  [ -f "$ORT_WIN/lib/onnxruntime.dll" ] && pass "ONNX Runtime (Windows)" || fail "ONNX Runtime (Windows)"

  echo; [ "$failures" -eq 0 ] && echo "setup finished. Run: scripts/dev-env.sh check" || echo "setup finished with $failures problem(s)."
  exit "$failures"
}

have() { command -v "$1" >/dev/null 2>&1; }
need() { if have "$1"; then pass "$1"; else fail "$1 not found"; fi; }

cmd_check() {
  step "tools"
  need cmake; need g++; need make; need git; need curl; need unzip; need zip; need patchelf
  need x86_64-w64-mingw32-g++-posix; need x86_64-w64-mingw32-windres
  need wine; need wine-quiet; need Xvfb; need xdotool; need import
  step "files"
  [ -e "$MINGW_INC/Windows.h" ] && pass "mingw Windows.h symlink" || fail "mingw Windows.h symlink (scripts/dev-env.sh setup)"
  [ -f /usr/include/X11/extensions/Xrandr.h ] && pass "X11 dev headers (needed to build the native juceaide tool)" || fail "X11 dev headers"
  [ -f "$ORT_LINUX/lib/libonnxruntime.so" ] && [ -f "$ORT_LINUX/include/onnxruntime_cxx_api.h" ] && pass "ONNX Runtime (Linux) $ORT_VER" || fail "ONNX Runtime (Linux) at $ORT_LINUX"
  [ -f "$ORT_WIN/lib/onnxruntime.dll" ] && [ -f "$ORT_WIN/include/onnxruntime_cxx_api.h" ] && pass "ONNX Runtime (Windows) $ORT_VER" || fail "ONNX Runtime (Windows) at $ORT_WIN"
  echo
  if [ "$failures" -eq 0 ]; then echo "check: everything present"; else echo "check: $failures problem(s). Run: scripts/dev-env.sh setup"; fi
  exit $(( failures > 0 ))
}

cmd_smoke() {
  local bin="${1:-build/AkwardFreQ_artefacts/Debug/Standalone/AkwardFreQ}" out="${2:-/tmp/afq-smoke.png}"
  local disp=":$((100 + $$ % 100))" xpid apid stddev
  [ -x "$bin" ] || { echo "FAIL  binary not found: $bin (build the Standalone target first)"; exit 1; }
  for t in Xvfb xdotool import identify; do have "$t" || { echo "FAIL  $t missing (scripts/dev-env.sh setup)"; exit 1; }; done

  Xvfb "$disp" -screen 0 1280x800x24 >/dev/null 2>&1 & xpid=$!
  sleep 2
  ( cd "$(dirname "$bin")" && DISPLAY=$disp exec "./$(basename "$bin")" >/tmp/afq-smoke.log 2>&1 ) & apid=$!

  local seen=0
  for _ in $(seq 1 20); do
    if DISPLAY=$disp xdotool search --name AkwardFreQ >/dev/null 2>&1; then seen=1; break; fi
    sleep 1
  done
  sleep 3   # let the first paint finish
  DISPLAY=$disp import -window root "$out" 2>/dev/null
  local alive=0; kill -0 "$apid" 2>/dev/null && alive=1
  stddev=$(identify -format '%[fx:standard_deviation]' "$out" 2>/dev/null || echo 0)
  kill "$apid" 2>/dev/null; kill "$xpid" 2>/dev/null; wait 2>/dev/null

  [ "$seen" -eq 1 ] && pass "app window appeared" || fail "no window named AkwardFreQ within 20 s (see /tmp/afq-smoke.log)"
  [ "$alive" -eq 1 ] && pass "app still running after start-up" || fail "app exited early (see /tmp/afq-smoke.log)"
  awk -v s="$stddev" 'BEGIN { exit !(s > 0.02) }' && pass "screenshot is not blank (contrast $stddev): $out" || fail "screenshot looks blank (contrast $stddev): $out"
  exit $(( failures > 0 ))
}

case "${1:-}" in
  setup) cmd_setup ;;
  check) cmd_check ;;
  smoke) shift; cmd_smoke "$@" ;;
  *) sed -n '2,8p' "$0"; exit 2 ;;
esac
