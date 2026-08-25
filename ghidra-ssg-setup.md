# Ghidra setup for ss and ssg

This repository has a local Ghidra project prepared for reverse engineering the original Amiga executables `ss` and `ssg`.

## Installed tools

Ghidra 12.1 is installed at:

```text
C:\Data\OtherDev\ghidra_12.1_PUBLIC_20260513\ghidra_12.1_PUBLIC
```

The Amiga loader/analyzer extension from:

```text
C:\Data\OtherDev\ghidra_12.1_PUBLIC_20260513\ghidra_12.0.1_PUBLIC_20260128_ghidra-amiga.zip
```

was installed into:

```text
C:\Data\OtherDev\ghidra_12.1_PUBLIC_20260513\ghidra_12.1_PUBLIC\Ghidra\Extensions\Ghidra\ghidra-amiga
```

The extension metadata was updated from `version=12.0.1` to `version=12.1` so Ghidra 12.1 accepts it.

## Java and settings

Ghidra is configured to use:

```text
C:\Users\xenon\AppData\Local\Programs\Eclipse Adoptium\jdk-25.0.3.9-hotspot
```

The original Ghidra launcher settings file was backed up as:

```text
C:\Data\OtherDev\ghidra_12.1_PUBLIC_20260513\ghidra_12.1_PUBLIC\support\launch.properties.codex-bak
```

The active `support\launch.properties` uses a repository-local settings directory:

```text
C:\Data\OtherDevDev\Eric-Graham-1987-Juggler-Raytracer-1.0\out\ghidra_settings
```

## Project

The Ghidra project is:

```text
C:\Data\OtherDevDev\Eric-Graham-1987-Juggler-Raytracer-1.0\out\ghidra\JugglerRE
```

It contains imported programs for:

```text
Raytracer_1987_Graham_Source_Code\ssg
Raytracer_1987_Graham_Source_Code\ss
```

The reverse-engineering target of interest is `ssg`; `ss` was imported only to verify that the Amiga plugin handles both original executables.

## Verification

Both executables were imported and analyzed successfully with Ghidra headless. The log reports:

```text
Using Loader: Amiga Hunk Executable
Using Language/Compiler: 68000:BE:32:default:default
Creating custom chips memory block
Amiga Library Calls
Analysis succeeded
```

This verifies that the Amiga Hunk loader, 68000 big-endian language, Amiga data archives, and Amiga library-call analyzer are operational.

## Re-running the imports

From the repository root, the imports can be reproduced with:

```bat
C:\Data\OtherDev\ghidra_12.1_PUBLIC_20260513\ghidra_12.1_PUBLIC\support\analyzeHeadless.bat out\ghidra JugglerRE -import Raytracer_1987_Graham_Source_Code\ssg -overwrite -analysisTimeoutPerFile 120
```

```bat
C:\Data\OtherDev\ghidra_12.1_PUBLIC_20260513\ghidra_12.1_PUBLIC\support\analyzeHeadless.bat out\ghidra JugglerRE -import Raytracer_1987_Graham_Source_Code\ss -overwrite -analysisTimeoutPerFile 120
```

## Next reverse-engineering goals

The immediate target is command-line behavior for `ssg`: option letters, required arguments, default output behavior, and the scene file path convention. After that, the useful next pass is identifying the scene parser and renderer routines by comparing their data flow and math calls against the surviving `rt*.c` source.
