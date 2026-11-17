# Code::Blocks v4.2 — OGRE wizard on Windows

**Status: the wizard has built and installed the full SDK on this machine, and the generated project has run.** M3 is exercised. M4's install contract is exercised. SampleBrowser failed first on missing `libfreetype.dll`, `SDL2.dll`, and `libassimp-5.dll`, then crashed in the MSYS2 `libstdc++-6.dll` that PATH supplied. The install step now copies those three libraries and the selected MinGW's `libgcc_s_seh-1.dll`, `libstdc++-6.dll`, and `libwinpthread-1.dll` into the OGRE `bin`. With that copy, SampleBrowser runs. Atomic Counters failed because the saved renderer was the legacy OpenGL plugin, whose GLSL stops at 1.20; the saved renderer is now OpenGL 3+. A full pass of the sample list has not been done, so M4 is not done. M2's second-run skip and cancel have not been exercised. M1's separate hand build was not run. The version is not done. v3.8 is the macOS OGRE wizard. v3.8.2 is the macOS OGRE tools menu. v4.1 is the separate OGRE-Next wizard and is not part of this version. The Code::Blocks installed at `D:\Program Files\CodeBlocks` still shows the old SVN Ogre wizard until this tree is rebuilt and that install is replaced. v4.2 replaces the Windows Ogre wizard with the v3.8 workflow and installs the tools that menu launches.

**Question this version answers:** can the Windows Ogre project wizard follow the same sequence, pins, patch, and generated-project contract as the current v3.8 wizard, with a difference only where Windows has no Homebrew and no `lib/macosx/Release` layout, and can that install include a working SampleBrowser and the command-line tools?

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

Windows adds one more entry in that same known-patch table. It does not edit the sidecar patch.

| Patch id | `ogre-14.6.3-win-config-dialog` |
| --- | --- |
| Patch file | `src/plugins/scriptedwizard/resources/patches/ogre-14.6.3-win-config-dialog.patch` |
| Sentinel | `Components/Bites/src/OgreWIN32ConfigDialog.cpp` |
| Revision check | exact tag `v14.6.3` |

The MinGW configuration dialog was looking for its window layout in `OgreMain.dll`. That layout is linked into `OgreBites.dll`, and `OgreMain.dll` has no resource section, so the first run stopped in `ConfigDialog::display` with "The specified image file did not contain a resource section." The patch changes the lookup to `OgreBites.dll` and `OgreBites_d.dll`. Declining it stops the wizard. Applying it marks the SDK for rebuild.

`Samples/Media`, including `models/ogrehead.mesh`, is a normal directory in that tag. This checkout has no `.gitmodules`. The clone does not need a submodule step to obtain the meshes. CMake installs that directory when `OGRE_INSTALL_SAMPLES` is on, and that option follows `OGRE_BUILD_SAMPLES`.

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

Windows OGRE configure reads `src/plugins/scriptedwizard/resources/patches/ogre-14.6.3-win.options` and passes each `-D` line in that file. Comments in the file are the record of why. These are CMake options, not a source patch, and the stored OGRE patch is not edited. A cache is reused only when it already records every option in that file.

The installed SDK is a full OGRE build. It includes `SampleBrowser` and the command-line tools v3.8.2 uses: `OgreAssimpConverter`, `OgreMeshUpgrader`, `OgreXMLConverter`, and `VRMLConverter`. `OGRE_BUILD_SAMPLES` and `OGRE_BUILD_TOOLS` stay on, so install also copies `Samples/Media`. The generated beginner project is still the rotating head. This version does not add the Windows `Tools -> OGRE` menu.

The options file passes `-DOGRE_BUILD_SAMPLES=ON`, `-DOGRE_BUILD_COMPONENT_OVERLAY_IMGUI=ON`, and `-DOGRE_BUILD_TOOLS=ON`. `-DOGRE_BUILD_DEPENDENCIES` stays `OFF`. With that option left on, OGRE downloads SDL2, Assimp 6, zlib, freetype, pugixml, and Bullet during configure. This MinGW CMake cannot verify those HTTPS certificates, so the archives are empty and generation stops. The wizard already builds SDL2 2.32.10 and Assimp 5.4.3, and Assimp builds its own zlib. Those stay the copies on `CMAKE_PREFIX_PATH`. OGRE's Assimp 6 download is not repeated. Bullet is not required for these tools.

pkgconf comes from MSYS2, and that prefix also contains Qt. Those Qt headers do not compile with the selected MinGW. SampleBrowser and the generated project use SDL2, so the file keeps `-DCMAKE_DISABLE_FIND_PACKAGE_Qt5=ON` and `-DCMAKE_DISABLE_FIND_PACKAGE_Qt6=ON`.

The dependencies the sample browser and the tools still need are cloned with Git, then built with the selected MinGW GCC. The same consent, reuse, and `%LOCALAPPDATA%\codeblocks` rules as SDL2 and Assimp apply. Configure does not fetch them. Each pin is the tag OGRE 14.6.3's CMake names.

| Library | Repository | Tag | Role |
|---|---|---|---|
| Dear ImGui 1.92.9b | `https://github.com/ocornut/imgui.git` | `v1.92.9b` | Overlay and SampleBrowser. The wizard passes `-DIMGUI_DIR` at that checkout. Overlay's CMake looks for `imgui-1.92.9b` and otherwise downloads it. |
| pugixml 1.15 | `https://github.com/zeux/pugixml.git` | `v1.15` | `OgreXMLConverter` and the DotScene plugin. Built static. The shared target compiles a Windows resource with windres, and windres launches the preprocessor from an unquoted path. This MinGW is under Program Files, so that command stops at the space. |
| FreeType 2.14.3 | `https://gitlab.freedesktop.org/freetype/freetype.git` | `VER-2-14-3` | Dear ImGui text in the overlay. OGRE's downloader uses the Savannah tarball of this same release. |

Source leaves are `imgui-source-1.92.9b`, `pugixml-source-1.15`, and `freetype-source-2.14.3`, each with `build-codeblocks` inside. Install leaves are `imgui`, `pugixml`, and `freetype`. pugixml and FreeType join `CMAKE_PREFIX_PATH`. Dear ImGui is a source tree, not an installed library, because Overlay compiles those sources itself.

### 3.4 Installed layout and the generated project

The macOS contract checks `lib/macosx/Release` and frameworks. Windows checks the DLLs and import libraries the install writes, then rebases `plugins.cfg` to that plugin directory. The template `ogre/files/plugins.cfg` keeps the `[OGRE_PLUGIN_FOLDER]` placeholder. The folder string is the Windows install layout, not `lib/macosx/Release`.

The wizard install on this machine wrote the runtime DLLs in `bin`, including `bin\RenderSystem_GL.dll`, `bin\SampleBrowser.exe`, and the four command-line tools. The import library is `lib\libOgreMain.dll.a`. The install also has `include\OGRE\Ogre.h`, `Media\models\ogrehead.mesh`, `bin\OgreMain.dll`, and `OGREConfig.cmake`. The search still accepts `bin\Release` and the `lib\OGRE` layouts, because a hand build might differ. This wizard install did not.

`resources.cfg` still rebases `[OGRE_MEDIA]` at the installed `Media` directory and drops the `[Tests]` section.

The generated project uses `ogre/files/main.cpp` unchanged: one camera, ambient and point light, textured `ogrehead.mesh`, slow vertical rotation. It is an ordinary Code::Blocks C++ project with Debug and Release targets. It does not point at the source or build tree, and it does not link `OgreMain_d` or `OIS`.

Windows copies the runtime DLLs the executable needs beside the built executable. That copy is the Windows counterpart of the macOS framework install names. Beginner-facing `main.cpp` does not perform it. `plugins.cfg` still names the OGRE install `bin` as `PluginFolder`, and Windows loads a plugin from that directory, then looks for that plugin's own dependencies beside `ogre.exe`. The copy list is therefore the OGRE DLLs the program and `plugins.cfg` import, plus `OgrePaging.dll` and `OgreTerrain.dll` (`Plugin_DotScene` imports `OgreTerrain`, and `OgreTerrain` imports `OgrePaging`), `SDL2.dll` from the SDL2 prefix, and `libfreetype.dll` from the FreeType prefix. `OgreOverlay.dll` imports that FreeType filename exactly. Both Debug and Release use the project directory as the working directory, which is where `resources.cfg` and `plugins.cfg` are written. When the wizard finishes, it shows the project tree and opens `main.cpp`.

SampleBrowser and the command-line tools are started from the install `bin`. Windows records only the DLL filename, not a search directory, and `OGRE_INSTALL_DEPENDENCIES` copies DLLs only from the OGRE dependency bundle this build does not use. After install, the wizard copies `SDL2.dll`, `libfreetype.dll`, and `libassimp-5.dll` into that `bin`. SampleBrowser imports the first two through `OgreBites` and `OgreOverlay`. Its `plugins.cfg` also loads `Codec_Assimp`, which imports `libassimp-5.dll`. It also copies `libgcc_s_seh-1.dll`, `libstdc++-6.dll`, and `libwinpthread-1.dll` from the selected MinGW, because the executable directory is searched before PATH. On this machine PATH finds a different `libstdc++-6.dll` under `D:\msys64\mingw64\bin`, and that copy crashes SampleBrowser inside `Ogre::Log`. The generated project copies the same three runtime DLLs beside `ogre.exe`. The installed `plugins.cfg` comments out `RenderSystem_GL` and `RenderSystem_GLES2`, leaving `RenderSystem_GL3Plus`. The legacy plugin's highest GLSL profile is `glsl120`. Atomic Counters uses `syntax glsl400`, so that plugin reports the fragment program as not supported even when the GPU is OpenGL 4.6. On this machine the saved choice in `Documents\OGRE Sample Browser\ogre.cfg` was the legacy plugin; it is now OpenGL 3+.

There is no separate asset-conversion page. v3.8 records that idea as M5 and the wizard does not implement it yet. v4.2 does not add one.

---

## 4. Milestones

| | Objective | Check | Status |
|---|---|---|---|
| M1 | Build tag `v14.6.3` by hand with the selected MinGW GCC, apply the stored patch, and install under `%LOCALAPPDATA%\codeblocks\ogre-14.6.3`. | An external project, not the wizard, builds and renders the textured, lit, rotating `ogrehead.mesh`. Stop if the stored patch does not apply to this tag. | Not run. The wizard install recorded the layout in section 3.4 instead. |
| M2 | Drive that same sequence from the Ogre wizard on Windows. | A second run skips a checkout, patch state, build tree, or SDK that already matches. Cancel does not delete a tree the run did not create. The macOS wizard is unchanged. | Implemented. One full run completed. The second-run skip and cancel have not been exercised. |
| M3 | Generate the ordinary C++ project. | Build and Run from the Windows IDE opens the same scene. The project contains no source-tree or build-tree path. | Exercised. The generated project built and ran. The first run needed `libfreetype.dll`, `OgreTerrain.dll`, and `OgrePaging.dll` beside the executable, and the configuration-dialog patch in section 2. |
| M4 | Install a full SDK: SampleBrowser, the command-line tools, and their git dependencies. | `%LOCALAPPDATA%\codeblocks` contains the Dear ImGui, pugixml, and FreeType checkouts from the table in section 3.3, built with the selected MinGW. The OGRE install contains `SampleBrowser`, `OgreAssimpConverter`, `OgreMeshUpgrader`, `OgreXMLConverter`, `VRMLConverter`, and `Media\models\ogrehead.mesh`. SampleBrowser opens. Configure does not download those dependencies. | Install exercised. The wizard finished the build and install, including those dependencies and the install contract. SampleBrowser did not start until `libfreetype.dll`, `SDL2.dll`, `libassimp-5.dll`, and the selected MinGW runtime were copied into the install `bin`. That copy is part of the install step. With those files present, SampleBrowser opens and runs samples. Atomic Counters stays unsupported until the renderer is OpenGL 3+ rather than the legacy OpenGL plugin. That is now the saved renderer. A full pass of the sample list has not been done. |

---

## 5. Acceptance

1. The Windows wizard clones `v14.6.3` and applies `ogre-14.6.3-assimp-sidecar-path.patch` unchanged.
2. Configure is `Release` with the final prefix set before the build. Install does not move the prefix.
3. Git is never installed. CMake, pkgconf, SDL2, and Assimp are installed only after Yes, and only from the pins in section 3.3.
4. The compiler is the selected GNU GCC. No second toolchain is downloaded.
5. Source, build, and install stay under `%LOCALAPPDATA%\codeblocks` with the v3.8 leaf names.
6. The generated project builds and runs against the installed SDK only.
7. `main.cpp` is the existing v3.8 template.
8. `macgit`, `maccmake`, `macbrew`, the macOS Ogre pages, and the OGRE-Next wizard are unchanged.
9. The old `OgreMain` / `OIS` Windows branch is no longer what the Ogre wizard runs.
10. The install contains a working SampleBrowser and `OgreAssimpConverter`, `OgreMeshUpgrader`, `OgreXMLConverter`, and `VRMLConverter`. Dear ImGui, pugixml, and FreeType were cloned from the pinned tags. OGRE configure did not download them.

---

## 6. Rejected

- A Windows OGRE-Next wizard, or the v4.1 pins and patches.
- A second OGRE tag, or a Windows edit of the stored OGRE patch.
- Homebrew, MSYS2, or MSVC as the OGRE build.
- Installing Git.
- Putting the SDK in `%APPDATA%\CodeBlocks`, beside `codeblocks.exe`, or in Program Files.
- Leaving the generated project pointed at the source or build tree.
- Adding the unfinished v3.8 asset-conversion page.
- A Windows `Tools -> OGRE` menu. v3.8.2 remains the macOS tools menu. This version installs the binaries that menu launches.
- Leaving samples and the Dear ImGui overlay off in the finished options file.
- Letting OGRE's CMake download Dear ImGui, pugixml, FreeType, or the other dependency archives during configure.
- Rebuilding the v4.0 installer as a requirement of this plan. A rebuilt IDE is how the wizard reaches a machine; the installer script is not part of the work.

---

## 7. Done when

On Windows, File → New → Project → Ogre project can acquire or reuse OGRE `v14.6.3`, apply the same patch, install a full SDK with the selected MinGW GCC, and generate a C++ project whose Build and Run shows the textured rotating head. That SDK includes a working SampleBrowser and the command-line tools, with Dear ImGui, pugixml, and FreeType cloned from git. The macOS wizard still does the v3.8 project flow on macOS, unchanged.
