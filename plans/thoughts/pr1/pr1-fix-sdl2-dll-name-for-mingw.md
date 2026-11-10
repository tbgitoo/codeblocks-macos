## Summary

While building `ogre-next-deps` with a MinGW toolchain, I encountered a DLL naming issue in the `SDL2` target.

`ogre-next-deps` includes `SDL2` as a submodule. With the current `SDL2` submodule and the tested x86_64 MinGW-w64 toolchain, `SDL2` builds the runtime library as `SDL2.dll`. The `ogre-next-deps` post-install step, however, expects `libSDL2.dll` for MinGW builds. As a result, the `SDL2` target fails in `MoveInstallFiles` after `SDL2` itself has built and installed successfully.

This PR proposes changing the expected MinGW runtime DLL name to `SDL2.dll`. With this one-line change, the targeted `SDL2` build completes successfully.

## Change

```diff
-    set(SDL2_BIN_NAME "libSDL2.dll")
+    set(SDL2_BIN_NAME "SDL2.dll")
```

## Test environment

- `ogre-next-deps` commit: `f7e23191f4fd89ed41134996bc47f9ee23d009df`
- Code::Blocks: 25.03, installed from the Windows `setup.exe`
- Compiler: GCC 14.2.0, target `x86_64-w64-mingw32`
- Generator: `MinGW Makefiles`
- Build type: `Release`
- Target tested: `SDL2`

The Code::Blocks MinGW toolchain was selected explicitly:

```bat
set "CB_MINGW=D:\Program Files\CodeBlocks\MinGW"
set "PATH=%CB_MINGW%\bin;%PATH%"
set "CC=%CB_MINGW%\bin\gcc.exe"
set "CXX=%CB_MINGW%\bin\g++.exe"
```

Configuration command:

```bat
cmake -S . -B build ^
  -G "MinGW Makefiles" ^
  -DCMAKE_BUILD_TYPE=Release ^
  -DCMAKE_MAKE_PROGRAM=mingw32-make.exe ^
  -DCMAKE_C_COMPILER="%CB_MINGW%\bin\gcc.exe" ^
  -DCMAKE_CXX_COMPILER="%CB_MINGW%\bin\g++.exe" ^
  -DCMAKE_RC_COMPILER=windres.exe
```

Targeted build command:

```bat
cmake --build build --target SDL2
```

## Behavior before the change

Configuration succeeds, and SDL2 builds and installs its files. The final `MoveInstallFiles` step then fails:

```text
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
```

`SDL2-MoveInstallFiles-err.log` identifies the missing filename:

```text
Error renaming from "bin/libSDL2.dll" to "bin/Debug/libSDL2.dll": No such file or directory
CMake Error at ogre-next-deps-pr1/build/src/SDL2-prefix/src/SDL2-stamp/SDL2-MoveInstallFiles-Release-impl.cmake:39 (message):
  Command failed (1):

   'D:/Program Files/CodeBlocks/MinGW/bin/cmake.exe' '-E' 'rename' 'bin/libSDL2.dll' 'bin/Debug/libSDL2.dll'
```

At that point, the installed runtime DLL is present as:

```text
build/ogredeps/bin/SDL2.dll
```

There is no `libSDL2.dll`.

## Behavior after the change

Using the same configuration and targeted build command, the SDL2 post-install step succeeds:

```text
-- Installing: ogre-next-deps-pr1/build/ogredeps/share/aclocal/sdl2.m4
[100%] Move SDL2 install files into the Release and Debug folders.
-- SDL2 MoveInstallFiles command succeeded. See also ogre-next-deps-pr1/build/src/SDL2-prefix/src/SDL2-stamp/SDL2-MoveInstallFiles-*.log
[100%] Completed 'SDL2'
[100%] Built target SDL2
```

The resulting MinGW files use the expected Windows/GNU naming split:

```text
build/ogredeps/bin/SDL2.dll
build/ogredeps/lib/libSDL2.dll.a
```

`SDL2.dll` is the runtime DLL; `libSDL2.dll.a` is the MinGW import library used while linking.
