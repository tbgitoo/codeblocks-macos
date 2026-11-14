# Code::Blocks v3.9 - OGRE-Next wizard, macOS first

**Status: M3 done.** The Windows manual install in `plans/thoughts/ogre-next-windows-codeblocks-mingw-student-guide.md` has been run from start to finish. The same pins build and install on Apple Silicon. The wizard helpers can check out those two revisions, apply the packaged patches, and configure, build, and install both trees. The OGRE-Next project entry is not registered yet. The existing OGRE 1.x wizard is left unchanged. The two packaged MinGW patches are unchanged.

**Question this version answers:** can Code::Blocks offer a separate OGRE-Next application wizard, on the same pattern as the v3.8 OGRE wizard, that acquires the pinned dependency and engine sources, applies the proven compatibility patches through the same constrained patch helper, installs a clean SDK, and generates an ordinary Code::Blocks project against that SDK?

OGRE-Next is not a replacement for OGRE 1.x. The two stay separate project choices, scripts, resources, and starter code.

---

## 1. Scope and design boundary

```text
Code::Blocks
    |
    +-- existing OGRE 1.x wizard          unchanged
    +-- new OGRE-Next wizard              this version, macOS first
            |
            +-- macbrew    declared prerequisites only
            +-- macgit     exact commits, never installs Git
            +-- macpatch   the two proven patches, same contract as
            |              ogre-14.6.3-assimp-sidecar
            +-- maccmake   configure, build, install
            |
            +-- installed dependency SDK
            +-- installed OGRE-Next SDK
            +-- ordinary generated C++ project
```

The Windows recipe below is the proven pin and patch baseline. It is not the implementation target while the Windows machine is unavailable. A later Windows wizard must consume this same pin and these same patch files. It must not invent a second pair of revisions.

The wizard hides acquisition, patching, and installation. The generated program remains ordinary C++ that uses the installed SDK.

---

## 2. Proven Windows baseline - DONE

Executed with the Code::Blocks MinGW toolchain, not another GCC on `PATH`:

```text
compiler root     D:\Program Files\CodeBlocks\MinGW
gcc -dumpmachine  x86_64-w64-mingw32
GCC               14.2.0
generator         MinGW Makefiles
build type        Release
```

`gcc`, `g++`, `ar`, `mingw32-make`, and `windres` all resolved from that one `bin` directory.

### 2.1 Pinned revisions

These are the revisions that completed the guide. They replace the earlier draft's floating `v3.0.0` tag.

```text
ogre-next-deps   f7e23191f4fd89ed41134996bc47f9ee23d009df
SDL2 submodule   5d249570393f7a37e037abf22cd6012a4cc56a71
OGRE-Next        4859016ad2cc956a653dfc4e4a859505696c6509
branch           v3-0
version          Eris 3.0.0
```

The dependency checkout is the parent commit, then `git submodule update --init --recursive --depth 1`. The SDL2 hash is the submodule that commit selects. The engine checkout is branch `v3-0` followed by that exact commit. A later `master` or a retagged `v3.0.0` is not this pin.

### 2.2 Patches that were required

Both files are already packaged:

```text
src/plugins/scriptedwizard/resources/patches/ogre-next-deps-mingw.patch
src/plugins/scriptedwizard/resources/patches/ogre-next-v3-0-mingw.patch
```

`git apply` of the dependency patch changes only:

```text
src/CMakeLists.txt
src/FreeImage/CMakeLists.txt
src/FreeImage/Source/OpenEXR/Imath/ImathVec.h
src/Remotery/CMakeLists.txt
src/Remotery/src/Remotery.c
src/zziplib/zzip/plugin.c
```

The guide's manual edit also deleted `src/FreeImage/Source/LibJXR/common/include/guiddef.h`. The packaged patch instead stops exporting that directory as a target-wide include, and adds it only for the LibJXR glue sources. That packaged form is the one the successful build used. The wizard applies the packaged patch, not the earlier hand-edit list.

`git apply` of the engine patch changes only:

```text
CMake/Packages/FindDirectX11.cmake
RenderSystems/Direct3D11/src/OgreD3D11Device.cpp
```

Most hunks are inside `MINGW`, `WIN32`, or `__MINGW32__`. The FreeImage include narrowing, the `<cstdint>` include, and the Remotery `rsize_t` condition are not fully guarded. Section 4 says how the macOS wizard treats that fact.

### 2.3 Commands that succeeded

Dependencies, from the pinned checkout after the dependency patch:

```bat
cmake -S . -B build ^
  -G "MinGW Makefiles" ^
  -DCMAKE_BUILD_TYPE=Release ^
  -DCMAKE_MAKE_PROGRAM=mingw32-make.exe ^
  -DCMAKE_C_COMPILER="%CB_MINGW%\bin\gcc.exe" ^
  -DCMAKE_CXX_COMPILER="%CB_MINGW%\bin\g++.exe" ^
  -DCMAKE_RC_COMPILER=windres.exe

cmake --build build --parallel 4
cmake --install build
```

The dependency SDK landed at:

```text
ogre-next-deps\build\ogredeps
```

Required files there included `include/FreeImage.h`, `include/SDL2/SDL.h`, `include/shaderc/shaderc.h`, `lib/Release/libFreeImage.a`, `lib/libSDL2.dll.a`, `lib/libshaderc_combined.a`, and `bin/Release/SDL2.dll`.

OGRE-Next, from the pinned checkout after the engine patch:

```bat
cmake -S . -B build ^
  -G "MinGW Makefiles" ^
  -DCMAKE_BUILD_TYPE=Release ^
  -DCMAKE_MAKE_PROGRAM=mingw32-make.exe ^
  -DCMAKE_C_COMPILER="%CB_MINGW%\bin\gcc.exe" ^
  -DCMAKE_CXX_COMPILER="%CB_MINGW%\bin\g++.exe" ^
  -DCMAKE_RC_COMPILER=windres.exe ^
  -DOGRE_DEPENDENCIES_DIR=<deps>/build/ogredeps ^
  -DCMAKE_INSTALL_PREFIX=<work>/install

cmake --build build --parallel 4
cmake --install build
```

The installed engine SDK contained `include/OGRE/OgreRoot.h`, the Unlit and PBS HLMS headers, `lib/Release/libOgreMain.dll.a`, `bin/Release/OgreMain.dll`, the Direct3D11, GL3Plus, and Vulkan render-system DLLs, `plugins.cfg`, `resources2.cfg`, and `Media/Hlms/Unlit` and `Media/Hlms/Pbs`.

### 2.4 Independent program

The program that opened the window is:

```text
plans/thoughts/ogre_next_example/Tutorial00_Basic.cpp
plans/thoughts/ogre_next_example/CMakeLists.txt
plans/thoughts/ogre_next_example/ogrehead.mesh
```

It was configured with `-DOGRE_SDK=` pointing at the install prefix only. It linked `OgreHlmsPbs`, `OgreHlmsUnlit`, and `OgreMain`. It was copied beside the installed `plugins.cfg` and run from `install/bin/Release`.

Success was:

1. a window titled `OGRE-Next Light Show`;
2. the Ogre head visible and rotating;
3. red, blue, and gold lights moving around it;
4. the console naming the selected render system;
5. closing the window printing `Light show rendered and closed cleanly.`;
6. `Ogre.log` recording initialization and a clean shutdown.

---

## 3. macOS wizard, following v3.8

v3.8 acquires one upstream tree, configures with the install prefix fixed before the build, installs under `~/Library/CodeBlocks`, and generates an ordinary Code::Blocks project that never refers to the source or build tree. v3.9 does the same for two trees: `ogre-next-deps`, then OGRE-Next.

Reuse the existing helpers. Do not add a general Git, CMake, or patch command API.

```text
Wizard.MacGitProbe / MacGitTry / MacGitClone
Wizard.MacGitAcquire(sourceId, explanation, confirmLabel)
Wizard.MacCMakeProbe / MacCMakePaths / MacCMakeConfigure
Wizard.MacCMakeOgreNext(projectId, operation, explanation, confirmLabel)
Wizard.MacPatchProbe / MacPatchApply
```

`MacGitAcquire` accepts only `ogre-next-deps` or `ogre-next`. The URL, commit, SDL2 pin, and destination are compiled in. The destination is `wxGetHomeDir()` plus `Library/CodeBlocks/` plus `ogre-next-deps-source-3.0.0` or `ogre-next-source-3.0.0`. A directory that is already that commit, and for the dependency tree also SDL2 `5d249570393f7a37e037abf22cd6012a4cc56a71`, is reported present and Git is not started. Any other existing directory is left untouched. Cancel before launch does not start Git.

`MacCMakeConfigure` still configures one OGRE 1.x tree, with the same arguments as before. `MacCMakeOgreNext` is the OGRE-Next sequence. `projectId` is only `ogre-next-deps` or `ogre-next`. `operation` is only `configure`, `build`, or `install`. The script cannot pass a path, a prefix, or a `-D` flag.

Dependency configure passes `Release`, `arm64`, and the SDK path from `xcrun --show-sdk-path`. It does not pass `CMAKE_INSTALL_PREFIX`. After configure, the cache prefix must be `<deps build>/ogredeps`. A cache whose prefix is already something else is left unchanged and CMake is not started. An empty `CMAKE_OSX_SYSROOT` from the M1 configure still matches; any other value must be that SDK path.

Engine configure passes `Release`, `arm64`, the SDK path, `CMAKE_INSTALL_PREFIX` set to `~/Library/CodeBlocks/ogre-next-3.0.0`, `OGRE_DEPENDENCIES_DIR` set to the `ogredeps` directory, and samples and tests off. It does not start unless the dependency file list is present. Install runs `cmake --install <build>` with no `--prefix`, and only after the cache prefix matches. Success requires the section 3.1 file list. Exit status alone is not success.

A cache and, for build or install, a file list that already match return success without a confirmation dialog and without starting CMake. Cancelling the confirmation does not start CMake.

Paths are resolved in native code with `wxGetHomeDir()`, as in v3.8. No username and no `D:\` literal in the macOS workflow.

Layout confirmed by M1:

```text
~/Library/CodeBlocks/
    ogre-next-deps-source-3.0.0/
        build-codeblocks/
            ogredeps/                  dependency SDK, same place as Windows
    ogre-next-source-3.0.0/
        build-codeblocks/
    ogre-next-3.0.0/                   CMAKE_INSTALL_PREFIX for the engine
```

`CMAKE_INSTALL_PREFIX` for OGRE-Next is passed at configure time. A later `cmake --install --prefix` is not the contract. v3.8 showed that swapping the prefix at install time leaves metadata pointed at the build tree.

The dependency project installs into `<build>/ogredeps` on Mac as it did on Windows. The engine configure receives that directory as `OGRE_DEPENDENCIES_DIR`. The generated project never receives it. The generated project receives only `~/Library/CodeBlocks/ogre-next-3.0.0`.

Homebrew stays limited to `cmake` and `pkgconf`. M1 did not need another formula. The Apple dependency defaults leave SDL2 and shaderc unbuilt, and the engine build with samples off did not link the Homebrew SDL2 that CMake noticed. Git is validated and never installed. Xcode's compiler is assumed, not installed by the wizard.

A second wizard run skips a stage whose checkout hash, patch state, build tree, or installed SDK already matches this pin.

### 3.1 What M1 actually installed

Checked out and left in place, outside this repository:

```text
ogre-next-deps   f7e23191f4fd89ed41134996bc47f9ee23d009df
SDL2 submodule   5d249570393f7a37e037abf22cd6012a4cc56a71
OGRE-Next        4859016ad2cc956a653dfc4e4a859505696c6509
reported version 3.0.0 (Eris)
```

Toolchain: Apple clang 21, CMake 4.4.5, Xcode 26.5, arm64, 12 cores. Samples and tests were left off. The dependency SDK is `ogre-next-deps-source-3.0.0/build-codeblocks/ogredeps`. It contains `include/FreeImage.h`, `include/ft2build.h`, `include/Remotery.h`, `include/rapidjson/`, `lib/Release/libFreeImage.a`, `libfreetype.a`, `libzziplib.a`, `libzlib.a`, and `libRemotery.dylib`. It does not contain SDL2 or shaderc.

The engine prefix `~/Library/CodeBlocks/ogre-next-3.0.0` contains:

```text
include/OGRE/OgreRoot.h
include/OGRE/Hlms/Common/
include/OGRE/Hlms/Unlit/OgreHlmsUnlit.h
include/OGRE/Hlms/Pbs/OgreHlmsPbs.h
include/OGRE/Compositor/
lib/Release/Ogre.framework/Versions/3.0/Ogre
lib/Release/OgreHlmsPbs.framework/Versions/3.0/OgreHlmsPbs
lib/Release/OgreHlmsUnlit.framework/Versions/3.0/OgreHlmsUnlit
lib/Release/RenderSystem_Metal.framework/Versions/3.0/RenderSystem_Metal
lib/Release/RenderSystem_GL3Plus.framework/Versions/3.0/RenderSystem_GL3Plus
lib/Release/RenderSystem_NULL.framework/Versions/3.0/RenderSystem_NULL
lib/Release/Plugin_ParticleFX.framework/Versions/3.0/Plugin_ParticleFX
bin/plugins.cfg
bin/resources2.cfg
Media/Hlms/Pbs/Metal
Media/Hlms/Unlit/Metal
Media/models/ogrehead.mesh
Media/materials/textures/GreenSkin.jpg
Media/materials/textures/tusk.jpg
```

There is no `libOgreMain`. The engine library is the `Ogre` framework. Install names are `@executable_path/../Frameworks/<Name>.framework/Versions/3.0/<Name>`. `bin/plugins.cfg` and `bin/resources2.cfg` are the sample-browser copies. A generated project does not use them as-is. `resources2.cfg` still points at `Samples/Media` and `Contents/Resources`.

M3 validates this file list, not a Windows `lib/Release/libOgreMain.dll.a` path.

---

## 4. Patch delivery

The two patches are applied the way `ogre-14.6.3-assimp-sidecar-path.patch` is applied.

That helper accepts only a known patch id. The script cannot supply a path, a diff, a target file, or a Git command. The helper checks that the checkout is the supported revision, runs `git apply` on the packaged file, and treats an already-applied patch as success.

Register two ids beside the existing Assimp id:

```text
patch id        ogre-next-deps-mingw
packaged file   ogre-next-deps-mingw.patch
revision        f7e23191f4fd89ed41134996bc47f9ee23d009df

patch id        ogre-next-deps-apple
packaged file   ogre-next-deps-apple.patch
revision        f7e23191f4fd89ed41134996bc47f9ee23d009df
applies after   ogre-next-deps-mingw

patch id        ogre-next-v3-0-mingw
packaged file   ogre-next-v3-0-mingw.patch
revision        4859016ad2cc956a653dfc4e4a859505696c6509
```

The script calls:

```text
Wizard.MacPatchProbe(patchId, sourceRoot)
Wizard.MacPatchApply(patchId, sourceRoot, explanation, confirmLabel)
```

Required behaviour, already proven for the Assimp patch:

```text
declined             tracked source stays pristine; configure does not start
accepted             the packaged diff is applied
already applied      no second mutation
wrong revision       unsupported_version; configure does not start
not a git checkout   unexpected_source; configure does not start
git missing          git_unavailable; configure does not start
apply then rebuild   a later configure/build uses the patched tree
repeated run         idempotent
```

The dependency patches are probed against the deps checkout, MinGW first and then `ogre-next-deps-apple`. The engine patch is probed against the OGRE-Next checkout. One patch must not be applied to the other tree. A clean checkout of each pin accepts its own patch and refuses the other. After a successful apply, `git apply --reverse --check` succeeds and a second apply does not. Declining the confirmation does not run `git apply`.

`ogre-next-deps-apple.patch` was generated from a clean checkout of `f7e23191` after `ogre-next-deps-mingw.patch` had been applied. It does not apply before that patch. With both applied, each patch still reverse-checks on its own. The dependency checkout under `~/Library/CodeBlocks` now matches that pair. The engine checkout's Metal and GL3Plus edits remain outside `ogre-next-v3-0-mingw.patch` and still reverse-check as already applied.

### 4.1 macOS decision, made in M1

`git apply --check` succeeded for both packaged MinGW files on the pinned checkouts, and both were applied. No unguarded hunk broke the Mac build. The Mac wizard applies both MinGW patches, then `ogre-next-deps-apple` on the dependency tree. The Windows wizard, when it is implemented later, applies the two MinGW patches and does not need the Apple patch.

The packaged MinGW patches were not sufficient to compile. The Remotery link is now `ogre-next-deps-apple.patch`, applied after `ogre-next-deps-mingw.patch`. It adds a second Apple block after `install_dep`, so it does not sit inside the MinGW hunk:

```text
ogre-next-deps src/Remotery/CMakeLists.txt
    After install_dep, link
    -framework Foundation -framework Metal -framework QuartzCore.
    RemoteryMetal.mm otherwise fails with undefined NSThread and objc_msgSend.
    The packaged ws2_32 hunk stays; it is unused on Mac.
```

Two engine edits are still only in the checkout. They are not part of either MinGW patch, and `MacCMakeOgreNext` does not apply them. A fresh engine checkout plus only the packaged patch still fails to compile until they are present. The checkout under `~/Library/CodeBlocks` already contains them:

```text
ogre-next RenderSystems/Metal/src/OgreMetalWindow.mm
    This pin is missing the brace that closes if( !externalWindowHandle )
    before else { mIsExternal = true; }. Clang reports "expected expression"
    at that else. One added closing brace. Not caused by the engine patch.

ogre-next RenderSystems/GL3Plus/CMakeLists.txt
    include/GL is currently inside if(NOT APPLE). The bundled
    glcorearb.h includes <KHR/khrplatform.h>, and that header is
    include/GL/KHR/khrplatform.h. Move the include/GL directory
    out of the Apple guard. Leave OPENGL_INCLUDE_DIR non-Apple.
```

Configure also needs two arguments the Windows recipe does not pass. Both are fixed values, not script-supplied free text:

```text
-DCMAKE_OSX_ARCHITECTURES=arm64
-DCMAKE_OSX_SYSROOT=<output of xcrun --show-sdk-path>
```

Without the architecture, this CMake writes `$(ARCHS_STANDARD_32_64_BIT)` and clang is invoked as `-arch -mmacosx-version-min=10.11`. Without the SDK path, CMake 4.4 on Xcode 26.5 stores the `xcodebuild` version banner as the sysroot, because `xcodebuild -n` is no longer accepted, and every compile rule then has an unmatched quote. `MacCMakeOgreNext` passes the architecture and the SDK path for both trees. It passes `CMAKE_INSTALL_PREFIX` and `OGRE_DEPENDENCIES_DIR` only when configuring the engine. The prefix is `~/Library/CodeBlocks/ogre-next-3.0.0`. `OGRE_DEPENDENCIES_DIR` is the `ogredeps` directory above.

---

## 5. Generated project

The starter is the proven light-show program. M1 kept that scene and changed only the Mac install contract. The copy used for the proof is `~/Library/CodeBlocks/ogre-next-m1-proof`. The file under `plans/thoughts/ogre_next_example/` was not edited.

```text
render system     "Metal Rendering Subsystem" first, then
                  "OpenGL 3+ Rendering Subsystem"
                  Metal ignores unknown "Video Mode" and "Full Screen".
                  "sRGB Gamma Conversion" = Yes is valid.
link              -F<SDK>/lib/Release
                  -framework Ogre
                  -framework OgreHlmsPbs
                  -framework OgreHlmsUnlit
                  There is no -lOgreMain and no lib/Release import library.
plugins.cfg       project-local. PluginFolder is empty.
                  PluginOptional=RenderSystem_Metal
                  PluginOptional=RenderSystem_GL3Plus
                  Plugin=Plugin_ParticleFX
                  On Mac, a bare name is loaded from
                  <app bundle>/Contents/Frameworks/<Name>.framework/<Name>.
                  PluginFolder is not prepended for that lookup.
resources2.cfg    project-local, one setting:
                  [Hlms]
                  DoNotUseAsResource=<SDK>/Media
                  The installed bin/resources2.cfg is not usable.
working directory the directory that contains plugins.cfg,
                  resources2.cfg, and ogrehead.mesh
app bundle        Contents/MacOS/<program>
                  Contents/Frameworks -> <SDK>/lib/Release
                  This matches the frameworks' @executable_path/../Frameworks
                  install names. A loose executable does not.
textures          <SDK>/Media/materials/textures
                  The Windows relative path ../../Media/materials/textures
                  only works when the working directory is two levels
                  below a Media directory. The generated project should
                  point at the SDK Media tree directly.
mesh              <SDK>/Media/models/ogrehead.mesh is present.
                  The proof used the copy shipped with the Windows example.
                  It loads as MeshSerializer_v1.100 and imports to a v2 mesh.
```

The proof linked only `~/Library/CodeBlocks/ogre-next-3.0.0`. It selected `Metal Rendering Subsystem` on Apple M2 Max, loaded `ogrehead.mesh`, compiled the light-show shaders, rendered 240 frames, printed `Light show rendered and closed cleanly.`, and exited 0. The frame count came from `OGRE_NEXT_M1_FRAMES` so the process could exit; the starter the wizard generates does not need that cap. Beginner-facing `main.cpp` does not convert models and does not build OGRE-Next.

The project is an ordinary Code::Blocks C++ project with Debug and Release targets. It is not a CMake project and not an OGRE-Next source build.

---

## 6. Milestones

| | Objective | Check | Status |
|---|---|---|---|
| M1 | Prove the pinned sources on Apple Silicon and freeze the installed-SDK contract. | Exact deps commit, SDL2 submodule hash, and engine commit build and install under `~/Library/CodeBlocks`. Each packaged patch is checked with `git apply --check`. An external program, based on `Tutorial00_Basic`, links only the engine install prefix and opens a rendered OGRE-Next window. The actual include, library, plugin, media, and working-directory layout is written down. | **DONE** |
| M2 | Acquire those checkouts from the wizard and apply the patches the M1 decision allows. | `macgit` clones the two pinned revisions and the recorded SDL2 submodule without installing Git. `macpatch` probes and applies `ogre-next-deps-mingw` and `ogre-next-v3-0-mingw` under the Assimp-patch contract. A declined patch leaves the checkout pristine. | **DONE** |
| M3 | Configure, build, and install both trees. | Deps install to the M1 dependency SDK. Engine configure passes that directory and the final `CMAKE_INSTALL_PREFIX` before the build. Install validation checks the M1 file list, not exit status alone. | **DONE** |
| M4 | Add the OGRE-Next wizard and starter project. | New Project shows an OGRE-Next entry distinct from OGRE. The existing OGRE wizard still builds. The generated ordinary C++ project builds and runs against the installed SDK and shows the light-show scene. | **NEXT** |
| M5 | Reuse a machine that already has these stages. | A second run skips a matching checkout, an already-applied patch, a finished build, and a validated SDK. It still generates a new project. No stage is cloned or rebuilt merely because the wizard was opened. | **PLANNED** |

Windows wizard automation is not one of these milestones. Section 2 is its specification for a later version.

---

## 7. Expected files

| Area | v3.9 role |
|---|---|
| New `src/plugins/scriptedwizard/resources/ogre-next/` | Wizard script, starter `main.cpp`, and macOS `plugins.cfg` / resource templates. Not a copy that replaces `resources/ogre/`. |
| `config.script` | A second project registration. The OGRE registration stays. |
| `macgit.cpp` / `.h` | Clone the two pinned revisions. No new general Git operation. |
| `macpatch.cpp` / `.h` | Add the two known patch ids and their revision guards. |
| `maccmake.cpp` / `.h` | `MacCMakeOgreNext` configures, builds, and installs `ogre-next-deps`, then `ogre-next`. The OGRE 1.x configure is unchanged. |
| `macbrew.cpp` / `.h` | `cmake` and `pkgconf`, plus a formula only if M1 required it. |
| `ogre-next-deps-mingw.patch` | Unchanged packaged dependency patch. |
| `ogre-next-deps-apple.patch` | Remotery framework link. Applies only after the MinGW dependency patch. |
| `ogre-next-v3-0-mingw.patch` | Unchanged packaged engine patch. |
| `plans/thoughts/ogre_next_example/` | Proven consumer. M1 may adapt paths; it does not replace the scene. |

---

## 8. Acceptance criteria

v3.9 is accepted only if:

1. The dependency checkout is `f7e23191f4fd89ed41134996bc47f9ee23d009df` and its SDL2 submodule is `5d249570393f7a37e037abf22cd6012a4cc56a71`.
2. The engine checkout is `4859016ad2cc956a653dfc4e4a859505696c6509` on branch `v3-0`.
3. The macOS wizard is a new entry. The OGRE 1.x wizard is unchanged and still works.
4. Git is found and never installed by Code::Blocks.
5. Source, build, and install directories are distinct and live under `~/Library/CodeBlocks`, resolved without a username literal.
6. The engine install prefix is fixed at configure time.
7. The generated project includes and links only that installed engine SDK.
8. Each patch the Mac wizard applies is one of the two packaged files, through `MacPatchProbe` / `MacPatchApply`, with the lifecycle in section 4.
9. A declined, wrong-revision, or non-git tree does not get configured.
10. An already-applied patch is not applied again.
11. A fresh generated project builds and opens an OGRE-Next window without hand-edited paths.
12. The starter shows the rotating illuminated Ogre head, or M1's recorded equivalent if the Mac render system changes only the renderer.
13. A second run skips completed clone, patch, configure, build, and install stages.
14. Nothing in this version retargets the Windows pin, the MinGW patches, or the Windows success criteria.

---

## 9. What stays rejected

- Replacing, renaming, or sharing starter code with the OGRE 1.x wizard.
- Treating OGRE-Next as OGRE 1.x, or the reverse.
- Cloning `master` or an unpinned `v3.0.0` tag in place of the commits in section 2.
- A script-supplied patch path, diff, or Git command.
- Applying the dependency patch to the engine tree, or the engine patch to the dependency tree.
- Hand-editing either tree when `git apply` fails.
- Installing Git, or installing Homebrew itself.
- Vendoring the dependency libraries into Code::Blocks.
- Installing the SDK into `/opt/homebrew` or into Code::Blocks' `Application Support/.../share/codeblocks` resources.
- Pointing the generated project at a source or build tree.
- Passing a different prefix to `cmake --install` than the one used at configure time.
- Putting conversion or SDK-build logic in beginner-facing `main.cpp`.
- Starting the Windows wizard implementation in this version.
- Changing either packaged patch while claiming the Windows run still covers it.

---

## 10. Stop conditions

Stop the macOS wizard if the pinned commits cannot be checked out, if a required patch fails `git apply --check` and the unpatched tree also fails to build, or if a rendered window requires the generated project to use the source or build tree.

Stop if the only Mac fix is a new patch written during implementation without a separate review. Section 4.1 already covers an unguarded hunk that applies and then breaks the Mac build.

Stop if `maccmake` would have to accept arbitrary CMake arguments from the script in order to pass `OGRE_DEPENDENCIES_DIR` or the install prefix.

---

## 11. Done when

**M3 done.** v3.9 is done when a user on Apple Silicon can choose the new OGRE-Next wizard, acquire the two pinned revisions, apply the two packaged patches plus the reviewed Mac edits from section 4.1, install the engine SDK under `~/Library/CodeBlocks`, and Build and Run an ordinary project that opens a rendered OGRE-Next window, without editing paths and without disturbing the OGRE 1.x wizard.

The Windows manual path is already done. It remains the pin, the patch set, and the picture of success for a later Windows wizard.
