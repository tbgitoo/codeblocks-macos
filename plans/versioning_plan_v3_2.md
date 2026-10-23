# Code::Blocks macOS v3.2 — existing SDL2 wizard becomes SDL3 on Apple Silicon

**Status: M1 done.** This document is the milestone. No source edit. SDL3 was not installed.

**Question this version answers:** what is the smallest coherent change that makes the existing SDL2 project wizard create an SDL3 project in this native Apple Silicon distribution, with SDL3 discovered automatically through the Homebrew wizard infrastructure established in v3.0?

**Product invariant, macOS only:** creating the project must not require the beginner to locate an `SDL2.framework`, to configure `$(#sdl2)` or `$(#sdl3)`, or to use the Global Variable Editor. A validated Homebrew SDL3 prefix supplies the include directory, the library directory, and the link name. Windows keeps a location page, now for SDL3. Other Unix keeps a config-tool line, now for SDL3. The separate SDL 1 wizard stays.

This wizard is the third client of the v3.0 helper. v3.2 does not add a second SDL wizard beside the SDL2 one. The registered SDL2 entry becomes the SDL3 entry. v3.2 does not change SFML, GLFW, the helper, or the global-variable system.

Upstream baseline remains Code::Blocks trunk **r14050** / IDE **25.03**.

Evidence labels used below: **CONFIRMED FROM SOURCE**, **CONFIRMED FROM LOCAL MACHINE**, **ASSUMPTION**, **PROPOSED DESIGN**, **TO BE MEASURED DURING IMPLEMENTATION**.

The method follows v3.1. The milestones do not. GLFW’s OpenGL core sample had to be macOS-only because Windows `opengl32` does not export those entry points. SDL3’s 2D renderer is the same call set on each platform, so this version changes the wizard’s library identity everywhere and limits the Homebrew client to macOS.

---

## 1. Existing SDL2 implementation map

### Entry the user sees

File → New → Project lists wizards from `src/plugins/scriptedwizard/resources/config.script`.

```47:48:src/plugins/scriptedwizard/resources/config.script
    RegisterWizard(wizProject,     _T("sdl"),          _("SDL project"),           _("2D/3D Graphics"));
    RegisterWizard(wizProject,     _T("sdl2"),         _("SDL2 project"),          _("2D/3D Graphics"));
```

Category **2D/3D Graphics**, title **SDL2 project**. No `PLATFORM` guard. The separate **SDL project** entry is the SDL 1 wizard in `resources/sdl/`. **CONFIRMED FROM SOURCE.**

`src/plugins/scriptedwizard/resources/sdl2/Makefile.am` installs `wizard.script`, `logo.png`, `wizard.png`, and `files/main.cpp` under `templates/wizard/sdl2/`. `resources/Makefile.am` lists `sdl2` as a subdirectory.

`PLATFORM` constants (`src/sdk/scripting/bindings/sc_consts.cpp`): `PLATFORM_MSW` 0, `PLATFORM_GTK` 1, `PLATFORM_MAC` 2, `PLATFORM_X11` 4.

### Wizard pages

`BeginWizard` in `src/plugins/scriptedwizard/resources/sdl2/wizard.script`:

1. Info page `SDL2Intro`. The text says “SDL2 project wizard” and “the SDL2 library”.
2. Project path page.
3. Location page `SDL2Path` only when `PLATFORM == PLATFORM_MSW`. Default text `$(#sdl2)`. The description points at the MinGW `i686-w64-mingw32` or `x86_64-w64-mingw32` folder inside an unpacked SDL2 tree.
4. Compiler page. Default output dirs are `bin/Debug` and `bin/Release`.

There is no version page and no static/dynamic page. On macOS the location page is not registered.

### What `SetupProject` writes

| Platform | What the project gets |
|----------|------------------------|
| `PLATFORM_MSW` | Include `$(#sdl2.include)` (or the folder the leave handler computed), lib dir likewise, link `mingw32`, `SDL2main`, `SDL2.dll`, `user32`, `gdi32`, `winmm`, `dxguid`, and a post-build `XCOPY` of `$(#sdl2)\bin\*.dll`. |
| `PLATFORM_MAC` | Linker option `-framework SDL2`, link library `SDL2main`, linker option `-framework Cocoa`. No include directory. No library directory. |
| everything else | Compiler option `` `sdl2-config --cflags` `` and linker option `` `sdl2-config --libs` ``. |

`GetFilesDir` returns `sdl2/files` on every platform. `WarningsOn` for the selected compiler. Debug is `ttConsoleOnly`. Release is `ttExecutable`. No `-std`. No rpath. No working directory.

### Sample

`sdl2/files/main.cpp` is a small C++ program. It includes `<SDL.h>`. `SDL_Init` is treated as failed when the result is not 0. It calls `SDL_CreateWindowAndRenderer(640, 480, SDL_WINDOW_SHOWN, &window, &renderer)`, then draws eight colour bars with `SDL_Rect`, `SDL_SetRenderDrawColor`, `SDL_RenderFillRect`, `SDL_RenderPresent`, and `SDL_Delay(500)`. The destructor calls `SDL_DestroyWindow`, then `SDL_DestroyRenderer`, then `SDL_Quit`. The exception methods use the dynamic exception specification `throw()`. **CONFIRMED FROM SOURCE.**

### SDL 1 wizard

`resources/sdl/` is a different wizard: `#include <SDL/SDL.h>`, `SDL_SetVideoMode`, `-framework SDL`, and `` `sdl-config` ``. v3.2 does not edit it.

---

## 2. Baseline macOS behaviour

The macOS branch does not open the Global Variable Editor. It never registers `SDL2Path`, so nothing expands `$(#sdl2)` before a page handler. The failure mode is the link line.

That line is the official SDL2 framework SDK: `-framework SDL2`, `-lSDL2main`, `-framework Cocoa`. A comment in the script says `libSDL2main.a` is not present by default and has to be built from `SDL2Main.m`. **CONFIRMED FROM SOURCE.**

What is installed on this machine is not that framework. **CONFIRMED FROM LOCAL MACHINE**, 23 October 2026, nothing installed by this plan:

- `/opt/homebrew/opt/sdl2` → `../Cellar/sdl2/2.32.10`. Headers are under `include/SDL2/`. `SDL_version.h` defines `SDL_MAJOR_VERSION` 2, `SDL_MINOR_VERSION` 32, `SDL_PATCHLEVEL` 10.
- `otool -D` on `lib/libSDL2.dylib` reports `/opt/homebrew/opt/sdl2/lib/libSDL2-2.0.0.dylib`.
- `otool -L` shows that dylib already linked to Cocoa, Carbon, IOKit, CoreAudio, AudioToolbox, Metal, QuartzCore, and the other system libraries it needs. The executable the wizard would generate is not given an include dir or a `-L` for this prefix, so this install does not satisfy the wizard.
- `/opt/homebrew/opt/sdl3` and `/opt/homebrew/include/SDL3` are absent.

`brew info sdl2` on the same day reports the current formula as **sdl2-compat** 2.32.10 → stable **2.32.74**, “SDL2 compatibility layer that uses SDL3 behind the scenes”, with alias `sdl2`, and the installed bottle above. `brew info sdl3` reports **sdl3** stable **3.4.18** (bottled), “Not installed”, homepage `https://libsdl.org/`. An `arm64_tahoe` bottle is listed. **CONFIRMED FROM LOCAL MACHINE.**

So the name `sdl2` in today’s Homebrew is no longer the current SDL library. The wizard must query formula `sdl3`. A prefix that contains `SDL2/SDL.h` and `libSDL2.dylib` is not a suitable SDL3 prefix. This plan does not upgrade, replace, or remove the installed SDL 2.32.10 bottle.

---

## 3. v3.0 Homebrew infrastructure available for reuse

The helper is unchanged from v3.0 and v3.1. Detection order, `--version` revalidation, the installed-prefix query, the consent dialog, cancellation, the password stop, and the report strings are generic. The script bindings are `Wizard.MacBrewProbe`, `MacBrewFormula`, `MacBrewTry`, `MacBrewChoice`, and `MacBrewInstall`.

Formula tokens allow letters, digits, and `.` `_` `+` `-` `@` `/`. The token `sdl3` is one token of letters and digits. **CONFIRMED FROM SOURCE** of `FormulaTokenOk` in `macbrew.cpp`.

SFML and GLFW each parse the report strings inside their own `wizard.script`. SDL3 does the same under its own names. Sharing a squirrel parser would be a new facility. v3.2 does not add one, and it does not edit the SFML or GLFW scripts.

No missing generic capability. **PROPOSED DESIGN:** leave `macbrew.cpp`, `macbrew.h`, `wiz.cpp`, and `scriptedwizard/Makefile.am` unchanged.

---

## 4. SDL3 formula and the calls the sample must change

Measured on 23 October 2026. Nothing was installed. Header and migration text are the tag **release-3.4.18**, which is the tarball named in Homebrew’s `Formula/s/sdl3.rb` at `homebrew-core` HEAD.

### Formula

**CONFIRMED FROM SOURCE** of that formula:

- Stable tarball `SDL3-3.4.18.tar.gz`. Build-time dependencies: `cmake`, `pkgconf`. No macOS runtime formula dependencies. The X11, Wayland, Mesa, and PulseAudio dependencies are inside `on_linux`.
- One CMake build, then `cmake --install`. `BUILD_SHARED_LIBS` is not set in the formula. SDL’s own `CMakeLists.txt` at the same tag, when `BUILD_SHARED_LIBS` is unset, turns the static default off and builds the shared library. The shared target’s `OUTPUT_NAME` is `SDL3`.
- The formula’s own test is:

```c
#include <SDL3/SDL.h>
int main() {
  if (SDL_Init(SDL_INIT_VIDEO) != 1) {
    return 1;
  }
  SDL_Quit();
  return 0;
}
```

compiled as `cc test.c -I#{include} -L#{lib} -lSDL3`. It does not pass `-framework`, `-lSDL2`, `-lSDL3main`, or a config tool. Success of `SDL_Init` is `1`, which is `true`. The SDL2 sample’s `!= 0` test treats that success as failure.

### Calls that do not survive a recompile

**CONFIRMED FROM SOURCE** of the 3.4.18 headers and `docs/README-migration.md` at that tag:

| SDL2 sample | SDL 3.4.18 |
|-------------|------------|
| `#include <SDL.h>` | `#include <SDL3/SDL.h>`. The migration guide also says the file that contains `main` includes `<SDL3/SDL_main.h>`, because `SDL.h` no longer includes it. |
| `SDL_Init` failed when the result was not 0 | `SDL_Init` returns `bool`. The formula test treats anything other than `1` as failure. |
| `SDL_INIT_VIDEO \| SDL_INIT_TIMER` | `SDL_INIT_TIMER` is removed. Timers are part of the library. The sample flag is `SDL_INIT_VIDEO`. |
| `SDL_CreateWindowAndRenderer(w, h, flags, &win, &ren)` | First parameter is the window title: `(const char *title, int width, int height, SDL_WindowFlags, SDL_Window **, SDL_Renderer **)`. Returns `bool`. |
| `SDL_WINDOW_SHOWN` | Removed. A window is shown unless created with `SDL_WINDOW_HIDDEN`. |
| `SDL_Rect` passed to `SDL_RenderFillRect` | `SDL_RenderFillRect(SDL_Renderer *, const SDL_FRect *)`. `SDL_FRect` is `float x, y, w, h`. The integer fill call was replaced by the old float call. Returns `bool`. |
| `SDL_SetRenderDrawColor`, `SDL_RenderClear`, `SDL_RenderPresent` | Same names. Each returns `bool`. |
| `SDL_Delay(500)` | Still declared: `void SDL_Delay(Uint32 ms)`. |
| Link `SDL2main` / `-framework SDL2` | “The SDLmain library has been removed, it's been entirely replaced by SDL_main.h.” The migration makefile snippet is `pkg-config sdl3 --cflags` and `pkg-config sdl3 --libs`. The formula test is `-lSDL3`. |
| macOS `main` | `SDL_main.h` redefines `main` only for the platforms it lists (Win32, GDK, iOS, tvOS, Android, Emscripten, PSP, PS2, 3DS). There is no macOS branch. Including the header in the `main` file follows the migration guide and does not replace `main` on this platform. |

`SDL_DestroyRenderer` and `SDL_DestroyWindow` are still declared. The SDL2 sample destroys the window and then the renderer. **PROPOSED DESIGN:** destroy the renderer, then the window, then `SDL_Quit`. M3 confirms that order against the built program.

### Four states

1. **Homebrew available.** `MacBrewFormula` access `works`.
2. **Formula installed.** State `installed`. The formula argument is `sdl3`, not `sdl2`.
3. **Prefix found.** One absolute directory line. The helper does not inspect it.
4. **Prefix suitable for this wizard.** Client check, **PROPOSED DESIGN**:
   - Prefix is non-empty and starts with `/`.
   - `prefix/include/SDL3/SDL.h` exists.
   - `prefix/lib/libSDL3.dylib` exists.

`libSDL3.a` alone is not suitable. The formula’s default build is the shared library, and the formula test links `-lSDL3` against that build. The installed SDL 2.32.10 tree fails this check. State `missing` remains the only “not installed” result. A failed query is not “SDL3 is not installed”.

No version parse. The sample is written against the 3.4.18 signatures above. A newer bottle that still provides those signatures needs no second sample.

---

## 5. Proposed design

### Wizard identity

**PROPOSED DESIGN.** The resource directory `sdl2` becomes `sdl3`. `config.script` registers id `sdl3` with title **SDL3 project**, same category. The intro text says SDL3. There is no remaining **SDL2 project** entry. The SDL 1 wizard is not renamed and not removed.

Keeping the directory name `sdl2` while the generated project includes `SDL3/SDL.h` would leave the wrong library in the path Code::Blocks loads. Adding a new SDL3 wizard beside the SDL2 one would keep generating the framework link line this distribution cannot satisfy.

Windows and other Unix change with the identity. A wizard titled SDL3 that still emits `SDL2main` or `` `sdl2-config` `` on those platforms is still an SDL2 wizard there.

### Pages

1. Info page. SDL3 wording.
2. On `PLATFORM_MAC` only, query formula `sdl3`.
   - Suitable prefix: store include `prefix/include` and lib `prefix/lib`. Add no diagnostic page and no location page.
   - Anything else: one info page, same role as the GLFW dependency page, with SDL3 wording. No project is created from that page.
3. Project path page. Still registered. Forward leave on macOS repeats the query and refuses to continue when the prefix is not suitable.
4. Location page only when `PLATFORM == PLATFORM_MSW`. The page name, the default text, and the checks become SDL3: default `$(#sdl3)`, header `SDL3/SDL.h` under the include directory, library stem `SDL3`. The description no longer tells the user to pick an SDL2 MinGW folder. `OnLeave` of that page stays the Windows path. It is not registered on macOS, so nothing expands `$(#sdl2)` or `$(#sdl3)` there.
5. Compiler page. Forward leave on macOS repeats the same check.

### Dependency page

Wording follows the helper’s states.

- Access `works` and state `missing`: Homebrew works and SDL3 is not installed. Next leads to the existing consent dialog. The button label is **Install SDL3 using Homebrew**. The explanation names the formula `sdl3` and says Cancel does not start Homebrew. After `success=1`, the query and the prefix check run again. A suitable prefix continues the wizard without restarting Code::Blocks. A still-unsuitable prefix stays on the explanation and does not create a project.
- Access is not `works`: Homebrew was not usable. Do not say SDL3 is missing. Next uses `MacBrewChoice`: **Help me find Homebrew** (`MacBrewTry`, then write `/macos/homebrew/executable` only when access is `works`) or **Installation instructions** (`https://brew.sh`). No Homebrew bootstrap.
- Access `works` and state `failed`, `rejected`, or `installed` with a failed suitability check: show that result. Do not offer install as if the formula were absent. Do not create a project. Do not offer to install or upgrade formula `sdl2`.

Cancellation stays the helper’s:

- Cancel on the consent dialog: brew was not started, no project.
- Cancel after the install process has started: the helper stops that process and does not undo files already written. The script does not report success and does not create a project.
- A password prompt or a non-zero exit: not success, no project.

### `SetupProject` on macOS

When the stored include or lib is empty, return false.

Otherwise, for both targets:

- `AddIncludeDir` of `prefix/include`.
- `AddLibDir` of `prefix/lib`.
- `AddLinkLib("SDL3")`, which becomes `-lSDL3`, matching the formula test.

Do not add `-framework SDL2`, `-framework SDL3`, `-framework Cocoa`, `-framework OpenGL`, or `-framework Metal`. Do not link `SDL2main` or `SDL3main`. The migration guide says the SDLmain library is gone. The installed SDL 2.32.10 dylib already carries Cocoa and the other system libraries inside itself; whether the SDL3 dylib does the same is **TO BE MEASURED DURING IMPLEMENTATION**. They are not part of the proposed application link line unless M3 reports an undefined symbol.

Do not set a working directory. The sample loads no file.

Do not add `-std`. The sample uses no language feature past the C++14 default already measured for this compiler in v3.1.

Do not write `$(#sdl2)`, `$(#sdl3)`, or a pkg-config backtick into the macOS project. Do not create or fill a persistent global variable.

No rpath in the wizard until `otool` of the installed `libSDL3.dylib` shows an install name that needs one. That dylib is not on this machine yet.

### Windows and other Unix

**PROPOSED DESIGN**, reviewed from the script. This machine is not those platforms.

- Windows keeps the location page and `$(#sdl3)`. Include and lib directories come from that page, as they do today. Link `SDL3` only. Do not link `mingw32`, `SDL2main`, `SDL2.dll`, `user32`, `gdi32`, `winmm`, or `dxguid` by copying the SDL2 line. The post-build `XCOPY` of `$(#sdl2)\bin\*.dll` becomes an `XCOPY` of `$(#sdl3)\bin\*.dll` only if that is still how the official Windows development archive is laid out. If it is not, drop the copy rather than invent a path.
- Other Unix replaces the two backticks with `` `pkg-config --cflags sdl3` `` and `` `pkg-config --libs sdl3` ``. That is the makefile snippet in the SDL 3.4.18 migration guide. `` `sdl2-config` `` would still generate an SDL2 project.

### Generated project

A normal Code::Blocks project. Absolute Homebrew prefix paths in the macOS `.cbp` are the intended result, as they were for SFML and GLFW. No shell profile, no `brew` on `PATH` at build time, no `DYLD_LIBRARY_PATH`, no lib_finder.

`GetFilesDir` returns `sdl3/files` on every platform. One sample. The renderer calls do not need a platform loader.

---

## 6. Build, link, and sample

| Item | Proposal | Evidence |
|------|----------|----------|
| Source | One `sdl3/files/main.cpp`, all platforms | The 2D renderer signatures are the same in the 3.4.18 headers. Unlike the GLFW core-profile triangle, Windows does not need a second library to link them. |
| Include, macOS | `prefix/include` so `<SDL3/SDL.h>` resolves | Formula test uses `-I#{include}` and that include. |
| Library dir, macOS | `prefix/lib` | Formula test uses `-L#{lib}`. |
| Link, macOS | `-lSDL3` | Formula test. Shared `OUTPUT_NAME` is `SDL3`. **PROPOSED DESIGN** until M3 links the real bottle. |
| Frameworks | None on the executable | Formula test passes none. Old `-framework SDL2`, `SDL2main`, and `-framework Cocoa` belong to the framework SDK this prefix is not. |
| `SDL_main.h` | Include it in the `main` file | Migration guide. macOS is not in the header’s `main`-redefine list. |
| Language | No extra `-std`. No `throw()` | C++14 is the default here. Dynamic exception specifications are the obsolete part of the current sample, and the SDL3 calls return `bool`. |
| Rpath | None until a measurement requires one | No SDL3 dylib is installed. |
| Resources | None | The sample loads no file. |
| Debugger | Ordinary Debug on the generated project | No SDL debugger mode. |

### Sample

The visible program stays the small one: a 640×480 window and eight colour bars, each held for half a second, then the program exits. That is the current SDL 2D renderer. It is not kept because the SDL2 source is shorter. The signatures change as in section 4.

**PROPOSED DESIGN**, the body of `main`:

- Include `<SDL3/SDL.h>` and `<SDL3/SDL_main.h>`.
- `SDL_Init(SDL_INIT_VIDEO)`. Failure is `!SDL_Init(...)`, then print `SDL_GetError()` and return 1.
- `SDL_CreateWindowAndRenderer("SDL3", 640, 480, 0, &window, &renderer)`. Flag 0 leaves the window shown. Failure uses the same error path.
- Clear, then the eight bars. The rectangle is an `SDL_FRect` with float `x`, `y`, `w`, `h`. `SDL_RenderFillRect` and `SDL_RenderPresent` after each bar. `SDL_Delay(500)` between bars.
- `SDL_DestroyRenderer`, then `SDL_DestroyWindow`, then `SDL_Quit`.

No event loop, no callback entry point (`SDL_MAIN_USE_CALLBACKS`), no shader, no texture, no `SDL_GPU`, no Metal, no OpenGL. The colour bars are the current drawing model. A callback main or a GPU sample would be a second program.

The class named `SDL` and the `InitError` type may stay if they still make the error path shorter, without `throw()`. They are not required. `if (!call)` plus `SDL_GetError()` matches the formula test and the `bool` returns.

---

## 7. Files

| File | Role | v3.2 |
|------|------|------|
| `src/plugins/scriptedwizard/resources/sdl2/` | SDL2 wizard | **Removed as the SDL2 wizard.** Its contents move to `sdl3/` and are rewritten. `logo.png` and `wizard.png` move with the directory. |
| `src/plugins/scriptedwizard/resources/sdl3/wizard.script` | SDL3 client | **New**, from the old script. macOS query, suitability check, dependency page, install labels, absolute include/lib, `-lSDL3`. Windows location page and other-Unix pkg-config line as in section 5. |
| `src/plugins/scriptedwizard/resources/sdl3/files/main.cpp` | Sample | **New.** Section 6. No `SDL_WINDOW_SHOWN`, no `SDL_INIT_TIMER`, no `SDL_Rect` fill, no `!= 0` success test, no `throw()`. |
| `src/plugins/scriptedwizard/resources/sdl3/Makefile.am` | Install rule | **New.** `templates/wizard/sdl3`, including `files/main.cpp`. |
| `src/plugins/scriptedwizard/resources/Makefile.am` | Subdirs | **Changes.** `sdl2` becomes `sdl3`. |
| `src/plugins/scriptedwizard/resources/config.script` | Menu entry | **Changes.** The `sdl2` / “SDL2 project” registration becomes `sdl3` / “SDL3 project”. The `sdl` / “SDL project” line stays. |
| `src/plugins/scriptedwizard/resources/sdl/` | SDL 1 wizard | Unchanged. |
| `macbrew.cpp`, `macbrew.h`, `wiz.cpp`, `scriptedwizard/Makefile.am` | Helper | Unchanged. |
| `sfml/wizard.script`, `glfw/wizard.script` | Other clients | Unchanged. |
| `common_functions.script`, `wizpage.cpp`, uservarmanager | Shared wizard machinery | Unchanged. The macOS path does not use the location page. |

---

## 8. Milestones

v3.0 built the helper. v3.1 made GLFW a client and replaced the macOS sample’s rendering model. v3.2 does not repeat either of those as a template. The work is the library identity, the macOS prefix, and the SDL3 signatures.

### M1 — Plan

**Objective.** Record the SDL2 wizard, why its macOS link line cannot use the installed Homebrew SDL, and the SDL3 client design.

**Files.** This document only.

**Non-goals.** No source edit, no SDL3 install, no change to the installed SDL 2.32.10 bottle.

**Check.** The document exists and distinguishes evidence from proposal.

**Stop.** Questions that need a built SDL3 are in section 11.

**Status.** Done by this file.

### M2 — SDL3 client and sample

**Objective.** Replace the SDL2 wizard with the SDL3 client in section 5 and the sample in section 6. On macOS, a suitable prefix skips any location page and the Global Variable Editor. A missing formula uses the existing install dialog under an SDL3 label.

**Files.** The `sdl3` wizard files, `resources/Makefile.am`, and `config.script`, as in section 7.

**Non-goals.** No helper API, no SFML or GLFW edit, no SDL 1 edit, no rpath, no global-variable write, no `brew update`, no uninstall, no bootstrap, no install of formula `sdl2` or `sdl2-compat`.

**Check.** This machine starts M2 with working Homebrew, SDL3 absent, and SDL 2.32.10 present. That absence is the first run. Do not install SDL3 before the wizard asks. The installed SDL 2 tree must not count as success.

- Enter the SDL3 wizard. The macOS path does not open the Global Variable Editor and does not require `$(#sdl3)` or `$(#sdl2)`.
- No `brew install` occurs before explicit consent.
- Cancel once on the consent dialog. No install starts.
- Then choose **Install SDL3 using Homebrew**. The formula argument is `sdl3`.
- After a successful install, the existing formula query and suitability check rediscover the prefix without restarting Code::Blocks.
- Complete project creation. The copied source is the section 6 sample. The `.cbp` contains `prefix/include` and `prefix/lib` and links `-lSDL3`. It does not contain `-framework SDL2`, `SDL2main`, `-framework Cocoa`, `$(#sdl2)`, or `$(#sdl3)`.
- Start the wizard again. This is the already-installed path: no dependency page, no location page, no Global Variable Editor.
- With Homebrew missing or `--version` failing: the page does not call that “SDL3 missing”. No project is created. No bootstrap.
- A non-zero install exit does not create a project.
- The Windows location page and the other-Unix pkg-config lines are reviewed from the script.

### M3 — Real bottle, real link, real window

**Objective.** Build the wizard project against the installed SDL 3.4.x bottle, or whatever stable bottle the M2 install actually poured. Confirm the link line. Run the colour bars.

**Files.** None unless the link or the window shows the proposal wrong. A wrong framework, a wrong library stem, or a wrong destroy order is fixed here and written back into this plan as measured. No new dependency is added because a command-line example mentioned it.

**Check.**

- Suitability files exist: `include/SDL3/SDL.h` and `lib/libSDL3.dylib`.
- `otool -D` and `otool -L` on that dylib. Record the install name and which system libraries the dylib already loads.
- The wizard binary links with the proposed `-lSDL3` and the absolute lib dir. Record every undefined-symbol failure. Add a framework only for a symbol the sample or the SDL3 headers actually reference and the link reports missing.
- The process starts with `DYLD_LIBRARY_PATH` unset. If the install name is absolute, add no rpath.
- `SDL_Init` succeeds. The window appears. The bars are drawn in order. The process then exits. Record the renderer name SDL chose. Do not force Metal or OpenGL in the sample to match that name.
- Apple clang warnings are recorded. Do not add a silence macro unless a warning is an error.

### M4 — Debugger smoke

**Objective.** One ordinary Debug session on the M3 binary with the bundled GDB. Break in `main`, continue, stop.

**Files.** None unless the smoke fails inside the wizard project itself. A debugger defect is out of scope; record it and stop.

**Non-goals.** No new debugger mode, no change to signing, no solib policy change.

---

## 9. Acceptance

v3.2 is done when all of these hold:

- The project wizard entry is **SDL3 project**. There is no **SDL2 project** entry. **SDL project** is still the SDL 1 wizard.
- On this Mac, with Homebrew working, the user can create a project whether or not SDL3 was already installed, without the Global Variable Editor and without choosing a framework or a prefix by hand.
- The generated macOS project includes `prefix/include`, searches `prefix/lib`, and links `-lSDL3`.
- The generated `main.cpp` is the section 6 program. It compiles against the installed SDL3 headers. It does not use `SDL_WINDOW_SHOWN`, `SDL_INIT_TIMER`, an `SDL_Rect` fill, or an SDL2 success test.
- The built program opens a window, draws the bars, and exits.
- Windows still has a location page, and that page is for SDL3. Other Unix still uses a config tool, and that tool is `pkg-config` for `sdl3`.
- The helper, SFML, GLFW, and the SDL 1 wizard are unchanged.

Success is not “the SDL2 sample still compiles”, and it is not “the project links the installed SDL 2.32.10 or sdl2-compat”.

---

## 10. Rejected

- Leaving the menu title and the link line on SDL2, including pointing macOS at the installed 2.32.10 bottle or at formula `sdl2` / `sdl2-compat`.
- Keeping an SDL2 wizard and adding an SDL3 wizard beside it.
- Renaming or deleting the SDL 1 wizard.
- Sending the macOS user to the Global Variable Editor, or writing `$(#sdl3)` into the macOS project.
- `-framework SDL2`, `-framework Cocoa`, or `SDL2main` carried over because the old macOS branch had them.
- `pkg-config` or `sdl3-config` inside the macOS project. Other Unix may use `pkg-config`. macOS uses the prefix.
- An SDL_GPU, Metal, OpenGL, or callback-main sample. The colour bars are the current 2D model.
- A version page, a static/shared page, or a second sample directory.
- A GL loader, an extra Homebrew formula, or a C++ standard flag copied from the SFML wizard.
- A speculative rpath.
- Editing `macbrew.cpp` to know SDL header names, library names, or version numbers.
- `brew install` during planning, or any install before the consent dialog in M2. No `brew update`, no uninstall, no bootstrap, and no upgrade of the installed SDL 2 bottle as part of this wizard.
- A debugger campaign. M4 is a smoke test.

---

## 11. Open implementation-time measurements

These stay open until M3. They are not a reason to install SDL3 during planning.

- Install name of `libSDL3.dylib`, and whether it is absolute.
- `otool -L` of that dylib: which of Cocoa, Metal, and the other system libraries it already loads.
- Whether `-lSDL3` alone links the colour-bar program. Any added framework has to correspond to an undefined symbol from that link.
- Whether including `<SDL3/SDL_main.h>` changes the macOS entry point. The header’s platform list says it does not.
- Destroy order: renderer then window, against the running program.
- The renderer name SDL selects when the create call passes no driver name.
- Whether the window is shown and the eight bars appear before the process exits.
- Warnings from the wizard’s compiler settings. Record them. Do not treat an SDL deprecation note as a reason to switch the sample to another graphics API.
