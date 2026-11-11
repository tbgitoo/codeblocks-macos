## Summary

Fix the bundled zziplib build with 64-bit MinGW-w64.

zziplib uses a callback table for low-level file reads and writes. On 64-bit
Windows, zziplib can request a number of bytes using a 64-bit zzip_size_t,
while the MinGW-w64 CRT read/write functions accept a 32-bit unsigned int.
Current GCC rejects assigning these functions directly to the zziplib
callbacks because their parameter types do not match.

This change adds small MinGW-w64 adapter functions with the callback signature
expected by zziplib in src/zziplib/zzip/plugin.c.

There is a second size constraint: zziplib reports the number of bytes
actually transferred using zzip_ssize_t, which is a 32-bit signed int in this
configuration. Each individual read/write operation is therefore limited to
INT_MAX (2 GiB - 1 byte). A larger request is handled as a partial transfer,
which the relevant zziplib read paths already support.

## Reproduction

Tested from current upstream/master with x86_64 MSYS2 MinGW-w64 and GCC 15.2.

Toolchain:

    set "MSYS_MINGW=D:\msys64\mingw64"
    set "PATH=%MSYS_MINGW%\bin;%PATH%"

    gcc --version
    g++ --version
    where gcc
    where g++
    where mingw32-make

Configure:

    cmake -S . -B build-zziplib-baseline ^
      -G "MinGW Makefiles" ^
      -DCMAKE_BUILD_TYPE=Release ^
      -DCMAKE_MAKE_PROGRAM=mingw32-make.exe ^
      -DCMAKE_C_COMPILER=gcc.exe ^
      -DCMAKE_CXX_COMPILER=g++.exe ^
      -DCMAKE_RC_COMPILER=windres.exe

Build:

    cmake --build build-zziplib-baseline --target zziplib

On the unmodified sources, compilation fails in
src/zziplib/zzip/plugin.c when initializing the default I/O table:

    error: initialization of
    'zzip_ssize_t (*)(int, void *, zzip_size_t)'
    from incompatible pointer type
    'int (*)(int, void *, unsigned int)'

The equivalent mismatch is also reported for the write callback.

The callback expects a zzip_size_t byte count, which is 64-bit on this
platform, while the MinGW-w64 CRT read/write functions accept an unsigned int
byte count.

## Validation

After applying the change, configure a clean build directory with the same
toolchain:

    set "MSYS_MINGW=D:\msys64\mingw64"
    set "PATH=%MSYS_MINGW%\bin;%PATH%"


    cmake -S . -B build-zziplib-test ^
      -G "MinGW Makefiles" ^
      -DCMAKE_BUILD_TYPE=Release ^
      -DCMAKE_MAKE_PROGRAM=mingw32-make.exe ^
      -DCMAKE_C_COMPILER=gcc.exe ^
      -DCMAKE_CXX_COMPILER=g++.exe ^
      -DCMAKE_RC_COMPILER=windres.exe

Then build:

    cmake --build build-zziplib-test --target zziplib

The zziplib target builds successfully with the MinGW-w64 adapters.