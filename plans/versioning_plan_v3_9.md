# Code::Blocks v3.9 - OGRE-Next wizard, macOS first

**Status: PLANNING.** The Windows manual install in `plans/thoughts/ogre-next-windows-codeblocks-mingw-student-guide.md` has been run from start to finish. It produced an installed OGRE-Next SDK and a window titled `OGRE-Next Light Show` with a rotating, illuminated Ogre head. No wizard work has started. The Windows machine is not available, so v3.9 implements the wizard on macOS first. The existing OGRE 1.x wizard is left unchanged.

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
Wizard.MacCMakeProbe / MacCMakePaths / MacCMakeConfigure
Wizard.MacPatchProbe / MacPatchApply
```

`MacCMakeConfigure` today configures one OGRE 1.x tree. v3.9 needs a second, still constrained, use of that configure/build/install sequence for the dependency tree, plus `OGRE_DEPENDENCIES_DIR` when configuring the engine. The script still cannot pass arbitrary CMake arguments.

Paths are resolved in native code with `wxGetHomeDir()`, as in v3.8. No username and no `D:\` literal in the macOS workflow.

Proposed layout, confirmed or adjusted by M1:

```text
~/Library/CodeBlocks/
    ogre-next-deps-source-3.0.0/
        build-codeblocks/
            ogredeps/                  dependency SDK, if install matches Windows
    ogre-next-source-3.0.0/
        build-codeblocks/
    ogre-next-3.0.0/                   CMAKE_INSTALL_PREFIX for the engine
```

`CMAKE_INSTALL_PREFIX` for OGRE-Next is passed at configure time. A later `cmake --install --prefix` is not the contract. v3.8 showed that swapping the prefix at install time leaves metadata pointed at the build tree.

The dependency project installed into `<build>/ogredeps` on Windows. M1 checks whether the Mac install does the same. The engine configure receives that directory as `OGRE_DEPENDENCIES_DIR`. The generated project never receives it. The generated project receives only `~/Library/CodeBlocks/ogre-next-3.0.0`.

Homebrew is limited to tools the Mac configure actually requires. The starting set is `cmake` and `pkgconf`. SDL2, FreeImage, and the other libraries built by `ogre-next-deps` are not Homebrew formulas unless M1 shows the pinned build looking for a system copy. Git is validated and never installed. Xcode's compiler is assumed, not installed by the wizard.

A second wizard run skips a stage whose checkout hash, patch state, build tree, or installed SDK already matches this pin.

---

## 4. Patch delivery

The two patches are applied the way `ogre-14.6.3-assimp-sidecar-path.patch` is applied.

That helper accepts only a known patch id. The script cannot supply a path, a diff, a target file, or a Git command. The helper checks that the checkout is the supported revision, runs `git apply` on the packaged file, and treats an already-applied patch as success.

Register two ids beside the existing Assimp id:

```text
patch id        ogre-next-deps-mingw
packaged file   ogre-next-deps-mingw.patch
revision        f7e23191f4fd89ed41134996bc47f9ee23d009df

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

The dependency patch is probed against the deps checkout. The engine patch is probed against the OGRE-Next checkout. One patch must not be applied to the other tree.

### 4.1 macOS decision, made in M1

The patch text was proven on MinGW. It is not yet proven on Apple Silicon. M1 runs `git apply --check` for each packaged file on its pinned checkout and then builds.

```text
--check fails
    Mac wizard does not offer that patch.
    Do not hand-edit the source to compensate.

--check succeeds and the Mac build of the patched tree succeeds
    Mac wizard applies that patch, so the tree matches the Windows pin.

--check succeeds but an unguarded hunk breaks the Mac build
    Stop. Do not publish a third patch in this plan until that failure
    is observed and a minimal replacement is reviewed.
```

The Windows wizard, when it is implemented later, always offers both patches. That path is already proven.

---

## 5. Generated project

The starter is the proven light-show program, adapted only where M1 shows the macOS install differs from `install/bin/Release` and the Windows DLL layout. Likely differences, to be recorded rather than guessed:

```text
render system     Metal or GL3Plus, not Direct3D11
library directory lib/macosx/Release or the path M1 actually installs
plugins.cfg       project-local, pointed at the installed renderer
resources         project-local, pointed at <SDK>/Media
working directory the directory that contains plugins.cfg and resources2.cfg
```

`plans/thoughts/ogre_next_example/ogrehead.mesh` is the mesh the Windows program displayed. The generated project consumes that asset, or the same mesh from the installed Media tree if M1 shows it there. Beginner-facing `main.cpp` does not convert models and does not build OGRE-Next.

The project is an ordinary Code::Blocks C++ project with Debug and Release targets. It is not a CMake project and not an OGRE-Next source build.

---

## 6. Milestones

| | Objective | Check | Status |
|---|---|---|---|
| M1 | Prove the pinned sources on Apple Silicon and freeze the installed-SDK contract. | Exact deps commit, SDL2 submodule hash, and engine commit build and install under `~/Library/CodeBlocks`. Each packaged patch is checked with `git apply --check`. An external program, based on `Tutorial00_Basic`, links only the engine install prefix and opens a rendered OGRE-Next window. The actual include, library, plugin, media, and working-directory layout is written down. | **NEXT** |
| M2 | Acquire those checkouts from the wizard and apply the patches the M1 decision allows. | `macgit` clones the two pinned revisions and the recorded SDL2 submodule without installing Git. `macpatch` probes and applies `ogre-next-deps-mingw` and `ogre-next-v3-0-mingw` under the Assimp-patch contract. A declined patch leaves the checkout pristine. | **PLANNED** |
| M3 | Configure, build, and install both trees. | Deps install to the M1 dependency SDK. Engine configure passes that directory and the final `CMAKE_INSTALL_PREFIX` before the build. Install validation checks the M1 file list, not exit status alone. | **PLANNED** |
| M4 | Add the OGRE-Next wizard and starter project. | New Project shows an OGRE-Next entry distinct from OGRE. The existing OGRE wizard still builds. The generated ordinary C++ project builds and runs against the installed SDK and shows the light-show scene. | **PLANNED** |
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
| `maccmake.cpp` / `.h` | Second constrained configure/build/install for deps, then engine configure with `OGRE_DEPENDENCIES_DIR` and the install prefix. |
| `macbrew.cpp` / `.h` | `cmake` and `pkgconf`, plus a formula only if M1 required it. |
| `ogre-next-deps-mingw.patch` | Unchanged packaged dependency patch. |
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

**PLANNING.** v3.9 is done when a user on Apple Silicon can choose the new OGRE-Next wizard, acquire the two pinned revisions, apply exactly the patches M1 showed are valid on macOS, install the engine SDK under `~/Library/CodeBlocks`, and Build and Run an ordinary project that opens a rendered OGRE-Next window, without editing paths and without disturbing the OGRE 1.x wizard.

The Windows manual path is already done. It remains the pin, the patch set, and the picture of success for a later Windows wizard.
