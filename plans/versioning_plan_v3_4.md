# Code::Blocks Windows v3.4 — installable Windows distribution

**Status: M3 done. No installer has been built.** v3.3 is complete and is the baseline. This version does not reopen the SFML, GLFW, or SDL3 wizards. The only compile change is declaring the existing `nativewatchsummary` sources on the Debugger target so that target links. Amended in planning: on Windows the package is Code::Blocks, not a side-by-side “macOS port” product, and `%APPDATA%\CodeBlocks` stays the shared profile on purpose. Amended again: neither installer nor a passive launch may overwrite, reset, or remove pre-existing settings in that profile. Only a deliberate settings change made by the user may be persisted. Amended again: v3.4 produces two installers, one without a compiler and one with the known-good MinGW.

**Question this version answers:** what is the smallest packaging step that lets a Windows user install this already-working build, launch it, and uninstall it, without the source tree, including a second installer for a user who has no compiler yet?

The desired path is: download an installer, double-click it, install, launch Code::Blocks from the Start Menu, and use the build and debug behaviour this tree already has. `codeblocks-v3.4-win64.exe` is the IDE. `codeblocks-v3.4mingw-win64.exe` is the same IDE plus one known-good MinGW. Packaging an existing runtime is the whole of the work. It is not a Windows port, not a new build system, and not a compiler-management feature.

Upstream baseline remains Code::Blocks trunk **r14050** / IDE **25.03**. The Windows IDE this plan packages is the 64-bit wx 3.3 build produced by `src\CodeBlocks_wx33_64.cbp` and `src\update.bat 33_64`.

Evidence labels: **CONFIRMED FROM SOURCE** (this tree, 24 October 2026), **CONFIRMED FROM WEB** (24 October 2026), **PROPOSED DESIGN**.

---

## 1. Current state

**CONFIRMED FROM SOURCE.** There is no `.nsi` file and no `packaging\` directory in this repository. `src\plugins\contrib\help_plugin\Makefile.am` names `windows_installer\Documentation`, and that directory is not present. A user who wants this build today clones the tree, builds `CodeBlocks_wx33_64.cbp`, and runs `src\update.bat 33_64`.

That batch writes two trees under `src\`:

| Tree | What it is |
|------|------------|
| `devel33_64` | The tree used while developing. Unstripped. `codeblocks.dll` is about 124 MB. It also holds `debugger.dll.bak-…`, `debugger.dll.prev`, and `codeblocks.RPT`. |
| `output33_64` | The distribution-shaped tree. `update.bat` copies the executables and DLLs across, then strips `*.exe` and `*.dll`. `codeblocks.dll` there is about 6.8 MB. |

`output33_64` was refreshed by M2. `scriptedwizard.dll` there is the stripped current plugin, 565,774 bytes written 24 October 2026 19:48, not the old 474 KB file. The matching unstripped plugin in `devel33_64` is 10,220,466 bytes. `templates\wizard` contains `sfml`, `glfw`, and `sdl3`, and does not contain `sdl`. The All target would not link Debugger until `plugins/debuggergdb/nativewatchsummary.cpp` and its header were declared on that target in `CodeBlocks_wx33_64.cbp`. `update.bat` was not edited. The rebuild was driven from `D:\Program Files\CodeBlocks\codeblocks.exe`.

The official 25.03 tree on the machine used to write this plan is `D:\Program Files\CodeBlocks`. That path shows a real install that is not on C:. It is not a path either installer requires. The default directory is `C:\Program Files\CodeBlocks`. The user can choose another valid directory, on C: or on another drive. The IDE-only installer does not bundle a compiler. The turnkey installer bundles the smaller WinLibs archive in section 2, under the directory the user selected, and it does not copy this Program Files tree. Neither installer changes a Code::Blocks directory the user did not select (section 4).

---

## 2. What is the application, and what is not

**CONFIRMED FROM SOURCE.** Listing either tree is not the file list. The installer ships a filtered copy of a fresh `output33_64`.

### Ship

These files are what the stripped tree needs in order to start, and what `update.bat` already treats as the program:

- `codeblocks.exe`, `codeblocks.dll`, `cb_console_runner.exe`
- `wxmsw334u_gcc_custom.dll`, the wx 3.3.4 this tree was linked against. It is not a wx the operating system provides.
- `libgcc_s_seh-1.dll`, `libstdc++-6.dll`, `libwinpthread-1.dll`
- `share\CodeBlocks\plugins\` for the fourteen plugins this project builds: `abbreviations.dll`, `astyle.dll`, `autosave.dll`, `classwizard.dll`, `codecompletion.dll`, `compiler.dll`, `debugger.dll`, `defaultmimehandler.dll`, `OccurrencesHighlighting.dll`, `openfileslist.dll`, `projectsimporter.dll`, `scriptedwizard.dll`, `todo.dll`, `xpmanifest.dll`
- The resource zips, lexers, images, scripts, and `templates\wizard` that `update.bat` copies beside those plugins. That includes `sfml`, `glfw`, and `sdl3`. Both current trees already omit `sdl\`, because `update.bat` excludes it.
- The crash-handler files `update.bat` copies beside the executable and does not strip: `exchndl.dll`, `dbghelp.dll`, `dbgcore.dll`, `mgwhelp.dll`, `symsrv.dll`, `symsrv.yes`. The sources live under `src\exchndl`.

### Do not ship

- Every `*.a` in the output root: `libcodeblocks.a`, `libwxscintilla_cb.a` (about 45 MB), `libsquirrel.a`, `libtxml.a`, `libdepslib.a`. `update.bat` copies them so a plugin can be linked against this SDK. A user who only runs the IDE does not need them. The staging step omits them. `update.bat` itself stays as it is.
- `*.bak`, `*.prev`, `*.RPT`, and anything under `src\.objs33_64`
- The `devel33_64` tree. It is the debug build, and it contains leftovers from plugin swaps.
- `gcc.exe`, `gdb.exe`, and any MinGW tree, from the IDE stage and from `codeblocks-v3.4-win64.exe`. Neither `devel33_64` nor `output33_64` contains one. **CONFIRMED FROM SOURCE.** The turnkey installer adds MinGW only in its own stage, below.

### Where a compiler is found

**CONFIRMED FROM SOURCE.** The install directory used in that search is `ConfigManager::GetExecutableFolder()`, which is `GetModuleFileName` of the running `codeblocks.exe` (`DetermineExecutablePath` in `configmanager.cpp`). The drive letter is the drive of that executable. It is not assumed to be C:.

`CompilerMINGW::AutoDetectInstallationDir` keeps a master path that already points at `bin\<compiler>`. Otherwise it searches `PATH` for a `bin` directory that contains `gcc.exe` or `mingw32-gcc.exe`, then `<exe>\bin`, then `<exe>\MinGW\bin`, then `MinGW.ini` and a few other well-known installs (`compilerMINGW.cpp`). `C:\MinGW` is the default read from `MinGW.ini` only after `<exe>\MinGW\bin\gcc.exe` was not found, and a version-string fallback when the master path is still empty. `cbDetectDebuggerExecutable` searches `PATH` for `gdb`, then `<exe>\MINGW\bin\gdb.exe` (`debuggermanager.cpp`; Windows treats `MinGW` and `MINGW` as the same directory), and only then `C:\MinGW\bin`. An empty debugger path falls through to that search (`debuggeroptionsdlg.cpp`).

No detection code changes. The turnkey layout is `<selected install>\MinGW\bin\gcc.exe`, `g++.exe`, and `gdb.exe`, whatever directory the user chose. A `gcc.exe` earlier on `PATH` wins over that folder. This packaging does not edit `PATH`.

**CONFIRMED FROM LOCAL MACHINE** (24 October 2026). `codeblocks.exe` was copied to `D:\cb-v34-layout-test`, with this section’s toolchain joined at `D:\cb-v34-layout-test\MinGW`. `PATH` was only `C:\Windows\System32;C:\Windows`. A fresh `--user-data-dir` logged `Checking [C::B\MinGW] master path='D:\cb-v34-layout-test\MinGW'` and `Final MinGW master path='D:\cb-v34-layout-test\MinGW'`. The IDE built a console C++ project with that compiler. The program printed `7`. `GNU GCC Compiler`, `GNU GCC MinGW Compiler`, and `GNU GCC MinGW64 Compiler` all saved that same master path.

Auto-detection does not run again once a master path is stored. `CompilerFactory::LoadSettings` calls it only when `GetMasterPath()` is empty (`compilerfactory.cpp`). After that `D:\cb-v34-layout-test\MinGW` junction was removed, the same profile was opened with `D:\cb-v34-layout-test-b\codeblocks.exe`, whose own `MinGW\bin\gcc.exe` existed. The IDE did not select it. The log said `Can't find compiler executable in your configured search paths (for GNU GCC Compiler)...`. That is a recorded planning issue. v3.4 does not change detection to repair it, and it does not add a private profile.

### Turnkey toolchain

**CONFIRMED FROM LOCAL MACHINE** (24 October 2026). The turnkey payload is the non-LLVM archive from the same WinLibs release as the larger tree previously considered, [14.2.0posix-12.0.0-ucrt-r3](https://github.com/brechtsanders/winlibs_mingw/releases/tag/14.2.0posix-12.0.0-ucrt-r3):

| | |
|--|--|
| Archive | `winlibs-x86_64-posix-seh-gcc-14.2.0-mingw-w64ucrt-12.0.0-r3.7z` |
| SHA-256 | `0475a96b4d687a3b85dd979a982883ed9172c55b2516e953664232d46471096f` |
| Compressed size | 101,317,610 bytes |
| Extracted size | 875,001,081 bytes (0.815 GiB) |

The previously measured LLVM tree was 1,533,687,789 bytes (1.43 GiB). This archive is the intact upstream file. It is not a hand-pruned copy. The 7z wraps a `mingw64` directory containing `bin`, `include`, `lib`, `libexec`, `share`, `x86_64-w64-mingw32`, and `version_info.txt`. Staging copies that directory to `<selected install>\MinGW`. The downloaded file matched the published `.sha256`.

`version_info.txt` begins `winlibs personal build version gcc-14.2.0-mingw-w64ucrt-12.0.0-r3`. It lists GCC 14.2.0, GDB 16.2, MinGW-w64 12.0.0 linked with UCRT, Binutils 2.44, Make 4.4.1, and the other tools in that file (CMake, Ninja, Doxygen, Cppcheck, ccache, and the rest). It does not list LLVM. `clang.exe` is absent. `gcc -dumpversion` is `14.2.0`, `gcc -dumpmachine` is `x86_64-w64-mingw32`, the thread model is posix, and `gcc -v` reports `MinGW-W64 x86_64-ucrt-posix-seh, built by Brecht Sanders, r3`. `gdb --version` reports `GNU gdb (GDB for MinGW-W64 x86_64, built by Brecht Sanders, r3) 16.2`. Those compiler and debugger identity strings match the r3 build already in use. The extra tools stay in the tree. v3.4 does not prune them, and it does not ship the LLVM archive.

**CONFIRMED FROM LOCAL MACHINE.** With this `gcc` and `g++`, a C program printed `c-ok` and a C++ program printed `7`. GDB stopped at a breakpoint in `main`, `next` reached the next lines, `print c` was `2`, and `print a` was a `std::string` with `_M_dataplus` / `_M_p` `"19403"`, `_M_string_length` 5, and `_M_local_buf`. `step` entered `hold`, where `n` was `2` and `s` was `"19403"`. That is the libstdc++ dump the existing watch summary already parses. An IDE click-through of Stop and the Watch window was not repeated in this pass. The IDE build from `D:\cb-v34-layout-test` is the detection check above.

**Licences.** [winlibs.com](https://winlibs.com/) states the licences of the components that this archive still contains: GCC, GDB, GNU Binutils, and GNU Make are GPL; MinGW-w64 is the Zope Public License with exceptions; Yasm and NASM are 2-clause BSD; JWasm is the Sybase Open Watcom Public License. LLVM is not in this payload. The extracted tree contains `version_info.txt` and `lib\python3.9\LICENSE.txt`. It does not contain a GCC or GDB `COPYING`. A notice that names those components is required with the binaries. That notice does not by itself satisfy the GPL duty to convey corresponding source. This pass did not find a source archive for this r3 build. The [winlibs_recipes](https://github.com/brechtsanders/winlibs_recipes) repository is MIT build recipes, and it was not shown to be the corresponding source of these binaries. The turnkey installer is blocked until the plan records a concrete way to convey that source. The IDE-only installer does not redistribute this toolchain, so it is not blocked by that duty. The compiler is not vendored into the repository, and it is not copied from Program Files.

### Official 25.03 installers

**CONFIRMED FROM LOCAL MACHINE** (24 October 2026). The files were downloaded from the SourceForge 25.03 Windows folder. SHA-256 matches `checkums.txt` there. Both are NSIS-3 Unicode installers (`7-Zip` subtype). The NSIS script inside them is compressed. It was not recovered, and it is not copied here.

| Installer | Measured size | SHA-256 |
|-----------|---------------|---------|
| `codeblocks-25.03-setup.exe` | 52,312,337 bytes | `88bca802df27fd9a9bae77453cf3bca36a7b69cbb3863e62d70a42f204f28df2` |
| `codeblocks-25.03mingw-setup.exe` | 423,494,500 bytes | `8712227526eb3bb26c90dd5c78301b3fa32bf5869a43294bbf4e9c5512c52792` |

The IDE-only installer contains no `MinGW` directory, no `gcc.exe`, and no `gdb.exe`. The MinGW installer contains `MinGW\version_info.txt` for `gcc-14.2.0-llvm-19.1.7-mingw-w64ucrt-12.0.0-r3`, plus `clang.exe`, `cmake.exe`, `ninja.exe`, `doxygen.exe`, and `cppcheck.exe`. `MinGW\bin\gcc.exe` in that installer is byte-identical to `D:\Program Files\CodeBlocks\MinGW\bin\gcc.exe`. Official 25.03 therefore ships the large LLVM WinLibs tree, not the smaller archive in the table above. That is their package. It is not a failure of the smaller archive. v3.4 keeps the smaller intact archive for the basic C and C++ workflow. It does not prune the large tree down to match, and it does not switch back to the LLVM archive.

The IDE files in the official installer are a different build from this repository’s stage: wxWidgets 3.2 (`wxmsw32u_gcc_custom.dll`), contrib plugins such as `clangd_client.dll`, and extras such as `CbLauncher.exe`, `cb_share_config.exe`, `cbp2make.exe`, and `Addr2LineUI.exe`. v3.4 still stages this repository’s wx 3.3.4 output. It does not adopt the official file list.

On this machine the official program is installed at `D:\Program Files\CodeBlocks`. With `PATH` limited to `C:\Windows\System32` and `C:\Windows`, a build logged `Final MinGW master path='D:\Program Files\CodeBlocks\MinGW'` and produced a program that printed `7`. A non-C: directory works for that official layout. The default directory string was not readable from the compressed script, so it is not stated here. The Start Menu shortcut is `CodeBlocks\CodeBlocks.lnk`. A desktop shortcut exists too. Both point at that `codeblocks.exe`. Their working directory is the Start Menu folder. Machine `PATH` and user `PATH` have no Code::Blocks entry. `.cbp` is associated with that executable, including Build and Rebuild verbs. v3.4 still does not add a desktop shortcut or a file association.

The uninstall registration is `HKCU\SOFTWARE\Microsoft\Windows\CurrentVersion\Uninstall\CodeBlocks`. `DisplayName` is `CodeBlocks`, `DisplayVersion` is `25.03`, and `UninstallString` is `D:\Program Files\CodeBlocks\uninstall.exe`. There is no `InstallLocation` value. `NoModify` and `NoRepair` are set. It is not an HKLM key. The uninstaller was not run, because this is the working install, and a second install was not started, because it would replace that per-user uninstall key. Our installers still request an administrator install so the default Program Files location works. They still record the selected directory and remove that directory only.

The MinGW payload’s licence files are `version_info.txt`, `lib\python3.9\LICENSE.txt`, a CMake template licence, and `share\man\man7\gpl.7`. There is no GCC or GDB `COPYING` and no source offer for the r3 build. Official practice does not supply the corresponding-source method M9 is waiting for.

A machine with no compiler can still open the IDE-only install. Build and debug on that install use whatever the existing search finds. The turnkey install is the one that makes Build, Run, and Debug work with no separate compiler setup.

### How the boundary is checked

Before `makensis` runs, the IDE stage must contain the executables and the wx DLL named above, and must contain no `*.a`, no `gcc.exe`, and no `gdb.exe`. `objdump -p` on `codeblocks.exe` and on each plugin must not name a non-OS DLL that is missing from the stage root. UCRT (`api-ms-win-crt-*`) comes with Windows 10 and is not bundled. Then the staged `codeblocks.exe` is started with a `PATH` of only System32 plus the stage directory. If the IDE opens, the program is not depending on a DLL that happens to sit on the packager’s `PATH`. That launch does not prove that compile or debug works. Those checks are M6 for an external toolchain, and M10 for the bundled one.

M2 refreshed `output33_64`, and M3 copied that tree into `packaging\windows\stage`. The staged IDE’s project wizards include SFML, GLFW, and SDL3, and do not include SDL project.

---

## 3. NSIS material that was actually found

**CONFIRMED FROM WEB** (24 October 2026). The public Code::Blocks source mirror does not contain a Windows NSIS script. The directory name `windows_installer` appears only as a documentation input to the help plugin, and the files are absent here.

The useful public description is the January 2022 forum thread [Unofficial Code::Blocks installers for Windows](https://forums.codeblocks.org/index.php?topic=24592.0). It says the 64-bit package was built with stock NSIS 3.7, installed under Program Files, offered a chosen directory and a Start Menu folder, and wrote an uninstaller. The GitHub branch named in that thread (`acotty/codeblocks_sf`, branch `AC-WindowsInstaller`) returns 404, so the script itself was not available. Nothing from it is copied.

Keep from that description: one 64-bit installer, an explicit directory, a Start Menu shortcut, an uninstaller that removes the files it installed, and a command-line `makensis` invocation.

Leave behind: the page that offers a choice of MinGW, MSYS2, TDM, or Cygwin; the optional “open a GDB download page” checkbox; the Inno Setup twin; Fortran and clangd extras; the XP file set; portable mode via `CbLauncher.exe`; and any uninstall choice that deletes the user’s Code::Blocks application data. This tree does not ship `CbLauncher.exe`. The second artefact is a fixed MinGW tree, not that chooser.

Stock NSIS is an external tool on the packager’s machine, the way `zip.exe` already is for `update.bat`. It is not forked and its source is not vendored. No NSIS plugin beyond a stock NSIS 3 install is required. The payload, once the import libraries are omitted, is well under the size that would need a special NSIS build.

---

## 4. Installer behaviour

**PROPOSED DESIGN.**

On Windows this distribution is Code::Blocks. The installer does not use a separate “macOS port” name. Internal application identifiers stay as they are, and the macOS distribution name is not part of this amendment.

| Piece | Name |
|-------|------|
| Default installation directory | `C:\Program Files\CodeBlocks` |
| Start Menu shortcut | `CodeBlocks` |
| Add/Remove Programs display name | `Code::Blocks` |
| Uninstall key | `Software\Microsoft\Windows\CurrentVersion\Uninstall\CodeBlocks` |
| IDE-only installer | `codeblocks-v3.4-win64.exe` |
| Turnkey installer | `codeblocks-v3.4mingw-win64.exe` |

The shortcut file is `CodeBlocks` because a Windows file name cannot contain `:`. The running program already presents itself as Code::Blocks. This amendment does not change `appglobals.cpp`.

`C:\Program Files\CodeBlocks` is the NSIS default (`InstallDir`), not a required path. Both artefacts show the directory page. The user’s choice is `$INSTDIR`. Files are installed there, the shortcut’s target is `$INSTDIR\codeblocks.exe`, and its working directory is `$INSTDIR`. The turnkey tree is `$INSTDIR\MinGW`. Nothing in the script later reconstructs a path from the default string, from `C:`, or from `D:\Program Files\CodeBlocks`. The same rules apply to `D:\Program Files\CodeBlocks`, `D:\CodeBlocks`, `E:\Development\CodeBlocks`, or any other valid directory. Those names are examples, not extra hard-coded locations.

The script requests an administrator install, because the default Program Files location needs one. Per-user install is not this version. One section copies the stage tree into `$INSTDIR`. The license page shows the repository `COPYING` text. The turnkey licence page also carries the toolchain notice from section 2. The About box is left alone; the installer version string is not a change to `appglobals.cpp`.

There is no desktop shortcut, no file association, and no `PATH` edit.

The installer looks only at the directory the user is installing into, and at its own uninstall key. It does not scan the disk for every historical Code::Blocks.

A. The selected directory has no `codeblocks.exe`, and this distribution is not already recorded there. Install normally.

B. This distribution is already installed in that directory: the uninstall key’s InstallLocation is that directory. Reinstall or upgrade in place. Run this distribution’s uninstaller silently, then copy the new files.

C. Code::Blocks exists in some other directory. The installer does not search for it, does not run its uninstaller, and does not modify that directory. Deliberate side-by-side installation is not the aim. Coexistence is simply what happens when the user never selects that other directory.

D. The user selects a directory that already contains `codeblocks.exe`, and this distribution’s uninstall key does not record that directory. The installer asks before replacing it. Only an explicit confirmation overwrites that directory. This is the same rule on C:, on D:, and on any other valid drive. Choosing the default path must not do this silently when that path already holds an official installation.

Silent upgrade runs only for case B. The installer never runs an uninstaller whose InstallLocation is a different directory.

Both artefacts use that same directory, shortcut, display name, and uninstall key. They are two files of one Code::Blocks install, not two products. There is no compiler-choice page. Installing one over the other, when the uninstall key already records that directory, is case B: the directory is replaced. Replacing a turnkey install with the IDE-only installer removes the bundled MinGW, because the whole directory is application-owned. Replacing an IDE-only install with the turnkey installer adds it.

The directory the user selected is application-owned. A `MinGW` directory that the turnkey installer placed inside that directory is part of it. Uninstall recursively removes that selected directory, the Start Menu shortcut, and this uninstall key. It does not keep a manifest of unknown files inside the directory, including when the user previously confirmed an overwrite of an older Code::Blocks in the same place.

Uninstall never recursively removes `%APPDATA%\CodeBlocks`, a user project or source directory, another Code::Blocks directory, or a MinGW that lives outside the selected directory. Detecting or using an external compiler does not make it ours to delete.

**CONFIRMED FROM SOURCE.** User settings are not in the install tree. `ConfigManager::GetUserDataFolder` uses the `APPDATA` environment variable and appends `\CodeBlocks`, which is `%APPDATA%\CodeBlocks` (`default.conf` and the rest of the profile). The application name in `appglobals.cpp` is already `Code::Blocks`, so this distribution and official Code::Blocks share that profile, including when the two programs live on different drives. Sharing it is a requirement. v3.4 does not add a private config directory, and it does not add automatic profile migration, backup, restore, or rollback.

Neither installer reads, creates, writes, copies, renames, migrates, resets, or deletes `%APPDATA%\CodeBlocks`. Uninstall leaves that folder untouched. That is stronger than only refusing to delete it.

Neither the installer nor passive application launch may overwrite, reset, or remove pre-existing user settings. Only deliberate settings changes made by the user may be persisted. A setting this build does not understand must not be silently replaced with a default merely because the profile was opened. Launching and cleanly closing this distribution, without an intentional configuration change, must not remove, reset, or replace pre-existing settings. Both versions remaining launchable is not evidence that those settings were preserved.

Four cases stay separate. The installer modifying the profile is forbidden. A development, staging, packaging, or ordinary launch test modifying the developer’s normal profile is also forbidden. The installed application passively modifying existing settings merely by being launched and closed is a defect, and every such difference is identified and investigated. A deliberate configuration change made by the user may be saved. The plan does not promise that the application will never overwrite settings.

Development, staging, packaging, and ordinary launch tests must not use the developer’s normal `%APPDATA%\CodeBlocks` profile. Except during an explicit shared-profile compatibility test, those launches use a disposable profile through the existing `--user-data-dir` mechanism. The installed shortcut does not gain that flag, and the product still uses `%APPDATA%\CodeBlocks`.

A shared-profile test uses a byte-for-byte copy of a representative existing Code::Blocks profile, never the user’s only real profile. Before the sequence, that copy is recorded or hashed. After each launch and clean close, the copy is compared with that record. Every difference caused by passive launch and exit is identified and investigated. It is not accepted because both IDEs still start. M6 and M10 are those tests. They do not change detection.

A saved compiler master path that still names a `gcc.exe` keeps winning. A saved path whose `gcc.exe` has been removed does not, by itself, fall through to `<new install>\MinGW`. The D: layout test in section 2 is that case. M6 and M10 record what the shared profile does across directories. They do not add a second profile, and they do not change detection.

---

## 5. Building the installer, and running the IDE

These are different machines.

**The packager** needs the existing Windows build (the project file, WinLibs GCC 14.2.0, wx 3.3.4, and `zip` on `PATH`, which is already how `update.bat` runs) and a stock NSIS 3 whose `makensis.exe` is on `PATH`. The usual install puts it at `C:\Program Files (x86)\NSIS\Bin\makensis.exe`. The repository does not download NSIS. For the turnkey artefact the packager also downloads the WinLibs 7z named in section 2, checks it against the `.sha256` published next to it, and extracts it outside the repository. The compiler tree is not committed.

**The user** of the IDE-only installer needs Windows 10 or later, because this binary is the UCRT build. They do not need wx, the source tree, or NSIS. They need a MinGW only when they want to compile or debug, and the IDE finds it the way section 2 already describes. **The user** of the turnkey installer does not install a compiler separately. v3.4 does not add a toolchain page inside Code::Blocks.

The packager’s command, once the scripts exist, is `packaging\windows\build-installer.bat`. That batch refreshes the IDE stage from `output33_64`, builds the turnkey stage from that same IDE stage plus the checked toolchain, and runs `makensis` twice. It writes `codeblocks-v3.4-win64.exe` and `codeblocks-v3.4mingw-win64.exe`. It does not open the NSIS GUI. CI is not part of this version. The batch is the thing a later CI job would call.

---

## 6. Files

| File | Role |
|------|------|
| `packaging\windows\stage.bat` | **New.** Copy a fresh `src\output33_64` to `packaging\windows\stage`, skipping `*.a`, `*.bak`, `*.prev`, and `*.RPT`. Fail if `codeblocks.exe`, `codeblocks.dll`, `cb_console_runner.exe`, `wxmsw334u_gcc_custom.dll`, or `debugger.dll` is missing. Fail if `gcc.exe` or `gdb.exe` is present. Run the `objdump -p` check from section 2. |
| `packaging\windows\stage-mingw.bat` | **New.** Copy `stage` to `stage-mingw`, then copy the checked toolchain root (the archive’s `mingw64` directory) to `stage-mingw\MinGW`. Fail unless the SHA-256 matches section 2 and `version_info.txt` names `gcc-14.2.0-mingw-w64ucrt-12.0.0-r3`, and `MinGW\bin` contains `gcc.exe`, `g++.exe`, and `gdb.exe`. Do not read `D:\Program Files\CodeBlocks`. |
| `packaging\windows\codeblocks.nsi` | **New.** Name, version, directory, license page, `File /r` of the given stage, Start Menu shortcut, uninstaller. No compiler page. One script, two invocations: the IDE stage writes `codeblocks-v3.4-win64.exe`; the turnkey stage writes `codeblocks-v3.4mingw-win64.exe`. The turnkey license page adds the toolchain notice from section 2. |
| `packaging\windows\build-installer.bat` | **New.** Run `stage.bat`, then `stage-mingw.bat`, then `makensis` for each artefact. |

Not modified: `src\update.bat`, debugger sources, wizard scripts, `winsdk.cpp`, compiler detection, macOS bundling, `appglobals.cpp`. `src\CodeBlocks_wx33_64.cbp` gained the existing `plugins/debuggergdb/nativewatchsummary.cpp` and `nativewatchsummary.h` on the Debugger target so that target links. `packaging\windows\stage\`, `stage-mingw\`, and the extracted toolchain are generated and are not committed.

The new directory is `packaging\windows\` rather than `src\windows_installer\`. The latter name is already wired to help-plugin documentation that this tree does not contain. Putting an NSIS script there would mix packaging with that missing documentation input.

---

## 7. Milestones

M1 is this document. No installer, no stage script, no change to the IDE. It records the two trees, the payload split, the missing upstream NSIS script, the install and uninstall rules, and the one WinLibs archive the turnkey installer uses.

| | Objective | Files | Non-goals | Check | Stop |
|--|-----------|-------|-----------|-------|------|
| M1 | **DONE** (plan). Record the runtime split, the NSIS reference that could actually be read, the install rules, and the turnkey toolchain identity. | This plan only. | No `makensis`, no new directory, no edit to `update.bat`. The 7z is not committed. | The statements marked **CONFIRMED FROM SOURCE** match `devel33_64`, `output33_64`, `update.bat`, `compilerMINGW.cpp`, `debuggermanager.cpp`, and `configmanager.cpp`. The turnkey archive is the non-LLVM 7z in section 2, including its SHA-256. | Met by this file. The community `.nsi` was not recoverable (404). Corresponding source for that 7z is still open, and it blocks M9. |
| M2 | **DONE.** A fresh `output33_64` matches the IDE that has been tested. | The existing `nativewatchsummary.cpp` and its header are now Debugger units in `CodeBlocks_wx33_64.cbp`. No new source. Rebuilt the All target from `D:\Program Files\CodeBlocks\codeblocks.exe` and ran `src\update.bat 33_64`. | `update.bat` was not edited. `devel33_64` was not packaged. | `output33_64\share\CodeBlocks\plugins\scriptedwizard.dll` is 565,774 bytes (24 October 2026 19:48), stripped from the current devel plugin, not the 474 KB file. `templates\wizard` contains `sfml`, `glfw`, and `sdl3`, and does not contain `sdl`. `debugger.dll` links with `TryNativeWatchSummary` defined. | The missing Debugger unit was the stop named here. It is now declared. |
| M3 | **DONE.** A stage directory that is only the application, and the only IDE payload both installers use. | `packaging\windows\stage.bat` | No NSIS script yet. The `*.a` files remain in `output33_64`. No MinGW in this stage. | The stage has `codeblocks.exe`, `codeblocks.dll`, `cb_console_runner.exe`, `wxmsw334u_gcc_custom.dll`, and `debugger.dll`. It has no `*.a`, no `gcc.exe`, and no `gdb.exe`. `stage.bat`’s `objdump -p` check passed. With `PATH` set to System32 plus the stage, the window opened as `Start here - Code::Blocks svn build`. The registered project wizards include `GLFW project`, `SDL3 project`, and `SFML project`, and do not include `SDL project`. | No missing non-OS DLL. The stage was not widened to `devel33_64`. |
| M4 | The IDE-only installer from `makensis`. | `codeblocks.nsi`, `build-installer.bat` | No desktop shortcut, no file associations, no `PATH` change, no compiler page, no per-user install. No second Windows product name. This artefact contains no GCC and no GDB. Do not hard-code `C:\Program Files\CodeBlocks` as the only install path. | `build-installer.bat` produces `codeblocks-v3.4-win64.exe` without the NSIS GUI. The default `$INSTDIR` is `C:\Program Files\CodeBlocks`. The directory page accepts another directory. The shortcut and the uninstaller use `$INSTDIR`. The Start Menu shortcut is `CodeBlocks`. The uninstall key is `CodeBlocks`. It does not modify a Code::Blocks directory the user did not select. It does not read, create, write, copy, rename, migrate, reset, or delete `%APPDATA%\CodeBlocks`. Overwriting `codeblocks.exe` in the selected directory happens only after an explicit confirmation, unless that directory is already this distribution, which upgrades in place. | Stop if stock NSIS 3 cannot pack the stage. Do not switch to Inno Setup inside this version; record the failure. |
| M5 | Install and launch the IDE-only artefact on a machine that does not have this repository, wx, or MSYS2. One of the installs is not on the default C: path. | No further product code if M4’s payload was complete. | Do not require a compiler for this milestone. Do not install under a separate product name. | Case A, with no GCC and no GDB for Code::Blocks to find: the Start Menu shortcut `CodeBlocks` starts the IDE from the directory the user selected, including one pass on another drive. The shortcut target is that directory. The window already presents Code::Blocks. The manager, resources, and Start Here page appear. The same wizard list as M3 is present. Quit and start again. Missing a compiler does not stop the IDE from opening. The launch uses `--user-data-dir` and a disposable directory, so it does not open the developer’s normal profile. The shortcut itself is not given that flag. | A missing resource or DLL is a stage-list fix, not a wizard or debugger change. A failure only on a non-C: directory is a packaging path bug. It is not a source change unless section 2’s relative detection is shown to be wrong. |
| M6 | Build and debug still work after strip, with an external toolchain, on the IDE-only installer. The shared profile still works in both directions. | None, unless M5 found a missing file. | Do not produce the turnkey artefact in this milestone. Do not change debugger or compiler detection to make a stripped binary pass. Do not add a private profile or a migration if no concrete break has been seen. | On that machine, with the WinLibs GCC 14.2.0 UCRT the IDE already knows how to detect: build and run a small C++ file; debug it, including Step into; debug one file with no project; Stop ends the program and the session; a `std::string` watch shows its text on the variable row. On a byte-for-byte copy of a representative profile, never the user’s only real profile: hash or record the copy, then official Code::Blocks, then this distribution, then official Code::Blocks, then this distribution, comparing the copy after each clean close. Launch and close without an intentional settings change must not remove, reset, or replace a pre-existing setting, and must not replace an unrecognised setting with a default. Every passive difference is identified and investigated. Both copies remaining launchable is not that check. The compiler and debugger settings the session actually used still work in both directions. A deliberate change made during the build and debug part of the test may persist, and it is recorded as deliberate. | If strip broke Stop or the watch summary, the fix is a rebuild of the plugin that already contains the fix, or a packaging omission. It is not a new debugger feature. If a passive launch removes, resets, or replaces a pre-existing setting, stop and record that difference. Do not add a second config directory, or a migration, inside this version. |
| M7 | Uninstall removes the application directory and leaves the user alone. | The uninstaller section of `codeblocks.nsi`, if M4’s draft was short of this check. | Do not read, write, or delete `%APPDATA%\CodeBlocks`. Do not add a “also delete my settings” checkbox. Do not keep a manifest of extra files inside the install directory. Do not delete a MinGW that is outside the installation directory. | After uninstall of either artefact, the installation directory is gone, including a bundled `MinGW` when the turnkey installer put it there, and the Start Menu shortcut is gone. On the test account, which is not the developer’s only real profile, the hash of `%APPDATA%\CodeBlocks` is unchanged, and the user’s source file is still there. A Code::Blocks directory that was not this installation directory is unchanged. An external compiler the IDE had detected is unchanged. | Met only when the application directory is gone and user-owned and externally owned files are kept. |
| M8 | The turnkey stage is the M3 IDE stage plus the non-LLVM archive from section 2. | `stage-mingw.bat` | Do not commit the 7z or the extracted tree. Do not copy `D:\Program Files\CodeBlocks\MinGW`. Do not prune the archive. Do not switch back to the LLVM 7z. Do not change detection so a different layout would work. | The downloaded 7z matches SHA-256 `0475a96b4d687a3b85dd979a982883ed9172c55b2516e953664232d46471096f`. `stage-mingw\MinGW\version_info.txt` names `gcc-14.2.0-mingw-w64ucrt-12.0.0-r3`. `gcc -dumpversion`, `gcc -dumpmachine`, and `gdb --version` match section 2. `stage\` still has no `gcc.exe`. | Stop if the archive identity does not match section 2. Do not invent a hand-pruned tree. |
| M9 | The turnkey installer. | The second `makensis` invocation in `build-installer.bat` | No compiler-choice page. No global `PATH` edit. No second uninstall key. No hard-coded install drive. | Blocked until section 2’s corresponding-source method is written down. After that, `codeblocks-v3.4mingw-win64.exe` is produced from `stage-mingw`. Its default directory, directory page, shortcut, and uninstall key match M4. `MinGW` is under `$INSTDIR`. | Do not build this artefact while the GPL source duty is unresolved. If stock NSIS 3 cannot pack the stage, record that. Do not switch installer technology inside this version. |
| M10 | Clean-machine Build, Run, and Debug with the bundled toolchain only, from a directory that is not the default C: path. | None, unless M9’s payload was short a file. | Do not let `PATH` or another install supply GCC or GDB. Do not change compiler or debugger detection if the bundled `MinGW` is not selected on a clean profile. | No pre-existing GCC or GDB that Code::Blocks can discover. Install `codeblocks-v3.4mingw-win64.exe` into a non-default directory on another drive. The selected `gcc.exe`, `g++.exe`, and `gdb.exe` are `<that directory>\MinGW\bin\`. Create a simple console C++ project. Build, run, debug, hit a breakpoint, step, inspect a scalar and a `std::string`, and Stop. Debug one file with no project. Repeat the M6 profile sequence on that hashed copy, not on the developer’s only profile, including a profile whose saved compiler path pointed at a bundled `MinGW` that was then removed. The section 2 result already says that stale path does not retarget itself. | A failure to find `<selected install>\MinGW` on a clean profile is a staging mistake. It is not a detection redesign. A profile break is recorded, not patched with a second config directory. |

---

## 8. Acceptance

1. **Packager.** From a freshly built `output33_64` and the checked WinLibs 7z, `packaging\windows\build-installer.bat` writes `codeblocks-v3.4-win64.exe` and `codeblocks-v3.4mingw-win64.exe` with stock `makensis`. The NSIS GUI is not part of the procedure. The IDE payload is staged once.
2. **IDE stage.** No import library, no compiler, no GDB, no `devel33_64` leftovers. The sanitized-`PATH` launch opens the IDE under `--user-data-dir`, not under the developer’s normal profile.
3. **IDE-only, no compiler.** The test machine does not contain this repository, and Code::Blocks cannot discover a GCC or GDB. `codeblocks-v3.4-win64.exe` is installed once at the default and once at a non-default directory on another drive. The Start Menu shortcut `CodeBlocks` targets the selected directory. The launch under test uses `--user-data-dir` and a disposable directory. Plugins, resources, and wizards load. New Project shows SFML, GLFW, and SDL3, and does not show SDL project. The IDE still opens.
4. **IDE-only, external toolchain.** With a WinLibs GCC 14.2.0 UCRT the existing search can see, a small program builds, runs, and debugs. Standalone debug, Stop, and a `std::string` watch behave as they do in the devel IDE. This is repeated from the non-default directory.
5. **Turnkey.** On a machine with no other GCC or GDB for Code::Blocks to discover, `codeblocks-v3.4mingw-win64.exe` is installed into a non-default directory on another drive. The selected `gcc.exe`, `g++.exe`, and `gdb.exe` are `<that directory>\MinGW\bin\`. A simple console C++ project builds, runs, and debugs: breakpoint, step, a scalar, a `std::string` watch, and Stop. Standalone debug does the same.
6. **Other install directories.** An official Code::Blocks that the user did not select is left unchanged, on whatever drive it occupies. Selecting that directory and confirming the prompt replaces it. The default directory logic does not replace it on its own. Neither artefact deletes a MinGW outside the selected directory.
7. **Shared profile.** The sequence runs on a byte-for-byte copy of a representative profile, never the user’s only real profile. The copy is hashed before the sequence and compared after each clean close: official Code::Blocks, then this distribution, then official Code::Blocks, then this distribution. The installs used are not all in the same directory. A passive launch does not remove, reset, or replace a pre-existing setting, and does not replace an unrecognised setting with a default. Every difference is identified. Both IDEs remaining launchable is not the pass condition. The sequence includes a profile saved while a bundled `MinGW` was the compiler, then that directory is removed and another Code::Blocks is started. A stale master path that no longer contains `gcc.exe` is the section 2 result: the IDE reports that it cannot find the compiler and does not retarget `<new install>\MinGW`. A real break is recorded. It is not patched with a second profile.
8. **Uninstall.** The directory that was actually selected is removed recursively, including a bundled `MinGW`. The shortcut is removed. The installer and the uninstaller do not read or write `%APPDATA%\CodeBlocks`. On the test account, that folder’s hash is unchanged. The user’s files, any other Code::Blocks directory, and any external compiler remain.
9. **Left in place.** `update.bat`, the wizards, debugger behaviour, compiler detection, macOS packaging, and the existing application name in `appglobals.cpp`. The project file’s only change is the Debugger unit for the existing `nativewatchsummary` sources.

---

## 9. What stays rejected

- Redesigning the Windows build, the plugin loader, or compiler detection.
- Editing `update.bat` so that it stops copying `*.a`. The stage filter is enough.
- Packaging `devel33_64`, or copying whichever DLLs happen to make the devel IDE start.
- Bundling SFML, GLFW, or SDL3 inside either installer. v3.3 already installs those libraries into the user profile when the user asks.
- Any MinGW other than the one archive in section 2. That includes the LLVM 7z from the same release, a hand-pruned subset of either archive, MSVC, clang as a selected compiler, MSYS2, Cygwin, TDM-GCC, and a choice of GCC versions.
- A compiler-download page or a toolchain chooser inside the installer or inside Code::Blocks. The two artefacts are the choice.
- Pruning the WinLibs tree, and committing that tree or its 7z.
- Editing `PATH`, or changing compiler or debugger detection so the bundled tree can live somewhere other than `<install>\MinGW`.
- File associations, a desktop shortcut, a `PATH` edit, per-user install, portable mode, and a second config directory.
- A Windows product name that marks this build as a macOS port, and any application-wide or macOS rebrand done for the sake of the installer.
- Detecting, converting, or removing every historical Code::Blocks installation. The installer only applies section 4 to the selected directory and its own uninstall key.
- A manifest that tries to preserve unknown files inside the installation directory.
- Deleting `%APPDATA%\CodeBlocks` on uninstall, including as an option. Reading, creating, writing, copying, renaming, migrating, or resetting that folder from either installer.
- Automatic profile migration, backup, restore, or rollback, and a promise that the application will never overwrite settings.
- Development, staging, packaging, or ordinary launch tests that use the developer’s normal `%APPDATA%\CodeBlocks` profile.
- Treating “both versions still launch” as evidence that pre-existing settings were preserved.
- Forking NSIS, vendoring it, or copying the unrecovered community script.
- Code signing, CI, auto-update, MSIX, installer artwork, and enterprise deployment.
- The tutorial “droplet” or single-wizard executable. That is a different distribution. v3.4 is the normal IDE.

---

## 10. Risks

**The output tree was refreshed in M2.** `scriptedwizard.dll` in `output33_64` is the stripped current plugin. Shipping the tree from before that run would have installed the old 474 KB plugin.

**Strip has not been retested.** Stop, and the `std::string` watch summary, were exercised in the unstripped devel IDE. M6 is the check that the stripped `output33_64` still does both. A failure there is a rebuild or a packaging omission, not a new debugger change.

**The IDE-only installer still has no compiler.** On a clean machine it opens, and Build and Debug wait until a MinGW the existing search can see is installed. That is what M5 and M6 check. The turnkey installer is the other artefact, not a page added to the first one.

**Shared `%APPDATA%\CodeBlocks` is required, and passive launch must not rewrite it.** This distribution and official Code::Blocks use that profile because the application name is already `Code::Blocks`. Neither installer touches the folder. A launch and clean close, with no deliberate settings change, must not remove, reset, or replace what was already there, including a setting this build does not understand. M6 compares a hashed copy after each close. A difference is investigated. Both IDEs still starting is not a pass. A private config directory, and automatic migration or rollback, stay rejected. Uninstall still must not read or write the folder.

**One uninstall key, two directories.** The key is `CodeBlocks`. This installer never runs an uninstaller for a different directory, and it never deletes a directory the user did not select. It also does not invent a second key so that two installs can both appear as separately branded products. If a test shows that writing this key drops a different install out of Add/Remove Programs while leaving its files in place, record that observation. Do not solve it by renaming the Windows product.

**Explicit overwrite removes that directory’s files.** Confirming case D replaces whatever Code::Blocks is in the selected directory, and a later uninstall removes that directory recursively, including a MinGW that lived inside it. On the planning machine, `D:\Program Files\CodeBlocks` also holds the MinGW this IDE detects. Overwriting that directory is allowed only when the user selects it and confirms. The default install does not do it. A MinGW outside the selected directory is not removed.

**A saved compiler path is not retargeted.** A master path that still contains `gcc.exe` wins over `<selected install>\MinGW`, and `PATH` is searched before that folder on a profile whose master path is empty. A master path whose `gcc.exe` is gone is not empty, so `CompilerFactory::LoadSettings` does not auto-detect. The D: test produced `Can't find compiler executable in your configured search paths (for GNU GCC Compiler)...` and left the new install’s `MinGW` unused. M10 records this. v3.4 does not change detection.

**GPL corresponding source is a blocker for the turnkey installer.** The archive, its checksum, and the component licence names are known. The tree does not contain the GCC or GDB licence texts, and a notice is not corresponding source. The official 25.03 MinGW installer does not include that source either. M9 does not build `codeblocks-v3.4mingw-win64.exe` until a real conveyance method is written into this plan. The IDE-only artefact is not waiting on that. Official 25.03 ships the larger LLVM WinLibs tree. v3.4 still uses the smaller intact archive.

**The turnkey tree is 0.815 GiB before compression.** If stock NSIS 3 cannot pack `stage-mingw`, M9 stops and records that. The installer technology does not change inside this version.

**The installer is unsigned.** SmartScreen will warn. Signing is later work.

**The community script is gone.** The forum post is the description that was readable. The branch it pointed at is 404. The script in this repository is written from section 4, not transcribed from that branch.

---

## 11. Done when

Both artefacts exist and pass their tests.

A Windows user can take `codeblocks-v3.4-win64.exe`, install it as Code::Blocks without the source tree, launch it from the Start Menu, build and debug with a compiler this IDE already knows how to find, move between it and official Code::Blocks using the same `%APPDATA%\CodeBlocks` without a passive launch removing or resetting settings that were already there, and uninstall it without either installer reading or writing that folder or their projects.

A Windows user with no compiler installed can take `codeblocks-v3.4mingw-win64.exe`, choose a directory, install Code::Blocks and the known-good MinGW there in one step, and Build, Run, and Debug a basic C++ program without installing a compiler or GDB separately. That directory does not have to be on C:. Uninstall removes the directory that was selected, including its `MinGW`, and leaves settings, projects, other Code::Blocks directories, and any external compiler in place. The turnkey artefact is not done while the GPL source duty in section 2 is still open.
