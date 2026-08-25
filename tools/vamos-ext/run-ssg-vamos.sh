#!/usr/bin/env bash
# Run the original 1987 Amiga `ssg` binary under vamos with the tracked stubs.
#
# NOTE: machine68k (the Musashi 68000 emulator) crashes on native Windows
# Python, so this must run under WSL / Linux.  The natively-compiled
# reconstruction (ssg_authentic) needs none of this; only the original 68k
# executable does.
#
# Usage (from WSL, repo root or anywhere):
#   tools/vamos-ext/run-ssg-vamos.sh E S=2 I=robot.dat D=out.rgb O=out.ssimg
#
# All arguments are passed straight to `ssg`.  Relative I=/D=/O= paths are
# resolved inside the scene directory (Raytracer_1987_Graham_Source_Code),
# which is used as the working directory so vamos can map them.
set -euo pipefail

here="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
root="$(cd "$here/../.." && pwd)"
scene="$root/Raytracer_1987_Graham_Source_Code"
venv="$root/.venv-vamos-wsl/bin/python"
amitools="$root/out/amitools-src"

[ -x "$venv" ] || { echo "error: WSL venv python not found at $venv" >&2; exit 1; }
[ -f "$scene/ssg" ] || { echo "error: ssg binary not found at $scene/ssg" >&2; exit 1; }

"$here/install.sh"

cd "$scene"
exec env PYTHONPATH="$amitools" "$venv" -m amitools.tools.vamos -- ssg "$@"
