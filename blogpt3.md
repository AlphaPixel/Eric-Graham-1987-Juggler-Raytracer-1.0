# Eric Graham's 1987 Amiga Juggler raytracer (Part 3): Reconstructing SSG

In Part 1 of this series, I recovered the original Amiga disk image, extracted the source, and got Eric Graham's permission to publish it.

In Part 2, I forward-ported the surviving C raytracer enough to build and run on a modern machine, using SDL as a small replacement for the Amiga display calls.

That got the published `rt1.c`, `rt2.c`, and `rt3.c` source running again, but did not recover the actual scene renderer that reads `robot.dat`, `dragon.dat`, and `ele.dat`. We had the executable, but no documentation survived on how to operate it.

The final stage (isn't the third part of a trilogy always the best or worst? Looking at you RoTK and Alien 3) was narrower and MUCH more difficult: reconstruct the missing `ssg` source from the original Amiga 68000 executable. In a reasonable universe, this is basically impossible. Eric claims he no longer has it. But, in 2026 (a very unreasonable place) can it be done?

## The missing links

The recovered disk includes two related Amiga executables:

`ss` identifies itself as:

```text
SS: Ray Tracing Display Program
Copyright 1987 Eric Graham
```

`ssg` identifies itself as:

```text
SSG: Scene Simulation Generator
Copyright 1987 Eric Graham
```

The first one is just the slideshow viewer. I don't care to focus on that, there isn't any secret sauce of value there.

The surviving `rt` C source draws a simple one-sphere scene. The disk also includes data files named `robot.dat`, `dragon.dat`, and `ele.dat`. Those files clearly describe more complex scenes (including the famous 'Juggler' robot), and the recovered still images prove those scenes were rendered at some point. But the published `rt` source does NOT contain the parser or renderer path needed to turn those `.dat` files into images.

`ssg` is the missing *executable-only* program. It's what really produced the Juggler animation that made the Amiga explode as a graphics computer. But, the documentation for it and the source, were long ago lost (according to Eric).

String analysis of the Amiga executable shows the executable contains parser format strings for vectors, angles, sphere positions, radii, colors, lamp counts, and error messages such as:

```text
No input file
input file error on '%s'
Total number of spheres=%d
Number of lamps=%d
```

## Why vamos (and not UAE)

While I own Amiga Forever and licensed ROM files, I needed to run the original 68k binary, preferably under automation, and preferably without dragging a full interactive Amiga desktop (WinUAE or Amiga Forever) into the loop every time I wanted one test image. I discovered `vamos`, from the `amitools` project. Luckily, despite being a graphics-renderer, this is not a GUI-heavy program.

`vamos` is useful here because it can run Amiga command-line programs in a host environment (using an m68k emulator). It does not require booting a full Workbench session to test a single tool. It also gives enough visibility into file I/O, command-line arguments, and library calls to make reverse engineering more interactive.

There *is* a catch, naturally.

`ssg` is a command-line renderer, but it is still an Amiga program with a preview GUI. Even when asked to write file output, it opens Amiga graphics and Intuition objects (so it can do a preview display). It non-optionally calls things like:

```text
OpenScreen
OpenWindow
SetPointer
SetRGB4
SetAPen
WritePixel
CloseWindow
CloseScreen
```

For this phase I needed `ssg` to believe the window existed long enough to parse the scene, compute the raytraced pixels, and write the raw RGB dump for validation.

So the practical answer was to stub enough Amiga graphics that the renderer could get back to doing math (ever so s l o w l y).

## Call me Stubby

The local `vamos` setup was extended with small no-op Intuition and Graphics stubs. It's really a cool capability of vamos.

The stubs returned *plausible* objects from `OpenScreen()` and `OpenWindow()`, accepted palette and pen calls, accepted `WritePixel()`, and let the program clean up normally. And mostly blindly ignored and discarded any data passed to them. There was also enough console handling for the program's final wait behavior to be bypassed with the recovered `E` option. Crucially, the stubs pretty much don't DO anything. They just plausibly *pretend* like they did, sufficient to keep ssg ignorant about the minimal emulation context so it keeps going. It doesn't KNOW that nothing is displaying, so it keeps calculating and writing data to disk.

My first goal was to get `D=...` output working.

The minimum useful run looks like this:

```text
ssg E I=robot.dat D=robot.rgb
```

The `E` option suppresses the final wait-for-input behavior. `I=` supplies the scene file. `D=` writes raw 24-bit RGB bytes.

A full render produced:

```text
320 * 200 * 3 = 192000 bytes
```

That was the first really useful validation point. It meant the original binary could still render a scene file, and it could emit a simple byte dump that did not require decoding HAM or reconstructing an Amiga bitmap.

## Finding the command line arg template

The `ssg` command line is minimal. Later Amiga tools used more verbose template style like FOO=blah BAR=100, etc. The `ssg` single-letter argument template options are:

```text
O D R M T S B V I E
```

Most switches use the `X=value` form. `E` and `V` are flag switches.

The useful options are:

`I=filename` [Input] reads a `.dat` scene file.

`D=filename` [D for...Data?] writes raw RGB output.

`O=filename` [Output] writes the file format read by `ss`.

`R=filename` [Register] seeds the HAM palette/register data from an existing `ss`-style output file.

`M=x0:x1:y0:y1:filename` [Mask] reuses an existing RGB dump outside an inclusive render rectangle and raytraces inside it. This permits selective refinement.

`S=n` [Subsample] sets the sampling step through a small 2^n table:

```text
0 -> 1
1 -> 2
2 -> 4
3 -> 8
```

`T=n` [Threshold] changes the palette/register allocation threshold used by the HAM output path.

`B=n` [Blend] changes the smoothing amount before HAM palette selection.

`V` [V = uh, V?] selects an alternate projection path. The exact original user-facing name is still unknown, but I think it might be something to do with "V"ertical.

That switch list came from a mix of disassembly, string references, live execution, and controlled output comparisons. The sampled renders made testing much faster:

```text
ssg E S=2 I=robot.dat D=robot-s2.rgb O=robot-s2.ssimg
```

With `S=2`, the render step is 4, so a 320 by 200 image becomes 80 by 50.

## The scene file

The `.dat` files are text scene descriptions.

The parser reads:

* observer position
* view angles
* focal length factor
* repeated sphere/object definitions
* lamp count
* lamp definitions
* ground, illumination, and sky colors

The recovered scene counts are:

```text
robot.dat    79 spheres, 1 lamp
ele.dat     120 spheres, 1 lamp
dragon.dat  288 spheres, 1 lamp
```

The object format is compact. An object starts with a color and type, then a first sphere position and radius. Later points interpolate additional spheres between control points. That is how the data files describe continuous-looking forms using chains of overlapping spheres, which is really pretty clever.

This matched the general shape of the surviving `rt` source. The old raytracer already knows about spheres, lamps, a ground plane, sky colors, dull and bright materials, and mirror reflection. `ssg` adds the missing runtime scene parser and more complete output paths.

## Ghidra and the old source

The reverse engineering work used Ghidra with an Amiga HUNK loader and 68000 big-endian analysis ( https://github.com/BartmanAbyss/ghidra-amiga ).

The executable is not a flat blob. It is an Amiga HUNK program with code, data, relocation records, runtime stubs, and library references. Amazingly, Ghidra could import it as a 68000 big-endian program and identify many of the Amiga calls.

Some useful hints came from library references. The binary references the old Amiga math runtime:

```text
mathffp.library
mathtrans.library
SPAdd
SPSub
SPMul
SPDiv
SPSqrt
```

That indicated single-precision floating-point behavior, which is backed by Eric's original programming notes. The surviving modernized `rt` code used `double`, but the reconstructed `ssg` source uses C `float` for the raytracing core.

The surviving `rt1.c` source was still valuable. It provided names, structure, and intent for the core raytracing functions:

```text
raytrace
pixline
intsplin
inthor
pixbrite
glint
mirror
skybrite
gingham
reflect
```

Where the binary matched that source, the reconstructed code follows the old structure and comments. Where `ssg` had code that was not present in `rt`, such as the `.dat` parser, sampled render loop, mask path, RGB dump, and HAM file writer, the reconstruction does its best.

The point is to recover the likely original program appearance closely enough that the source remains useful as historical source, instead of producing a modern approximation that happens to draw similar pictures.

## Rebuilding SSG in C

The reconstructed source lives under `ssg-src-authentic/`.

It is split into a few modules that follow Eric's own `rt` file structure:

```text
ssg.c        command line, the .dat scene reader, render loop, program entry
raytrace.c   raytracing core + all the vector math (mirrors rt1.c)
ham.c        HAM6 palette/bitplane encoder and ss image writer (mirrors rt3.c)
ssg.h        shared declarations (mirrors rt.h)
```

The CMake target is:

```text
ssg_authentic
```

The reconstructed `ssg` target is a command-line renderer and image writer. I elected NOT to try to extend it with SDL the way I had with the modernized rt code.

A typical run is:

```text
ssg_authentic E S=2 I=robot.dat D=robot-s2.rgb O=robot-s2.ssimg
```

The `O=` writer emits the recovered `ss` image container:

```text
uint16_be width
uint16_be height
48 bytes of RGB4 palette/register data
six Amiga bitplanes
```

The bitplane rows use the Amiga-style row stride:

```text
rowbytes = ((width + 15) / 16) * 2
```

For an 80 by 50 sampled render, the file is:

```text
52 + 10 * 50 * 6 = 3052 bytes
```

That matches the original format used by `ss`.

## Validation

The validation process was simple: run the original unmodified `ssg` binary under `vamos` 68k emulation, capture raw RGB output, run the reconstructed C program, and compare the bytes.

At `S=2`, the current comparisons are:

```text
robot.dat    10 differing bytes / 12000, mean absolute error 0.00442
ele.dat       4 differing bytes / 12000, mean absolute error 0.00033
dragon.dat    1 differing byte  / 12000, mean absolute error 0.00008
robot.dat V   4 differing bytes / 12000, mean absolute error 0.00800
```

Full-resolution comparisons:

```text
robot.dat   233 differing bytes / 192000, mean absolute error 0.04347
ele.dat      69 differing bytes / 192000, mean absolute error 0.00098
```

Those are close enough that remaining differences are now in the weeds: single-precision behavior, library math details, and a few edge cases in reconstructed formulas. I don't think I can get any closer.

The reconstructed code also builds with MSVC through CMake/Ninja and with `gcc` under WSL. The MSVC and `gcc` builds produce byte-identical sampled outputs for the tested scenes.

The repository has a validation helper:

```text
python tools/validate-ssg.py
```

It checks the current RGB differences, output dimensions, file sizes, and palette/register expectations.

## ~~Paradise~~ Juggler Lost, and Found.

We now have a *reconstructed* source version of the `SSG: Scene Simulation Generator`, built from the original 1987 Amiga executable, checked against the original binary, and tied back to the published raytracer source wherever the implementation clearly overlaps. It can read the original scene files:

```text
robot.dat
ele.dat
dragon.dat
```

It can write raw RGB for modern inspection, and it can write the recovered `ss` display format. This gives the old data files a working source-level renderer again.

## Unknowns

The user-facing name of the `V` switch is still unknown.

The reconstructed source is intended to build on modern compilers. It is not yet verified as source that can be compiled back into an Amiga 68k executable with a modern m68k compiler and Amiga SDK. I might try this sometime with a little help from my friends.

For now, the important part is that the lost Juggler ssg renderer is no longer only an undocumented binary. We now have documentation to successfully run it, and a C representation of what it originally did.

## Coda

The actual scene files for the Juggler frames themselves were *also* lost. Eric said he had written a tool that generated each frame as an individual .dat file and rendered those and then assembled the final animation. Those dat files are lost, but some kind of machine learning and computer vision approach might be able to reconstruct the original scene geometry and motion paths from the final output frame images. Maybe I'll tackle that sometime in the future.

