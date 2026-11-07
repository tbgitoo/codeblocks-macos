# OGRE-Next on Windows with Code::Blocks MinGW: Manual Validation Findings

**Date:** 2026-11-07  
**Status:** active investigation; dependency build not yet fully passing  
**Purpose:** preserve the exact findings from manual validation so they can later be converted into a rigorous Cursor implementation plan for the Code::Blocks v3.9 OGRE-Next wizard.

## 1. What is being validated

The target is deliberately not "any MinGW that can build OGRE-Next." The target is the **MinGW GCC toolchain selected by Code::Blocks**, because the future wizard must generate and build projects with that same compiler.

Current Code::Blocks GNU GCC installation:

```text
D:\Program Files\CodeBlocks\MinGW
```

Relevant executables:

```text
D:\Program Files\CodeBlocks\MinGW\bin\gcc.exe
D:\Program Files\CodeBlocks\MinGW\bin\g++.exe
D:\Program Files\CodeBlocks\MinGW\bin\ar.exe
D:\Program Files\CodeBlocks\MinGW\bin\mingw32-make.exe
```

The machine contains several other GCC/MinGW installations. Therefore, simply requiring `gcc`, `g++`, and `mingw32-make` to exist on `PATH` is insufficient.

The validated shell setup is conceptually:

```bat
set "CB_MINGW=D:\Program Files\CodeBlocks\MinGW"
set "PATH=%CB_MINGW%\bin;%PATH%"
set "CC=%CB_MINGW%\bin\gcc.exe"
set "CXX=%CB_MINGW%\bin\g++.exe"
```

`where gcc`, `where g++`, `where ar`, and `where mingw32-make` must all resolve first to the Code::Blocks MinGW installation.

---

## 2. Finding 1: SDL2 DLL naming mismatch

### Failure

The OGRE-Next dependencies build failed in the generated SDL2 `MoveInstallFiles` step while attempting approximately:

```text
cmake -E rename bin/libSDL2.dll bin/Debug/libSDL2.dll
```

Inspection showed that SDL2 had actually built and installed successfully, but produced:

```text
SDL2.dll
libSDL2.dll.a
```

There was no `libSDL2.dll`.

### Source

In:

```text
ogre-next-deps\src\CMakeLists.txt
```

the MinGW branch contains:

```cmake
if (MINGW)
  list(APPEND SDL2_CMAKE_ARGS "-DDIRECTX=0")
  set(SDL2_BIN_NAME "libSDL2.dll")
```

### Working correction

Change:

```cmake
set(SDL2_BIN_NAME "libSDL2.dll")
```

to:

```cmake
set(SDL2_BIN_NAME "SDL2.dll")
```

This allowed the dependency build to proceed beyond SDL2.

### Implication for later automation

The wizard must either use a dependency revision in which this is fixed or apply a controlled, explicit compatibility patch. It must not silently rename arbitrary files after a failed build.

---

## 3. Finding 2: multiple MinGW installations caused a mixed toolchain

### Failure

An early shaderc/SPIRV-Tools build reached the linker but failed mysteriously. Inspection of the generated build revealed tools from different MSYS2 environments being mixed, including approximately:

```text
D:\msys64\mingw64\bin\ar.exe
D:\msys64\ucrt64\bin\gcc.exe
```

The shaderc CMake cache showed inconsistent compiler/tool selections.

### Cause

The machine has multiple GCC distributions on `PATH`, including MSYS2 MINGW64, MSYS2 UCRT64, and the Code::Blocks compiler. Environment/compiler selection was therefore ambiguous.

### Resolution

For the actual Code::Blocks validation, the authoritative compiler is **the compiler configured in Code::Blocks**, not an arbitrary MSYS2 environment.

The future wizard must resolve all relevant tools from one selected Code::Blocks compiler installation and must give nested CMake builds a controlled environment.

### Important design requirement

Do not implement the wizard as:

```text
find gcc on PATH
find g++ on PATH
find make on PATH
```

Instead, derive the toolchain root from the Code::Blocks compiler selected for the project and resolve `gcc.exe`, `g++.exe`, `ar.exe`, and `mingw32-make.exe` coherently from that installation.

---

## 4. Finding 3: recursive Make fails with the normal Code::Blocks path containing spaces

### Failure

With the correct Code::Blocks compiler selected, CMake correctly identified:

```text
C compiler:   D:/Program Files/CodeBlocks/MinGW/bin/gcc.exe
CXX compiler: D:/Program Files/CodeBlocks/MinGW/bin/g++.exe
```

However, the shaderc ExternalProject failed at its build step with:

```text
'D:/Program' is not recognized as an internal or external command
```

The generated dependency build invoked recursive Make via:

```make
$(MAKE)
```

and the CMake cache contained:

```text
CMAKE_MAKE_PROGRAM:FILEPATH=D:/Program Files/CodeBlocks/MinGW/bin/mingw32-make.exe
```

The absolute executable path containing `Program Files` was not surviving the recursive invocation correctly.

### Working correction

Put the selected Code::Blocks MinGW `bin` directory first on `PATH`, but configure CMake with the executable name rather than the absolute space-containing path:

```bat
cmake -S ogre-next-deps -B ogre-next-deps\build ^
  -G "MinGW Makefiles" ^
  -DCMAKE_BUILD_TYPE=Release ^
  -DCMAKE_MAKE_PROGRAM=mingw32-make.exe
```

This allowed shaderc to proceed.

### Implication for the wizard

The wizard should construct a controlled process environment with the selected compiler's `bin` directory first on `PATH`. For recursive Make, using `mingw32-make.exe` through that controlled `PATH` is safer than embedding the normal Code::Blocks absolute path underneath `Program Files`.

---

## 5. Finding 4: FreeImage/LibJXR `guiddef.h` collision with MinGW-w64

### Initial failure

The build later failed in the MinGW-w64 Windows headers (`unknwnbase.h`) with errors around:

```text
__mingw_uuidof
__mingw_uuidof_s
```

### Investigation

FreeImage's bundled LibJXR contains:

```text
Source\LibJXR\common\include\guiddef.h
```

`JXRGlue.h` contains:

```cpp
#include <guiddef.h>
```

FreeImage's CMake setup globally adds all bundled sublibrary include directories, including:

```cmake
include_directories(Source/LibJXR/common/include)
```

Therefore the LibJXR directory, containing its private `guiddef.h`, is globally visible while ordinary FreeImage sources include Windows/MinGW headers.

This is consistent with a previously reported FreeImage/MinGW-w64 problem in which the JXR-provided `guiddef.h` conflicted with the MinGW headers.

### Experiment A: remove the LibJXR common include directory

Commenting out:

```cmake
include_directories(Source/LibJXR/common/include)
```

allowed compilation to get farther, but eventually `PluginJXR.cpp` failed because LibJXR genuinely requires headers from that directory, for example:

```text
wmspecstring.h: No such file or directory
```

Therefore the include directory cannot simply be removed.

### Experiment B: keep the directory but temporarily hide its `guiddef.h`

The include directory was restored, and the bundled file was temporarily renamed:

```text
guiddef.h -> guiddef.h.disabled
```

The intention was to retain LibJXR-specific headers such as `wmspecstring.h` while letting `#include <guiddef.h>` resolve to MinGW-w64's own system header.

This got substantially farther, demonstrating that the original collision had been bypassed. However, compilation later exposed another independent LibJXR portability problem described below.

### Current interpretation

The `guiddef.h` collision is real, but the final production-quality solution is not yet decided. A future Cursor plan must not treat the temporary rename as the final patch without further validation.

---

## 6. Finding 5: FreeImage forces contradictory LibJXR platform modes

### New failure

With the bundled `guiddef.h` bypassed, compilation progressed to approximately 26% and then failed in LibJXR with:

```text
CT_ASSERT(sizeof(UINTPTR_T) == sizeof(void*), strcodec1)
CT_ASSERT(sizeof(INTPTR_T) == sizeof(void*), strcodec2)
```

The compiler warnings immediately before the assertions showed:

```text
"FORCE_INLINE" redefined
"UINTPTR_T" redefined
"INTPTR_T" redefined
```

### Relevant LibJXR logic

`strcodec.h` selects the x86 Windows header when `WIN32` is defined:

```c
#if defined(WIN32) && !defined(UNDER_CE)
#define PLATFORM_X86
#include "../x86/x86.h"
#endif
```

Later it independently selects the ANSI implementation:

```c
#ifdef __ANSI__
#define PLATFORM_ANSI
#include "ansi.h"
#endif
```

The x86 header defines pointer-width types appropriately:

```c
#define UINTPTR_T uintptr_t
#define INTPTR_T intptr_t
```

but `ansi.h` subsequently redefines them as:

```c
#define UINTPTR_T unsigned int
#define INTPTR_T int
```

On a 64-bit build, `int`/`unsigned int` are 32-bit while `void*` is 64-bit, so LibJXR's own compile-time assertions correctly fail.

### Why both modes are active

FreeImage's CMake configuration explicitly contains:

```cmake
add_definitions("-D__ANSI__=1")
```

at approximately line 952 of:

```text
ogre-next-deps\src\FreeImage\CMakeLists.txt
```

Thus a Windows MinGW build is simultaneously selecting LibJXR's Windows/x86 path and its ANSI path.

### Current experiment

The next controlled test is to comment out only:

```cmake
# add_definitions("-D__ANSI__=1")
```

while otherwise preserving the current experiment, then reconfigure and continue the build.

This result is **not yet known at the time of writing**.

---

## 7. Things deliberately not done

Several tempting workarounds were rejected because they would hide causality or damage reproducibility:

- Do not edit generated `*-impl.cmake`, `build.make`, or other generated build files as the real fix.
- Do not manually copy `SDL2.dll` into the filename expected by a stale script and call the problem solved.
- Do not accept a build that silently uses a different GCC than Code::Blocks.
- Do not mix MSYS2 MINGW64, UCRT64, Code::Blocks MinGW, or other GCC installations.
- Do not patch MinGW-w64's installed `unknwnbase.h` to invent missing UUID macros.
- Do not enable Windows 8.3 filenames merely to work around `Program Files`.
- Do not relocate the Code::Blocks installation merely to make dependency scripts happier.
- Do not remove the LibJXR include directory globally, because LibJXR demonstrably needs headers from it.
- Do not yet treat renaming LibJXR's `guiddef.h` as a final solution.

The purpose of manual validation is to establish causality before encoding fixes into the wizard.

---

## 8. What has actually been demonstrated so far

The investigation has established the following sequence:

```text
Code::Blocks MinGW toolchain selected coherently
        |
        v
SDL2 DLL naming mismatch found and corrected
        |
        v
mixed-toolchain contamination identified and eliminated
        |
        v
recursive mingw32-make Program Files quoting problem identified
        |
        v
shaderc proceeds with controlled PATH / make invocation
        |
        v
FreeImage/LibJXR guiddef.h collision identified
        |
        v
guiddef.h bypass permits substantially more FreeImage compilation
        |
        v
LibJXR Windows + __ANSI__ contradictory platform selection exposed
        |
        v
current test: remove FreeImage's forced __ANSI__ definition
```

This is important: these are not a single cascading compiler error presented in different forms. They are multiple independent compatibility assumptions exposed sequentially as earlier blockers are removed.

---

## 9. Requirements this creates for the eventual Cursor/wizard plan

A future implementation plan should preserve these principles:

1. **Selected Code::Blocks compiler is authoritative.**
   - Resolve the toolchain from Code::Blocks configuration.
   - Do not rely on whichever GCC happens to be globally first on `PATH`.

2. **Use one coherent MinGW toolchain.**
   - `gcc.exe`, `g++.exe`, `ar.exe`, and `mingw32-make.exe` must belong to the same installation.

3. **Control the child-process environment.**
   - Put the selected compiler's `bin` first on `PATH`.
   - Ensure nested CMake ExternalProjects inherit that environment.

4. **Handle paths with spaces intentionally.**
   - The standard Code::Blocks location under `Program Files` must work.
   - Do not require users to reinstall Code::Blocks into a special path.

5. **Compatibility patches must be explicit and revision-specific.**
   - SDL2 naming currently needs a known correction.
   - FreeImage/LibJXR currently needs further validated fixes.
   - Do not create open-ended text substitutions against arbitrary dependency versions.

6. **Manual Gates 1-3 remain authoritative.**
   - Dependency build must pass first.
   - OGRE-Next itself must then build and install.
   - A minimal manually created program must consume only the installed SDK and open a rendered window.
   - Only then should the wizard automate the proven sequence.

7. **Keep OGRE and OGRE-Next separate.**
   - The new v3.9 OGRE-Next wizard must not replace or repurpose the existing OGRE 1.x wizard.

---

## 10. Open questions

The following remain unresolved:

- Does removing FreeImage's forced `__ANSI__=1` allow LibJXR to compile cleanly under the Code::Blocks GCC 14.2 toolchain?
- Is using MinGW-w64's system `guiddef.h` fully compatible with all bundled LibJXR/JPEG-XR code, or does a narrower compatibility patch need to be written?
- Are further FreeImage/LibJXR assumptions exposed after the current 26% point?
- Once dependencies pass, does OGRE-Next `v3.0.0` itself require MinGW-specific fixes?
- Which exact dependency revision should eventually be pinned by the wizard?
- Which compatibility changes should be upstreamed versus maintained locally by the Code::Blocks wizard infrastructure?

These questions should be answered experimentally before producing the final Cursor implementation plan.

---

## 11. Working rule for the rest of the investigation

For each new failure:

1. preserve the exact compiler/CMake error;
2. identify the generated command and actual source configuration that caused it;
3. make the smallest source-level experimental change possible;
4. rebuild far enough to establish whether that change is causal;
5. distinguish temporary diagnostic modifications from candidate production fixes;
6. record the result here before moving on.

The objective is not merely to obtain one lucky successful build. The objective is to understand the minimum reproducible compatibility layer needed to make OGRE-Next dependable from a Code::Blocks wizard on Windows.
