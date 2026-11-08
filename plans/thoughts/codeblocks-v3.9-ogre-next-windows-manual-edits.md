# Code::Blocks v3.9 - OGRE-Next on Windows

**Status:** manual build validated; clean-clone replay in progress.  
**Expected first build time:** approximately 1-2 hours in total. On the tested computer, dependencies alone took about 30 minutes with four parallel jobs.  
**Goal:** build OGRE-Next with the MinGW GCC toolchain selected in Code::Blocks, install a clean SDK, and run a small independent program. The existing OGRE 1.x wizard remains unchanged.

OGRE and OGRE-Next are separate libraries and must remain separate project choices in Code::Blocks.

## 1. Versions and files

Pinned revisions:

```text
ogre-next-deps  f7e23191f4fd89ed41134996bc47f9ee23d009df
OGRE-Next       4859016ad2cc956a653dfc4e4a859505696c6509
                 branch v3-0, Eris 3.0.0
```

Code::Blocks compiler:

```text
D:\Program Files\CodeBlocks\MinGW
```

Example files:

```text
D:\Users\thoma\Documents\git\codeblocks-macos\plans\thoughts\ogre_next_example\Tutorial00_Basic.cpp
D:\Users\thoma\Documents\git\codeblocks-macos\plans\thoughts\ogre_next_example\CMakeLists.txt
```

Use **Command Prompt**, not PowerShell. Keep the same Command Prompt open throughout the build.

## 2. Prepare a clean working folder

```bat
mkdir D:\Users\thoma\Documents\git\ogre-next-test
cd /d D:\Users\thoma\Documents\git\ogre-next-test

mkdir window-test
copy /Y "D:\Users\thoma\Documents\git\codeblocks-macos\plans\thoughts\ogre_next_example\Tutorial00_Basic.cpp" window-test\
copy /Y "D:\Users\thoma\Documents\git\codeblocks-macos\plans\thoughts\ogre_next_example\CMakeLists.txt" window-test\

dir window-test\CMakeLists.txt
dir window-test\Tutorial00_Basic.cpp
```

Stop if either example file is missing. The folders `ogre-next-deps`, `ogre-next`, and `install` should not exist yet.

## 3. Select the Code::Blocks compiler

```bat
set "CB_MINGW=D:\Program Files\CodeBlocks\MinGW"
set "PATH=%CB_MINGW%\bin;%PATH%"
set "CC=%CB_MINGW%\bin\gcc.exe"
set "CXX=%CB_MINGW%\bin\g++.exe"

where gcc
where g++
where ar
where mingw32-make
where windres

gcc -dumpmachine
gcc --version
cmake --version
git --version
```

The first path for `gcc`, `g++`, `ar`, `mingw32-make`, and `windres` must begin with:

```text
D:\Program Files\CodeBlocks\MinGW\bin
```

`gcc -dumpmachine` must print:

```text
x86_64-w64-mingw32
```

The tested compiler is GCC 14.2.0. Stop if another MinGW installation appears first. If a new Command Prompt is opened later, repeat this section.

## 4. Clone the dependencies

```bat
cd /d D:\Users\thoma\Documents\git\ogre-next-test

git clone https://github.com/OGRECave/ogre-next-deps.git
cd ogre-next-deps
git checkout f7e23191f4fd89ed41134996bc47f9ee23d009df
git submodule update --init --recursive --depth 1

git rev-parse HEAD
git -C src\SDL2 rev-parse HEAD
```

Expected revisions:

```text
ogre-next-deps  f7e23191f4fd89ed41134996bc47f9ee23d009df
SDL2            5d249570393f7a37e037abf22cd6012a4cc56a71
```

## 5. Make the dependency compatibility edits

Use a text editor. Make only the edits below. Save each file after editing.

### Edit 1 - correct the SDL2 DLL name

Open:

```text
src\CMakeLists.txt
```

Find:

```cmake
set(SDL2_BIN_NAME "libSDL2.dll")
```

Replace it with:

```cmake
set(SDL2_BIN_NAME "SDL2.dll")
```

### Edit 2 - select the correct LibJXR platform code

Open:

```text
src\FreeImage\CMakeLists.txt
```

Find:

```cmake
# LibJXR
add_definitions( "-DDISABLE_PERF_MEASUREMENT" )
if( NOT MSVC )
    add_definitions( "-D__ANSI__=1" )
endif()
```

Replace that block with:

```cmake
# LibJXR
add_definitions( "-DDISABLE_PERF_MEASUREMENT" )
if( NOT MSVC AND NOT WIN32 )
    add_definitions( "-D__ANSI__=1" )
endif()
if( MINGW )
    add_definitions( "-D__MINGW64_TOOLCHAIN__" )
endif()
```

### Edit 3 - remove the obsolete private Windows GUID header

Delete this one file:

```bat
del src\FreeImage\Source\LibJXR\common\include\guiddef.h
```

Do **not** delete the `common\include` directory. LibJXR needs its other headers.

### Edit 4 - include fixed-width integer declarations in Imath

Open:

```text
src\FreeImage\Source\OpenEXR\Imath\ImathVec.h
```

Find:

```cpp
#include "ImathMath.h"

#include <iostream>
```

Replace it with:

```cpp
#include "ImathMath.h"

#include <cstdint>
#include <iostream>
```

### Edit 5 - avoid redefining `rsize_t` in Remotery

Open:

```text
src\Remotery\src\Remotery.c
```

Find:

```c
#if (!defined(_WIN64) && !defined(__APPLE__)) || (defined(__MINGW32__) && !defined(RSIZE_T_DEFINED))
typedef unsigned int rsize_t;
#endif
```

Replace it with:

```c
#if !defined(_WIN64) && !defined(__APPLE__) && !defined(__MINGW32__)
typedef unsigned int rsize_t;
#endif
```

### Edit 6 - link Remotery to Windows sockets

Open:

```text
src\Remotery\CMakeLists.txt
```

Find:

```cmake
if( APPLE )
    set( RMT_USE_METAL 1 )
endif()
configure_file( RemoteryConfig.h.in ${CMAKE_CURRENT_SOURCE_DIR}/src/RemoteryConfig.h )
```

Insert the following block between `endif()` and `configure_file(...)`:

```cmake
if( WIN32 )
    target_link_libraries( Remotery ws2_32 )
endif()
```

### Edit 7 - adapt zziplib read/write functions for 64-bit MinGW

Open:

```text
src\zziplib\zzip\plugin.c
```

Immediately after:

```c
#include <zzip/format.h>
```

insert:

```c
#if defined(__MINGW32__)
static zzip_ssize_t zzip_mingw_read(int fd, void *buf, zzip_size_t len)
{
    if (len > 0xffffffffu)
        len = 0xffffffffu;
    return (zzip_ssize_t) _read(fd, buf, (unsigned int) len);
}

static zzip_ssize_t zzip_mingw_write(int fd, const void *buf, zzip_size_t len)
{
    if (len > 0xffffffffu)
        len = 0xffffffffu;
    return (zzip_ssize_t) _write(fd, buf, (unsigned int) len);
}
#endif
```

In the `default_io` block, replace:

```c
    &_zzip_read,
```

with:

```c
#if defined(__MINGW32__)
    &zzip_mingw_read,
#else
    &_zzip_read,
#endif
```

Then replace:

```c
    &_zzip_write
```

with:

```c
#if defined(__MINGW32__)
    &zzip_mingw_write
#else
    &_zzip_write
#endif
```

### Check the dependency edits

Run:

```bat
git status --short
git diff --check
git diff --stat
```

Expected changed paths:

```text
src/CMakeLists.txt
src/FreeImage/CMakeLists.txt
src/FreeImage/Source/LibJXR/common/include/guiddef.h  deleted
src/FreeImage/Source/OpenEXR/Imath/ImathVec.h
src/Remotery/CMakeLists.txt
src/Remotery/src/Remotery.c
src/zziplib/zzip/plugin.c
```

`git diff --check` should print nothing. Stop if additional files changed or if an expected file is absent.

## 6. Configure, build, and install the dependencies

```bat
cmake -S . -B build ^
  -G "MinGW Makefiles" ^
  -DCMAKE_BUILD_TYPE=Release ^
  -DCMAKE_MAKE_PROGRAM=mingw32-make.exe ^
  -DCMAKE_C_COMPILER="%CB_MINGW%\bin\gcc.exe" ^
  -DCMAKE_CXX_COMPILER="%CB_MINGW%\bin\g++.exe" ^
  -DCMAKE_RC_COMPILER=windres.exe
```

The last line should say that build files were written to:

```text
D:/Users/thoma/Documents/git/ogre-next-test/ogre-next-deps/build
```

Build and install:

```bat
cmake --build build --parallel 4
cmake --install build
```

Check:

```bat
dir build\ogredeps\include\FreeImage.h
dir build\ogredeps\include\SDL2\SDL.h
dir build\ogredeps\include\shaderc\shaderc.h
dir build\ogredeps\lib\Release\libFreeImage.a
dir build\ogredeps\lib\libSDL2.dll.a
dir build\ogredeps\lib\libshaderc_combined.a
dir build\ogredeps\bin\Release\SDL2.dll
```

**Gate 1 passes** when every file exists. The dependency SDK is:

```text
D:\Users\thoma\Documents\git\ogre-next-test\ogre-next-deps\build\ogredeps
```

If Windows error 1455 occurs, rerun the build with `--parallel 4`. Do not increase the number.

## 7. Clone OGRE-Next

```bat
cd /d D:\Users\thoma\Documents\git\ogre-next-test

git clone --branch v3-0 https://github.com/OGRECave/ogre-next.git
cd ogre-next
git checkout 4859016ad2cc956a653dfc4e4a859505696c6509
git rev-parse HEAD
```

Expected revision:

```text
4859016ad2cc956a653dfc4e4a859505696c6509
```

Do not use `master`; it is the later 4.x development line.

## 8. Make the OGRE-Next compatibility edits

### Edit 1 - link the Windows libraries MinGW needs explicitly

Open:

```text
CMake\Packages\FindDirectX11.cmake
```

Find:

```cmake
set(DirectX11_LIBRARY d3d11.lib dxgi.lib dxguid.lib)
```

Replace it with:

```cmake
set(DirectX11_LIBRARY d3d11.lib dxgi.lib dxguid.lib d3dcompiler.lib)
if(MINGW)
    list(APPEND DirectX11_LIBRARY psapi.lib)
endif()
```

### Edit 2 - provide the missing Direct3D 11 flag

Open:

```text
RenderSystems\Direct3D11\src\OgreD3D11Device.cpp
```

Find:

```cpp
#include "OgreException.h"
```

Immediately after that line insert:

```cpp
#ifndef D3D11_RLDO_IGNORE_INTERNAL
#define D3D11_RLDO_IGNORE_INTERNAL ( (D3D11_RLDO_FLAGS)0x4 )
#endif
```

### Check the OGRE-Next edits

```bat
git status --short
git diff --check
git diff --stat
```

Expected changed paths:

```text
CMake/Packages/FindDirectX11.cmake
RenderSystems/Direct3D11/src/OgreD3D11Device.cpp
```

`git diff --check` should print nothing.

## 9. Configure, build, and install OGRE-Next

```bat
cmake -S . -B build ^
  -G "MinGW Makefiles" ^
  -DCMAKE_BUILD_TYPE=Release ^
  -DCMAKE_MAKE_PROGRAM=mingw32-make.exe ^
  -DCMAKE_C_COMPILER="%CB_MINGW%\bin\gcc.exe" ^
  -DCMAKE_CXX_COMPILER="%CB_MINGW%\bin\g++.exe" ^
  -DCMAKE_RC_COMPILER=windres.exe ^
  -DOGRE_DEPENDENCIES_DIR=D:/Users/thoma/Documents/git/ogre-next-test/ogre-next-deps/build/ogredeps ^
  -DCMAKE_INSTALL_PREFIX=D:/Users/thoma/Documents/git/ogre-next-test/install

cmake --build build --parallel 4
cmake --install build
```

Check:

```bat
dir ..\install\include\OGRE\OgreRoot.h
dir ..\install\include\OGRE\Hlms\Unlit\OgreHlmsUnlit.h
dir ..\install\include\OGRE\Hlms\Pbs\OgreHlmsPbs.h
dir ..\install\lib\Release\libOgreMain.dll.a
dir ..\install\bin\Release\OgreMain.dll
dir ..\install\bin\Release\RenderSystem_Direct3D11.dll
dir ..\install\bin\Release\RenderSystem_GL3Plus.dll
dir ..\install\bin\Release\RenderSystem_Vulkan.dll
dir ..\install\bin\Release\plugins.cfg
dir ..\install\bin\Release\resources2.cfg
dir ..\install\Media\Hlms\Unlit
dir ..\install\Media\Hlms\Pbs
```

**Gate 2 passes** when every file or directory exists. The installed SDK is:

```text
D:\Users\thoma\Documents\git\ogre-next-test\install
```

Future programs must use this installed SDK, not the source or build trees.

## 10. Build the independent test program

```bat
cd /d D:\Users\thoma\Documents\git\ogre-next-test

cmake -S window-test -B window-test\build ^
  -G "MinGW Makefiles" ^
  -DCMAKE_BUILD_TYPE=Release ^
  -DCMAKE_MAKE_PROGRAM=mingw32-make.exe ^
  -DCMAKE_C_COMPILER="%CB_MINGW%\bin\gcc.exe" ^
  -DCMAKE_CXX_COMPILER="%CB_MINGW%\bin\g++.exe" ^
  -DOGRE_SDK=D:/Users/thoma/Documents/git/ogre-next-test/install

cmake --build window-test\build --parallel 4
dir window-test\build\window-test.exe
```

Do not add the OGRE-Next source or build tree to the example's include or library paths.

## 11. Run the test program

```bat
copy /Y window-test\build\window-test.exe install\bin\Release\window-test.exe
cd /d D:\Users\thoma\Documents\git\ogre-next-test\install\bin\Release
window-test.exe
```

**Gate 3 passes** when:

1. a window titled `Tutorial 00: Basic` opens;
2. the interior is blue;
3. the console reports the selected render system;
4. closing the window prints `Rendered and closed cleanly.`;
5. `Ogre.log` records successful initialization and clean shutdown.

## 12. What happened?

```text
Git downloaded exact source revisions.
We made known MinGW compatibility edits and inspected them with git diff.
CMake configured the sources for the Code::Blocks compiler.
MinGW built the dependencies.
CMake installed a clean dependency SDK.
MinGW built OGRE-Next against that SDK.
CMake installed a clean OGRE-Next SDK.
The small program used only the installed SDK and opened a rendered window.
```

## 13. Later Code::Blocks wizard

The manual edits are educational. The future wizard should apply the tested version-specific patch files automatically.

The new wizard must:

- remain separate from the existing OGRE 1.x wizard;
- warn that first-time setup may take 1-2 hours;
- show stage progress and live build output;
- use the compiler selected in Code::Blocks and its adjacent tools;
- pin the exact revisions above;
- apply the two tested patch files;
- use `mingw32-make.exe` through a controlled `PATH`;
- build with `--parallel 4` by default;
- resume from completed, validated stages;
- keep source, build trees, dependency SDK, and installed SDK separate;
- generate projects against the installed SDK only;
- preserve the runtime working directory, plugins, and HLMS media layout.

## 14. Acceptance

Code::Blocks v3.9 is accepted when:

- this clean-clone manual procedure passes Gates 1-3;
- the new OGRE-Next wizard reproduces the same stages;
- the existing OGRE wizard remains unchanged;
- a fresh wizard project builds and runs without hand-editing paths;
- the generated project uses neither source nor build trees;
- a second wizard run skips valid completed stages.
