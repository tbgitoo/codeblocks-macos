# Code::Blocks v4.2.1 — upstream status

**Status: Planning.** v4.2 stays the Windows wizard. The config-dialog fix is merged. The Assimp request is open. The GLSupport follow-up is still to prepare. Acceptance of a pull request is not required for the wizard. A request is one defect, one repository, one commit, taken from clean current upstream. The wizard, its options file, and its DLL copies stay out of every request.

---

## 1. Merged — OGRE config dialog

[OGRE PR #3823](https://github.com/OGRECave/ogre/pull/3823) is merged to `master`. `DialogBox` was given `OgreMain.dll`, which has no resource section. `IDD_DLG_CONFIG` is compiled into `OgreBites`.

```text
Components/Bites/src/OgreWIN32ConfigDialog.cpp
```

The pinned `v14.6.3` patch `ogre-14.6.3-win-config-dialog` only renames the two hard-coded modules to `OgreBites.dll` and `OgreBites_d.dll`. That stored patch stays until the wizard pin moves to a release that contains the merge.

The pull request removes the MinGW branch and uses the address lookup for every compiler. MinGW GCC needs the initializer:

```cpp
static const TCHAR staticVar = 0;
GetModuleHandleEx(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT, &staticVar, &mImpl->mHInstance);
```

The GLSupport files in section 3 were not part of the merged request.

---

## 2. Submitted — Assimp ccache path with a space

[Assimp PR #7057](https://github.com/assimp/assimp/pull/7057). One file, `CMakeLists.txt`. `find_program` returns `D:/Program Files/CodeBlocks/MinGW/bin/ccache.exe`. `RULE_LAUNCH_COMPILE` and `RULE_LAUNCH_LINK` put that path in the MinGW recipe without quotes, so `cmd.exe` tries to run `D:/Program`.

| Form | Result |
|---|---|
| `${CCACHE_PATH}` | Fail. Launcher unquoted. `'D:/Program' is not recognized` at `adler32.c.obj`. |
| `"${CCACHE_PATH}"` | Fail. The generated launcher is still unquoted. Same error. |
| `"\"${CCACHE_PATH}\""` | Pass. The recipe quotes ccache. The `assimp` target compiled and linked, including `libassimp-6.dll`. |

The request is the third form. Copying that same Code::Blocks MinGW tree to `D:\CodeBlocks-MinGW` let pristine master build, so the space in the ccache path is the failure.

`assimp-5.4.3-ccache-path.patch` still skips ccache when the path contains a space. It stays the wizard's patch for pinned Assimp 5.4.3. It is not the pull request, and the wizard is not moved to Assimp 6 here.

---

## 3. Next — OGRE GLSupport module lookups

One follow-up pull request. Two files. Same repository as the config-dialog request, and a different request.

```text
RenderSystems/GLSupport/src/win32/OgreWin32Window.cpp
RenderSystems/GLSupport/src/win32/OgreWin32GLSupport.cpp
```

`OgreGLSupport` is a static library linked into `RenderSystem_GL.dll`, `RenderSystem_GL3Plus.dll`, and `RenderSystem_GLES2.dll`. The MinGW lines still name one DLL:

```text
OgreWin32Window.cpp       GetModuleHandle("GLSupport.dll")
OgreWin32GLSupport.cpp    GetModuleHandle("RenderSystem_GL3Plus.dll")
```

There is no `GLSupport.dll`. `RenderSystem_GL3Plus.dll` is the right image only for a default-named GL3Plus plugin. The same object code is linked into the other two plugins, so one filename cannot name the module in every case. `GetModuleHandleEx` with `GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS` returns the module that contains the code. Use `static const TCHAR staticVar = 0`, as in the config-dialog request.

Leave `RenderSystems/Direct3D9/src/OgreD3D9Plugin.cpp` out. `RenderSystem_Direct3D9.dll` still matches the target that contains that file. A later rename would be safer with the same call. That is not this defect.

Also leave the config-dialog file, the resource script, and CMake out. `OgreWin32EGLWindow.cpp`, `OgreD3D11RenderWindow.cpp`, and `OgreVulkanWindow.cpp` already use the address lookup.

---

## 4. Already upstream — sidecar paths

[OGRE PR #3788](https://github.com/OGRECave/ogre/pull/3788) resolves Assimp referenced files relative to the model source. Do not open a second request and do not edit `ogre-14.6.3-assimp-sidecar-path.patch`. The wizard keeps that stored patch, including its `if (APPLE)` hunk, until a pinned OGRE release contains the fix.

---

## 5. Local only

These solved a Windows run. They were not part of the merged config-dialog request, and they are not added to the open Assimp request or the GLSupport follow-up.

| Local choice | Why it stays here |
|---|---|
| `ogre-14.6.3-win.options` | CMake `-D` lines for this wizard. Dependencies stay off because this MinGW CMake cannot finish those HTTPS downloads. Qt find stays disabled because an MSYS2 `pkgconf` prefix exposes Qt headers this GCC does not compile. |
| Copy `SDL2.dll`, `libfreetype.dll`, `libassimp-5.dll`, and the selected MinGW runtime beside the executables | Windows records a DLL name, not a search directory. |
| Comment `RenderSystem_GL` and `RenderSystem_GLES2` in `plugins.cfg` | A local renderer default so `glsl400` samples load. Upstream still ships both plugins. |
| Static pugixml | Avoids an unquoted `windres` launch under `Program Files`. No pugixml patch was written. |
| The Windows wizard script | The v4.2 feature. Upstream Code::Blocks keeps its own Ogre wizard. |
| `README.windows.md`, the NSIS installer, and these plans | They document this tree. |

---

## 6. Status

```text
[done]  OGRE config-dialog pull request #3823 merged to master
[done]  Assimp ccache pull request #7057 submitted
[done]  OGRE pull request #3788 already open for sidecar paths
[next]  Prepare and test the OGRE GLSupport follow-up
[later] Retire a local patch only when its dependency pin moves to a release that contains the fix
```

Upstream review does not change the v4.2 wizard.

---

## 7. Retirement

Remove `ogre-14.6.3-win-config-dialog` when the pinned OGRE release contains the address-based lookup. Remove `assimp-5.4.3-ccache-path` when the pinned Assimp release runs ccache from a path that contains a space. Retire the sidecar patch when the pinned OGRE release contains PR #3788. The DLL copy and the `plugins.cfg` edit stay local for this Windows layout.
