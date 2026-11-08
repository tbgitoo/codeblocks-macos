# OGRE-Next on Windows with Code::Blocks MinGW

Investigation date: 2026-11-07.
Toolchain under test: `D:\Program Files\CodeBlocks\MinGW` (GCC 14.2.0, `x86_64-w64-mingw32`, ucrt posix seh).
Working root: `D:\Users\thoma\Documents\git\ogre-next-test`.

The existing Code::Blocks OGRE 1.x wizard was not modified. No wizard was implemented.

## A. Recommended revisions

These two commits, plus the two patch files in `patches\`, are the reproducible pair.

| Piece | Revision |
| --- | --- |
| OGRE-Next | branch `v3-0`, commit `4859016ad2cc956a653dfc4e4a859505696c6509` (2026-11-02). Version macros remain 3.0.0 “Eris”. |
| ogre-next-deps | `f7e23191f4fd89ed41134996bc47f9ee23d009df` (current `origin/master` on the day of the investigation). |
| SDL2 submodule `src/SDL2` | `5d249570393f7a37e037abf22cd6012a4cc56a71` (`release-2.32.10`). |
| shaderc third-party submodules | effcee `33d438fb1939e94e5507d38dee9d999f60a03d96`, glslang `3ee5f2f1d3316e228916788b300d786bb574d337`, googletest `a781fe29bcf73003559a3583167fe3d647518464`, re2 `ca11026a032ce2a3de4b3c389ee53d2bdc8794d6`, SPIRV-Headers `979924c8bc839e4cb1b69d03d48398551f369ce7`. shaderc itself is vendored, not a submodule. |
| FreeImage | Vendored, not a submodule. CMake project FreeImageRe 0.2. `FreeImage.h` reports 3.18.0. |
| LibJXR | Bundled inside that FreeImage tree at `src/FreeImage/Source/LibJXR`. Its README points at `https://jxrlib.codeplex.com/`. No separate submodule revision. |

Do not use OGRE-Next `master`. On the day of the investigation it was OGRE 4.0.0 unstable and far ahead of `v3-0`. Do not use the `v3.0.0` tag (`75643c3997f5b6d2aa1d7bd8400b9be6736d9908`) as the student target: `v3-0` is that release plus later 3.0-compatible fixes, including a GCC 15 STLAllocator fix and STBI codec fixes, and the version macros are still 3.0.0. There is no newer ogre-next-deps revision than `f7e23191`. `versions.txt` in that repository is stale and must not be used to identify FreeImage or SDL2.

Dependency patch: `patches\ogre-next-deps-mingw.patch`
Engine patch: `patches\ogre-next-v3-0-mingw.patch`

Both patches were checked with `git apply --reverse --check` against the trees that built.

## B. Root causes

| Failure | Class | What was actually wrong |
| --- | --- | --- |
| SDL2 install looked for `libSDL2.dll` | MinGW assumption | SDL2 2.32.10 built with this MinGW produces `SDL2.dll` and `libSDL2.dll.a`. Upstream ogre-next-deps still expects `libSDL2.dll`. No later deps commit fixes it. |
| Generated build used `D:\msys64\mingw64\bin\ar.exe` and `D:\msys64\ucrt64\bin\gcc.exe` | Environment contamination | Several GCC installs are on the machine. “gcc is on PATH” does not select the Code::Blocks compiler. |
| shaderc failed with `'D:/Program' is not recognized` | Path-quoting problem | `CMAKE_MAKE_PROGRAM` was the absolute path under `Program Files`. Nested `$(MAKE)` was split on the space. |
| FreeImage failed in `unknwnbase.h` on `__mingw_uuidof` | Source portability problem | LibJXR ships `common/include/guiddef.h`. `JXRGlue.h` includes `<guiddef.h>`, so the bundled header shadows the MinGW-w64 one. |
| Removing `Source/LibJXR/common/include` | Disproven | That directory also provides `wmspecstring.h`, which PluginJXR needs. The directory stays. Only `guiddef.h` is removed. |
| `CT_ASSERT(sizeof(UINTPTR_T) == sizeof(void*))` | Source portability problem | FreeImage’s CMake defined `__ANSI__` for every non-MSVC build. LibJXR then included both `x86.h` (pointer-width types) and `ansi.h` (32-bit `int` on Win64). |
| `_byteswap_ulong` conflicting types in `segdec.c` | MinGW assumption | Without `__MINGW64_TOOLCHAIN__`, LibJXR declares `U32 _byteswap_ulong(U32)`. MinGW-w64 `stdlib.h` already declares `unsigned long _byteswap_ulong(unsigned long)`. FreeImage’s own MinGW makefile defines this macro. It was not defined by the CMake build. |
| `ImathVec.h`: `int64_t` was not declared | Stale dependency | Bundled Imath 3.1.4 relies on a transitive include that GCC 14 no longer provides. |
| zziplib `plugin.c` incompatible function pointers | Source portability problem, GCC 14 | On Win64, `_zzip_size_t` is 64-bit while the CRT `_read` / `_write` count is `unsigned int`. GCC 14 rejects that pointer conversion. |
| Remotery `rsize_t` redefinition, then missing `send` / `WSA*` | Source portability problem | MinGW-w64 already typedefs `rsize_t` to `size_t`. Remotery then needed `ws2_32`, which MSVC pulls in without an explicit link and MinGW does not. |
| `D3D11_RLDO_IGNORE_INTERNAL` undeclared | MinGW assumption | This MinGW’s `d3d11sdklayers.h` has `D3D11_RLDO_SUMMARY` and `D3D11_RLDO_DETAIL` only. The Windows 10 SDK value of the missing enumerator is `0x4`. |
| Undefined `D3DCompile`, `D3DReflect`, `EnumProcessModules`, `GetModuleFileNameExA` | MinGW assumption | MinGW ignores MSVC `#pragma comment(lib)`. The Windows SDK branch of `FindDirectX11.cmake` did not list `d3dcompiler` or `psapi`. |
| OgreMain mass-failed opening headers (`Error 1455`, “Invalid argument”) | Environment, not a source bug | A 12-way compile exhausted memory. The same sources compiled with `--parallel 4`. |
| pkg-config found `D:/msys64/mingw64/lib/libfreetype.dll.a` for the Debug freetype slot | Environment contamination | The Release link line used `ogredeps`. It did not affect the installed SDK. |

JPEG XR was not disabled. After the build, the independent program’s log registered FreeImage 3.18.0 and listed `jxr`, `wdp`, and `hdp` among the supported formats.

## C. Final patch inventory

The future wizard should apply both patches. They are the whole compatibility delta. Do not edit generated Makefiles, ExternalProject scripts, or installed MinGW headers.

### ogre-next-deps `f7e23191` — `patches\ogre-next-deps-mingw.patch`

1. `src/CMakeLists.txt`
   MinGW `SDL2_BIN_NAME` changes from `libSDL2.dll` to `SDL2.dll`.
   Reason: that is the DLL this toolchain produces.
   Upstream fix: none on current `origin/master`.
   Wizard: apply.

2. `src/FreeImage/CMakeLists.txt`
   Define `__ANSI__` only when the target is not MSVC and not WIN32. Define `__MINGW64_TOOLCHAIN__` when `MINGW`.
   Reason: stops the dual platform-header path, and selects the CRT `_byteswap_ulong` instead of LibJXR’s conflicting declaration. This matches FreeImage’s MinGW makefile and the vcpkg jxrlib change that dropped a forced `__ANSI__`.
   Upstream fix: not in this FreeImage snapshot. The idea already exists in FreeImage’s makefiles and in downstream jxrlib packaging.
   Wizard: apply.

3. `src/FreeImage/Source/LibJXR/common/include/guiddef.h`
   Delete the file. Leave the rest of `common/include` in place.
   Reason: `#include <guiddef.h>` must resolve to the MinGW-w64 header. MSYS2, the AUR mingw-w64-freeimage recipe, and vcpkg all remove or rename this bundled header. A rename to `guiddef.h.disabled` was only a diagnostic; the patch deletes the file. The original content is kept outside the repo at `_investigation\guiddef.h.original`.
   Wizard: apply the deletion.

4. `src/FreeImage/Source/OpenEXR/Imath/ImathVec.h`
   Add `#include <cstdint>`.
   Reason: GCC 14 does not provide `int64_t` transitively. One missing include accounted for the whole Imath error cascade.
   Upstream fix: later Imath already includes it. This tree does not.
   Wizard: apply.

5. `src/zziplib/zzip/plugin.c`
   On `__MINGW32__`, call `_read` and `_write` through small wrappers and store those pointers in `default_io`.
   Reason: GCC 14 rejects the 64-bit size versus `unsigned int` function-pointer mismatch.
   Upstream fix: not in this snapshot.
   Wizard: apply. The wrappers also clamp a length above 4 GiB. `_zzip_lseek` was left unchanged; zip members above 2 GiB were already outside this zziplib Win32 path.

6. `src/Remotery/src/Remotery.c` and `src/Remotery/CMakeLists.txt`
   Do not typedef `rsize_t` on MinGW. Link `ws2_32` on WIN32.
   Reason: the MinGW typedef conflicts with `corecrt.h`, and the socket API is otherwise unresolved.
   Remotery is optional (`OGREDEPS_BUILD_REMOTERY`, default ON) and is not used by the window test. It is part of the default dependency build, so it is fixed rather than switched off.
   Wizard: apply.

### OGRE-Next `4859016` — `patches\ogre-next-v3-0-mingw.patch`

1. `RenderSystems/Direct3D11/src/OgreD3D11Device.cpp`
   If `D3D11_RLDO_IGNORE_INTERNAL` is missing, define it as `(D3D11_RLDO_FLAGS)0x4`.
   Reason: this MinGW header is behind the Windows 10 SDK header that OGRE expects.
   Wizard: apply. A later MinGW that already defines the enumerator keeps its own value because of `#ifndef`.

2. `CMake/Packages/FindDirectX11.cmake`
   Add `d3dcompiler.lib` on the Windows SDK branch. Append `psapi.lib` when `MINGW`.
   Reason: HLSL compile/reflect and the Nsight process lookup are referenced from OGRE sources. MSVC would satisfy them with `#pragma comment(lib)`. MinGW will not.
   Wizard: apply.

## D. Rejected approaches

- Treating “gcc is on PATH” as proof of the compiler. The first `where` result must be under `D:\Program Files\CodeBlocks\MinGW\bin`.
- Passing the absolute `mingw32-make.exe` path as `CMAKE_MAKE_PROGRAM`. The nested shaderc build splits on the space in `Program Files`.
- Enabling Windows 8.3 short names, or moving Code::Blocks out of `Program Files`.
- Editing generated `MoveInstallFiles` scripts or copying `SDL2.dll` by hand.
- Deleting `Source/LibJXR/common/include`. PluginJXR then cannot find `wmspecstring.h`.
- Leaving the `guiddef.h` rename as the delivered fix. The delivered change is a deletion in the patch.
- Rewriting the `_byteswap_ulong` prototype. The existing `__MINGW64_TOOLCHAIN__` switch is the right lever.
- Defining `__ANSI__` for every non-MSVC compiler. That is what broke pointer width on Win64.
- Disabling JPEG XR or FreeImage to get past the compile. JPEG XR is an optional FreeImage codec, and Tutorial00 loads no images, but OGRE’s normal image path is FreeImage (`OGRE_CONFIG_ENABLE_FREEIMAGE` defaults ON; STBI defaults OFF). The codec compiles with the bounded patch, and the runtime log lists `jxr`. Turning it off would drop JPEG XR only if LibJXR were removed, or the whole FreeImage format set if FreeImage were removed (`bmp`, `jpg`, `png`, `tif`, `exr`, `jxr`, and the other formats FreeImage registered).
- Replacing this FreeImage with a newer FreeImageRe. The import is already FreeImageRe 0.2 and is wired through OGRE’s image interfaces. A library upgrade was a larger change than the six hunks that made this snapshot compile.
- Using a prebuilt MSYS2 or vcpkg dependency SDK. Those builds are not this compiler’s output. They were used only as evidence of existing MinGW fixes.
- Switching the engine to `master` because it is newer. It is the 4.x unstable line.
- Building OGRE with the default high parallelism on this 12-thread machine. Retry with `--parallel 4`.
- Using the in-tree `Sample_Tutorial00_Basic.exe` as Gate 3. That executable is produced by the OGRE build. The accepted consumer is `window-test`, which links only `install\lib\Release`.

## Strategy comparison

| Option | Result |
| --- | --- |
| A. Newer ogre-next-deps | No such revision. `f7e23191` is `origin/master`. |
| B. This revision plus a small patch | Chosen. Two patch files, no generated-file edits. Students apply them with `git apply`. The wizard applies the same files. |
| C. Update FreeImage | Rejected for this investigation. High chance of API drift. The GCC 14 and MinGW fixes are local. |
| D. Replace LibJXR | Rejected as a library swap. The maintained MinGW practice (drop the private `guiddef.h`, do not force `__ANSI__`, define `__MINGW64_TOOLCHAIN__`) was applied to the bundled copy. |
| E. Disable only JPEG XR | No CMake switch exists. Not worth a source amputation: the patched LibJXR compiles, and the format remains available. |
| F. Disable FreeImage and use STBI | Supported by OGRE as “bring your own codecs”, and STBI fixes exist on `v3-0`, but it is off by default and drops FreeImage’s format set. Not required. |
| G. External prebuilt dependencies | Rejected. ABI and CRT would have to be proven for this exact GCC. Building the deps with that GCC removes the question. |

## Configure rules that are part of the solution

Use Command Prompt. PowerShell strips a trailing `.exe` from an unquoted `-D` argument, and CMake then warns `Ignoring extra path from command line: ".exe"`.

Every new Command Prompt needs the Code::Blocks `bin` directory first on `PATH`, plus `CC` and `CXX` set to that `gcc.exe` and `g++.exe`.

CMake must also be told:

- `-G "MinGW Makefiles"`
- `-DCMAKE_BUILD_TYPE=Release`
- `-DCMAKE_MAKE_PROGRAM=mingw32-make.exe` (the bare name, never the `Program Files` path)
- `-DCMAKE_C_COMPILER`, `-DCMAKE_CXX_COMPILER`, and `-DCMAKE_RC_COMPILER` pointing at `gcc.exe`, `g++.exe`, and `windres.exe` in the Code::Blocks `bin` directory

`CC`/`CXX` alone were enough once `PATH` was clean, because CMake recorded those compilers. Passing the three compiler variables makes a clean configure deterministic even if another `gcc.exe` appears later on `PATH`. `CMAKE_AR` followed `PATH` to the Code::Blocks `ar.exe` and did not need its own override when `bin` was first.

Build both trees with `cmake --build ... --parallel 4`.

## Gate 3 consumer

`window-test` is not part of either git clone. It is supplied beside the patches.

It follows `Samples/2.0/Tutorials/Tutorial00_Basic` on `v3-0`: `Root` with `plugins.cfg`, `ogre.cfg`, and `Ogre.log`; HLMS registration from `resources2.cfg`; `ST_GENERIC`; one camera; `createBasicWorkspaceDef` with clear colour `(0.2, 0.4, 0.6)`. The tutorial’s `showConfigDialog()` was replaced by an explicit choice, in order: Direct3D11, OpenGL 3+, Vulkan. The chosen system is set windowed at `1280 x 720 @ 32-bit colour` with sRGB on. The loop renders until the window is closed.

The link line is only:

`-L install/lib/Release -lOgreHlmsPbs -lOgreHlmsUnlit -lOgreMain`

No include or library from `ogre-next\` or `ogre-next\build\` is used.

Observed run, Direct3D11 on Intel Iris Xe:

- Log: `D3D11RenderSystem::_createRenderWindow "Tutorial 00: Basic", 1280x720 windowed`
- Log: `*** D3D11: Subsystem Initialized OK ***`
- A visible window titled `Tutorial 00: Basic` showed the blue clear colour.
- Closing the window printed `Rendered and closed cleanly.` and the log ended with `D3D11: Shutting down cleanly.`

### Runtime contract

Working directory: `D:\Users\thoma\Documents\git\ogre-next-test\install\bin\Release`

`plugins.cfg` sets `PluginFolder=.` and optionally loads Direct3D11, GL3Plus, and Vulkan, plus `Plugin_ParticleFX`.
`resources2.cfg` sets `[Hlms] DoNotUseAsResource=../../Media`. From `bin\Release`, that is `install\Media`.

Include directories:

- `install\include\OGRE`
- `install\include\OGRE\Hlms\Common`
- `install\include\OGRE\Hlms\Unlit`
- `install\include\OGRE\Hlms\Pbs`

Import libraries: `libOgreMain.dll.a`, `libOgreHlmsUnlit.dll.a`, `libOgreHlmsPbs.dll.a` in `install\lib\Release`.

DLLs that must be loadable from that directory or from `PATH`:

- `OgreMain.dll`, `OgreHlmsUnlit.dll`, `OgreHlmsPbs.dll`
- `RenderSystem_Direct3D11.dll`, `RenderSystem_GL3Plus.dll`, `RenderSystem_Vulkan.dll`, `Plugin_ParticleFX.dll`
- `amd_ags_x64.dll` (already installed next to the render-system DLL)
- MinGW runtime, resolved from `D:\Program Files\CodeBlocks\MinGW\bin` on `PATH`: `libgcc_s_seh-1.dll`, `libstdc++-6.dll`, `libwinpthread-1.dll`
- Windows: `d3d11.dll`, `dxgi.dll`, `D3DCOMPILER_47.dll`, `PSAPI.DLL`

`OgreMain.dll` does not import `SDL2.dll`. The Win32 window path does not need it. `SDL2.dll` is still installed with the dependency SDK.

HLMS template trees used at runtime live under `install\Media\Hlms`. The window test creates no mesh and loads no texture. It still needs those template directories because `registerHlms()` opens them.

## E. Final reproducibility result

Gates 1, 2, and 3 passed on this machine.

- Gate 1. `ogre-next-deps` at `f7e23191` plus `patches\ogre-next-deps-mingw.patch` configured, built, and installed with Code::Blocks GCC 14.2.0. The SDK is `ogre-next-deps\build\ogredeps`.
- Gate 2. OGRE-Next `v3-0` at `4859016` plus `patches\ogre-next-v3-0-mingw.patch` configured against that SDK, built with `--parallel 4`, and installed to `D:\Users\thoma\Documents\git\ogre-next-test\install`.
- Gate 3. `window-test.exe`, linked only to that install, opened a Direct3D11 window and closed cleanly.

The builds that passed were the patched working trees. The patch files were then exported from those trees and match them (`git apply --reverse --check`). A second clone was not built after the patch files were written. A student following `STUDENT-INSTRUCTIONS-OGRE-NEXT-WINDOWS.md` starts from clean clones and applies those same patches.

Compiler identity recorded in the CMake caches: `CMAKE_C_COMPILER`, `CMAKE_CXX_COMPILER`, `CMAKE_RC_COMPILER`, and `ar` under `D:/Program Files/CodeBlocks/MinGW/bin/`, with `CMAKE_MAKE_PROGRAM` equal to `mingw32-make.exe`.
