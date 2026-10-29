#!/usr/bin/env bash
# Rebuild everything in tables/ that is derived. No effect outside tables/.
#
#   ./tables/build.sh                 anchors-vs-src check, redraw map + palettes
#   ./tables/build.sh svg             also write the vector map
#   ./tables/build.sh base city       composite base on assets/city.jpg -> tables/table_city/base.png
#   ./tables/build.sh table_city      build that table's art from tables/table_city/table.json
set -euo pipefail
cd "$(dirname "$0")/.."

case "${1:-}" in
    base) python3 tools/table_map.py base --backdrop "${2:-city}.jpg"; exit 0 ;;
    svg)  python3 tools/table_map.py check; python3 tools/table_map.py figure
          python3 tools/table_map.py palettes; python3 tools/table_map.py svg; exit 0 ;;
    "")   python3 tools/table_map.py check; python3 tools/table_map.py figure
          python3 tools/table_map.py palettes; exit 0 ;;
    *)    if [ -f "tables/$1/table.json" ]; then
              python3 tools/table_map.py check
              python3 tools/table_art.py build "tables/$1/table.json"
          else
              echo "no tables/$1/table.json - a table folder is tables/<name>/ with table.json inside" >&2
              exit 2
          fi ;;
esac
