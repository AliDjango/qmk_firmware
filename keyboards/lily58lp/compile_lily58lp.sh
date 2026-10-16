#!/usr/bin/env bash
set -euo pipefail
QMK_DIR="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/../.." && pwd)"
cd "$QMK_DIR"
# Build only. Flashing is an explicit separate action.
make lily58lp:upstream -j"${JOBS:-8}"
printf 'Firmware: %s/lily58lp_upstream.uf2\n' "$QMK_DIR"
sha256sum "$QMK_DIR/lily58lp_upstream.uf2"
