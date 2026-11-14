# Summary

MinGW needs a definition of the Windows API version being targeted (0x0500 for Windows 2000, 0x0600 for Windows Vista). This value is used by the Windows headers to determine which Windows functions are available to the code being compiled.



ogre-next currently defines `\_WIN32\_WINNT` as 0x0500 for MinGW builds, corresponding to Windows 2000. However, `OgreCondVariableWin32.cpp` uses several condition-variable functions introduced with Windows Vista, including `InitializeConditionVariable`, `SleepConditionVariableCS`, `WakeConditionVariable`, and `WakeAllConditionVariable`.



With \_WIN32\_WINNT=0x0500, MinGW-w64 therefore does not expose declarations for these functions and compilation fails.



This PR changes \_WIN32\_WINNT to 0x0600, which is consistent with the Windows API already used by OGRE.



This PR makes two small build-configuration changes:

1. Change the global MinGW target from `\\\_WIN32\\\_WINNT=0x0500` to `\\\_WIN32\\\_WINNT=0x0600` in the root `CMakeLists.txt`
2. Remove the older MinGW override `\\\_WIN32\\\_WINNT=0x0501`, specifically added for Intel Threading Building Blocks ("tbb") in `CMake/ConfigureBuild.cmake`.

No condition-variable source code is changed. The new Windows API level matches the minimum API level already required by the existing implementation.

# Root cause

`OgreCondVariableWin32.cpp` uses:

```text
InitializeConditionVariable
SleepConditionVariableCS
WakeConditionVariable
WakeAllConditionVariable
```

For MinGW-w64, these functions are exposed by the Windows headers only when:

```cpp
\_WIN32\_WINNT >= 0x0600
```

The current MinGW build instead defines:

```text
-D\_WIN32\_WINNT=0x0500
```

which causes the condition-variable type to remain visible while the four function declarations are hidden.

# Reproducing

The reproduction uses sibling repositories:

```text
workdir/
  ogre-next/
  ogre-next-deps/
  install/
```

## Toolchain

The baseline was reproduced with MSYS2 MINGW64 / GCC 15.2.0.

Select the toolchain and clear any inherited compiler overrides:

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

Clone the `ogre-next-deps` repository, then initialize all dependency submodules:

```bat

git clone https://github.com/OGRECave/ogre-next-deps/
git submodule update --init --recursive
```

The MinGW-w64 dependency build requires the separate portability fixes from the related `ogre-next-deps` work (pull requests https://github.com/OGRECave/ogre-next-deps/pull/18 to https://github.com/OGRECave/ogre-next-deps/pull/21 currently open on ogre-next-deps). For this reproduction, those fixes were applied as a combined patch:

```bat
git apply ogre-next-deps-mingw.patch
```

The patch is a prerequisite for constructing the MSYS2 MinGW-w64 test environment; it is not part of the `\\\_WIN32\\\_WINNT` fix in this PR.

Configure, build, and install the dependency tree with the same MSYS2 MINGW64 toolchain:

```bat
cmake -S . -B build ^
  -G "MinGW Makefiles" ^
  -DCMAKE\\\_BUILD\\\_TYPE=Release

cmake --build build -j 8
cmake --install build
```

The install step stages the dependency headers and libraries under:

```text
ogre-next-deps/build/ogredeps
```

## Configure OGRE-Next

From the sibling `ogre-next` repository (aka this repo):

```bat
cmake -S . -B build ^
  -G "MinGW Makefiles" ^
  -DCMAKE\\\_BUILD\\\_TYPE=Release ^
  -DOGRE\\\_DEPENDENCIES\\\_DIR=../ogre-next-deps/build/ogredeps ^
  -DCMAKE\\\_INSTALL\\\_PREFIX=../install
```

Verify that CMake selected the intended compiler:

```bat
findstr /I "CMAKE\\\_C\\\_COMPILER: CMAKE\\\_CXX\\\_COMPILER:" build\\\\CMakeCache.txt
```

The effective Windows target can be checked with:

```bat
findstr /I "\\\_WIN32\\\_WINNT" build\\\\OgreMain\\\\CMakeFiles\\\\OgreNextMain.dir\\\\flags.make
```

On the unmodified baseline this contains:

```text
-D\\\_WIN32\\\_WINNT=0x0500
```

# Observed behavior

## Baseline

Building the relevant prerequisite target:

```bat
cmake --build build --target OgreNextMain -j 8
```

fails in `OgreMain/src/Threading/OgreCondVariableWin32.cpp`:

```text
OgreCondVariableWin32.cpp:39:9: error: 'InitializeConditionVariable' was not declared in this scope
   39 |         InitializeConditionVariable( \\\&mCondVariable );
      |         ^\\\~\\\~\\\~\\\~\\\~\\\~\\\~\\\~\\\~\\\~\\\~\\\~\\\~\\\~\\\~\\\~\\\~\\\~\\\~\\\~\\\~\\\~\\\~\\\~\\\~\\\~

OgreCondVariableWin32.cpp:58:13: error: 'SleepConditionVariableCS' was not declared in this scope
   58 |             SleepConditionVariableCS( \\\&mCondVariable, \\\&mMutex, INFINITE );
      |             ^\\\~\\\~\\\~\\\~\\\~\\\~\\\~\\\~\\\~\\\~\\\~\\\~\\\~\\\~\\\~\\\~\\\~\\\~\\\~\\\~\\\~\\\~\\\~

OgreCondVariableWin32.cpp:62:44: error: 'WakeConditionVariable' was not declared in this scope
   62 |     void CondVariable::notifyOne( void ) { WakeConditionVariable( \\\&mCondVariable ); }
      |                                            ^\\\~\\\~\\\~\\\~\\\~\\\~\\\~\\\~\\\~\\\~\\\~\\\~\\\~\\\~\\\~\\\~\\\~\\\~\\\~\\\~

OgreCondVariableWin32.cpp:64:44: error: 'WakeAllConditionVariable' was not declared in this scope
   64 |     void CondVariable::notifyAll( void ) { WakeAllConditionVariable( \\\&mCondVariable ); }
      |                                            ^\\\~\\\~\\\~\\\~\\\~\\\~\\\~\\\~\\\~\\\~\\\~\\\~\\\~\\\~\\\~\\\~\\\~\\\~\\\~\\\~\\\~\\\~\\\~
```

The surrounding `NOMINMAX`, `MemoryBarrier`, signedness, and cast diagnostics are warnings and are unrelated to this change.

## With this PR

After changing the MinGW target to Vista and removing the lower TBB override, reconfiguration produces:

```text
CXX\\\_DEFINES = ... -D\\\_WIN32\\\_WINNT=0x0600
```

with no `\\\_WIN32\\\_WINNT=0x0500` or `\\\_WIN32\\\_WINNT=0x0501` override.

Validation command:

```bat
cmake --build build --target OgreNextMain -j 8
```

With the fix, `OgreCondVariableWin32.cpp` compiles succesfully and the build of `OgreNextMain` completes succesfullly.

# Compatibility

`0x0600` is the minimum Windows API level exposing the condition-variable functions already used by `OgreCondVariableWin32.cpp`. Raising `\\\_WIN32\\\_WINNT` therefore makes the declared MinGW target consistent with the runtime API already required by the existing implementation.

MSVC is unaffected because this definition is applied only to MinGW builds.

The removed TBB-specific `\\\_WIN32\\\_WINNT=0x0501` definition previously requested Windows XP. Vista is already above that historical minimum, so retaining the TBB override would only redefine the macro to a value too low for OGRE's condition-variable implementation.

This PR intentionally does not address unrelated MinGW warnings such as `NOMINMAX` or `MemoryBarrier` redefinitions.



