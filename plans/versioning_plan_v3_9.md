# Code::Blocks v3.9 — OGRE-Next on Windows

**Status:** planning and manual validation.

**Goal:** install OGRE-Next manually with Git, CMake, and the MinGW GCC toolchain selected in Code::Blocks; prove it with a small manual project; then implement a new OGRE-Next wizard. The existing OGRE wizard remains unchanged.

OGRE-Next is not a replacement for OGRE 1.x. Code::Blocks must expose them as separate project choices.

## 1\. Preconditions

Use the compiler installation configured for the selected Code::Blocks GNU GCC compiler. For the current validation:

```bat
set "CB\\\_MINGW=D:\\Program Files\\CodeBlocks\\MinGW"
set "PATH=%CB\\\_MINGW%\\bin;%PATH%"
set "CC=%CB\\\_MINGW%\\bin\\gcc.exe"
set "CXX=%CB\\\_MINGW%\\bin\\g++.exe"
```

The following commands must work in Command Prompt:

```bat
git --version
cmake --version
gcc --version
g++ --version
mingw32-make --version

where gcc
where g++
where ar
where mingw32-make
```

The first result for all compiler tools must come from the same Code::Blocks MinGW installation. Do not mix MSYS2 MINGW64, UCRT64, WinLibs, or other GCC installations.

Use a short path without spaces:

```bat
mkdir D:\\Users\\thoma\\Documents\\git\\ogre-next-test
cd /d D:\\Users\\thoma\\Documents\\git\\ogre-next-test
```

Keep source, build, dependencies, and installed SDK distinct.

## 2\. Clone and build the dependencies

```bat
git clone --recurse-submodules --shallow-submodules https://github.com/OGRECave/ogre-next-deps.git

cmake -S ogre-next-deps -B ogre-next-deps\\build ^
  -G "MinGW Makefiles" ^
  -DCMAKE\\\_BUILD\\\_TYPE=Release
```

Before building, edit:

```text
D:\\Users\\thoma\\Documents\\git\\ogre-next-test\\ogre-next-deps\\src\\CMakeLists.txt
```

At approximately line 200, change:

```cmake
set(SDL2\\\_BIN\\\_NAME "libSDL2.dll")
```

to:

```cmake
set(SDL2\\\_BIN\\\_NAME "SDL2.dll")
```

Then build and install:

```bat
cmake --build ogre-next-deps\\build
cmake --install ogre-next-deps\\build
```

**Gate 1:** stop if this fails. This step proves that the MinGW GCC toolchain selected in Code::Blocks can build the dependencies. Do not accept a build performed accidentally with another GCC installation on `PATH`.

Expected dependency SDK:

```text
D:\\Users\\thoma\\Documents\\git\\ogre-next-test\\ogre-next-deps\\build\\ogredeps
```

## 3\. Clone, build, and install OGRE-Next

Use the published `v3.0.0` release rather than moving `master`:

```bat
git clone --branch v3.0.0 --depth 1 https://github.com/OGRECave/ogre-next.git

cmake -S ogre-next -B ogre-next\\build ^
  -G "MinGW Makefiles" ^
  -DCMAKE\\\_BUILD\\\_TYPE=Release ^
  -DOGRE\\\_DEPENDENCIES\\\_DIR=D:\\Users\\thoma\\Documents\\git\\ogre-next-test\\ogre-next-deps\\build\\ogredeps ^
  -DCMAKE\\\_INSTALL\\\_PREFIX=D:\\Users\\thoma\\Documents\\git\\ogre-next-test\\install

cmake --build ogre-next\\build
cmake --install ogre-next\\build
```

**Gate 2:** inspect `D:\\Users\\thoma\\Documents\\git\\ogre-next-test\\install`. Future projects must use this installed SDK, not the OGRE-Next source or build tree.

If configuration offers sample builds, enable the smallest useful sample set and run one installed or built sample before writing a new program.

## 4\. Small manual test project

Create:

```text
D:\\Users\\thoma\\Documents\\git\\ogre-next-test\\hello-ogre-next\\
    CMakeLists.txt
    main.cpp
```

Base `main.cpp` on OGRE-Next's own `Samples/2.0/Tutorials/Tutorial00\\\_Basic`. Do not invent initialization from memory. The first test should only:

* initialize OGRE-Next;
* create a render system and window;
* create the minimum scene/compositor state;
* render a basic frame or clear color;
* close cleanly.

The `CMakeLists.txt` must consume `D:\\Users\\thoma\\Documents\\git\\ogre-next-test\\install`. It must not include or link directly from `ogre-next\\` or `ogre-next\\build\\`.

```bat
cmake -S hello-ogre-next -B hello-ogre-next\\build -G "MinGW Makefiles"
cmake --build hello-ogre-next\\build
```

Copy or reference the required OGRE-Next runtime DLLs, plug-in configuration, HLMS resources, and media exactly as required by the installed SDK/sample layout.

**Gate 3:** the manually created executable opens an OGRE-Next-rendered window. Record every required include directory, library, DLL, plug-in, configuration file, resource directory, and working-directory assumption.

## 5\. New Code::Blocks wizard

Only after Gates 1–3 pass, add a new OGRE-Next wizard for v3.9.

* Do not replace, rename, or repurpose the existing OGRE wizard.
* Give OGRE and OGRE-Next separate entries, resources, scripts, and starter code.
* Reuse the proven Windows Git/CMake helper infrastructure where its contracts fit.
* Pin the same OGRE-Next tag and dependency revision proven manually.
* Resolve `gcc.exe`, `g++.exe`, `ar.exe`, and adjacent `mingw32-make.exe` from the compiler selected in Code::Blocks. Do not trust the global `PATH`.
* Give CMake and nested builds a controlled environment using that one toolchain.
* Clone dependencies and OGRE-Next only after explicit consent.
* Configure, build, and install into separate paths under `%LOCALAPPDATA%\\codeblocks`.
* Generate projects against the installed SDK only.
* Encode the DLL, plug-in, HLMS, media, and working-directory requirements learned in Gate 3.
* A second wizard run skips valid completed stages.

## 6\. Acceptance

v3.9 is accepted when:

* the dependency build succeeds with the same compiler installation selected in Code::Blocks, without mixing toolchains;
* OGRE-Next `v3.0.0` builds and installs with MinGW;
* the manual test opens an OGRE-Next window from the installed SDK;
* the existing OGRE wizard is unchanged and still works;
* the new OGRE-Next wizard has its own visible project entry;
* a fresh wizard project builds and runs without hand-editing paths;
* the generated project uses neither source nor build trees;
* a second run skips validated clone, configure, build, and install stages.

## 7\. Done when

The children can first explain and perform:

```text
Git gets the source.
CMake configures it for MinGW.
The compiler builds it.
CMake installs a clean SDK.
A small program uses that SDK.
```

They can then create the equivalent project through the new OGRE-Next wizard and compare each automated stage with the manual one.

## References

* [OGRE-Next repository](https://github.com/OGRECave/ogre-next)
* [OGRE-Next dependencies](https://github.com/OGRECave/ogre-next-deps)
* OGRE-Next Windows setup
* OGRE-Next releases
* `Tutorial00\\\_Basic`



