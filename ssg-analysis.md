# `ssg` reverse-engineering notes

These are working notes on the Amiga 68k executable `Raytracer_1987_Graham_Source_Code/ssg`.

`ssg` is not the same program as `ss`, and it is not the same program as the modernized `rt` source. It appears to be the original **SSG: Scene Simulation Generator**, copyright 1987 Eric Graham.

This analysis is based on the Amiga HUNK executable, its strings, relocation tables, and targeted disassembly around string and library call references.

## Binary identity

File:

```text
Raytracer_1987_Graham_Source_Code/ssg
```

Size:

```text
49,992 bytes
```

SHA-256:

```text
00A576C9244548747DF36D62E660712C1A42AFD42AFF493D7215B4C62F690571
```

The executable is an Amiga HUNK file. It begins with:

```text
000003f3
```

That is `HUNK_HEADER`.

The header allocation table describes 161 hunks. Most are small runtime/library stubs or data/BSS hunks. The largest application code hunk found so far is hunk 2:

```text
hunk 2: CODE, 17,384 bytes, 647 relocations
```

The parser strings are in hunk 3:

```text
hunk 3: DATA, 996 bytes
```

## Important strings

The executable contains these identifying strings:

```text
SSG: Scene Simulation Generator
Copyright 1987 Eric Graham
```

The following strings show that `ssg` has a built-in text scene reader:

```text
No input file
input file error on '%s'
(%lf,%lf,%lf)
[%lf,%lf]
%lf
%lf,%lf,%lf> %d
(%lf,%lf,%lf):%lf
Total number of spheres=%d
Number of lamps=%d
(%lf,%lf,%lf):%lf
<%lf,%lf,%lf>
Error before:
input error
```

Those format strings line up with the known `.dat` file grammar.

## Imported/runtime symbols

Several hunks contain symbol names for Amiga library or runtime stubs.

Examples:

```text
_Open
_Close
_Read
_Write
_Input
_Output
_Seek
_DeleteFile
_Rename
_IoErr

_AllocMem
_FreeMem
_OpenLibrary
_CloseLibrary

_SetRGB4
_WritePixel
_SetAPen
_OpenScreen
_OpenWindow
_CloseScreen
_CloseWindow

_SPFix
_SPFlt
_SPCmp
_SPTst
_SPNeg
_SPAdd
_SPSub
_SPMul
_SPDiv
_SPSqrt
_SPFieee
```

The `_SP...` functions indicate use of the Amiga FFP/single-precision math runtime, with references to:

```text
mathffp.library
mathtrans.library
```

This is a notable difference from the modernized `rt` source, which currently uses C `double`.

## Calling convention

The code uses normal 68000 C stack frames:

```asm
link a6,#-locals
...
unlk a6
rts
```

Arguments are passed on the stack. The caller pushes arguments right-to-left and adjusts `sp` after the call. Return values use `d0` for integer/pointer results, with the runtime math library handling floating-point values.

The large scene parser function starts in hunk 2 at offset:

```text
hunk 2 + 0x21f6
```

It has a frame size of 246 bytes:

```asm
link a6,#-246
```

The main routine calls it by pushing three arguments:

```asm
move.l input_filename,-(sp)
pea    observer_storage
pea    world_storage
bsr.w  parser_function
```

Because C arguments are pushed right-to-left, the parser signature is probably:

```c
int read_scene(struct observer *o, struct world *w, const char *filename);
```

Inside the parser, the filename argument is used with the `"r"` mode string and an `_Open`/stdio-style call. If opening fails, it prints:

```text
input file error on '%s'
```

## Command-line convention

The startup routine begins at hunk 2 offset `0x0000`.

It walks command-line arguments and dispatches on one-letter switches. Most switches appear to use this form:

```text
X=value
```

The dispatch table contains these switch letters:

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

Observed meanings from string references and call sites:

```text
I=...   input scene file
O=...   output file, opened for writing
D=...   dump file, opened for writing
R=...   register file, opened for reading
M=...   parsed with "%d:%d:%d:%d:%s", then opens the parsed filename for reading
T=...   parsed as integer
S=...   parsed as integer
B=...   parsed as integer
V       flag switch, no "=" required
E       flag switch, no "=" required
```

The exact meanings of `E`, `V`, `B`, `S`, `M`, and `T` are not fully recovered yet.

The input filename does not appear to be accepted as a bare positional argument. The startup code checks for an input file after option parsing and prints:

```text
No input file
```

That makes this the likely minimum invocation form:

```text
ssg I=robot.dat
```

It may also require `O=...` or other output-related switches for useful operation. The strings show that it can open output, dump, and register files, but the required combination has not been proven yet.

## Parser structure

The parser at hunk 2 offset `0x21f6` reads the known `.dat` grammar.

Observed parse order:

```text
open input file
scan "(%lf,%lf,%lf)"              observer position
scan "[%lf,%lf]"                  altitude and azimuth
scan "%lf"                        focal length factor
scan repeated object definitions
scan "%d"                         number of lamps
scan lamp definitions
scan five "<%lf,%lf,%lf>" colors
```

The object loop accepts:

```text
<r,g,b> type
(x,y,z):radius
count (x,y,z):radius
...
;
```

The parser prints:

```text
Total number of spheres=%d
Number of lamps=%d
```

It allocates storage after counting spheres and lamps.

It has error recovery/reporting that prints:

```text
Error before:
```

then reads and prints up to roughly 100 following characters before calling the input error path.

## Difference from Alain Thellier's `rt-gcc`

> Note: Alain Thellier's `rt-gcc` is a separate third-party project and is **not
> included in this repository** — it is omitted for licensing reasons. The
> references below are to that external project for comparison only.

Alain Thellier's `rt-gcc/scenes.c` manually transcribes the `.dat` files into C setup functions. It is not a runtime `.dat` parser.

`ssg` is different. The format strings and parser function show that it reads `.dat`-style text at runtime.

This also means `ssg` is probably a better reference for the original `.dat` format than `rt-gcc/scenes.c`, once its parser is fully decompiled.

## Decompilation targets

The next useful decompilation targets are:

1. The startup option parser at hunk 2 offset `0x0000`.
2. The scene parser at hunk 2 offset `0x21f6`.
3. The renderer/math functions in hunk 2 between the parser and the later display/HAM routines.
4. The code that uses `O=`, `D=`, `R=`, and `M=`.

The parser is the best first C reconstruction target because its string references make the control flow relatively clear.

## Provisional parser prototype

Based on the call site and stack layout:

```c
int read_scene(struct observer *o, struct world *w, const char *filename);
```

The exact return value meaning is not yet confirmed.

## Current open questions

- Does `ssg` render directly, generate another executable, or both?
- What are the exact meanings of `E`, `V`, `B`, `S`, `M`, and `T`?
- Does `M=%d:%d:%d:%d:%s` define a mask rectangle and file?
- What output file format does `O=` write?
- Does the parser's sphere interpolation include or exclude segment endpoints?
- Does `ssg` use a different mirror/reflection algorithm from `rt1.c`?

