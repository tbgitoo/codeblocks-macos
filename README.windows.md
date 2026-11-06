# Building Code::Blocks on Windows

This document explains how to build the 64-bit IDE from this source tree. There is no Windows installer here. The build produces a directory you can run directly.

The project is `src\CodeBlocks_wx33_64.cbp`. It builds against wxWidgets 3.3 as one shared Unicode library, using MinGW-w64.

The paths below use these names. Substitute your own directories.

| Name | Meaning |
|---|---|
| `<repo>` | This source tree |
| `<CodeBlocks>` | An installed Code::Blocks, used only to drive the build |
| `<MinGW>` | The MinGW root: the directory that contains `bin\g++.exe` |
| `<wxWidgets>` | The wxWidgets 3.3.x source tree, after the build in step 2 |

A normal Code::Blocks installation directory is `C:\Program Files\CodeBlocks`. Another drive or directory is fine. The bootstrap IDE and the MinGW tree do not have to share a parent directory. The MinGW used to check these steps was WinLibs GCC 14.2.0 with POSIX threads, SEH, and UCRT.

## What you need

- 64-bit Windows 10 or later
- A 64-bit MinGW-w64 GCC with POSIX threads. It must provide `gcc`, `g++`, `mingw32-make`, `windres`, `ar`, and `strip`. `gcc -dumpmachine` should print `x86_64-w64-mingw32`. GCC 14.2.0 is known to build this tree.
- Info-ZIP `zip.exe` on `PATH`. The project post-build steps and `src\update.bat` run `zip` by name. `zip -h` should name Info-ZIP. Zip 2.3 or newer is enough for the flags this tree uses.
- wxWidgets 3.3.x source. The project links `libwxmsw33u.a`, so a wxWidgets 3.2 library will not link. These steps were checked with wxWidgets 3.3.4.
- A Code::Blocks binary, used only as the build driver. The IDE you are compiling cannot build itself the first time. The binary does not have to match this tree's revision.

## 1. Put the compiler on PATH

Open a command prompt and put MinGW's `bin` directory first on `PATH`:

```bat
set PATH=<MinGW>\bin;%PATH%
```

`where gcc`, `where g++`, `where mingw32-make`, `where windres`, and `where strip` should all resolve under `<MinGW>\bin`. `where zip` should resolve to Info-ZIP.

## 2. Build wxWidgets

Skip this step when `<wxWidgets>\lib\gcc_dll\libwxmsw33u.a` and `<wxWidgets>\lib\gcc_dll\mswu\wx\setup.h` are already present from the command below.

From a prompt prepared as in step 1:

```bat
cd /d <wxWidgets>\build\msw
mingw32-make -f makefile.gcc SHARED=1 MONOLITHIC=1 BUILD=release -j%NUMBER_OF_PROCESSORS%
```

`makefile.gcc` defaults to a static debug build. The three settings above are required. Leave `CFG` unset and leave `VENDOR` at its default, `custom`. A non-empty `CFG` changes the directory name, and the project looks in `lib\gcc_dll`.

A wxWidgets 3.3.4 build then contains:

```text
<wxWidgets>\lib\gcc_dll\libwxmsw33u.a
<wxWidgets>\lib\gcc_dll\wxmsw334u_gcc_custom.dll
<wxWidgets>\lib\gcc_dll\mswu\wx\setup.h
```

The `334` in the DLL name is the wxWidgets release. A later 3.3.x release changes that number. The import library stays `libwxmsw33u.a`.

`BUILD=release` writes headers under `lib\gcc_dll\mswu`. A debug wxWidgets build writes `lib\gcc_dll\mswud` instead, which this project does not use.

## 3. Configure the bootstrap IDE

Start `<CodeBlocks>\codeblocks.exe`.

**Compiler.** Open **Settings → Compiler… → GNU GCC Compiler → Toolchain executables**. Set **Compiler's installation directory** to `<MinGW>`, the directory that contains `bin`.

**wxWidgets.** Open **Settings → Global variables…**. Create a variable named `wx33_64` and set:

| Field | Value |
|---|---|
| base | `<wxWidgets>` |
| include | `<wxWidgets>\include` |
| lib | `<wxWidgets>\lib` |

The project reads `$(#wx33_64)`, `$(#wx33_64.include)`, and `$(#wx33_64.lib)`.

Two other names appear in the project as compiler options. Defining them is optional. When they are absent, Code::Blocks omits them from the command line and writes a message to the build log. The build continues.

| Name | base | What it does |
|---|---|---|
| `cb_cpp_std` | `-std=gnu++17` | Selects the C++ language mode. GCC 14 already defaults to this. |
| `cb_release_type` | empty, `-g`, or `-O2` | Adds an extra compiler flag. The project already defines `cbDEBUG`. An empty value is a normal build. |

## 4. Build the IDE

Close any `codeblocks.exe` from this source tree before building. If another Code::Blocks is already running, the command below still starts its own instance.

From a Command Prompt prepared as in step 1, with `zip.exe` still on `PATH`:

```bat
cd /d <repo>\src
start /wait "" "<CodeBlocks>\codeblocks.exe" --no-splash-screen --no-dde --multiple-instance --build --target=All CodeBlocks_wx33_64.cbp
```

`start /wait` is required. `codeblocks.exe` is a GUI program, so a Command Prompt otherwise continues before the build finishes and does not report its status. A build window opens and closes when the build finishes. `%ERRORLEVEL%` is then 0 after a successful build.

The same build from the IDE: open `src\CodeBlocks_wx33_64.cbp`, select the **All** virtual target, and choose **Build → Build**.

**All** builds the IDE, its libraries, and the core plugins. Object files go to `src\.objs33_64`. Executables and plugins go to `src\devel33_64`.

`src\CodeBlocks_wx33_64.workspace` also builds extra tools and contributed plugins. Some of those plugins need libraries that are not part of these steps. They are not required to produce the IDE.

`src\batch_build_core_33_64.bat` runs this project with different default directories (`C:\Devel\CodeBlocks` and a `MinGW64` folder). Set `CB_ROOT` to `<CodeBlocks>` and `GCC_ROOT` to `<MinGW>` before running it. That script keeps the build window open.

## 5. Copy the runtime DLLs

`src\devel33_64\codeblocks.exe` needs these DLLs beside it:

From `<wxWidgets>\lib\gcc_dll`:

- `wxmsw334u_gcc_custom.dll` (use the DLL name produced in step 2)

From `<MinGW>\bin`:

- `libgcc_s_seh-1.dll`
- `libstdc++-6.dll`
- `libwinpthread-1.dll`

```bat
copy /Y "<wxWidgets>\lib\gcc_dll\wxmsw334u_gcc_custom.dll" "<repo>\src\devel33_64\"
copy /Y "<MinGW>\bin\libgcc_s_seh-1.dll" "<repo>\src\devel33_64\"
copy /Y "<MinGW>\bin\libstdc++-6.dll" "<repo>\src\devel33_64\"
copy /Y "<MinGW>\bin\libwinpthread-1.dll" "<repo>\src\devel33_64\"
```

## 6. Assemble a runnable tree

`strip` must be on `PATH`. From `src`:

```bat
cd /d <repo>\src
update.bat 33_64
```

`update.bat` fills in resources under `src\devel33_64` and writes a stripped copy to `src\output33_64`.

Run either copy:

```text
<repo>\src\devel33_64\codeblocks.exe
<repo>\src\output33_64\codeblocks.exe
```

`devel33_64` keeps symbols. `output33_64` is the smaller tree. Both use the `%APPDATA%\CodeBlocks` profile already used by other copies of Code::Blocks on this account.
