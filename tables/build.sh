#!/usr/bin/env bash
# Rebuild everything in tables/ that is derived. Safe to run any time; no side effects outside tables/.
#
#   ./tables/build.sh            check anchors, redraw the map + palettes
#   ./tables/build.sh base city  also draw the composite base on a real backdrop
set -euo pipefail
cd "$(dirname "$0")/.."

python3 tools/table_map.py check
python3 tools/table_map.py figure
python3 tools/table_map.py palettes

if [ "${1:-}" = "base" ]; then
    if [ -n "${2:-}" ]; then
        python3 tools/table_map.py base --backdrop "${2}.jpg"
    else
        python3 tools/table_map.py base
    fi
fi
