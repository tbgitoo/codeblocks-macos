# OGRE-Next on Windows with Code::Blocks MinGW

This guide builds OGRE-Next with the MinGW compiler supplied by Code::Blocks, installs a clean SDK, and runs a small independent program that opens a rendered window.

The first build is large. Allow approximately **1 to 2 hours** and keep the same Command Prompt open throughout.

## 1\. Versions and files used

Exact source revisions:

```text
ogre-next-deps  f7e23191f4fd89ed41134996bc47f9ee23d009df
OGRE-Next       4859016ad2cc956a653dfc4e4a859505696c6509
                 branch v3-0, version Eris 3.0.0
```

Code::Blocks compiler:

```text
D:\\Program Files\\CodeBlocks\\MinGW
```

Example files:

```text
D:\\Users\\thoma\\Documents\\git\\codeblocks-macos\\plans\\thoughts\\ogre\_next\_example\\Tutorial00\_Basic.cpp
D:\\Users\\thoma\\Documents\\git\\codeblocks-macos\\plans\\thoughts\\ogre\_next\_example\\CMakeLists.txt
```

Use **Command Prompt**, not PowerShell.

## 2\. Prepare a clean working folder

Open Command Prompt and run:

```bat
mkdir D:\\Users\\thoma\\Documents\\git\\ogre-next-test
cd /d D:\\Users\\thoma\\Documents\\git\\ogre-next-test

mkdir window-test
copy /Y "D:\\Users\\thoma\\Documents\\git\\codeblocks-macos\\plans\\thoughts\\ogre\_next\_example\\Tutorial00\_Basic.cpp" window-test\\
copy /Y "D:\\Users\\thoma\\Documents\\git\\codeblocks-macos\\plans\\thoughts\\ogre\_next\_example\\CMakeLists.txt" window-test\\

dir window-test\\CMakeLists.txt
dir window-test\\Tutorial00\_Basic.cpp
```

Both files must be listed. The folders `ogre-next-deps`, `ogre-next`, and `install` should not exist yet.

## 3\. Select the Code::Blocks compiler

Run:

```bat
set "CB\_MINGW=D:\\Program Files\\CodeBlocks\\MinGW"
set "PATH=%CB\_MINGW%\\bin;%PATH%"
set "CC=%CB\_MINGW%\\bin\\gcc.exe"
set "CXX=%CB\_MINGW%\\bin\\g++.exe"

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

The first path printed for `gcc`, `g++`, `ar`, `mingw32-make`, and `windres` must begin with:

```text
D:\\Program Files\\CodeBlocks\\MinGW\\bin
```

`gcc -dumpmachine` must print:

```text
x86\_64-w64-mingw32
```

The tested compiler is GCC 14.2.0. If a new Command Prompt is opened later, repeat this section.



The reason for doing this configuration is that otherwise, the compilation will mix different compiler and tooling and that's going to be a mess.



## 4\. Clone the dependencies

Run:

```bat
cd /d D:\\Users\\thoma\\Documents\\git\\ogre-next-test

git clone https://github.com/OGRECave/ogre-next-deps.git
cd ogre-next-deps
git checkout f7e23191f4fd89ed41134996bc47f9ee23d009df
git submodule update --init --recursive --depth 1

git rev-parse HEAD
git -C src\\SDL2 rev-parse HEAD
```

Expected revisions:

```text
ogre-next-deps  f7e23191f4fd89ed41134996bc47f9ee23d009df
SDL2            5d249570393f7a37e037abf22cd6012a4cc56a71
```



Here we get our source from internet. We want an exactly identical starting point if we have to repeat.



## 5\. Correct the dependency source code

Use a text editor. Make only the edits in this section, then save each file.

### 5.1 Use the Windows SDL2 DLL name

SDL2 creates `SDL2.dll` on this toolchain. The original build instruction expects the Unix-style name `libSDL2.dll`, so the expected filename must be corrected.

Open:

```text
D:\\Users\\thoma\\Documents\\git\\ogre-next-test\\ogre-next-deps\\src\\CMakeLists.txt
```

Search for:

```cmake
set(SDL2\_BIN\_NAME "libSDL2.dll")
```

Replace it with:

```cmake
set(SDL2\_BIN\_NAME "SDL2.dll")
```



The reason why we have to do this is that are really two conventions of naming library files: the Windows one (SDL2.dll) and the Linux one (libSDL2.so, by extension libSDL2.dll when ported to windows). And well, it's the exact file name that counts so this messes up every once a while.



### 5.2 Select the Windows version of the JPEG XR code

LibJXR contains separate platform paths. Defining both the Windows and ANSI paths at the same time gives 32-bit integer definitions to a 64-bit Windows build, so MinGW must use the Windows path only.

Open:

```text
D:\\Users\\thoma\\Documents\\git\\ogre-next-test\\ogre-next-deps\\src\\FreeImage\\CMakeLists.txt
```

Search for the block starting with `# LibJXR` and ending with the first `endif()`:

```cmake
# LibJXR
add\_definitions( "-DDISABLE\_PERF\_MEASUREMENT" )
if( NOT MSVC )
    add\_definitions( "-D\_\_ANSI\_\_=1" )
endif()
```

Replace the complete block with:

```cmake
# LibJXR
add\_definitions( "-DDISABLE\_PERF\_MEASUREMENT" )
if( NOT MSVC AND NOT WIN32 )
    add\_definitions( "-D\_\_ANSI\_\_=1" )
endif()
if( MINGW )
    add\_definitions( "-D\_\_MINGW64\_TOOLCHAIN\_\_" )
endif()
```



The reason why we have to do this is historical: People have increased the size of integers as machines grew bigger. But some old code has survived.





### 5.3 Use MinGW's Windows GUID header

LibJXR includes an old private copy of `guiddef.h`. MinGW already supplies the correct Windows header, and the private copy can hide it. Delete only the private file, not the directory containing the other LibJXR headers.

Run in Command Prompt:

```bat
del src\\FreeImage\\Source\\LibJXR\\common\\include\\guiddef.h
```



Long ago, LibJXR needed the Windows definitions from a file called guiddef.h, so a copy of that file was included inside LibJXR. Meanwhile, Windows and MinGW continued evolving their own guiddef.h. The two copies kept the same name but no longer evolved together. Now they are conflicting, so we need to remove one.







### 5.4 Include the fixed-width integer definitions in Imath

The bundled Imath code uses names such as `int64\_t`. Modern C++ declares these names in `<cstdint>`, so the header should include what it uses instead of depending on another header to include it indirectly.

Open:

```text
D:\\Users\\thoma\\Documents\\git\\ogre-next-test\\ogre-next-deps\\src\\FreeImage\\Source\\OpenEXR\\Imath\\ImathVec.h
```

Search for:

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





This one is from the deep ocean floor of image mathematics. Here, IMath expects the integer sizes to be predefined, but does not explicitly include the necessary "hashtag file". Fortunately, including the cstdint "hashtag file" is enough (this provides machine integer definitions).



### 5.5 Keep MinGW's 64-bit `rsize\_t`

On 64-bit Windows, MinGW already defines `rsize\_t` with the correct pointer-sized type. Remotery must not redefine it as a 32-bit `unsigned int`.

Open:

```text
D:\\Users\\thoma\\Documents\\git\\ogre-next-test\\ogre-next-deps\\src\\Remotery\\src\\Remotery.c
```

Search for:

```c
#if (!defined(\_WIN64) \&\& !defined(\_\_APPLE\_\_)) || (defined(\_\_MINGW32\_\_) \&\& !defined(RSIZE\_T\_DEFINED))
typedef unsigned int rsize\_t;
#endif
```

Replace it with:

```c
#if !defined(\_WIN64) \&\& !defined(\_\_APPLE\_\_) \&\& !defined(\_\_MINGW32\_\_)
typedef unsigned int rsize\_t;
#endif
```



Windows changed its addressing system on the road from 32-bit to 64-bit. Think of going from “Fritz Müller, Bern” to “Fritz Müller, Länggasse, Bern”: the new address needs a larger envelope to write everything down.



Here we are not directly changing the addresses themselves, but the size of the “envelopes” used to describe sizes in memory (rsize\_t). MinGW already knows that the modern 64-bit format needs a larger envelope. Remotery contains an older compatibility rule that tries to replace it with the old, smaller format. We therefore ask Remotery to leave MinGW's modern definition alone.



### 5.6 Link Remotery with Windows sockets

Remotery communicates through network sockets. Microsoft compilers can obtain some Windows libraries implicitly, while MinGW expects the build file to name the Winsock library explicitly.

Open:

```text
D:\\Users\\thoma\\Documents\\git\\ogre-next-test\\ogre-next-deps\\src\\Remotery\\CMakeLists.txt
```

Search for:

```cmake
if( APPLE )
    set( RMT\_USE\_METAL 1 )
endif()
configure\_file( RemoteryConfig.h.in ${CMAKE\_CURRENT\_SOURCE\_DIR}/src/RemoteryConfig.h )
```

Replace it with:

```cmake
if( APPLE )
    set( RMT\_USE\_METAL 1 )
endif()
if( WIN32 )
    target\_link\_libraries( Remotery ws2\_32 )
endif()
configure\_file( RemoteryConfig.h.in ${CMAKE\_CURRENT\_SOURCE\_DIR}/src/RemoteryConfig.h )
```



Remotery is used to survey the performance of program parts of Ogre-next. It's like a phone operator that from time to time calls in to say where the work stands. For that it uses a phone line. Remotery knows how to make calls, but phone calls need to be made through the Windows phone call company (e.g. Swisscom, not Salt; here "Remotery ws2\_32"), otherwise it doesn't work. 



### 5.7 Adapt zziplib's read and write functions

The old zziplib interface uses a 64-bit length on this platform, while the Windows `\_read` and `\_write` functions accept a 32-bit count. Small wrapper functions make that conversion explicitly. A single `\_read` or `\_write` call is therefore limited to `0xffffffff` bytes, which is already far larger than the files used by this example.

Open:

```text
D:\\Users\\thoma\\Documents\\git\\ogre-next-test\\ogre-next-deps\\src\\zziplib\\zzip\\plugin.c
```

Search for:

```c
#include <zzip/format.h>
```

Immediately after that line, insert:

```c
#if defined(\_\_MINGW32\_\_)
static zzip\_ssize\_t zzip\_mingw\_read(int fd, void \*buf, zzip\_size\_t len)
{
    if (len > 0xffffffffu)
        len = 0xffffffffu;
    return (zzip\_ssize\_t) \_read(fd, buf, (unsigned int) len);
}

static zzip\_ssize\_t zzip\_mingw\_write(int fd, const void \*buf, zzip\_size\_t len)
{
    if (len > 0xffffffffu)
        len = 0xffffffffu;
    return (zzip\_ssize\_t) \_write(fd, buf, (unsigned int) len);
}
#endif
```

In the `default\_io` block, search for:

```c
    \&\_zzip\_read,
```

Replace it with:

```c
#if defined(\_\_MINGW32\_\_)
    \&zzip\_mingw\_read,
#else
    \&\_zzip\_read,
#endif
```

In the same block, search for:

```c
    \&\_zzip\_write
```

Replace it with:

```c
#if defined(\_\_MINGW32\_\_)
    \&zzip\_mingw\_write
#else
    \&\_zzip\_write
#endif
```



This one goes the other way around: Windows has increased integer size, but we use a zip utility that hasn't :(



### 5.8 Check the dependency edits

Run:

```bat
git status --short
git diff --stat
```

The changed paths should be:

```text
src/CMakeLists.txt
src/FreeImage/CMakeLists.txt
src/FreeImage/Source/LibJXR/common/include/guiddef.h  deleted
src/FreeImage/Source/OpenEXR/Imath/ImathVec.h
src/Remotery/CMakeLists.txt
src/Remotery/src/Remotery.c
src/zziplib/zzip/plugin.c
```







This just make sure that we actually edited the targetted files 



## 6\. Configure, build, and install the dependencies

Run:

```bat
cmake -S . -B build ^
  -G "MinGW Makefiles" ^
  -DCMAKE\_BUILD\_TYPE=Release ^
  -DCMAKE\_MAKE\_PROGRAM=mingw32-make.exe ^
  -DCMAKE\_C\_COMPILER="%CB\_MINGW%\\bin\\gcc.exe" ^
  -DCMAKE\_CXX\_COMPILER="%CB\_MINGW%\\bin\\g++.exe" ^
  -DCMAKE\_RC\_COMPILER=windres.exe
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

Check the installed dependency SDK:

```bat
dir build\\ogredeps\\include\\FreeImage.h
dir build\\ogredeps\\include\\SDL2\\SDL.h
dir build\\ogredeps\\include\\shaderc\\shaderc.h
dir build\\ogredeps\\lib\\Release\\libFreeImage.a
dir build\\ogredeps\\lib\\libSDL2.dll.a
dir build\\ogredeps\\lib\\libshaderc\_combined.a
dir build\\ogredeps\\bin\\Release\\SDL2.dll
```

Every file must exist. The installed dependency SDK is:

```text
D:\\Users\\thoma\\Documents\\git\\ogre-next-test\\ogre-next-deps\\build\\ogredeps
```

## 7\. Clone OGRE-Next

Run:

```bat
cd /d D:\\Users\\thoma\\Documents\\git\\ogre-next-test

git clone --branch v3-0 https://github.com/OGRECave/ogre-next.git
cd ogre-next
git checkout 4859016ad2cc956a653dfc4e4a859505696c6509
git rev-parse HEAD
```

The final command must print:

```text
4859016ad2cc956a653dfc4e4a859505696c6509
```

## 8\. Correct the OGRE-Next source code

### 8.1 Link the Windows libraries explicitly

OGRE-Next uses the Direct3D shader compiler and the Windows Process Status API. Microsoft compilers can obtain these libraries through compiler-specific source directives, but MinGW needs them listed in the CMake link libraries.

Open:

```text
D:\\Users\\thoma\\Documents\\git\\ogre-next-test\\ogre-next\\CMake\\Packages\\FindDirectX11.cmake
```

Search for the line beginning with `set(DirectX11\_LIBRARY`:

```cmake
set(DirectX11\_LIBRARY d3d11.lib dxgi.lib dxguid.lib)
```

Replace it with:

```cmake
set(DirectX11\_LIBRARY d3d11.lib dxgi.lib dxguid.lib d3dcompiler.lib)
if(MINGW)
    list(APPEND DirectX11\_LIBRARY psapi.lib)
endif()
```

### 8.2 Provide a Direct3D flag missing from this MinGW header set

`D3D11\_RLDO\_IGNORE\_INTERNAL` asks the Direct3D debugging interface to ignore objects held only by internal references. The value is `0x4` in the Windows SDK, but the MinGW header supplied with this Code::Blocks compiler does not declare it.

Open:

```text
D:\\Users\\thoma\\Documents\\git\\ogre-next-test\\ogre-next\\RenderSystems\\Direct3D11\\src\\OgreD3D11Device.cpp
```

Search for:

```cpp
#include "OgreException.h"
```

Immediately after that line, insert:

```cpp
#ifndef D3D11\_RLDO\_IGNORE\_INTERNAL
#define D3D11\_RLDO\_IGNORE\_INTERNAL ( (D3D11\_RLDO\_FLAGS)0x4 )
#endif
```

### 8.3 Check the OGRE-Next edits

Run:

```bat
git status --short
git diff --check
git diff --stat
```

The changed paths should be:

```text
CMake/Packages/FindDirectX11.cmake
RenderSystems/Direct3D11/src/OgreD3D11Device.cpp
```

`git diff --check` should print nothing.

## 9\. Configure, build, and install OGRE-Next

Run:

```bat
cmake -S . -B build ^
  -G "MinGW Makefiles" ^
  -DCMAKE\_BUILD\_TYPE=Release ^
  -DCMAKE\_MAKE\_PROGRAM=mingw32-make.exe ^
  -DCMAKE\_C\_COMPILER="%CB\_MINGW%\\bin\\gcc.exe" ^
  -DCMAKE\_CXX\_COMPILER="%CB\_MINGW%\\bin\\g++.exe" ^
  -DCMAKE\_RC\_COMPILER=windres.exe ^
  -DOGRE\_DEPENDENCIES\_DIR=D:/Users/thoma/Documents/git/ogre-next-test/ogre-next-deps/build/ogredeps ^
  -DCMAKE\_INSTALL\_PREFIX=D:/Users/thoma/Documents/git/ogre-next-test/install

cmake --build build --parallel 4
cmake --install build
```

Check the installed SDK:

```bat
dir ..\\install\\include\\OGRE\\OgreRoot.h
dir ..\\install\\include\\OGRE\\Hlms\\Unlit\\OgreHlmsUnlit.h
dir ..\\install\\include\\OGRE\\Hlms\\Pbs\\OgreHlmsPbs.h
dir ..\\install\\lib\\Release\\libOgreMain.dll.a
dir ..\\install\\bin\\Release\\OgreMain.dll
dir ..\\install\\bin\\Release\\RenderSystem\_Direct3D11.dll
dir ..\\install\\bin\\Release\\RenderSystem\_GL3Plus.dll
dir ..\\install\\bin\\Release\\RenderSystem\_Vulkan.dll
dir ..\\install\\bin\\Release\\plugins.cfg
dir ..\\install\\bin\\Release\\resources2.cfg
dir ..\\install\\Media\\Hlms\\Unlit
dir ..\\install\\Media\\Hlms\\Pbs
```

Every file or directory must exist. The installed OGRE-Next SDK is:

```text
D:\\Users\\thoma\\Documents\\git\\ogre-next-test\\install
```

## 10\. Build the independent test program

Run:

```bat
cd /d D:\\Users\\thoma\\Documents\\git\\ogre-next-test

cmake -S window-test -B window-test\\build ^
  -G "MinGW Makefiles" ^
  -DCMAKE\_BUILD\_TYPE=Release ^
  -DCMAKE\_MAKE\_PROGRAM=mingw32-make.exe ^
  -DCMAKE\_C\_COMPILER="%CB\_MINGW%\\bin\\gcc.exe" ^
  -DCMAKE\_CXX\_COMPILER="%CB\_MINGW%\\bin\\g++.exe" ^
  -DOGRE\_SDK=D:/Users/thoma/Documents/git/ogre-next-test/install

cmake --build window-test\\build --parallel 4
dir window-test\\build\\window-test.exe
```

The example must use the installed SDK. Do not add `ogre-next` or `ogre-next\\build` to its include or library paths.

## 11\. Run the test program

Run:

```bat
copy /Y window-test\\build\\window-test.exe install\\bin\\Release\\window-test.exe
cd /d D:\\Users\\thoma\\Documents\\git\\ogre-next-test\\install\\bin\\Release
window-test.exe
```

The build is successful when:

1. a window titled `Tutorial 00: Basic` opens;
2. the window interior is blue;
3. the console reports the selected render system;
4. closing the window prints `Rendered and closed cleanly.`;
5. `Ogre.log` records successful initialization and clean shutdown.

## 12\. What happened?

```text
Git downloaded exact source revisions.
We made the required MinGW compatibility edits.
CMake configured the sources for the Code::Blocks compiler.
MinGW built the dependencies.
CMake installed a clean dependency SDK.
MinGW built OGRE-Next against that SDK.
CMake installed a clean OGRE-Next SDK.
The small program used only the installed SDK and opened a rendered window.
```

