OGRE-Next on Windows with Code::Blocks MinGW

This guide builds OGRE-Next with the MinGW compiler supplied by Code::Blocks, installs a clean SDK, and runs a small independent program that opens a rendered window.

The first build is large. Allow approximately 1 to 2 hours and keep the same Command Prompt open throughout.


0. General approach ======================================================

This is OGRE-Next plus the necessary ogre-next-deps.

We want to get this compiling using a Code::Blocks MINGW tool chain.

So what is done is generally download the source, patch, build, install.



1. Versions and files used ===============================================

We want to be reproducible, that's why we use exact versions. We pin these:

Exact source revisions:

ogre-next-deps  f7e23191f4fd89ed41134996bc47f9ee23d009df
OGRE-Next       4859016ad2cc956a653dfc4e4a859505696c6509
                branch v3-0, version Eris 3.0.0
                
This is our tool chain:

Code::Blocks compiler:

D:\Program Files\CodeBlocks\MinGW

And this is our test: This must compile

D:\Users\thoma\Documents\git\codeblocks-macos\plans\thoughts\ogre_next_example\Tutorial00_Basic.cpp
D:\Users\thoma\Documents\git\codeblocks-macos\plans\thoughts\ogre_next_example\CMakeLists.txt

Now to the protocol: First, use Command Prompt, not PowerShell. On Windows, press Windows+R, enter cmd, and press Enter

2. Prepare a clean working folder =======================================================s

If there are remants from a previous build, remove D:\Users\thoma\Documents\git\ogre-next-test (remove only the ogre-next-test folder) 

Open Command Prompt and run:

mkdir D:\Users\thoma\Documents\git\ogre-next-test
cd /d D:\Users\thoma\Documents\git\ogre-next-test

mkdir window-test
copy /Y "D:\Users\thoma\Documents\git\codeblocks-macos\plans\thoughts\ogre_next_example\Tutorial00_Basic.cpp" window-test\
copy /Y "D:\Users\thoma\Documents\git\codeblocks-macos\plans\thoughts\ogre_next_example\CMakeLists.txt" window-test\
copy /Y "D:\Users\thoma\Documents\git\codeblocks-macos\plans\thoughts\ogre_next_example\ogrehead.mesh" window-test\


Make sure things worked as expected:

dir window-test\CMakeLists.txt
dir window-test\Tutorial00_Basic.cpp
dir window-test\ogrehead.mesh

All three files must be listed. The folders ogre-next-deps, ogre-next, and install should not exist yet.

3. Select the Code::Blocks compiler ======================================================s

Run:

set "CB_MINGW=D:\Program Files\CodeBlocks\MinGW"
set "PATH=%CB_MINGW%\bin;%PATH%"
set "CC=%CB_MINGW%\bin\gcc.exe"
set "CXX=%CB_MINGW%\bin\g++.exe"

And test:

where gcc
where g++
where ar
where mingw32-make
where windres

gcc -dumpmachine
gcc --version
cmake --version
git --version

The first path printed for gcc, g++, ar, mingw32-make, and windres must begin with:

D:\Program Files\CodeBlocks\MinGW\bin

gcc -dumpmachine must print:

x86_64-w64-mingw32

The tested compiler is GCC 14.2.0. If a new Command Prompt is opened later, repeat this section.

The reason for doing this configuration is that otherwise, the compilation will mix different compiler and tooling and that's going to be a mess.

4. Clone the dependencies ===========================================================s

In this section we get ogre-next-deps from the internet. We also pin the versions.

Run:

cd /d D:\Users\thoma\Documents\git\ogre-next-test

git clone https://github.com/OGRECave/ogre-next-deps.git
cd ogre-next-deps
git checkout f7e23191f4fd89ed41134996bc47f9ee23d009df
git submodule update --init --recursive --depth 1

git rev-parse HEAD
git -C src\SDL2 rev-parse HEAD

Expected revisions:

ogre-next-deps  f7e23191f4fd89ed41134996bc47f9ee23d009df
SDL2            5d249570393f7a37e037abf22cd6012a4cc56a71

Here we get our source from internet. We want an exactly identical starting point if we have to repeat.

5. Correct the dependency source code ======================================================s

Apply patch

cd /d D:\Users\thoma\Documents\git\ogre-next-test

copy /Y D:\Users\thoma\Documents\git\codeblocks-macos\src\plugins\scriptedwizard\resources\patches\ogre-next-deps-mingw.patch ogre-next-deps

cd /d D:\Users\thoma\Documents\git\ogre-next-test\ogre-next-deps

git apply ogre-next-deps-mingw.patch


======================== If you want to apply the patch by hand, here's the instructions ======================================

In this section we have to do little updates to the code. These are related to Windows 64 bits and the specific mingw environment from Code::Blocks that we are using. It won't run without these changes.

Use a text editor. Make only the edits in this section, then save each file.

5.1 Use the Windows SDL2 DLL name

SDL2 is a graphics library used by OGRE-next. It is included as dynamic link library (dll). 
SDL2 creates SDL2.dll on our Windows tool chain. The original build instruction expects the Unix-style name libSDL2.dll, so the expected filename must be corrected.

Open:

D:\Users\thoma\Documents\git\ogre-next-test\ogre-next-deps\src\CMakeLists.txt

Search for:

set(SDL2_BIN_NAME "libSDL2.dll")

Replace it with:

set(SDL2_BIN_NAME "SDL2.dll")

The reason why we have to do this is that are really two conventions of naming library files: the Windows one (SDL2.dll) and the Linux one (libSDL2.so, by extension libSDL2.dll when ported to windows). And well, it's the exact file name that counts so this messes up every once a while.

5.2 Select the Windows version of the JPEG XR code

LibJXR is a library for reading some particular jpeg format sometimes found on Windows.

There is an issue with Integer definitions (number of bytes in an integer) that we need to fix.

Open:

D:\Users\thoma\Documents\git\ogre-next-test\ogre-next-deps\src\FreeImage\CMakeLists.txt

Search for the block starting with # LibJXR and ending with the first endif():

# LibJXR
add_definitions( "-DDISABLE_PERF_MEASUREMENT" )
if( NOT MSVC )
    add_definitions( "-D__ANSI__=1" )
endif()

Replace the complete block with:

# LibJXR
add_definitions( "-DDISABLE_PERF_MEASUREMENT" )
if( NOT MSVC AND NOT WIN32 )
    add_definitions( "-D__ANSI__=1" )
endif()
if( MINGW )
    add_definitions( "-D__MINGW64_TOOLCHAIN__" )
endif()

The reason why we have to do this is historical: People have increased the size of integers as machines grew bigger. But some old code has survived.

5.3 Use MinGW's Windows GUID header

Here we get rid of duplicate header file definition that otherwise causes a crash during compilation.

Run in Command Prompt:

del src\FreeImage\Source\LibJXR\common\include\guiddef.h

Long ago, LibJXR needed the Windows definitions from a file called guiddef.h, so a copy of that file was included inside LibJXR. Meanwhile, Windows and MinGW continued evolving their own guiddef.h. The two copies kept the same name but no longer evolved together. Now they are conflicting, so we need to remove one.

On those guid things: Windows uses GUIDs (Globally Unique Identifiers) to identify many kinds of objects and definitions. Among these things are pixel formats - how colors are stored, and LibJXR needs to use these GUIDS to access to color formats. Unfortunately, the file defining that GUID format for C has become duplicated in different places and now there are collisions. We need to take care of this.

5.4 Include the fixed-width integer definitions in Imath

Here, in an image math library, we need to explicitly provide integer definitions (machine format for integers)

Open:

D:\Users\thoma\Documents\git\ogre-next-test\ogre-next-deps\src\FreeImage\Source\OpenEXR\Imath\ImathVec.h

Search for:

#include "ImathMath.h"

#include <iostream>

Replace it with:

#include "ImathMath.h"

#include <cstdint>
#include <iostream>

This one is from the deep ocean floor of image mathematics. Here, IMath expects the integer sizes to be predefined, but does not explicitly include the necessary "hashtag file". Fortunately, including the cstdint "hashtag file" is enough (this provides machine integer definitions).

More precisely, the bundled Imath code uses names such as int64_t. Modern C++ declares these names in <cstdint>, so the header should include what it uses instead of depending on another header to include it indirectly.

5.5 Keep MinGW's 64-bit rsize_t

Another correction related to integer sizes.

Open:

D:\Users\thoma\Documents\git\ogre-next-test\ogre-next-deps\src\Remotery\src\Remotery.c

Search for:

#if (!defined(_WIN64) && !defined(__APPLE__)) || (defined(__MINGW32__) && !defined(RSIZE_T_DEFINED))
typedef unsigned int rsize_t;
#endif

Replace it with:

#if !defined(_WIN64) && !defined(__APPLE__) && !defined(__MINGW32__)
typedef unsigned int rsize_t;
#endif

Windows changed its addressing system on the road from 32-bit to 64-bit. Think of going from "Fritz Muller, Bern" to "Fritz Muller, Langgasse, Bern": the new address needs a larger envelope to write everything down.

Here we are not directly changing the addresses themselves, but the size of the "envelopes" used to describe sizes in memory (rsize_t). MinGW already knows that the modern 64-bit format needs a larger envelope. Remotery contains an older compatibility rule that tries to replace it with the old, smaller format. We therefore ask Remotery to leave MinGW's modern definition alone.



5.6 Link Remotery with Windows sockets

Here, we are including a library explicitly that is not found on mingw without our intervention.

Open:

D:\Users\thoma\Documents\git\ogre-next-test\ogre-next-deps\src\Remotery\CMakeLists.txt

Search for:

if( APPLE )
    set( RMT_USE_METAL 1 )
endif()
configure_file( RemoteryConfig.h.in ${CMAKE_CURRENT_SOURCE_DIR}/src/RemoteryConfig.h )

Replace it with:

if( APPLE )
    set( RMT_USE_METAL 1 )
endif()
if( WIN32 )
    target_link_libraries( Remotery ws2_32 )
endif()
configure_file( RemoteryConfig.h.in ${CMAKE_CURRENT_SOURCE_DIR}/src/RemoteryConfig.h )

Remotery is used to survey the performance of program parts of Ogre-next. It's like a phone operator that from time to time calls in to say where the work stands. For that it uses a phone line. Remotery knows how to make calls, but phone calls need to be made through the Windows phone call company (e.g. Swisscom, not Salt; here "Remotery ws2_32"), otherwise it doesn't work.

More precisely, this bug arises because Microsoft compilers can obtain some Windows libraries implicitly, while MinGW expects the build file to name the Winsock library explicitly.


5.7 Adapt zziplib's read and write functions

Another integer size issue, now in zipping/unzipping.

Open:

D:\Users\thoma\Documents\git\ogre-next-test\ogre-next-deps\src\zziplib\zzip\plugin.c

Search for:

#include <zzip/format.h>

Immediately after that line, insert:

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

In the default_io block, search for:

    &_zzip_read,

Replace it with:

#if defined(__MINGW32__)
    &zzip_mingw_read,
#else
    &_zzip_read,
#endif

In the same block, search for:

    &_zzip_write

Replace it with:

#if defined(__MINGW32__)
    &zzip_mingw_write
#else
    &_zzip_write
#endif

This one goes the other way around: Windows has increased integer size, but we use a zip utility that hasn't :(

More precisely: The old zziplib interface uses a 64-bit length on this platform, while the Windows _read and _write functions accept a 32-bit count. Small wrapper functions make that conversion explicitly. A single _read or _write call is therefore limited to 0xffffffff bytes, which is already far larger than the files used by this example.




5.8 Check the dependency edits

This is to check again that all the edits have been made.

Run:

git status --short
git diff --stat

The changed paths should be:

src/CMakeLists.txt
src/FreeImage/CMakeLists.txt
src/FreeImage/Source/LibJXR/common/include/guiddef.h  deleted
src/FreeImage/Source/OpenEXR/Imath/ImathVec.h
src/Remotery/CMakeLists.txt
src/Remotery/src/Remotery.c
src/zziplib/zzip/plugin.c

It should be exactly those files and nothing else.


=========================== end of instructions for manual patching ===============================================

6. Configure, build, and install the dependencies =========================================

This to prepare the compilation, it explores the tools we have and sets all sorts of options correspondingly. This is necessary to make compilation possible on our machine.

Run:

cmake -S . -B build ^
  -G "MinGW Makefiles" ^
  -DCMAKE_BUILD_TYPE=Release ^
  -DCMAKE_MAKE_PROGRAM=mingw32-make.exe ^
  -DCMAKE_C_COMPILER="%CB_MINGW%\bin\gcc.exe" ^
  -DCMAKE_CXX_COMPILER="%CB_MINGW%\bin\g++.exe" ^
  -DCMAKE_RC_COMPILER=windres.exe

The last line should say that build files were written to:

D:/Users/thoma/Documents/git/ogre-next-test/ogre-next-deps/build

Now we can actually compile ("build"), and then install the resulting files as a Software Development Kit SDK (a collection of library files)

Build and install:

cmake --build build --parallel 4
cmake --install build

Check the installed dependency SDK:

dir build\ogredeps\include\FreeImage.h
dir build\ogredeps\include\SDL2\SDL.h
dir build\ogredeps\include\shaderc\shaderc.h
dir build\ogredeps\lib\Release\libFreeImage.a
dir build\ogredeps\lib\libSDL2.dll.a
dir build\ogredeps\lib\libshaderc_combined.a
dir build\ogredeps\bin\Release\SDL2.dll

Every file must exist. The installed dependency SDK is:

D:\Users\thoma\Documents\git\ogre-next-test\ogre-next-deps\build\ogredeps

7. Clone OGRE-Next ==================================================================

With steps 1-6 we have now the complete set of dependencies that we need, properly installed as SDK.

We now download, patch and compile ogre-next itself.

Run:

cd /d D:\Users\thoma\Documents\git\ogre-next-test

git clone --branch v3-0 https://github.com/OGRECave/ogre-next.git
cd ogre-next
git checkout 4859016ad2cc956a653dfc4e4a859505696c6509
git rev-parse HEAD

The final command must print:

4859016ad2cc956a653dfc4e4a859505696c6509

8. Patchhing: Correct the OGRE-Next source code ===========================================

8.1 Link the Windows libraries explicitly

Another library that is found on some compilers but not this one.

Open:

D:\Users\thoma\Documents\git\ogre-next-test\ogre-next\CMake\Packages\FindDirectX11.cmake

Search for the line beginning with set(DirectX11_LIBRARY:

set(DirectX11_LIBRARY d3d11.lib dxgi.lib dxguid.lib)

OGRE-Next uses the Direct3D shader compiler and the Windows Process Status API. Microsoft compilers can obtain these libraries through compiler-specific source directives, but MinGW needs them listed in the CMake link libraries.

Replace it with:

set(DirectX11_LIBRARY d3d11.lib dxgi.lib dxguid.lib d3dcompiler.lib)
if(MINGW)
    list(APPEND DirectX11_LIBRARY psapi.lib)
endif()

8.2 Provide a Direct3D flag missing from this MinGW header set

Here we patch a missing flag, i.e. we substitute it directly where it is needed because the mingw headers don't define it while other compilers do.

Open:

D:\Users\thoma\Documents\git\ogre-next-test\ogre-next\RenderSystems\Direct3D11\src\OgreD3D11Device.cpp

Search for:

#include "OgreException.h"

Immediately after that line, insert:

#ifndef D3D11_RLDO_IGNORE_INTERNAL
#define D3D11_RLDO_IGNORE_INTERNAL ( (D3D11_RLDO_FLAGS)0x4 )
#endif


D3D11_RLDO_IGNORE_INTERNAL asks the Direct3D debugging interface to ignore objects held only by internal references. The value is 0x4 in the Windows SDK, but the MinGW header supplied with this Code::Blocks compiler does not declare it.

8.3 Check the OGRE-Next edits

With this, we are done with the edits, let's check:

Run:

git status --short
git diff --stat

The changed paths should be:

CMake/Packages/FindDirectX11.cmake
RenderSystems/Direct3D11/src/OgreD3D11Device.cpp



9. Configure, build, and install OGRE-Next ===================================================

As above, configuring means setting local options as a function of the detected environment

Run:

cmake -S . -B build ^
  -G "MinGW Makefiles" ^
  -DCMAKE_BUILD_TYPE=Release ^
  -DCMAKE_MAKE_PROGRAM=mingw32-make.exe ^
  -DCMAKE_C_COMPILER="%CB_MINGW%\bin\gcc.exe" ^
  -DCMAKE_CXX_COMPILER="%CB_MINGW%\bin\g++.exe" ^
  -DCMAKE_RC_COMPILER=windres.exe ^
  -DOGRE_DEPENDENCIES_DIR=D:/Users/thoma/Documents/git/ogre-next-test/ogre-next-deps/build/ogredeps ^
  -DCMAKE_INSTALL_PREFIX=D:/Users/thoma/Documents/git/ogre-next-test/install

And then compile

cmake --build build --parallel 4

And install the finished ogre-next SDK

cmake --install build

Check the installed SDK:

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

Every file or directory must exist. The installed OGRE-Next SDK is:

D:\Users\thoma\Documents\git\ogre-next-test\install

10. Build the independent test program ==============================================

Run:

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

The example must use the installed SDK. Do not add ogre-next or ogre-next\build to its include or library paths.

11. Run the test program ========================================================

Run:
cd /d D:\Users\thoma\Documents\git\ogre-next-test
copy /Y window-test\build\window-test.exe install\bin\Release\window-test.exe
copy /Y window-test\ogrehead.mesh install\bin\Release\ogrehead.mesh
cd /d D:\Users\thoma\Documents\git\ogre-next-test\install\bin\Release
window-test.exe


The build is successful when:

1. a window titled OGRE-Next Light Show opens;
2. the Ogre head is visible and rotating;
3. red, blue and gold light move around the Ogre head;
4. the changing illumination is visible on the Ogre head;
5. the console reports the selected render system;
6. closing the window prints Light show rendered and closed cleanly.;
7. Ogre.log records successful initialization and clean shutdown.

12. What happened? =========================================================

Git downloaded exact source revisions.
We made the required MinGW compatibility edits.
CMake configured the sources for the Code::Blocks compiler.
MinGW built the dependencies.
CMake installed a clean dependency SDK.
MinGW built OGRE-Next against that SDK.
CMake installed a clean OGRE-Next SDK.
The small program used only the installed SDK, loaded an Ogre mesh, created moving lights, and rendered an animated 3D scene.


