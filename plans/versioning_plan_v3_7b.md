# Code::Blocks v3.7b — MCS51 on Windows

**Status: in progress.** The Windows detection handoff and the SDCC make rule are in the tree. A wizard run has not yet confirmed the browser page, a generated build, or `s51`. v3.7 is done. It makes the MCS51 wizard use Homebrew SDCC on macOS, refreshes the existing SDCC compiler profile, and runs the generated Intel HEX image through `s51`. Windows in v3.7 still offers SDCC, Keil, and IAR and does not check that SDCC is installed. This version is the Windows detection and Run path for that same wizard.

**Question this version answers:** what replaces `brew install sdcc` on Windows, and how does Run launch `s51`?

The official SDCC installer replaces `brew install sdcc`. Code::Blocks does not. On Windows the wizard calls `Wizard.AutoDetectCompiler("sdcc")`. A usable installation is accepted, whatever version the existing search finds. A missing installation is explained, and **Yes** opens the official SDCC 4.6.0 Windows x64 release page in the user's browser. The user installs SDCC with that installer, outside Code::Blocks. **Next** runs detection again. The wizard never asks for a compiler directory.

This is intentional, and it is not the FLTK v3.6b shape. FLTK 1.4.5 has no prebuilt Windows install, so that wizard builds FLTK. SDCC publishes a Windows installer, so this wizard points at that installer and then uses the installation the existing compiler profile detects.

Once detection succeeds, the wizard integrates that installation. The make program comes from the SDCC tree when `make.exe` is there, and otherwise from `mingw32-make.exe` in the configured GNU GCC profile. The generated project still produces `.ihx` and `.hex`. Run starts `<master>\bin\s51.exe` and leaves the simulator interactive.

Upstream baseline remains Code::Blocks trunk **r14050** / IDE **25.03**.

Evidence labels: **CONFIRMED FROM SOURCE** (this tree, 7 November 2026), **CONFIRMED FROM WEB** (SDCC 4.6.0, released 22 June 2026), **CONFIRMED FROM THE INSTALLER** (the official x64 setup, hashed and listed on 7 November 2026), **PROPOSED DESIGN**.

---

## 1. Who installs, and who integrates

v3.7 on macOS asks Homebrew about the `sdcc` formula and, when it is missing, calls `Wizard.MacBrewInstall`. It then requires `bin/sdcc`, `bin/sdar`, and `bin/packihx` under the Homebrew prefix, and `Wizard.AutoDetectCompiler("sdcc")` must report a master path. The generated Intel HEX target uses `<prefix>/bin/s51` as its host application. Code::Blocks does not install Homebrew.

Windows does not grow a second installer beside the vendor's.

| macOS v3.7 | Windows v3.7b |
|---|---|
| `Wizard.MacBrewFormula("sdcc")` | `Wizard.AutoDetectCompiler("sdcc")` |
| `Wizard.MacBrewInstall("sdcc", …)` when the formula is missing | No download and no install. **Yes** opens the official release page with `wxLaunchDefaultBrowser` |
| Homebrew prefix | The master path auto-detect already returned |
| `Wizard.AutoDetectCompiler("sdcc")` | The same call. The wizard does not assign the master path |
| Keil and IAR are not on the macOS compiler page | Keil and IAR remain selectable. They never open the SDCC page |
| `s51` from the Homebrew prefix | `s51.exe` from the detected master path |

The starter, the memory-model page, and the Keil and IAR project lines stay as they are. `main.c` is unchanged. `macbrew.cpp` and `winsdk.cpp` are not edited.

---

## 2. Why the wizard does not touch the installer

**CONFIRMED FROM WEB.** SDCC 4.6.0 was released 22 June 2026. The Windows x64 folder for that release contains `sdcc-4.6.0-x64-setup.exe` and two release-candidate installers. It contains no zip. The page the wizard opens is that folder, not a file URL:

```text
https://sourceforge.net/projects/sdcc/files/sdcc-win64/4.6.0/
```

**CONFIRMED FROM THE INSTALLER.** The setup that was inspected is the file at `https://downloads.sourceforge.net/project/sdcc/sdcc-win64/4.6.0/sdcc-4.6.0-x64-setup.exe`. It is 9,949,266 bytes, SHA-256 `0a165e155a052fcf7c29ea703ee77d5a8eb578eba58279e79e885618dc4b2e1a`. Those two facts identify the file. They are not wizard inputs. The file is an NSIS-3 Unicode installer, solid LZMA, and its manifest requests `requireAdministrator`. Windows `tar.exe` does not recognise it. A conventional install of this file contains:

```text
bin\sdcc.exe
bin\sdar.exe
bin\packihx.exe
bin\s51.exe
bin\cc1
include\mcs51\8051.h
```

`bin\cc1` has no `.exe` suffix. There is no `make.exe`.

**CONFIRMED FROM SOURCE** of `support/scripts/sdcc.nsi`, the script that produces this installer. The default directory is `%ProgramFiles%\SDCC`. A silent install writes `bin` into the user `PATH` and writes `HKLM\Software\Microsoft\Windows\CurrentVersion\Uninstall\SDCC` value `InstallLocation`.

That is the vendor's installation. The wizard does not download the setup, does not execute it, does not unpack it, and does not write `PATH` or the registry. `WinSdkStage` and `WinSdkPublish` are not called for SDCC. `winsdk.cpp` gains no hash exception.

An SDCC the profile already detects does not have to be 4.6.0. The release page is the current official Windows x64 installer the wizard names when nothing usable is detected. The wizard does not reject another version that the existing search found and that still has the tools below.

---

## 3. Detection that already exists

**CONFIRMED FROM SOURCE.** `compiler_sdcc.xml` searches, in order, `PATH`, then

```text
HKEY_LOCAL_MACHINE\Software\Microsoft\Windows\CurrentVersion\Uninstall\SDCC
    InstallLocation
```

and then falls back to `%ProgramFiles%\sdcc`. `AddPath` normalises with environment variables, so that fallback is not a literal `%ProgramFiles%` string. A detected master path is the directory above `bin`. Include is `<master>\include` and lib is `<master>\lib`.

`Wizard.AutoDetectCompiler("sdcc")` is already bound. It returns three fields and nothing else:

```text
found=1 or 0
detected=1 or 0
master=<path>
```

`found=1` means the SDCC compiler profile exists. `detected=1` means `AutoDetectInstallationDir()` returned `adrDetected`, which today means `sdcc.exe` was found under the master path or an extra path. The wizard does not add a `%LOCALAPPDATA%\codeblocks\sdk\sdcc\...` search. `compiler_sdcc.xml` and `options_sdcc.xml` stay as they are. The Windows program names are already `sdcc.exe`, `sdar.exe`, and `make.exe`.

**PROPOSED DESIGN.** A usable Windows SDCC is all of the following, checked by the script against the master path from that report:

```text
detected=1
<master>\bin\sdcc.exe
<master>\bin\sdar.exe
<master>\bin\packihx.exe
<master>\bin\s51.exe
<master>\include\mcs51\8051.h
```

`IO.FileExists` is the check. The script does not parse `sdcc -v` and does not require 4.6.0. The wizard remembers that master path for the `s51` host. It does not write it into the compiler profile. Auto-detect already did that.

`bin\cc1` is part of the inspected 4.6.0 tree. It is not an extra script marker. SDCC finds it beside `sdcc.exe` when the master path is the installation root.

---

## 4. Make is not in the official tree

**CONFIRMED FROM THE INSTALLER.** The 4.6.0 x64 setup has no `make.exe`. **CONFIRMED FROM SOURCE.** `options_sdcc.xml` still names the Windows make program `make.exe`. The build prepends `<master>\bin` and the compiler's extra paths to `PATH`. An SDCC master path alone does not find a make program. The same build prepend is how `packihx.exe` is found. The existing post-build line stays:

```text
cmd /c "packihx <$(TARGET_OUTPUT_DIR)$(TARGET_OUTPUT_BASENAME).ihx >$(TARGET_OUTPUT_DIR)$(TARGET_OUTPUT_BASENAME).hex"
```

The wizard does not add an SDCC directory to the user `PATH`.

**PROPOSED DESIGN.** SDCC auto-detect grows a sibling of `EnsureWindowsLdcMake` in `compilerXML.cpp`. The LDC function stays the LDC function and is not generalised. For compiler id `sdcc` only:

- `bin\make.exe` under the detected master path is used when it exists. The XML name `make.exe` then matches a file in that `bin`.
- Otherwise, if the GNU GCC profile (`gcc`) has `<master>\bin\mingw32-make.exe`, that `bin` is added as an SDCC extra path and the SDCC make program is set to `mingw32-make.exe`.
- If neither file exists, auto-detect does not return `adrDetected`.

Settings → Compiler **Auto-detect** goes through the same detector. The wizard does not copy make into the SDCC tree and does not hard-code a MinGW directory.

The script can tell this failure apart from a missing SDCC without a new report field. `detected=0` with `<master>\bin\sdcc.exe` present, and the other four markers present, means the compiler was found and the make program was not. The message then says that the GNU GCC profile has no `mingw32-make.exe`. It does not send the user to install SDCC again. `detected=0` with no `sdcc.exe` under the master path means SDCC was not found.

---

## 5. The handoff, using calls that already exist

**CONFIRMED FROM SOURCE.** The pieces the handoff needs are already bound.

`OnLeave_CompilerPage` is already the MCS51 compiler-page leave function. `WizPageBase::OnPageChanging` vetoes the page change when that function returns false. The user stays on the compiler page. The next **Next** calls it again. That second call is the retry. There is no separate "detect again" API, and this version does not add one.

`Message(text, caption, style)` is the global `cbMessageBox` binding. The console wizard already uses `Message(..., wxICON_QUESTION | wxYES_NO)` and compares the result with `wxID_YES`. `wxYES_NO`, `wxID_YES`, and `wxID_NO` are bound constants. The buttons are the stock **Yes** and **No**. Their captions cannot be changed through this call.

`wxLaunchDefaultBrowser(url, 0)` is the global binding the MCS51 macOS path already uses to open `https://brew.sh`. The same call opens the SDCC release page. It returns whether the browser was launched. It does not download.

`Wizard.MacBrewChoice` draws a dialog with custom button labels. It is compiled and bound only under `__WXMAC__`, and the dialog title is Homebrew. Windows does not call it. This version does not add a Windows copy of that dialog. Stock **Yes** and **No**, with the question saying what each one does, are the existing mechanism.

`Wizard.AddInfoPage` is static text plus the wizard's Back, Next, and Cancel. It has no hyperlink and no extra button. It is also registered from `BeginWizard`, before the user has chosen SDCC, Keil, or IAR. The handoff is not an info page. Keil and IAR never see an SDCC page.

`Wizard.AddGenericSelectPathPage` asks the user for a directory. This version does not register it. A usable auto-detect is the directory.

**PROPOSED DESIGN.** The Windows change in the wizard is confined to `src/plugins/scriptedwizard/resources/mcs51/wizard.script`. The compiler page stays:

```text
Wizard.AddCompilerPage("", "sdcc*;keil*;iar8051", true, true)
```

on Windows, and the macOS page stays SDCC only.

`OnLeave_CompilerPage`, going forward:

- **macOS.** Unchanged. `SDCCMacAllowForward()` still runs.
- **Windows, Keil or IAR.** Return true immediately. No detection, no dialog, no browser.
- **Windows, SDCC, and the section 3 check passes.** Remember the master path. Return true. No dialog.
- **Windows, SDCC, `sdcc.exe` is present, the other markers are present, and `detected=0`.** Show an error that the GNU GCC profile has no `mingw32-make.exe`. Do not open the SDCC page. Return false. No project is created.
- **Windows, SDCC, otherwise.** `Message` asks, in substance: SDCC is not available to Code::Blocks. Code::Blocks does not install it. **Yes** opens the official SDCC 4.6.0 Windows x64 download page. Install SDCC with that installer, then press **Next**. **Next** detects the installation again. A compiler directory is not required. **No** stays on this page without opening the browser.

`wxID_YES` calls:

```text
wxLaunchDefaultBrowser(
    "https://sourceforge.net/projects/sdcc/files/sdcc-win64/4.6.0/",
    0)
```

Every result of that dialog, including **No**, returns false from `OnLeave_CompilerPage`. The page change is vetoed. **Next** is the retry: the same function runs, calls `Wizard.AutoDetectCompiler("sdcc")` again, and continues with no dialog when the section 3 check passes. If the browser call returns false, the error says the page could not be opened and still shows the URL as text. The wizard still does not download the file.

Going backward from the compiler page returns true and does not detect.

The wizard's own **Cancel** remains the framework cancel. It creates no project. The `Message` dialog is `wxYES_NO`, so it has no third button.

A detected master path that is missing one of `sdar.exe`, `packihx.exe`, `s51.exe`, or `include\mcs51\8051.h` uses the same dialog. The text names the missing file. It still does not ask for a directory.

---

## 6. Project and simulator

**PROPOSED DESIGN.** `SetupProject` on Windows SDCC keeps `-mmcs51`, the selected memory model, the memory-size linker options, `--out-fmt-ihx` or `--out-fmt-s19`, and the `packihx` post-build line. Debug stays an MCS51 image with debug symbols. Release stays an ordinary optimised MCS51 image. The output extension stays `.ihx` for Intel HEX and `.s19` otherwise.

`ConfigureSDCCSimulator` gains the Windows SDCC Intel HEX case beside the macOS one. The host application is the remembered `<master>\bin\s51.exe`. On Windows the target type is console, set before the output filename, so Run's working directory is that output directory. The image argument is `"$(TARGET_OUTPUT_FILENAME)"`. uCsim reads `file[:offset]`, and a drive letter is that colon, so an absolute `D:\...` path is loaded as a file named `D`. macOS keeps the quoted `$(PROJECT_DIR)` plus the output path. `SetRunHostApplicationInTerminal(true)` and `SetUseConsoleRunner(true)` stay as on macOS. SDCC's console and native link commands are the same line.

**CONFIRMED FROM SOURCE.** v3.7 already makes a configured host application take precedence over executing the target output, on Windows as well as on macOS. Run therefore starts `s51.exe` and passes the `.ihx`. It does not start the `.ihx` as a native program. This version does not edit `compilergcc.cpp` for that, and it does not reopen the Terminal.app standalone-file fix.

Program Console remains the persistent pane when that preference is on. The external console remains `cb_console_runner`. The wizard does not send `run` to the simulator. Build remains the SDCC compile. `s51` is not a post-build command. S19 output does not set a host, matching macOS, where the simulator is configured only for Intel HEX.

Keil and IAR `SetupProject` branches are unchanged.

---

## 7. Milestones

| | Objective | Check | Status |
|---|---|---|---|
| M1 | Keep the macOS path, the memory-model page, and Keil and IAR. | `wizard.script` still calls `MacBrewInstall` on macOS. `macbrew.cpp` and `winsdk.cpp` are not edited. Keil and IAR never open the SDCC page and never call `WinSdkStage`. | |
| M2 | Hand a missing SDCC to the official installer. | With no usable SDCC, **Next** shows the `Message` dialog and creates no project. **Yes** opens the 4.6.0 Windows x64 release page. **No** does not. Neither answer leaves the compiler page. No setup is downloaded or started. | |
| M3 | Detect the installation the user just made. | After the official installer has finished, **Next** on the same page calls `Wizard.AutoDetectCompiler("sdcc")` again and continues when section 3 passes. The user types no directory. The make program is `make.exe` from the SDCC `bin` when that file exists, otherwise `mingw32-make.exe` from the GNU GCC profile. | |
| M4 | Build the MCS51 image. | A fresh project produces `.ihx` and `.hex` without a hand-set compiler directory. | |
| M5 | Run the image through `s51`. | Run and Build and Run launch `<master>\bin\s51.exe` with the generated image. Simulated execution can be started and stopped. A later wizard run, while that installation still detects, shows no SDCC dialog. | |

M5 is the wizard. A hand compile does not close M5. Opening the browser does not close M2 until the dialog, the veto, and the absence of a download have all been seen.

---

## 8. Acceptance

v3.7b is accepted when all of the following hold:

1. On macOS the wizard still installs or accepts Homebrew SDCC, and Run still launches `s51` from that prefix.
2. On Windows the compiler page still offers SDCC, Keil, and IAR.
3. With a usable SDCC already detected, the wizard does not open a browser and does not show the install dialog. The version does not have to be 4.6.0.
4. With no usable SDCC, nothing is downloaded. **Yes** opens `https://sourceforge.net/projects/sdcc/files/sdcc-win64/4.6.0/` in the normal browser. **No** does not open it. The compiler page stays up either way, and no project is created.
5. The setup is not executed. No administrator prompt, no `Program Files` write, and no change to `PATH` or the registry comes from the wizard.
6. After the user has installed SDCC with the official installer, **Next** detects it through `Wizard.AutoDetectCompiler("sdcc")` and continues. The wizard never asks for the SDCC directory.
7. The build finds `make.exe` in the SDCC `bin` when it is there, otherwise `mingw32-make.exe` through the SDCC extra path. `packihx.exe` is found through `<master>\bin`.
8. The generated project builds an `.ihx`, and Run launches `<master>\bin\s51.exe` on that image. The simulator remains interactive.
9. Selecting Keil or IAR does not show the SDCC dialog and does not open the SDCC page.
10. No project is created when the user cancels, when detection fails, or when SDCC is present but the GNU GCC profile cannot supply `mingw32-make.exe` and the SDCC tree has no `make.exe`.
11. `winsdk.cpp` has no SDCC hash, no NSIS unpack, and no published `sdk\sdcc` tree. Other libraries that already use `WinSdkStage` are unchanged.

---

## 9. What stays rejected

- Downloading `sdcc-4.6.0-x64-setup.exe` from the wizard, including through `WinSdkStage` or `WinSdkFetch`.
- Executing that setup, including a silent install and a `/D` directory.
- Unpacking the NSIS payload, in `winsdk.cpp` or anywhere else, and a SHA-256 exception that would unpack this one executable.
- Publishing an SDCC tree under `%LOCALAPPDATA%\codeblocks\sdk`, or adding that directory to `compiler_sdcc.xml`.
- Writing `PATH`, the SDCC uninstall key, or any other registry value from the wizard.
- 7-Zip, Winget, Chocolatey, Scoop, and MSYS2.
- A private SDCC installation that Code::Blocks copies, builds, or otherwise owns.
- Asking the user to type or browse to the SDCC directory when `Wizard.AutoDetectCompiler("sdcc")` can see the installation.
- A new script API for a custom-labelled dialog. `Message` with `wxYES_NO` is the call. `Wizard.MacBrewChoice` stays macOS-only.
- The 32-bit setup, the release-candidate setups, and a daily snapshot zip, as files the wizard would fetch. The user may install whatever the official page offers. The wizard only opens the 4.6.0 Windows x64 folder.
- Building SDCC from source.
- Downloading Keil or IAR, or changing their project lines.
- Writing the SDCC master path from the wizard, or hard-coding the MinGW directory into the generated project.
- Treating `.ihx` as a native executable, or moving the simulator into a post-build command.
- Sending `run` to `s51` automatically.
- Editing `compilergcc.cpp` to reopen the v3.7 Terminal.app standalone-file fix.
- Adding `Press any key` to the persistent Program Console, or folding ANSI colour rendering into this wizard.
- Changing `main.c`, the macOS Homebrew lines, or the macOS `s51` path.

---

## 10. Known follow-up

v3.7 left uCsim's ANSI colour sequences as visible text in the Program Console. That presentation issue stays outside this version. It does not change detection, the compiler profile, or the decision to launch `s51` interactively.

---

## 11. Done when

A Windows user can choose the MCS51 wizard and leave SDCC selected. If SDCC is already installed in the ordinary way, the wizard detects it and does not ask for a directory. If it is not, **Yes** opens the official 4.6.0 Windows x64 page, the user installs SDCC there, and **Next** detects that installation. The generated project builds an `.ihx` and a `.hex`. Run and Build and Run open `s51.exe` on the image, and the simulator accepts interactive commands. A later run does not ask again while that installation still detects.

Code::Blocks integrates the SDCC installation it detected. The official installer remains the only program that installs SDCC.

## References

- [SDCC 4.6.0 release notes](https://sourceforge.net/p/sdcc/news/2026/06/sdcc-460-released/)
- [SDCC 4.6.0 Windows x64 files](https://sourceforge.net/projects/sdcc/files/sdcc-win64/4.6.0/)
- [v3.7 MCS51 wizard](versioning_plan_v3_7.md)
- [v3.6b Windows FLTK](versioning_plan_v3_6b.md)
