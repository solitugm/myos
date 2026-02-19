#!/usr/bin/env bash
set -euo pipefail

if [ "$#" -ne 1 ]; then
  echo "usage: $0 <output-image>" >&2
  exit 1
fi

img="$1"
root_dir="$(cd "$(dirname "$0")/.." && pwd)"

rm -f "$img"
dd if=/dev/zero of="$img" bs=1024 count=1440 status=none

mformat -i "$img" -f 1440 ::

mcopy -i "$img" "$root_dir/disk/HELLO.TXT" ::HELLO.TXT
mcopy -i "$img" "$root_dir/disk/HELLO.BIN" ::HELLO.BIN

echo "[disk] created $img"
