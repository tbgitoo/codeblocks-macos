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

Current OGRE `master` still has the MinGW lines `GetModuleHandle("OgreMain.dll")` and `GetModuleHandle("OgreMain_d.dll")` in `Components/Bites/src/OgreWIN32ConfigDialog.cpp`. Current Assimp `master` still enables ccache with `if (CCACHE_PATH)` and no space check. Both candidates are still open.

---

## 2. Unit A — OGRE MinGW config dialog

**Upstream.** One commit. One file. Repository `https://github.com/OGRECave/ogre.git`.

```text
Components/Bites/src/OgreWIN32ConfigDialog.cpp
```

The MinGW, non-static branch is the only edit. It asks for the module that contains the dialog resource:

```text
GetModuleHandle("OgreMain_d.dll")  ->  GetModuleHandle("OgreBites_d.dll")
GetModuleHandle("OgreMain.dll")    ->  GetModuleHandle("OgreBites.dll")
```

That is the stored patch `ogre-14.6.3-win-config-dialog.patch`. The upstream commit is reconstructed on a clean upstream tree. The stored patch file is not rewritten and is not attached as the pull request.

### Why this is the whole fix

`Components/Bites/CMakeLists.txt` compiles `misc/OgreWin32Resources.rc` into the `OgreBites` target. That resource file holds `IDD_DLG_CONFIG`. `OgreMain.dll` has no resource section. `DialogBox` then fails with "The specified image file did not contain a resource section." The MSVC branch in the same constructor already resolves the right module: it calls `GetModuleHandleEx` on a static defined in this `.cpp`, which is linked into `OgreBites`. The MinGW branch names `OgreMain` instead. OGRE issue #1064 is the same failure.

The pull request says that, and nothing else. It does not disable the native dialog, does not change CMake, and does not move the resource into `OgreMain`.

### Left out of this commit

```text
the MSVC GetModuleHandleEx branch
OgreWin32Resources.rc and CMakeLists.txt
plugins.cfg, render-system order, and GLSL profiles
runtime DLL install rules
the Windows wizard and ogre-14.6.3-win.options
the Assimp sidecar patch and PR #3788
```

---

## 3. Unit B — Assimp ccache path with a space

**Upstream, after a quoting check.** One commit. One block. Repository `https://github.com/assimp/assimp.git`. This is not an OGRE pull request.

```text
CMakeLists.txt
```

The stored patch `assimp-5.4.3-ccache-path.patch` is the local make-it-work fix. It changes `if (CCACHE_PATH)` to `if (CCACHE_PATH AND NOT CCACHE_PATH MATCHES " ")`, so a ccache whose path contains a space is not used. The compile then runs the compiler directly. That keeps the wizard's Assimp 5.4.3 build working. It is not the upstream change.

The root cause is the unquoted path. `find_program` returns a single path such as `C:/Program Files/.../ccache.exe`. The current call passes that value unquoted:

```cmake
set_property(GLOBAL PROPERTY RULE_LAUNCH_COMPILE ${CCACHE_PATH})
set_property(GLOBAL PROPERTY RULE_LAUNCH_LINK ${CCACHE_PATH})
```

CMake splits an unquoted argument on whitespace, so the property does not receive the whole path. The MinGW makefile then starts `mingw32-make` with a broken launch command and the first source file fails. The candidate fix keeps ccache enabled and quotes the path:

```cmake
if(ASSIMP_BUILD_USE_CCACHE)
  find_program(CCACHE_PATH ccache)
  if(CCACHE_PATH)
    set_property(GLOBAL PROPERTY RULE_LAUNCH_COMPILE "${CCACHE_PATH}")
    set_property(GLOBAL PROPERTY RULE_LAUNCH_LINK "${CCACHE_PATH}")
  endif()
endif()
```

`ASSIMP_BUILD_USE_CCACHE` stays default ON. A path with no space must still launch ccache. A path with a space must launch that same ccache as one command.

### Check before the request is opened

Quoting the CMake argument is not yet shown to survive into the generated makefile. Open the request only after this check on a clean Assimp master tree, with the MinGW Makefiles generator and a `CCACHE_PATH` that contains a space:

1. Configure with the quoted `set_property` lines and no other edit.
2. Read the generated compile recipe. The ccache path must appear as one program, quotes included if the generator needs them.
3. Build one source file. `mingw32-make` must invoke that ccache and compile the file.
4. Repeat with a ccache path that has no space. That build must still use ccache.

If the generated recipe splits the path, or the quotes become part of the program name, the quoted form is not submitted. The space-skip patch is not submitted in its place. The wizard keeps `assimp-5.4.3-ccache-path.patch` for the pinned 5.4.3 archive. Do not retarget the wizard at Assimp 6 in this version.

Current master still has the unquoted `if (CCACHE_PATH)` block. If that block has already been replaced when the check is run, Unit B is not forced into a pull request.

### Left out of this commit

```text
the space-skip condition from assimp-5.4.3-ccache-path.patch
-DASSIMP_BUILD_USE_CCACHE=OFF as a substitute
any change to pugixml, windres, or zlib
OGRE's Assimp finder or Codec_Assimp
the sidecar resource-name patch
```

---

## 4. Already upstream — do not send again

The sidecar patch is the v3.8.1 correction. It is already:

```text
OGRE PR #3788
Assimp: resolve referenced files relative to the model source
```

v4.2 applies the stored file `ogre-14.6.3-assimp-sidecar-path.patch` unchanged, including its `if (APPLE)` hunk. v4.2.1 does not open a second pull request, does not split that patch again, and does not edit the stored file. When upstream lands the correction and a later pin adopts that release, the local offer is removed under the v3.8.1 retirement rule. That adoption is not this version.

---

## 5. Not upstream

These are local wizard choices. Each one solved a Windows run. None of them is a one-file defect in upstream source, and none of them is added to Unit A or Unit B.

| Local choice | Why it stays here |
|---|---|
| `ogre-14.6.3-win.options` | CMake `-D` lines: dependencies off, samples and tools on, Qt5 and Qt6 find disabled. OGRE's defaults are not wrong. Dependencies are off because this MinGW CMake cannot complete those HTTPS downloads. Qt is disabled because an MSYS2 `pkgconf` prefix also exposes Qt headers that this GCC does not compile. |
| Copy `SDL2.dll`, `libfreetype.dll`, `libassimp-5.dll`, and the selected MinGW runtime into the OGRE `bin` and beside `ogre.exe` | Windows records a DLL name, not a search directory. `OGRE_INSTALL_DEPENDENCIES` only sees the unused dependency bundle. Another `libstdc++-6.dll` on `PATH` is the user's machine. Copying the known runtime next to the executable is deployment, not an OGRE source change. |
| Comment `RenderSystem_GL` and `RenderSystem_GLES2` in the installed `plugins.cfg` | The legacy plugin's GLSL stops at `glsl120`, so Atomic Counters (`syntax glsl400`) reports "not supported" when that plugin is the saved renderer. Upstream ships both plugins and lets the setup dialog choose. Commenting them out removes a working renderer. Reordering the stock plugin list is a product default, not the bug that was fixed. |
| Static pugixml | The shared pugixml target compiles `scripts/pugixml_dll.rc`, and `windres` launches `gcc` unquoted when the compiler is under `Program Files`. The wizard builds pugixml static instead. No pugixml patch was written. This version does not invent one. |
| Windows wizard script, project working directory, opening `main.cpp`, patch consent, and skip-if-present | That is the v4.2 Code::Blocks feature. Upstream Code::Blocks still has its own Ogre wizard. Importing this wizard is a feature port, not a minimal defect fix, and it is outside v4.2.1. |
| `README.windows.md`, the NSIS installer, and these plans | They document this tree. They are not OGRE or Assimp changes. |

A pull request that mixes any row of this table into Unit A or Unit B is out of scope and is not opened.

---

## 6. How a request is prepared

Same rule as v3.8.1: the production delta is taken from a clean upstream checkout, not from the wizard's already-patched tree.

```text
Unit A
    clone OGRE master
    confirm the MinGW branch still names OgreMain.dll
    apply only the two GetModuleHandle renames
    one commit, one file

Unit B
    clone Assimp master
    confirm the RULE_LAUNCH_* lines are still unquoted
    try the quoted "${CCACHE_PATH}" form only
    read the MinGW makefile recipe and compile one file
    open the request only if a path with a space launches ccache
```

If either confirmation fails because upstream has already changed that line, that unit is not submitted. The local patch for `v14.6.3` or Assimp 5.4.3 stays until the wizard's pin moves.

The two requests are opened separately. Neither description mentions the Code::Blocks wizard except as the place the failure was observed.

---

## 7. Milestones

| | Objective | Check | Status |
|---|---|---|---|
| M1 | Record the cut: two upstream units, one already-open request, and the local choices that stay here. | This plan names the file and the hunk for each unit, and section 5 names what is refused. | This document. |
| M2 | Open the OGRE config-dialog request as Unit A. | The diff is only the two `GetModuleHandle` renames. Current master still has the old names at submission time. | Not opened. |
| M3 | Check the quoted `RULE_LAUNCH_*` form, then open Unit B only if a ccache path with a space survives into the MinGW makefile and compiles one file. | The diff quotes `"${CCACHE_PATH}"`. It does not add the space-skip. It is not an OGRE request and does not turn ccache off. | Not checked. |
| M4 | Leave PR #3788 as the sidecar request. | No second sidecar pull request and no edit to the stored patch. | Holds. |

Upstream review after M2 and M3 is external, as with PR #3788. It does not change the v4.2 wizard.

---

## 8. Acceptance

1. v4.2 remains the Windows wizard and is not resubmitted as a Code::Blocks feature.
2. Unit A touches only `OgreWIN32ConfigDialog.cpp` and only the MinGW DLL names.
3. Unit B touches only the Assimp `RULE_LAUNCH_COMPILE` and `RULE_LAUNCH_LINK` lines, and only by quoting `"${CCACHE_PATH}"`, after the makefile check in section 3 passes.
4. Unit A and Unit B are different repositories and different requests.
5. The stored sidecar patch and PR #3788 are unchanged.
6. The options file, DLL copies, plugin-list edits, static pugixml, and the wizard script are absent from both requests.
7. A unit whose line has already changed on current upstream is not forced into a pull request.
8. The local patches remain the wizard's solution for the pinned versions until those pins are deliberately moved.

---

## 9. Retirement

When an adopted OGRE release contains the `OgreBites` module lookup, remove `ogre-14.6.3-win-config-dialog` from the known-patch table and stop offering it. When an adopted Assimp release launches ccache for a path that contains a space, remove `assimp-5.4.3-ccache-path`. The space-skip stays the wizard's patch until that release is pinned. The sidecar patch retires only under the v3.8.1 rule. The wizard's DLL copy and `plugins.cfg` edit remain local for the Windows layout even after those source fixes land.
