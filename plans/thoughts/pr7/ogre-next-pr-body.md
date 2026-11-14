# Summary

Fix Direct3D11 compilation and linking with MinGW-w64.

OGRE's Direct3D11 implementation uses the Microsoft DirectX/Windows API. MinGW-w64 provides compatible Windows and DirectX headers and libraries, but there are two differences from the Microsoft build environment that currently prevent the Direct3D11 render system from building.

First, OGRE uses `D3D11\\\_RLDO\\\_IGNORE\\\_INTERNAL` when calling the Direct3D debugging API to report live device objects. Some MinGW-w64 Direct3D headers do not define this flag, although the corresponding Windows SDK value is `0x4`. Compilation therefore stops because `D3D11\\\_RLDO\\\_IGNORE\\\_INTERNAL` is unknown.

Second, OGRE uses `D3DCompile` and `D3DReflect` from `d3dcompiler`, and `OgreNsightChecker` uses the Windows PSAPI functions `EnumProcessModules` and `GetModuleFileNameExA`. With the Microsoft compiler, some required libraries can be selected through `#pragma comment(lib)` directives. MinGW-w64 does not use this Microsoft-specific mechanism, so the required libraries must be supplied explicitly to the linker.

This PR therefore contains two compatibility fixes:

1. **Provide the missing `D3D11\\\_RLDO\\\_IGNORE\\\_INTERNAL` definition**

   A guarded compatibility definition supplies the Windows SDK value when the MinGW-w64 headers do not already define it:

```cpp
   #ifndef D3D11\\\_RLDO\\\_IGNORE\\\_INTERNAL
   #define D3D11\\\_RLDO\\\_IGNORE\\\_INTERNAL ( (D3D11\\\_RLDO\\\_FLAGS)0x4 )
   #endif
   ```

The `#ifndef` ensures that SDKs or MinGW-w64 versions which already provide the definition are unaffected.

2. **Explicitly link libraries required by the Direct3D11 implementation**

   `d3dcompiler.lib` is added explicitly to `DirectX11\\\_LIBRARY` for `D3DCompile` and `D3DReflect`.

   For MinGW builds, `psapi.lib` is additionally linked for `EnumProcessModules` and `GetModuleFileNameExA`.

These changes do not change the Direct3D11 implementation itself. They make dependencies that are provided implicitly or differently in the Microsoft toolchain explicit for MinGW-w64.

# Prerequisites

**Depends on PR #601.** This PR is stacked on the MinGW condition-variable fix because, without that prerequisite, `OgreNextMain` fails before the Direct3D11 target is reached.

The MSYS2 reproduction also uses the separately developed MinGW-w64 fixes for `ogre-next-deps`. Those fixes are needed to construct a coherent MSYS2 MINGW64 dependency tree; they are not part of the Direct3D11 changes in this PR.

# Reproducing

The reproduction uses sibling repositories:

```text
workdir/
  ogre-next/
  ogre-next-deps/
  install/
```

## MSYS2 MINGW64 toolchain

The baseline was reproduced with MSYS2 MINGW64 / GCC 15.2.0.

Select the toolchain and clear inherited compiler overrides:

```bat
set "MSYS\\\_MINGW=D:\\\\msys64\\\\mingw64"
set "PATH=%MSYS\\\_MINGW%\\\\bin;%PATH%"
set CC=
set CXX=
```

The selected tools can be checked with:

```bat
where gcc
where g++
where windres
where mingw32-make
where cmake
```

## Build ogre-next-deps

From the sibling `ogre-next-deps` repository, initialize the dependency submodules:

```bat
git submodule update --init --recursive
```

Apply the combined MinGW-w64 dependency patch used for this reproduction:

```bat
git apply ogre-next-deps-mingw.patch
```

Configure, build, and install the dependencies with the same MSYS2 MINGW64 toolchain:

```bat
cmake -S . -B build ^
  -G "MinGW Makefiles" ^
  -DCMAKE\\\_BUILD\\\_TYPE=Release

cmake --build build -j 8
cmake --install build
```

This stages the dependency tree under:

```text
ogre-next-deps/build/ogredeps
```

## Configure OGRE-Next

Use an `ogre-next` tree containing PR #601 as the prerequisite, but without the Direct3D11 changes from this PR.

From the sibling `ogre-next` repository:

```bat
cmake -S . -B build ^
  -G "MinGW Makefiles" ^
  -DCMAKE\\\_BUILD\\\_TYPE=Release ^
  -DOGRE\\\_DEPENDENCIES\\\_DIR=../ogre-next-deps/build/ogredeps ^
  -DCMAKE\\\_INSTALL\\\_PREFIX=../install
```

Verify that CMake selected the MSYS2 compilers:

```bat
findstr /I "CMAKE\\\_C\\\_COMPILER: CMAKE\\\_CXX\\\_COMPILER:" build\\\\CMakeCache.txt
```

Verify the prerequisite Windows API setting:

```bat
findstr /I "\\\_WIN32\\\_WINNT" build\\\\OgreMain\\\\CMakeFiles\\\\OgreNextMain.dir\\\\flags.make
```

which should contain:

```text
-D\\\_WIN32\\\_WINNT=0x0600
```

Build the prerequisite target:

```bat
cmake --build build --target OgreNextMain -j 8
```

This completes successfully:

```text
\\\[100%] Linking CXX shared library ..\\\\bin\\\\OgreNextMain.dll
\\\[100%] Built target OgreNextMain
```

# Observed behavior

## Baseline: missing Direct3D11 flag

Build the Direct3D11 render system:

```bat
cmake --build build --target RenderSystem\\\_Direct3D11 -j 8
```

The unmodified Direct3D11 source fails in `OgreD3D11Device.cpp`:

```text
OgreD3D11Device.cpp:80:48: error: 'D3D11\\\_RLDO\\\_IGNORE\\\_INTERNAL' was not declared in this scope
   80 |                                                D3D11\\\_RLDO\\\_IGNORE\\\_INTERNAL );
      |                                                ^\\\~\\\~\\\~\\\~\\\~\\\~\\\~\\\~\\\~\\\~\\\~\\\~\\\~\\\~\\\~\\\~\\\~\\\~\\\~\\\~\\\~\\\~\\\~\\\~\\\~
```

The surrounding `OgreComPtr`, switch-enum, shadowing, and cast diagnostics are warnings and are unrelated to this failure.

## After the header compatibility fix: missing libraries

After adding the guarded `D3D11\\\_RLDO\\\_IGNORE\\\_INTERNAL` definition, `OgreD3D11Device.cpp` compiles and the build proceeds to linking `RenderSystem\\\_Direct3D11.dll`.

Before regenerating the build system with the updated DirectX11 library configuration, the linker reports the second part of the problem:

```text
OgreD3D11HLSLProgram.cpp.obj: undefined reference to `D3DCompile'
OgreD3D11HLSLProgram.cpp.obj: undefined reference to `D3DReflect'
OgreNsightChecker.cpp.obj: undefined reference to `EnumProcessModules'
OgreNsightChecker.cpp.obj: undefined reference to `GetModuleFileNameExA'
collect2.exe: error: ld returned 1 exit status
```

`D3DCompile` and `D3DReflect` require `d3dcompiler`, while `EnumProcessModules` and `GetModuleFileNameExA` require PSAPI. Under MSVC, required libraries can be pulled in through Microsoft-specific `#pragma comment(lib)` directives; MinGW-w64 does not use that mechanism.

## With this PR

After applying both changes, rerun CMake so that the updated `FindDirectX11.cmake` library list is reflected in the generated link command:

```bat
cmake -S . -B build ^
  -G "MinGW Makefiles" ^
  -DCMAKE\\\_BUILD\\\_TYPE=Release ^
  -DOGRE\\\_DEPENDENCIES\\\_DIR=../ogre-next-deps/build/ogredeps ^
  -DCMAKE\\\_INSTALL\\\_PREFIX=../install
```

Then rebuild the same target:

```bat
cmake --build build --target RenderSystem\\\_Direct3D11 -j 8
```

The build completes successfully:

```text
\\\[100%] Linking CXX shared library ..\\\\..\\\\bin\\\\RenderSystem\\\_Direct3D11.dll
\\\[100%] Built target RenderSystem\\\_Direct3D11
```

The same target therefore progresses through both previously observed failures: `OgreD3D11Device.cpp` compiles with the missing flag supplied, and the resulting Direct3D11 render system links with the required `d3dcompiler` and PSAPI libraries.

# Generalization note

The Direct3D11 issue was originally encountered with the MinGW-w64 toolchain bundled with Code::Blocks. It was subsequently reproduced independently with MSYS2 MINGW64 / GCC 15.2.0 using dependencies built with the same MSYS2 toolchain.

The two environments use different GCC and MinGW-w64 distributions. Reproducing both the missing `D3D11\\\_RLDO\\\_IGNORE\\\_INTERNAL` definition and the subsequent missing-library link errors under MSYS2 confirms that these are MinGW-w64 portability issues rather than artifacts of the original Code::Blocks installation.



