# Code::Blocks v4.2 — OGRE wizard on Windows

**Status: M2 and M3 are implemented in the wizard. M1 has not been run, so none of the milestones are done.** v3.8 is the macOS OGRE wizard. v4.1 is the separate OGRE-Next wizard and is not part of this version. The Code::Blocks installed at `D:\Program Files\CodeBlocks` still shows the old SVN Ogre wizard until this tree is rebuilt and that install is replaced. v4.2 replaces the Windows Ogre wizard with the v3.8 workflow.

**Question this version answers:** can the Windows Ogre project wizard follow the same sequence, pins, patch, and generated-project contract as the current v3.8 wizard, with a difference only where Windows has no Homebrew and no `lib/macosx/Release` layout?

The pin is the one the current wizard script and patch table use, not the earlier `v14.6.2` line still written in `versioning_plan_v3_8.md`.

---

## 1. Scope

```text
File -> New -> Project -> Ogre project
    |
    +-- macOS                         unchanged v3.8 path
    +-- Windows                       this version
            |
            +-- same tag, same patch, same page order
            +-- WinGit, WinCMake, Windows patch binding
            +-- selected MinGW GCC
            +-- %LOCALAPPDATA%\codeblocks\ogre-14.6.3
            +-- ordinary C++ project
```

OGRE-Next stays registered only when `PLATFORM == PLATFORM_MAC`. This version does not add a Windows OGRE-Next entry and does not apply the v4.1 patches.

The legacy Windows branch in `SetupProject` (`OgreMain`, `OIS`, `bin\Debug`, `bin\Release`) is the behaviour being replaced. Unix behaviour other than the existing macOS path stays as it is.

---

## 2. Same pin and the same patch

| Item | Value |
|---|---|
| Repository | `https://github.com/OGRECave/ogre.git` |
| Tag | `v14.6.3` |
| Checkout | `git clone --branch v14.6.3 --depth 1` |
| Source leaf | `ogre-source-14.6.3` |
| Build leaf | `build-codeblocks` inside the source leaf |
| Install leaf | `ogre-14.6.3` |
| Build type | `Release` |
| Patch id | `ogre-14.6.3-assimp-sidecar` |
| Patch file | `src/plugins/scriptedwizard/resources/patches/ogre-14.6.3-assimp-sidecar-path.patch` |
| Revision check | exact tag `v14.6.3` |

The patch file is applied as stored. It is not rewritten into a Windows patch. The `Samples/Browser/CMakeLists.txt` hunk is inside `if (APPLE)` and remains in the file. The `Tools/AssimpConverter/main.cpp` hunk is the shared change. The probe sentinel stays `PlugIns/Assimp/src/AssimpLoader.cpp`.

Consent, cancellation, and skip-if-present follow the macOS pages:

1. Git is usable, or the user points at an existing `git.exe`. Code::Blocks does not install Git.
2. The checkout is absent, or it is already that tag. A different existing directory is left untouched.
3. The patch is absent, already applied, or declined. An unexpected tree is not modified.
4. CMake cache is absent, or its prefix and `Release` already match.
5. The installed SDK already satisfies the file contract.

`CMAKE_INSTALL_PREFIX` is passed at configure time. Install does not pass a replacement `--prefix`.

---

## 3. Windows specificities

These are the only intended differences.

### 3.1 Helpers

`macgit`, `maccmake`, `macbrew`, and the macOS wizard pages stay macOS-only. Windows calls the existing pair from v3.6b:

```text
Wizard.WinGitProbe()
Wizard.WinGitTry(path)
Wizard.WinGitClone(url, tag, destination, explanation, confirmLabel)
Wizard.WinCMakeProbe()
Wizard.WinCMakeTry(path)
Wizard.WinCMakePaths(sourceLeaf, installLeaf)
Wizard.WinCMakeConfigure(...)
Wizard.WinCMakeBuild(...)
Wizard.WinCMakeInstall(...)
```

Patch probe and apply need a Windows binding with the same states as `Wizard.MacPatchProbe` (`applicable`, `already_applied`, `unsupported_version`, `git_unavailable`, `unexpected_source`). That binding reads the same patch id and the same patch file. It does not grow a second patch list.

`WinCMakeConfigure` today is the FLTK argv. v4.2 adds an OGRE configure operation with a fixed argv, or a separate function, rather than overloading the FLTK arguments. The OGRE argv is the macOS argv without the two macOS entries:

```text
-S <source>
-B <build>
-G "MinGW Makefiles"
-DCMAKE_BUILD_TYPE=Release
-DCMAKE_INSTALL_PREFIX=<install>
-DCMAKE_C_COMPILER=<selected gcc.exe>
-DCMAKE_CXX_COMPILER=<selected g++.exe>
-DCMAKE_MAKE_PROGRAM=<selected mingw32-make.exe>
```

Not passed on Windows:

```text
-DCMAKE_INSTALL_RPATH=<install>/lib/macosx/Release
-DOGRE_PLUGINS_PATH=lib/macosx/Release
```

Windows also passes `-DCMAKE_PREFIX_PATH` set to the selected MinGW prefix plus any detected SDL2 or Assimp prefix that is not already that directory. That is how the detected headers become visible to OGRE's CMake. It is not a macOS rpath or plugin-path setting.

The selected compiler is the active GNU GCC installation directory. If `gcc.exe`, `g++.exe`, or `mingw32-make.exe` is missing there, the wizard stops. This version does not download a compiler.

### 3.2 Directories

`WinCMakePaths` already resolves:

```text
%LOCALAPPDATA%\codeblocks\ogre-source-14.6.3
%LOCALAPPDATA%\codeblocks\ogre-source-14.6.3\build-codeblocks
%LOCALAPPDATA%\codeblocks\ogre-14.6.3
```

That root is the Windows counterpart of `~/Library/CodeBlocks`. It is not `%APPDATA%\CodeBlocks`, not the IDE install directory, and not Program Files. The leaf names stay `ogre-source-14.6.3` and `ogre-14.6.3`.

The wizard keeps using the `%LOCALAPPDATA%\codeblocks` path. When that directory is a symlink or junction, file operations follow it, and path comparisons treat the link and its target as the same directory. A target on another drive is accepted. A target under Program Files, or beside `codeblocks.exe`, is still refused.

### 3.3 Prerequisites

The macOS wizard requires Homebrew formulas `cmake`, `pkgconf`, `sdl2`, and `assimp`. Windows has no Homebrew step. The four roles stay:

| Role | Windows rule |
|---|---|
| Git | Detect only, through `WinGit`. Never installed. |
| CMake | Detect through `WinCMake`, including `cmake.exe` in the selected MinGW `bin` when it is present. |
| pkgconf | Detect `pkgconf.exe` or `pkg-config.exe`. |
| SDL2 | Detect a development tree that contains the SDL2 headers OGRE's CMake looks for. |
| Assimp | Detect `include/assimp/Importer.hpp`, the same check the macOS page uses. |

A missing CMake, pkgconf, SDL2, or Assimp is installed only after an explicit Yes. The bits are not unpacked into the OGRE source tree and are not vendored into Code::Blocks. Git, CMake, pkgconf, and the compiler are not downloaded.

SDL2 and Assimp are reused when the header check already succeeds under the selected MinGW prefix or under the install leaf below. Otherwise the wizard asks, then downloads the pinned official source and builds it with the selected MinGW GCC. A second run skips a source tree, CMake cache, or install that already matches.

| Library | Source | Install leaf | SHA-256 |
|---|---|---|---|
| SDL 2.32.10 | `https://github.com/libsdl-org/SDL/releases/download/release-2.32.10/SDL2-2.32.10.tar.gz` | `%LOCALAPPDATA%\codeblocks\sdl2` | `5f5993c530f084535c65a6879e9b26ad441169b3e25d789d83287040a9ca5165` |
| Assimp 5.4.3 | `https://github.com/assimp/assimp/archive/refs/tags/v5.4.3.tar.gz` | `%LOCALAPPDATA%\codeblocks\assimp` | `66dfbaee288f2bc43172440a55d0235dfc7bf885dda6435c038e8000e79582cb` |

Source leaves are `sdl2-source-2.32.10` and `assimp-source-5.4.3`, each with `build-codeblocks` inside. Configure is `Release` with the install prefix fixed first. SDL2 is built shared, without the test library. Assimp is built shared, with its vendored zlib, and without its tools and tests. pkg-config is limited to the selected MinGW prefix and these install prefixes, so a different MinGW on `PATH` is not linked in.

Assimp 5.4.3 is unpacked from the pinned archive, so that tree has no Git checkout. Before configure, the wizard applies one more entry in the same known-patch table:

| Patch id | `assimp-5.4.3-ccache-path` |
| --- | --- |
| Patch file | `src/plugins/scriptedwizard/resources/patches/assimp-5.4.3-ccache-path.patch` |
| Sentinel | `CMakeLists.txt` |
| Marker | `PROJECT(Assimp VERSION 5.4.3)` |

The official file launches `ccache` whenever `ccache.exe` is found. The MinGW makefile generator writes that path unquoted, and a path containing a space makes `mingw32-make` stop on the first source file. The patch skips that launcher only when the path contains a space. ccache stays in use when its path has no space. The stored OGRE patch is not edited. A `CMakeLists.txt` newer than `CMakeCache.txt` is configured again. A cache counts as configured only when the MinGW `Makefile` was generated.

OGRE configure passes `-DOGRE_BUILD_DEPENDENCIES=OFF`, `-DOGRE_BUILD_COMPONENT_OVERLAY_IMGUI=OFF`, and `-DOGRE_BUILD_SAMPLES=OFF`. With the dependencies option left on, OGRE downloads SDL2, Assimp 6, zlib, freetype, pugixml, and Bullet during configure. This MinGW CMake cannot verify those HTTPS certificates, so the archives are empty and generation stops. SDL2 and Assimp 5.4.3 are the trees already on `CMAKE_PREFIX_PATH`. The generated project does not build OGRE's sample browser or the Dear ImGui overlay.

### 3.4 Installed layout and the generated project

The macOS contract checks `lib/macosx/Release` and frameworks. Windows checks the DLLs and import libraries the install writes, then rebases `plugins.cfg` to that plugin directory. The template `ogre/files/plugins.cfg` keeps the `[OGRE_PLUGIN_FOLDER]` placeholder. The folder string is the Windows install layout, not `lib/macosx/Release`.

Until M1 records the directory this machine actually produced, the wizard accepts the first existing file among `bin\RenderSystem_GL.dll`, `bin\Release\RenderSystem_GL.dll`, `lib\OGRE\RenderSystem_GL.dll`, `lib\Release\RenderSystem_GL.dll`, and `lib\RenderSystem_GL.dll`. The import library is `libOgreMain.dll.a` under `lib`, `lib\Release`, or `lib\OGRE`. The install also requires `include\OGRE\Ogre.h`, `Media\models\ogrehead.mesh`, `OgreMain.dll`, and `OGREConfig.cmake`. M1 replaces this search with the directory that build wrote if it is different.

`resources.cfg` still rebases `[OGRE_MEDIA]` at the installed `Media` directory and drops the `[Tests]` section.

The generated project uses `ogre/files/main.cpp` unchanged: one camera, ambient and point light, textured `ogrehead.mesh`, slow vertical rotation. It is an ordinary Code::Blocks C++ project with Debug and Release targets. It does not point at the source or build tree, and it does not link `OgreMain_d` or `OIS`.

Windows copies the OGRE runtime DLLs the executable needs, including the renderer and codec plugins named by `plugins.cfg`, beside the built executable. That copy is the Windows counterpart of the macOS framework install names. Beginner-facing `main.cpp` does not perform it.

There is no separate asset-conversion page. v3.8 records that idea as M5 and the wizard does not implement it yet. v4.2 does not add one.

---

## 4. Milestones

| | Objective | Check | Status |
|---|---|---|---|
| M1 | Build tag `v14.6.3` by hand with the selected MinGW GCC, apply the stored patch, and install under `%LOCALAPPDATA%\codeblocks\ogre-14.6.3`. | An external project, not the wizard, builds and renders the textured, lit, rotating `ogrehead.mesh`. The plugin directory, import libraries, and any prerequisite downloads used are written into this plan. Stop if the stored patch does not apply to this tag. | Not run |
| M2 | Drive that same sequence from the Ogre wizard on Windows. | A second run skips a checkout, patch state, build tree, or SDK that already matches. Cancel does not delete a tree the run did not create. The macOS wizard is unchanged. | Implemented, not exercised |
| M3 | Generate the ordinary C++ project. | Build and Run from the Windows IDE opens the same scene. The project contains no source-tree or build-tree path. Closing the IDE does not rewrite `%APPDATA%\CodeBlocks` except for the normal project and layout records. | Implemented, not exercised |

---

## 5. Acceptance

1. The Windows wizard clones `v14.6.3` and applies `ogre-14.6.3-assimp-sidecar-path.patch` unchanged.
2. Configure is `Release` with the final prefix set before the build. Install does not move the prefix.
3. Git is never installed. CMake, pkgconf, SDL2, and Assimp are installed only after Yes, and only from artifacts recorded in M1.
4. The compiler is the selected GNU GCC. No second toolchain is downloaded.
5. Source, build, and install stay under `%LOCALAPPDATA%\codeblocks` with the v3.8 leaf names.
6. The generated project builds and runs against the installed SDK only.
7. `main.cpp` is the existing v3.8 template.
8. `macgit`, `maccmake`, `macbrew`, the macOS Ogre pages, and the OGRE-Next wizard are unchanged.
9. The old `OgreMain` / `OIS` Windows branch is no longer what the Ogre wizard runs.

---

## 6. Rejected

- A Windows OGRE-Next wizard, or the v4.1 pins and patches.
- A second OGRE tag, or a Windows edit of the stored OGRE patch.
- Homebrew, MSYS2, or MSVC as the OGRE build.
- Installing Git.
- Putting the SDK in `%APPDATA%\CodeBlocks`, beside `codeblocks.exe`, or in Program Files.
- Leaving the generated project pointed at the source or build tree.
- Adding the unfinished v3.8 asset-conversion page.
- Rebuilding the v4.0 installer as a requirement of this plan. A rebuilt IDE is how the wizard reaches a machine; the installer script is not part of the work.

---

## 7. Done when

On Windows, File → New → Project → Ogre project can acquire or reuse OGRE `v14.6.3`, apply the same patch, install it with the selected MinGW GCC, and generate a C++ project whose Build and Run shows the textured rotating head. The macOS wizard still does that on macOS, unchanged.
