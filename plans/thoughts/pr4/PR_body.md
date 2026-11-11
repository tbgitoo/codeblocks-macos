## Summary

Fix two issues in the bundled Remotery sources when building with current
x86_64 MinGW-w64.

### 1. Avoid redefining rsize_t with MinGW-w64 in `src/Remotery/src/Remotery.c`

Current MinGW-w64 already defines rsize_t through its CRT headers as size_t.
Remotery also provides a fallback definition:

    typedef unsigned int rsize_t;

On x86_64 MinGW-w64 this results in conflicting declarations, since size_t
is 64-bit while unsigned int is 32-bit.

Do not provide Remotery's fallback rsize_t typedef when compiling with
MinGW-w64.

### 2. Link Remotery with Winsock on Windows in `src/Remotery/CMakeLists.txt`

After resolving the rsize_t compilation error, Remotery compiles but fails
while linking with unresolved Windows Sockets API symbols including:

    WSAStartup
    WSAGetLastError
    WSACleanup
    socket
    bind
    listen
    send
    recv
    select
    accept
    closesocket

Remotery uses the Windows Sockets API, so link ws2_32 explicitly on Windows
rather than relying on toolchain-specific implicit linkage.

## Validation

Tested from upstream/master at 4c6e8fd using x86_64 MSYS2 MinGW-w64.

Configuration:

    cmake -S . -B build-remotery-baseline ^
      -G "MinGW Makefiles" ^
      -DCMAKE_BUILD_TYPE=Release ^
      -DCMAKE_MAKE_PROGRAM=mingw32-make.exe ^
      -DCMAKE_C_COMPILER=gcc.exe ^
      -DCMAKE_CXX_COMPILER=g++.exe ^
      -DCMAKE_RC_COMPILER=windres.exe

Build:

    cmake --build build-remotery-baseline --target Remotery

The failure sequence was reproduced incrementally:

1. Pristine upstream fails while compiling Remotery.c because rsize_t is
   redefined.

2. After the first commit, Remotery.c compiles and the build proceeds to
   linking, which fails with unresolved Winsock symbols.

3. After the second commit, the Remotery target builds successfully.

The changes are kept as separate commits because the first fixes a
source-level type conflict while the second declares Remotery's Windows
socket library dependency explicitly.