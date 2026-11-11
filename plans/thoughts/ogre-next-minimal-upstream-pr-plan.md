# OGRE-Next MinGW Upstream PR Plan

## Principle

Keep every pull request as small and reviewable as possible.

Do not upstream the complete Code::Blocks compatibility patch as one PR. The investigation found several independent problems in several bundled projects. Each PR should fix one obvious problem, ideally one file and one logical change.

Code::Blocks should generally not be mentioned unless it is essential. The useful upstream claim is usually: **current x86_64 MinGW-w64 build**.

Before each PR:

```bat
git status
git diff
```

Start from the upstream branch expected by the repository, make only the intended change, build enough to prove that change, then inspect the final diff before committing.

---

## PR 1 - ogre-next-deps: SDL2 DLL name on MinGW. Status: Submitted online

**Repository:** `OGRECave/ogre-next-deps`

**Scope:** one line in `src/CMakeLists.txt`.

Change:

```cmake
set(SDL2_BIN_NAME "libSDL2.dll")
```

to:

```cmake
set(SDL2_BIN_NAME "SDL2.dll")
```

### Suggested title

```text
Fix SDL2 DLL name for MinGW
```

### Suggested description

```text
Current SDL2 produces SDL2.dll when built with MinGW, while the dependency
post-install step expects libSDL2.dll. Use the produced DLL name for the
MinGW case.
```

### Validation

Clean configure/build of `ogre-next-deps` with x86_64 MinGW-w64 proceeds through SDL2 and installs `SDL2.dll`.

**Upstream priority:** very high. Tiny, obvious, isolated.

---

## PR 2 - ogre-next-deps: fix FreeImage build with current MinGW-w64. Status: Submitted online

Repository: OGRECave/ogre-next-deps

Scope:
- src/FreeImage/CMakeLists.txt
- src/FreeImage/Source/OpenEXR/Imath/ImathVec.h

Implemented as one PR with three commits.

### Commit 1 - Configure LibJXR correctly for MinGW-w64

Commit:
7a5a0b6

File:
src/FreeImage/CMakeLists.txt

Two related LibJXR platform-selection changes are made in this commit.

1. Do not define __ANSI__ on Windows

Change:

    if( NOT MSVC )

to:

    if( NOT MSVC AND NOT WIN32 )

Reason:

LibJXR's __ANSI__ switch selects its non-Windows platform header. MinGW is
a WIN32 target, and defining both WIN32 and __ANSI__ activates incompatible
platform definitions. In particular, ansi.h redefines UINTPTR_T/INTPTR_T as
32-bit int on 64-bit Windows.

2. Select LibJXR's existing MinGW-w64 code path

Add:

    if( MINGW )
        add_definitions( "-D__MINGW64_TOOLCHAIN__" )
    endif()

Reason:

LibJXR already contains a __MINGW64_TOOLCHAIN__ code path. FreeImage's
existing MinGW-w64 makefiles define this switch, but the CMake build did not.
Without it, LibJXR selects code that conflicts with declarations provided by
the MinGW-w64 CRT.

Result:

The initial failure in FreeImage.cpp is resolved and the build proceeds to
the next MinGW-w64-specific failure.

### Commit 2 - Keep LibJXR's private guiddef.h scoped to LibJXR sources

Commit:
2186a10

File:
src/FreeImage/CMakeLists.txt

Remove LibJXR's private common/include directory from the target-wide include
path and apply it only to the LibJXR source files that require it.

Reason:

LibJXR contains its own private guiddef.h under common/include. MinGW-w64 also
provides the Windows system guiddef.h.

Making common/include a target-wide include directory allows LibJXR's private
guiddef.h to shadow the MinGW-w64 Windows header outside LibJXR. This breaks
code that expects the MinGW-w64 definitions, in particular __mingw_uuidof.

The include directory is therefore scoped with a source-file
INCLUDE_DIRECTORIES property to:

    Source/LibJXR/jxrgluelib/JXRGlue.c
    Source/LibJXR/jxrgluelib/JXRGlueJxr.c
    Source/LibJXR/jxrgluelib/JXRGluePFC.c
    Source/LibJXR/jxrgluelib/JXRMeta.c
    Source/FreeImage/PluginJXR.cpp

This preserves LibJXR's private header for the sources that need it without
allowing it to shadow the Windows system header elsewhere in FreeImage.

Result:

Compilation proceeds through most of FreeImage and its dependencies and
reaches the bundled OpenEXR/Imath code.

### Commit 3 - Include fixed-width integer declarations in bundled Imath

Commit:
e2d12b1

File:
src/FreeImage/Source/OpenEXR/Imath/ImathVec.h

Add:

    #include <cstdint>

Reason:

ImathVec.h uses fixed-width integer types but relied on those declarations
being provided transitively by other headers. This happens to work with MSVC
but fails with current MinGW-w64/GCC.

Including <cstdint> directly makes the dependency explicit and portable.

Result:

The FreeImage target builds successfully with the tested MinGW-w64
toolchains.

### PR title

Fix FreeImage build on mingw64 (minor LibJXR configuration and OpenEXR header inclusion changes)

### Validation

The failure sequence was reproduced from the unmodified baseline using the
MSYS2 MinGW-w64 toolchain.

Toolchain:

    set "MSYS_MINGW=D:\msys64\mingw64"
    set "PATH=%MSYS_MINGW%\bin;%PATH%"

Configuration:

    cmake -S . -B build-freeimage-baseline ^
      -G "MinGW Makefiles" ^
      -DCMAKE_BUILD_TYPE=Release ^
      -DCMAKE_MAKE_PROGRAM=mingw32-make.exe ^
      -DCMAKE_C_COMPILER=gcc.exe ^
      -DCMAKE_CXX_COMPILER=g++.exe ^
      -DCMAKE_RC_COMPILER=windres.exe

Build:

    cmake --build build-freeimage-baseline --target FreeImage

Observed progression:

1. Baseline:
   FreeImage.cpp fails while processing the MinGW-w64 Windows headers, with
   __mingw_uuidof-related errors. This is consistent with LibJXR's ANSI
   configuration being selected alongside the Windows configuration.

2. After 7a5a0b6:
   FreeImage.cpp compiles. The next failure occurs around UUID definitions in
   PluginRAW.cpp and can be traced to LibJXR's private guiddef.h escaping its
   intended scope.

3. After 2186a10:
   Compilation proceeds through most of FreeImage and its dependencies to
   ImathColorAlgo.cpp/ImathVec.h, where int64_t is not defined and GCC notes
   that <cstdint> is required.

4. After e2d12b1:
   The FreeImage target builds successfully.

The same failure/success progression was also tested with the MinGW toolchain
distributed with Code::Blocks 25.03.

Upstream priority: high.

Status: Submitted online.

## PR 4 - ogre-next-deps: Remotery Winsock linkage

**Repository:** `OGRECave/ogre-next-deps`

**Scope:** `src/Remotery/CMakeLists.txt` only.

Add:

```cmake
if( WIN32 )
    target_link_libraries( Remotery ws2_32 )
endif()
```

### Suggested title

```text
Link Remotery with Winsock on Windows
```

### Suggested description

```text
Remotery uses the Windows socket API. Link ws2_32 explicitly so the target
also links with MinGW, which does not acquire the library through MSVC-specific
implicit linkage.
```

**Upstream priority:** high. Small and self-contained.

---

## PR 5 - ogre-next-deps: Remotery `rsize_t`

**Repository:** `OGRECave/ogre-next-deps`

**Scope:** one conditional in `src/Remotery/src/Remotery.c`.

Change the local `rsize_t` typedef condition so it is not defined under MinGW-w64, where the CRT already supplies `rsize_t`.

### Suggested title

```text
Avoid redefining rsize_t with MinGW-w64
```

### Suggested description

```text
MinGW-w64 already defines rsize_t through its CRT headers. Avoid Remotery's
fallback typedef in that configuration, which conflicts on 64-bit Windows.
```

**Upstream priority:** high, but check whether this is better proposed to Remotery upstream first. If OGRE carries a frozen vendored copy, an ogre-next-deps fix is still reasonable.

---

## PR 6 - OGRE-Next: explicit D3D compiler library

**Repository:** `OGRECave/ogre-next`

**Scope:** ideally one line in `CMake/Packages/FindDirectX11.cmake`.

Change:

```cmake
set(DirectX11_LIBRARY d3d11.lib dxgi.lib dxguid.lib)
```

to:

```cmake
set(DirectX11_LIBRARY d3d11.lib dxgi.lib dxguid.lib d3dcompiler.lib)
```

### Suggested title

```text
Link D3DCompiler explicitly in FindDirectX11
```

### Suggested description

```text
OGRE uses D3DCompile/D3DReflect. Add d3dcompiler to the DirectX11 libraries
instead of relying on compiler-specific implicit linkage.
```

**Upstream priority:** very high. Tiny and conceptually clean.

---

## PR 7 - OGRE-Next: MinGW PSAPI linkage

**Repository:** `OGRECave/ogre-next`

**Scope:** a few lines in `CMake/Packages/FindDirectX11.cmake`.

Add:

```cmake
if(MINGW)
    list(APPEND DirectX11_LIBRARY psapi.lib)
endif()
```

### Suggested title

```text
Link PSAPI explicitly with MinGW
```

### Suggested description

```text
OgreNsightChecker uses EnumProcessModules and GetModuleFileNameExA. MinGW does
not obtain PSAPI through MSVC-specific implicit linkage, so link it explicitly.
```

**Upstream priority:** high.

---

## PR 8 - OGRE-Next: missing MinGW D3D11 debug flag

**Repository:** `OGRECave/ogre-next`

**Scope:** compatibility definition in `RenderSystems/Direct3D11/src/OgreD3D11Device.cpp`.

Candidate:

```cpp
#ifndef D3D11_RLDO_IGNORE_INTERNAL
#define D3D11_RLDO_IGNORE_INTERNAL ( (D3D11_RLDO_FLAGS)0x4 )
#endif
```

### Suggested title

```text
Handle missing D3D11_RLDO_IGNORE_INTERNAL in older MinGW headers
```

This one deserves more caution than the one-line linkage fixes. Before opening the PR, verify whether maintainers prefer a CMake feature check, a local constant, or simply requiring newer MinGW-w64 headers.

**Upstream priority:** medium. Do after the obvious PRs.

---

## Changes not to lead with

These worked locally but are less attractive as first upstream PRs:

### Delete bundled LibJXR `guiddef.h`

The fix is well motivated and matches downstream packaging practice, but deleting a vendored third-party header is more invasive than a one-line build fix. First search for an accepted upstream/downstream precedent and consider filing it separately with that evidence.

### zziplib MinGW read/write adapters

The wrappers are bounded and useful, but they alter third-party C code and include a >4 GiB clamp. That deserves its own focused discussion and ideally belongs upstream in zziplib or in a clearly documented vendored compatibility patch.

### Documentation PR

Do not start with documentation. First get the small build fixes accepted. Once the supported behavior is clearer, a concise MinGW-w64 build note is much easier to write and defend.

---

## Recommended submission order

Submit **one PR at a time**, starting with the least controversial changes:

```text
1. deps: SDL2 DLL name                    one line
2. deps: Imath <cstdint>                  one line
3. OGRE: explicit d3dcompiler             one line
4. deps: Remotery ws2_32                  small
5. OGRE: MinGW psapi                      small
6. deps: LibJXR ANSI condition            one-line condition
7. deps: LibJXR MinGW switch              small
8. deps: Remotery rsize_t                 small
9. OGRE: D3D11 debug enum shim            discuss/verify approach
10. LibJXR guiddef / zziplib               separate deeper PRs only if wanted
```

Do not open all PRs simultaneously. Start with PR 1. The maintainer response will reveal preferred style and whether related MinGW fixes should remain separate or be grouped.

---

## PR discipline

For each PR:

1. Branch from the current upstream branch expected by the repository.
2. Make only the single intended logical change.
3. Run the narrowest meaningful build test plus the full relevant build when practical.
4. Run:

```bat
git diff
git status
```

5. Keep comments out of the source unless the code genuinely needs explanation.
6. Do not mention the six-hour investigation in the PR body.
7. Describe the observed failure, the exact cause, and why the tiny change is sufficient.
8. If the maintainer prefers a different implementation, follow upstream style rather than defending the local patch shape.

The objective is not to upstream the local compatibility patch verbatim. The objective is to turn the validated findings into the smallest independently useful fixes the maintainers are comfortable accepting.






## Notes for PR1

Pristine ogre-next-deps (commit f7e23191f4fd89ed41134996bc47f9ee23d009df9)

Configuration for using the codeblocks mingw toolchain (codeblocks version 25.03, installed from the setup.exe)

set "CB_MINGW=D:\Program Files\CodeBlocks\MinGW"
set "PATH=%CB_MINGW%\bin;%PATH%"
set "CC=%CB_MINGW%\bin\gcc.exe"
set "CXX=%CB_MINGW%\bin\g++.exe"

cmake -S . -B build ^
  -G "MinGW Makefiles" ^
  -DCMAKE_BUILD_TYPE=Release ^
  -DCMAKE_MAKE_PROGRAM=mingw32-make.exe ^
  -DCMAKE_C_COMPILER="%CB_MINGW%\bin\gcc.exe" ^
  -DCMAKE_CXX_COMPILER="%CB_MINGW%\bin\g++.exe" ^
  -DCMAKE_RC_COMPILER=windres.exe

Configuration runs successfully, but then

cmake --build build --target SDL2

fails as follows:

... 

-- Installing: ogre-next-deps-pr1/build/ogredeps/share/aclocal/sdl2.m4
[100%] Move SDL2 install files into the Release and Debug folders.
CMake Error at ogre-next-deps-pr1/build/src/SDL2-prefix/src/SDL2-stamp/SDL2-MoveInstallFiles-Release.cmake:49 (message):
  Command failed: 1

   'D:/Program Files/CodeBlocks/MinGW/bin/cmake.exe' '-Dmake=' '-Dconfig=' '-P' 'ogre-next-deps-pr1/build/src/SDL2-prefix/src/SDL2-stamp/SDL2-MoveInstallFiles-Release-impl.cmake'

  See also

    ogre-next-deps-pr1/build/src/SDL2-prefix/src/SDL2-stamp/SDL2-MoveInstallFiles-*.log


mingw32-make[3]: *** [src\CMakeFiles\SDL2.dir\build.make:95: src/SDL2-prefix/src/SDL2-stamp/SDL2-MoveInstallFiles] Error 1
mingw32-make[2]: *** [CMakeFiles\Makefile2:406: src/CMakeFiles/SDL2.dir/all] Error 2
mingw32-make[1]: *** [CMakeFiles\Makefile2:413: src/CMakeFiles/SDL2.dir/rule] Error 2
mingw32-make: *** [Makefile:175: SDL2] Error 2

Inspection of ogre-next-deps-pr1/build/src/SDL2-prefix/src/SDL2-stamp/SDL2-MoveInstallFiles-err.log gives:

Error renaming from "bin/libSDL2.dll" to "bin/Debug/libSDL2.dll": No such file or directory
CMake Error at ogre-next-deps-pr1/build/src/SDL2-prefix/src/SDL2-stamp/SDL2-MoveInstallFiles-Release-impl.cmake:39 (message):
  Command failed (1):

   'D:/Program Files/CodeBlocks/MinGW/bin/cmake.exe' '-E' 'rename' 'bin/libSDL2.dll' 'bin/Debug/libSDL2.dll'

Inspection of ogre-next-deps-pr1\build\ogredeps\bin indicates the DLL is SDL2.dll, not libSDL2.dll


After patching `src/CMakeLists.txt`:

same configuration and build command.

sdl2 target builds successfully:

…
-- Installing: ogre-next-deps-pr1/build/ogredeps/share/aclocal/sdl2.m4
[100%] Move SDL2 install files into the Release and Debug folders.
-- SDL2 MoveInstallFiles command succeeded.  See also ogre-next-deps-pr1/build/src/SDL2-prefix/src/SDL2-stamp/SDL2-MoveInstallFiles-*.log
[100%] Completed 'SDL2'
[100%] Built target SDL2



# Control msys64
set "MSYS_MINGW=D:\msys64\mingw64"
set "PATH=%MSYS_MINGW%\bin;%PATH%"

gcc --version
g++ --version
where gcc
where g++
where mingw32-make

cmake -S . -B build-msys2 ^
  -G "MinGW Makefiles" ^
  -DCMAKE_BUILD_TYPE=Release ^
  -DCMAKE_MAKE_PROGRAM=mingw32-make.exe ^
  -DCMAKE_C_COMPILER=gcc.exe ^
  -DCMAKE_CXX_COMPILER=g++.exe ^
  -DCMAKE_RC_COMPILER=windres.exe

# Notes for PR2
msys64
set "MSYS_MINGW=D:\msys64\mingw64"
set "PATH=%MSYS_MINGW%\bin;%PATH%"

gcc --version
g++ --version
where gcc
where g++
where mingw32-make

cmake -S . -B build-freeimage-baseline ^
  -G "MinGW Makefiles" ^
  -DCMAKE_BUILD_TYPE=Release ^
  -DCMAKE_MAKE_PROGRAM=mingw32-make.exe ^
  -DCMAKE_C_COMPILER=gcc.exe ^
  -DCMAKE_CXX_COMPILER=g++.exe ^
  -DCMAKE_RC_COMPILER=windres.exe

cmake --build build-freeimage-baseline --target FreeImage


Without PR2 commits, fails at ogre-next-deps-pr1\src\FreeImage\Source\FreeImage\FreeImage.cpp with '__mingw_uuidof' not declared errors



