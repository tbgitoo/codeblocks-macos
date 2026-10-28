# Code::Blocks Windows v4.0 — installable Windows distribution

**Status: M4 done. `codeblocks-v4.0-win64.exe` has been built and has not been installed.** v3.3 is complete and is the baseline. This version does not reopen the SFML, GLFW, or SDL3 wizards. The only compile change is declaring the existing `nativewatchsummary` sources on the Debugger target so that target links. On Windows the package is Code::Blocks, not a side-by-side “macOS port” product, and `%APPDATA%\CodeBlocks` stays the shared profile on purpose. Neither the installer nor a passive launch may overwrite, reset, or remove pre-existing settings in that profile. Only a deliberate settings change made by the user may be persisted. v4.0 produces one installer. It does not embed a compiler. An optional page can download one pinned WinLibs archive from its upstream publisher, check its SHA-256 with Windows PowerShell `Get-FileHash`, and extract it as `$INSTDIR\MinGW`. `certutil.exe` is not that check. The optional compiler step downloads pinned 7-Zip 26.05 `7zr.exe` from its upstream publisher and checks that file's own SHA-256. The installer does not embed `7zr.exe`.

**Question this version answers:** what is the smallest packaging step that lets a Windows user install this already-working build, launch it, and uninstall it, without the source tree, and optionally obtain the known-good MinGW from its upstream publisher during that install?

The desired path is: download an installer, double-click it, install, launch Code::Blocks from the Start Menu, and use the build and debug behaviour this tree already has. `codeblocks-v4.0-win64.exe` is the IDE. The same installer, when the user leaves the compiler option selected and the network works, also places the pinned MinGW at `<selected install>\MinGW`. Packaging an existing runtime is the whole of the work. It is not a Windows port, not a new build system, and not a compiler-management feature.

Upstream baseline remains Code::Blocks trunk **r14050** / IDE **25.03**. The Windows IDE this plan packages is the 64-bit wx 3.3 build produced by `src\CodeBlocks_wx33_64.cbp` and `src\update.bat 33_64`.

Evidence labels: **CONFIRMED FROM SOURCE** (this tree, 24 October 2026), **CONFIRMED FROM LOCAL MACHINE** (24–25 October 2026), **CONFIRMED FROM WEB** (24–25 October 2026), **PROPOSED DESIGN**.

---

## 1. Current state

**CONFIRMED FROM SOURCE.** `packaging\windows` contains `stage.bat`, `codeblocks.nsi`, and `build-installer.bat`. The generated stage and `codeblocks-v4.0-win64.exe` are not committed. `src\plugins\contrib\help_plugin\Makefile.am` names `windows_installer\Documentation`, and that directory is not present. A user who wants this build today clones the tree, builds `CodeBlocks_wx33_64.cbp`, and runs `src\update.bat 33_64`.

`update.bat` writes two trees under `src\`:

| Tree | What it is |
|---|---|
| `devel33_64` | Development tree. Unstripped. `codeblocks.dll` is about 124 MB. It also holds debugger backup/previous files and `codeblocks.RPT`. |
| `output33_64` | Distribution-shaped tree. `update.bat` copies executables and DLLs, then strips `*.exe` and `*.dll`. `codeblocks.dll` is about 6.8 MB. |

`output33_64` was refreshed by M2. `scriptedwizard.dll` there is the stripped current plugin, 565,774 bytes written 24 October 2026 19:48, not the old 474 KB file. The matching unstripped plugin in `devel33_64` is 10,220,466 bytes. `templates\wizard` contains `sfml`, `glfw`, and `sdl3`, and does not contain `sdl`. The All target would not link Debugger until `plugins/debuggergdb/nativewatchsummary.cpp` and its header were declared on that target in `CodeBlocks_wx33_64.cbp`. `update.bat` was not edited. The rebuild was driven from `D:\Program Files\CodeBlocks\codeblocks.exe`.

The official 25.03 tree on the planning machine is `D:\Program Files\CodeBlocks`. That is evidence that a real install may be off C:. It is not a required path. The default is `C:\Program Files\CodeBlocks`; the user may choose another valid directory. The installer does not bundle a compiler, does not copy the official Program Files tree, and does not change a Code::Blocks directory the user did not select.

---

## 2. Application payload and compiler boundary

### Ship

- `codeblocks.exe`, `codeblocks.dll`, `cb_console_runner.exe`
- `wxmsw334u_gcc_custom.dll`, the wx 3.3.4 runtime linked by this build
- `libgcc_s_seh-1.dll`, `libstdc++-6.dll`, `libwinpthread-1.dll`
- `share\CodeBlocks\plugins\` for the fourteen built plugins: `abbreviations.dll`, `astyle.dll`, `autosave.dll`, `classwizard.dll`, `codecompletion.dll`, `compiler.dll`, `debugger.dll`, `defaultmimehandler.dll`, `OccurrencesHighlighting.dll`, `openfileslist.dll`, `projectsimporter.dll`, `scriptedwizard.dll`, `todo.dll`, `xpmanifest.dll`
- Resource zips, lexers, images, scripts, and `templates\wizard` copied by `update.bat`, including `sfml`, `glfw`, and `sdl3`, but not `sdl`
- Crash-handler files copied beside the executable and not stripped: `exchndl.dll`, `dbghelp.dll`, `dbgcore.dll`, `mgwhelp.dll`, `symsrv.dll`, `symsrv.yes`

### Do not ship

- Any `*.a` in the output root, including `libcodeblocks.a`, `libwxscintilla_cb.a`, `libsquirrel.a`, `libtxml.a`, and `libdepslib.a`
- `*.bak`, `*.prev`, `*.RPT`, or anything under `src\.objs33_64`
- `devel33_64`
- `gcc.exe`, `gdb.exe`, or any embedded MinGW tree
- `7zr.exe`; the optional compiler step downloads the pinned file and deletes it before completion

### Compiler discovery

**CONFIRMED FROM SOURCE.** The executable directory is `ConfigManager::GetExecutableFolder()`, based on `GetModuleFileName` of the running `codeblocks.exe`. The drive letter is therefore the drive of that executable.

`CompilerMINGW::AutoDetectInstallationDir` keeps a stored master path that already resolves to `bin\<compiler>`. Otherwise it searches `PATH`, then `<exe>\bin`, then `<exe>\MinGW\bin`, then `MinGW.ini` and other recognised locations. `cbDetectDebuggerExecutable` searches `PATH` for GDB, then `<exe>\MINGW\bin\gdb.exe`, and only then `C:\MinGW\bin`. Windows treats `MinGW` and `MINGW` as the same directory.

No detection code changes. The optional layout is:

```text
<selected install>\MinGW\bin\gcc.exe
<selected install>\MinGW\bin\g++.exe
<selected install>\MinGW\bin\gdb.exe
```

A `gcc.exe` earlier on `PATH` wins. The packaging does not edit `PATH`.

**CONFIRMED FROM LOCAL MACHINE.** A copied IDE at `D:\cb-v40-layout-test` with the toolchain joined as `D:\cb-v40-layout-test\MinGW`, and a `PATH` limited to Windows System32, detected that compiler and built a console C++ program. The program printed `7`.

Auto-detection does not rerun once a master path is stored. If the stored master path later becomes stale, another installation's `<exe>\MinGW` is not automatically selected. This is a recorded planning issue; v4.0 does not redesign compiler detection and does not add a private profile.

### Optional compiler

The only compiler archive this installer may obtain is the non-LLVM archive from WinLibs release `14.2.0posix-12.0.0-ucrt-r3`:

| Field | Value |
|---|---|
| Archive | `winlibs-x86_64-posix-seh-gcc-14.2.0-mingw-w64ucrt-12.0.0-r3.7z` |
| SHA-256 | `0475a96b4d687a3b85dd979a982883ed9172c55b2516e953664232d46471096f` |
| Compressed size | 101,317,610 bytes |
| Extracted size | 875,001,081 bytes (0.815 GiB) |
| URL | `https://github.com/brechtsanders/winlibs_mingw/releases/download/14.2.0posix-12.0.0-ucrt-r3/winlibs-x86_64-posix-seh-gcc-14.2.0-mingw-w64ucrt-12.0.0-r3.7z` |

There is no “latest” URL and no fallback. The installer does not try the LLVM archive, the ZIP twin, another release, or a project-hosted copy.

The 7z contains a top-level `mingw64` directory. After successful verification and extraction, that directory is renamed to `<selected install>\MinGW`. `version_info.txt` begins:

```text
winlibs personal build version gcc-14.2.0-mingw-w64ucrt-12.0.0-r3
```

It identifies GCC 14.2.0, GDB 16.2, MinGW-w64 12.0.0 with UCRT, Binutils 2.44, Make 4.4.1, and additional tools. LLVM is not listed and `clang.exe` is absent. `gcc -dumpmachine` is `x86_64-w64-mingw32`; the thread model is POSIX; GDB reports the WinLibs r3 16.2 build.

Local tests covered C, C++, a GDB breakpoint, `next`, scalar evaluation, a `std::string` value, and stepping into a function. The extracted archive remains intact; v4.0 does not prune its extra tools.

### Pinned extractor

The compiler archive uses a solid 7z method requiring BCJ2/LZMA support. Stock NSIS is not the extractor. The installer downloads one pinned upstream `7zr.exe`:

| Field | Value |
|---|---|
| Version | 7-Zip 26.05, 21 October 2026 |
| File | `7zr.exe` (x86) |
| URL | `https://github.com/ip7z/7zip/releases/download/26.05/7zr.exe` |
| Size | 602,624 bytes |
| SHA-256 | `9bf582b894dfa665f852d7e9112b02290a1c303c2d95a21fcdc41ffcb03143b7` |

The unversioned `https://www.7-zip.org/a/7zr.exe` is not used.

### Licence boundary

The installer conveys this project's IDE. It does not contain or serve the WinLibs archive or `7zr.exe`. The user's computer downloads the exact pinned files from their upstream publishers. This is the operating legal design; embedding, mirroring, caching, modifying, repackaging, or serving either file is outside scope and would require a fresh licence analysis and any applicable source/notice obligations.

The optional page identifies WinLibs, GCC/GDB/Binutils/Make licensing, 7-Zip 26.05, the upstream origins, and the fact that neither payload is contained in this installer. A short notice is not represented as corresponding source.

### Stage checks

Before `makensis`:

1. The stage must contain `codeblocks.exe`, `codeblocks.dll`, `cb_console_runner.exe`, `wxmsw334u_gcc_custom.dll`, and `debugger.dll`.
2. The stage must contain no `*.a`, no `gcc.exe`, and no `gdb.exe`.
3. `objdump -p` on the executable and plugins must not name a missing non-OS DLL.
4. The staged IDE must open with `PATH` restricted to System32 plus the stage directory and with a disposable `--user-data-dir`.

M2 refreshed `output33_64`; M3 staged it; M4 produced `codeblocks-v4.0-win64.exe` with stock NSIS 3.11. The installer has not yet been run.

---

## 3. Download and hashing mechanism

Stock NSIS 3.11 does not provide the required HTTPS download and arbitrary-file SHA-256 combination. This version does not add Inetc, Crypto, or another NSIS plug-in.

`certutil.exe` is rejected. Windows PowerShell 5.1 `Get-FileHash` is used with `-NoProfile -NonInteractive`. The installer invokes an individual command, not a `.ps1`, and does not use `-ExecutionPolicy Bypass`.

The file path and output path are passed through environment variables:

```text
CB_V40_ARCHIVE
CB_V40_HASHOUT
```

The command writes the SHA-256 digest as ASCII. PowerShell must exit 0. After CR/LF removal, the output must be exactly 64 hexadecimal digits and equal the pinned digest case-insensitively. A failure deletes the download and digest output and does not continue.

Downloads use Windows `curl.exe -fL --retry 2`. A 32-bit NSIS process prefers the `Sysnative` copies of `curl.exe` and `powershell.exe` when available and otherwise uses the system-directory copies. NSISdl, Inetc, `tar.exe`, `certutil.exe`, and an embedded hash utility are not used.

The same closed verification procedure applies independently to `7zr.exe` and the WinLibs archive. `7zr.exe` is never executed before its own digest matches, and it is never run on the archive before the archive digest matches.

---

## 4. Installer behaviour

| Piece | Name |
|---|---|
| Default installation directory | `C:\Program Files\CodeBlocks` |
| Start Menu shortcut | `CodeBlocks` |
| Add/Remove Programs display name | `Code::Blocks` |
| Uninstall key | `Software\Microsoft\Windows\CurrentVersion\Uninstall\CodeBlocks` |
| Installer | `codeblocks-v4.0-win64.exe` |
| Successful compiler marker | `$INSTDIR\MinGW\.codeblocks-v40-mingw` |

The default path is not mandatory. Every install, shortcut, compiler, and uninstall operation uses the selected `$INSTDIR`. There is no reconstructed C: path.

The installer requests administrator rights because the default is Program Files. Per-user install is not in scope. It copies the IDE stage, creates a Start Menu shortcut, and writes an uninstaller. There is no desktop shortcut, file association, or `PATH` edit.

One optional page offers a single checkbox, selected by default: download and install the recommended MinGW compiler. The page names the pinned WinLibs archive and `7zr.exe`, states the approximate download and extracted sizes, requires a network connection, and carries the applicable notices. Clearing the checkbox installs only the IDE and performs no download.

The installer examines only the selected directory and its own uninstall key:

- **A. Empty/new selected directory:** install IDE; if selected and no `$INSTDIR\MinGW`, run compiler steps.
- **B. This distribution already recorded in the selected directory:** replace IDE files in place; do not delete existing MinGW; download only if selected and MinGW is absent.
- **C. Another Code::Blocks exists elsewhere:** do not search for, uninstall, or modify it.
- **D. Selected directory contains `codeblocks.exe` but is not recorded as this distribution:** ask before replacing IDE files. Explicit confirmation is required. Existing MinGW is neither deleted nor overwritten.

Silent upgrade applies only to B. The installer never runs an uninstaller whose recorded installation directory differs.

### Compiler transaction

These steps happen only after IDE installation, with the checkbox set and no existing `$INSTDIR\MinGW`. They never touch `%APPDATA%\CodeBlocks`.

Downloads and digest files live in `$PLUGINSDIR`. Extraction is done on the `$INSTDIR` volume in a unique directory created by this run:

```text
$INSTDIR\cb-v40-<id>
```

If the chosen temporary name already exists, stop without deleting it. Never delete a `cb-v40-*` directory this run did not create.

1. Require about 1.1 GiB free on the `$INSTDIR` volume and about 110 MB on `$PLUGINSDIR`'s volume when different.
2. Download the pinned `7zr.exe` using `curl.exe -fL --retry 2`.
3. Verify its SHA-256 exactly. Do not execute it on failure.
4. Download the pinned WinLibs archive.
5. Verify the archive SHA-256 exactly. A verified extractor does not imply a trusted archive.
6. Use the verified extractor to unpack into this run's temporary directory.
7. Require `mingw64\bin\gcc.exe`, `g++.exe`, and `gdb.exe`; require the prescribed beginning of `mingw64\version_info.txt`.
8. Immediately before the final move, refuse to overwrite, merge with, or delete a newly appeared `$INSTDIR\MinGW`.
9. Rename `mingw64` to `$INSTDIR\MinGW`.
10. Write `$INSTDIR\MinGW\.codeblocks-v40-mingw` last, containing the WinLibs SHA-256.
11. Delete `7zr.exe`, the archive, digest files, and the now-empty run-owned temporary directory.

Every failure or cancellation leaves the IDE installed but no partial MinGW, no marker, no downloaded files, and no run-owned temporary directory. It does not delete a pre-existing `MinGW.incomplete` or another run's `cb-v40-*` directory.

### Uninstall

Uninstall removes:

- IDE files written by this installer;
- the Start Menu shortcut;
- this uninstall key;
- `$INSTDIR\MinGW` only when `.codeblocks-v40-mingw` is present.

An unmarked MinGW remains. An external compiler remains. Uninstall never reads or writes `%APPDATA%\CodeBlocks`, project directories, another Code::Blocks directory, or a compiler outside `$INSTDIR`.

### Shared profile

The product deliberately uses `%APPDATA%\CodeBlocks`. The installer must not read, create, write, copy, rename, migrate, reset, or delete that directory. The compiler download also must not touch it.

Development, staging, packaging, and ordinary launch tests use `--user-data-dir` with a disposable profile. The installed shortcut does not carry this option.

Shared-profile compatibility tests use a byte-for-byte copy of a representative profile, never the user's only profile. The copy is recorded or hashed before testing and compared after each clean close. Every passive difference is identified and investigated. A passive launch must not silently remove, reset, or replace pre-existing settings, including settings this build does not understand. A deliberate user change may persist and is recorded as deliberate.

A stale saved compiler master path remains a known behaviour: it does not automatically retarget another installation's MinGW. v4.0 records this and does not redesign detection.

---

## 5. Building and running

### Packager requirements

- The existing Windows build environment: WinLibs GCC 14.2.0, wxWidgets 3.3.4, and `zip` on `PATH`
- A fresh `src\output33_64`
- Stock NSIS 3 with `makensis.exe` on `PATH`

The repository does not download NSIS. Neither WinLibs nor `7zr.exe` is an input to `makensis`.

Run:

```bat
packaging\windows\build-installer.bat
```

The batch refreshes the IDE stage and invokes `makensis` once. It writes:

```text
codeblocks-v4.0-win64.exe
```

CI is not part of this version.

### User requirements

Windows 10 or later. Users do not need wxWidgets, the source tree, or NSIS. Without the optional compiler, compilation/debugging requires a toolchain the existing detection can find. With the checkbox selected and both pinned downloads verified, the user does not install GCC or GDB separately.

---

## 6. Files

| File | Role |
|---|---|
| `packaging\windows\stage.bat` | Done in M3. Copy `src\output33_64` to the stage, filtering import libraries and leftovers; validate required files and dependency closure. |
| `packaging\windows\codeblocks.nsi` | Done in M4. Installer metadata, directory, licence page, optional compiler page, staged IDE, shortcut, compiler transaction, and uninstaller. Writes `codeblocks-v4.0-win64.exe`. |
| `packaging\windows\build-installer.bat` | Done in M4. Run `stage.bat`, then `makensis` once. Do not download or package WinLibs or `7zr.exe`. |

Not modified: `src\update.bat`, debugger sources, wizard scripts, `winsdk.cpp`, compiler detection, macOS bundling, and `appglobals.cpp`. `src\CodeBlocks_wx33_64.cbp` contains the only compile-graph change: the existing `nativewatchsummary.cpp` and header are declared as Debugger target units.

Generated and not committed: `packaging\windows\stage\`, `7zr.exe`, and the WinLibs archive.

---

## 7. Milestones

| Milestone | Objective and current state | Pass condition / stop rule |
|---|---|---|
| M1 | **DONE.** Plan the runtime split, installer rules, and one pinned compiler archive. | Source observations and pinned identities match the recorded evidence. |
| M2 | **DONE.** Refresh `output33_64` from the tested tree. | Current stripped plugins and debugger link successfully; no edit to `update.bat`. |
| M3 | **DONE.** Produce application-only stage. | Required IDE/runtime files present; no import libraries, GCC, or GDB; dependency and sanitised-PATH launch checks pass. |
| M4 | **DONE.** Produce IDE installer with stock NSIS. | `codeblocks-v4.0-win64.exe` is built without embedding compiler/archive/extractor. Installer not yet executed. |
| M5 | Install and launch without repository/wx/MSYS2; include a non-default drive; compiler unchecked or offline. | IDE starts from Start Menu; resources and wizards load; MinGW is absent; disposable profile used for test launch. |
| M6 | Verify stripped Build/Run/Debug with external WinLibs and shared-profile compatibility. | Normal and standalone debugging, Stop, scalar and `std::string` watches work; passive profile differences are absent or fully identified. Stop on unexplained passive setting loss/reset. |
| M7 | Verify uninstall ownership. | IDE and shortcut removed; user profile/projects/other installs/external or unmarked MinGW untouched. |
| M8 | Exercise optional compiler success and every failure path. | Both hashes checked before use; successful tree and marker correct; failures leave IDE but no partial MinGW/downloads/run-owned temp. Stop if pinned `7zr.exe` cannot extract the pinned archive. |
| M9 | Clean-machine Build/Run/Debug using only downloaded toolchain from non-default drive; repeat shared-profile sequence. | Selected compiler/debugger are `$INSTDIR\MinGW\bin`; C++ build/run/debug/standalone debug pass; download itself does not modify profile. |

### M8 failure matrix

Test independent runs for:

- no network;
- 7-Zip URL unavailable;
- truncated `7zr.exe`;
- wrong extractor hash;
- WinLibs download failure;
- wrong archive hash;
- extractor cannot run;
- extraction failure;
- post-extraction validation failure;
- insufficient disk space;
- cancellation;
- `$INSTDIR\MinGW` appearing immediately before final move.

Each run leaves the IDE installed, no partial MinGW, no marker, no downloaded payloads, no run-owned temporary directory, and no profile write.

---

## 8. Acceptance

1. **Packager:** `build-installer.bat` writes `codeblocks-v4.0-win64.exe` with stock `makensis`; no WinLibs/7zr input.
2. **Stage:** application-only, no import libraries/compiler/GDB/development leftovers; sanitised-PATH launch succeeds using disposable profile.
3. **IDE without compiler:** default and non-default drive installs open and load plugins/resources/wizards; no MinGW is created.
4. **External toolchain:** C++ build, run, project debug, standalone debug, Stop, scalar watch, and `std::string` summary pass.
5. **Optional compiler:** both hashes match before use; compiler lands at selected `$INSTDIR\MinGW`; marker is present; downloaded files disappear; C++ build/run/debug passes; failure matrix leaves no partial installation.
6. **Other installs:** unselected Code::Blocks directories and compilers remain untouched. Replacing files in a selected foreign directory requires explicit confirmation and does not delete unmarked MinGW.
7. **Shared profile:** tests use a copy; passive launch/close does not remove/reset/replace prior settings; every difference is identified; stale compiler-path behaviour is recorded rather than patched through a second profile.
8. **Uninstall:** removes installed IDE, shortcut, key, and only marked MinGW; leaves profile, projects, other installations, external compilers, and unmarked MinGW untouched.
9. **Left in place:** `update.bat`, wizard behaviour, debugger behaviour, compiler detection, macOS packaging, and application name. The project file change remains limited to declaring the existing native-watch-summary units.

---

## 9. Rejected scope

- Redesigning the Windows build, plugin loader, compiler detection, or debugger
- Editing `update.bat` to stop copying import libraries; the stage filter is sufficient
- Packaging `devel33_64`
- Bundling SFML, GLFW, or SDL3
- Any compiler other than the one pinned non-LLVM WinLibs archive
- Compiler chooser, multiple GCC versions, LLVM archive, ZIP twin, “latest”, MSVC, MSYS2, Cygwin, or TDM-GCC
- Embedding, mirroring, modifying, serving, or committing WinLibs or `7zr.exe`
- NSISdl, Inetc, `tar.exe`, `certutil.exe`, hash plug-ins, or an embedded hashing executable
- Deleting a temporary/compiler directory not created and owned by the current run
- Editing `PATH`
- File associations, desktop shortcut, per-user install, portable mode, or second config directory
- Rebranding this as a macOS-port-derived Windows product
- Searching for and converting/removing all historical Code::Blocks installations
- Deleting any unmarked MinGW
- Installer access of any kind to `%APPDATA%\CodeBlocks`
- Automatic profile migration, backup, restore, or rollback
- Using the developer's normal profile for routine tests
- Treating “both versions launch” as evidence of settings preservation
- Forking or vendoring NSIS
- Signing, CI, auto-update, MSIX, installer artwork, or enterprise deployment
- Tutorial droplet/single-wizard distribution

---

## 10. Risks

- **Stale output risk:** M2 refreshed `output33_64`; an older tree would ship an older scripted-wizard plugin.
- **Strip risk:** Stop and native `std::string` summary were exercised in devel; M6 must prove the stripped plugin preserves them.
- **Compiler-optional risk:** skipped/failed compiler installation must still leave a usable IDE; M5, M8, and M9 separate these cases.
- **Shared-profile risk:** passive launch must not rewrite prior settings. Tests use a hashed copy after every clean close; unexplained differences stop the milestone.
- **Single uninstall key:** recording this installer may displace another install's Add/Remove Programs entry while leaving its files. Record that outcome; do not solve it through product rebranding in v4.0.
- **Ownership risk:** only a marked MinGW is removable by this installer.
- **Stale master-path risk:** a saved but missing compiler path does not retarget the new local MinGW. Record rather than redesign.
- **Hash risk:** every missing tool, non-zero exit, malformed digest, or mismatch fails closed.
- **Upstream availability risk:** removal/change of either pinned asset leaves the IDE installed without compiler; no mirror or fallback is provided.
- **Extractor cleanup risk:** a crash may leave a run-owned `cb-v40-<id>` directory. A later run must not delete it because the later run did not create it.
- **Conditional licence analysis:** direct upstream fetching is part of the legal boundary. Embedding/mirroring/repackaging requires a new analysis.
- **Unsigned installer:** SmartScreen may warn; signing is later work.
- **No recovered community NSIS script:** the local script follows this plan and is not transcribed from the unavailable branch.

---

## 11. Done when

`codeblocks-v4.0-win64.exe` exists and passes M5–M9 and the acceptance checks above.

A Windows user can install this Code::Blocks build without the source tree, launch it from the Start Menu, build and debug with a compiler the IDE can find, move between this build and official Code::Blocks using the same `%APPDATA%\CodeBlocks` without passive loss/reset of settings, and uninstall without the installer reading or writing that profile or the user's projects. Only a MinGW carrying `.codeblocks-v40-mingw` is removed by uninstall.

A user with no compiler can leave the optional compiler checkbox selected and, when both pinned upstream files download and both SHA-256 checks pass, Build, Run, and Debug a basic C++ program without separately installing GCC or GDB. The selected installation may be on any valid drive. The installer itself contains neither `7zr.exe` nor WinLibs. A failure leaves the IDE installed and no partial MinGW. A pre-existing MinGW is never replaced.

