# Code::Blocks v3.5b — LDC on Windows

**Status: planning.** v3.5 is done. It makes the D application wizard prefer LDC, installs the selected compiler with Homebrew on macOS, and then calls `Wizard.AutoDetectCompiler` so the existing compiler profile finds that install. Windows in v3.5 does none of this: leaving the compiler page does not check that a compiler exists. This version is the Windows install and configuration path for that same wizard. It does not replace the macOS Homebrew path.

**Question this version answers:** what replaces `brew install ldc` on Windows, and how does the wizard leave the LLVM D Compiler profile usable?

The answer is the v3.3 Windows helper plus the v3.5 auto-detect bridge. `Wizard.WinSdkStage` and `Wizard.WinSdkPublish` download one official LDC archive after **Install LDC**, check its SHA-256, and publish a versioned tree under `%LOCALAPPDATA%\codeblocks\sdk`. `Wizard.AutoDetectCompiler("ldc")` then finds `ldc2.exe` in that tree. The wizard does not write the master path. DMD and GDC stay on the compiler page. This wizard does not download them.

Upstream baseline remains Code::Blocks trunk **r14050** / IDE **25.03**. The make program this plan relies on is `mingw32-make.exe` from the GNU GCC compiler profile already configured for this IDE. That profile’s toolchain on the machine used here is WinLibs GCC 14.2.0, UCRT, `x86_64-w64-mingw32`.

Evidence labels: **CONFIRMED FROM SOURCE** (this tree, 6 November 2026), **CONFIRMED FROM WEB** (GitHub release 1.43.0, published 30 August 2026), **CONFIRMED FROM LOCAL MACHINE** (6 November 2026), **PROPOSED DESIGN**.

---

## 1. Homebrew, and what stands in for it

v3.5 on macOS maps the selected compiler to a Homebrew formula (`ldc` → `ldc`, `dmd` → `dmd`, `gdc` → `gcc`) and, when that formula is missing, calls `Wizard.MacBrewInstall`. Afterward `Wizard.AutoDetectCompiler` must report a detected master path before a project is created. Code::Blocks does not install Homebrew.

Windows uses that shape for LDC only. No new download helper, and `macbrew.cpp` gains no Windows code.

| macOS v3.5 | Windows v3.5b |
|---|---|
| `Wizard.MacBrewFormula("ldc")` | `Wizard.WinSdkTree("ldc")`, then the script checks `bin/ldc2.exe`, `import/std/stdio.d`, and `lib/phobos2-ldc.lib` |
| `Wizard.MacBrewInstall("ldc", …)` | `Wizard.WinSdkStage` then `Wizard.WinSdkPublish` |
| Homebrew prefix, discovered by the LDC XML search of `/opt/homebrew/bin` | The published directory under `%LOCALAPPDATA%\codeblocks\sdk\ldc\1.43.0-x86_64`, discovered by a new Windows search in the same XML |
| `Wizard.AutoDetectCompiler("ldc")` | The same call. The wizard still does not assign the master path |
| DMD and GDC can be installed by Homebrew | DMD and GDC remain selectable. A profile that already detects is accepted. A missing one is not downloaded |
| No project when the formula or the detection fails | No project when the archive, the 1.43.0 tree, or the detection fails |

The generated project stays `d/console`, with the same Debug and Release targets. `hello.d` is unchanged.

---

## 2. The archive

**CONFIRMED FROM WEB.** LDC 1.43.0, release tag `v1.43.0`, published 30 August 2026. The asset used here is the official Windows x64 archive from that GitHub release. The multilib installer, the multilib 7z, the x86 7z, and the source archives on the same release are the wrong files.

| Field | Value |
|---|---|
| File | `ldc2-1.43.0-windows-x64.7z` |
| Size | 45,810,532 bytes |
| SHA-256 | `60ae3d5e34287aa25433c7550520060a52f9f212e3d5ff6472a9067d8d17e47e` |
| URL | `https://github.com/ldc-developers/ldc/releases/download/v1.43.0/ldc2-1.43.0-windows-x64.7z` |

**CONFIRMED FROM LOCAL MACHINE.** Windows `tar.exe` is bsdtar 3.8.9 (libarchive 3.8.9). It lists and unpacks this 7z, including when the file is named `_cb_download.zip`, which is the name `WinSdkStage` writes. The verbose listing contains only ordinary files and directories, so the helper’s existing entry check accepts it. `winsdk.cpp` does not need a 7z special case.

The archive has one top-level directory, `ldc2-1.43.0-windows-x64\`, containing `bin\`, `import\`, `lib\`, and `etc\`. `bin\ldc2.exe` is present. `bin\ldc.exe` is not. The markers the script passes are:

```text
bin/ldc2.exe
import/std/stdio.d
lib/phobos2-ldc.lib
```

There is no “latest” URL. The wizard does not launch the multilib `.exe`, and it does not install into `Program Files`.

The same starter as the wizard, `hello.d`, was compiled with this `ldc2.exe`. On this machine Visual Studio 2022 is installed, and LDC’s default link used that installation’s `link.exe`. The executable ran and printed `Hello World`. A second link with `--link-internally` also ran. Its imports were `KERNEL32.dll`, `ADVAPI32.dll`, and `WS2_32.dll`. The LDC README in the archive says that, when no Visual C++ install is found, LDC uses its integrated LLD and the WinSDK import libraries under `lib\mingw`. The wizard does not pass `--link-internally`. The link line stays the one already in `options_ldc.xml`: `ldc2` is the compiler and the linker.

---

## 3. The profile has to name `ldc2.exe`

**CONFIRMED FROM SOURCE.** v3.5 changed the non-Windows programs in `options_ldc.xml` from `ldc` to `ldc2`. The Windows programs are still:

```xml
<Program name="C"   value="ldc.exe"/>
<Program name="CPP" value="ldc.exe"/>
<Program name="LD"  value="ldc.exe"/>
```

`compiler_ldc.xml` searches `PATH` for that program and, on Windows, falls back to `C:\ldc`. A detected master path is the directory above `bin`. The include directory is `<master>\import` and the library directory is `<master>\lib`. Both match this archive once the master path is the published tree.

v3.5b changes the three Windows program names to `ldc2.exe`. `C:\ldc` remains the fallback for a tree someone already placed there, and it too is recognised only when it contains `bin\ldc2.exe`.

The published tree is not on `PATH` and is not `C:\ldc`. **CONFIRMED FROM SOURCE:** a `<Search path>` value is used literally. `%ProgramFiles%` appears in other compiler XML files and is not expanded. v3.5b expands environment variables on that attribute, then adds this Windows search after `PATH` and before the `C:\ldc` fallback:

```xml
<Search path="%LOCALAPPDATA%\codeblocks\sdk\ldc\1.43.0-x86_64"
        for="C"/>
```

An `ldc2.exe` already on `PATH` still wins. Otherwise auto-detect resolves `bin\ldc2.exe` under that directory and sets the master path to the published tree. The wizard does not write the path itself.

---

## 4. Make is not in the archive

**CONFIRMED FROM LOCAL MACHINE.** `bin\` in the archive has `ldc2.exe` and no `make.exe`. `make.exe` is not on `PATH`. The GNU GCC profile’s `bin` contains `mingw32-make.exe`. Code::Blocks runs `$make` with the selected compiler’s `bin`, its extra paths, and `PATH` prepended. An LDC master path alone will not find a make program.

**PROPOSED DESIGN.** LDC auto-detect, not the wizard script, fills that gap after the master path is known. If `make.exe` is absent from the LDC `bin`, and the GNU GCC compiler profile’s `bin` contains `mingw32-make.exe`, the detector adds that `bin` as an LDC extra path and sets the LDC make program to `mingw32-make.exe`. The Settings → Compiler **Auto-detect** action goes through the same detector, so it does not belong only to the wizard. If that make program cannot be found, detection does not count as success and no project is created. The wizard does not copy make into the LDC tree and does not hard-code a MinGW directory.

---

## 5. Wizard behaviour

**PROPOSED DESIGN.** The Windows change in the wizard is confined to `src/plugins/scriptedwizard/resources/d/wizard.script`. The library id is `ldc`. The edition is `1.43.0-x86_64`. The saved record is `/windows/sdk/ldc/root`.

The compiler page stays `Wizard.AddCompilerPage(_T("ldc"), _T("ldc;dmd;gdc"), true, true)`.

On Windows, leaving that page forward does what leaving it does on macOS: the selected compiler must be usable, or the wizard stops without a project.

- **LDC, and auto-detect already succeeds.** No install page and no download. This covers an `ldc2.exe` already on `PATH` and a published tree that still validates.
- **LDC, and auto-detect fails.** The install page explains that the project needs LDC and that nothing is downloaded until **Install LDC**. Cancel on that dialog does not connect. After a published tree passes the marker check, `Wizard.AutoDetectCompiler("ldc")` must report `found=1`, `detected=1`, and a master path whose `bin\ldc2.exe` exists. Only then may the wizard continue.
- **DMD or GDC, and auto-detect succeeds.** Continue. No download.
- **DMD or GDC, and auto-detect fails.** No download. The wizard says that the automatic install applies to LDC, and no project is created.

Order for the LDC install, already implemented by `WinSdkStage` / `WinSdkPublish`:

1. Consent dialog.
2. Download into a disposable directory under `%LOCALAPPDATA%\codeblocks\sdk\_cb_stage\ldc\`.
3. SHA-256 must match before unpack.
4. Unpack, then find the directory that contains every marker.
5. The script checks the three markers. A failed check publishes nothing.
6. Publish to `%LOCALAPPDATA%\codeblocks\sdk\ldc\1.43.0-x86_64` and remember that path.
7. Run `Wizard.AutoDetectCompiler("ldc")`. The make rule in section 4 is part of that detection. Only a successful report continues to `SetupProject`.

A hash mismatch, a rejected archive entry, a failed unpack, or a failed detection leaves any previously published tree in place. Cancel after the transfer starts stops that transfer only and is not success. The helper refuses to publish under a Code::Blocks install directory.

`SetupProject` is unchanged. It does not add include or library directories. Those come from the LDC profile once the master path is the published tree.

---

## 6. Milestones

| | Objective | Check | Status |
|---|---|---|---|
| M1 | Keep the macOS path, the compiler page, and `d/console`. | `wizard.script` still calls `MacBrewInstall` on macOS. `macbrew.cpp` and `winsdk.cpp` are not edited. DMD and GDC remain selectable. | |
| M2 | Install the 1.43.0 x64 archive only for LDC, and only after **Install LDC**. | Cancel publishes nothing. DMD and GDC never call `WinSdkStage`. | |
| M3 | Point the Windows LDC profile at `ldc2.exe` and at the published tree. | Auto-detect reports the published directory as the master path without the wizard assigning it. | |
| M4 | Make the profile able to run a build. | After detection, the LDC extra path reaches `mingw32-make.exe` from the GNU GCC profile. | |
| M5 | Generate, build, and run from the wizard. | A fresh D project builds with the published `ldc2.exe` and runs `Hello World`. A second wizard run does not download while that tree still validates. | |

M5 is the wizard. The hand compile in section 2 does not close M5.

---

## 7. Acceptance

v3.5b is accepted when all of the following hold:

1. On macOS the wizard still installs or accepts Homebrew LDC, DMD, and GDC, and still preselects LDC.
2. On Windows the compiler page still preselects LDC and still offers DMD and GDC.
3. With no working LDC profile, nothing is downloaded until **Install LDC**.
4. The file fetched is the 1.43.0 x64 archive above, and its SHA-256 matches before unpack.
5. The published tree is `%LOCALAPPDATA%\codeblocks\sdk\ldc\1.43.0-x86_64`.
6. `Wizard.AutoDetectCompiler("ldc")` reports that directory as the master path, and the profile’s compiler and linker are `ldc2.exe`.
7. The build finds `mingw32-make.exe` through the LDC extra path.
8. The generated project builds and prints `Hello World`.
9. A later wizard run does not download while `bin\ldc2.exe` is still in that tree and auto-detect still succeeds.
10. Selecting DMD or GDC does not download this archive. An already detected DMD or GDC profile can still create a project.
11. No project is created when the user cancels, the hash fails, or auto-detect fails.

---

## 8. What stays rejected

- Winget, Chocolatey, Scoop, MSYS2, or an installer `.exe` for LDC.
- Installing LDC into `Program Files` or beside `codeblocks.exe`.
- The multilib archive, the x86 archive, and a build of LDC from source.
- Passing `--link-internally` or a Visual Studio path from the wizard. LDC chooses its linker.
- Downloading DMD or GDC. The DMD profile still expects `bin\dmd.exe` and `<master>\src\phobos`, which is not the official `dmd2\windows\bin` layout, and this version does not retarget it. The GDC profile still expects `mingw32-gdc.exe`, which the WinLibs GCC 14.2.0 used here does not provide.
- Writing the LDC master path from the wizard, or hard-coding the MinGW directory into the generated project.
- A new download helper, or Windows code in `macbrew.cpp`.
- Changing `hello.d`, the macOS formulas, or the macOS LDC program names.

---

## 9. Done when

A Windows user can choose the D application wizard, leave LDC selected, press **Install LDC** once, and build and run the starter without setting a compiler directory and without repairing the LLVM D Compiler profile. The next run does not ask again while that published tree still has `ldc2.exe` and auto-detect still succeeds.

That is the Homebrew step for the preferred compiler, replaced by one pinned upstream archive and the existing auto-detect bridge. DMD and GDC stay on the page for a profile that already detects.

## References

- [LDC 1.43.0 release](https://github.com/ldc-developers/ldc/releases/tag/v1.43.0)
- [v3.5 D wizard](versioning_plan_v3_5.md)
- [v3.3 Windows SDK helper](versioning_plan_v3_3.md)
