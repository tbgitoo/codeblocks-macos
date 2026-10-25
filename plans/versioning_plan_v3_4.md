# Code::Blocks Windows v3.4 — installable Windows distribution

**Status: M4 done. `codeblocks-v3.4-win64.exe` has been built and has not been installed.** v3.3 is complete and is the baseline. This version does not reopen the SFML, GLFW, or SDL3 wizards. The only compile change is declaring the existing `nativewatchsummary` sources on the Debugger target so that target links. Amended in planning: on Windows the package is Code::Blocks, not a side-by-side “macOS port” product, and `%APPDATA%\CodeBlocks` stays the shared profile on purpose. Amended again: neither the installer nor a passive launch may overwrite, reset, or remove pre-existing settings in that profile. Only a deliberate settings change made by the user may be persisted. Amended again: v3.4 produces one installer. It does not embed a compiler. An optional page can download one pinned WinLibs archive from its upstream publisher, check its SHA-256 with Windows PowerShell `Get-FileHash`, and extract it as `$INSTDIR\MinGW`. `certutil.exe` is not that check. Amended again: the optional compiler step downloads pinned 7-Zip 26.05 `7zr.exe` from its upstream publisher and checks that file's own SHA-256. The installer does not embed `7zr.exe`.

**Question this version answers:** what is the smallest packaging step that lets a Windows user install this already-working build, launch it, and uninstall it, without the source tree, and optionally obtain the known-good MinGW from its upstream publisher during that install?

The desired path is: download an installer, double-click it, install, launch Code::Blocks from the Start Menu, and use the build and debug behaviour this tree already has. `codeblocks-v3.4-win64.exe` is the IDE. The same installer, when the user leaves the compiler option selected and the network works, also places the pinned MinGW at `<selected install>\MinGW`. Packaging an existing runtime is the whole of the work. It is not a Windows port, not a new build system, and not a compiler-management feature.

Upstream baseline remains Code::Blocks trunk **r14050** / IDE **25.03**. The Windows IDE this plan packages is the 64-bit wx 3.3 build produced by `src\CodeBlocks_wx33_64.cbp` and `src\update.bat 33_64`.

Evidence labels: **CONFIRMED FROM SOURCE** (this tree, 24 October 2026), **CONFIRMED FROM LOCAL MACHINE** (24 October 2026), **CONFIRMED FROM WEB** (24 October 2026), **PROPOSED DESIGN**.

---

## 1. Current state

**CONFIRMED FROM SOURCE.** `packaging\windows` contains `stage.bat`, `codeblocks.nsi`, and `build-installer.bat`. The generated stage and `codeblocks-v3.4-win64.exe` are not committed. `src\plugins\contrib\help_plugin\Makefile.am` names `windows_installer\Documentation`, and that directory is not present. A user who wants this build today clones the tree, builds `CodeBlocks_wx33_64.cbp`, and runs `src\update.bat 33_64`.

That batch writes two trees under `src\`:

| Tree | What it is |
|------|------------|
| `devel33_64` | The tree used while developing. Unstripped. `codeblocks.dll` is about 124 MB. It also holds `debugger.dll.bak-…`, `debugger.dll.prev`, and `codeblocks.RPT`. |
| `output33_64` | The distribution-shaped tree. `update.bat` copies the executables and DLLs across, then strips `*.exe` and `*.dll`. `codeblocks.dll` there is about 6.8 MB. |

`output33_64` was refreshed by M2. `scriptedwizard.dll` there is the stripped current plugin, 565,774 bytes written 24 October 2026 19:48, not the old 474 KB file. The matching unstripped plugin in `devel33_64` is 10,220,466 bytes. `templates\wizard` contains `sfml`, `glfw`, and `sdl3`, and does not contain `sdl`. The All target would not link Debugger until `plugins/debuggergdb/nativewatchsummary.cpp` and its header were declared on that target in `CodeBlocks_wx33_64.cbp`. `update.bat` was not edited. The rebuild was driven from `D:\Program Files\CodeBlocks\codeblocks.exe`.

The official 25.03 tree on the machine used to write this plan is `D:\Program Files\CodeBlocks`. That path shows a real install that is not on C:. It is not a path this installer requires. The default directory is `C:\Program Files\CodeBlocks`. The user can choose another valid directory, on C: or on another drive. The installer does not bundle a compiler and does not copy this Program Files tree. It does not change a Code::Blocks directory the user did not select (section 4).

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
- `gcc.exe`, `gdb.exe`, and any MinGW tree. Neither `devel33_64` nor `output33_64` contains one. **CONFIRMED FROM SOURCE.** The installer does not embed the WinLibs archive and does not stage it.
- `7zr.exe`. The optional compiler step downloads the pinned file in section 2 and deletes it before the installer finishes.

### Where a compiler is found

**CONFIRMED FROM SOURCE.** The install directory used in that search is `ConfigManager::GetExecutableFolder()`, which is `GetModuleFileName` of the running `codeblocks.exe` (`DetermineExecutablePath` in `configmanager.cpp`). The drive letter is the drive of that executable. It is not assumed to be C:.

`CompilerMINGW::AutoDetectInstallationDir` keeps a master path that already points at `bin\<compiler>`. Otherwise it searches `PATH` for a `bin` directory that contains `gcc.exe` or `mingw32-gcc.exe`, then `<exe>\bin`, then `<exe>\MinGW\bin`, then `MinGW.ini` and a few other well-known installs (`compilerMINGW.cpp`). `C:\MinGW` is the default read from `MinGW.ini` only after `<exe>\MinGW\bin\gcc.exe` was not found, and a version-string fallback when the master path is still empty. `cbDetectDebuggerExecutable` searches `PATH` for `gdb`, then `<exe>\MINGW\bin\gdb.exe` (`debuggermanager.cpp`; Windows treats `MinGW` and `MINGW` as the same directory), and only then `C:\MinGW\bin`. An empty debugger path falls through to that search (`debuggeroptionsdlg.cpp`).

No detection code changes. The optional compiler layout is `<selected install>\MinGW\bin\gcc.exe`, `g++.exe`, and `gdb.exe`, whatever directory the user chose. A `gcc.exe` earlier on `PATH` wins over that folder. This packaging does not edit `PATH`.

**CONFIRMED FROM LOCAL MACHINE** (24 October 2026). `codeblocks.exe` was copied to `D:\cb-v34-layout-test`, with this section’s toolchain joined at `D:\cb-v34-layout-test\MinGW`. `PATH` was only `C:\Windows\System32;C:\Windows`. A fresh `--user-data-dir` logged `Checking [C::B\MinGW] master path='D:\cb-v34-layout-test\MinGW'` and `Final MinGW master path='D:\cb-v34-layout-test\MinGW'`. The IDE built a console C++ project with that compiler. The program printed `7`. `GNU GCC Compiler`, `GNU GCC MinGW Compiler`, and `GNU GCC MinGW64 Compiler` all saved that same master path.

Auto-detection does not run again once a master path is stored. `CompilerFactory::LoadSettings` calls it only when `GetMasterPath()` is empty (`compilerfactory.cpp`). After that `D:\cb-v34-layout-test\MinGW` junction was removed, the same profile was opened with `D:\cb-v34-layout-test-b\codeblocks.exe`, whose own `MinGW\bin\gcc.exe` existed. The IDE did not select it. The log said `Can't find compiler executable in your configured search paths (for GNU GCC Compiler)...`. That is a recorded planning issue. v3.4 does not change detection to repair it, and it does not add a private profile.

### Optional compiler

**CONFIRMED FROM LOCAL MACHINE** (24 October 2026). The only compiler this installer may obtain is the non-LLVM archive from [14.2.0posix-12.0.0-ucrt-r3](https://github.com/brechtsanders/winlibs_mingw/releases/tag/14.2.0posix-12.0.0-ucrt-r3):

| | |
|--|--|
| Archive | `winlibs-x86_64-posix-seh-gcc-14.2.0-mingw-w64ucrt-12.0.0-r3.7z` |
| SHA-256 | `0475a96b4d687a3b85dd979a982883ed9172c55b2516e953664232d46471096f` |
| Compressed size | 101,317,610 bytes |
| Extracted size | 875,001,081 bytes (0.815 GiB) |
| URL | `https://github.com/brechtsanders/winlibs_mingw/releases/download/14.2.0posix-12.0.0-ucrt-r3/winlibs-x86_64-posix-seh-gcc-14.2.0-mingw-w64ucrt-12.0.0-r3.7z` |

That URL returned this file, and the file’s SHA-256 matched the table. The release page’s asset list for that tag is this archive, its `.zip` twin, the LLVM variants, and checksum files. There is no “latest” URL in this design. If this URL does not return this exact file, the compiler step fails. It does not try another release, the LLVM archive, the `.zip` twin, or a copy hosted by this project.

The previously measured LLVM tree was 1,533,687,789 bytes (1.43 GiB). This archive is the intact upstream file. It is not a hand-pruned copy. The 7z wraps a `mingw64` directory containing `bin`, `include`, `lib`, `libexec`, `share`, `x86_64-w64-mingw32`, and `version_info.txt`. After a successful check, the installer renames that `mingw64` directory to `<selected install>\MinGW`. **CONFIRMED FROM LOCAL MACHINE.** `7z l -slt` reports `Type = 7z`, `Solid = +`, and `Method = Delta LZMA2:28 LZMA:20 BCJ2`.

`version_info.txt` begins `winlibs personal build version gcc-14.2.0-mingw-w64ucrt-12.0.0-r3`. It lists GCC 14.2.0, GDB 16.2, MinGW-w64 12.0.0 linked with UCRT, Binutils 2.44, Make 4.4.1, and the other tools in that file (CMake, Ninja, Doxygen, Cppcheck, ccache, and the rest). It does not list LLVM. `clang.exe` is absent. `gcc -dumpversion` is `14.2.0`, `gcc -dumpmachine` is `x86_64-w64-mingw32`, the thread model is posix, and `gcc -v` reports `MinGW-W64 x86_64-ucrt-posix-seh, built by Brecht Sanders, r3`. `gdb --version` reports `GNU gdb (GDB for MinGW-W64 x86_64, built by Brecht Sanders, r3) 16.2`. Those compiler and debugger identity strings match the r3 build already in use. The extra tools stay in the tree. v3.4 does not prune them, and it does not fetch the LLVM archive.

**CONFIRMED FROM LOCAL MACHINE.** With this `gcc` and `g++`, a C program printed `c-ok` and a C++ program printed `7`. GDB stopped at a breakpoint in `main`, `next` reached the next lines, `print c` was `2`, and `print a` was a `std::string` with `_M_dataplus` / `_M_p` `"19403"`, `_M_string_length` 5, and `_M_local_buf`. `step` entered `hold`, where `n` was `2` and `s` was `"19403"`. That is the libstdc++ dump the existing watch summary already parses. An IDE click-through of Stop and the Watch window was not repeated in this pass. The IDE build from `D:\cb-v34-layout-test` is the detection check above.

### Licences

Two different duties stay separate. Notices tell the user what they are installing. Corresponding source is the duty that attaches when someone conveys a GPL-covered binary.

**What this installer conveys.** It conveys this project's IDE. It does not contain the WinLibs archive or `7zr.exe`, and it does not serve either file.

**CONFIRMED FROM WEB.** [GPLv3](https://www.gnu.org/licenses/gpl-3.0.txt) section 0: to “convey” a work means any kind of propagation that enables other parties to make or receive copies. Mere interaction with a user through a computer network, with no transfer of a copy, is not conveying. Section 6’s corresponding-source duty applies when you convey a covered work in object-code form. The same “when you distribute the binary” shape is in GPLv2 section 3.

**PROPOSED DESIGN, reading that text.** The user’s computer fetches the pinned archive from the WinLibs release URL above. This installer supplies the URL and the expected SHA-256. It does not transfer the archive’s bytes. On that reading, WinLibs is the party that conveys those binaries to the user, and section 6 does not attach to this project for them. This is a reading of the licence text, not a court ruling. It holds only while all of the following stay true: the installer contains no WinLibs bytes; the download goes directly to that upstream URL; this project does not mirror, cache, modify, or repackage the archive; a failed download is not replaced with another toolchain or with a copy we host. Breaking any of those turns this project into a redistributor, and the corresponding-source duty comes back. That duty is not met by a notice, by `version_info.txt`, or by linking at the [winlibs_recipes](https://github.com/brechtsanders/winlibs_recipes) repository. Those recipes are MIT build scripts. They are not a source bundle for this r3 tree, and the repository has no tag for this release.

The optional page still tells the user, before the download starts, that the compiler is a separate WinLibs package, that GCC, GDB, GNU Binutils, and GNU Make in it are GPL, and that the bytes come from the upstream URL rather than from this project. [winlibs.com](https://winlibs.com/) names those licences, plus MinGW-w64 as the Zope Public License with exceptions, Yasm and NASM as 2-clause BSD, and JWasm as the Sybase Open Watcom Public License. **CONFIRMED FROM LOCAL MACHINE.** The extracted tree also contains other libraries the `version_info.txt` list does not name, including `libreadline8.dll`, `libsource-highlight-4.dll`, and `libpython3.9.dll`, and its only licence files are `lib\python3.9\LICENSE.txt`, a CMake template licence, and `share\man\man7\gpl.7`. There is no GCC or GDB `COPYING` in the tree. The page does not pretend that a short notice is the corresponding source of that archive. The upstream publisher ships the archive in that form; this project does not add a substitute licence file.

**The extractor.** Stock Windows and stock NSIS cannot be relied on to unpack this 7z (section 3). The installer does not embed an extractor. The optional compiler step downloads one pinned `7zr.exe` and hashes it before it runs.

**CONFIRMED FROM WEB** (25 October 2026). The [7-Zip 26.05 download page](https://www.7-zip.org/download.html) links `7zr.exe (x86)` to `https://github.com/ip7z/7zip/releases/download/26.05/7zr.exe`. The release `26.05`, published 2026-10-21 by `ip7z`, lists that asset at 602624 bytes. The same release includes `7z2605-src.7z`. There is no checksum file beside `7zr.exe`. `https://www.7-zip.org/a/7zr.exe` is not used: that name is not tied to one version.

| | |
|--|--|
| Version | 7-Zip 26.05, 2026-10-21 |
| File | `7zr.exe` (x86) |
| URL | `https://github.com/ip7z/7zip/releases/download/26.05/7zr.exe` |
| Size | 602624 bytes |
| SHA-256 | `9bf582b894dfa665f852d7e9112b02290a1c303c2d95a21fcdc41ffcb03143b7` |

**CONFIRMED FROM LOCAL MACHINE** (25 October 2026). The file from that URL is 602624 bytes. `Get-FileHash -Algorithm SHA256` produced the digest in the table. `7zr.exe` prints `7-Zip (r) 26.05 (x86)`. Its codec list includes BCJ2, Delta, LZMA2, and LZMA. `7zr l` opened the pinned WinLibs archive and reported `Method = Delta LZMA2:28 LZMA:20 BCJ2`. The console banner also says `Public domain`. That banner is not the licence.

**CONFIRMED FROM WEB.** [7-Zip license.txt](https://www.7-zip.org/license.txt) says files other than `7z.dll` are under the GNU LGPL, and that redistributions in binary form must reproduce the related licence information from that file. The unRAR restriction in the same file is described for some code in `7z.dll`. The [7-Zip FAQ](https://www.7-zip.org/faq.html) says an application that uses the 7-Zip EXE must state that parts of 7-Zip were used, that 7-Zip is GNU LGPL, and must link to `https://www.7-zip.org/`, where the source can be found.

**PROPOSED DESIGN, reading that text.** This installer does not redistribute `7zr.exe`. The user's computer fetches that exact URL. On that reading, the binary-redistribution sentence in `license.txt` does not attach to this project, for the same reason section 2 gives for the WinLibs archive: this project does not transfer the bytes. This is a reading of the licence text, not a ruling. It holds only while the installer contains no `7zr.exe` bytes and this project does not mirror, cache, modify, or serve that file. If a later version embeds, mirrors, modifies, or serves `7zr.exe`, that version has to reproduce the related text from `license.txt` and meet the FAQ conditions above. The optional compiler page, before either download, says that `7zr.exe` is 7-Zip 26.05, that it is GNU LGPL, and that it comes from the URL in the table rather than from this installer. The IDE licence page shows this repository's `COPYING` text. It does not carry a 7-Zip redistribution notice.

### Official 25.03 installers

**CONFIRMED FROM LOCAL MACHINE** (24 October 2026). The files were downloaded from the SourceForge 25.03 Windows folder. SHA-256 matches `checkums.txt` there. Both are NSIS-3 Unicode installers (`7-Zip` subtype). The NSIS script inside them is compressed. It was not recovered, and it is not copied here.

| Installer | Measured size | SHA-256 |
|-----------|---------------|---------|
| `codeblocks-25.03-setup.exe` | 52,312,337 bytes | `88bca802df27fd9a9bae77453cf3bca36a7b69cbb3863e62d70a42f204f28df2` |
| `codeblocks-25.03mingw-setup.exe` | 423,494,500 bytes | `8712227526eb3bb26c90dd5c78301b3fa32bf5869a43294bbf4e9c5512c52792` |

The IDE-only installer contains no `MinGW` directory, no `gcc.exe`, and no `gdb.exe`. The MinGW installer contains `MinGW\version_info.txt` for `gcc-14.2.0-llvm-19.1.7-mingw-w64ucrt-12.0.0-r3`, plus `clang.exe`, `cmake.exe`, `ninja.exe`, `doxygen.exe`, and `cppcheck.exe`. `MinGW\bin\gcc.exe` in that installer is byte-identical to `D:\Program Files\CodeBlocks\MinGW\bin\gcc.exe`. Official 25.03 therefore ships the large LLVM WinLibs tree. That is their package. v3.4 does not embed either archive. It does not prune the large tree, and it does not switch to the LLVM archive.

The IDE files in the official installer are a different build from this repository’s stage: wxWidgets 3.2 (`wxmsw32u_gcc_custom.dll`), contrib plugins such as `clangd_client.dll`, and extras such as `CbLauncher.exe`, `cb_share_config.exe`, `cbp2make.exe`, and `Addr2LineUI.exe`. v3.4 still stages this repository’s wx 3.3.4 output. It does not adopt the official file list.

On this machine the official program is installed at `D:\Program Files\CodeBlocks`. With `PATH` limited to `C:\Windows\System32` and `C:\Windows`, a build logged `Final MinGW master path='D:\Program Files\CodeBlocks\MinGW'` and produced a program that printed `7`. A non-C: directory works for that official layout. The default directory string was not readable from the compressed script, so it is not stated here. The Start Menu shortcut is `CodeBlocks\CodeBlocks.lnk`. A desktop shortcut exists too. Both point at that `codeblocks.exe`. Their working directory is the Start Menu folder. Machine `PATH` and user `PATH` have no Code::Blocks entry. `.cbp` is associated with that executable, including Build and Rebuild verbs. v3.4 still does not add a desktop shortcut or a file association.

The uninstall registration is `HKCU\SOFTWARE\Microsoft\Windows\CurrentVersion\Uninstall\CodeBlocks`. `DisplayName` is `CodeBlocks`, `DisplayVersion` is `25.03`, and `UninstallString` is `D:\Program Files\CodeBlocks\uninstall.exe`. There is no `InstallLocation` value. `NoModify` and `NoRepair` are set. It is not an HKLM key. The uninstaller was not run, because this is the working install, and a second install was not started, because it would replace that per-user uninstall key. Our installer still requests an administrator install so the default Program Files location works. It still records the selected directory and removes the files it installed there.

The official MinGW payload’s licence files are `version_info.txt`, `lib\python3.9\LICENSE.txt`, a CMake template licence, and `share\man\man7\gpl.7`. There is no GCC or GDB `COPYING` and no source offer for that r3 build. Official practice is not the conveyance method for v3.4, because v3.4 does not ship that payload.

A machine with no compiler can still open this IDE. Build and debug then use whatever the existing search finds, unless the optional download in section 4 completed.

### How the boundary is checked

Before `makensis` runs, the IDE stage must contain the executables and the wx DLL named above, and must contain no `*.a`, no `gcc.exe`, and no `gdb.exe`. `objdump -p` on `codeblocks.exe` and on each plugin must not name a non-OS DLL that is missing from the stage root. UCRT (`api-ms-win-crt-*`) comes with Windows 10 and is not bundled. Then the staged `codeblocks.exe` is started with a `PATH` of only System32 plus the stage directory. If the IDE opens, the program is not depending on a DLL that happens to sit on the packager’s `PATH`. That launch does not prove that compile or debug works. Those checks are M6 for an external toolchain, and M9 for the optionally downloaded one.

M2 refreshed `output33_64`, and M3 copied that tree into `packaging\windows\stage`. M4 built `codeblocks-v3.4-win64.exe` from that stage with stock NSIS 3.11. The installer was not run. The staged IDE’s project wizards include SFML, GLFW, and SDL3, and do not include SDL project.

---

## 3. NSIS material that was actually found

**CONFIRMED FROM WEB** (24 October 2026). The public Code::Blocks source mirror does not contain a Windows NSIS script. The directory name `windows_installer` appears only as a documentation input to the help plugin, and the files are absent here.

The useful public description is the January 2022 forum thread [Unofficial Code::Blocks installers for Windows](https://forums.codeblocks.org/index.php?topic=24592.0). It says the 64-bit package was built with stock NSIS 3.7, installed under Program Files, offered a chosen directory and a Start Menu folder, and wrote an uninstaller. The GitHub branch named in that thread (`acotty/codeblocks_sf`, branch `AC-WindowsInstaller`) returns 404, so the script itself was not available. Nothing from it is copied.

Keep from that description: one 64-bit installer, an explicit directory, a Start Menu shortcut, an uninstaller that removes the files it installed, and a command-line `makensis` invocation.

Leave behind: the page that offers a choice of MinGW, MSYS2, TDM, or Cygwin; the optional “open a GDB download page” checkbox; the Inno Setup twin; Fortran and clangd extras; the XP file set; portable mode via `CbLauncher.exe`; and any uninstall choice that deletes the user’s Code::Blocks application data. This tree does not ship `CbLauncher.exe`. The optional page in section 4 downloads one pinned archive. It is not that chooser.

**CONFIRMED FROM WEB.** Stock NSIS 3 includes the [NSISdl](https://nsis.sourceforge.io/NSISdl_plug-in) plugin. NSISdl downloads HTTP only. An HTTPS URL is fetched as HTTP, and an HTTP-to-HTTPS redirect times out. The pinned URL is HTTPS on GitHub. NSISdl is not the download mechanism. [Inetc](https://nsis.sourceforge.io/Inetc_plug-in) can fetch HTTPS, and it is not part of a stock NSIS install. This version does not add Inetc or any other download plugin.

**CONFIRMED FROM WEB.** Stock NSIS 3.11 builds these plugins: AdvSplash, Banner, BgImage, Dialer, InstallOptions, LangDLL, TypeLib, Math, nsDialogs, nsExec, NSISdl, Splash, StartMenu, System, UserInfo, and VPatch. None of them computes a SHA-256 of an arbitrary file. [`CRCCheck`](https://nsis.sourceforge.io/Reference/CRCCheck) is a CRC of the installer executable itself. It is not a SHA-256 of the downloaded archive. The separate [Crypto plug-in](https://nsis.sourceforge.io/Crypto_plug-in) can hash a file with `Crypto::HashFile "SHA2"`. Its page describes it as Freeware, distributed as its own download, and it is not in that stock plugin list. This version does not add it, and it does not add another hashing plugin or a hashing executable.

**CONFIRMED FROM WEB.** [certutil](https://learn.microsoft.com/en-us/windows-server/administration/windows-commands/certutil) can print a file hash, and Microsoft Learn says: “Certutil isn't recommended to be used in any production code and doesn't provide any guarantees of live site support or application compatibilities.” It is not the verification mechanism. A successful local run is not a reason to keep it.

**CONFIRMED FROM WEB.** [Get-FileHash](https://learn.microsoft.com/en-us/powershell/module/microsoft.powershell.utility/get-filehash?view=powershell-5.1) is a Windows PowerShell 5.1 cmdlet whose documented purpose is to compute the hash of a file. SHA256 is an accepted algorithm and the default. The `Hash` property is the hexadecimal digest. [Windows PowerShell](https://learn.microsoft.com/en-us/powershell/scripting/install/installing-windows-powershell) is the PowerShell edition that ships in Windows, and 5.1 is its latest version. [about_Execution_Policies](https://learn.microsoft.com/en-us/powershell/module/microsoft.powershell.core/about/about_execution_policies) says the Restricted policy permits individual commands and does not allow scripts. The installer uses one `-Command`. It does not run a `.ps1` file, and it does not pass `-ExecutionPolicy Bypass`.

**CONFIRMED FROM LOCAL MACHINE** (25 October 2026). `C:\Windows\System32\WindowsPowerShell\v1.0\powershell.exe` and `C:\Windows\SysWOW64\WindowsPowerShell\v1.0\powershell.exe` both exist. The System32 copy is Windows PowerShell 5.1.26100.9711. With `-NoProfile -NonInteractive -ExecutionPolicy Restricted`, and with the archive path passed only through an environment variable, `Set-Content -Encoding ascii` of `(Get-FileHash -LiteralPath $env:CB_V34_ARCHIVE -Algorithm SHA256).Hash` wrote 66 bytes: `0475A96B4D687A3B85DD979A982883ED9172C55B2516E953664232D46471096F` followed by CRLF. That is the pinned digest. `C:\Windows\System32\curl.exe` and `C:\Windows\SysWOW64\curl.exe` exist (curl 8.21.0). `C:\Windows\System32\tar.exe` is bsdtar 3.8.9 and could list entries in this 7z. That tar is this operating system’s tar. It is not established for the Windows 10 baseline, and stock NSIS has no 7z extractor. The installer therefore does not use NSISdl, Inetc, `tar.exe`, `certutil.exe`, or an extra hash plugin.

**PROPOSED DESIGN.** The script runs Windows `curl.exe` for the download. After that download has finished, it runs `powershell.exe` from Windows PowerShell, not `pwsh.exe`, with `-NoProfile -NonInteractive`. The archive path and a temporary digest path are environment variables, so the command string does not embed `$INSTDIR` or the temporary directory. The command is `Set-Content -LiteralPath $env:CB_V34_HASHOUT -Value (Get-FileHash -LiteralPath $env:CB_V34_ARCHIVE -Algorithm SHA256).Hash -Encoding ascii`. PowerShell must exit 0. After CR and LF are removed, the digest file must be exactly 64 hexadecimal digits and must equal `0475a96b4d687a3b85dd979a982883ed9172c55b2516e953664232d46471096f` without regard to case. Any other result deletes the archive and the digest file. `7zr.exe` is not started. A 32-bit NSIS process prefers `$WINDIR\Sysnative\curl.exe` and `$WINDIR\Sysnative\WindowsPowerShell\v1.0\powershell.exe` when those paths exist, and otherwise the copies under `$SYSDIR`. The same hash command is used for `7zr.exe` and for the WinLibs archive, each against its own pinned digest. `7zr.exe` runs only after its digest matches. The archive is extracted only after its own digest matches. Neither file is embedded. No hashing program is embedded.

Stock NSIS is an external tool on the packager’s machine, the way `zip.exe` already is for `update.bat`. It is not forked and its source is not vendored. The IDE payload, once the import libraries are omitted, is well under the size that would need a special NSIS build. `makensis.exe` was not present on the planning machine. The packager still supplies stock NSIS 3.

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
| Installer | `codeblocks-v3.4-win64.exe` |
| Marker, only after a successful compiler install | `$INSTDIR\MinGW\.codeblocks-v34-mingw` |

The shortcut file is `CodeBlocks` because a Windows file name cannot contain `:`. The running program already presents itself as Code::Blocks. This amendment does not change `appglobals.cpp`.

`C:\Program Files\CodeBlocks` is the NSIS default (`InstallDir`), not a required path. The installer shows the directory page. The user’s choice is `$INSTDIR`. Files are installed there, the shortcut’s target is `$INSTDIR\codeblocks.exe`, and its working directory is `$INSTDIR`. Nothing in the script reconstructs a path from the default string, from `C:`, or from `D:\Program Files\CodeBlocks`. The same rules apply to `D:\Program Files\CodeBlocks`, `D:\CodeBlocks`, `E:\Development\CodeBlocks`, or any other valid directory. Those names are examples, not extra hard-coded locations.

The script requests an administrator install, because the default Program Files location needs one. Per-user install is not this version. One section copies the IDE stage into `$INSTDIR`. The IDE files are installed before any compiler download starts. The licence page shows the repository `COPYING` text. The 7-Zip notice is on the optional compiler page, as section 2 describes, because this installer does not contain `7zr.exe`. The About box is left alone; the installer version string is not a change to `appglobals.cpp`.

There is no desktop shortcut, no file association, and no `PATH` edit.

One optional page offers a single checkbox, selected by default: download and install the recommended MinGW compiler. The page names the WinLibs archive and the `7zr.exe` file in section 2, says the compiler download is about 101 MB and the extracted tree about 0.8 GiB, says the machine needs a network connection, and carries the WinLibs and 7-Zip notices from section 2. Clearing the checkbox installs the IDE only and does not contact the network. There is no list of compilers.

The installer looks only at the directory the user is installing into, and at its own uninstall key. It does not scan the disk for every historical Code::Blocks.

A. The selected directory has no `codeblocks.exe`, and this distribution is not already recorded there. Install the IDE. If the checkbox is set and `$INSTDIR\MinGW` does not exist, run the compiler steps below.

B. This distribution is already installed in that directory: the uninstall key’s InstallLocation is that directory. Replace the IDE files in place. The checkbox does not delete an existing `MinGW`. If the checkbox is set and `$INSTDIR\MinGW` does not exist, run the compiler steps. If `MinGW` exists, leave it, whether or not the marker is present.

C. Code::Blocks exists in some other directory. The installer does not search for it, does not run its uninstaller, and does not modify that directory. Deliberate side-by-side installation is not the aim. Coexistence is simply what happens when the user never selects that other directory.

D. The user selects a directory that already contains `codeblocks.exe`, and this distribution’s uninstall key does not record that directory. The installer asks before replacing the IDE files there. Only an explicit confirmation overwrites those files. An existing `$INSTDIR\MinGW` is not deleted and is not overwritten. Choosing the default path must not do this silently when that path already holds an official installation.

Silent upgrade runs only for case B. The installer never runs an uninstaller whose InstallLocation is a different directory.

### Compiler steps

These run only after the IDE files are in place, only if the checkbox is set, and only if `$INSTDIR\MinGW` does not already exist. They never read or write `%APPDATA%\CodeBlocks`.

Downloads, digest files, and `7zr.exe` live in `$PLUGINSDIR`, which is outside `$INSTDIR`. The step deletes those files on every path and does not keep them. Extraction is different: a rename onto another volume is not one operation, so the unpack directory is created by this run on the volume of `$INSTDIR`. Its name is `$INSTDIR\cb-v34-<id>`, where `<id>` is chosen by this run. That path is checked before any download. If it already exists, the step stops and does not delete it. The step never deletes `$INSTDIR\MinGW.incomplete` or any `cb-v34-*` directory it did not create.

1. Require about 1.1 GiB free on the volume of `$INSTDIR`, and about 110 MB free on the volume of `$PLUGINSDIR` when that volume is different. If there is not enough space, stop this step, tell the user, and leave the IDE installed.
2. Download the section 2 `7zr.exe` URL with `curl.exe -fL --retry 2` into `$PLUGINSDIR`. A missing `curl.exe`, a refusal, a timeout, an HTTP error, or a cancelled transfer deletes that partial file and stops this step. `7zr.exe` is not started.
3. `Get-FileHash -Algorithm SHA256`, as section 3 specifies, must produce `9bf582b894dfa665f852d7e9112b02290a1c303c2d95a21fcdc41ffcb03143b7` for that file. Any other result deletes the downloaded `7zr.exe` and its digest file and stops this step. `7zr.exe` is not started. There is no warning-and-continue.
4. Download the WinLibs URL from section 2 into `$PLUGINSDIR`. A download failure deletes the partial archive and the downloaded `7zr.exe` and stops this step.
5. `Get-FileHash -Algorithm SHA256` must produce `0475a96b4d687a3b85dd979a982883ed9172c55b2516e953664232d46471096f` for the archive. A verified `7zr.exe` does not make the archive trusted. Any other digest deletes the archive, `7zr.exe`, and the digest files, and stops this step. `7zr.exe` is not started on the archive. The installer does not offer to continue, does not try another WinLibs release or "latest", does not use a mirror hosted by this project, and does not accept a different digest.
6. The verified `7zr.exe` extracts the archive into the unpack directory this run created. The result must contain `mingw64\bin\gcc.exe`, `g++.exe`, and `gdb.exe`, and `mingw64\version_info.txt` must begin `winlibs personal build version gcc-14.2.0-mingw-w64ucrt-12.0.0-r3`. If `7zr.exe` will not run, or extraction or this check fails, delete only the temporary files and the unpack directory this run created. Do not create `$INSTDIR\MinGW` and do not write the marker.
7. Immediately before the move, if `$INSTDIR\MinGW` now exists, do not overwrite it, merge into it, or delete it. Delete only this run's temporary files and unpack directory. Leave the IDE installed and say that the compiler could not be installed because MinGW now exists.
8. Rename `mingw64` to `$INSTDIR\MinGW`. Write `$INSTDIR\MinGW\.codeblocks-v34-mingw` last, containing the WinLibs SHA-256. Then delete this run's temporary files and the empty unpack directory.

A failure or cancellation in this list leaves the IDE usable and does not leave a partial `MinGW`, a marker, `7zr.exe`, the WinLibs archive, or the unpack directory this run created. The installer says what failed. It does not offer another compiler. Re-running the installer later, with the checkbox set and with no `$INSTDIR\MinGW`, tries the downloads again.

### Uninstall

Uninstall removes the IDE files this installer wrote, the Start Menu shortcut, and this uninstall key. It removes `$INSTDIR\MinGW` only when `.codeblocks-v34-mingw` is present. A `MinGW` without that marker is left where it is, including one that was already in the directory before this installer ran. Uninstall does not delete `$INSTDIR\MinGW.incomplete` or a `cb-v34-*` directory. If `$INSTDIR` is empty afterwards, it is removed. Uninstall does not recursively delete a pre-existing `MinGW` in order to remove the parent directory.

Uninstall never reads or writes `%APPDATA%\CodeBlocks`, a user project or source directory, another Code::Blocks directory, or a MinGW that lives outside the selected directory. Detecting or using an external compiler does not make it ours to delete. Clearing the checkbox on a later reinstall does not delete a `MinGW` the earlier run installed; uninstall does, when the marker is present.

**CONFIRMED FROM SOURCE.** User settings are not in the install tree. `ConfigManager::GetUserDataFolder` uses the `APPDATA` environment variable and appends `\CodeBlocks`, which is `%APPDATA%\CodeBlocks` (`default.conf` and the rest of the profile). The application name in `appglobals.cpp` is already `Code::Blocks`, so this distribution and official Code::Blocks share that profile, including when the two programs live on different drives. Sharing it is a requirement. v3.4 does not add a private config directory, and it does not add automatic profile migration, backup, restore, or rollback.

The installer does not read, create, write, copy, rename, migrate, reset, or delete `%APPDATA%\CodeBlocks`. Downloading a compiler does not initialise or rewrite that profile. Uninstall leaves that folder untouched.

Neither the installer nor passive application launch may overwrite, reset, or remove pre-existing user settings. Only deliberate settings changes made by the user may be persisted. A setting this build does not understand must not be silently replaced with a default merely because the profile was opened. Launching and cleanly closing this distribution, without an intentional configuration change, must not remove, reset, or replace pre-existing settings. Both versions remaining launchable is not evidence that those settings were preserved.

Four cases stay separate. The installer modifying the profile is forbidden. A development, staging, packaging, or ordinary launch test modifying the developer’s normal profile is also forbidden. The installed application passively modifying existing settings merely by being launched and closed is a defect, and every such difference is identified and investigated. A deliberate configuration change made by the user may be saved. The plan does not promise that the application will never overwrite settings.

Development, staging, packaging, and ordinary launch tests must not use the developer’s normal `%APPDATA%\CodeBlocks` profile. Except during an explicit shared-profile compatibility test, those launches use a disposable profile through the existing `--user-data-dir` mechanism. The installed shortcut does not gain that flag, and the product still uses `%APPDATA%\CodeBlocks`.

A shared-profile test uses a byte-for-byte copy of a representative existing Code::Blocks profile, never the user’s only real profile. Before the sequence, that copy is recorded or hashed. After each launch and clean close, the copy is compared with that record. Every difference caused by passive launch and exit is identified and investigated. It is not accepted because both IDEs still start. M6 and M9 are those tests. They do not change detection.

A saved compiler master path that still names a `gcc.exe` keeps winning. A saved path whose `gcc.exe` has been removed does not, by itself, fall through to `<new install>\MinGW`. The D: layout test in section 2 is that case. M6 and M9 record what the shared profile does across directories. They do not add a second profile, and they do not change detection.

---

## 5. Building the installer, and running the IDE

These are different machines.

**The packager** needs the existing Windows build (the project file, WinLibs GCC 14.2.0, wx 3.3.4, and `zip` on `PATH`, which is already how `update.bat` runs) and a stock NSIS 3 whose `makensis.exe` is on `PATH`. The usual install puts it at `C:\Program Files (x86)\NSIS\Bin\makensis.exe`. The repository does not download NSIS. `7zr.exe` and the WinLibs 7z are not inputs to `makensis`. Neither file is committed.

**The user** needs Windows 10 or later, because this binary is the UCRT build. They do not need wx, the source tree, or NSIS. With the checkbox cleared, or when the download fails, they need a MinGW only when they want to compile or debug, and the IDE finds it the way section 2 already describes. With the checkbox left on and the network available, they do not install a compiler separately. v3.4 does not add a toolchain page inside Code::Blocks.

The packager’s command, once the scripts exist, is `packaging\windows\build-installer.bat`. That batch refreshes the IDE stage from `output33_64` and runs `makensis` once. It writes `codeblocks-v3.4-win64.exe`. It does not open the NSIS GUI. CI is not part of this version. The batch is the thing a later CI job would call.

---

## 6. Files

| File | Role |
|------|------|
| `packaging\windows\stage.bat` | **Done in M3.** Copy a fresh `src\output33_64` to `packaging\windows\stage`, skipping `*.a`, `*.bak`, `*.prev`, and `*.RPT`. Fail if `codeblocks.exe`, `codeblocks.dll`, `cb_console_runner.exe`, `wxmsw334u_gcc_custom.dll`, or `debugger.dll` is missing. Fail if `gcc.exe` or `gdb.exe` is present. Run the `objdump -p` check from section 2. |
| `packaging\windows\codeblocks.nsi` | **Done in M4.** Name, version, directory, licence page, optional compiler page, `File /r` of the IDE stage, Start Menu shortcut, uninstaller. One `makensis` run writes `codeblocks-v3.4-win64.exe`. The compiler section is the steps in section 4. It does not include `7zr.exe`. |
| `packaging\windows\build-installer.bat` | **Done in M4.** Run `stage.bat`, then `makensis` once. Do not download or pack `7zr.exe` or the WinLibs archive. |

Not modified: `src\update.bat`, debugger sources, wizard scripts, `winsdk.cpp`, compiler detection, macOS bundling, `appglobals.cpp`. `src\CodeBlocks_wx33_64.cbp` gained the existing `plugins/debuggergdb/nativewatchsummary.cpp` and `nativewatchsummary.h` on the Debugger target so that target links. `packaging\windows\stage\` is generated and is not committed. `7zr.exe` and the WinLibs archive are not installer inputs and are not committed.

The new directory is `packaging\windows\` rather than `src\windows_installer\`. The latter name is already wired to help-plugin documentation that this tree does not contain. Putting an NSIS script there would mix packaging with that missing documentation input.

---

## 7. Milestones

M1 is this document. No installer, no stage script, no change to the IDE. It records the runtime split, the missing upstream NSIS script, the install and uninstall rules, and the one WinLibs archive the optional compiler step uses. M1–M4 stay done. The sentence that used to block a second, embedded-compiler installer is replaced by this replan: that second artefact is not built.

| | Objective | Files | Non-goals | Check | Stop |
|--|-----------|-------|-----------|-------|------|
| M1 | **DONE** (plan). Record the runtime split, the NSIS reference that could actually be read, the install rules, and the compiler archive identity. | This plan only. | No `makensis`, no new directory, no edit to `update.bat`. The 7z is not committed. | The statements marked **CONFIRMED FROM SOURCE** match `devel33_64`, `output33_64`, `update.bat`, `compilerMINGW.cpp`, `debuggermanager.cpp`, and `configmanager.cpp`. The optional compiler archive is the non-LLVM 7z in section 2, including its SHA-256 and URL. | Met by this file. The community `.nsi` was not recoverable (404). |
| M2 | **DONE.** A fresh `output33_64` matches the IDE that has been tested. | The existing `nativewatchsummary.cpp` and its header are now Debugger units in `CodeBlocks_wx33_64.cbp`. No new source. Rebuilt the All target from `D:\Program Files\CodeBlocks\codeblocks.exe` and ran `src\update.bat 33_64`. | `update.bat` was not edited. `devel33_64` was not packaged. | `output33_64\share\CodeBlocks\plugins\scriptedwizard.dll` is 565,774 bytes (24 October 2026 19:48), stripped from the current devel plugin, not the 474 KB file. `templates\wizard` contains `sfml`, `glfw`, and `sdl3`, and does not contain `sdl`. `debugger.dll` links with `TryNativeWatchSummary` defined. | The missing Debugger unit was the stop named here. It is now declared. |
| M3 | **DONE.** A stage directory that is only the application. | `packaging\windows\stage.bat` | No NSIS script yet. The `*.a` files remain in `output33_64`. No MinGW in this stage. | The stage has `codeblocks.exe`, `codeblocks.dll`, `cb_console_runner.exe`, `wxmsw334u_gcc_custom.dll`, and `debugger.dll`. It has no `*.a`, no `gcc.exe`, and no `gdb.exe`. `stage.bat`’s `objdump -p` check passed. With `PATH` set to System32 plus the stage, the window opened as `Start here - Code::Blocks svn build`. The registered project wizards include `GLFW project`, `SDL3 project`, and `SFML project`, and do not include `SDL project`. | No missing non-OS DLL. The stage was not widened to `devel33_64`. |
| M4 | **DONE.** The installer from `makensis`, IDE payload only. The optional page may be present, and leaving it clear must not download anything. | `codeblocks.nsi`, `build-installer.bat` | No desktop shortcut, no file associations, no `PATH` change, no compiler chooser, no per-user install. No second Windows product name. The artefact contains no GCC, no GDB, no WinLibs archive, and no `7zr.exe`. Do not hard-code `C:\Program Files\CodeBlocks` as the only install path. Do not add Inetc. | `build-installer.bat` produces `codeblocks-v3.4-win64.exe` without the NSIS GUI. On 25 October 2026 that batch did so with stock NSIS 3.11 `makensis /V2`. The installer is 21,164,857 bytes, smaller than the 101,317,610-byte WinLibs archive, so that archive is not inside. The only `File` instruction is `File /r` of the stage, and that stage has no `7zr.exe`, `gcc.exe`, `g++.exe`, or `gdb.exe`. The installer was not executed. The built file contains no `7zr.exe`. The default `$INSTDIR` is `C:\Program Files\CodeBlocks`. The directory page accepts another directory. The shortcut and the uninstaller use `$INSTDIR`. The Start Menu shortcut is `CodeBlocks`. The uninstall key is `CodeBlocks`. It does not modify a Code::Blocks directory the user did not select. It does not read, create, write, copy, rename, migrate, reset, or delete `%APPDATA%\CodeBlocks`. Overwriting `codeblocks.exe` in the selected directory happens only after an explicit confirmation, unless that directory is already this distribution, which replaces the IDE files in place. An existing `MinGW` in that directory is not deleted. | Stop if stock NSIS 3 cannot pack the stage. Do not switch to Inno Setup inside this version; record the failure. |
| M5 | Install and launch on a machine that does not have this repository, wx, or MSYS2. One of the installs is not on the default C: path. The compiler checkbox is clear, or the machine is offline. | No further product code if M4’s payload was complete. | Do not require a compiler for this milestone. Do not install under a separate product name. Do not let a failed or skipped download block the IDE. | Case A, with no GCC and no GDB for Code::Blocks to find: the Start Menu shortcut `CodeBlocks` starts the IDE from the directory the user selected, including one pass on another drive. The shortcut target is that directory. The window already presents Code::Blocks. The manager, resources, and Start Here page appear. The same wizard list as M3 is present. Quit and start again. Missing a compiler does not stop the IDE from opening. `$INSTDIR\MinGW` was not created. The launch uses `--user-data-dir` and a disposable directory, so it does not open the developer’s normal profile. The shortcut itself is not given that flag. | A missing resource or DLL is a stage-list fix, not a wizard or debugger change. A failure only on a non-C: directory is a packaging path bug. It is not a source change unless section 2’s relative detection is shown to be wrong. |
| M6 | Build and debug still work after strip, with an external toolchain. The shared profile still works in both directions. | None, unless M5 found a missing file. | Do not exercise the optional download in this milestone. Do not change debugger or compiler detection to make a stripped binary pass. Do not add a private profile or a migration if no concrete break has been seen. | On that machine, with the WinLibs GCC 14.2.0 UCRT the IDE already knows how to detect: build and run a small C++ file; debug it, including Step into; debug one file with no project; Stop ends the program and the session; a `std::string` watch shows its text on the variable row. On a byte-for-byte copy of a representative profile, never the user’s only real profile: hash or record the copy, then official Code::Blocks, then this distribution, then official Code::Blocks, then this distribution, comparing the copy after each clean close. Launch and close without an intentional settings change must not remove, reset, or replace a pre-existing setting, and must not replace an unrecognised setting with a default. Every passive difference is identified and investigated. Both copies remaining launchable is not that check. The compiler and debugger settings the session actually used still work in both directions. A deliberate change made during the build and debug part of the test may persist, and it is recorded as deliberate. | If strip broke Stop or the watch summary, the fix is a rebuild of the plugin that already contains the fix, or a packaging omission. It is not a new debugger feature. If a passive launch removes, resets, or replaces a pre-existing setting, stop and record that difference. Do not add a second config directory, or a migration, inside this version. |
| M7 | Uninstall removes the IDE and leaves the user alone, including a `MinGW` this installer did not mark. | The uninstaller section of `codeblocks.nsi`, if M4’s draft was short of this check. | Do not read, write, or delete `%APPDATA%\CodeBlocks`. Do not add a “also delete my settings” checkbox. Do not delete a `MinGW` that has no `.codeblocks-v34-mingw` marker. Do not delete a MinGW that is outside the installation directory. | After uninstall, the IDE files and the Start Menu shortcut are gone. A `MinGW` placed in `$INSTDIR` beforehand, without the marker, is still there. On the test account, which is not the developer’s only real profile, the hash of `%APPDATA%\CodeBlocks` is unchanged, and the user’s source file is still there. A Code::Blocks directory that was not this installation directory is unchanged. An external compiler the IDE had detected is unchanged. | Met only when the IDE files are gone and user-owned and externally owned files, including an unmarked `MinGW`, are kept. |
| M8 | The optional compiler step downloads, checks, and extracts the pinned archive, and failures do not leave a partial tree. | The compiler section of `codeblocks.nsi`. `7zr.exe` is not an input. | Do not embed the WinLibs archive or `7zr.exe`. Do not commit either file. Do not use NSISdl, Inetc, `tar.exe`, or `certutil.exe`. Do not add a hash plugin or a hashing executable. Do not prune the archive. Do not accept another URL, another hash, or the LLVM archive. Do not delete a pre-existing `MinGW.incomplete` or a `cb-v34-*` directory this run did not create. Do not write `%APPDATA%\CodeBlocks`. Do not overwrite an existing `$INSTDIR\MinGW`. | `Get-FileHash` matches the section 2 digest of `7zr.exe` before that file runs, and matches the WinLibs digest before extraction. The verified `7zr.exe` extracts the archive, including its BCJ2 solid method, into the unpack directory from section 4, not into `MinGW.incomplete`. Install into a non-default directory on another drive with the checkbox set. `$INSTDIR\MinGW\version_info.txt` names `gcc-14.2.0-mingw-w64ucrt-12.0.0-r3`. `gcc.exe`, `g++.exe`, and `gdb.exe` are in `$INSTDIR\MinGW\bin`. The marker exists and contains the WinLibs SHA-256. `7zr.exe`, the archive, and the unpack directory are gone. Separate runs, each starting from an IDE install without `MinGW`, cover: no network, the 7-Zip URL unavailable, a truncated `7zr.exe`, a wrong `7zr.exe` hash, a WinLibs download failure, a wrong WinLibs hash, an extractor that will not run, extraction failure, validation failure, not enough disk space, cancellation, and `$INSTDIR\MinGW` appearing before the final move. Each leaves the IDE installed, leaves no `MinGW` and no marker, does not delete a pre-existing `MinGW.incomplete`, and does not write the profile. A pre-existing `MinGW` is left byte-for-byte unchanged when the installer is run again. Re-running with the checkbox clear does not delete a marked `MinGW`. Uninstall removes a marked `MinGW` and does not remove an unmarked one. | Stop if the pinned `7zr.exe` cannot extract this archive. Do not embed it, and do not switch the WinLibs file to its `.zip` twin. Do not host a fallback copy of either file. |
| M9 | Clean-machine Build, Run, and Debug with the downloaded toolchain only, from a directory that is not the default C: path. The shared-profile sequence includes that install. | None, unless M8’s tree was short a file. | Do not let `PATH` or another install supply GCC or GDB. Do not change compiler or debugger detection if `<selected install>\MinGW` is not selected on a clean profile. Do not use the developer’s normal profile. | No pre-existing GCC or GDB that Code::Blocks can discover. Install with the checkbox set into a non-default directory on another drive. The selected `gcc.exe`, `g++.exe`, and `gdb.exe` are `<that directory>\MinGW\bin\`. Create a simple console C++ project. Build, run, debug, hit a breakpoint, step, inspect a scalar and a `std::string`, and Stop. Debug one file with no project. Repeat the M6 profile sequence on that hashed copy, not on the developer’s only profile, including a profile whose saved compiler path pointed at a `MinGW` that was then removed. The section 2 result already says that stale path does not retarget itself. The download does not change the hashed profile by itself. | A failure to find `<selected install>\MinGW` on a clean profile is an installer-path mistake. It is not a detection redesign. A profile break is recorded, not patched with a second config directory. |

---

## 8. Acceptance

1. **Packager.** From a freshly built `output33_64`, `packaging\windows\build-installer.bat` writes `codeblocks-v3.4-win64.exe` with stock `makensis`. The NSIS GUI is not part of the procedure. `7zr.exe` and the WinLibs archive are not inputs. The IDE payload is staged once.
2. **IDE stage.** No import library, no compiler, no GDB, no `devel33_64` leftovers. The sanitized-`PATH` launch opens the IDE under `--user-data-dir`, not under the developer’s normal profile.
3. **IDE, compiler not installed.** The test machine does not contain this repository, and Code::Blocks cannot discover a GCC or GDB. The installer is run once at the default and once at a non-default directory on another drive, with the checkbox clear or the network unavailable. The Start Menu shortcut `CodeBlocks` targets the selected directory. The launch under test uses `--user-data-dir` and a disposable directory. Plugins, resources, and wizards load. New Project shows SFML, GLFW, and SDL3, and does not show SDL project. The IDE still opens. `$INSTDIR\MinGW` does not exist.
4. **External toolchain.** With a WinLibs GCC 14.2.0 UCRT the existing search can see, a small program builds, runs, and debugs. Standalone debug, Stop, and a `std::string` watch behave as they do in the devel IDE. This is repeated from the non-default directory.
5. **Optional compiler.** On a machine with no other GCC or GDB for Code::Blocks to discover, the installer is run with the checkbox set, into a non-default directory on another drive. `Get-FileHash` matches the `7zr.exe` digest before that file runs, and matches the WinLibs digest before the archive is extracted. The selected `gcc.exe`, `g++.exe`, and `gdb.exe` are `<that directory>\MinGW\bin\`. A simple console C++ project builds, runs, and debugs: breakpoint, step, a scalar, a `std::string` watch, and Stop. Standalone debug does the same. The marker is present. `7zr.exe` and the archive are gone. Each failure in M8 leaves the IDE installed and does not leave a partial `MinGW`.
6. **Other install directories.** An official Code::Blocks that the user did not select is left unchanged, on whatever drive it occupies. Selecting that directory and confirming the prompt replaces the IDE files there and does not delete a `MinGW` that lacks the marker. The default directory logic does not replace it on its own. The installer does not delete a MinGW outside the selected directory.
7. **Shared profile.** The sequence runs on a byte-for-byte copy of a representative profile, never the user’s only real profile. The copy is hashed before the sequence and compared after each clean close: official Code::Blocks, then this distribution, then official Code::Blocks, then this distribution. The installs used are not all in the same directory. A passive launch does not remove, reset, or replace a pre-existing setting, and does not replace an unrecognised setting with a default. Every difference is identified. Both IDEs remaining launchable is not the pass condition. The sequence includes a profile saved while a downloaded `MinGW` was the compiler, then that directory is removed and another Code::Blocks is started. A stale master path that no longer contains `gcc.exe` is the section 2 result: the IDE reports that it cannot find the compiler and does not retarget `<new install>\MinGW`. The compiler download itself does not modify the profile. A real break is recorded. It is not patched with a second profile.
8. **Uninstall.** The IDE files that were installed are removed. A marked `$INSTDIR\MinGW` is removed. An unmarked `MinGW` is not. The shortcut is removed. The installer and the uninstaller do not read or write `%APPDATA%\CodeBlocks`. On the test account, that folder’s hash is unchanged. The user’s files, any other Code::Blocks directory, and any external compiler remain.
9. **Left in place.** `update.bat`, the wizards, debugger behaviour, compiler detection, macOS packaging, and the existing application name in `appglobals.cpp`. The project file’s only change is the Debugger unit for the existing `nativewatchsummary` sources.

---

## 9. What stays rejected

- Redesigning the Windows build, the plugin loader, or compiler detection.
- Editing `update.bat` so that it stops copying `*.a`. The stage filter is enough.
- Packaging `devel33_64`, or copying whichever DLLs happen to make the devel IDE start.
- Bundling SFML, GLFW, or SDL3. v3.3 already installs those libraries into the user profile when the user asks.
- Any compiler other than the one archive in section 2. That includes the LLVM 7z from the same release, the `.zip` twin of the pinned archive, a hand-pruned subset, “latest”, MSVC, clang as a selected compiler, MSYS2, Cygwin, TDM-GCC, and a choice of GCC versions.
- A compiler chooser inside the installer or inside Code::Blocks. The one checkbox downloads one pinned archive or it does not.
- Embedding the WinLibs archive or `7zr.exe` in the installer, mirroring either file, or committing either file. The unversioned `https://www.7-zip.org/a/7zr.exe` URL is not the extractor download.
- NSISdl, Inetc, or another download plugin. `curl.exe` is the download.
- `certutil.exe -hashfile`, an NSIS hash plugin, and a separate hashing executable. Windows PowerShell `Get-FileHash` is the SHA-256 check.
- Using this machine's `tar.exe` as the extractor. Pinned `7zr.exe` is the extractor, and only after its own SHA-256 matches. It is downloaded for that step and then deleted.
- Deleting a pre-existing `$INSTDIR\MinGW.incomplete`, or a `cb-v34-*` directory this run did not create.
- Editing `PATH`, or changing compiler or debugger detection so the downloaded tree can live somewhere other than `<install>\MinGW`.
- File associations, a desktop shortcut, a `PATH` edit, per-user install, portable mode, and a second config directory.
- A Windows product name that marks this build as a macOS port, and any application-wide or macOS rebrand done for the sake of the installer.
- Detecting, converting, or removing every historical Code::Blocks installation. The installer only applies section 4 to the selected directory and its own uninstall key.
- Deleting an existing `$INSTDIR\MinGW` that has no `.codeblocks-v34-mingw` marker, during install or uninstall.
- Deleting `%APPDATA%\CodeBlocks` on uninstall, including as an option. Reading, creating, writing, copying, renaming, migrating, or resetting that folder from the installer. Using the compiler download as a reason to touch that folder.
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

**The IDE still installs with no compiler.** Clearing the checkbox, or any failure in the compiler steps, leaves an IDE that opens. Build and Debug then wait until a MinGW the existing search can see is present. M5 checks the skip. M8 checks the failures. M9 checks the successful download.

**Shared `%APPDATA%\CodeBlocks` is required, and passive launch must not rewrite it.** This distribution and official Code::Blocks use that profile because the application name is already `Code::Blocks`. The installer does not touch the folder, including while it downloads a compiler. A launch and clean close, with no deliberate settings change, must not remove, reset, or replace what was already there, including a setting this build does not understand. M6 and M9 compare a hashed copy after each close. A difference is investigated. Both IDEs still starting is not a pass. A private config directory, and automatic migration or rollback, stay rejected. Uninstall still must not read or write the folder.

**One uninstall key.** The key is `CodeBlocks`. This installer never runs an uninstaller for a different directory, and it never deletes a directory the user did not select. It also does not invent a second key so that two installs can both appear as separately branded products. If a test shows that writing this key drops a different install out of Add/Remove Programs while leaving its files in place, record that observation. Do not solve it by renaming the Windows product.

**An unmarked MinGW is not ours.** Confirming case D replaces the IDE files in the selected directory and does not delete a `MinGW` already there. Uninstall removes `$INSTDIR\MinGW` only when the marker is present. On the planning machine, `D:\Program Files\CodeBlocks` also holds the MinGW this IDE detects. Overwriting that directory is allowed only when the user selects it and confirms, and that confirmation still does not delete that MinGW. A MinGW outside the selected directory is not removed.

**A saved compiler path is not retargeted.** A master path that still contains `gcc.exe` wins over `<selected install>\MinGW`, and `PATH` is searched before that folder on a profile whose master path is empty. A master path whose `gcc.exe` is gone is not empty, so `CompilerFactory::LoadSettings` does not auto-detect. The D: test produced `Can't find compiler executable in your configured search paths (for GNU GCC Compiler)...` and left the new install’s `MinGW` unused. M9 records this. v3.4 does not change detection.

**The hash check fails closed.** `Get-FileHash` writes a temporary ASCII digest. `7zr.exe` runs only when its digest is the 64 pinned hex digits in section 2. The WinLibs archive is extracted only when its own digest matches. A missing `powershell.exe`, a non-zero exit, or any other text deletes the downloaded files and leaves the IDE installed. `certutil.exe` is not used. Microsoft Learn says it is not recommended for production code and gives no application-compatibility guarantee.

**The compiler step needs two upstream URLs.** The pinned 7-Zip asset and the pinned WinLibs asset are the only downloads. If either asset is removed or its bytes change, that hash check fails and the IDE remains. This project does not publish a mirror to paper over that. Offline installation of the compiler is not part of this design. Offline installation of the IDE is.

**`7zr.exe` is not in the installer.** The optional page says it is 7-Zip, GNU LGPL, and obtained from the section 2 URL. The IDE licence page does not carry a redistribution notice for a file this installer does not contain. A later version that embeds, mirrors, modifies, or serves `7zr.exe` has to follow `license.txt` and the 7-Zip FAQ. The console banner's "Public domain" line is not that licence. A crash can leave `$INSTDIR\cb-v34-<id>` behind. A later run does not delete that directory, because this run did not create it.

**The GPL reading is conditional.** Section 2 reads GPLv3 section 0 and section 6 as attaching corresponding source to the party who conveys the binary. This installer does not convey the WinLibs archive while it only names the upstream URL and checks the hash. Embedding, mirroring, or substituting the archive would make this project the conveyor, and no corresponding-source bundle for this r3 tree was established. Those actions stay rejected.

**The installer is unsigned.** SmartScreen will warn. Signing is later work.

**The community script is gone.** The forum post is the description that was readable. The branch it pointed at is 404. The script in this repository is written from section 4, not transcribed from that branch.

---

## 11. Done when

`codeblocks-v3.4-win64.exe` exists and passes the tests above.

A Windows user can take that installer, install Code::Blocks without the source tree, launch it from the Start Menu, build and debug with a compiler this IDE already knows how to find, move between it and official Code::Blocks using the same `%APPDATA%\CodeBlocks` without a passive launch removing or resetting settings that were already there, and uninstall it without the installer reading or writing that folder or their projects. Uninstall removes a `MinGW` only when this installer wrote the marker.

A Windows user with no compiler installed can leave the compiler checkbox selected, choose a directory, and, when both pinned upstream files download and each SHA-256 matches, Build, Run, and Debug a basic C++ program without installing a compiler or GDB separately. That directory does not have to be on C:. The installer file itself does not contain `7zr.exe` or the compiler. If either download cannot be completed, the IDE is still installed and no partial `MinGW` remains. A `MinGW` that was already in the selected directory is not replaced.
