#!/usr/bin/env bash
set -euo pipefail
QMK_DIR="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/../.." && pwd)"
case "${1:-}" in
    left|right) side="$1" ;;
    *) printf 'Usage: %s left|right\n' "$0" >&2; exit 2 ;;
esac
cd "$QMK_DIR"
# QMK builds a side-specific image, establishes EE_HANDS, and flashes it.
# Connect only the selected half in BOOTSEL mode before running this script.
make "lily58lp:upstream:uf2-split-$side"
