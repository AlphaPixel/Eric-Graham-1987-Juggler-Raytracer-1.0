# vamos extensions for running the original `ssg` binary

These are the small `vamos` (amitools) library stubs used to run the original
1987 Amiga `ssg` executable (`Raytracer_1987_Graham_Source_Code/ssg`) under
emulation for reverse-engineering and verification. They live here, in a
git-tracked location, rather than only inside the gitignored amitools checkout
under `out/`.

## Why these exist

`ssg` is a command-line renderer, but it is still an Amiga program: even when
asked only to write files, it opens `graphics.library`/`intuition.library`,
creates a screen and window, and plots every rendered pixel with
`SetAPen`/`WritePixel`. To run it headlessly, vamos provides Python
implementations of those calls.

The earlier stubs were **no-ops** — they accepted `WritePixel` and threw the
pixel away. That was enough to capture `ssg`'s raw `D=` RGB dump (which is
computed before any Amiga plotting) and the `O=` file's 52-byte header and
palette. But it left the `O=` file's **six-bitplane body unusable**: `ssg`
writes that body straight from the window RastPort's BitMap after plotting
(`FUN_00224acc` reads `window->RPort->BitMap->Planes[0..5]`, stride
`BitMap.BytesPerRow`), and with no-op plotting that memory was never populated.

## What they add

- **`IntuitionLibrary.OpenWindow`** allocates a real, output-sized BitMap (six
  planes) and wires it to the window's RastPort (`RastPort.BitMap`,
  `Window.RPort`), sized from `NewWindow.Width/Height`.
- **`GraphicsLibrary.WritePixel`** plots the current `SetAPen` pen into those six
  bitplanes (`byte = y*BytesPerRow + x/8`, `mask = 0x80 >> (x&7)`), with clipping
  for the edge/flush calls `ssg` makes past the raster.

Nothing else changes: `SetRGB4` still just records the palette, and the `D=`
render math is untouched.

## Verified result

Running `ssg E S=2 I=robot.dat D=... O=...` under these stubs and comparing to
the natively-compiled reconstruction (`ssg_authentic`):

```text
D= raw RGB   : 0 differing bytes vs the previously captured oracle
               (the stubs do not perturb the render)
O= palette   : identical
O= bitplane body : 13 / 3000 differing bytes vs native ssg_authentic
               (same single-precision-noise order as the D= diffs)
```

So the `O=` bitplane body is now a genuine byte-for-byte oracle. Previously the
body could only be described as "not a reliable oracle"; it can now verify the
reconstruction's entire HAM output path — register-vs-hold-and-modify decisions,
dither, `B=` blur, and plane bit-packing — against the real 68000 executable.

## Requirements and how to run

**WSL / Linux only.** `machine68k` (the Musashi 68000 core) imports on native
Windows Python but **crashes with an access violation when a `Machine` is
constructed**, so the 68k binary must be run under WSL. (The reconstruction
`ssg_authentic` is plain C and builds/runs natively on Windows — none of this is
needed for it; only for driving the original executable.)

Prerequisites, all already present in the working tree:

- `out/amitools-src` — the vendored amitools checkout (gitignored)
- `.venv-vamos-wsl` — a WSL Python venv with `machine68k` installed

Install the stubs into the amitools checkout and run `ssg`:

```sh
# from WSL, at the repo root
tools/vamos-ext/install.sh                                  # copy stubs in
tools/vamos-ext/run-ssg-vamos.sh E S=2 I=robot.dat \
    D=vamos-robot.rgb O=vamos-robot.ssimg                   # run under vamos
```

`run-ssg-vamos.sh` runs `install.sh` for you, sets `PYTHONPATH` to the vendored
amitools, uses `Raytracer_1987_Graham_Source_Code/` as the working directory
(so relative `I=`/`D=`/`O=` paths map), and passes all arguments straight to
`ssg`.

## Files

```text
GraphicsLibrary.py    graphics.library stub: SetRGB4, SetAPen, real WritePixel
IntuitionLibrary.py   intuition.library stub: OpenScreen/Window with a real BitMap
install.sh            copy the two stubs into out/amitools-src/.../vamos/lib/
run-ssg-vamos.sh      install + run ssg under vamos (WSL)
```

These override the same-named modules in amitools; `install.sh` overwrites the
copies under `out/amitools-src/amitools/vamos/lib/`. If the vendored amitools is
re-fetched, re-run `install.sh`.
