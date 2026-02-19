#!/usr/bin/env bash
set -euo pipefail

if [ "$#" -ne 2 ]; then
  echo "usage: $0 <disk-image> <out-dir>" >&2
  exit 1
fi

img="$1"
out="$2"

rm -rf "$out"
mkdir -p "$out"

mcopy -s -i "$img" ::* "$out/"

echo "[disk] pulled files from $img to $out"
