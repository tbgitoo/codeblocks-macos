# Code::Blocks for Windows

## Official install

For the general official Windows install, use the official Code::Blocks distribution:

[Official Code::Blocks website](https://www.codeblocks.org)

## Pre-packaged Code::Blocks from this source tree

From v4.0, this repository also publishes a Windows installer in the **Releases** section (https://github.com/tbgitoo/codeblocks-macos/releases). It may be useful when you want the debugger for a source file that is not part of a Code::Blocks project, or the wizard updates in this tree. The installer contains the IDE. A selected compiler option downloads a pinned MinGW toolchain during setup; clearing that option installs the IDE only.

## Build from this source tree

This section explains how to build the 64-bit IDE from this source tree. The build produces a directory you can run directly.

### Build summary

The project is `src\CodeBlocks_wx33_64.cbp`. It builds against wxWidgets 3.3 as one shared Unicode library, using MinGW-w64. It needs a pre-existing Code::Blocks installation different from this source tree to be built.

### Detailed build instructions

#### Environment variables

Open **Command Prompt** (`cmd.exe`). First, define paths that are needed. This depends on your machine. gitignore foresees a local_paths.txt where you can store these for regular use (not preconfigured)

```bat
set "REPO=<actual path>\codeblocks-macos"
set "CODEBLOCKS=D:\Program Files\CodeBlocks"
set "MINGW=%CODEBLOCKS%\MinGW"
set "WXWIDGETS=<path to source>\wxWidgets-3.3.4"
set "ZIPDIR=<path to folder with zip.exe>"
set "PATH=%MINGW%\bin;%ZIPDIR%;%PATH%"
```

wxWidgets is available from:
https://github.com/wxWidgets/wxWidgets/releases/download/v3.3.4/wxWidgets-3.3.4.zip

unzip it and point the WXWIDGETS variable to the folder.

Important: A particularity of the Windows Code::Blocks setup is that to compile Code::Blocks, a pre-existing Code::Blocks binary is absolutely required. This development tree cannot compile itself, and `%CODEBLOCKS%` must point to a pre-existing codeblocks installation different from this source tree. The existing official Code::Blocks release at https://www.codeblocks.org/downloads/binaries/ is ideal. Point `%CODEBLOCKS%` at your installation of Code::Blocks. 

Also note that zip.exe is needed and is found via ZIPDIR. This zip.exe can be any Info-ZIP zip.exe of version 2.3 or newer understands those. zip -h should say Info-ZIP. For example, Free Pascal FPC has such a zip.exe.

Once those 4 environment variables + PATH are correctly defined, you can copy-paste the rest of the commands unchanged. 

`gcc -dumpmachine` should print `x86_64-w64-mingw32`. 
`zip -h` should name Info-ZIP. Zip 2.3 or newer is enough for the flags this tree uses.

The following need to exist and refer to MINGW tools / Info-Zip.

```bat
where gcc
where g++
where mingw32-make
where windres
where ar
where strip
where zip
gcc -dumpmachine
```

#### Build wxWidgets

Skip this step when `%WXWIDGETS%\lib\gcc_dll\libwxmsw33u.a` ,
`%WXWIDGETS%\lib\gcc_dll\wxmsw334u_gcc_custom.dll`
and `%WXWIDGETS%\lib\gcc_dll\mswu\wx\setup.h` 
are already present.

To check:
```bat
dir "%WXWIDGETS%\lib\gcc_dll\libwxmsw33u.a"
dir "%WXWIDGETS%\lib\gcc_dll\wxmsw334u_gcc_custom.dll"
dir "%WXWIDGETS%\lib\gcc_dll\mswu\wx\setup.h"
```

If needed you can build wxWidgets as follows:

```bat
cd /d "%WXWIDGETS%\build\msw"
mingw32-make -f makefile.gcc SHARED=1 MONOLITHIC=1 BUILD=release -j%NUMBER_OF_PROCESSORS%
```
Notes regarding this build:
`makefile.gcc` defaults to a static debug build. `SHARED=1`, `MONOLITHIC=1`, and `BUILD=release` are required. Leave `CFG` unset and leave `VENDOR` at its default, `custom`. By default, the library lands in `lib\gcc_dll` within the wxWidget folder; this the expected folder location for the Code::Blocks project, so there should be no need to define a specific target folder here.

As noted above, a succesful wxWidgets 3.3.4 build must contain:

```text
%WXWIDGETS%\lib\gcc_dll\libwxmsw33u.a
%WXWIDGETS%\lib\gcc_dll\wxmsw334u_gcc_custom.dll
%WXWIDGETS%\lib\gcc_dll\mswu\wx\setup.h
```

The `334` in the DLL name is the wxWidgets release. Note that for the Code::Blocks project, as of present, the import library is `libwxmsw33u.a` and 3.3.X versions, but not higher or lower will link. To be concrete, a wxWidgets 3.2 library will not link.

Regarding the include headers, the Code::Blocks project expects the headers under  `lib\gcc_dll\mswu`, which is where they should be produced using the above release configuration for wxWidgets.

#### Configure the bootstrap IDE



Start `%CODEBLOCKS%\codeblocks.exe`.

**Compiler.** Open **Settings → Compiler… → GNU GCC Compiler → Toolchain executables**. Set **Compiler's installation directory** to the `MINGW` directory, the one that contains `bin`:

```text
D:\Program Files\CodeBlocks\MinGW
```

**wxWidgets.** Open **Settings → Global variables…**. Create a variable named `wx33_64` and set:

| Field | Value |
|---|---|
| base | `D:\Users\thoma\Documents\wxWidgets-3.3.4` |
| include | `D:\Users\thoma\Documents\wxWidgets-3.3.4\include` |
| lib | `D:\Users\thoma\Documents\wxWidgets-3.3.4\lib` |

The project reads `$(#wx33_64)`, `$(#wx33_64.include)`, and `$(#wx33_64.lib)`. Use the directory you assigned to `WXWIDGETS` if you changed the opening block.

Two other names appear in the project as compiler options. Defining them is optional. When they are absent, Code::Blocks omits them from the command line and writes a message to the build log. The build continues.

| Name | base | What it does |
|---|---|---|
| `cb_cpp_std` | `-std=gnu++17` | Selects the C++ language mode. GCC 14 already defaults to this. |
| `cb_release_type` | empty, `-g`, or `-O2` | Adds an extra compiler flag. The project already defines `cbDEBUG`. An empty value is a normal build. |

#### Build Code::Blocks

Close any `codeblocks.exe` started from `%REPO%\src` before building. The command below starts its own instance.

```bat
cd /d "%REPO%\src"
start /wait "" "%CODEBLOCKS%\codeblocks.exe" --no-splash-screen --no-dde --multiple-instance --build --target=All CodeBlocks_wx33_64.cbp
echo %ERRORLEVEL%
```

`start /wait` is required. `codeblocks.exe` is a GUI program, so Command Prompt otherwise continues before the build finishes. A build window opens and closes when the build finishes. `%ERRORLEVEL%` is 0 after a successful build.

If you wish, you can run the same build from the IDE: open `%REPO%\src\CodeBlocks_wx33_64.cbp`, select the **All** virtual target, and choose **Build → Build**.

**All** builds the IDE, its libraries, and the core plugins. Object files go to `%REPO%\src\.objs33_64`. Executables and plugins go to `%REPO%\src\devel33_64`.

`%REPO%\src\CodeBlocks_wx33_64.workspace` also builds extra tools and contributed plugins. Some of those plugins need libraries that are not part of these steps. They are not required to produce the IDE.

`src\batch_build_core_33_64.bat` is another way to start the same project. It keeps the build window open. Its own defaults are `C:\Devel\CodeBlocks` and a `MinGW64` folder, so set the two variables first:

```bat
cd /d "%REPO%\src"
set "CB_ROOT=%CODEBLOCKS%"
set "GCC_ROOT=%MINGW%"
batch_build_core_33_64.bat
```

#### Copy the runtime DLLs

`%REPO%\src\devel33_64\codeblocks.exe` needs these DLLs beside it.

```bat
copy /Y "%WXWIDGETS%\lib\gcc_dll\wxmsw334u_gcc_custom.dll" "%REPO%\src\devel33_64\"
copy /Y "%MINGW%\bin\libgcc_s_seh-1.dll" "%REPO%\src\devel33_64\"
copy /Y "%MINGW%\bin\libstdc++-6.dll" "%REPO%\src\devel33_64\"
copy /Y "%MINGW%\bin\libwinpthread-1.dll" "%REPO%\src\devel33_64\"
```

The wxWidgets DLL name is the one produced for 3.3.4. Use the DLL name from the wxWidgets build if you built a different 3.3.x release.

### Assemble a runnable tree

```bat
cd /d "%REPO%\src"
update.bat 33_64
```

`update.bat` fills in resources under `%REPO%\src\devel33_64` and writes a stripped copy to `%REPO%\src\output33_64`. `strip` and `zip` must be on `PATH`, which the opening block arranges.

Run:

```bat
"%REPO%\src\output33_64\codeblocks.exe"
```

For debugging, there is also a development copy at `%REPO%\src\devel33_64\codeblocks.exe`, which keeps the debug symbols. `output33_64` is the smaller tree. Both use the `%APPDATA%\CodeBlocks` profile already used by other copies of Code::Blocks on the same user account.

# Build the NSIS installer

## Installer build summary

This set of instructions builds a windows installer for distribution.

It uses makensis.exe (from https://nsis.sourceforge.io/Download) to pack the executable and ressources into a setup.exe for automated installation.

## Preconditions

These steps package `%REPO%\src\output33_64` into a Windows setup.exe program. Run these steps only after `update.bat` has finished (see above), and after the four runtime DLLs have been copied into `devel33_64`. `update.bat` copies those DLLs into `output33_64`. The installer does not contain a compiler. Its optional page downloads one while the setup program is running.

Install stock NSIS 3 from https://nsis.sourceforge.io/Download. The default install puts `makensis.exe` in `C:\Program Files (x86)\NSIS\Bin`. The build script looks there, and in `C:\Program Files\NSIS\Bin`, and on `PATH`. This repository does not download NSIS.

`objdump` must still be on `PATH`. The opening block setting the environment variables (above) already adds `%MINGW%\bin`, which is where `objdump` lives.

```bat
cd /d "%REPO%\packaging\windows"
build-installer.bat
```

The script copies `output33_64` to `packaging\windows\stage`, leaves import libraries and any `gcc.exe` or `gdb.exe` out, checks that the staged program can find its DLLs, then runs `makensis` on `codeblocks.nsi`. The setup program is written beside the script:

```bat
"%REPO%\packaging\windows\codeblocks-v3.4-win64.exe"
```

Run that file to install. The default directory is `C:\Program Files\CodeBlocks`. Leave the compiler checkbox selected to download the pinned MinGW during setup, or clear it to install the IDE only.
