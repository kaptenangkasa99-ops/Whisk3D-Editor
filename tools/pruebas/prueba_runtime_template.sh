#!/usr/bin/env bash
set -euo pipefail

ROOT="$(cd "$(dirname "$0")/../.." && pwd)"
TEMPLATE_DIR="$ROOT/templates/platformer"
BUILD_DIR="$TEMPLATE_DIR/platform-build/linux/out"
CAPTURE="/tmp/w3d_platformer_runtime_smoke.png"
LOG="/tmp/w3d_platformer_runtime_smoke.log"

cmake --build "$ROOT/build" -j2
export W3D_RUNTIME_SOURCE="$ROOT/tools/pruebas/runtime_smoke/w3drun.cpp"
(cd "$ROOT/tools/pruebas" &&
    "$ROOT/build/whisk3d" --script prueba_runtime_template_build.w3s)

if [ ! -x "$BUILD_DIR/platformer" ]; then
    printf 'Generated game binary was not produced: %s\n' "$BUILD_DIR/platformer" >&2
    exit 1
fi

(cd "$BUILD_DIR" &&
    W3D_CAPTURA="$CAPTURE" W3D_CAPTURA_FRAME=30 ./platformer >"$LOG" 2>&1)
cat "$LOG"
grep -qF "[RUNTIME SMOKE PASS]" "$LOG"
test -s "$CAPTURE"
printf 'Runtime template verification passed; capture: %s\n' "$CAPTURE"
