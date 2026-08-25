#!/usr/bin/env bash
# Install the git-tracked vamos library stubs into the (gitignored) amitools
# checkout so `vamos` uses them when running the original Amiga `ssg` binary.
#
# Usage:  tools/vamos-ext/install.sh
# Run from anywhere; paths are resolved relative to this script.
set -euo pipefail

here="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
root="$(cd "$here/../.." && pwd)"
dst="$root/out/amitools-src/amitools/vamos/lib"

if [ ! -d "$dst" ]; then
    echo "error: amitools checkout not found at $dst" >&2
    echo "       (expected the vendored amitools under out/amitools-src)" >&2
    exit 1
fi

for f in GraphicsLibrary.py IntuitionLibrary.py; do
    cp "$here/$f" "$dst/$f"
    echo "installed $f -> $dst/$f"
done
