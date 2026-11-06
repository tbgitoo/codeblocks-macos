# Code::Blocks v3.4b — raylib on Windows

**Status: planning.** v3.4 is done. It replaced the Irrlicht Scripted Wizard with raylib and installs that library through Homebrew on macOS. Windows in v3.4 asks for a folder the user already has and downloads nothing. This version is the Windows install path for that same wizard. It does not reopen the Irrlicht replacement, the artwork, or the macOS Homebrew path.

**Question this version answers:** what replaces `brew install raylib` on Windows?

The answer is the v3.3 Windows helper, not a package manager. `Wizard.WinSdkStage` and `Wizard.WinSdkPublish` download one official raylib zip after **Install raylib**, check its SHA-256, and publish a versioned tree under `%LOCALAPPDATA%\codeblocks\sdk`. The generated project links that tree’s shared library and copies `raylib.dll` beside the executable. The static library in the same zip is not the Windows counterpart of the Homebrew library: it does not link with the WinLibs GCC 14.2.0 this tree builds with.

Upstream baseline remains Code::Blocks trunk **r14050** / IDE **25.03**. The compiler the archive was linked against is the selected GNU GCC Compiler’s own `gcc.exe`, probed by `Wizard.WinSdkCompiler`. It is not the first `gcc` on `PATH`. The probe used while writing this plan was WinLibs GCC 14.2.0, UCRT, `x86_64-w64-mingw32`.

Evidence labels: **CONFIRMED FROM SOURCE** (this tree, 6 November 2026), **CONFIRMED FROM WEB** (GitHub release 6.2, published 15 October 2026), **CONFIRMED FROM LOCAL MACHINE** (6 November 2026), **PROPOSED DESIGN**.

---

## 1. Homebrew, and what stands in for it

v3.4 on macOS calls `Wizard.MacBrewFormula("raylib")` and, when the formula is missing, `Wizard.MacBrewInstall`. A suitable prefix has `include/raylib.h` and either `lib/libraylib.dylib` or `lib/libraylib.a`. The generated project records those directories. Code::Blocks does not install Homebrew, and nothing is installed before the user agrees.

Windows already has that shape for SFML, GLFW, and SDL3. raylib joins it. No new download helper, and `macbrew.cpp` gains no Windows code.

| macOS v3.4 | Windows v3.4b |
|---|---|
| `Wizard.MacBrewFormula("raylib")` | `Wizard.WinSdkTree("raylib")`, then the script checks the header, the import library, and the DLL |
| `Wizard.MacBrewInstall("raylib", …)` | `Wizard.WinSdkStage` then `Wizard.WinSdkPublish` |
| Homebrew prefix `include/` and `lib/` | The published directory under `%LOCALAPPDATA%\codeblocks\sdk\raylib\6.2-x86_64` |
| `libraylib.dylib` or `libraylib.a` | `lib/libraylibdll.a` and `lib/raylib.dll`. Not `lib/libraylib.a` |
| `-framework CoreVideo` and the other macOS frameworks | None. Those libraries are inside `raylib.dll` |
| No location page when the prefix validates | No location page when the published tree validates |
| Homebrew missing: find `brew`, or open https://brew.sh. No bootstrap | Compiler is not `x86_64-w64-mingw32`: no download. The v3.4 location page stays. There is no package manager to bootstrap |

Other Unix stays `pkg-config --cflags raylib` and `pkg-config --libs raylib`.

---

## 2. The archive

**CONFIRMED FROM WEB.** raylib 6.2, release tag `6.2`, published 15 October 2026. The asset used here is the official MinGW-w64 zip from that GitHub release. The MSVC zips on the same release are the wrong files. SourceForge is a mirror, not the URL the wizard fetches.

| Field | Value |
|---|---|
| File | `raylib-6.2_win64_mingw-w64.zip` |
| Size | 1,785,574 bytes |
| SHA-256 | `48ba378d100fe7e13fc2c35d708764b855b815596f3b0a31746e37259283961f` |
| URL | `https://github.com/raysan5/raylib/releases/download/6.2/raylib-6.2_win64_mingw-w64.zip` |

**CONFIRMED FROM LOCAL MACHINE.** The zip has one top-level directory, `raylib-6.2_win64_mingw-w64\`, containing:

```text
include\raylib.h
include\raymath.h
include\rlgl.h
lib\libraylib.a
lib\libraylibdll.a
lib\raylib.dll
```

`raylib.h` defines `RAYLIB_VERSION` as `"6.2"`. `WinSdkStage` finds a marker root one directory below the unpack folder, which is this layout. The markers the script passes are:

```text
include/raylib.h
lib/libraylibdll.a
lib/raylib.dll
```

There is no “latest” URL. The wizard does not try the MSVC zip, the win32 zip, the macOS tarball, or a copy hosted by this project.

The win32 file `raylib-6.2_win32_mingw-w64.zip` is on the same release and was not linked here. A compiler whose `dumpmachine` is not `x86_64-w64-mingw32` does not download. The v3.4 location page remains for that compiler, including a 32-bit MinGW.

---

## 3. Shared library, not the static archive

**CONFIRMED FROM LOCAL MACHINE.** `libraylib.a` in this zip was built by MinGW-Builds GCC 15.2.0 (`x86_64-posix-seh-rev1`). Linking the v3.4 starter against it with WinLibs `g++` 14.2.0 fails:

```text
undefined reference to `stat64i32'
```

from `rcore.o`. That is the same CRT mismatch already reported against this prebuilt static library. The static archive is therefore not the Windows stand-in for the Homebrew library.

The shared library does link. The same starter, compiled with:

```text
-DBUILD_LIBTYPE_SHARED
-lraylibdll
```

and the zip’s `include` and `lib` directories, produced an executable. `objdump -p` shows it imports `raylib.dll` and the UCRT API sets. It does not import `opengl32`, `gdi32`, or `winmm`. Those are imports of `raylib.dll` itself, together with `USER32`, `SHELL32`, and `KERNEL32`. With `raylib.dll` beside the executable, the process stayed running after `InitWindow`.

`BUILD_LIBTYPE_SHARED` is what `raylib.h` uses to declare `RLAPI` as `__declspec(dllimport)`. Without it, a link of `libraylibdll.a` is the wrong import mode. The automatic project defines it. The automatic project does not pass `-lraylib`, `-lopengl32`, `-lgdi32`, or `-lwinmm`.

---

## 4. Wizard behaviour

**PROPOSED DESIGN.** The change is confined to `src/plugins/scriptedwizard/resources/raylib/wizard.script`. `winsdk.cpp` is used as it stands. The library id is `raylib`. The edition is `6.2-x86_64`. The saved record is `/windows/sdk/raylib/root`, the same key pattern as SFML.

`BeginWizard` on Windows calls `Wizard.WinSdkCompiler(Wizard.GetCompilerID())`.

- **Access is `works` and the machine is `x86_64-w64-mingw32`.** This is the automatic path. A saved tree that still has `include/raylib.h` reporting 6.2, `lib/libraylibdll.a`, and `lib/raylib.dll` adds no dependency page and no location page. A missing or unsuitable tree adds the dependency page. Nothing is downloaded until **Install raylib**. Cancel on that dialog does not connect and does not create a project. After a published tree passes the same checks, the wizard continues without restarting the IDE.
- **Any other compiler.** No download. The existing v3.4 page remains: the user selects a folder, and the script keeps the include and library directories it actually accepted. That page still links `-lraylib` plus `opengl32`, `gdi32`, and `winmm`, which is the right link line for a static tree the user built with their own GCC.

The install explanation matches the macOS one in substance: the project needs raylib, nothing is downloaded until **Install raylib**, and Cancel does not start a download. The button does not name a zip, a compiler vendor, or a folder to browse to.

Order, already implemented by `WinSdkStage` / `WinSdkPublish`:

1. Consent dialog.
2. Download into a disposable directory under `%LOCALAPPDATA%\codeblocks\sdk\_cb_stage\raylib\`.
3. SHA-256 must match before unpack.
4. Unpack, then find the directory that contains every marker.
5. The script checks `RAYLIB_VERSION` `"6.2"` and the two shared-library files. A failed check publishes nothing.
6. Publish to `%LOCALAPPDATA%\codeblocks\sdk\raylib\6.2-x86_64` and remember that path.
7. Run the saved-tree check again. Only then may `SetupProject` run.

A hash mismatch, a rejected archive entry, a failed unpack, or a failed version check leaves any previously published tree in place. Cancel after the transfer starts stops that transfer only and is not success. Staging left behind is disposable. The helper refuses to publish under a Code::Blocks install directory.

`SetupProject` on the automatic path writes absolute include and library directories from the accepted tree. It does not write `$(#raylib)`. After the link it copies `raylib.dll` to `$(TARGET_OUTPUT_DIR)`, the same kind of post-build copy GLFW uses for `glfw3.dll`. Debug stays a console target with debug symbols. Release stays an ordinary optimised executable. The starter `main.cpp` is unchanged.

---

## 5. Milestones

| | Objective | Check | Status |
|---|---|---|---|
| M1 | Keep v3.4’s macOS path, starter, and artwork. Add Windows install only. | `wizard.script` still calls `MacBrewInstall` on macOS. `macbrew.cpp` and `winsdk.cpp` are not edited for raylib-specific names. | |
| M2 | Select the 6.2 win64 zip only for `x86_64-w64-mingw32`. | A probe of any other machine shows the v3.4 location page and does not call `WinSdkStage`. | |
| M3 | Install only after **Install raylib**. | Cancel on the dialog creates no project and no published tree. A bad hash or a `RAYLIB_VERSION` other than 6.2 publishes nothing. | |
| M4 | Generate the shared-library project. | The `.cbp` has the published include and lib directories, `-DBUILD_LIBTYPE_SHARED`, `-lraylibdll`, and a copy of `raylib.dll`. It has no `$(#raylib)`, no `-lraylib`, and no `-lopengl32`. | |
| M5 | Build and run on the WinLibs compiler. | A fresh wizard project builds without hand repair. Run opens the raylib window. A second wizard run has no dependency page while the saved tree still validates. | |

M5 is the wizard. The link test in section 3 was the archive alone, compiled by hand, and does not close M5.

---

## 6. Acceptance

v3.4b is accepted when all of the following hold:

1. On macOS the wizard still installs or accepts Homebrew raylib, and the generated macOS project is unchanged.
2. On Windows, with the WinLibs GCC 14.2.0 `x86_64-w64-mingw32` compiler selected, a missing raylib tree downloads nothing until **Install raylib**.
3. The file fetched is the 6.2 win64 MinGW zip above, and its SHA-256 matches before unpack.
4. The published tree is `%LOCALAPPDATA%\codeblocks\sdk\raylib\6.2-x86_64`, and a later run reuses it only while `raylib.h` still says 6.2 and the DLL and import library are present.
5. The generated project links `libraylibdll` with `BUILD_LIBTYPE_SHARED`, copies `raylib.dll` next to the executable, and builds with that GCC.
6. Run opens the starter window.
7. A compiler that is not `x86_64-w64-mingw32` is not offered this download. The v3.4 folder page still works.
8. No project is created when the user cancels, the hash fails, or the unpacked tree fails the 6.2 check.

---

## 7. What stays rejected

- Winget, Chocolatey, Scoop, MSYS2, vcpkg, or an MSVC raylib zip.
- Building raylib from source inside the wizard.
- Linking `libraylib.a` from the official win64 zip with this WinLibs GCC.
- Treating the win32 zip as tested. It stays off the automatic path until it is linked the same way.
- A new download helper, or Windows code in `macbrew.cpp`.
- Installing into `Program Files` or beside `codeblocks.exe`.
- Changing the macOS frameworks, the starter source, the wizard artwork, or the Irrlicht material left in place by v3.4.
- A raylib-specific debugger or build system.

---

## 8. Done when

A Windows user with the selected 64-bit WinLibs GCC can choose raylib from New Project, press **Install raylib** once, and build and run the starter without choosing a folder and without repairing the project. The next run does not ask again while that published tree still validates.

That is the Homebrew step, replaced by one pinned upstream zip and the shared library that actually links. raylib stays out of Code::Blocks. The manual folder page remains for a compiler this zip is not offered to.

## References

- [raylib 6.2 release](https://github.com/raysan5/raylib/releases/tag/6.2)
- [v3.4 raylib wizard](versioning_plan_v3_4.md)
- [v3.3 Windows SDK helper](versioning_plan_v3_3.md)
