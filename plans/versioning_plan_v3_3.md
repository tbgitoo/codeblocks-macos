# Code::Blocks Windows v3.3 — SFML, GLFW, and SDL3

**Status: planning.** No code in this version yet. v3.0, v3.1, and v3.2 stay the macOS plans. This file is their Windows implementation, one version for all three.

**Question this version answers:** what is the smallest change that gives the existing SFML, GLFW, and SDL3 wizards the same automatic setup on Windows that Homebrew gives them on macOS?

The user creates a project. A missing library is installed only after an explicit button. The button names the library. It does not name a zip, a compiler vendor, an exception model, or a folder the user must browse to. A later run finds the installed tree and adds no dependency page. The generated project is a normal Code::Blocks project with absolute include and library directories. Build does not depend on `PATH`, on `$(#sfml)` / `$(#glfw)` / `$(#sdl3)`, or on the Global Variable Editor.

Upstream baseline remains Code::Blocks trunk **r14050** / IDE **25.03**. The compiler this plan is written against is the MinGW in the official 25.03 installer: WinLibs GCC 14.2.0, UCRT, 64-bit SEH, master path `D:\Program Files\CodeBlocks\MinGW` on the machine used to write this plan. The wizard reads the compiler the IDE will use to build the project. It does not read the first `gcc` on `PATH`. An MSYS2 GCC earlier on `PATH` must not choose the archive.

Evidence labels: **CONFIRMED FROM SOURCE** (this tree or a published page), **CONFIRMED FROM WEB** (23 October 2026), **PROPOSED DESIGN**.

---

## 1. What is the same, and what is not

The page order, the consent rule, the cancel rule, and the “already installed” path are the macOS ones. The thing being installed is not Homebrew.

| macOS | Windows |
|-------|---------|
| `Wizard.MacBrewFormula` / `MacBrewInstall` | A Windows-only helper. It is not a stub of `macbrew.cpp`, and `macbrew.cpp` grows no Windows code. |
| Formula name `sfml`, `glfw`, `sdl3` | One HTTPS zip per library, chosen from the compiler identity. The script holds the URL. The helper does not know header names. |
| Prefix `/opt/homebrew/opt/<formula>` | A directory the helper unpacked under the user profile, not under `D:\Program Files\CodeBlocks`. |
| No location page when the prefix validates | The version list, the static/dynamic page, and the location page are not registered on that path. |
| Homebrew missing: locate `brew`, or open https://brew.sh. No bootstrap. | Compiler does not match the archive: no download. The existing location page is the way to point at a tree the user already has. There is no installer to locate and no package manager to bootstrap. |
| Dynamic library, absolute install name, no rpath | Dynamic library. Copy the needed DLLs next to the executable. There is no rpath step. |

Other Unix is unchanged: SFML’s commented `pkg-config`, GLFW’s `GL` / `GLU` / `pthread` / `Xxf86vm` and `glfw/files/main.cpp`, SDL3’s `pkg-config` lines. **SDL project** (SDL 1) is removed on Windows as well. It stays registered and installed on Unix. v3.2 M5 already retired it on macOS. The sources under `resources/sdl/` stay in the tree for that Unix build.

---

## 2. Helper

New `src/plugins/scriptedwizard/winsdk.cpp` and `winsdk.h`, listed in `scriptedwizard/Makefile.am`, bound in `wiz.cpp` only inside `#if defined(__WXMSW__)`. Same place `MacBrew*` is bound inside `#if defined(__WXMAC__)`. Linux gets neither.

The helper does four things.

1. **Compiler identity.** From the compiler id the wizard is using (`Wizard.GetCompilerID()`, otherwise the default compiler), read that compiler’s master path and run `<master>\bin\gcc.exe`, not a `gcc` found on `PATH`. Report `dumpversion`, `dumpmachine`, and whether `gcc -v`’s configuration line contains `ucrt`. Report `works` only when that executable runs. This is the Windows M2: no download.
2. **Saved tree.** A library id token (`sfml`, `glfw`, `sdl3`, same character rule as a Homebrew formula token) maps to the directory unpacked last time. The helper does not look inside it.
3. **Fetch.** Arguments are the URL, the library id, the explanation, and the button label. The dialog is shown before any transfer. Cancel on that dialog does not connect. After the transfer starts, Cancel stops that transfer only, does not report success, does not delete a partial unpack, and does not claim the disk was rolled back. Success is a finished download, a finished unzip, and exit without cancel. The IDE environment is not changed. No percentage unless the response has a length; do not invent one.
4. **Nothing else.** No search, no upgrade, no uninstall, no `winget`, no `pacman`, no `vcpkg`, no compile-from-source.

Unpack root is one directory under `%LOCALAPPDATA%\codeblocks\sdk\<id>`. Official Code::Blocks in `D:\Program Files\CodeBlocks` is not a destination. The client walks the unpack root and one subdirectory to find the marker files below. Zip archives wrap that marker in a versioned folder; the helper must not require the script to know that folder’s name.

---

## 3. Versions

**CONFIRMED FROM WEB** (23 October 2026). These are the archives. The wizard page does not show the filenames.

| Wizard | macOS, already shipped | Windows archive | Why this far and no further |
|--------|------------------------|-----------------|-----------------------------|
| SFML | Homebrew formula at implementation was **3.0.2** | **3.1.0**, `SFML-3.1.0-windows-gcc-14.2.0-mingw-64-bit.zip` (32-bit: the `-mingw-32-bit.zip`). <https://www.sfml-dev.org/files/SFML-3.1.0-windows-gcc-14.2.0-mingw-64-bit.zip> | Newest SFML release. SFML’s download page says this zip matches WinLibs UCRT GCC 14.2.0 64-bit SEH, and no other GCC. The Visual C++ zip is the wrong file. |
| GLFW | Homebrew **3.5.1** | **3.5.1**, `glfw-3.5.1.bin.WIN64.zip` (32-bit: `glfw-3.5.1.bin.WIN32.zip`). <https://github.com/glfw/glfw/releases/download/3.5.1/glfw-3.5.1.bin.WIN64.zip> | Current GLFW release (31 July 2026). The official Windows binaries include MinGW-w64. GLFW is C, so the SFML “compiler must match 100%” rule does not apply. |
| SDL3 | Homebrew **3.4.18** | **3.4.18**, `SDL3-devel-3.4.18-mingw.zip` (both architectures inside). <https://libsdl.org/release/SDL3-devel-3.4.18-mingw.zip> | Current SDL3 release (2 October 2026), same version the macOS sample was written for. There is no newer release to bump to. |

Rejected for all three: winget, Chocolatey, Scoop, MSYS2, vcpkg’s MSVC triplet, and the Visual C++ archive of the same library. Also rejected: building the library with CMake inside the wizard.

---

## 4. SFML — v3.0’s milestones on Windows

The sample stays `sfml/sfml3/main.cpp`. It was written for 3.0.2 (`sf::VideoMode({800, 600})`, `sf::Sprite(texture)`, `pollEvent` optional, `event->is<sf::Event::Closed>()`). 3.1.0 is a minor release of that API. Do not add a 3.1-only sample. If a 3.1.0 header rejects one of those calls, fix that call so 3.0.2 still accepts it, or stop and record the line. Do not fork the sample.

Suitability, dynamic only: `include/SFML/Graphics.hpp`, `include/SFML/Config.hpp`, `SFML_VERSION_MAJOR` 3, and import libraries `libsfml-graphics.dll.a`, `libsfml-window.dll.a`, `libsfml-system.dll.a`. A static `libsfml-graphics.a` with no import library is not suitable. Major 1 or 2 is not suitable on this path: the download is 3.1.0, and the 1.6 / 2.0 list is not shown. An unreadable version stops.

**CONFIRMED FROM WEB** (SFML FAQ): a missing dynamic module is reported as `sfml-graphics-3.dll` (and window, system). Debug builds of the official zip are not what that page’s release archives are. At the first unpack, list `lib` and `bin`. If no `sfml-*-d` import library exists, leave `SFMLDebugLib` unset. Do not invent `-d` or `-s`.

Link line, both targets, from the existing Windows branch, dynamic arm only: include and lib dirs of the unpacked tree; `mingw32`, `user32`, `gdi32`, `winmm`, `dxguid`; then `sfml-graphics`, `sfml-window`, `sfml-system`. Add `-std=c++17`, as on macOS. Do not add `-DSFML_STATIC`, `sfml-main`, or `-s`. Do not set the working directory to the SDK `bin`. `cb.bmp` is the project-directory relative file already measured on macOS.

After link, copy `sfml-graphics-3.dll`, `sfml-window-3.dll`, and `sfml-system-3.dll` from the unpacked `bin` to the target output directory. The copy command stores that `bin` path. It does not use `$(#sfml)`. The sample does not link audio, so do not copy `openal32.dll` unless a run then reports it missing. SFML’s own FAQ says a procedure-entry-point error means a different SFML DLL was found first on `PATH`; the copy beside the executable is what prevents that.

| Mac milestone | Windows difference |
|---------------|--------------------|
| M1 Plan | This section. |
| M2 Probe | `WinSdkCompiler`. No network. |
| M3 Formula query | No formula. “Installed” means the saved directory passes the suitability check. |
| M4 Client check | Import libraries and major 3, not dylibs. Version list, link-type page, and location page are not registered when the compiler matches. |
| M5 Diagnostic page | Two texts. Compiler matches and SFML is absent: say SFML is not installed. Compiler does not match: say the automatic install does not apply, and register the existing location page. Do not say “Homebrew”. |
| M6 Install | Button **Install SFML**. Explanation: the project needs SFML, nothing is downloaded until that button, Cancel does not start a download. URL is the 3.1.0 GCC 14.2.0 zip for `dumpmachine` (`x86_64-w64-mingw32` → 64-bit SEH zip, `i686-w64-mingw32` → 32-bit DW2 zip). Fetch only when version is 14.2.0 and `gcc -v` says `ucrt`. |
| M7 Sample | Same `sfml3/main.cpp` and `cb.bmp`. The installed headers are 3.1.0. `GetFilesDir` is `sfml/sfml3`. |
| M8 Rpath | No rpath. The three DLLs sit beside the executable. Run from the project directory with no extra `PATH` entry. |
| M9 Debugger | Ordinary Windows debugger on that binary. Break in `main.cpp`, continue, stop. No Darwin thread or `.eh_frame` work. No README. |

**Check.** Compiler 14.2.0 UCRT 64-bit, empty SDK directory: no download before **Install SFML**. Cancel on the dialog creates nothing. After a successful unpack the same check accepts the tree without restarting the IDE. The `.cbp` has the unpacked include and lib dirs, `-std=c++17`, and no `$(#sfml)`. The process starts. A second wizard run has no dependency page and no location page.

**Stop.** Do not download the Visual C++ zip to make a mismatched compiler pass. Do not add a static page to avoid the DLL copy.

---

## 5. GLFW — v3.1’s milestones on Windows

v3.1 left Windows on `glfw/files/main.cpp` (fixed-function `glBegin`) because `opengl32` exports OpenGL 1.1 and the macOS core entry points do not link. That reason still holds. The example still moves forward: the Windows sample is the same 4.1 core triangle, and the entry points come from GLFW.

**PROPOSED DESIGN.** New `glfw/win/main.cpp`. `GetFilesDir` returns `glfw/win` on Windows and keeps `glfw/mac` on macOS and `glfw/files` on other Unix. The Windows file requests the same context the macOS file requests:

```text
glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 4);
glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 1);
glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GLFW_TRUE);
```

Shaders stay `#version 410 core`. One triangle, no matrix, no texture. 4.1 is the macOS sample’s version. A 4.6 context is not a reasonable bump: the shaders would diverge, and a machine whose driver stops at 4.1 would fail. Do not add GLAD, GLEW, or GLM.

**CONFIRMED FROM SOURCE** of GLFW’s current build guide (3.5.1): the header does not ship `GL/glcorearb.h`. `GLFW_INCLUDE_GLCOREARB` would include that header and then expect the core functions to link. On Windows they do not. The Windows sample defines `GLFW_INCLUDE_NONE` and `GLFW_DLL`, includes `<GLFW/glfw3.h>`, and after `glfwMakeContextCurrent` loads the calls it makes with `glfwGetProcAddress` (`glCreateShader`, `glShaderSource`, `glCompileShader`, `glGetShaderiv`, `glGetShaderInfoLog`, `glDeleteShader`, `glCreateProgram`, `glAttachShader`, `glLinkProgram`, `glUseProgram`, `glGenBuffers`, `glBindBuffer`, `glBufferData`, `glGenVertexArrays`, `glBindVertexArray`, `glVertexAttribPointer`, `glEnableVertexAttribArray`, `glDrawArrays`, `glClear`, `glViewport`, `glClearColor`, `glDeleteProgram`). Constants those calls need are numeric literals or an enum in that file. A missing pointer aborts before the draw. Do not include `<GL/gl.h>` for those names: it is the 1.1 header.

Suitability: `include/GLFW/glfw3.h` and `lib-mingw-w64/libglfw3dll.a` and `lib-mingw-w64/glfw3.dll`. The include directory written to the project is the archive’s `include`. The library directory is `lib-mingw-w64`. The user never chooses that folder. `lib-vc2022` (or any `lib-vc*`) is not suitable. `libglfw3.a` alone is not suitable: the 3.4 binaries were built with an older MSVCRT MinGW, and this compiler is UCRT. The DLL is the boundary that does not require the static archive’s CRT. Read the 3.5.1 archive README once at implementation and record the MinGW version it names. Do not switch to the static archive unless that README says the static library is UCRT.

Link, **CONFIRMED FROM SOURCE** of the same build guide: `-DGLFW_DLL`, `-lglfw3dll`, `-lopengl32`, `-lgdi32`. `AddLinkLib` of `glfw3dll` produces `-lglfw3dll`. Do not also link `glfw3`: that is the current Windows line and it selects `libglfw3.a`. Do not link `glu32`. The triangle does not call GLU. Do not set the working directory to the SDK directory. Copy `glfw3.dll` beside the executable; the build guide says that is how the process finds it.

Any 64-bit or 32-bit GCC whose `WinSdkCompiler` access is `works` may fetch the matching `WIN64` or `WIN32` zip. GLFW is not held to the SFML 14.2.0 UCRT test. A compiler that fails `dumpmachine` (`x86_64-w64-mingw32` or `i686-w64-mingw32`) does not download; the existing location page remains.

| Mac milestone | Windows difference |
|---------------|--------------------|
| M1 Plan | This section. |
| M2 Client | No location page when the tree validates. Sample is `glfw/win/main.cpp`, not `glfw/files/main.cpp`. Button **Install GLFW**. |
| M3 Link and run | No `otool`. The import library is `libglfw3dll.a`. The running process loads `glfw3.dll` from its own directory. The context reports 4.1 core. The triangle is visible. Escape or the close control ends it. |
| M4 Debugger | Ordinary Windows debugger. Break in `main.cpp`, continue, stop. No new GDB work. |

**Check.** First run downloads nothing until **Install GLFW**. The `.cbp` contains the unpacked include dir, `lib-mingw-w64`, `-DGLFW_DLL`, `-lglfw3dll`, `-lopengl32`, `-lgdi32`. It does not contain `$(#glfw)`, `glu32`, or `lib-vc2022`. The historical `glBegin` sample is not the file that was copied.

**Stop.** Do not add a loader library. Do not keep the fixed-function sample in order to avoid `glfwGetProcAddress`.

---

## 6. SDL3 — v3.2’s milestones on Windows

The sample stays `sdl3/files/main.cpp`. It is already the 3.4.18 program: `<SDL3/SDL.h>`, `<SDL3/SDL_main.h>`, `SDL_Init` as `bool`, `SDL_CreateWindowAndRenderer("SDL3", 640, 480, 0, ...)`, `SDL_FRect`, poll until quit or Escape. One sample for every platform. There is no newer SDL3 release than 3.4.18 on the day of this plan, so the library version does not move.

**CONFIRMED FROM WEB** (SDL’s MinGW introduction): the development zip extracts to two trees, `i686-w64-mingw32` and `x86_64-w64-mingw32`. Each has `include`, `lib`, and `bin`. The documented compile line is `-I <arch>/include -L <arch>/lib -lSDL3`, then copy `<arch>/bin/SDL3.dll` beside the executable. `dumpmachine` selects the tree. The current wizard text tells the user to pick that folder, and the post-build line is `XCOPY $(#sdl3)\bin\*.dll`. Both go away on the automatic path. The include directory is `<arch>/include` so that `<SDL3/SDL.h>` resolves. The library directory is `<arch>/lib`. The copy source is `<arch>/bin/SDL3.dll`, written as that path, not as `$(#sdl3)`.

Suitability: `<arch>/include/SDL3/SDL.h` and `<arch>/lib/libSDL3.dll.a` and `<arch>/bin/SDL3.dll`. The VC development zip is not this file and is not suitable.

Link `SDL3` only. The current Windows branch already does. Do not put back `mingw32`, `SDL2main`, `SDL2.dll`, `user32`, `gdi32`, `winmm`, or `dxguid`. `SDL_main.h` is already included; on Windows that header is what supplies the entry point, and the SDL3 migration notes say the SDLmain library is gone. Do not add `-mwindows` unless the link of this sample reports that it is required. The debug target stays `ttConsoleOnly`.

Any matching `dumpmachine` may fetch the one mingw zip. SDL is not held to the SFML 14.2.0 UCRT test. A compiler outside those two machine names does not download; the existing location page remains, and its checks stay `SDL3/SDL.h` and the `SDL3` library stem.

| Mac milestone | Windows difference |
|---------------|--------------------|
| M1 Plan | This section. The SDL2 wizard is already gone in this tree. |
| M2 Client | No location page when `<arch>` validates. Button **Install SDL3**. Same `sdl3/files/main.cpp`. |
| M3 Window | No `otool`. Link is `-lSDL3` plus the copied `SDL3.dll`. The window opens, the bars draw, close or Escape exits. Record the renderer name. Do not force a driver. |
| M4 Debugger | Ordinary Windows debugger. Break in `main.cpp`, continue, stop. |
| M5 Retire SDL 1 | On Windows too. `config.script` registers **SDL project** only when `PLATFORM` is neither `PLATFORM_MAC` nor `PLATFORM_MSW`, so Unix (`PLATFORM_GTK`, `PLATFORM_X11`) still shows it. `src/update.bat` excludes `sdl\` from the wizard `xcopy`, and an existing `templates/wizard/sdl` under the Windows devel and output trees is removed. `resources/Makefile.am` still installs `sdl` for the non-Darwin build. Do not edit `resources/sdl/`. |

**Check.** An installed SDL2 tree, or an `sdl2-compat` DLL, does not satisfy the check. The `.cbp` has no `$(#sdl3)`, no `SDL2`, and no pkg-config. The post-build copy names the unpacked `<arch>\bin`.

**Stop.** Do not point the copy at the zip root’s `bin`. That directory is not where this archive puts `SDL3.dll`.

---

## 7. Files

| File | v3.3 |
|------|------|
| `winsdk.cpp`, `winsdk.h`, `scriptedwizard/Makefile.am`, `wiz.cpp` | **New helper**, Windows bindings only. |
| `sfml/wizard.script` | **Windows branch.** Compiler probe, diagnostic page, install button, skip version / link-type / location pages when the 3.1.0 tree validates, absolute include and lib, DLL copy. macOS branch unchanged. |
| `glfw/wizard.script`, `glfw/Makefile.am` | **Windows branch** plus install of `glfw/win/main.cpp`. macOS branch unchanged. `glfw/files/main.cpp` unchanged for other Unix. |
| `glfw/win/main.cpp` | **New.** Section 5. |
| `sdl3/wizard.script` | **Windows branch.** Automatic `<arch>` prefix and DLL copy. macOS branch unchanged. Sample unchanged. |
| `config.script` | **SDL project** is registered for Unix only. macOS and Windows do not register it. |
| `src/update.bat` | **Windows install.** Exclude the `sdl` wizard from the template copy. |
| `resources/sdl/`, `resources/Makefile.am` | **Sources unchanged.** Unix still installs that wizard. The Darwin branch already skips it. |
| `common_functions.script`, `macbrew.*`, other wizards, `src/templates/common/sfml*`, `src/templates/common/glfw*` | **Unchanged.** |

The running IDE loads wizard scripts from its data directory. Copying the changed scripts and the new sample into that tree is how a milestone is tried. A rebuild is required for `winsdk.cpp`.

---

## 8. Acceptance

1. **Each library absent, compiler matches.** The wizard does not open the Global Variable Editor and does not ask for a folder. Nothing is downloaded before **Install SFML**, **Install GLFW**, or **Install SDL3**. Cancel on that dialog downloads nothing and creates no project. After success, the project is created without restarting the IDE.
2. **Second run.** No dependency page and no location page.
3. **SFML compiler is not 14.2.0 UCRT of the matching machine.** No SFML zip is fetched. The existing SFML location page still works. GLFW and SDL3 are judged on their own rules, not on this SFML rule.
4. **Non-zero download or a failed unzip.** No success text and no project.
5. **Cancel after the transfer starts.** That transfer stops. No rollback claim, no project. The next attempt looks at the machine again.
6. **Generated projects.** Absolute include and lib directories. SFML: `-std=c++17`, dynamic `sfml-graphics` / `sfml-window` / `sfml-system`, the existing Win32 libs, the three `sfml-*-3.dll` beside the executable, sample `sfml/sfml3`. GLFW: `-DGLFW_DLL`, `-lglfw3dll`, `-lopengl32`, `-lgdi32`, `glfw3.dll` beside the executable, sample `glfw/win`. SDL3: `-lSDL3`, `SDL3.dll` beside the executable, sample `sdl3/files`. No `$(#sfml)`, `$(#glfw)`, or `$(#sdl3)`.
7. **Run.** Each program starts from the IDE with no added `PATH` entry. SFML opens its window. GLFW shows the triangle in a 4.1 core context. SDL3 draws the bars and stays open until close or Escape.
8. **Debugger.** One ordinary session per generated project: break in `main`, continue, stop.
9. **Left in place.** macOS behaviour of the three wizards. Other Unix, including **SDL project**. The official tree `D:\Program Files\CodeBlocks` is not modified. Windows does not list **SDL project** and does not install `templates/wizard/sdl`.

---

## 9. Rejected

- A Windows v3.0, then a Windows v3.1, then a Windows v3.2.
- Showing the zip name, the URL, “UCRT”, “SEH”, “DW2”, or `lib-mingw-w64` / `x86_64-w64-mingw32` on the wizard page.
- Winget, Chocolatey, Scoop, MSYS2, vcpkg, or a Visual C++ zip.
- Unpacking into `D:\Program Files\CodeBlocks`, or shipping the libraries inside the IDE install.
- The 1.6 / 2.0 list, the static/dynamic page, or a hand-entered prefix, when the automatic tree validates.
- Linking SFML or GLFW statically to avoid copying a DLL.
- GLAD, GLEW, GLM, or the fixed-function GLFW sample, on the automatic Windows path.
- A second SDL sample, or an SDL_GPU sample.
- Deleting `resources/sdl/`, or hiding **SDL project** on Unix. Windows and macOS do not register or install it. Unix does.
- Editing `macbrew.cpp` so that it downloads zip files.
- `brew`, `pkg-config`, or a global-variable write.
- A debugger campaign. The three debugger milestones are smoke tests.
