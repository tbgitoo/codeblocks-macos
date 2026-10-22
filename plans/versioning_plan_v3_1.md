# Code::Blocks macOS v3.1 — existing GLFW wizard on Apple Silicon

**Status: M1 done. The macOS Homebrew client is in `glfw/wizard.script`. The macOS sample is still the historical fixed-function program; replacing that sample is the remaining M2 work. GLFW is not installed.**

**Question this version answers:** what is the smallest coherent change that makes the existing GLFW project wizard usable in this native Apple Silicon distribution, with GLFW discovered automatically through the Homebrew wizard infrastructure established in v3.0?

**Product invariant, macOS only:** creating a GLFW project must not require the beginner to configure `$(#glfw)` or to use the Global Variable Editor. A validated Homebrew GLFW prefix supplies the include directory, the library directory, and the link name. Windows and other Unix keep the location page and `$(#glfw)`.

GLFW is the second client of the v3.0 helper. v3.1 does not add a GLFW wizard. `config.script` already registers it. v3.1 does not change SFML, the helper, or the global-variable system.

Upstream baseline remains Code::Blocks trunk **r14050** / IDE **25.03**.

Evidence labels used below: **CONFIRMED FROM SOURCE**, **CONFIRMED FROM LOCAL MACHINE**, **ASSUMPTION**, **PROPOSED DESIGN**, **TO BE MEASURED DURING IMPLEMENTATION**.

---

## 1. Existing GLFW implementation map

### Entry the user sees

File → New → Project lists wizards from `src/plugins/scriptedwizard/resources/config.script`.

```33:33:src/plugins/scriptedwizard/resources/config.script
    RegisterWizard(wizProject,     _T("glfw"),         _("GLFW project"),          _("2D/3D Graphics"));
```

Category **2D/3D Graphics**, title **GLFW project**. No `PLATFORM` guard, so the entry is already shown on macOS. **CONFIRMED FROM SOURCE.**

`src/plugins/scriptedwizard/resources/glfw/Makefile.am` installs `wizard.script`, `logo.png`, `wizard.png`, and `files/main.cpp` under `templates/wizard/glfw/`.

`PLATFORM` constants (`src/sdk/scripting/bindings/sc_consts.cpp`): `PLATFORM_MSW` 0, `PLATFORM_GTK` 1, `PLATFORM_MAC` 2, `PLATFORM_X11` 4.

### Wizard pages

`BeginWizard` in `src/plugins/scriptedwizard/resources/glfw/wizard.script` registers four pages, with no platform test:

1. Info page `GlfwIntro`.
2. Project path page. Shared with every project wizard.
3. GLFW location page `GlfwPath`. Default text `$(#glfw)`.
4. Compiler page (debug/release output dirs). Default output dirs are `bin/Debug` and `bin/Release`.

There is no version page and no static/dynamic page.

### Location page

Globals at the top of `wizard.script`:

```8:11:src/plugins/scriptedwizard/resources/glfw/wizard.script
GlfwPathDefault    <- _T("$(#glfw)");
GlfwPathDefaultInc <- _T("$(#glfw.include)");
GlfwPathDefaultLib <- _T("$(#glfw.lib)");
GlfwPath <- _T("");
```

`OnLeave_GlfwPath` reads `txtFolder`, then:

- `VerifyDirectory` on the text.
- `GetCompilerIncludeDir`, then `VerifyFile` for `GLFW/glfw3.h` under that include directory.
- `GetCompilerLibDir`, then `VerifyLibFile` for the library stem `glfw3`.
- Stores the original text in `GlfwPath`.
- Keeps `$(#glfw.include)` / `$(#glfw.lib)` when `GetCompilerIncludeMacro` / `GetCompilerLibMacro` return a non-empty string. Those helpers return the expanded directory when the default macro resolves, so a defined variable leaves the macros in `GlfwPathDefaultInc` and `GlfwPathDefaultLib`.

`VerifyLibFile` / `SilentVerifyLibFile` (`common_functions.script`) look for `.a`, `.lib`, `.dll.a`, `.la`, and `.so`. They do not look for `.dylib`. **CONFIRMED FROM SOURCE.** Changing that helper would affect every wizard. The macOS check belongs in `glfw/wizard.script`.

### What `SetupProject` writes

For every platform, both targets:

- Include dir `GlfwPathDefaultInc` (`$(#glfw.include)` unless the leave handler replaced it).
- Lib dir `GlfwPathDefaultLib`.
- Link library `glfw3` (`AddLinkLib` stores the name; the compiler plugin turns it into `-l`).

Then a platform branch:

| Platform | Extra link libraries |
|----------|----------------------|
| `PLATFORM_MSW` | `opengl32`, `glu32`, `gdi32` |
| everything else, including macOS | `GL`, `GLU`, `pthread`, `Xxf86vm` |

No raw linker options. No frameworks. No `-std`. `WarningsOn` for the selected compiler.

Debug target: `ttConsoleOnly`, output `bin/Debug/<name>`, debug symbols. Release target: `ttExecutable`, output `bin/Release/<name>`, optimisations.

Both targets call `SetWorkingDir(GlfwPath + "/bin")`. The script never copies a resource file. The sample does not open one.

### Files the wizard copies

`GetFilesDir()` returns `glfw/files`. The only project source is `files/main.cpp`. No bitmap, no generated makefile.

### Sample

`files/main.cpp` is the historical sample. It includes `<GLFW/glfw3.h>` and draws with fixed-function OpenGL: `glMatrixMode`, `glOrtho`, `glRotatef`, `glBegin` / `glEnd`, `glColor3f`, `glVertex3f`. It sets no `glfwWindowHint`. **CONFIRMED FROM SOURCE.** That file is not the macOS result. Section 6 records the current macOS sample. `GetFilesDir()` returns `glfw/files` for every platform today, so a macOS-only replacement has to be a second directory selected from `GetFilesDir` when `PLATFORM == PLATFORM_MAC`. The historical file stays the non-mac sample.

### Older GLFW artifacts that are not this wizard

Leave these alone. They are GLFW 2, and they are not the New Project entry named “GLFW project”.

| Path | What it is |
|------|------------|
| `src/templates/common/glfw.template` | “GLFW Application”, copies `glfw-main.cpp` |
| `src/templates/common/glfw-main.cpp` | `#include <GL/glfw.h>`, `glfwOpenWindow` |
| `src/templates/unix/glfw-static.cbp` | Links `glfw`, `GL`, `GLU`, `pthread`, `Xxf86vm`, lib dir `/usr/X11R6/lib` |
| `src/templates/win32/glfw-static.cbp` | `$(#glfw.include)`, `$(#glfw.lib)`, library `glfw` |
| `src/plugins/contrib/lib_finder/lib_finder/glfw.xml` | Header `gl/glfw.h`, pkg-config name `libglfw` |

### What must stay unchanged

- The Windows and other-Unix location page, `$(#glfw)` / `$(#glfw.include)` / `$(#glfw.lib)`, the existing `SetupProject` link list for those platforms, and `glfw/files/main.cpp`.
- `common_functions.script`.
- `config.script` registration and the other graphics wizards.
- The GLFW 2 templates and `lib_finder/glfw.xml`.
- SFML’s wizard and the generic Homebrew helper.

---

## 2. Baseline macOS behaviour and the Global Variable Editor

Manual testing already reached the Global Variable Editor for a variable named `glfw`, with empty base, include, and lib fields. The source path that produces that dialog is below. **The editor observation is the reported manual test. The call chain is CONFIRMED FROM SOURCE.**

### Why the editor opens

`$(#glfw)` is not defined anywhere in this tree for macOS.

The location page is a `WizGenericSelectPathPanel`. Its default, when `project_wizard` `/generic_paths/GlfwPath` is empty, is `$(#glfw)` (`wiz.cpp` `AddGenericSelectPathPage`, `wizpage.cpp` constructor).

Forward navigation runs `WizGenericSelectPathPanel::OnPageChanging` before the squirrel `OnLeave_GlfwPath`:

```474:481:src/plugins/scriptedwizard/wizpage.cpp
        const wxString &originalDir = m_pGenericSelectPath->txtFolder->GetValue();
        wxString dir = Manager::Get()->GetMacrosManager()->ReplaceMacros(originalDir);
        if (!wxDirExists(dir))
        {
            cbMessageBox(_("Please select a valid location..."), _("Error"), wxICON_ERROR, GetParent());
            event.Veto();
```

`MacrosManager::ReplaceMacros` sends `$(#…)` to `UserVariableManager::Replace`. `ParseVariableName` (`uservarmanager.cpp`) treats a name with no dot as the member `base`. An unknown variable is not a silent empty string. `Replace` tells the user that the global compiler variable is unknown and calls `OpenEditWindow` for that name (`uservardlgs.cpp`). The editor layout in `src/src/resources/global_uservars.xrc` has the fields **base**, **include**, and **lib**. A new variable has those fields empty.

That is the dialog in the manual test. It is opened by the generic path page expanding the default `$(#glfw)`. It is not opened by a GLFW-specific editor, and it happens before a project exists.

If the user dismisses the editor without creating a directory that exists, the page shows “Please select a valid location...” and vetoes Next. The compiler page and `SetupProject` are not reached.

If the variable is defined and its base directory exists, the page continues and `OnLeave_GlfwPath` also expands `$(#glfw.include)` and `$(#glfw.lib)`. A successful leave stores those macros in the project. Later expansion of a missing member opens the same editor at build time. The macOS design avoids that by never writing the macros.

### What a completed macOS run would generate

This path is what the script writes after the location page has been satisfied. It was not executed for this plan. **CONFIRMED FROM SOURCE as the script’s intent. Not executed in the IDE for v3.1.**

For a project named `Game` in `~/Game/`, with the defaults:

```text
~/Game/Game.cbp
~/Game/main.cpp          copied from glfw/files/main.cpp
```

Project-wide:

- Include dir `$(#glfw.include)` (or a concrete include dir if the macro could not be kept).
- Lib dir `$(#glfw.lib)` (same rule).
- Link libraries `glfw3`, `GL`, `GLU`, `pthread`, `Xxf86vm`. The `else` branch is not Unix-only. macOS falls into it.
- No framework, no `-std`, no rpath.

Debug `bin/Debug/Game`: console subsystem, debug symbols, working directory `<chosen glfw path>/bin`.

Release `bin/Release/Game`: GUI subsystem, optimisations, same working directory.

`GlfwPath` starts as `""`. If the macOS path skipped the page and still executed today’s `SetupProject`, the working directory would be the string `/bin`. **PROPOSED DESIGN** does not do that.

### Why a hand-entered Homebrew prefix still fails today’s page

Even after the editor, `OnLeave_GlfwPath` calls `VerifyLibFile(..., "glfw3")`. That helper does not accept a `.dylib`, and the stem `glfw3` is the static archive name from GLFW’s CMake, not the shared library Homebrew’s own formula test links. Section 4 records that split. A beginner who pastes `/opt/homebrew/opt/glfw` into the location page is still stopped. **CONFIRMED FROM SOURCE** for the check. The installed tree was not present, so this was not run against a prefix.

### Unresolved references if the project were forced through

A project created with the current macOS branch still contains `$(#glfw.include)` and `$(#glfw.lib)` whenever the leave handler kept the macros. Build and run then depend on that global variable. There is no pkg-config line and no `DYLD_LIBRARY_PATH` in the script. The X11 library names are still on the link line.

---

## 3. v3.0 Homebrew infrastructure available for reuse

SFML is a client of a generic helper. The helper does not know SFML headers, library names, or SFML 2 versus 3 (`macbrew.h`). The same boundary holds for GLFW: the helper must not learn GLFW headers, library names, OpenGL, or version meaning.

Implemented on macOS only (`#if defined(__WXMAC__)` in `macbrew.cpp` / `wiz.cpp`):

| Script call | Behaviour |
|-------------|-----------|
| `Wizard.MacBrewFormula(formula)` | Revalidate Homebrew, then `{brew, "--prefix", "--installed", formula}`. Failed `--version` becomes `brew_unavailable`, not `missing`. |
| `Wizard.MacBrewInstall(formula, explanation, confirmLabel)` | Consent dialog first. Cancel before launch does not start `brew`. The process is `{brew, "install", formula}` with `NONINTERACTIVE=1` and `HOMEBREW_NO_AUTO_UPDATE=1`. Success is exit 0, not cancelled, no password prompt. |
| `Wizard.MacBrewChoice(message, first, second)` | Two buttons plus Cancel. Does not run brew. |
| `Wizard.MacBrewTry(executable, wantPrefix)` | Revalidate one chosen executable. |
| `Wizard.MacBrewProbe(wantPrefix)` | Locate and revalidate. Available; the SFML client uses the formula query, which probes internally. |

Formula states in the report string: `rejected`, `brew_unavailable`, `missing`, `installed`, `failed`. Access: `works`, `present_unusable`, `not_present`. A token may contain letters, digits, and `.` `_` `+` `-` `@` `/`. `glfw` is a legal token. **CONFIRMED FROM SOURCE.**

Install report fields: `launched`, `cancelled`, `password`, `exit`, `success`, `output`.

Saved Homebrew executable: ConfigManager `/macos/homebrew/executable` in the `"scripts"` namespace. The SFML script writes it only after `MacBrewTry` returns `works`. GLFW should use that same key. It is the helper’s location hint, not an SFML setting.

There is no general `ExecuteBrewCommand`. v3.1 does not add one.

### What is generic versus what the SFML script owns

Generic, already done: detection order, `--version` revalidation, the installed-prefix query, the consent dialog, the install process, cancellation, the password stop, and the report strings.

SFML-specific, inside `sfml/wizard.script`: the words on the page, the formula token `"sfml"`, `SfmlReportField`, header and dylib checks, SFML 2 versus 3 sample selection, and `SetupProject`’s macOS include/lib/`-std=c++17`.

GLFW can be a second client by a macOS branch in `glfw/wizard.script` that calls the same five methods. No missing generic capability showed up in the helper. Prefix suitability is a client check, as it is for SFML. **PROPOSED DESIGN:** leave `macbrew.cpp`, `macbrew.h`, `wiz.cpp`, and `scriptedwizard/Makefile.am` unchanged.

The page text and the report-field parser will be duplicated in the GLFW script under GLFW names. Sharing them would be a new squirrel facility. v3.1 does not do that, and it does not edit the SFML script to extract one.

---

## 4. GLFW formula and prefix

Measured on 22 October 2026. Nothing was installed.

**CONFIRMED FROM LOCAL MACHINE**

- `brew info glfw`: formula `glfw`, stable **3.5.1** (bottled), “Not installed”, homepage `https://www.glfw.org/`.
- `brew --prefix --installed glfw` exits with “The following formulae are not installed: glfw”.
- `/opt/homebrew/opt/glfw`, `/opt/homebrew/include/GLFW`, and `/usr/local/opt/glfw` are absent.

So this plan does not have an installed prefix, an `otool` dump, or a built sample. Those are implementation-time measurements.

**CONFIRMED FROM SOURCE** of Homebrew’s formula `Formula/g/glfw.rb` at `homebrew-core` HEAD, which is the 3.5.1 tarball:

- Build-time dependencies: `cmake`, `pkgconf`. No runtime formula dependencies on macOS. Linux adds X11 and Mesa; that block is `on_linux`.
- The formula configures twice. The first build follows CMake’s default `BUILD_SHARED_LIBS` OFF and copies `build/src/libglfw3.a` into `lib`. The second build passes `-DBUILD_SHARED_LIBS=TRUE` and runs `cmake --install`.
- The formula’s own test compiles with `-I#{include} -L#{lib} -lglfw`. It does not pass `-lglfw3`, `-lGL`, `-lGLU`, `-lXxf86vm`, or any `-framework`.

**CONFIRMED FROM SOURCE** of GLFW 3.5.1 `src/CMakeLists.txt` (the tag the formula archives):

```text
if (UNIX AND GLFW_BUILD_SHARED_LIBRARY)
    GLFW_LIB_NAME = glfw
else
    GLFW_LIB_NAME = glfw3
```

Apple is UNIX in that file. The shared library’s output name is `glfw` (`libglfw.dylib`, `SOVERSION` 3). The static library’s output name is `glfw3` (`libglfw3.a`). The Cocoa build links the library itself, privately, to the Cocoa, IOKit, QuartzCore, and CoreFoundation frameworks. It does not link OpenGL.

`include/GLFW/glfw3.h` from the same tag includes `<OpenGL/gl.h>` on Apple when `GLFW_INCLUDE_NONE` is not set. The wizard sample does not define `GLFW_INCLUDE_NONE`, and it calls `gl*` itself.

### Four states

1. **Homebrew available.** `MacBrewFormula` access `works`. This is the helper’s existing result.
2. **Formula installed.** State `installed`, prefix is one absolute directory line.
3. **Prefix found.** That directory string. The helper does not inspect it.
4. **Prefix suitable for this wizard.** Client check, **PROPOSED DESIGN**:
   - Prefix is non-empty and starts with `/`.
   - `prefix/include/GLFW/glfw3.h` exists.
   - `prefix/lib/libglfw.dylib` exists.

`libglfw3.a` alone is not suitable. `-lglfw3` would select that static archive and would not match the formula test’s `-lglfw`. A missing header or a missing `libglfw.dylib` is an unsuitable install, not “GLFW is not installed”. State `missing` remains the only “not installed” result.

No version parse. The sample is already GLFW 3, and 3.5.1 still provides the calls it uses. A second sample is not required by the evidence gathered here. If the 3.5.1 headers reject the sample, that becomes a measurement, not a planned rewrite.

---

## 5. Proposed macOS-only client design

All of this lives in `glfw/wizard.script`.

### Pages

1. Info page. Unchanged.
2. On `PLATFORM_MAC` only, query formula `glfw`.
   - Suitable prefix: store include `prefix/include` and lib `prefix/lib`. Add no diagnostic page and no location page.
   - Anything else: add one info page, same role as SFML’s dependency page, with GLFW wording. No project is created from that page.
3. Project path page. Still registered. Forward leave on macOS repeats the query and refuses to continue when the prefix is not suitable.
4. Location page `GlfwPath` only when `PLATFORM != PLATFORM_MAC`. Default remains `$(#glfw)`. `OnLeave_GlfwPath` stays as it is for that page.
5. Compiler page. Forward leave on macOS repeats the same check.

The location page is not hidden by skipping its handler. It is not registered on macOS, so `OnPageChanging` never expands `$(#glfw)`.

### Dependency page

Wording follows the helper’s states. Labels are GLFW’s.

- Access `works` and state `missing`: say Homebrew works and GLFW is not installed. Next leads to the existing consent dialog. The button label is **Install GLFW using Homebrew**. The explanation names the formula `glfw` and says Cancel does not start Homebrew. After `success=1`, query and the prefix check run again. A suitable prefix continues the wizard without restarting Code::Blocks. A still-unsuitable prefix stays on the explanation and does not create a project.
- Access is not `works`: say Homebrew was not usable. Do not say GLFW is missing. Next uses `MacBrewChoice`: **Help me find Homebrew** (`MacBrewTry`, then write `/macos/homebrew/executable` only when access is `works`) or **Installation instructions** (`https://brew.sh`). No Homebrew bootstrap.
- Access `works` and state `failed`, `rejected`, or `installed` with a failed suitability check: show that result. Do not offer install as if the formula were absent. Do not create a project.

Cancellation stays the helper’s:

- Cancel on the consent dialog: brew was not started, no project.
- Cancel after the install process has started: the helper stops that process and does not undo files already written. The script does not report success and does not create a project.
- A password prompt or a non-zero exit: not success, no project.

### `SetupProject` on macOS

When the stored include or lib is empty, return false.

Otherwise, for both targets:

- `AddIncludeDir` of `prefix/include`.
- `AddLibDir` of `prefix/lib`.
- `AddLinkLib("glfw")`, which becomes `-lglfw`, matching the formula test.
- `AddLinkerOption("-framework OpenGL")`. **PROPOSED DESIGN** until M3 verifies the generated program. The macOS sample calls core-profile entry points exported by `OpenGL.framework`. The shared GLFW library does not provide those calls. M3 confirms the complete link line.

Do not add Cocoa, IOKit, QuartzCore, or CoreFoundation on the application unless M3 shows the link fails without one of them. GLFW’s own command-line example repeats those frameworks even for `libglfw.3.dylib` (`docs/build.md`). The Homebrew formula test links with `-lglfw` only and does not call OpenGL. The sample source does not reference Cocoa, IOKit, QuartzCore, or CoreFoundation symbols, so they are not part of the proposed link line.

Do not add `GL`, `GLU`, `pthread`, or `Xxf86vm` on macOS. Those remain the non-mac `else` branch.

Do not set a working directory on macOS. The sample has no resource file. An empty working directory is resolved from the project directory at Run time. Do not set it to `prefix/bin` or to `/bin`.

Do not add `-std`. Apple clang 21 on this machine defines `__cplusplus` as `201402L` with no `-std` flag (**CONFIRMED FROM LOCAL MACHINE**). The macOS sample uses no feature past that default. The historical non-mac sample’s range-for is C++11 and already fits that default. SFML’s `-std=c++17` came from SFML 3 headers.

Do not write `$(#glfw)`, `$(#glfw.include)`, or `$(#glfw.lib)` into the macOS project. Do not create or fill a persistent `glfw` global variable.

The non-mac body of `SetupProject` stays the current include/lib/`glfw3`/Windows-or-X11 logic, including `SetWorkingDir(GlfwPath + "/bin")`.

`GetFilesDir` returns `glfw/mac` when `PLATFORM == PLATFORM_MAC` and `glfw/files` otherwise. The macOS directory holds the current sample. The historical sample stays in `glfw/files`.

### Generated project

A normal Code::Blocks project. Absolute Homebrew prefix paths in the `.cbp` are the intended macOS result, as they were for SFML. No shell profile, no `brew` on `PATH` at build time, no `DYLD_LIBRARY_PATH`, no pkg-config backticks, no lib_finder.

---

## 6. Build, link, and sample

| Item | macOS proposal | Evidence |
|------|----------------|----------|
| Source | macOS: new `glfw/mac/main.cpp`. Non-mac: keep `files/main.cpp` | A core-profile triangle is the current macOS example. The historical file stays for Windows and other Unix. |
| Include | `prefix/include` so `<GLFW/glfw3.h>` resolves | Formula installs `include/GLFW`. Header presence is the suitability check. |
| Library dir | `prefix/lib` | Same. |
| GLFW link | `-lglfw` | Formula test. Shared CMake output name `glfw`. |
| OpenGL | `-framework OpenGL` | **PROPOSED DESIGN** until M3. Core entry points used by the triangle are exported by `OpenGL.framework`’s `gl3.h`. `-lGL` is the X11 library and is not the macOS link. |
| GLU | Omit on macOS | Sample does not call `glu*`. |
| X11 / pthread | Omit on macOS | Not used by the sample. `Xxf86vm` is the current non-mac branch. |
| Frameworks inside GLFW | Do not repeat on the executable | CMake links them into the shared library; the formula test does not pass them. |
| Language | No extra `-std` | C++14 is already the compiler default here. |
| Rpath | None in the wizard until a measurement requires one | No installed dylib was available to inspect. |
| Resources | None | The sample loads no file. |
| Debugger | Ordinary Debug on the generated project | No GLFW debugger mode. |

### Current macOS sample

The historical Sierpinski program is not the macOS model. It is shorter because it uses removed OpenGL. The macOS sample is the smallest program that asks GLFW for the current macOS context and draws with that context’s rendering model.

**Context. CONFIRMED FROM SOURCE** of GLFW 3.5.1 `docs/window.md`, `include/GLFW/glfw3.h`, and `src/nsgl_context.m`:

- macOS supports core-profile contexts for OpenGL 3.2 and later. OpenGL 3.0 and 3.1 are rejected. A compatibility profile for version 3 or later is rejected (`"The compatibility profile is not available on macOS"`).
- Before `glfwCreateWindow` of version 3.2 or later, the profile hint must be set. `NSGL` then selects `NSOpenGLProfileVersion4_1Core` when the major version is at least 4, and `NSOpenGLProfileVersion3_2Core` when it is at least 3.
- Apple’s SDK header `OpenGL/gl3.h` defines `GL_VERSION_4_1`. **CONFIRMED FROM LOCAL MACHINE** (MacOSX.sdk). 4.1 is the current macOS OpenGL implementation. A 3.2 core context is also non-legacy, and it is not the current one.

**PROPOSED DESIGN**, requested explicitly before `glfwCreateWindow`:

```text
glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 4);
glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 1);
glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GLFW_TRUE);
```

The version and core-profile hints are the ones NSGL and the GLFW macOS remark require. `GLFW_OPENGL_FORWARD_COMPAT` is appropriate: a macOS core context has no compatibility profile, and the hint says removed functionality is not wanted. NSGL does not read that flag when it builds the pixel format (**CONFIRMED FROM SOURCE** of `nsgl_context.m`). M3 checks that the created context is 4.1 core. If creation fails only because of the forward-compat hint, drop that one hint. Do not drop the version or the core profile.

**What the sample draws. PROPOSED DESIGN.** One triangle in clip space. Core profile has no fixed-function vertex processing, so the triangle is a vertex shader, a fragment shader, one vertex-array object, one buffer, and `glDrawArrays`. Positions are three hardcoded clip-space vertices. The fragment shader writes a constant colour. There is no matrix, no uniform, and no texture. That is the machinery the current model requires for one visible primitive, and nothing past it. A clear-colour window would show that GLFW created a context and would skip that model. Escape still calls `glfwSetWindowShouldClose`. The window close flag ends the loop, then the program destroys the window and calls `glfwTerminate`.

The sample defines `GLFW_INCLUDE_GLCOREARB` before including `<GLFW/glfw3.h>`. On Apple that include is `<OpenGL/gl3.h>` (**CONFIRMED FROM SOURCE** of GLFW 3.5.1 `glfw3.h`). The default include is `<OpenGL/gl.h>`, which is the legacy header. Do not use the default.

**Loader. CONFIRMED FROM LOCAL MACHINE** for the header, and **CONFIRMED FROM SOURCE** of that header: `gl3.h` sets `GL3_PROTOTYPES` on Apple and declares `glCreateShader`, `glShaderSource`, `glCompileShader`, `glCreateProgram`, `glAttachShader`, `glLinkProgram`, `glUseProgram`, `glGenBuffers`, `glBindBuffer`, `glBufferData`, `glGenVertexArrays`, `glVertexAttribPointer`, `glEnableVertexAttribArray`, `glDrawArrays`, `glClear`, `glViewport`, and `glClearColor` as ordinary functions. They are marked deprecated by Apple after 10.14 and they are still the exported core entry points. The triangle does not need GLAD, GLEW, or `glfwGetProcAddress`. GLFW’s context guide describes glad as the usual way to load OpenGL when the platform header does not export the calls. That is the Windows case, where `opengl32` exports OpenGL 1.1 and a core-profile triangle does not link. It is not this macOS sample. GLM is not required: the vertices are already in clip space.

**Why the sample is macOS-only.** `GetFilesDir` can select a directory per platform, which is how the SFML wizard selects a sample tree. One shared current sample would be compiled by the Windows wizard as well. That wizard links `opengl32`, and the core entry points above are not part of that import library. A loader would then become a second dependency on every platform so that Windows could build a sample this macOS target does not need. The historical `files/main.cpp` stays the Windows and other-Unix sample, with that path’s existing link line. v3.1 does not modernise those wizards. There is no GLFW version page.

**Language.** No `-std` flag. The triangle can be written with string literals and a plain array. No C++17 feature is required.

**Retina.** GLFW’s macOS remark says a window is not full resolution unless `GLFW_SCALE_FRAMEBUFFER` is true and the application bundle sets `NSHighResolutionCapable`. The wizard does not add an Info.plist. The triangle uses `glfwGetFramebufferSize` for `glViewport`. Whether the window is pixel-doubled is **TO BE MEASURED DURING IMPLEMENTATION**. A visible triangle does not depend on a Retina plist.

**Link, still PROPOSED DESIGN until M3.** `-lglfw` and `-framework OpenGL`. Do not add `GL`, `GLU`, `pthread`, or `Xxf86vm`. Do not add Cocoa, IOKit, QuartzCore, or CoreFoundation unless the real link of this triangle reports an undefined symbol from one of them. No rpath until `otool` of the installed `libglfw.dylib` requires one. No second Homebrew formula.

---

## 7. Files

| File | Role | v3.1 |
|------|------|------|
| `src/plugins/scriptedwizard/resources/glfw/wizard.script` | GLFW client | **Changes.** macOS query, suitability check, dependency page, install labels, skip the location page, write absolute include/lib and the macOS link line. `GetFilesDir` returns `glfw/mac` on macOS. Windows and other Unix stay on the current page, `glfw/files`, and `SetupProject` body. The Homebrew client is already in this file. The `GetFilesDir` branch is not. |
| `src/plugins/scriptedwizard/resources/glfw/mac/main.cpp` | macOS sample | **New.** OpenGL 4.1 core triangle described in section 6. |
| `src/plugins/scriptedwizard/resources/glfw/files/main.cpp` | Non-mac sample | **Unchanged.** Historical fixed-function program. Windows and other Unix keep it. |
| `src/plugins/scriptedwizard/resources/glfw/Makefile.am` | Install list | **Changes.** Install `mac/main.cpp` beside `files/main.cpp`, the same way the SFML wizard installs more than one sample tree. |
| `src/plugins/scriptedwizard/macbrew.cpp` | Generic helper | **Unchanged.** |
| `src/plugins/scriptedwizard/macbrew.h` | Generic helper | **Unchanged.** |
| `src/plugins/scriptedwizard/wiz.cpp` | Script bindings | **Unchanged.** `MacBrewFormula`, `MacBrewInstall`, `MacBrewChoice`, and `MacBrewTry` are already bound on macOS. |
| `src/plugins/scriptedwizard/Makefile.am` | Plugin sources | **Unchanged.** |
| `src/plugins/scriptedwizard/resources/config.script` | Wizard list | **Unchanged.** |
| `src/plugins/scriptedwizard/resources/common_functions.script` | Shared path checks | **Unchanged.** |
| `src/plugins/scriptedwizard/resources/sfml/wizard.script` | SFML client | **Unchanged.** |
| `src/plugins/scriptedwizard/wizpage.cpp` | Generic path page | **Unchanged.** The macOS fix is to not register that page. |
| `src/sdk/uservarmanager.cpp`, `uservardlgs.cpp`, `global_uservars.xrc` | Global Variable Editor | **Unchanged.** |
| `src/templates/common/glfw*`, `src/templates/unix/glfw-static.cbp`, `src/templates/win32/glfw-static.cbp` | GLFW 2 templates | **Unchanged.** |
| `src/plugins/contrib/lib_finder/lib_finder/glfw.xml` | lib_finder | **Unchanged.** |

The running IDE loads wizard scripts and sample files from the installed data directory, not automatically from this source tree. M2’s check uses the copies the IDE actually reads. Copying `wizard.script` and `mac/main.cpp` into the app’s `templates/wizard/glfw/` is an install step, not a source change. A full Code::Blocks rebuild is not required for that copy.

---

## 8. Milestones

v3.0 already built the helper. v3.1 should not grow a parallel M1–M9.

### M1 — Plan

**Objective.** Record the current wizard, the editor’s cause, and the macOS client design.

**Files.** This document only.

**Non-goals.** No source edit, no GLFW install.

**Check.** The document exists and distinguishes evidence from proposal.

**Stop.** Planning questions that need a built GLFW are listed in section 11, not answered by installing during M1.

**Status.** Done by this file.

### M2 — macOS client

**Objective.** Make the GLFW wizard a second client of the existing helper, and make the macOS sample the OpenGL 4.1 core triangle from section 6. On macOS, a suitable prefix skips the location page and the Global Variable Editor. A missing formula uses the existing install dialog under a GLFW label. Windows and other Unix keep `$(#glfw)` and `files/main.cpp`.

**Files.** `glfw/wizard.script` (`GetFilesDir` and the client already started there), `glfw/Makefile.am`, and new `glfw/mac/main.cpp`.

**Non-goals.** No helper API, no SFML edit, no change to `files/main.cpp`, no GLAD/GLEW/GLM, no rpath, no global-variable write, no `brew update` / uninstall / bootstrap.

**Status.** The Homebrew client is in `wizard.script`. The macOS sample and the `GetFilesDir` branch are not.

**Check.** This machine starts M2 with working Homebrew and GLFW absent. That absence is the first run. Do not install GLFW before the wizard asks.

- Enter the GLFW wizard. The macOS path does not open the Global Variable Editor and does not require `$(#glfw)`.
- No `brew install` occurs before explicit consent.
- Cancel once on the consent dialog. No install starts.
- Then choose **Install GLFW using Homebrew**.
- After a successful install, the existing formula query and suitability check rediscover the prefix without restarting Code::Blocks.
- Complete project creation. The copied source is `glfw/mac/main.cpp`. It requests OpenGL 4.1 core, sets `GLFW_OPENGL_FORWARD_COMPAT`, and contains no `glBegin`, `glVertex*`, `glColor*`, `glMatrixMode`, `glLoadIdentity`, `glOrtho`, or other fixed-function matrix call. The `.cbp` contains `prefix/include` and `prefix/lib`, links `-lglfw`, and records the proposed `-framework OpenGL`. It does not contain `$(#glfw)`.
- Start the wizard again. This is the already-installed path: no dependency page, no GLFW location page, no Global Variable Editor.
- With Homebrew missing or `--version` failing: the page does not call that “GLFW missing”. No project is created. No bootstrap.
- A non-zero install exit does not create a project.
- A Windows or other-Unix reading of the same script still registers `GlfwPath` and still links `glfw3` plus that platform’s extra libraries. This can be reviewed from the script when those platforms are not at hand.

**Stop.** Do not add an rpath, a loader, or a third sample to make this milestone pass. If the prefix check cannot be expressed with the existing report fields, stop and name the missing field. Do not add `ExecuteBrewCommand`.

### M3 — Link and run against the real bottle

**Objective.** Build the generated Debug target against the real Homebrew GLFW bottle, confirm the 4.1 core context and the triangle, and inspect load commands.

**Files.** None unless the inspection contradicts section 6. A contradicted link line is a one-line change in `glfw/wizard.script`. An rpath is added only when the inspected install name requires it. The sample changes only when a specific result contradicts section 6.

**Non-goals.** No speculative frameworks. No `DYLD_LIBRARY_PATH`. No debugger work.

**Check.**

- `otool -D` and `otool -L` on `prefix/lib/libglfw.dylib`, plus its `LC_RPATH` load commands.
- The same for `bin/Debug/<name>` after a Code::Blocks build.
- The executable starts from Code::Blocks with no `DYLD_LIBRARY_PATH`.
- Verify the complete link line. Record whether `-framework OpenGL` was sufficient, and whether Cocoa, IOKit, QuartzCore, or CoreFoundation were required by an undefined symbol.
- The created context is OpenGL 4.1 core. The triangle is visible. Escape or the window close control ends the loop. `glfwTerminate` runs and the process exits.

**Stop.** If the dylib id is absolute, add no rpath. If the sample fails, classify the failure before editing it.

### M4 — Debugger smoke

**Objective.** Confirm the ordinary debugger still drives this project. This is a regression check. The SFML work already fixed the macOS debugger defects found on a graphical program.

**Files.** None unless a new reproducible defect appears. A new defect is classified before any debugger change, and a debugger change is outside v3.1 unless it is required for this smoke test to be meaningful.

**Non-goals.** No new GDB campaign, no Program Console redesign, no signing work.

**Check.** Debug build from the wizard. Start under the ordinary debugger. A breakpoint in `main.cpp` is hit. Continue runs. Stop returns through the ordinary path.

**Stop.** One reproducible failure is recorded and classified. It does not by itself expand v3.1.

---

## 9. Acceptance

1. **macOS, Homebrew works, GLFW absent (M2’s first run).** The wizard does not open the Global Variable Editor and does not require `$(#glfw)`. Nothing is installed before explicit consent. Cancel once on the consent dialog does not start `brew`. **Install GLFW using Homebrew** then installs. The existing formula query and suitability check rediscover the prefix without restarting Code::Blocks. Project creation completes.
2. **macOS, second wizard start after that install.** No dependency page. No GLFW location page. No Global Variable Editor. No typed paths. The generated project is the core-profile triangle and it builds.
3. **macOS, Homebrew not detected.** The existing locate / instructions path. No bootstrap. No project.
4. **macOS, Homebrew present but `--version` fails.** The failure is visible and is not reported as “GLFW missing”. No project.
5. **macOS, `brew install glfw` exits non-zero.** No success claim. No project.
6. **macOS cancel.** Cancel before launch does not start Homebrew. Cancel after launch does not claim rollback, does not claim success, and does not create a project. A later attempt reads the machine as it is.
7. **Generated macOS project.** Include directory is `prefix/include`. Library directory is `prefix/lib`. The project records `-lglfw` and the proposed `-framework OpenGL`. M3 verifies the complete link line against the generated program. No `$(#glfw)`. No pkg-config. No `DYLD_LIBRARY_PATH`.
8. **Runtime.** The program creates the requested 4.1 core context. The sample contains no legacy fixed-function OpenGL. The triangle appears. Escape and the window close control work. The process exits cleanly. Library resolution matches the executable’s load commands. No GLAD, GLEW, GLM, or second Homebrew formula was added.
9. **Debugger.** The M4 smoke check passes.
10. **Windows.** The location page and `$(#glfw)` remain. Extra libraries remain `opengl32`, `glu32`, `gdi32`. The copied source remains `files/main.cpp`.
11. **Other Unix.** The location page, `$(#glfw)`, and `GL` / `GLU` / `pthread` / `Xxf86vm` remain. The copied source remains `files/main.cpp`.
12. **SFML.** The v3.0 wizard behaviour remains. The helper’s report strings and install rules remain.

---

## 10. Rejected

- Making the Global Variable Editor easier, or filling a persistent `glfw` variable so the old page still works.
- Any edit to the generic editor, `uservarmanager`, or `wizpage.cpp`.
- Removing `$(#glfw)` from Windows or other Unix.
- Homebrew pages on non-mac platforms.
- SFML edits for symmetry.
- SDL, SDL2, GLUT, the OpenGL wizard, wxWidgets, FLTK, GTK, OGRE, or the other graphics wizards.
- lib_finder, pkg-config as the project’s link line, or a compiler-plugin change.
- Homebrew bootstrap, `brew update`, `brew upgrade`, `brew uninstall`, or a formula browser.
- A general brew argv binding.
- Teaching `macbrew` about `glfw3.h`, `libglfw`, or OpenGL.
- Bundling GLFW in the DMG, editing the shell profile, or setting `DYLD_LIBRARY_PATH`.
- An rpath added without an `otool` result.
- A new GLFW sample written only because SFML needed an SFML 3 sample.
- Preserving `glBegin` / `glEnd` because that sample is shorter.
- Keeping the historical Sierpinski program on macOS for continuity.
- A compatibility-profile context chosen so the old sample still runs.
- A large OpenGL tutorial, a rendering framework, or extra classes around one triangle.
- GLAD, GLEW, or GLM without a demonstrated need. The macOS triangle does not have that need.
- A GLFW version-selection page.
- Modernising the OpenGL wizard, or converting this sample to Metal or Vulkan.
- A debugger, Program Console, or signing change.
- A README announcement.
- Cleanup of the GLFW 2 templates.

---

## 11. Open implementation-time measurements

These need a real Homebrew GLFW prefix. They were not settled by installing during planning.

1. `otool -D`, `otool -L`, and `LC_RPATH` of `prefix/lib/libglfw.dylib` after `brew install glfw`.
2. The same load commands on the wizard-built executable, and whether it starts with no `DYLD_LIBRARY_PATH` and no extra rpath.
3. Whether `-lglfw` plus the proposed `-framework OpenGL` links the triangle. Add Cocoa, IOKit, QuartzCore, or CoreFoundation only if the link reports an undefined symbol from that framework.
4. The context version and profile `glfwCreateWindow` actually returns for the four hints in section 6, and whether `GLFW_OPENGL_FORWARD_COMPAT` had to be removed for creation to succeed.
5. That the triangle is visible, that Escape and the close control end the loop, and that the process exits after `glfwTerminate`.
