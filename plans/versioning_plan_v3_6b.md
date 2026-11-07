# Code::Blocks v3.6b — FLTK on Windows

**Status: planning.** v3.6 is done. It makes the FLTK application wizard use a Homebrew FLTK prefix on macOS. Windows in v3.6 still asks for a folder the user already has and downloads nothing. This version is the Windows install path for that same wizard.

**Question this version answers:** what replaces `brew install fltk` on Windows?

FLTK 1.4.5 publishes source only, the same situation OGRE is in. v3.8 does not download an OGRE zip. It clones one official tag with `macgit`, then configures, builds, and installs with `maccmake`. v3.6b follows that sequence for FLTK. The release gzip is not fetched, and `WinSdkStage` is not used.

The clone is tag `release-1.4.5` of `https://github.com/fltk/fltk.git`. The build is a static library made with the selected GNU GCC. The generated project links the installed `libfltk.a` and does not point at the source or build tree.

Upstream baseline remains Code::Blocks trunk **r14050** / IDE **25.03**.

Evidence labels: **CONFIRMED FROM SOURCE** (this tree, 7 November 2026), **CONFIRMED FROM WEB** (GitHub release `release-1.4.5`, published 25 April 2026), **PROPOSED DESIGN**. The clone, the MinGW configure, and the Windows libraries the static library needs are not yet confirmed on a machine.

---

## 1. Windows files, not a branch inside the macOS helpers

**PROPOSED DESIGN.** The Windows workflow lives in a new pair of files. `macgit` and `maccmake` stay macOS-only.

| macOS, v3.8, proven | Windows, v3.6b |
|---|---|
| `src/plugins/scriptedwizard/macgit.h` | `src/plugins/scriptedwizard/wingit.h` |
| `src/plugins/scriptedwizard/macgit.cpp` | `src/plugins/scriptedwizard/wingit.cpp` |
| `src/plugins/scriptedwizard/maccmake.h` | `src/plugins/scriptedwizard/wincmake.h` |
| `src/plugins/scriptedwizard/maccmake.cpp` | `src/plugins/scriptedwizard/wincmake.cpp` |

`macgit.cpp` and `maccmake.cpp` are each one translation unit inside `#if defined(__WXMAC__)`. They are not a shared core with a small platform leaf. A Windows branch in those files would be a second implementation living in a file the OGRE wizard still owns. v3.8 is still in progress, and its acceptance says existing Windows behaviour is left alone by not editing those helpers for another platform.

The CMake helper is the part that cannot be shared by an `#ifdef`. `ConfigureMacCMake` already builds a fixed argv. That argv contains `-DCMAKE_INSTALL_RPATH=<install>/lib/macosx/Release` and `-DOGRE_PLUGINS_PATH=lib/macosx/Release`. Those arguments belong to the OGRE macOS build. The FLTK Windows configure is a different fixed argv: the MinGW Makefiles generator, the selected `gcc.exe` and `g++.exe`, `mingw32-make.exe`, a static library, and the FLTK test, FLUID, and example programs left off. Putting both argv lists in `maccmake.cpp` would either change the proven OGRE line or add a library switch the helper was not given.

`wingit` copies the `macgit` contract and nothing wider:

```text
Wizard.WinGitProbe()
Wizard.WinGitTry(path)
Wizard.WinGitClone(url, tag, destination, explanation, confirmLabel)
```

The clone is the equivalent of `git clone --branch <tag> --depth 1 <url> <destination>`. The helper revalidates `git.exe` with `git --version`, never installs Git, requires consent, constructs argv itself, and refuses an unsafe destination. The report uses the same field names as `FormatMacGitProbe` and `FormatMacGitClone` (`access`, `executable`, `version`, `success`, `cancelled`, `output`).

`wincmake` copies the `maccmake` contract:

```text
Wizard.WinCMakeProbe()
Wizard.WinCMakeTry(path)
Wizard.WinCMakePaths(sourceLeaf, installLeaf)
Wizard.WinCMakeConfigure(source, build, install, buildType, explanation, confirmLabel)
Wizard.WinCMakeBuild(build, explanation, confirmLabel)
Wizard.WinCMakeInstall(build, install, explanation, confirmLabel)
```

The script passes leaves and directories. It does not pass `-D` flags, a generator name, or a shell command. The MinGW argv is written in `wincmake.cpp`, the same way the OGRE rpath and plug-in path are written in `maccmake.cpp`.

This version does not extract the dialog and process code into a file shared with `macgit` or `maccmake`. That extraction would edit the proven macOS helpers. The duplication is the cost of leaving v3.8 untouched. A later cleanup can share it after both workflows are accepted.

Bindings go in `wiz.cpp` under `__WXMSW__`. `Makefile.am` lists the four new files. `macgit.cpp`, `maccmake.cpp`, `macbrew.cpp`, and `winsdk.cpp` are not edited for FLTK names.

---

## 2. What each helper is allowed to know

`wingit` knows Git. It does not know FLTK, CMake, or the compiler. The saved hint is `/windows/git/executable`. Search candidates are absolute paths only, as on macOS:

```text
%ProgramFiles%\Git\cmd\git.exe
%ProgramFiles%\Git\bin\git.exe
```

A missing Git opens the same kind of file choice the OGRE wizard uses. The chosen file is remembered only after `git --version` succeeds. Code::Blocks does not install Git.

`wincmake` knows this one FLTK Windows build. It does not accept a general CMake command. The saved hint is `/windows/cmake/executable`. The absolute candidate is:

```text
%ProgramFiles%\CMake\bin\cmake.exe
```

A missing CMake opens a file choice. The chosen file is remembered only after `cmake --version` succeeds. Code::Blocks does not download CMake.

The compiler is the one `WinSdkCompiler` already probes from `winsdk.cpp`. `wincmake` calls that probe. It does not grow a second compiler search, and it does not change `winsdk.cpp`. The automatic path requires `access=works` and machine `x86_64-w64-mingw32`, with `mingw32-make.exe` in the same `bin` as that `gcc.exe`. Any other compiler keeps the v3.6 folder page and does not clone.

Path leaves are fixed strings, resolved in C++ from `%LOCALAPPDATA%`, the same variable `winsdk.cpp` already reads. The root is the parent of the zip warehouse, so a source build is not stored as a downloaded edition:

```text
%LOCALAPPDATA%\codeblocks\
    fltk-source-1.4.5\
        build-codeblocks\
    fltk-1.4.5\
```

`WinCMakePaths("fltk-source-1.4.5", "fltk-1.4.5")` returns that root, the source directory, `source\build-codeblocks`, and the install directory. Source, build, and install are absolute and distinct. A drive root is refused. The generated project is allowed to remember the install directory only.

---

## 3. The tag and the build

**CONFIRMED FROM WEB.** The repository is `https://github.com/fltk/fltk.git`. The tag is `release-1.4.5`, the GitHub release published 25 April 2026. There is no MinGW zip and no MSVC zip on that release. The official source gzip is the same tree, and this version does not download it.

**PROPOSED DESIGN.** The operation the user approves is:

```text
git clone --branch release-1.4.5 --depth 1
    https://github.com/fltk/fltk.git
    %LOCALAPPDATA%\codeblocks\fltk-source-1.4.5
```

A checkout is ready when `.git\HEAD`, `CMakeLists.txt`, and `FL\Fl.H` exist. The helper does not grow a general Git status command. An existing ready checkout is reused and is not cloned again.

Configure is a separate consent, and it fixes the prefix before anything is built. The argv in `wincmake.cpp` is the equivalent of:

```text
cmake -S <source> -B <build> -G "MinGW Makefiles"
    -DCMAKE_BUILD_TYPE=Release
    -DCMAKE_INSTALL_PREFIX=<install>
    -DCMAKE_C_COMPILER=<gcc.exe>
    -DCMAKE_CXX_COMPILER=<g++.exe>
    -DCMAKE_MAKE_PROGRAM=<mingw32-make.exe>
    -DBUILD_SHARED_LIBS=OFF
    -DFLTK_BUILD_TEST=OFF
    -DFLTK_BUILD_FLUID=OFF
    -DFLTK_BUILD_EXAMPLES=OFF
```

The generator and the compiler paths are set so a machine that also has Visual Studio still builds with the selected GCC. The CMake option names are read from the cloned `CMakeLists.txt` before this list is treated as frozen. Install runs `cmake --install <build>` and does not pass a replacement `--prefix`.

Success is the installed contract, not the process exit alone:

```text
<install>\include\FL\Fl.H      reports FLTK 1.4.5
<install>\lib\libfltk.a
```

A failed clone, configure, build, version check, or install leaves any previous install in place and does not publish a new one. Cancel stops that step only.

The automatic project links `libfltk.a` from the install prefix, plus the Windows libraries that this static build needs. Those names are read from the build during implementation. They are not copied from the macOS `fltk-config` output. The manual folder page keeps today's link line, which is only `-lfltk`.

---

## 4. Wizard behaviour

**PROPOSED DESIGN.** The script change is `src/plugins/scriptedwizard/resources/fltk/wizard.script`. On macOS it still calls `Wizard.MacBrewFormula("fltk")` and `Wizard.MacBrewInstall`. On Windows, `BeginWizard` probes the selected compiler.

- **The compiler is `x86_64-w64-mingw32` and its `bin` contains `mingw32-make.exe`.** The pages follow the OGRE order. Git is probed, then the user may pick `git.exe`. CMake is probed, then the user may pick `cmake.exe`. Paths are resolved. A missing checkout asks for the shallow clone. A missing `CMakeCache.txt` asks to configure. A missing built library asks to build. A missing install contract asks to install. Each of those four actions has its own explanation and confirm label. A later wizard run skips a step whose marker is already present. After the install validates, there is no folder page.
- **Any other compiler, or no `mingw32-make.exe` beside the selected `gcc.exe`.** No clone and no CMake. The v3.6 folder page remains.

`SetupProject` on the automatic path writes the install include and library directories, `-lfltk`, and the Windows libraries from section 3. It does not write `$(#fltk)`, and it does not point at `fltk-source-1.4.5` or `build-codeblocks`. Debug stays an executable target with debug symbols. Release stays an ordinary optimised executable. The starter source is unchanged.

Leaving the compiler page forward re-checks the probe. A compiler that is not `x86_64-w64-mingw32` does not receive the automatic project.

---

## 5. Milestones

| | Objective | Check | Status |
|---|---|---|---|
| M1 | Add `wingit` without touching `macgit`. | On Windows, probe and try report `git --version`. Clone of `release-1.4.5` lands in the resolved source directory. Cancel starts no process. `macgit.cpp` is unchanged. | |
| M2 | Add `wincmake` without touching `maccmake`. | Paths resolve under `%LOCALAPPDATA%\codeblocks`. Configure writes `CMakeCache.txt` with the install prefix and the selected GCC. `maccmake.cpp` is unchanged. | |
| M3 | Build and install the static library. | `cmake --install` leaves `include\FL\Fl.H` reporting 1.4.5 and `lib\libfltk.a`. A failed step does not replace a previous install. | |
| M4 | Generate the project from the install prefix. | The `.cbp` links that prefix. It has no `$(#fltk)` and no source or build directory. The macOS Homebrew path is unchanged. | |
| M5 | Run the wizard on the WinLibs compiler. | A fresh project builds without hand repair and opens the FLTK window. A second run skips clone, configure, build, and install while the markers still validate. | |

M5 is the wizard. A hand clone does not close M5.

---

## 6. Acceptance

v3.6b is accepted when all of the following hold:

1. On macOS the wizard still installs or accepts Homebrew FLTK, and `macgit.cpp` and `maccmake.cpp` are unchanged.
2. On Windows, with the WinLibs GCC 14.2.0 `x86_64-w64-mingw32` compiler selected, a missing FLTK tree clones nothing until the user confirms the shallow clone, and builds nothing until the user confirms configure, build, and install.
3. The clone is `https://github.com/fltk/fltk.git` at tag `release-1.4.5`. Git and CMake are found or chosen. Neither is installed by Code::Blocks.
4. The install prefix is `%LOCALAPPDATA%\codeblocks\fltk-1.4.5`, the prefix was fixed at configure time, and the header still says 1.4.5 with `lib\libfltk.a` present.
5. The generated project links that static library, builds with that GCC, and opens the starter window. It does not use the source or build directory.
6. A compiler that is not `x86_64-w64-mingw32`, or a compiler without `mingw32-make.exe`, is not offered the clone. The v3.6 folder page still works.
7. No project is created when the user cancels, the clone fails, the build fails, or the installed tree fails the 1.4.5 check.
8. The script cannot pass a Git command, a CMake argument, or a generator. Those strings live in `wingit.cpp` and `wincmake.cpp`.

---

## 7. What stays rejected

- A Windows branch inside `macgit.cpp` or `maccmake.cpp`.
- A shared process file that requires editing those macOS helpers in this version.
- A general Git or CMake command API for wizard scripts.
- Fetching the release gzip, the tag zipball, or any archive through `WinSdkStage`.
- Winget, Chocolatey, Scoop, MSYS2, or vcpkg.
- Installing Git or CMake, or building with the first `gcc` on `PATH`.
- An MSVC generator, including on a machine where Visual Studio is installed.
- Installing into `Program Files` or beside `codeblocks.exe`.
- Changing the macOS `fltk-config` lines, the starter source, or the manual folder page's link line.

---

## 8. Done when

A Windows user with the selected 64-bit WinLibs GCC, an existing Git, and an existing CMake can choose the FLTK application from New Project, confirm the shallow clone and the static build once, and build and run the starter. The next run does not ask again while the checkout and the installed tree still validate.

That is the Homebrew step, replaced the way v3.8 replaces it for OGRE: one pinned tag, tools that are found and never installed, and a static library built with the compiler the wizard selected. FLTK stays out of Code::Blocks. The folder page remains for a compiler this build is not offered to.

## References

- [v3.8 OGRE wizard](versioning_plan_v3_8.md)
- [v3.6 FLTK wizard](versioning_plan_v3_6.md)
- [FLTK 1.4.5 release](https://github.com/fltk/fltk/releases/tag/release-1.4.5)
- [v3.3 Windows SDK helper](versioning_plan_v3_3.md)
