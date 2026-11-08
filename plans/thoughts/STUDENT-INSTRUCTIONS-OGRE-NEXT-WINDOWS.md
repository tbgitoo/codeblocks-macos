STUDENT INSTRUCTIONS - OGRE-NEXT ON WINDOWS

These steps build OGRE-Next 3.0.0 (Eris) with the MinGW compiler that Code::Blocks is using, then run a small program that opens a rendered window.

The program at the end is supplied with these instructions. You do not type it in.

Before STEP 1, this folder must already contain:

```
D:\Users\thoma\Documents\git\ogre-next-test\patches\ogre-next-deps-mingw.patch
D:\Users\thoma\Documents\git\ogre-next-test\patches\ogre-next-v3-0-mingw.patch
D:\Users\thoma\Documents\git\ogre-next-test\window-test\CMakeLists.txt
D:\Users\thoma\Documents\git\ogre-next-test\window-test\Tutorial00_Basic.cpp
```

Use Command Prompt. Do not use PowerShell for these commands. PowerShell can drop the `.exe` off a CMake argument, and the wrong build tool is then selected.

The dependency build and the OGRE-Next build each take a long time. Leave the window open until the command prints that it finished. Do not start a second build in another window.

================================================================================
STEP 1 - OPEN COMMAND PROMPT AND PREPARE THE WORKING DIRECTORY
================================================================================

Open Command Prompt.

Run:

```
cd /d D:\Users\thoma\Documents\git\ogre-next-test
dir patches\ogre-next-deps-mingw.patch
dir patches\ogre-next-v3-0-mingw.patch
dir window-test\Tutorial00_Basic.cpp
dir window-test\CMakeLists.txt
```

PASS:
All four `dir` commands print the file. You are in `D:\Users\thoma\Documents\git\ogre-next-test`.

FAIL:
Stop. Do not continue. The handout files are missing, or the working directory is wrong. Fix that before configuring anything.

The folders `ogre-next-deps` and `ogre-next` must not exist yet. If either folder is already there, stop and ask for a clean working directory. Do not reuse a half-built tree.

================================================================================
STEP 2 - SET AND VERIFY THE CODE::BLOCKS MINGW ENVIRONMENT
================================================================================

Stay in the same Command Prompt. Run:

```
set "CB_MINGW=D:\Program Files\CodeBlocks\MinGW"
set "PATH=%CB_MINGW%\bin;%PATH%"
set "CC=%CB_MINGW%\bin\gcc.exe"
set "CXX=%CB_MINGW%\bin\g++.exe"
```

Then run:

```
where gcc
where g++
where ar
where mingw32-make
where windres
where cmake
echo %CC%
echo %CXX%
gcc -dumpmachine
gcc --version
cmake --version
```

PASS:
The first line of `where gcc`, `where g++`, `where ar`, `where mingw32-make`, `where windres`, and `where cmake` is under:

```
D:\Program Files\CodeBlocks\MinGW\bin
```

`echo %CC%` prints `D:\Program Files\CodeBlocks\MinGW\bin\gcc.exe`.
`echo %CXX%` prints `D:\Program Files\CodeBlocks\MinGW\bin\g++.exe`.
`gcc -dumpmachine` prints `x86_64-w64-mingw32`.
The first `gcc --version` line contains `14.2.0`.
`cmake --version` prints `cmake version 3.31.5`.

Later lines from `where` may name other copies. Only the first line matters. The space in `Program Files` is expected. Do not move Code::Blocks.

FAIL:
Stop. Do not continue with the dependency build. The compiler that Code::Blocks is using is not the one that will compile the code. Close this Command Prompt, open a new one, and repeat STEP 2. Do not add `C:\msys64`, another MinGW, or another CMake in front of the Code::Blocks `bin` directory.

Every later step must be run in this same Command Prompt. If you open a new one, repeat STEP 2 before doing anything else.

================================================================================
STEP 3 - CLONE THE PINNED ogre-next-deps REVISION
================================================================================

Current directory: `D:\Users\thoma\Documents\git\ogre-next-test`

Run:

```
git clone https://github.com/OGRECave/ogre-next-deps.git
cd /d D:\Users\thoma\Documents\git\ogre-next-test\ogre-next-deps
git checkout f7e23191f4fd89ed41134996bc47f9ee23d009df
git submodule update --init --recursive --depth 1
git rev-parse HEAD
git -C src\SDL2 rev-parse HEAD
```

PASS:
`git rev-parse HEAD` prints:

```
f7e23191f4fd89ed41134996bc47f9ee23d009df
```

`git -C src\SDL2 rev-parse HEAD` prints:

```
5d249570393f7a37e037abf22cd6012a4cc56a71
```

The directory `src\SDL2` exists.

FAIL:
Stop. Do not configure. If `git checkout` says the commit is unknown, the clone is incomplete. Delete the `ogre-next-deps` folder and run STEP 3 again from the working root.

================================================================================
STEP 4 - APPLY THE DEPENDENCY COMPATIBILITY PATCH
================================================================================

Current directory: `D:\Users\thoma\Documents\git\ogre-next-test\ogre-next-deps`

Run:

```
git apply --check ..\patches\ogre-next-deps-mingw.patch
git apply ..\patches\ogre-next-deps-mingw.patch
git status --short
```

PASS:
`git apply --check` prints nothing.
`git status --short` shows exactly these lines:

```
 M src/CMakeLists.txt
 M src/FreeImage/CMakeLists.txt
 D src/FreeImage/Source/LibJXR/common/include/guiddef.h
 M src/FreeImage/Source/OpenEXR/Imath/ImathVec.h
 M src/Remotery/CMakeLists.txt
 M src/Remotery/src/Remotery.c
 M src/zziplib/zzip/plugin.c
```

FAIL:
Stop. Do not configure. If `--check` prints an error, the checkout is not `f7e23191` or the patch file is not the handout file. Do not edit the sources yourself.

================================================================================
STEP 5 - CONFIGURE ogre-next-deps
================================================================================

Current directory: `D:\Users\thoma\Documents\git\ogre-next-test\ogre-next-deps`

Run this as one command. The carets must be the last character on their line.

```
cmake -S . -B build ^
  -G "MinGW Makefiles" ^
  -DCMAKE_BUILD_TYPE=Release ^
  -DCMAKE_MAKE_PROGRAM=mingw32-make.exe ^
  -DCMAKE_C_COMPILER="%CB_MINGW%\bin\gcc.exe" ^
  -DCMAKE_CXX_COMPILER="%CB_MINGW%\bin\g++.exe" ^
  -DCMAKE_RC_COMPILER="%CB_MINGW%\bin\windres.exe"
```

PASS:
The command ends with:

```
-- Build files have been written to: D:/Users/thoma/Documents/git/ogre-next-test/ogre-next-deps/build
```

The folder `build` now exists.
There is no message `Ignoring extra path from command line`.
There is no message that `'D:/Program' is not recognized`.

FAIL:
Stop. Do not build. Check STEP 2 again in this same window. `CMAKE_MAKE_PROGRAM` must be the bare name `mingw32-make.exe`. Do not replace it with the full path under `Program Files`.

================================================================================
STEP 6 - BUILD ogre-next-deps
================================================================================

Current directory: `D:\Users\thoma\Documents\git\ogre-next-test\ogre-next-deps`

Run:

```
cmake --build build --parallel 4
```

PASS:
The command returns to the prompt and does not finish with `Error 1`.
The file `build\src\FreeImage\libFreeImage.a` exists.

FAIL:
Stop. Do not install. If the compiler reports `Error 1455` or `Invalid argument` while opening a header, the machine ran out of memory. Run the same command again:

```
cmake --build build --parallel 4
```

Do not raise the parallel number. Do not delete the `build` folder unless configure itself was wrong.

================================================================================
STEP 7 - INSTALL AND VERIFY THE DEPENDENCY SDK
================================================================================

Current directory: `D:\Users\thoma\Documents\git\ogre-next-test\ogre-next-deps`

Run:

```
cmake --install build
dir build\ogredeps\include\FreeImage.h
dir build\ogredeps\include\ft2build.h
dir build\ogredeps\include\zlib.h
dir build\ogredeps\include\zzip\zzip.h
dir build\ogredeps\include\SDL2\SDL.h
dir build\ogredeps\include\shaderc\shaderc.h
dir build\ogredeps\lib\Release\libFreeImage.a
dir build\ogredeps\lib\Release\libfreetype.a
dir build\ogredeps\lib\Release\libzlib.a
dir build\ogredeps\lib\Release\libzziplib.a
dir build\ogredeps\lib\libSDL2.dll.a
dir build\ogredeps\lib\libshaderc_combined.a
dir build\ogredeps\bin\Release\SDL2.dll
```

PASS:
`cmake --install` finishes without an error.
Every `dir` prints the named file.
The dependency SDK is the folder:

```
D:\Users\thoma\Documents\git\ogre-next-test\ogre-next-deps\build\ogredeps
```

FAIL:
Stop. Do not clone OGRE-Next. If install says a file is missing, the build in STEP 6 did not finish. Return to STEP 6. Do not copy DLLs by hand.

================================================================================
STEP 8 - CLONE THE PINNED OGRE-NEXT REVISION
================================================================================

Run:

```
cd /d D:\Users\thoma\Documents\git\ogre-next-test
git clone --branch v3-0 https://github.com/OGRECave/ogre-next.git
cd /d D:\Users\thoma\Documents\git\ogre-next-test\ogre-next
git checkout 4859016ad2cc956a653dfc4e4a859505696c6509
git rev-parse HEAD
findstr OGRE_VERSION_NAME OgreMain\include\OgrePrerequisites.h
```

PASS:
`git rev-parse HEAD` prints:

```
4859016ad2cc956a653dfc4e4a859505696c6509
```

`findstr` prints:

```
#define OGRE_VERSION_NAME "Eris"
```

FAIL:
Stop. Do not configure OGRE-Next. Delete `ogre-next` and repeat STEP 8. Do not switch to `master`.

================================================================================
STEP 9 - APPLY THE OGRE-NEXT PATCH AND CONFIGURE
================================================================================

Current directory: `D:\Users\thoma\Documents\git\ogre-next-test\ogre-next`

Run:

```
git apply --check ..\patches\ogre-next-v3-0-mingw.patch
git apply ..\patches\ogre-next-v3-0-mingw.patch
git status --short
```

PASS:
`git status --short` shows exactly:

```
 M CMake/Packages/FindDirectX11.cmake
 M RenderSystems/Direct3D11/src/OgreD3D11Device.cpp
```

Then run this as one command:

```
cmake -S . -B build ^
  -G "MinGW Makefiles" ^
  -DCMAKE_BUILD_TYPE=Release ^
  -DCMAKE_MAKE_PROGRAM=mingw32-make.exe ^
  -DCMAKE_C_COMPILER="%CB_MINGW%\bin\gcc.exe" ^
  -DCMAKE_CXX_COMPILER="%CB_MINGW%\bin\g++.exe" ^
  -DCMAKE_RC_COMPILER="%CB_MINGW%\bin\windres.exe" ^
  -DOGRE_DEPENDENCIES_DIR=D:/Users/thoma/Documents/git/ogre-next-test/ogre-next-deps/build/ogredeps ^
  -DCMAKE_INSTALL_PREFIX=D:/Users/thoma/Documents/git/ogre-next-test/install
```

PASS:
The command ends with:

```
-- Build files have been written to: D:/Users/thoma/Documents/git/ogre-next-test/ogre-next/build
```

Then run:

```
findstr /C:"ogre-next-deps/build/ogredeps/lib/Release/libFreeImage.a" build\CMakeCache.txt
findstr /C:"ogre-next-deps/build/ogredeps/lib/Release/libfreetype.a" build\CMakeCache.txt
findstr /C:"ogre-next-deps/build/ogredeps/lib/Release/libzlib.a" build\CMakeCache.txt
findstr /C:"ogre-next-deps/build/ogredeps/lib/Release/libzziplib.a" build\CMakeCache.txt
findstr /C:"ogre-next-deps/build/ogredeps/lib/libSDL2.dll.a" build\CMakeCache.txt
findstr OGRE_BUILD_RENDERSYSTEM_D3D11 build\CMakeCache.txt
findstr OGRE_BUILD_RENDERSYSTEM_GL3PLUS build\CMakeCache.txt
findstr OGRE_BUILD_RENDERSYSTEM_VULKAN build\CMakeCache.txt
```

Each `findstr` prints a line. The three render-system lines end in `=ON`.
A Debug freetype entry that points somewhere else is not a failure. The Release freetype line above must point into `ogredeps`.

FAIL:
Stop. Do not build. If a dependency is reported as not found, STEP 7 did not produce the SDK. If `git apply --check` fails, the checkout is not `4859016`.

================================================================================
STEP 10 - BUILD OGRE-NEXT
================================================================================

Current directory: `D:\Users\thoma\Documents\git\ogre-next-test\ogre-next`

Run:

```
cmake --build build --parallel 4
```

PASS:
The build returns to the prompt after linking the last sample, with no `Error 1` at the end.
The file `build\bin\Release\OgreMain.dll` exists.
The file `build\bin\Release\RenderSystem_Direct3D11.dll` exists.

FAIL:
Stop. Do not install. If you see `Error 1455` or a compiler that cannot open a header that is on disk, run the same command again:

```
cmake --build build --parallel 4
```

Do not raise the parallel number.

================================================================================
STEP 11 - INSTALL AND INSPECT THE OGRE-NEXT SDK
================================================================================

Current directory: `D:\Users\thoma\Documents\git\ogre-next-test\ogre-next`

Run:

```
cmake --install build
dir ..\install\include\OGRE\OgreRoot.h
dir ..\install\include\OGRE\Hlms\Common\OgreHlmsBufferManager.h
dir ..\install\include\OGRE\Hlms\Unlit\OgreHlmsUnlit.h
dir ..\install\include\OGRE\Hlms\Pbs\OgreHlmsPbs.h
dir ..\install\lib\Release\libOgreMain.dll.a
dir ..\install\lib\Release\libOgreHlmsUnlit.dll.a
dir ..\install\lib\Release\libOgreHlmsPbs.dll.a
dir ..\install\bin\Release\OgreMain.dll
dir ..\install\bin\Release\OgreHlmsUnlit.dll
dir ..\install\bin\Release\OgreHlmsPbs.dll
dir ..\install\bin\Release\RenderSystem_Direct3D11.dll
dir ..\install\bin\Release\RenderSystem_GL3Plus.dll
dir ..\install\bin\Release\RenderSystem_Vulkan.dll
dir ..\install\bin\Release\Plugin_ParticleFX.dll
dir ..\install\bin\Release\amd_ags_x64.dll
dir ..\install\bin\Release\plugins.cfg
dir ..\install\bin\Release\resources2.cfg
dir ..\install\Media\Hlms\Unlit
dir ..\install\Media\Hlms\Pbs
```

PASS:
`cmake --install` finishes without an error.
Every `dir` prints the named file or directory.
The installed SDK is:

```
D:\Users\thoma\Documents\git\ogre-next-test\install
```

Open `..\install\bin\Release\plugins.cfg`. It must contain:

```
PluginFolder=.
```

and `PluginOptional` lines for `RenderSystem_Direct3D11`, `RenderSystem_GL3Plus`, and `RenderSystem_Vulkan`.

Open `..\install\bin\Release\resources2.cfg`. The `[Hlms]` section must contain:

```
DoNotUseAsResource=../../Media
```

FAIL:
Stop. Do not compile the test program. If a listed file is missing, STEP 10 did not finish or the install prefix was not `D:\Users\thoma\Documents\git\ogre-next-test\install`.

================================================================================
STEP 12 - CHECK THE INDEPENDENT TEST PROJECT
================================================================================

The test project is already in `window-test`. It is not inside the OGRE-Next source tree. It initializes OGRE the way Tutorial00 does, selects Direct3D11 (then OpenGL 3+, then Vulkan), opens a 1280 by 720 window, clears it to blue, and renders until you close the window.

Run:

```
cd /d D:\Users\thoma\Documents\git\ogre-next-test
dir window-test\CMakeLists.txt
dir window-test\Tutorial00_Basic.cpp
```

PASS:
Both files are listed.

FAIL:
Stop. The handout’s `window-test` folder is missing. Do not copy a tutorial from `ogre-next\Samples` and build it there. That would compile against the source tree instead of the installed SDK.

================================================================================
STEP 13 - CONFIGURE AND BUILD THE TEST PROJECT
================================================================================

Current directory: `D:\Users\thoma\Documents\git\ogre-next-test`

Run:

```
cmake -S window-test -B window-test\build ^
  -G "MinGW Makefiles" ^
  -DCMAKE_BUILD_TYPE=Release ^
  -DCMAKE_MAKE_PROGRAM=mingw32-make.exe ^
  -DCMAKE_C_COMPILER="%CB_MINGW%\bin\gcc.exe" ^
  -DCMAKE_CXX_COMPILER="%CB_MINGW%\bin\g++.exe" ^
  -DOGRE_SDK=D:/Users/thoma/Documents/git/ogre-next-test/install
cmake --build window-test\build --parallel 4
```

PASS:
Configure ends with `Build files have been written to: D:/Users/thoma/Documents/git/ogre-next-test/window-test/build`.
The build ends with:

```
[100%] Built target window-test
```

The file `window-test\build\window-test.exe` exists.

FAIL:
Stop. If the linker cannot find `OgreMain`, STEP 11 did not install `install\lib\Release\libOgreMain.dll.a`. Do not add `ogre-next\build` to the library path.

================================================================================
STEP 14 - PREPARE THE RUNTIME DIRECTORY
================================================================================

The executable has to run with its current directory set to the installed `bin\Release` folder. From there, `PluginFolder=.` loads the plugins beside the executable, and `../../Media` is `install\Media`.

Run:

```
copy /Y window-test\build\window-test.exe install\bin\Release\window-test.exe
dir install\bin\Release\window-test.exe
dir "%CB_MINGW%\bin\libstdc++-6.dll"
dir "%CB_MINGW%\bin\libgcc_s_seh-1.dll"
dir "%CB_MINGW%\bin\libwinpthread-1.dll"
```

PASS:
The copy succeeds and all four `dir` commands print a file.
You do not copy those three MinGW DLLs. They are found because STEP 2 put the Code::Blocks `bin` directory on `PATH`.
You do not copy `SDL2.dll` next to this program. This window does not load it.

FAIL:
Stop. If the MinGW runtime DLLs are not in `D:\Program Files\CodeBlocks\MinGW\bin`, STEP 2 was pointed at the wrong compiler.

================================================================================
STEP 15 - RUN THE TEST PROGRAM
================================================================================

Current directory must become the install bin folder. Run:

```
cd /d D:\Users\thoma\Documents\git\ogre-next-test\install\bin\Release
window-test.exe
```

PASS:
A window titled `Tutorial 00: Basic` opens. Its client area is a solid blue.
The console prints:

```
Selected render system: Direct3D11 Rendering Subsystem
```

Leave the window open for a moment so you can see the colour.

FAIL:
Stop. Do not relaunch from another folder. If the console says `No render system was loaded from plugins.cfg`, the current directory is not `install\bin\Release`. If a DLL is reported missing, check STEP 2 and STEP 14. Read `Ogre.log` in this same folder for the first OGRE error.

================================================================================
STEP 16 - VERIFY THE WINDOW, THEN CLOSE IT
================================================================================

PASS:
You can see the window `Tutorial 00: Basic` with a blue interior. That blue is the clear colour from the Tutorial00 workspace, red 0.2, green 0.4, blue 0.6.

Close the window with its close button.

The console then prints:

```
Rendered and closed cleanly.
```

and the program returns to the Command Prompt.
`Ogre.log` in `install\bin\Release` contains:

```
*** D3D11: Subsystem Initialized OK ***
```

and later:

```
D3D11: Shutting down cleanly.
```

FAIL:
If the window never appears, or the process exits before you close it, the run did not succeed. Read the last lines of `Ogre.log`. Do not mark the step passed because `window-test.exe` exists.

================================================================================
WHAT JUST HAPPENED?
================================================================================

Git got the dependency source at one exact commit, including the SDL2 submodule.
CMake configured that source for the Code::Blocks MinGW compiler.
MinGW compiled it.
CMake installed a clean dependency SDK in `ogre-next-deps\build\ogredeps`.

Git got OGRE-Next 3.0.0 (Eris) at one exact commit on the `v3-0` branch.
CMake configured OGRE-Next against that installed dependency SDK.
MinGW compiled OGRE-Next.
CMake installed a clean OGRE-Next SDK in `install`.

The final program used that installed SDK. It did not include or link the OGRE-Next source tree or the OGRE-Next build tree. It loaded the plugins, HLMS templates, and media from the installed SDK, and it opened a window drawn by OGRE-Next.
