# Code::Blocks v4.2.1 — upstream only the proven one-file fixes

**Status: Planning.** v4.2 is done and stays the Windows wizard. This version decides which of its workarounds are defects in upstream projects, and it cuts those defects into separate pull requests. It does not upstream the wizard.

v3.8.1 is the pattern. The local pin keeps working through the packaged patch. An upstream pull request is a separate track. Acceptance of that request is not required for the local wizard, and a request is opened only for a change that is still present on current upstream and that can be defended as one commit in one repository.

---

## 1. What v4.2 already proved

The Windows wizard builds OGRE `v14.6.3` and the generated project and SampleBrowser run. Along the way it applied two source patches and several deployment choices:

```text
source patches
    ogre-14.6.3-assimp-sidecar          already upstream as OGRE PR #3788
    ogre-14.6.3-win-config-dialog       MinGW dialog looks in the wrong DLL
    assimp-5.4.3-ccache-path            local skip when the ccache path contains a space

deployment, not source
    ogre-14.6.3-win.options             CMake -D choices for this wizard
    copy runtime DLLs beside the exe    Windows has no install RPATH
    comment legacy GL in plugins.cfg    local default so glsl400 samples load
    static pugixml                      avoids an unquoted windres command
```

Only the second and third lines are new upstream candidates. Each one is a separate repository and a separate pull request. They are not combined, and neither request includes the wizard, the options file, or the DLL copy.

The OGRE config-dialog request has been submitted. It does not rename the DLL. It removes the MinGW branch in `Components/Bites/src/OgreWIN32ConfigDialog.cpp` and uses `GetModuleHandleEx` on a static in that file. The maintainer accepted the root cause. The same MinGW lookup in `OgreWin32Window.cpp` and `OgreWin32GLSupport.cpp` is a separate OGRE request, and it is not started until the Assimp ccache check below is finished. Current Assimp `master` still enables ccache with `if (CCACHE_PATH)` and no space check.

---

## 2. Unit A — OGRE MinGW config dialog

**Submitted.** One commit. One file. Repository `https://github.com/OGRECave/ogre.git`.

```text
Components/Bites/src/OgreWIN32ConfigDialog.cpp
```

The opened request deletes the MinGW branch. Every compiler uses the address lookup, with the initializer GCC 15 requires:

```cpp
static const TCHAR staticVar = 0;
GetModuleHandleEx(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT, &staticVar, &mImpl->mHInstance);
```

The stored patch `ogre-14.6.3-win-config-dialog.patch` is still the two-name rename, `OgreMain.dll` to `OgreBites.dll`. That file stays the wizard's patch for the pinned `v14.6.3` tree. It is not the pull-request diff and it is not rewritten.

### Why this is the whole fix

`Components/Bites/CMakeLists.txt` compiles `misc/OgreWin32Resources.rc` into the `OgreBites` target. That resource file holds `IDD_DLG_CONFIG`. `OgreMain.dll` has no resource section. `DialogBox` then fails with "The specified image file did not contain a resource section." The MSVC branch in the same constructor already resolves the right module: it calls `GetModuleHandleEx` on a static defined in this `.cpp`, which is linked into `OgreBites`. The MinGW branch names `OgreMain` instead. OGRE issue #1064 is the same failure.

The pull request says that, and nothing else. It does not disable the native dialog, does not change CMake, and does not move the resource into `OgreMain`.

### Left out of this commit

```text
OgreWin32Window.cpp and OgreWin32GLSupport.cpp    Unit C, after Unit B
OgreD3D9Plugin.cpp                                 the hard-coded name still matches
OgreWin32Resources.rc and CMakeLists.txt
plugins.cfg, render-system order, and GLSL profiles
runtime DLL install rules
the Windows wizard and ogre-14.6.3-win.options
the Assimp sidecar patch and PR #3788
```

---

## 3. Unit B — Assimp ccache path with a space

**Upstream, after the experiment below.** One commit. One block. Repository `https://github.com/assimp/assimp.git`. This is not an OGRE pull request. The request is not opened until one stage below produces a launcher that runs.

```text
CMakeLists.txt
```

The stored patch `assimp-5.4.3-ccache-path.patch` is the local make-it-work fix. It changes `if (CCACHE_PATH)` to `if (CCACHE_PATH AND NOT CCACHE_PATH MATCHES " ")`, so a ccache whose path contains a space is not used. The compile then runs the compiler directly. That keeps the wizard's Assimp 5.4.3 build working. It is not the upstream change, and it is not rewritten.

Current master still does:

```cmake
find_program(CCACHE_PATH ccache)
if(CCACHE_PATH)
  set_property(GLOBAL PROPERTY RULE_LAUNCH_COMPILE ${CCACHE_PATH})
  set_property(GLOBAL PROPERTY RULE_LAUNCH_LINK ${CCACHE_PATH})
endif()
```

`find_program` returns one path. An unquoted `${CCACHE_PATH}` can be split on whitespace before the property is set. Quoting `"${CCACHE_PATH}"` in the CMake file is a separate question from quoting the command MinGW Makefiles writes into the recipe. The CMake quotes can make the expansion one CMake argument and still leave the generated launcher unquoted. Neither quoted form is the upstream fix until the recipe and a compile both say so.

`ASSIMP_BUILD_USE_CCACHE` stays default ON. The experiment does not turn ccache off and does not add the space-skip.

### Experiment, before any request

Clean current Assimp master. The primary reproducer is the Code::Blocks 25.03 MinGW that exposed the defect, not MSYS2. Windows, MinGW Makefiles, Release. Each stage uses a fresh build directory.

Select that toolchain exactly:

```bat
set "CB_MINGW=D:\Program Files\CodeBlocks\MinGW"
set "PATH=%CB_MINGW%\bin;%PATH%"
set "CC=%CB_MINGW%\bin\gcc.exe"
set "CXX=%CB_MINGW%\bin\g++.exe"
```

Then record `where gcc`, `where g++`, `where mingw32-make`, `where windres`, `where ccache`, `gcc --version`, `gcc -dumpmachine`, and `ccache --version`. `where ccache` is the check that matters. Assimp sets `CCACHE_PATH` with `find_program(CCACHE_PATH ccache)`, so the defect is the full path that search returns.

If that `where` prints `D:\Program Files\CodeBlocks\MinGW\bin\ccache.exe`, use it. That path already contains a space. Do not copy ccache to an artificial directory such as `D:\Program Files\ccache-test\ccache.exe` unless this installation has no `ccache.exe` under a path that contains a space.

For each stage, confirm the `CCACHE_PATH` CMake found, read the generated MinGW makefile recipe, and build with `cmake --build <build-dir> --target assimp --parallel 1 --verbose`, or the smallest current target name if `assimp` is not the target. Record the generated launcher command, whether ccache is invoked, whether the command is split at the space, and the exact result, including the first compile failure when the build fails. Do not treat `"${CCACHE_PATH}"` as the fix. The recipe and this build decide which form, if any, is submitted.

**A. Baseline.** Current upstream, no edit:

```cmake
set_property(GLOBAL PROPERTY RULE_LAUNCH_COMPILE ${CCACHE_PATH})
set_property(GLOBAL PROPERTY RULE_LAUNCH_LINK ${CCACHE_PATH})
```

This stage records the failure. It is not a candidate.

**B. CMake-argument quoting.** The only edit is:

```cmake
set_property(GLOBAL PROPERTY RULE_LAUNCH_COMPILE "${CCACHE_PATH}")
set_property(GLOBAL PROPERTY RULE_LAUNCH_LINK "${CCACHE_PATH}")
```

Do not treat this as sufficient because the CMake argument is one token. If the recipe and the build both launch that ccache as one program, B is the candidate and C is not run. If the launcher is the same broken command as A, B is ineffective and is not submitted.

**C. Literal launcher quoting.** Run only when B fails. The only edit is:

```cmake
set_property(GLOBAL PROPERTY RULE_LAUNCH_COMPILE "\"${CCACHE_PATH}\"")
set_property(GLOBAL PROPERTY RULE_LAUNCH_LINK "\"${CCACHE_PATH}\"")
```

The escaped quotes are there so the generated recipe can contain a quoted program path. If that recipe launches the ccache as one program and the file compiles, C is the candidate. If the quotes become part of the program name, or the path is still split, C is rejected.

The request is opened only for the one form that passes on the Code::Blocks MinGW path. The commit touches only those two `set_property` lines. The upstream aim is that ccache still runs when that path contains a space. After that form passes, repeat it once as a no-space control with MSYS2 MinGW-w64, whose normal prefix `D:\msys64\mingw64` has no space. That control is secondary. MSYS2 does not replace the Code::Blocks run. If neither B nor C passes, no Assimp request is opened. The space-skip patch is not submitted in their place. The wizard keeps `assimp-5.4.3-ccache-path.patch` for the pinned 5.4.3 archive. Do not retarget the wizard at Assimp 6 in this version.

If the `RULE_LAUNCH_*` lines have already been replaced when stage A is configured, Unit B is not forced into a pull request.

### Left out of this commit

```text
the space-skip condition from assimp-5.4.3-ccache-path.patch
-DASSIMP_BUILD_USE_CCACHE=OFF as a substitute
any change to pugixml, windres, or zlib
OGRE's Assimp finder or Codec_Assimp
the sidecar resource-name patch
```

---

## 4. Unit C — GLSupport module handle

**Upstream, after Unit B.** One commit. Two files. The same repository as Unit A, and a different pull request. Do not start it while the Assimp ccache check is unfinished. Do not add it to the open config-dialog request.

```text
RenderSystems/GLSupport/src/win32/OgreWin32Window.cpp
RenderSystems/GLSupport/src/win32/OgreWin32GLSupport.cpp
```

`OgreGLSupport` is a static library. `RenderSystem_GL`, `RenderSystem_GL3Plus`, and `RenderSystem_GLES2` each link it. The MinGW lines still name one DLL:

```text
OgreWin32Window.cpp       GetModuleHandle("GLSupport.dll")
OgreWin32GLSupport.cpp    GetModuleHandle("RenderSystem_GL3Plus.dll")
```

There is no `GLSupport.dll`, so the window-class handle is null. `RenderSystem_GL3Plus.dll` is the right image only for a default-named GL3Plus plugin. The other two plugins contain the same `dummyWndProc` and the same `"OgreGLWindow"` class. The commit removes both MinGW branches and uses the Unit A call, including `static const TCHAR staticVar = 0`. The address is inside whichever of those three DLLs was linked, and inside the executable when the library is static.

This is the follow-up that was described to the maintainer. It waits until Unit B is either opened or closed without a request.

### Left out of this commit

```text
OgreD3D9Plugin.cpp
    RenderSystem_Direct3D9.dll still matches the plugin target.
    A rename would be safer with the same call. That is not this request.
OgreWIN32ConfigDialog.cpp          already Unit A
OgreWin32EGLWindow.cpp             already address-based; Win32 does not build it by default
OgreD3D11RenderWindow.cpp          already address-based
OgreVulkanWindow.cpp               already address-based
the config-dialog resource, CMake, and the wizard
```

---

## 5. Already upstream — do not send again

The sidecar patch is the v3.8.1 correction. It is already:

```text
OGRE PR #3788
Assimp: resolve referenced files relative to the model source
```

v4.2 applies the stored file `ogre-14.6.3-assimp-sidecar-path.patch` unchanged, including its `if (APPLE)` hunk. v4.2.1 does not open a second pull request, does not split that patch again, and does not edit the stored file. When upstream lands the correction and a later pin adopts that release, the local offer is removed under the v3.8.1 retirement rule. That adoption is not this version.

---

## 6. Not upstream

These are local wizard choices. Each one solved a Windows run. None of them is a one-file defect in upstream source, and none of them is added to Unit A, Unit B, or Unit C.

| Local choice | Why it stays here |
|---|---|
| `ogre-14.6.3-win.options` | CMake `-D` lines: dependencies off, samples and tools on, Qt5 and Qt6 find disabled. OGRE's defaults are not wrong. Dependencies are off because this MinGW CMake cannot complete those HTTPS downloads. Qt is disabled because an MSYS2 `pkgconf` prefix also exposes Qt headers that this GCC does not compile. |
| Copy `SDL2.dll`, `libfreetype.dll`, `libassimp-5.dll`, and the selected MinGW runtime into the OGRE `bin` and beside `ogre.exe` | Windows records a DLL name, not a search directory. `OGRE_INSTALL_DEPENDENCIES` only sees the unused dependency bundle. Another `libstdc++-6.dll` on `PATH` is the user's machine. Copying the known runtime next to the executable is deployment, not an OGRE source change. |
| Comment `RenderSystem_GL` and `RenderSystem_GLES2` in the installed `plugins.cfg` | The legacy plugin's GLSL stops at `glsl120`, so Atomic Counters (`syntax glsl400`) reports "not supported" when that plugin is the saved renderer. Upstream ships both plugins and lets the setup dialog choose. Commenting them out removes a working renderer. Reordering the stock plugin list is a product default, not the bug that was fixed. |
| Static pugixml | The shared pugixml target compiles `scripts/pugixml_dll.rc`, and `windres` launches `gcc` unquoted when the compiler is under `Program Files`. The wizard builds pugixml static instead. No pugixml patch was written. This version does not invent one. |
| Windows wizard script, project working directory, opening `main.cpp`, patch consent, and skip-if-present | That is the v4.2 Code::Blocks feature. Upstream Code::Blocks still has its own Ogre wizard. Importing this wizard is a feature port, not a minimal defect fix, and it is outside v4.2.1. |
| `README.windows.md`, the NSIS installer, and these plans | They document this tree. They are not OGRE or Assimp changes. |

A pull request that mixes any row of this table into Unit A, Unit B, or Unit C is out of scope and is not opened.

---

## 7. How a request is prepared

Same rule as v3.8.1: the production delta is taken from a clean upstream checkout, not from the wizard's already-patched tree.

```text
Unit A
    opened
    one file, the address lookup, initializer included
    do not add the GLSupport files to this request

Unit B
    clone Assimp master
    confirm the RULE_LAUNCH_* lines are still unquoted
    select D:\Program Files\CodeBlocks\MinGW and record where ccache
    stage A records that CCACHE_PATH, the recipe, and the first failure
    stage B tries "${CCACHE_PATH}" only
    stage C tries "\"${CCACHE_PATH}\"" only if B produces the same broken launcher
    open the request only for the form whose recipe launches that ccache as one program
    then repeat that form under D:\msys64\mingw64, a path with no space

Unit C
    start only after Unit B is opened, or closed because the check failed
    clone OGRE master
    confirm both GLSupport MinGW branches still name those DLLs
    replace each branch with the Unit A call, initializer included
    one commit, two files
    leave OgreD3D9Plugin.cpp unchanged
```

If a confirmation fails because upstream has already changed that line, that unit is not submitted. The local patch for `v14.6.3` or Assimp 5.4.3 stays until the wizard's pin moves. Unit C has no local patch.

The requests stay separate. Unit C is not prepared in parallel with Unit B. No description mentions the Code::Blocks wizard except as the place the failure was observed.

---

## 8. Milestones

| | Objective | Check | Status |
|---|---|---|---|
| M1 | Record the cut: the open dialog request, the Assimp check, the later GLSupport request, and the local choices that stay here. | This plan names the file and the hunk for each unit, and section 6 names what is refused. | This document. |
| M2 | Open the OGRE config-dialog request as Unit A. | The diff is `OgreWIN32ConfigDialog.cpp` only: the MinGW branch is gone and the address lookup has `static const TCHAR staticVar = 0`. The stored rename patch is not the pull request. | Submitted. External. |
| M3 | Run the Unit B stages on a ccache path that contains a space. Open a request only for the form the recipe and one verbose compile both accept. | Stage A is the baseline. Stage B is `"${CCACHE_PATH}"`. Stage C is `"\"${CCACHE_PATH}\""` and runs only if B matches A. The diff is those two lines. It does not add the space-skip, it is not an OGRE request, and it does not turn ccache off. | Next. Not run. |
| M4 | Leave PR #3788 as the sidecar request. | No second sidecar pull request and no edit to the stored patch. | Holds. |
| M5 | After M3 is opened or closed, open the GLSupport request as Unit C. | The diff is `OgreWin32Window.cpp` and `OgreWin32GLSupport.cpp` only, using the Unit A call. `OgreD3D9Plugin.cpp` is absent. It is not pushed into the Unit A request. | Waiting on M3. |

Upstream review after M2, M3, and M5 is external, as with PR #3788. It does not change the v4.2 wizard.

---

## 9. Acceptance

1. v4.2 remains the Windows wizard and is not resubmitted as a Code::Blocks feature.
2. Unit A touches only `OgreWIN32ConfigDialog.cpp`. The MinGW branch is removed. The call is `GetModuleHandleEx` with `static const TCHAR staticVar = 0`.
3. Unit B touches only the Assimp `RULE_LAUNCH_COMPILE` and `RULE_LAUNCH_LINK` lines, and only with the one form from section 3 that a spaced ccache path survives into the MinGW recipe and a verbose single-job compile.
4. Unit C touches only `OgreWin32Window.cpp` and `OgreWin32GLSupport.cpp`, uses that same call, and is opened only after Unit B is settled. `OgreD3D9Plugin.cpp` is not in it.
5. Unit A, Unit B, and Unit C are different requests. Unit A and Unit C are the same repository and are not combined.
6. The stored sidecar patch and PR #3788 are unchanged. The stored dialog patch remains the two-name rename.
7. The options file, DLL copies, plugin-list edits, static pugixml, and the wizard script are absent from all three requests.
8. A unit whose line has already changed on current upstream is not forced into a pull request.
9. The local patches remain the wizard's solution for the pinned versions until those pins are deliberately moved.

---

## 10. Retirement

When an adopted OGRE release contains the `OgreBites` module lookup, remove `ogre-14.6.3-win-config-dialog` from the known-patch table and stop offering it. When an adopted Assimp release launches ccache for a path that contains a space, remove `assimp-5.4.3-ccache-path`. The space-skip stays the wizard's patch until that release is pinned. The sidecar patch retires only under the v3.8.1 rule. The wizard's DLL copy and `plugins.cfg` edit remain local for the Windows layout even after those source fixes land.
