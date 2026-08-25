# `ss` and `ssg` reverse-engineering notes

This note compares the original Amiga executables:

```text
Raytracer_1987_Graham_Source_Code\ss
Raytracer_1987_Graham_Source_Code\ssg
```

The current evidence comes from Ghidra 12.1 with the Amiga Hunk loader, exported decompiler reports, forced-disassembly of the command-line startup areas, and raw byte dumps around the short embedded strings.

Generated working files:

```text
out\ghidra_exports\ss-report.md
out\ghidra_exports\ssg-report.md
out\ghidra_exports\ss-start-asm.txt
out\ghidra_exports\ssg-start-asm.txt
out\ghidra_exports\ssg-options-asm.txt
out\ghidra_exports\ss-option-bytes.txt
out\ghidra_exports\ssg-option-bytes.txt
out\ghidra_exports\ssg-data-bytes.txt
```

## High-level difference

`ssg` identifies itself as:

```text
SSG: Scene Simulation Generator
Copyright 1987 Eric Graham
```

It contains the text scene parser and the raytracing loop. Its strings include the known `.dat` grammar:

```text
(%lf,%lf,%lf)
[%lf,%lf]
%lf,%lf,%lf> %d
(%lf,%lf,%lf):%lf
<%lf,%lf,%lf>
Total number of spheres=%d
Number of lamps=%d
```

`ss` identifies itself as:

```text
SS: Ray Tracing Display Program
Copyright 1987 Eric Graham
```

It does not contain the `.dat` parser strings. It reads already-rendered image files, opens an Amiga display, loads a palette and per-bitplane image data, displays each illustration, and waits for return between files. The embedded text says:

```text
After each illustration is displayed,
press return to display the next.

Press escape to exit to AmigaDos

Press return to start.
```

Current assessment: `ssg` is important for reconstructing the renderer and command-line control of scene rendering. `ss` is important for reconstructing the output file format that `ssg` writes with `O=...`, but it does not appear to be needed for `.dat` scene parsing or raytracing logic.

## `ss` command-line behavior

`ss` does not appear to use letter switches. It treats arguments as image filenames.

The startup routine checks each argument:

1. If the first character is `*`, it resets the argument index to zero. This looks like a repeat/loop sentinel.
2. Otherwise it compares the argument suffix or marker string against the embedded short string `$`.
3. If the comparison succeeds, it opens the filename in read mode using the embedded short string `r`.
4. If open fails, it prints:

```text
Unable to open file '%s'
```

The raw byte dump around the short strings is:

```text
0021f87c  24 00 24 00 72 00 ...   $.$.r.
```

This means `ss` is looking for `$`-marked files. The two `$` strings are used by startup checks before opening the file.

After opening a file, `ss` reads:

1. A 16-bit width.
2. A 16-bit height.
3. A 48-byte palette, matching 16 colors with 3 nibbles/channels each.
4. Six bitplane-like image sections, one section per plane.

It checks the display program text blocks with a small checksum/hash routine and reports:

```text
Executable file is corrupted
```

if the embedded text does not match the expected values. That appears to be an integrity check, not a user-facing file format error.

## `ssg` command-line syntax

`ssg` parses one-letter arguments. Most are of the form:

```text
X=value
```

Two flags, `E` and `V`, are accepted without `=`.

If an argument is neither `X=value` nor an accepted flag, it prints:

```text
command line error %s
```

If the switch letter is not in the dispatch table, it prints:

```text
Illegal switch %s
```

The switch dispatch table is:

```text
E
I
V
B
S
M
T
R
D
O
```

If no input scene file was supplied through `I=...`, it prints:

```text
No input file
```

So the minimum scene-rendering form is expected to be:

```text
ssg I=robot.dat
```

Useful output probably requires at least `O=...` if a reusable rendered file is desired.

## `ssg` recovered options

`I=filename`

Sets the input scene filename. This is the file passed to the `.dat` parser. This option is required.

`O=filename`

Opens the output file in write mode. If the open fails, it prints:

```text
Unable to open output file '%s'
```

This is probably the main rendered image output consumed by `ss`.

`D=filename`

Opens a dump file in write mode. If the open fails, it prints:

```text
Unable to open dump file '%s'
```

During rendering, this file receives one byte per RGB component before final HAM/palette plotting. The code writes three bytes per pixel when this file is active.

`R=filename`

Opens a register file in read mode. If the open fails, it prints:

```text
Unable to open register file '%s'
```

The handler reads a width, a height, and 48 bytes of palette/register data, then loads those colors into the display palette. This looks like a way to reuse a previous palette/register state before rendering.

`M=x0:y0:x1:y1:filename`

Parses four integers and a filename with:

```text
%d:%d:%d:%d:%s
```

Then opens the parsed filename in read mode. During rendering, if the current pixel is outside the rectangle, `ssg` reads RGB bytes from this file instead of raytracing that pixel and prints:

```text
masked
```

This appears to be a rectangular mask/reuse option.

`S=n`

Parses an integer, clamps it to `0..3`, and maps it through this table:

```text
0 -> 1
1 -> 2
2 -> 4
3 -> 8
```

The resulting value is used as the pixel step in both the X and Y render loops. This is probably a sampling/skip factor for faster preview renders.

`T=n`

Sets the color-distance threshold used when deciding whether to add a new palette entry. The default value at startup is `4`. The palette starts with two entries, black and white, and may grow up to 16 colors.

`V`

Sets a global flag that changes X/Y projection orientation in several math routines. This may be a vertical/transposed rendering mode. The exact user-facing meaning is not fully named yet.

`E`

Clears the flag that otherwise waits for input near the end of rendering. This appears to suppress the final wait-for-key behavior after the render completes.

`B=n`

Parses an integer into a global initialized to `3`. The exact effect is not yet recovered. It may control bit depth, bitplanes, or output packing, but the current decompiler report only shows the initialization and option assignment, not a clean use site.

## Headless execution / `vamos` feasibility

The current `ssg` executable does not appear to have a command-line mode that avoids Intuition and Graphics.

After parsing arguments and reading the `.dat` file, startup unconditionally calls the display initializer at `FUN_00224604`. That function opens:

```text
graphics.library
intuition.library
dos.library
```

and then calls:

```text
OpenScreen
OpenWindow
SetRGB4
SetPointer
CreatePort
CreateStdIO
OpenDevice("console.device")
```

The render loop then calls the HAM/display plotting function for every pixel:

```text
FUN_00222e96(local_fc, local_100, aiStack_18c)
```

That function eventually calls:

```text
SetAPen
WritePixel
```

This happens even when file output options are active:

```text
O=filename   writes the final `ss`-readable image format
D=filename   writes per-channel RGB bytes
R=filename   reads palette/register data
M=...        reads RGB bytes for masked/reused regions
```

Those options add file I/O, but they do not bypass display creation or display plotting.

Current assessment: the unmodified `ssg` binary is unlikely to run under a CLI-oriented Amiga compatibility layer unless that layer provides enough `graphics.library`, `intuition.library`, `console.device`, screen/window, viewport, palette, and raster-port behavior for these calls to succeed. A minimal DOS/math-only environment is not enough.

There are still two possible lower-level paths:

1. Patch the binary so the call to the display initializer is bypassed and `FUN_00222e96` no-ops or writes only to file-backed buffers.
2. Provide stubs for the required graphics/intuition calls that return plausible objects and accept `SetAPen`/`WritePixel` calls.

Both are reverse-engineering projects in their own right. For immediate behavioral testing, an emulator that can run the real Amiga libraries remains the lower-risk path.

## Short embedded strings in `ssg`

The raw option-string cluster confirms the tiny strings Ghidra did not list as full strings:

```text
0022372a  "%s"
0022372d  "w"
00223750  "w"
00223771  "r"
00223796  "%d"
00223799  "%d:%d:%d:%d:%s"
002237a8  "r"
002237aa  "%d"
002237ad  "%d"
```

These match the option handlers:

```text
O uses "w"
D uses "w"
R uses "r"
M uses "%d:%d:%d:%d:%s", then "r"
T uses "%d"
S uses "%d"
B uses "%d"
```

## Next disassembly targets

For `ssg`, the most useful next targets are:

1. Rename the startup locals/globals in Ghidra using the recovered option meanings.
2. Rename the stdio-like runtime functions:
   - `FUN_00224f28`: fopen-like
   - `FUN_0022542c`: fread-like
   - `FUN_0022567c`: fwrite-like
   - `FUN_002254a8`: fscanf-like
3. Re-export decompilation after names are applied.
4. Reconstruct the `.dat` parser at `FUN_0022141e`.
5. Reconstruct the render loop in `FUN_0021f228`.
6. Reconstruct the `O=` output format by comparing `ssg` writes with `ss` reads.

For `ss`, the best target is the image-file reader. It should be decompiled enough to document the exact file format written by `ssg O=...`.

## `raytrace.a` and `raytrace.BAK`

There is no `raytrace.as` file in the extracted directory. The relevant files are:

```text
Raytracer_1987_Graham_Source_Code\raytrace.a
Raytracer_1987_Graham_Source_Code\raytrace.BAK
```

Both are plain text AmigaBASIC source files. They are not assembler source and not object files.

`raytrace.BAK` is an older backup of the same BASIC source. A file diff shows no program logic changes between `raytrace.BAK` and `raytrace.a`; `raytrace.a` only adds a 12-line `REM` copyright/license preamble at the top.

The BASIC source is meaningful for reverse engineering, but mainly as an ancestor/reference for the published `rt*.c` source, not as a separate executable target. Its routines map closely to the C implementation:

```text
raytrace       -> rt1.c raytrace()
skybrite       -> rt1.c skybrite()
pixline        -> rt1.c pixline()
intsplin       -> rt1.c intsplin()
qintsplin      -> rt1.c qintsplin()
inthor         -> rt1.c inthor()
genline        -> rt1.c genline()
dot            -> rt1.c dot()
getpoint       -> rt1.c point()
glint          -> rt1.c glint()
mirror         -> rt1.c mirror()
pixbrite       -> rt1.c pixbrite()
setnorm        -> rt1.c setnorm()
gingham        -> rt1.c gingham()
reflect        -> rt1.c reflect()
vecprod        -> rt1.c vecprod()
ham / ham2     -> rt3.c HAM plotting path
nearestp       -> rt3.c nearestp()
coldist        -> rt3.c coldist()
coldist2       -> rt3.c coldist2()
setup          -> rt2.c setup()
```

The BASIC source can help in several concrete ways:

1. It confirms the historical structure of the renderer before the C version was factored into `rt1.c`, `rt2.c`, and `rt3.c`.
2. It gives another readable version of the HAM color allocation path, including the initial black/white palette, threshold value, and hold-and-modify channel mapping.
3. It may help name or sanity-check decompiled `ssg` functions because `ssg` still has routines corresponding to ray generation, sphere intersection, sky color, horizon color, reflections, and HAM/palette output.
4. It is less useful for command-line reverse engineering because it has no command-line parser, no `.dat` parser, and no `O=`, `D=`, `R=`, or `M=` file behavior.

Current assessment: keep `raytrace.a` as a high-value readability reference for renderer math and HAM behavior. `raytrace.BAK` does not add additional technical content beyond proving that the later `.a` file only added the license header.
