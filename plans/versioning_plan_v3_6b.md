# Code::Blocks v3.6b — FLTK on Windows

**Status: in progress.** v3.6 is done. It makes the FLTK application wizard use a Homebrew FLTK prefix on macOS. Windows in v3.6 still asks for a folder the user already has and downloads nothing. This version is the Windows install path for that same wizard. The helpers and the wizard script are in the tree. On one Windows machine the shallow clone, the MinGW configure, the static build, and the install contract have succeeded, and a later wizard run skips those four steps. The generated project's Build and Run is still open.

**Question this version answers:** what replaces `brew install fltk` on Windows?

FLTK 1.4.5 publishes source only, the same situation OGRE is in. v3.8 does not download an OGRE zip. It clones one official tag with `macgit`, then configures, builds, and installs with `maccmake`. v3.6b follows that sequence for FLTK. The release gzip is not fetched, and `WinSdkStage` is not used.

The clone is tag `release-1.4.5` of `https://github.com/fltk/fltk.git`. The build is a static library made with the selected GNU GCC. The generated project links the installed `libfltk.a` and does not point at the source or build tree.

Upstream baseline remains Code::Blocks trunk **r14050** / IDE **25.03**.

Evidence labels: **CONFIRMED FROM SOURCE** (this tree, 7 November 2026), **CONFIRMED FROM WEB** (GitHub release `release-1.4.5`, published 25 April 2026), **CONFIRMED ON WINDOWS** (one machine, 7 November 2026), **PROPOSED DESIGN**. The clone, the MinGW configure, the installed header, and the libraries named by this build's `fltk-config` are confirmed. The wizard-generated project has not yet been built from the IDE.

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
    -DFLTK_BUILD_SHARED_LIBS=OFF
    -DFLTK_BUILD_TEST=OFF
    -DFLTK_BUILD_FLUID=OFF
    -DFLTK_BUILD_EXAMPLES=OFF
    -DFLTK_BUILD_FLTK_OPTIONS=OFF
    -DFLTK_BUILD_FORMS=OFF
    -DFLTK_BUILD_GL=OFF
```

**CONFIRMED FROM SOURCE.** Those option names are the ones in FLTK 1.4.5 `CMake/options.cmake`. The shared-library switch is `FLTK_BUILD_SHARED_LIBS`. The generator and the compiler paths are set so a machine that also has Visual Studio still builds with the selected GCC. Install runs `cmake --install <build>` and does not pass a replacement `--prefix`.

Success is the installed contract, not the process exit alone:

```text
<install>\include\FL\Fl.H      reports FLTK 1.4.5
<install>\lib\libfltk.a
```

A failed clone, configure, build, version check, or install leaves any previous install in place and does not publish a new one. Cancel stops that step only.

**CONFIRMED ON WINDOWS.** The `fltk-config` written by this static build reports `LDFLAGS` as `-mwindows` and `LDLIBS` as `-lgdiplus -lole32 -luuid -lcomctl32 -lws2_32 -lwinspool`. The automatic project adds `-mwindows`, then `-lfltk`, then those libraries, in that order. The same install also writes `libfltk_images.a`, `libfltk_jpeg.a`, `libfltk_png.a`, and `libfltk_z.a`. The starter links only `libfltk.a`. The manual folder page keeps today's link line, which is only `-lfltk`. A hand link of the starter with this line produced an executable. The wizard-generated project is still the M5 check.

---

## 4. Wizard behaviour

**PROPOSED DESIGN.** The script change is `src/plugins/scriptedwizard/resources/fltk/wizard.script`. On macOS it still calls `Wizard.MacBrewFormula("fltk")` and `Wizard.MacBrewInstall`. On Windows, `BeginWizard` probes the selected compiler.

- **The compiler is `x86_64-w64-mingw32` and its `bin` contains `mingw32-make.exe`.** The pages follow the OGRE order. Git is probed, then the user may pick `git.exe`. CMake is probed, then the user may pick `cmake.exe`. Paths are resolved. A missing checkout asks for the shallow clone. A missing `CMakeCache.txt` asks to configure. A missing built library asks to build. A missing install contract asks to install. Each of those four actions has its own explanation and confirm label. A later wizard run skips a step whose marker is already present. After the install validates, there is no folder page.
- **Any other compiler, or no `mingw32-make.exe` beside the selected `gcc.exe`.** No clone and no CMake. The v3.6 folder page remains.

`SetupProject` on the automatic path writes the install include and library directories, `-lfltk`, and the Windows libraries from section 3. It does not write `$(#fltk)`, and it does not point at `fltk-source-1.4.5` or `build-codeblocks`. Debug stays an executable target with debug symbols. Release stays an ordinary optimised executable. The starter source is unchanged.

Leaving the compiler page forward re-checks the probe. A compiler that is not `x86_64-w64-mingw32` does not receive the automatic project. The compiler page is last, so configure, build, and install use the compiler id current when those pages run.

---

## 5. Technical lessons

These are the corrections the first Windows run forced. The argv in section 3 and the checks in `wincmake.cpp` and `wizard.script` already include them.

**The CMake cache type is not part of the name.** CMake 4.4 wrote `CMAKE_C_COMPILER:STRING` and `CMAKE_CXX_COMPILER:STRING` in the same cache that wrote `CMAKE_INSTALL_PREFIX:PATH` and `CMAKE_MAKE_PROGRAM:FILEPATH`. A check that required `CMAKE_C_COMPILER:FILEPATH` rejected a finished configure, and the wizard reported "CMake finished, but the cache does not record this install prefix and gcc". The check matches `NAME:` and then the value after `=`, for whichever type CMake wrote. Paths compare case-insensitively after slash normalisation. The script uses the same rule in `FLTKWinCacheVariable`.

**The installed version macros are padded.** `include\FL\Enumerations.H` from this install contains `#define FL_MAJOR_VERSION        1`, and the same run of spaces for minor 4 and patch 5. A search for one space between the name and the number fails. `cmake --install` had succeeded, the C++ check allows the extra whitespace, and the script check did not. `OnLeave` of the install page returns the script result with no further dialog, so Next stayed on that page and ran install again. Both checks now accept whitespace between the name and the number and require the number to end at whitespace or a comment. A tree that already satisfies the contract skips the install page.

**A Squirrel `wxString` assignment shares the object.** `local out = path` followed by `out.MakeLower()` lowercases `path` as well. `FLTKWinConfigured` compared the install prefix through that function, so the prefix drawn on the install page was entirely lowercase while the build directory, which had not been passed through it, kept its case. The copy is `path.Mid(0, path.Length())` before `Replace` or `MakeLower`. Windows file lookup is case-insensitive, so the lowercased prefix still found the files.

**The install helper refuses a prefix that is not already in the cache.** `cmake --install` receives the build directory and no replacement `--prefix`. A prefix token that a shell does not expand is a relative directory under the working directory. That is how a hand install, outside the wizard, wrote a tree whose directory name was the unexpanded token.

**Tool search stays on the two absolute candidates.** Git at `%ProgramFiles%\Git\cmd\git.exe` was found by the probe. CMake was absent from `%ProgramFiles%\CMake\bin\cmake.exe` on this machine. The wizard asked once, remembered the chosen `cmake.exe` after `cmake --version`, and did not search `PATH`. The saved keys are `/windows/git/executable` and `/windows/cmake/executable`.

**The built library is `build-codeblocks\lib\libfltk.a`.** The build page is skipped when that file exists. The install page is skipped when `include\FL\Fl.H`, the 1.4.5 macros, and `<install>\lib\libfltk.a` all exist.

**A script change and a DLL change land differently.** The wizard reads `wizard.script` from the installed templates tree when the wizard opens, so a script-only fix is copied into both the devel and output template trees and takes effect on the next wizard run. A new `scriptedwizard.dll` is loaded when Code::Blocks starts.

---

## 6. Milestones

| | Objective | Check | Status |
|---|---|---|---|
| M1 | Add `wingit` without touching `macgit`. | On Windows, probe and try report `git --version`. Clone of `release-1.4.5` lands in the resolved source directory. Cancel starts no process. `macgit.cpp` is unchanged. | Confirmed on one machine. |
| M2 | Add `wincmake` without touching `maccmake`. | Paths resolve under `%LOCALAPPDATA%\codeblocks`. Configure writes `CMakeCache.txt` with the install prefix and the selected GCC. `maccmake.cpp` is unchanged. | Confirmed. The cache records the prefix as `PATH` and gcc as `STRING`. |
| M3 | Build and install the static library. | `cmake --install` leaves `include\FL\Fl.H` reporting 1.4.5 and `lib\libfltk.a`. A failed step does not replace a previous install. | Confirmed, after the version check was taught to allow the padded macros. |
| M4 | Generate the project from the install prefix. | The `.cbp` links that prefix. It has no `$(#fltk)` and no source or build directory. The macOS Homebrew path is unchanged. | Open. The link line is confirmed by a hand link of the starter. The wizard has not yet emitted the `.cbp`. |
| M5 | Run the wizard on the WinLibs compiler. | A fresh project builds without hand repair and opens the FLTK window. A second run skips clone, configure, build, and install while the markers still validate. | Open. A later run does skip the four setup steps. Build and Run of the generated project is still ahead. |

M5 is the wizard. A hand clone does not close M5.

---

## 7. Acceptance

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

## 8. What stays rejected

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

## 9. Done when

A Windows user with the selected 64-bit WinLibs GCC, an existing Git, and an existing CMake can choose the FLTK application from New Project, confirm the shallow clone and the static build once, and build and run the starter. The next run does not ask again while the checkout and the installed tree still validate.

That is the Homebrew step, replaced the way v3.8 replaces it for OGRE: one pinned tag, tools that are found and never installed, and a static library built with the compiler the wizard selected. FLTK stays out of Code::Blocks. The folder page remains for a compiler this build is not offered to.

## References

- [v3.8 OGRE wizard](versioning_plan_v3_8.md)
- [v3.6 FLTK wizard](versioning_plan_v3_6.md)
- [FLTK 1.4.5 release](https://github.com/fltk/fltk/releases/tag/release-1.4.5)
- [v3.3 Windows SDK helper](versioning_plan_v3_3.md)
