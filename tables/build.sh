#!/usr/bin/env bash
# Rebuild everything in tables/ that is derived. No effect outside tables/.
#
#   ./tables/build.sh              anchors-vs-src check, redraw the map + palettes
#   ./tables/build.sh svg          also write the vector version of the map
#   ./tables/build.sh base city    also draw the composite base on assets/city.jpg
set -euo pipefail
cd "$(dirname "$0")/.."

python3 tools/table_map.py check
python3 tools/table_map.py figure
python3 tools/table_map.py palettes

case "${1:-}" in
    svg)  python3 tools/table_map.py svg ;;
    base) python3 tools/table_map.py base --backdrop "${2:-city}.jpg" ;;
esac
