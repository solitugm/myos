#!/usr/bin/env bash
set -euo pipefail

if [ "$#" -ne 1 ]; then
  echo "usage: $0 <output-elf>" >&2
  exit 1
fi

out="$1"
root_dir="$(cd "$(dirname "$0")/.." && pwd)"
tmp_o="$(mktemp /tmp/hello-elf-XXXXXX.o)"
trap 'rm -f "$tmp_o"' EXIT

nasm -f elf32 "$root_dir/disk/HELLO_ELF.asm" -o "$tmp_o"
ld -m elf_i386 -nostdlib -e _start -Ttext 0 -o "$out" "$tmp_o"

echo "[disk] built ELF sample: $out"
