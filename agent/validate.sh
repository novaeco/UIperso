#!/usr/bin/env bash
set -euo pipefail
PORT="${1:-}"

test -f "CMakeLists.txt" || { echo "Run from repo root"; exit 1; }

idf.py set-target esp32s3
idf.py fullclean
idf.py build

if [[ -n "$PORT" ]]; then
  idf.py -p "$PORT" flash
  idf.py -p "$PORT" monitor
else
  echo "No port given. Usage: ./agent/validate.sh /dev/ttyUSB0"
fi
