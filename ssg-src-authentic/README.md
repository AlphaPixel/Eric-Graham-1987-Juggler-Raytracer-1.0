# `ssg` source reconstruction

This directory is a source-level reconstruction of the original Amiga `SSG: Scene
Simulation Generator`, recovered from the 1987 Amiga 68000 executable. The goal is
not merely to draw the same pictures but to recover, as closely as the surviving
evidence allows, **how Eric Graham's own source actually worked** — its names, its
structure, its scene grammar, its single-precision math, and its screen-space
culling renderer. It compiles on modern toolchains and its output is validated
byte-for-byte against the original binary run under `vamos`.

## What makes it faithful

Every decision is driven by a surviving code artifact: Eric's published
`rt1.c`/`rt2.c`/`rt3.c` (`../src`), the `rt.h` declarations, or the strings and
code recovered from the `ssg` binary itself. As far as we know, no surviving piece
of evidence — down to a misspelling — was left unused.

| Aspect | The reconstruction | Evidence |
|---|---|---|
| Function names | bare `raytrace`, `dot`, `setupfromdat` | `rt1.c`; `setupfromdat` is named in `rt2.c`/`rt.h` |
| Types | bare `struct sphere`, `struct world` | `rt.h` |
| Precision | C `float` (FFP single) | binary links `mathffp`/`mathtrans` |
| HAM state | file-scope `nallocr`, `creg`, **`threshhold`** (misspelling kept) | `rt3.c` |
| Scene parser | `fscanf` straight off the stream with the binary's format strings | decompiled reader `FUN_0022141e`; `(%lf,%lf,%lf)`, `<%lf,%lf,%lf> %d`, `(%lf,%lf,%lf):%lf`, … |
| Sphere storage | one fixed up-front allocation (400 spheres) | binary's `AllocMem(0x5140)` = 400 × 0x34-byte records |
| Object detection | read one `%c`, test `'<'`/`';'`, skip only `\n`/space/tab | decompiled `%c` loop |
| Renderer | per-sphere screen box + per-scanline active lists + per-pixel column cull | `FUN_002225ca`, `FUN_0021fbea`, `FUN_0021fd1a` |
| Object record | `pos/color/radius/flag/ymin/ymax/xmin/xmax/type` — the exact 0x34-byte layout | binary sphere record offsets |
| Open-fail message | `input file error on '%s'` | binary string |
| Parse-error path | `Error before:` + context + `input error` | binary strings |
| Formatting | Eric's dense multi-statement lines, trailing comments | `rt*.c` house style |

Eric's *published* `rt1.c` reflection routine listing contains a garbled `reflect()`;
the compiled `ssg` executable uses the ordinary `y = x - 2(x·n)n`. The reconstruction
follows the binary, because that is the code that actually produced the surviving renders.

## The renderer is not brute force

The easy way to write a raytracer renders brute force — every sphere tested against
every pixel. The original does not, and getting this right is what most separates a
faithful reconstruction from one that merely draws the same picture. The
decompilation shows a scanline-coherent, screen-space-culling renderer, and this
reconstruction reproduces it:

- **Load-time projection** (`FUN_002225ca` → `project()` in `raytrace.c`). Every sphere
  and lamp is projected to a screen-space bounding box and classified as wholly behind
  the observer, straddling it, or wholly in front. Those results live in the object
  record itself — which is exactly why the binary's sphere record is 0x34 bytes with a
  `flag`/`ymin`/`ymax`/`xmin`/`xmax` block between the radius and the type.
- **Per-scanline active lists** (`FUN_0021fbea` → `actsp`/`actlmp` in `ssg.c`). Before
  each row is drawn, the spheres and lamps whose vertical screen extent crosses that row
  (and that aren't behind the camera) are gathered into small index lists.
- **Per-pixel column cull** (`FUN_0021fd1a` → the culled search in `raytrace`). For each
  pixel the nearest-hit search walks only that scanline's active list, and skips any
  object whose horizontal screen extent does not contain the column.
- **Mirror bounces skip the cull.** Reflected rays are not screen-aligned, so the mirror
  path calls `raytrace(...,0,0,0)` and tests every sphere, exactly as the binary's
  unculled search path does.

Because a correct cull only ever skips objects that provably cannot be hit at that pixel,
the output is unchanged — the culled renderer is byte-for-byte identical to the brute-force
one, and simply faster (noticeably so on the 288-sphere dragon). The projection is the
same inverse-`pixline` perspective map the rays use, including the `V`-flag axis swap, so
the recovered `robot.dat V` render still matches.

## The one divergent modernization

Eric's original was K&R C. To build on MSVC and gcc, this variant uses ANSI
function prototypes — the same minimal change the published `rt` port made. That
apart, the style stays as close to 1987 as the evidence supports. We want people
to be able to learn from, build and experiment with this code, and K&R style makes
this deliberately obtuse. Changing to ANSI style doesn't materially change the
code aspects that people want to learn from.

## Cracks in the mirror ball

A few things genuinely cannot be pinned down from the artifacts and are
reconstructed to match behavior, not to claim Eric wrote them exactly this way.
Each is marked in the source where it occurs:

- The dither before the 4-bit HAM crush and the **`B=` blur** are recovered from
  the executable's behavior; they are not in `rt3.c`, so their exact source form
  is unknown. The math is verified against the binary but the phrasing is a
  reconstruction.
- The scene reader's number reads add a leading space to the recovered format
  strings (`" (%lf,%lf,%lf)"` rather than `"(%lf,%lf,%lf)"`). ISO `fscanf` does
  not skip whitespace before a literal `(`/`<`, but the Amiga C library did, so
  the original strings had no such space. This is the same kind of concession as
  the ANSI-prototype one, and does not change what is parsed.
- The whole-scene **bounding sphere** (`world.bound`) is built exactly as the
  binary builds it at load time, but its consumer was not located in the
  recovered render code. It is kept for load-time fidelity and is not used by the
  cull; if a future pass finds where the binary reads it, it can be wired in.
- The sphere list is a single fixed up-front allocation matching the binary's
  `AllocMem(0x5140)` (400 spheres). Unlike the binary, which trusts the file, the
  reconstruction treats an over-full scene as a parse error instead of writing
  past the buffer — a safety net that never triggers on any shipped scene.

## Build

```text
cmake -S . -B build-ssg -DJUGGLER_BUILD_RAYTRACE=OFF
cmake --build build-ssg --target ssg_authentic
```

Typical run (identical CLI to the original and to `ssg_recreated`):

```text
ssg_authentic E S=2 I=robot.dat D=robot-s2.rgb O=robot-s2.ssimg
```

## Validation

The authentic executable is registered as its own CTest, `ssg_authentic_validate`,
running the same `tools/validate-ssg.py` checks as `ssg_recreated`:

```text
ctest --test-dir build-ssg --output-on-failure -R ssg_validate
```

Sampled `S=2` versus the original Amiga binary:

```text
robot.dat    10 differing bytes / 12000   (ss palette/register block matches)
ele.dat       4 differing bytes / 12000
dragon.dat    1 differing byte  / 12000
robot.dat V   4 differing bytes / 12000   (ss palette/register block matches)
```

Full-resolution raw RGB:

```text
robot.dat   233 differing bytes / 192000
ele.dat      69 differing bytes / 192000
```

The screen-space cull is checked against a plain every-sphere search: with culling
on and off the `D=` dumps and `O=` files are **byte-identical**, so the
acceleration provably changes nothing but speed.

## Modules

```text
ssg.h        shared declarations (mirrors rt.h; single precision; 0x34-byte sphere record)
raytrace.c   the renderer + all vector math (mirrors rt1.c) + the screen-box projection
ham.c        HAM encoder + ss file writer (mirrors rt3.c, plus recovered dither/blur)
ssg.c        command line, the fscanf .dat reader, visibility preprocessing, render loop, main
```
