# Summary

This PR fixes Assimp's ccache launcher handling when the `ccache` executable is installed at a path containing spaces.

With the Code::Blocks 25.03 MinGW toolchain installed under `D:\Program Files\CodeBlocks\MinGW`, Assimp correctly finds:

```text
D:/Program Files/CodeBlocks/MinGW/bin/ccache.exe
```

but the generated MinGW Makefiles invoke that path without command-line quotes. `cmd.exe` therefore attempts to execute `D:/Program` and the first compilation fails.

The proposed change preserves literal quotes in `RULE_LAUNCH_COMPILE` and `RULE_LAUNCH_LINK`, so the generated command invokes the complete ccache path as one executable name.

# Context

This issue was found while building Assimp with a MinGW toolchain on Windows. The original downstream workaround skipped ccache whenever its path contained a space. That makes the build work, but unnecessarily disables ccache and is therefore not the proposed upstream fix.

The failure reproduces directly on current Assimp `master`; OGRE and the downstream wizard are not required for the reproducer.

Assimp enables ccache through:

```cmake
find_program(CCACHE_PATH ccache)
if(CCACHE_PATH)
  set_property(GLOBAL PROPERTY RULE_LAUNCH_COMPILE ${CCACHE_PATH})
  set_property(GLOBAL PROPERTY RULE_LAUNCH_LINK ${CCACHE_PATH})
endif()
```

CMake correctly finds the complete executable path. In the tested environment, `CMakeCache.txt` contains:

```text
CCACHE_PATH:FILEPATH=D:/Program Files/CodeBlocks/MinGW/bin/ccache.exe
```

The problem occurs when that value is emitted into the MinGW Makefile launcher command.

# Root cause

With the current code, the generated compile recipe contains an unquoted ccache executable path:

```text
D:/Program Files/CodeBlocks/MinGW/bin/ccache.exe "D:\Program Files\CodeBlocks\MinGW\bin\gcc.exe" ...
```

When this recipe is executed by `cmd.exe`, the space after `Program` terminates the executable name. Windows therefore attempts to execute:

```text
D:/Program
```

and reports:

```text
'D:/Program' is not recognized as an internal or external command,
operable program or batch file.
```

Simply quoting the CMake variable expansion as `"${CCACHE_PATH}"` was also tested. That keeps the value as one CMake argument, but does not preserve command-line quotes in the generated MinGW Makefile. The generated recipe is unchanged and fails in the same way.

The working form stores literal quote characters in the launcher property:

```cmake
"\"${CCACHE_PATH}\""
```

The generated compile command then contains:

```text
"D:/Program Files/CodeBlocks/MinGW/bin/ccache.exe" "D:\Program Files\CodeBlocks\MinGW\bin\gcc.exe" ...
```

With the launcher path quoted, `ccache.exe` is invoked as one executable name and compilation proceeds past the source file that fails with current `master`.

# Proposed change

In `CMakeLists.txt`, preserve command-line quotes around the ccache launcher path for both compilation and linking:

```diff
-    set_property(GLOBAL PROPERTY RULE_LAUNCH_COMPILE ${CCACHE_PATH})
-    set_property(GLOBAL PROPERTY RULE_LAUNCH_LINK ${CCACHE_PATH})
+    set_property(GLOBAL PROPERTY RULE_LAUNCH_COMPILE "\"${CCACHE_PATH}\"")
+    set_property(GLOBAL PROPERTY RULE_LAUNCH_LINK "\"${CCACHE_PATH}\"")
```

This keeps `ASSIMP_BUILD_USE_CCACHE` enabled and continues using the ccache executable found by `find_program`. It does not skip ccache merely because its path contains a space.

# Test environment

- Assimp: current `master` at test time
- Code::Blocks: 25.03, installed under `D:\Program Files\CodeBlocks`
- Compiler: GCC 14.2.0, target `x86_64-w64-mingw32`
- Generator: `MinGW Makefiles`
- Build type: `Release`
- Platform: Windows

The Code::Blocks MinGW toolchain was selected explicitly:

```bat
set "CB_MINGW=D:\Program Files\CodeBlocks\MinGW"
set "PATH=%CB_MINGW%\bin;%PATH%"
set "CC=%CB_MINGW%\bin\gcc.exe"
set "CXX=%CB_MINGW%\bin\g++.exe"

where gcc
where g++
where mingw32-make
where windres
where ccache

gcc --version
gcc -dumpmachine
ccache --version
```

`where ccache` resolves to:

```text
D:\Program Files\CodeBlocks\MinGW\bin\ccache.exe
```

Configuration command, run from the repository root:

```bat
cmake -S . -B build-ccache-baseline ^
  -G "MinGW Makefiles" ^
  -DCMAKE_BUILD_TYPE=Release ^
  -DCMAKE_MAKE_PROGRAM=mingw32-make.exe ^
  -DCMAKE_C_COMPILER="%CC%" ^
  -DCMAKE_CXX_COMPILER="%CXX%" ^
  -DCMAKE_RC_COMPILER=windres.exe ^
  -DASSIMP_BUILD_TESTS=OFF ^
  -DASSIMP_BUILD_ASSIMP_TOOLS=OFF
```

The generated cache confirms that CMake found the intended executable:

```text
CCACHE_PATH:FILEPATH=D:/Program Files/CodeBlocks/MinGW/bin/ccache.exe
```

For readable reproduction of the first launcher invocation, the targeted build was run serially and verbosely:

```bat
cmake --build build-ccache-baseline --target assimp --parallel 1 --verbose
```

# Behavior before the change

Current `master` generates an unquoted launcher command. The first zlib compilation starts with:

```text
D:/Program Files/CodeBlocks/MinGW/bin/ccache.exe "D:\Program Files\CodeBlocks\MinGW\bin\gcc.exe" ...
```

and fails immediately with:

```text
'D:/Program' is not recognized as an internal or external command,
operable program or batch file.
mingw32-make[3]: *** [contrib\zlib\CMakeFiles\zlibstatic.dir\build.make:79: contrib/zlib/CMakeFiles/zlibstatic.dir/adler32.c.obj] Error 1
```

# Quoting check

As a control, the following form was tested first:

```cmake
set_property(GLOBAL PROPERTY RULE_LAUNCH_COMPILE "${CCACHE_PATH}")
set_property(GLOBAL PROPERTY RULE_LAUNCH_LINK "${CCACHE_PATH}")
```

Using a fresh build directory, the generated launcher command remained unquoted:

```text
D:/Program Files/CodeBlocks/MinGW/bin/ccache.exe "D:\Program Files\CodeBlocks\MinGW\bin\gcc.exe" ...
```

and failed with the same `'D:/Program' is not recognized` error. Quoting the CMake argument alone is therefore not sufficient for this property.

# Behavior after the change

With literal launcher quoting:

```cmake
set_property(GLOBAL PROPERTY RULE_LAUNCH_COMPILE "\"${CCACHE_PATH}\"")
set_property(GLOBAL PROPERTY RULE_LAUNCH_LINK "\"${CCACHE_PATH}\"")
```

and a fresh build directory, the verbose build generates:

```text
"D:/Program Files/CodeBlocks/MinGW/bin/ccache.exe" "D:\Program Files\CodeBlocks\MinGW\bin\gcc.exe" ...
```

The previously failing `adler32.c` compilation succeeds, and the build proceeds to subsequent source files through the same quoted ccache launcher.

The complete `assimp` target is also built with the same configuration to exercise both compilation and linking through the updated launcher properties.

# Additional control

Before submission, the same change is also checked with a ccache path that does not contain spaces. That control must continue to invoke ccache and build successfully; this change must not regress the ordinary no-space case.
