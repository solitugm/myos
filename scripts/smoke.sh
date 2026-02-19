#!/usr/bin/env bash
set -euo pipefail

cd "$(dirname "$0")/.."

for i in 1 2 3; do
  echo "[smoke] build #$i"
  make clean >/dev/null
  make >/dev/null
  echo "[smoke] boot #$i (5s)"
  timeout 5s qemu-system-i386 -cdrom myos.iso -m 256M -no-reboot -no-shutdown -display none >/dev/null 2>&1 || true
done

echo "[smoke] done"
