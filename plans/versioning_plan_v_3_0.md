# Code::Blocks macOS v3.0 — existing SFML wizard on Apple Silicon

**Status: M2 DONE (tree).** M3+ not started. Locate, `--version`, and `--prefix` only. No formula query, no install, no SFML page.

**Question this version answers:** what is the smallest coherent change that makes the existing SFML project wizard usable in this native Apple Silicon distribution?

Homebrew discovery is not an SFML-only detail. It is a small shared facility for scripted project wizards. SFML is the first and, in v3.0, the only client. The facility is not a package manager.

v3.0 does not add SFML support. The wizard is already registered. v3.0 does not add a second wizard, a game-project type, or a project-system change.

Upstream baseline remains Code::Blocks trunk **r14050** / IDE **25.03**. v2.x console, debugger, standalone-file, and signing behaviour stays as shipped. An SFML project is a normal project.

---

## 1. Existing implementation map

### Entry the user sees

File → New → Project lists wizards from `src/plugins/scriptedwizard/resources/config.script`.

```49:49:src/plugins/scriptedwizard/resources/config.script
    RegisterWizard(wizProject,     _T("sfml"),         _("SFML project"),          _("2D/3D Graphics"));
```

Category **2D/3D Graphics**, title **SFML project**. No `PLATFORM` guard, so the entry is already shown on macOS. The scripted-wizard plugin loads `templates/wizard/sfml/wizard.script` (`src/plugins/scriptedwizard/resources/sfml/Makefile.am` installs `wizard.script`, `logo.png`, `wizard.png`, and the `sfml1/` and `sfml2/` trees).

There is a second, older artifact under `src/templates/common/`: `sfml.template` (“SFML Application”), `sfml.cbp`, `sfml-main.cpp`. That is not the New Project entry named “SFML project”. Its source is the SFML 1 API (`LoadFromFile`, `IsOpened`) and its `.cbp` links `sfml-audio`, `sfml-graphics`, `sfml-network`, `sfml-system`, and `sfml-window` with no include or library directories. Leave it alone in v3.0.

### Wizard pages (`wizard.script` `BeginWizard`)

1. Info page.
2. Version list: **SFML 1.6** or **SFML 2.0**. Default `SFMLVersion <- 1`, which selects 2.0.
3. Static vs dynamic: **only if `PLATFORM == PLATFORM_MSW`**. On macOS the global stays `SFMLLinkType <- 1` (dynamic).
4. Project path page (name and directory). Shared with every project wizard.
5. SFML location page: **only if `PLATFORM == PLATFORM_MSW`**. Default text `$(#sfml)`, checked on leave for `include/SFML/{Config,Graphics,System,Window}.hpp` and libs `sfml-audio`, `sfml-graphics`, `sfml-network`, `sfml-system`, `sfml-window`. Debug libs (`sfml-window-d` or `sfml-window-s-d`) set `SFMLDebugLib`.
6. Compiler page (debug/release output dirs). Default output dirs are `bin/Debug` and `bin/Release` (`wiz.cpp` and `wizpage.cpp`).

`PLATFORM_MAC` is squirrel constant `2` (`src/sdk/scripting/bindings/sc_consts.cpp`).

### Files copied

`GetFilesDir()` returns `sfml/sfml2` when `SFMLVersion == 1`, otherwise `sfml/sfml1`. `wiz.cpp` copies every file in that directory into the new project directory and adds them to every target. Installed files:

| Tree | Sources |
|------|---------|
| `sfml/sfml2/` | `main.cpp`, `cb.bmp` |
| `sfml/sfml1/` | `main.cpp`, `cb.bmp` |

No generated makefile is written by the script. Code::Blocks generates the build from the project options.

### Settings `SetupProject` writes

Platform branch, then shared target setup.

| Platform | What the branch adds |
|----------|----------------------|
| Windows | Include dir, lib dir, `mingw32`, `user32`, `gdi32`, `winmm`, `dxguid`, and `-DSFML_STATIC` when the user chose static. |
| macOS | Linker option `-framework SFML` only. The `_GNU_SOURCE` / `_THREAD_SAFE` line is commented out. No include dir. No lib dir. |
| Other Unix | Nothing. The `pkg-config sfml` lines are commented with “once SFML gets a sfml.pc”. |

After that branch, **both targets on every platform** get link libraries. Dynamic (the macOS default): `sfml-graphics`, `sfml-window`, `sfml-system`. Static: those names with `-s`, plus `sfml-main`. Debug appends `-d` only when the Windows page found debug libs. `sfml-audio` and `sfml-network` are verified on Windows and are **not** linked. That matches the sample, which only uses graphics.

Debug target: `ttConsoleOnly`, output `bin/Debug/<name>`, debug symbols. Release target: `ttExecutable`, output `bin/Release/<name>`, optimisations. The script never sets a working directory. Run (`compilergcc.cpp`) uses the target working directory and, when it is empty, resolves it against the project directory. `cb.bmp` is copied into the project directory, not into `bin/Debug`.

`AddLinkLib` stores a library name (`compileoptionsbase.cpp`). The compiler plugin turns that into `-l`. `-framework SFML` is stored as a raw linker option.

### What must stay unchanged

- Windows pages, `$(#sfml)` / `$(#sfml.include)` / `$(#sfml.lib)`, static vs dynamic, `-d` / `-s` names, and Win32 system libs.
- The commented Unix `pkg-config` branch. Do not enable it globally. Upstream left it off, and a single `sfml.pc` is not how current SFML ships modules.
- `config.script` registration and the other graphics wizards (SDL2, GLUT, GLFW, …).
- `src/templates/common/sfml*`.
- `common_functions.script` `VerifyLibFile` / `SilentVerifyLibFile`. They look for `.a`, `.lib`, `.dll.a`, `.la`, `.so`, and not `.dylib`. Changing that helper would affect every wizard. A macOS SFML check can look for the dylib next to the script instead.

SDL2 and GLUT show the intended shape: a macOS branch inside the same script (`-framework SDL2`, `-framework GLUT`), with Windows and Unix left on their own paths. SFML’s macOS branch is the same idea, aimed at an `SFML.framework`, and then the shared `AddLinkLib` calls still run. GLUT puts its link lines **inside** the platform tests, so it does not double-link. SFML does not.

### Global variables and lib_finder

`$(#sfml)` is the existing Code::Blocks global-variable mechanism. It is only consulted from the Windows page. Nothing in this tree defines an `sfml` variable for macOS.

`src/plugins/contrib/lib_finder/` can attach `pkg-config` results to a project from the build-options UI. That is a manual library picker, not the New Project path. It also shells out to `pkg-config` on the IDE’s PATH. A Finder-launched `.app` does not inherit a shell profile, so Homebrew’s `pkg-config` is not a reliable beginner mechanism. Do not route v3.0 through lib_finder.

---

## 2. Current macOS behaviour

SFML is **not** installed on the machine used for this investigation (`/opt/homebrew/include/SFML` absent, `Cellar/sfml` absent). Nothing was installed. Behaviour below is what the script would write. It was not executed inside the IDE, and it was not compiled.

### What the wizard would generate

Assuming the user keeps the defaults (SFML 2.0, both targets, default output dirs) and names the project `Game` in `~/Game/`:

```text
~/Game/Game.cbp
~/Game/main.cpp          copied from sfml/sfml2/main.cpp
~/Game/cb.bmp
```

Project-wide linker option: `-framework SFML`.

No SFML include directory. No SFML library directory.

Debug target `bin/Debug/Game`: `-lsfml-graphics -lsfml-window -lsfml-system`, console subsystem, debug symbols.

Release target `bin/Release/Game`: the same three `-l` names, GUI subsystem, optimisations.

No `-DSFML_STATIC`, no `sfml-main`, no `-d` suffix, no audio, no network. Compiler is whatever the compiler page selects; the script does not override it.

`main.cpp` is the SFML 2 sample: `sf::VideoMode(800, 600)`, `sf::Texture::loadFromFile("cb.bmp")`, `sf::Event` with `event.type == sf::Event::Closed`, `pollEvent`, `isOpen`, `clear` / `draw` / `display`.

### Confirmed from the script

- The macOS branch never asks where SFML is and never records an include or library directory.
- It always adds `-framework SFML`.
- It also links `sfml-graphics`, `sfml-window`, and `sfml-system` because those calls sit outside the platform branch (`wizard.script` around the Debug and Release blocks).
- Apple’s linker looks for `SFML.framework` on the framework search path. A Homebrew install is dylibs (`libsfml-*.dylib`) plus headers under `include/SFML`, not that framework. **Assumption:** the SFML Mac framework package is the layout this `-framework SFML` line was written for. It is not the layout `brew install sfml` produces.
- Default Apple Clang does not search `/opt/homebrew/include` or `/opt/homebrew/lib`. Without search paths, `#include <SFML/Graphics.hpp>` and `-lsfml-graphics` fail even if Homebrew SFML is present.
- `cb.bmp` is not copied to the output directory and there is no post-build copy on macOS. A working directory left empty is resolved from the project directory at Run time, which is where `cb.bmp` sits. Launching `bin/Debug/Game` from Finder uses that binary’s directory and would not find `cb.bmp`. That second case is a consequence of the relative filename in the sample, not something the wizard tries to fix on any platform.

### Unverified (do not treat as requirements)

- The exact compiler and linker diagnostics, because the wizard was not run and SFML was not installed.
- Whether current Homebrew `sfml` is SFML 3. Upstream SFML 3 changed `VideoMode`, `Event`, and `pollEvent`. If the formula is SFML 3, the shipped `sfml2/main.cpp` will not compile against it. Confirm the formula at implementation time. Do not download it for planning.
- Homebrew dylib install names. **Assumption, typical of Homebrew:** they are absolute (`/opt/homebrew/lib/libsfml-….dylib`), so a successfully linked binary finds them with no rpath, including when started from `bin/Debug`, until that prefix is removed. If an install uses `@rpath` instead, Run from the output directory fails until an rpath is added. Check `otool -D` on the real dylibs before adding any rpath.
- Whether a release `ttExecutable` target on this port skips Program Console. The debug target is `ttConsoleOnly`, so the v2.0 console path applies to Debug. The sample prints nothing. No SFML-specific console or debugger defect was found.
- `codesign` / Gatekeeper behaviour of a student SFML binary. Out of scope. Do not reuse GDB signing.

### v2.x does not already solve this

Program Console, bundled GDB, and debugger provisioning operate on whatever binary the project builds. They do not supply headers, link lines, or SFML. No change to standalone-file ownership, PREPARE/COMMIT, Program Console, bundled GDB, provisioning, or the v2.3 signing logic.

---

## 3. Shared Homebrew facility

Detection, a harmless execution test, formula queries, and an explicit install are one macOS-only helper used by wizards. They are not copied into each `wizard.script`, and they are not a package browser.

### Why the script cannot host this

Confirmed in this tree:

| Piece | Where | What it can do | Why it is not the helper |
|-------|--------|----------------|--------------------------|
| `IO.FileExists`, `IO.DirectoryExists`, `IO.ReadFileContents`, `IO.SelectFile` | `src/sdk/scripting/bindings/sc_io.cpp` | File checks, read a header, pick a file. Always bound. | No process execution. `SelectFile` is the file dialog for “Locate Homebrew…”. |
| `IO.Execute`, `IO.ExecuteAndGetOutput`, `IO.ExecuteAndGetOutputAndError` | same file, inside `#ifndef NO_INSECURE_SCRIPTS` | `wxExecute(const wxString&, wxArrayString&, wxEXEC_NODISABLE)`. The wx header says the array form is always synchronous. | One command string, not an argv. `SecurityAllows` shows `ScriptSecurityWarningDlg` with that string. `wxEXEC_NODISABLE` leaves other windows enabled during the call. This tree does not define `NO_INSECURE_SCRIPTS`, so the methods exist; they are still the wrong call. |
| `ProgressDialog::DoUpdate` | `src/sdk/scripting/bindings/sc_progress.cpp` | `wxProgressDialog` value 0–100. | A percent for `brew install` would be fake. |
| `Message` | `src/sdk/scripting/bindings/sc_globals.cpp` `MessageBoxFunc` → `cbMessageBox` | Stock Yes/No/OK/Cancel. | Cannot label a button “Install SFML using Homebrew”. |
| `wxLaunchDefaultBrowser` | same file, `gWxLaunchDefaultBrowser` | Opens a URL. | The Homebrew-absent help action. Not an installer. |
| `GetConfigManager` | same file, `GetCM` | `ConfigManager` namespace `"scripts"`. | OpenCV’s wizard already stores options with `GetConfigManager().Read` / `.Write` (`opencv/wizard.script`). Remembering a chosen `brew` path uses this store. No new configuration database. |
| `Compiler::Execute` | `src/sdk/compiler.cpp` | On non-Windows: `wxEXEC_SYNC \| wxEXEC_NOEVENTS`. The yield loop is inside `#ifdef __WXMSW__` and is not a public type. | `wxEXEC_NOEVENTS` freezes the IDE for the whole install. |
| `PipedProcess::Launch` | `src/sdk/pipedprocess.cpp` | Async `wxExecute` of one command string, timer, idle read. Used by `ToolsManager`. | String command, and it is tied to tool-output ids. |
| `ToolsManager` | `src/sdk/toolsmanager.cpp` | User tools. macOS console tools go through `DEFAULT_CONSOLE_TERM` in `src/sdk/globals.cpp`, an `osascript` that runs `$SCRIPT` in Terminal.app. | Opens a shell. That is how a bootstrap would leak in. |
| Wizard pages | `Wiz::AddInfoPage` → `WizInfoPanel` (`wiz.cpp`). `OnLeave_*` in `WizPageBase::OnPageChanging` (`wizpage.cpp`) can veto Next. | Static text plus Back/Next/Cancel. | No per-page custom button. Custom labels live in a dialog the helper shows. |
| `wxExecute(const char* const* argv, int flags, wxProcess*, const wxExecuteEnv*)` | Installed `wx/utils.h` | Argv, async, optional child environment. | This is the process call the helper should use. Nothing in the squirrel bindings exposes it. |

`BeginWizard` runs before `wxWizard::RunWizard` (`wiz.cpp`). Short queries may run there. If `BeginWizard` adds no pages, the engine reports that the wizard failed. Declining an install must not do that.

The string form of `wxExecute` was not proved to invoke `/bin/sh` in this wx build (`libwx_baseu` has no `/bin/sh` literal). The helper still uses the argv overload so `brew`, `--prefix`, `--installed`, and the formula are separate arguments.

### Where it lives

`src/plugins/scriptedwizard/macbrew.cpp` and `macbrew.h`, whole file under `#if defined(__WXMAC__)`, added in `src/plugins/scriptedwizard/Makefile.am` beside `wiz.cpp`.

Bindings are methods on `Wiz`, registered in `wiz.cpp` only inside the same guard, next to `AddInfoPage`. Every scripted wizard already has `Wizard`. Windows and Linux do not get a stub. v3.0’s only caller is `sfml/wizard.script`, and only when `PLATFORM == PLATFORM_MAC`.

Not a new plugin. Not an SDK service. Not `IO.Execute`. The compiler and Tools helpers do not offer argv plus a dialog, so a short `wxProcess` in this plugin is the existing wx mechanism, not a new process framework.

The helper knows brew’s executable, brew’s prefix, safe queries, a formula prefix, an explicit install, exit status, and captured output. It does not know `SFML/Graphics.hpp`, the three `sfml-*` libraries, or SFML 2 versus 3.

**Proposed design.** Generic in data does not mean generic in capability. The v3.0 methods on `Wizard` are only the operations this SFML workflow uses: locate a `brew` executable, revalidate it, obtain the Homebrew prefix when that flow needs it, query one caller-supplied formula, obtain that formula’s prefix when it is installed, install that one formula after consent, and return the process result and output for the diagnostic. There is no Squirrel binding equivalent to `ExecuteBrewCommand` or `ExecuteBrew(argv)`. v3.0 does not add formula search or browsing, upgrade, upgrade-all, uninstall, cleanup, taps, casks, a dependency graph, package-metadata browsing, a Homebrew bootstrap, or a package-manager UI. SDL2 in section 5 is evidence that another wizard has the same kind of gap. It is not a reason to widen this API.

Formula strings are checked before they are put in argv: one token, letters, digits, and `.` `_` `+` `-` `@` `/` only. The SFML script passes the literal `sfml`. Names are never read from a `.cbp`.

### Four operations

Measured on this machine with `/opt/homebrew/bin/brew`, Homebrew **7.0.9**. Nothing was installed. `/usr/local/bin/brew` is absent here. These commands are the ones to call; they are not a guess.

**A. Detect.** No `PATH` search, no `wxSetEnv`, no home-directory path. Order:

1. The path stored in `ConfigManager` `"scripts"` under `/macos/homebrew/executable`, if that file still exists. A user who used “Help me find Homebrew” chose that installation. Do not replace it with `/opt/homebrew/bin/brew` or `/usr/local/bin/brew` while the file is still there.
2. `/opt/homebrew/bin/brew`
3. `/usr/local/bin/brew`

The saved path is a location hint, not a stored pass. **Proposed design:** every session that needs Homebrew runs `{brew, "--version"}` on the candidate before accepting it. A path that passed once is not trusted again without that test.

If the saved file is gone, probe 2 then 3. A missing file is “not present” at that path, not a failed execution. The diagnostic shows whichever later candidate passes. It does not claim the old path still works.

If the saved file exists and `--version` fails, stop. Do not probe the standard locations. The page shows that path and that access failed. Offer **Help me find Homebrew** and **Installation instructions**. Do not offer **Install SFML**. Do not create a project.

The same three results apply to a standard-location candidate when no saved file is in use. The first of `/opt/homebrew/bin/brew` and `/usr/local/bin/brew` that exists is the candidate. If that file fails `--version`, report present-but-unusable and do not try the other one. If neither file exists, Homebrew is not detected.

**B. Test.** Argv `{brew, "--version"}` on the candidate from A, including a saved path. **Confirmed from this machine:** `/opt/homebrew/bin/brew --version` printed `Homebrew 7.0.9` and exited 0. It does not install, does not need sudo, and does not need a shell. Capture stdout/stderr. Non-zero, or no “Homebrew” line, means “executable present, access from Code::Blocks failed.” Show that. Do not treat it as “SFML missing.”

**C. Prefix and formula.** Two different commands:

- Argv `{brew, "--prefix"}` prints the Homebrew prefix. This run printed `/opt/homebrew`, exit 0. That is not an SFML path.
- Argv `{brew, "--prefix", "sfml"}` is the wrong installed-test. Homebrew’s own help says a formula argument is “where formula is **or would be** installed.” This run printed `/opt/homebrew/opt/sfml` and exited 0 while SFML is not installed (`Cellar/sfml` absent, and the next command failed).
- Argv `{brew, "--prefix", "--installed", formula}` is the query. This run for `sfml` exited 1 and printed `Error: The following formulae are not installed: sfml`. Exit 0 plus a single directory line means installed, and that line is the package prefix. Any other exit is a failed query, not “missing.”

The wizard then checks that prefix itself. For SFML the planning assumption, still to be confirmed against a real install, is `include/SFML/Graphics.hpp` plus `lib/libsfml-graphics.dylib`, `lib/libsfml-window.dylib`, and `lib/libsfml-system.dylib`. The helper does not contain those paths.

**D. Install.** Never from opening the wizard or from a successful detect. Only after a dialog that states the dependency, why this wizard needs it, that Homebrew will be used, and the formula. The button label is supplied by the caller (“Install SFML using Homebrew”). Argv `{brew, "install", formula}` with the same token check. Async `wxExecute` plus `wxProcess::Redirect()`. The dialog says what is happening and can show the captured output. No percent (`ProgressDialog` is the wrong widget). The helper records the pid it launched and does not signal any other Homebrew process.

**Proposed design.** Cancel has two boundaries.

- Before `brew install` has been launched: do not start it. No package operation runs. Do not create the project.
- After it has been launched: `wxKill` that child only. This is a request to stop that process, not a rollback. Do not report success. Do not call `SetupProject`. Do not run `brew uninstall`, do not delete Homebrew files, and do not claim the prefix is unchanged. What that process already wrote is **not known until it is measured** at implementation time. The next attempt starts with a fresh detect, `--version`, formula query, and SFML validation, and uses only what those checks find.

`brew help install` on 7.0.9 says a plain install may upgrade an outdated formula and may update dependents unless `HOMEBREW_NO_INSTALLED_DEPENDENTS_CHECK` is set, and may cleanup. For “not installed” the command is still `install`. Child environment only, via `wxExecuteEnv`: start from the current environment and set `NONINTERACTIVE=1` and `HOMEBREW_NO_AUTO_UPDATE=1`. **Assumption:** a non-empty `wxExecuteEnv::env` replaces the child environment rather than merging. Copy the current environment first. Do not call `wxSetEnv` on the IDE and do not edit shell profiles. **Assumption:** an Apple Silicon Homebrew prefix owned by the user does not prompt for an administrator password. If a test shows a password prompt, stop. Do not reuse the GDB askpass path.

No `sudo`, no `curl | sh`, no Homebrew bootstrap, no `brew update` as its own step.

### Diagnostics the user can see

Automate detection, the execution test, and prefix lookup. Do not install in secret, do not hide a failed test, and do not claim a project is ready.

When SFML is already valid, no dependency page. Case A in the acceptance section.

Otherwise one info page, text filled from the query results, for example:

```text
Homebrew
    Found: /opt/homebrew/bin/brew
    Access from Code::Blocks: OK

SFML
    Not installed
```

or “Homebrew: Not detected”, with the sentence that some project templates obtain libraries through Homebrew. Wording can change. The page must not ask for include directories, library directories, or flags.

Actions that need real labels are the helper dialog, because `Message` cannot rename buttons:

- Homebrew works, formula missing: **Install SFML using Homebrew**. Cancel on that dialog, before install starts, returns to the page and does not launch `brew`.
- Saved or standard `brew` exists, but `--version` failed: **Help me find Homebrew** and **Installation instructions**. Do not switch to a different `brew` in the same attempt.
- No `brew` file at the saved path or the standard paths: the same two actions. Probing continues only when the saved file is absent, as in A.

**Help me find Homebrew** uses `IO.SelectFile` (or the same `wxFileDialog` from C++). The user picks the `brew` executable, not a Cellar and not an include directory. The helper then runs `--version` and `--prefix`. Only a passing test is stored with `ConfigManager::Write` on the `"scripts"` namespace, key `/macos/homebrew/executable`. That write records the location. The next session still has to pass `--version` before the path is accepted. A later failure stays on that path and is shown as present-but-unusable.

**Installation instructions** calls `wxLaunchDefaultBrowser("https://brew.sh")`. The IDE does not run the bootstrap.

After a successful install, the same query and the wizard’s own header check run again in that wizard session. The user does not restart Code::Blocks. A failed install, a cancel before launch, or a cancel after launch does not call `SetupProject`. A later attempt rediscovers; it does not reuse the cancelled process as the source of truth.

---

## 4. Diagnostic tests

These call the helper the wizard calls. They are not a second copy of the logic. Nothing here installs SFML until test 7, and test 7 runs only when someone is implementing that milestone and explicitly confirms the install.

| | State | Command / check | Pass |
|--|--------|-----------------|------|
| 1 | Executable | Saved path if the file exists, else `/opt/homebrew/bin/brew`, else `/usr/local/bin/brew` | A candidate path, or “not detected”. A saved file that fails test 2 is not replaced by a standard path. A missing saved file is not a failure; probing continues. |
| 2 | Execution | `{brew, "--version"}` on that candidate, every session | Exit 0 and a Homebrew version line. Distinct from test 1. A previous pass does not count. |
| 3 | Homebrew prefix | `{brew, "--prefix"}` | Exit 0, one directory. Not a formula prefix. |
| 4 | Formula | `{brew, "--prefix", "--installed", "sfml"}` | Exit 1 and the “not installed” error, or exit 0. Do not use `--prefix` without `--installed`. |
| 5 | Formula prefix | Same command, exit 0 | The printed directory. |
| 6 | SFML suitability | Wizard, not the helper | `Graphics.hpp` and the three dylibs under that prefix, then `SFML_VERSION_MAJOR` in `Config.hpp`. |
| 7 | Explicit install | `{brew, "install", "sfml"}` only after the button | Process exit observed. Cancel before launch does not start `brew`. Cancel after launch stops that pid only, does not report success, and does not roll back. |
| 8 | Rediscovery | Tests 4–6 again, same functions | After a successful install, valid SFML without restarting the IDE. After a cancel-during-install, the same functions decide what is present. Do not infer that from the killed process. |

Tests 1–3 were run from this shell against `/opt/homebrew/bin/brew` (Homebrew 7.0.9, prefix `/opt/homebrew`). Test 4 was run the same way: SFML is not installed. Tests 5–8 wait until an install exists. A Finder-launched app is the same argv if the path is absolute; that still has to be repeated inside the IDE at implementation time, because this shell is not the IDE.

---

## 5. Other wizards, not in this milestone

Same gap, same helper, no SFML facts required. Not modified in v3.0.

| Wizard | What macOS does today | Later use of the helper |
|--------|------------------------|-------------------------|
| `sdl/wizard.script`, `sdl2/wizard.script` | Path page is Windows-only. macOS adds `-framework SDL` or `-framework SDL2` and no prefix. Same shape as SFML. | Query a formula, then the wizard checks its own headers. |
| `glfw/wizard.script` | Path page on every platform, including macOS, default `$(#glfw)`. | Could skip that page when a formula prefix validates. |
| `fltk/wizard.script` | Non-Windows branch is `` `fltk-config --cxxflags` ``. Finder launches do not have Homebrew on `PATH`. | A prefix would not fix the backticks by itself. Out of scope even as a client until someone replaces those backticks. |
| `gtk/wizard.script`, `ogre/wizard.script` | `` `pkg-config …` `` in the project. | Same backtick problem. Do not “fix” them here. |
| `glut/wizard.script` | macOS points at `/System/Library/Frameworks/GLUT.framework` and links `-framework GLUT`. | Not a Homebrew lookup. |
| `wxwidgets/wizard.script` | macOS still appends `` `wx-config …` `` and a post-build `/Developer/Tools/Rez` line. | A different, older macOS path. Not this milestone. |

SDL2 is the cleanest later client. v3.0 does not edit it and does not add helper operations for it.

---

## 6. SFML client

```text
Start SFML wizard
    → helper: query formula "sfml"
        → installed and the wizard’s header/lib check passes
              → read SFML_VERSION_MAJOR
              → no dependency page
              → rest of the wizard
        → Homebrew runs, formula missing or check fails
              → diagnostic page
              → explicit Install SFML
              → same query and check again
        → Homebrew missing or --version fails
              → diagnostic page
              → locate brew, or open https://brew.sh
```

Once a prefix is accepted, the macOS branch of `sfml/wizard.script` still does the project edits from the earlier investigation:

- Major 2: existing `sfml/sfml2`. Major 3: a new `sfml3/main.cpp` only after those headers are read. Major 1, or an unreadable version: stop with a clear message. Do not ask the beginner to pick 1.6 versus 2.0.
- `AddIncludeDir` and `AddLibDir` for that prefix. No `pkg-config` in the `.cbp`. Build does not depend on `PATH`.
- Keep the shared `AddLinkLib` lines `sfml-graphics`, `sfml-window`, `sfml-system`. Dynamic link stays the default (`SFMLLinkType` is already 1 when the Windows page is skipped).
- Do not add `-framework SFML` for this dylib prefix.
- No rpath until `otool -D` / `otool -L` on the installed dylibs shows install names that need one.
- Windows pages, the commented Unix `pkg-config` lines, `src/templates/common/sfml*`, `common_functions.script`, lib_finder, and v2.x debugger behaviour stay as they are.
- Do not bundle SFML in the DMG.

`$(#sfml)` remains a Windows mechanism. **Assumption:** the script may not be able to create that global variable. An absolute prefix in this project is acceptable. It is not a home-directory path.

---

## 7. Files

| File | Role | Why it has to change | Generic or SFML | Why the existing piece is not enough |
|------|------|----------------------|-----------------|--------------------------------------|
| `src/plugins/scriptedwizard/macbrew.cpp`, `macbrew.h` | New. Detect, `--version`, `--prefix`, `--prefix --installed`, explicit `install`, dialog, saved executable. | Argv execution and custom buttons do not exist on `Wizard`. | Generic. No SFML names inside. | `IO.Execute` is a synchronous command string plus a security dialog. `Compiler::Execute` freezes the UI. `PipedProcess::Launch` is a command string owned by Tools. |
| `src/plugins/scriptedwizard/Makefile.am` | Add the two sources. | Automake will not compile a file that is not listed. | Generic. | |
| `src/plugins/scriptedwizard/wiz.cpp` | Bind the methods under `#if defined(__WXMAC__)`. | Scripts need a bridge. The registration block is already here (`BindMethod` … `AddInfoPage`). | Generic bindings. | No current method runs an argv. |
| `src/plugins/scriptedwizard/resources/sfml/wizard.script` | macOS branch: call the helper, validate headers, choose `sfml2`/`sfml3`, write include and lib dirs, skip `-framework SFML`. | This is the client. | SFML only. | The helper must not learn `Graphics.hpp`. |
| `sfml/sfml3/main.cpp`, `sfml/Makefile.am` | Only if test 6 shows major 3. | The shipped sample is SFML 2. | SFML only. | |
| `README.macos.md` | Beginner steps and the diagnostic cases. | Last milestone. | Docs. | |

Not modified: `config.script`, other wizards, `common_functions.script`, `src/templates/common/sfml*`, compiler plugin, lib_finder, debugger, GDB, Program Console, signing, `make_cb_app_bundle.sh`.

---

## 8. Milestones

M1 is this document. No code. It is done: sections 3–7 record the process, config, and URL mechanisms that were checked in `sc_io.cpp`, `wiz.cpp`, `compiler.cpp`, and `pipedprocess.cpp`, and the Homebrew 7.0.9 commands checked with local `brew help` (`--version`, `--prefix`, `--prefix --installed`). The later corrections (saved path first, revalidate every session, cancel-before versus cancel-during, narrow API) are part of that same plan.

M2 is done in tree. `macbrew.cpp` locates one executable, revalidates it with `{brew,"--version"}`, and runs `{brew,"--prefix"}` only after that succeeds. `Wizard.MacBrewProbe` returns one report string. There is no formula query and no install.

**CONFIRMED FROM LOCAL MACHINE** (2026-10-21). The installed app was started with `PATH=/usr/bin:/bin:/usr/sbin:/sbin` and no `HOMEBREW_*`. `which brew` cannot see Homebrew on that path. `Wizard.MacBrewProbe` from the IDE:

1. No saved path, prefix requested: `access=works`, executable `/opt/homebrew/bin/brew`, `version_exit=0`, `version=Homebrew 7.0.9`, `prefix=/opt/homebrew`, `saved_missing=0`.
2. Saved `/no/such/brew`: `saved_missing=1`, then the same `/opt/homebrew/bin/brew` with `access=works`. Prefix was not requested, so `prefix_exit=-1`.
3. Saved `/bin/echo`, prefix requested: `access=present_unusable`, executable stayed `/bin/echo`, `version_exit=0`, first line `--version` (no `Homebrew`), `prefix_exit=-1`. The standard locations were not tried.

The saved key was cleared in the throwaway `--user-data-dir` afterwards. The real `~/Library/Application Support/codeblocks/default.conf` was not modified.

| | Objective | Files | Non-goals | Check | Stop |
|--|-----------|-------|-----------|-------|------|
| M1 | **DONE** (plan). Record the process, config, and URL mechanisms above, and the Homebrew 7.0.9 commands. | This plan only. | No code, no install. | Review against `sc_io.cpp`, `wiz.cpp`, `compiler.cpp`, and a local `brew help`. | Met. The plan matches those sources. |
| M2 | **DONE** (tree). Tests 1–3 inside the helper, from the IDE. | `macbrew.cpp`, `macbrew.h`, `Makefile.am`, `wiz.cpp` `MacBrewProbe`. | No formula query, no install, no SFML page. No general argv binding. | Met. Saved path first, then the two standard paths. `--version` on every use. `/bin/echo` stayed unusable and was not replaced. Missing `/no/such/brew` fell through. `--prefix` only after a good `--version`. | Met. Absolute `/opt/homebrew/bin/brew --version` returned Homebrew 7.0.9 while `PATH` was `/usr/bin:/bin:/usr/sbin:/sbin`. |
| M3 | Test 4–5 for the literal `sfml`. | `macbrew.cpp` | No header check, no install. | Installed and not-installed are different results. `--prefix` without `--installed` is not used as the installed test. | Stop if exit 0 is reported while the formula is absent. |
| M4 | SFML wizard validates the prefix and writes the `.cbp` when SFML is already installed. Stays before M6 so a bad project is not confused with a bad install. | `sfml/wizard.script` | No install button. No other wizard. | Tests 6 and the generated include/lib dirs. No `-framework SFML`. Windows diff empty outside the macOS branch. | Stop if the headers do not match the assumed layout; fix the check from the real tree before generating a project. |
| M5 | Diagnostic page for “Homebrew ok, SFML missing” versus “Homebrew missing / `--version` failed.” Locate and https://brew.sh. | `wizard.script`, dialog strings from the wizard | No `brew install`. | Cases C, D, E. Case A still has no page. | Stop if Cancel or a failed test still produces a project. |
| M6 | Explicit install, then tests 4–6 again. Still after M4. | `macbrew.cpp` install path | No uninstall UI, no `brew update`, no bootstrap, no rollback. | Case B. IDE stays responsive. No percent. Cancel before launch starts nothing. Cancel after launch kills only that pid, reports no success, creates no project, and the next attempt uses fresh discovery. | Stop on a password prompt, if success is reported when the process exit is non-zero, or if cancel is described as undoing Homebrew’s files. |
| M7 | `sfml3/main.cpp` only if M4/M6 headers are major 3. | `sfml3/main.cpp`, `sfml/Makefile.am` | No new sample artwork. | Build and Run shows the window and `cb.bmp`. | Skip the whole milestone if the headers are major 2. |
| M8 | `otool -D` and `otool -L` on the three dylibs and the linked binary. | `wizard.script` linker line only if required | No rpath “just in case.” | `bin/Debug/<name>` starts from the IDE and from that directory. | Stop without an rpath if the install names are absolute. |
| M9 | `README.macos.md`: New → SFML project, what the diagnostic means, Homebrew is installed by the user from Homebrew’s site. | `README.macos.md` | No search-path tutorial. | A reader is not told to type include flags. | |

---

## 9. Acceptance

- **A.** Homebrew and a suitable SFML are already installed. Both are detected. No dependency page. The project builds with no hand-entered paths.
- **B.** Homebrew works, SFML is absent. The page says so. Nothing is installed until **Install SFML using Homebrew**. After success, discovery runs again and the wizard continues. No IDE restart.
- **C.** Homebrew is not found. The page says so. No install of Homebrew and no install of SFML. The user can open Homebrew’s site.
- **D.** `brew` is not in the two default locations. **Help me find Homebrew** validates the chosen executable and stores that path. Later sessions still require `--version` before the path is accepted. The user is not asked for SFML include or lib directories.
- **E.** The executable exists but `--version` fails, or `install` exits non-zero. The failure is visible. The IDE does not say SFML was installed and does not write a project.
- **F.** Cancel before installation starts performs no package operation. Cancel during installation requests termination of the Homebrew process Code::Blocks launched, does not report success, does not create the project, and causes fresh discovery before any subsequent attempt. Code::Blocks does not claim that cancellation rolls back changes already made by Homebrew.

Windows and non-mac Unix wizard behaviour stay as they are. SFML stays out of the DMG. v2.x debugger, console, and signing behaviour stay as they are.

---

## 10. What stays rejected

Bundling SFML in the DMG stays rejected. It grows the image, adds license notices for SFML’s dependencies, and makes projects point inside `CodeBlocks.app`. lib_finder and `` `pkg-config` `` backticks stay rejected: they are not on the New Project path, and a Finder-launched app does not have Homebrew on `PATH`. The shared helper is the discovery path, and it installs a formula only after an explicit button.
