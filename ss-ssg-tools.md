# `ss` and `ssg` tool notes

This document records current working knowledge of the original Amiga 68k tools:

```text
Raytracer_1987_Graham_Source_Code/ss
Raytracer_1987_Graham_Source_Code/ssg
```

The notes combine string analysis, Ghidra disassembly/decompiler inspection, and live execution of `ssg` under `vamos` with minimal Intuition/Graphics stubs.

## Summary

`ssg` is the renderer. It identifies itself as:

```text
SSG: Scene Simulation Generator
Copyright 1987 Eric Graham
```

It reads text `.dat` scene files, parses observer/lamp/sphere data, raytraces the scene, plots pixels through Amiga graphics calls, and can write raw RGB dump output with `D=...`.

`ss` is the display program. It identifies itself as:

```text
SS: Ray Tracing Display Program
Copyright 1987 Eric Graham
```

It does not contain the `.dat` parser strings. It appears to read already-rendered image files, load a palette and bitplane-like image data, display each illustration, and wait for user input between illustrations.

## `ssg` command line

`ssg` accepts one-letter switches. Most use:

```text
X=value
```

Two known flags, `E` and `V`, do not require `=`.

The dispatch table accepts these switches:

```text
O D R M T S B V I E
```

No other command-line switches have been found in the parser table.

If no input file is supplied with `I=...`, `ssg` prints:

```text
No input file
```

If a malformed argument is supplied, it can print:

```text
command line error %s
Illegal switch %s
```

### Confirmed useful forms

Render a `.dat` file and write raw RGB bytes:

```text
ssg E I=robot.dat D=robot.rgb
```

Render a lower-resolution sampled preview:

```text
ssg E S=2 I=dragon.dat D=dragon-s2.rgb
```

The `E` flag suppresses the final wait-for-input behavior. It is useful for headless runs.

## `ssg` options

`I=filename`

Input scene file. This is required for scene rendering. Confirmed with:

```text
I=robot.dat
I=ele.dat
I=dragon.dat
```

`D=filename`

Raw RGB dump output. Confirmed by live execution. The file receives 24-bit RGB data, three bytes per pixel, after raytracing.

For full-resolution renders, the output is:

```text
320 * 200 * 3 = 192000 bytes
```

`O=filename`

Main rendered output file, opened for writing. This is the `ss`-readable Amiga display/image format.

Validated with:

```text
ssg E S=2 I=robot.dat D=robot-s2.rgb O=robot-s2.ssimg
```

The generated `robot-s2.ssimg` begins with big-endian dimensions:

```text
00 50 00 32
```

which is 80 by 50 for an `S=2` render. The file length is:

```text
52 + rowbytes * height * 6
```

where:

```text
rowbytes = ((width + 15) / 16) * 2
```

For 80 by 50 this is:

```text
52 + 10 * 50 * 6 = 3052 bytes
```

The 52-byte header is:

```text
uint16_be width
uint16_be height
48 bytes of palette/register data
```

The body is six 1-bit Amiga bitplanes, stored plane-by-plane. Each plane contributes `rowbytes * height` bytes. This is the form loaded directly into the six display bitplanes by `ss`.

When `ssg` is run under the current no-op `vamos` graphics stubs, the palette/register block is still useful for validation, but the bitplane body is not a reliable rendered-output oracle. The original binary writes the body from the Amiga bitmap after `WritePixel` calls. If those calls are stubbed without updating bitmap memory, the body can contain non-rendered memory contents even though the palette path and file container are exercised.

`R=filename`

Register/palette input file, opened for reading. Disassembly shows it reads a width, height, and 48 bytes of palette/register data, then loads those colors into the display palette.

Live validation with `R=robot-s2-b0.ssimg` showed:

- The raw RGB raytrace written by `D=` was unchanged.
- The `O=` file adopted the palette/register data from the `R=` file.
- The output file otherwise matched a run that produced the same palette directly.

This means `R=` seeds or fixes the palette/register block for the HAM output path. It does not alter scene geometry or the raw RGB raytrace.

`M=x0:x1:y0:y1:filename`

Mask/reuse option. Parsed with:

```text
%d:%d:%d:%d:%s
```

The parsed file is opened for reading. During rendering, `ssg` reads three RGB bytes from this file for every output pixel before deciding whether to raytrace the pixel. Pixels inside the inclusive rectangle are raytraced. Pixels outside the rectangle are reused from the mask file. For reused pixels, the program prints:

```text
masked
```

The argument order is not the natural `x0:y0:x1:y1` order. The order validated by controlled runs is:

```text
x_min:x_max:y_min:y_max:file
```

For an 80 by 50 sampled render:

```text
ssg E S=2 M=20:60:10:40:mask-grad-s2.rgb I=robot.dat D=robot-s2-mask-grad-reordered.rgb
```

produced:

```text
1271 pixels matching the normal robot render
2729 pixels matching the supplied mask RGB file
0 other pixels
```

The normal-rendered rectangle was exactly:

```text
x = 20..60 inclusive
y = 10..40 inclusive
```

The first attempted form:

```text
M=20:10:60:40:file
```

created an inverted/empty rectangle and reused the supplied file for every pixel.

`S=n`

Sampling/render step option. The parsed integer is clamped to `0..3` and mapped through:

```text
0 -> 1
1 -> 2
2 -> 4
3 -> 8
```

The resulting step is used in X and Y. Confirmed behavior for `D=` output:

```text
S omitted: 320x200 RGB, 192000 bytes
S=2:       80x50 RGB,    12000 bytes
```

`T=n`

Color-distance threshold used by the HAM/palette selection path. The default is `4`, stored in the initialized data near the palette table.

Controlled `S=2` robot runs showed that changing `T=` changed the palette/register block in `O=` output but did not change the six-bitplane body for that sample. Higher values reduced the number of nonzero palette entries. This is consistent with `T=` acting as the distance threshold before allocating another palette/register color.

`B=n`

Blur/smoothing amount used by the HAM output path. Parsed as an integer into a global initialized to `3`.

The value is used after the raw raytraced RGB values have been computed and before the HAM palette/register choice is finalized. The code keeps three scanlines of RGB values and, for each non-edge pixel and color channel, computes a weighted blend of the center pixel and its eight neighbors:

```text
smoothed = (B * sum_of_8_neighbors + (16 - B) * 8 * center + 64) / 128
```

The result is then reduced to the 0..15 range used by the HAM/palette code.

Practical interpretation:

```text
B=0   no blur; use the center pixel
B=3   default; light smoothing
B=16  use the average of the 8 neighboring pixels, with no center-pixel term
```

Values are not clamped in the option parser. Values outside `0..16` would make the center-pixel weight negative or larger than normal, so the meaningful range appears to be `0..16`.

Controlled `S=2` robot runs with `B=0`, `B=1`, `B=2`, `B=3`, and `B=16` showed:

- The `O=` width, height, and six-bitplane body were identical for that sample under the no-op graphics stubs.
- The 48-byte palette/register block changed.
- The raw raytraced RGB output was not changed by `B=`.

This option is therefore an output smoothing/blur coefficient for HAM conversion, not bitplane depth and not scene-rendering quality.

`V`

Sets a flag used by projection/orientation code. Disassembly shows the flag
selecting alternate screen-axis projection branches. In the normal projection,
horizontal screen coordinates use the observer `uhat` basis vector and vertical
screen coordinates use `vhat`. With `V`, those basis-vector roles are swapped,
with the sign convention used by the binary.

Live `S=2` robot runs showed a large change in raw RGB output. The
reconstructed `V` path now compares to the original `robot-s2-v.rgb` with 4
differing bytes out of 12000, and the reconstructed `robot-s2-v.ssimg`
palette/register block matches the original.

The user-facing name is not recovered. `V` is not a file-output transpose.

`E`

Suppresses the final wait-for-input behavior. Confirmed useful for headless execution.

## `.dat` parser behavior

`ssg` contains the known scene parser strings:

```text
(%lf,%lf,%lf)
[%lf,%lf]
%lf
%lf,%lf,%lf> %d
(%lf,%lf,%lf):%lf
<%lf,%lf,%lf>
Total number of spheres=%d
Number of lamps=%d
```

The parser reads observer data, view angles, focal data, object/sphere definitions, lamps, and color triples.

Live runs confirmed these parsed scene counts:

```text
robot.dat   79 spheres, 1 lamp
ele.dat    120 spheres, 1 lamp
dragon.dat 288 spheres, 1 lamp
```

## `D=` RGB output

The `D=` output is raw RGB byte triplets. No header is written.

Full-resolution output:

```text
width:  320
height: 200
bytes:  192000
layout: RGBRGBRGB...
```

Sampled output dimensions follow the render step:

```text
S=2 -> step 4 -> 80x50 -> 12000 bytes
```

The repository now includes a small dependency-free inspection helper:

```text
tools/rgb-to-png.py
```

Example:

```text
python tools/rgb-to-png.py Raytracer_1987_Graham_Source_Code/robot.rgb Raytracer_1987_Graham_Source_Code/robot.png
```

For sampled `S=2` dragon output:

```text
python tools/rgb-to-png.py --width 80 --height 50 Raytracer_1987_Graham_Source_Code/dragon-s2.rgb Raytracer_1987_Graham_Source_Code/dragon-s2.png
```

## Live validation

The original Amiga `ssg` binary was run under WSL using `vamos`, `machine68k`, and local no-op Intuition/Graphics stubs added to the checked-out `amitools` source under:

```text
out/amitools-src
```

The stubs do not draw a real window. They only return plausible Amiga objects and accept the calls `ssg` expects:

```text
OpenScreen
OpenWindow
CloseScreen
CloseWindow
SetPointer
SetRGB4
SetAPen
WritePixel
console.device open
```

This was enough for `ssg` to compute and write `D=` output.

Validated runs:

```text
ssg E I=robot.dat D=robot.rgb
```

Result:

```text
robot.rgb: 192000 bytes
robot.png: visually plausible robot scene
```

```text
ssg E I=ele.dat D=ele.rgb
```

Result:

```text
ele.rgb: 192000 bytes
ele.png: visually plausible elephant scene
```

```text
ssg E S=2 I=dragon.dat D=dragon-s2.rgb
```

Result:

```text
dragon-s2.rgb: 12000 bytes
dragon-s2.png: visually plausible low-resolution dragon scene
```

A full-resolution dragon run:

```text
ssg E I=dragon.dat D=dragon.rgb
```

was still computing at the 15-minute timeout. It reached a partial `D=` output size of 131072 bytes before timeout. This indicates the scene parsed and rendering was underway, but a full validation image was not completed in that run.

Additional option validation used sampled `robot.dat` renders to keep run times short:

```text
ssg E S=2 I=robot.dat D=robot-s2.rgb O=robot-s2.ssimg
ssg E S=2 B=0 I=robot.dat O=robot-s2-b0.ssimg
ssg E S=2 B=1 I=robot.dat O=robot-s2-b1.ssimg
ssg E S=2 B=2 I=robot.dat O=robot-s2-b2.ssimg
ssg E S=2 B=3 I=robot.dat O=robot-s2-b3.ssimg
ssg E S=2 B=16 I=robot.dat O=robot-s2-b16.ssimg
ssg E S=2 T=0 I=robot.dat O=robot-s2-t0.ssimg
ssg E S=2 T=1 I=robot.dat O=robot-s2-t1.ssimg
ssg E S=2 T=4 I=robot.dat O=robot-s2-t4.ssimg
ssg E S=2 T=16 I=robot.dat O=robot-s2-t16.ssimg
ssg E S=2 T=64 I=robot.dat O=robot-s2-t64.ssimg
ssg E V S=2 I=robot.dat D=robot-s2-v.rgb O=robot-s2-v.ssimg
ssg E S=2 R=robot-s2-b0.ssimg I=robot.dat D=robot-s2-rb0.rgb O=robot-s2-rb0.ssimg
ssg E S=2 M=20:60:10:40:mask-grad-s2.rgb I=robot.dat D=robot-s2-mask-grad-reordered.rgb O=robot-s2-mask-grad-reordered.ssimg
```

These runs established:

- `O=` writes a 52-byte header followed by six Amiga bitplanes.
- `B=` affected the smoothing used before HAM palette/register choice.
- `T=` affected the palette/register block in the sampled test.
- `R=` reused palette/register data from an existing `O=` file.
- `M=` used the supplied RGB file outside the inclusive render rectangle and raytraced inside it.
- `V` swaps the screen-axis basis vectors used by projection and is not a simple output transpose.

## Reconstructed source validation

The reconstructed ANSI C implementation in `ssg-src-authentic/` builds as the
standalone `ssg_authentic` target. At `S=2`, raw RGB output was compared against
the original Amiga binary output generated under `vamos`:

```text
robot.dat    10 differing bytes / 12000, mean absolute error 0.00442
ele.dat       4 differing bytes / 12000, mean absolute error 0.00033
dragon.dat    1 differing byte  / 12000, mean absolute error 0.00008
robot.dat V   4 differing bytes / 12000, mean absolute error 0.00800
```

Full-resolution raw RGB comparison at 320 by 200:

```text
robot.dat   233 differing bytes / 192000, mean absolute error 0.04347
ele.dat      69 differing bytes / 192000, mean absolute error 0.00098
```

The reconstructed `robot.dat` render also produces the same 48-byte
palette/register block as the original `robot-s2.ssimg`. The sampled `V` render
also produces the same 48-byte palette/register block as the original
`robot-s2-v.ssimg`.

The reconstructed source was also built with MSVC 19.44 through CMake/Ninja.
For `robot.dat`, `ele.dat`, and `dragon.dat` at `S=2`, the MSVC executable
produced byte-identical RGB dumps and `O=` files to the WSL/`gcc` executable.
The same was verified for `robot.dat` with `V`.

The reconstructed source uses C `float` arithmetic for the raytracing core,
matching the original binary's single-precision math support calls more closely
than host `double` arithmetic.

## Windows and WSL notes

`machine68k` currently crashes on this Windows Python setup before any Amiga code runs. The minimal failing Windows test is:

```text
python -u -X faulthandler -c "import machine68k; c=machine68k.cpu_type_from_str('68000'); machine68k.Machine(c, 1024)"
```

The failure is a native access violation in the CPU/Musashi initialization path.

The same constructor succeeds under WSL Linux:

```text
Machine(CPU(type=68000),Memory(ram_size_kib=1024))
```

All successful `ssg` execution so far was done under WSL.

## `ss` command line

`ss` appears to treat command-line arguments as rendered image filenames rather than as letter switches.

Current inferred behavior:

1. Each argument is inspected as a filename.
2. An argument beginning with `*` appears to reset the argument index to zero, likely a repeat/loop sentinel.
3. Arguments are checked against embedded `$` marker strings before opening.
4. Files are opened in read mode.
5. If open fails, `ss` prints:

```text
Unable to open file '%s'
```

The display text embedded in `ss` says:

```text
After each illustration is displayed,
press return to display the next.

Press escape to exit to AmigaDos

Press return to start.
```

## `ss` file format notes

The `ss` input format is the same file written by `ssg O=...`.

The file layout is:

```text
uint16_be width
uint16_be height
48 bytes palette/register data, 16 entries * 3 RGB4 nibbles
plane 0 bytes
plane 1 bytes
plane 2 bytes
plane 3 bytes
plane 4 bytes
plane 5 bytes
```

Each plane has:

```text
rowbytes * height
```

bytes, with:

```text
rowbytes = ((width + 15) / 16) * 2
```

The bitplanes are loaded into the display bitmap one plane at a time. `ss` clears 8000 bytes per plane before reading, which matches a maximum 320 by 200 Amiga low-resolution display:

```text
40 rowbytes * 200 rows = 8000 bytes
```

The generated `robot-s2.ssimg` file was accepted by `ss` under the current `vamos` stubs. The run produced no file-format error and waited in the display/input path until the host timeout.

The program includes an integrity check for embedded display text and can print:

```text
Executable file is corrupted
```

That appears to be an executable self-check, not a normal image-file error.

## Current limits and unknowns

Known:

- `ssg` is the `.dat` scene renderer.
- `ss` is the display program for rendered files.
- `D=` produces raw RGB.
- `O=` produces the `ss` display file format.
- `R=` reads palette/register data from an existing `O=`/`ss` file.
- `M=` reuses an existing RGB dump outside an inclusive render rectangle.
- `I=`, `D=`, `O=`, `R=`, `M=`, `E`, and `S=` are validated by live execution.
- `ssg` still calls Intuition/Graphics even when writing file output.
- Minimal no-op `vamos` stubs are enough for `D=` output under WSL.

Not yet fully known:

- The exact user-facing name for `V`, though its projection behavior is now recovered.
- Whether full-resolution `dragon.dat` completes under `vamos` with enough time, or whether it needs further speed work.
