#!/usr/bin/env bash
set -euo pipefail

if [ "$#" -ne 1 ]; then
  echo "usage: $0 <output-image>" >&2
  exit 1
fi

img="$1"
root_dir="$(cd "$(dirname "$0")/.." && pwd)"
tmp_elf="$(mktemp /tmp/myos-hello-elf-XXXXXX.elf)"
trap 'rm -f "$tmp_elf"' EXIT

rm -f "$img"
dd if=/dev/zero of="$img" bs=1024 count=1440 status=none

mformat -i "$img" -f 1440 ::

"$root_dir/scripts/build_hello_elf.sh" "$tmp_elf"

mcopy -i "$img" "$root_dir/disk/HELLO.TXT" ::HELLO.TXT
mcopy -i "$img" "$root_dir/disk/HELLO.BIN" ::HELLO.BIN
mcopy -i "$img" "$tmp_elf" ::HELLO.ELF
mcopy -i "$img" "$root_dir/disk/AUTOEXEC.BAT" ::AUTOEXEC.BAT

echo "[disk] created $img"
