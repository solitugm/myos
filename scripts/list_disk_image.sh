#!/usr/bin/env bash
set -euo pipefail

if [ "$#" -ne 1 ]; then
  echo "usage: $0 <disk-image>" >&2
  exit 1
fi

img="$1"
mdir -i "$img" ::
