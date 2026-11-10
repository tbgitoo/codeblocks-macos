# Select LibJXR MinGW-w64 code path in CMake build

## Summary

While building the `FreeImage` target in `ogre-next-deps` with MinGW-w64, the build fails in Windows headers with errors around `\_\_uuidof`, including:

```text
error: `\_\_mingw\_uuidof` was not declared in this scope
error: expected primary-expression before `\_\_typeof\_\_`
```

The current FreeImage CMake configuration defines LibJXR's `\_\_ANSI\_\_` platform switch for every non-MSVC build, including MinGW builds on Windows. 



This selects LibJXR's ANSI platform definitions even though the build is targeting Windows.

LibJXR already provides a dedicated `\_\_MINGW64\_TOOLCHAIN\_\_` switch, and FreeImage's MinGW build configuration uses that switch. This PR aligns the CMake build with that existing MinGW-w64 path by:

1. not defining `\_\_ANSI\_\_` on Windows; and
2. defining `\_\_MINGW64\_TOOLCHAIN\_\_` for MinGW builds.

## Change

In `src/FreeImage/CMakeLists.txt`, two commits:

First commit:

```cmake
if( NOT MSVC AND NOT WIN32 )
    add\_definitions( -D\_\_ANSI\_\_ )
endif()
```

The first commit adds "NOT WIN32" to avoid that the LibJXR `\_\_ANSI\_\_` platform is activated on win64.



Second commit:

```
if( MINGW )
    add\_definitions( -D\_\_MINGW64\_TOOLCHAIN\_\_ )
endif()
```

The second commit correctly the present failure mode by using LibJXR's pre-configured \_\_MINGW64\_TOOLCHAIN\_\_ switch.



These two changes form one platform-selection fix: Windows/MinGW builds no longer select LibJXR's ANSI path and instead select LibJXR's existing MinGW-w64 path.



## Test environment

The following details are included for reproduction:

* `ogre-next-deps` base commit: `f7e23191f4fd89ed41134996bc47f9ee23d009df`
* CMake generator: `MinGW Makefiles`
* Build type: `Release`
* Target tested: `FreeImage`
* Toolchain: MSYS2 MinGW-w64, GCC 15.2.0, target `x86\_64-w64-mingw32`

The MSYS2 toolchain was selected with:

```bat
set "MSYS\_MINGW=D:\\msys64\\mingw64"
set "PATH=%MSYS\_MINGW%\\bin;%PATH%"
```

Configuration command:

```bat
cmake -S . -B build-freeimage-baseline ^
  -G "MinGW Makefiles" ^
  -DCMAKE\_BUILD\_TYPE=Release ^
  -DCMAKE\_MAKE\_PROGRAM=mingw32-make.exe ^
  -DCMAKE\_C\_COMPILER=gcc.exe ^
  -DCMAKE\_CXX\_COMPILER=g++.exe ^
  -DCMAKE\_RC\_COMPILER=windres.exe
```

Targeted build command:

```bat
cmake --build build-freeimage-baseline --target FreeImage
```

## Behavior before the change

Configuration succeeds, but the unmodified `FreeImage` target fails early while compiling `Source/FreeImage/FreeImage.cpp`.

The compiler reaches the Windows headers through `windows.h`, then fails in `unknownbase.h` while expanding `\_\_uuidof`:

```text
\[  3%] Building CXX object src/FreeImage/CMakeFiles/FreeImage.dir/Source/FreeImage/FreeImage.cpp.obj
In file included from D:/msys64/mingw64/include/windows.h:9,
                 from src/FreeImage/Source/FreeImage/FreeImage.cpp:26:
D:/msys64/mingw64/include/unknownbase.h: In member function 'HRESULT IUnknown::QueryInterface(Q\*\*)':
D:/msys64/mingw64/include/unknownbase.h:84:29: error: '\_\_mingw\_uuidof' was not declared in this scope
D:/msys64/mingw64/include/unknownbase.h:84:29: error: expected primary-expression before '\_\_typeof\_\_'
```

This is consistent with the build selecting LibJXR's ANSI configuration while compiling against the MinGW-w64 Windows headers.



## Behavior after the change

Using the same MSYS2 MinGW-w64 toolchain, CMake generator, build type, and targeted `FreeImage` build, the patched configuration selects LibJXR's existing MinGW-w64 code path.

<!-- Replace this comment with the successful final build lines after running the patched control, for example:

```text
\[100%] Built target FreeImage
```

