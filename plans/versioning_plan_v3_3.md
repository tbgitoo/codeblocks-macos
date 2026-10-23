# Code::Blocks Windows v3.3 — SFML, GLFW, and SDL3

**Status: planning.** No code in this version yet. v3.0, v3.1, and v3.2 stay the macOS plans. This file is their Windows implementation, one version for all three. This update records the install transaction, the hash check, versioned trees, and revalidation. It does not add code.

**Question this version answers:** what is the smallest change that gives the existing SFML, GLFW, and SDL3 wizards the same automatic setup on Windows that Homebrew gives them on macOS?

The user creates a project. A missing library is installed only after an explicit button. The button names the library. It does not name a zip, a compiler vendor, an exception model, or a folder the user must browse to. A later run revalidates the saved tree and adds no dependency page only when that check still passes. The generated project is a normal Code::Blocks project with absolute include and library directories. Build does not depend on `PATH`, on `$(#sfml)` / `$(#glfw)` / `$(#sdl3)`, or on the Global Variable Editor.

Upstream baseline remains Code::Blocks trunk **r14050** / IDE **25.03**. The compiler this plan is written against is the MinGW in the official 25.03 installer: WinLibs GCC 14.2.0, UCRT, 64-bit SEH, master path `D:\Program Files\CodeBlocks\MinGW` on the machine used to write this plan. The wizard reads the compiler the IDE will use to build the project. It does not read the first `gcc` on `PATH`. An MSYS2 GCC earlier on `PATH` must not choose the archive.

Evidence labels: **CONFIRMED FROM SOURCE** (this tree or a published page), **CONFIRMED FROM WEB** (23 October 2026), **PROPOSED DESIGN**.

---

## 1. What is the same, and what is not

The page order, the consent rule, the cancel rule, and the “already installed” path are the macOS ones. The thing being installed is not Homebrew.

| macOS | Windows |
|-------|---------|
| `Wizard.MacBrewFormula` / `MacBrewInstall` | A Windows-only helper. It is not a stub of `macbrew.cpp`, and `macbrew.cpp` grows no Windows code. |
| Formula name `sfml`, `glfw`, `sdl3` | One HTTPS zip per library, chosen from the compiler identity. The script holds the URL. The helper does not know header names. |
| Prefix `/opt/homebrew/opt/<formula>` | A versioned directory the helper published under the user profile, not under `D:\Program Files\CodeBlocks`. |
| No location page when the prefix validates | The version list, the static/dynamic page, and the location page are not registered on that path. |
| Homebrew missing: locate `brew`, or open https://brew.sh. No bootstrap. | Compiler does not match the archive: no download. The existing location page is the way to point at a tree the user already has. There is no installer to locate and no package manager to bootstrap. |
| Dynamic library, absolute install name, no rpath | Dynamic library. Copy the needed DLLs next to the executable. There is no rpath step. |

Other Unix is unchanged: SFML’s commented `pkg-config`, GLFW’s `GL` / `GLU` / `pthread` / `Xxf86vm` and `glfw/files/main.cpp`, SDL3’s `pkg-config` lines. **SDL project** (SDL 1) is removed on Windows as well. It stays registered and installed on Unix. v3.2 M5 already retired it on macOS. The sources under `resources/sdl/` stay in the tree for that Unix build.

---

## 2. Helper

New `src/plugins/scriptedwizard/winsdk.cpp` and `winsdk.h`, listed in `scriptedwizard/Makefile.am`, bound in `wiz.cpp` only inside `#if defined(__WXMSW__)`. Same place `MacBrew*` is bound inside `#if defined(__WXMAC__)`. Linux gets neither.

The helper does four things.

1. **Compiler identity.** From the compiler id the wizard is using (`Wizard.GetCompilerID()`, otherwise the default compiler), read that compiler’s master path and run `<master>\bin\gcc.exe`, not a `gcc` found on `PATH`. The probe is `gcc -dumpfullversion -dumpversion`, `gcc -dumpmachine`, and `gcc -v`. Keep the raw output. Report `works` only when that executable runs. The identity the script receives is: `works`, the compiler executable path, the Code::Blocks compiler id, the full GCC version, the target triple, the CRT family when the probe establishes one, the architecture, and the raw output. **PROPOSED DESIGN.** Do not treat one substring of `gcc -v` as the compatibility test before that compiler has been read. At implementation, record the output of the GCC shipped in the official Code::Blocks 25.03 Windows installation and keep the smallest reading that still enforces section 4. Also confirm that this compiler is the one the generated project will use at the moment the archive is chosen. If the wizard can still change compiler after the probe, repeat the probe then. This probe is the Windows M2: no download.
2. **Saved tree.** A library id token (`sfml`, `glfw`, `sdl3`, same character rule as a Homebrew formula token) maps to the published tree selected last time. That path is a candidate. On every wizard run the helper returns it and the script runs that library’s suitability checks again. Pages are skipped only when those checks pass. A missing, moved, edited, or quarantined tree is the absent path, including the existing location page where this plan already keeps one. A saved record by itself is never “installed”. The helper does not contain SFML, GLFW, or SDL3 header names, library names, or version rules. It may find an extraction root only by marker paths the script passes in. The script owns suitability.
3. **Fetch.** Arguments are the URL, the expected SHA-256, the library id, the explanation, and the button label. Each wizard owns its URL and its hash. The dialog is shown before any transfer. Cancel on that dialog does not connect. The transfer never unpacks into the published tree. Staging is under the same user SDK area. The order is: download into staging; verify the completed file against the expected SHA-256; unpack into a fresh staging directory; find the candidate root with the script’s markers; the script validates that root; only then publish into the versioned final directory; only then write the saved-tree record. A hash mismatch, a failed unpack, a rejected archive entry, or a failed validation publishes nothing and does not update the saved record. Cancel after the transfer starts stops that transfer only and does not report success. It must not replace the previously published tree. Staging left behind is disposable. Do not promise to delete every staging file. After publication, store a small provenance record with that tree: library id, library version, source URL, the SHA-256 that was expected and verified, the compiler identity used to choose the archive when the archive depends on it, and the resolved SDK root. It is for diagnosis. It is not a wizard page. The IDE environment is not changed. No percentage unless the response has a length; do not invent one.

   **PROPOSED DESIGN**, the hashes. They are not in this plan. At implementation, take the SHA-256 of each exact archive in section 3 from that archive’s authoritative release, record it beside the URL, and pass it in. Do not invent one here.

   **PROPOSED DESIGN**, zip safety. An entry is rejected if it is absolute, drive-qualified, contains `..`, or otherwise leaves the staging directory. If the extraction API can emit a link or reparse point that leaves that directory, reject that entry too. Rejection is a failed fetch: nothing is published.
4. **Nothing else.** No search, no upgrade, no uninstall, no `winget`, no `pacman`, no `vcpkg`, no compile-from-source.

The published tree is versioned under `%LOCALAPPDATA%\codeblocks\sdk\`. It is not a single `sdk\sfml`, `sdk\glfw`, or `sdk\sdl3` that a later install would overwrite while an existing `.cbp` still points at it. The directory names are an implementation detail. The invariant is a distinct tree per library version and per archive identity, for example `sdk\sfml\3.1.0\<compiler-identity>`, `sdk\glfw\3.5.1\<architecture>`, and `sdk\sdl3\3.4.18\<architecture>`. The saved-tree record names the tree that last passed validation. Official Code::Blocks in `D:\Program Files\CodeBlocks` is not a destination. Zip archives wrap the marker in a folder whose name the script does not have to know; the helper finds that root from the markers.

**PROPOSED DESIGN**, the post-build copy used by SFML, GLFW, and SDL3. The generated command quotes the source path and the destination path, uses that target’s own Debug or Release output directory, does not depend on the shell’s current directory, and fails the build visibly when a required DLL is missing. A profile path that contains spaces must still copy. Do not add a `PATH` entry or a global variable to avoid quoting.

---

## 3. Versions

**CONFIRMED FROM WEB** (23 October 2026). These are the archives. The wizard page does not show the filenames.

| Wizard | macOS, already shipped | Windows archive | Why this far and no further |
|--------|------------------------|-----------------|-----------------------------|
| SFML | Homebrew formula at implementation was **3.0.2** | **3.1.0**, `SFML-3.1.0-windows-gcc-14.2.0-mingw-64-bit.zip` (32-bit: the `-mingw-32-bit.zip`). <https://www.sfml-dev.org/files/SFML-3.1.0-windows-gcc-14.2.0-mingw-64-bit.zip> | Newest SFML release. SFML’s download page says this zip matches WinLibs UCRT GCC 14.2.0 64-bit SEH, and no other GCC. The Visual C++ zip is the wrong file. |
| GLFW | Homebrew **3.5.1** | **3.5.1**, `glfw-3.5.1.bin.WIN64.zip` (32-bit: `glfw-3.5.1.bin.WIN32.zip`). <https://github.com/glfw/glfw/releases/download/3.5.1/glfw-3.5.1.bin.WIN64.zip> | Current GLFW release (31 July 2026). The official Windows binaries include MinGW-w64. GLFW is C, so the SFML “compiler must match 100%” rule does not apply. |
| SDL3 | Homebrew **3.4.18** | **3.4.18**, `SDL3-devel-3.4.18-mingw.zip` (both architectures inside). <https://libsdl.org/release/SDL3-devel-3.4.18-mingw.zip> | Current SDL3 release (2 October 2026), same version the macOS sample was written for. There is no newer release to bump to. |

Rejected for all three: winget, Chocolatey, Scoop, MSYS2, vcpkg’s MSVC triplet, and the Visual C++ archive of the same library. Also rejected: building the library with CMake inside the wizard.

SHA-256 values for these three files are not recorded here. **PROPOSED DESIGN.** Implementation reads each hash from the authoritative release of that exact archive.

---

## 4. SFML — v3.0’s milestones on Windows

The sample stays `sfml/sfml3/main.cpp`. It was written for 3.0.2 (`sf::VideoMode({800, 600})`, `sf::Sprite(texture)`, `pollEvent` optional, `event->is<sf::Event::Closed>()`). 3.1.0 is a minor release of that API. Do not add a 3.1-only sample. If a 3.1.0 header rejects one of those calls, fix that call so 3.0.2 still accepts it, or stop and record the line. Do not fork the sample.

Suitability, dynamic only: `include/SFML/Graphics.hpp`, `include/SFML/Config.hpp`, `SFML_VERSION_MAJOR` 3, and import libraries `libsfml-graphics.dll.a`, `libsfml-window.dll.a`, `libsfml-system.dll.a`. A static `libsfml-graphics.a` with no import library is not suitable. Major 1 or 2 is not suitable on this path: the download is 3.1.0, and the 1.6 / 2.0 list is not shown. An unreadable version stops.

**CONFIRMED FROM WEB** (SFML FAQ): a missing dynamic module is reported as `sfml-graphics-3.dll` (and window, system). Debug builds of the official zip are not what that page’s release archives are. At the first unpack, list `lib` and `bin`. If no `sfml-*-d` import library exists, leave `SFMLDebugLib` unset. Do not invent `-d` or `-s`.

Link line, both targets, from the existing Windows branch, dynamic arm only: include and lib dirs of the unpacked tree; `mingw32`, `user32`, `gdi32`, `winmm`, `dxguid`; then `sfml-graphics`, `sfml-window`, `sfml-system`. Add `-std=c++17`, as on macOS. Do not add `-DSFML_STATIC`, `sfml-main`, or `-s`. Do not set the working directory to the SDK `bin`. `cb.bmp` is the project-directory relative file already measured on macOS.

After link, copy the runtime DLLs this sample needs from the accepted archive’s `bin` to the target output directory. The expected direct modules are `sfml-graphics-3.dll`, `sfml-window-3.dll`, and `sfml-system-3.dll`. **PROPOSED DESIGN** until implementation opens the 3.1.0 archive and runs the generated executable with no SFML directory on `PATH`. Record the set that run actually requires, and copy that set. Do not copy every DLL in `bin`. Do not copy an audio module or `openal32.dll` unless that run shows the sample needs it. The copy command stores those paths, follows section 2, and does not use `$(#sfml)`. SFML’s own FAQ says a procedure-entry-point error means a different SFML DLL was found first on `PATH`; the copy beside the executable is what prevents that. The executable must start with those DLLs beside it and with no SFML installation directory added to `PATH`.

| Mac milestone | Windows difference |
|---------------|--------------------|
| M1 Plan | This section. |
| M2 Probe | `WinSdkCompiler`. No network. |
| M3 Formula query | No formula. “Installed” means the saved directory passes the suitability check. |
| M4 Client check | Import libraries and major 3, not dylibs. Version list, link-type page, and location page are not registered when the compiler matches. |
| M5 Diagnostic page | Two texts. Compiler matches and SFML is absent: say SFML is not installed. Compiler does not match: say the automatic install does not apply, and register the existing location page. Do not say “Homebrew”. |
| M6 Install | Button **Install SFML**. Explanation: the project needs SFML, nothing is downloaded until that button, Cancel does not start a download. URL is the 3.1.0 GCC 14.2.0 zip for the probed machine (`x86_64-w64-mingw32` → 64-bit SEH zip, `i686-w64-mingw32` → 32-bit DW2 zip), plus the SHA-256 recorded for that file at implementation. Fetch only when the identity is the compiler this archive is built for: GCC 14.2.0, UCRT, and that machine. **PROPOSED DESIGN** for how those three facts are read from the probe. The requirement is not relaxed. |
| M7 Sample | Same `sfml3/main.cpp` and `cb.bmp`. The installed headers are 3.1.0. `GetFilesDir` is `sfml/sfml3`. |
| M8 Rpath | No rpath. The runtime DLL set recorded for this sample sits beside the executable. Run from the project directory with no extra `PATH` entry and with no SFML directory on `PATH`. |
| M9 Debugger | Ordinary Windows debugger on that binary. Break in `main.cpp`, continue, stop. No Darwin thread or `.eh_frame` work. No README. |

**Check.** Compiler 14.2.0 UCRT 64-bit, empty SDK directory: no download before **Install SFML**. Cancel on the dialog creates nothing. After a successful publication the same check accepts the new tree without restarting the IDE. A hash mismatch or a failed validation does not replace a tree that was already published. The `.cbp` has that tree’s include and lib dirs, `-std=c++17`, and no `$(#sfml)`. The process starts with the recorded DLLs beside it. A second wizard run has no dependency page and no location page only while the saved tree still passes the same checks.

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

**CONFIRMED FROM SOURCE** of GLFW’s current build guide (3.5.1): the header does not ship `GL/glcorearb.h`. `GLFW_INCLUDE_GLCOREARB` would include that header and then expect the core functions to link. On Windows they do not. The Windows sample defines `GLFW_INCLUDE_NONE` and `GLFW_DLL`, includes `<GLFW/glfw3.h>`, and after `glfwMakeContextCurrent` loads the calls it makes with `glfwGetProcAddress` (`glCreateShader`, `glShaderSource`, `glCompileShader`, `glGetShaderiv`, `glGetShaderInfoLog`, `glDeleteShader`, `glCreateProgram`, `glAttachShader`, `glLinkProgram`, `glUseProgram`, `glGenBuffers`, `glBindBuffer`, `glBufferData`, `glGenVertexArrays`, `glBindVertexArray`, `glVertexAttribPointer`, `glEnableVertexAttribArray`, `glDrawArrays`, `glClear`, `glViewport`, `glClearColor`, `glDeleteProgram`). Do not include `<GL/gl.h>` for those names: it is the 1.1 header.

**PROPOSED DESIGN**, the loader in that file. One routine resolves those calls. Each pointer has a typed alias whose name cannot collide with an OpenGL declaration (`p_glCreateShader`, and the same pattern for the rest). Every pointer is checked before it is used. A null pointer names the missing symbol and returns before any draw. Constants those calls need live together in one named enum or `constexpr` block in that file, not as scattered literals in the draw. Implementation runs this file on Windows and accounts for the split between the 1.1 exports and entry points that exist only after the context is current. Do not drop the core profile to avoid that split.

Suitability: `include/GLFW/glfw3.h` and `lib-mingw-w64/libglfw3dll.a` and `lib-mingw-w64/glfw3.dll`. The include directory written to the project is the archive’s `include`. The library directory is `lib-mingw-w64`. The user never chooses that folder. `lib-vc2022` (or any `lib-vc*`) is not suitable. `libglfw3.a` alone is not suitable: the 3.4 binaries were built with an older MSVCRT MinGW, and this compiler is UCRT. The DLL is the boundary that does not require the static archive’s CRT. Read the 3.5.1 archive README once at implementation and record the MinGW version it names. Do not switch to the static archive unless that README says the static library is UCRT.

Link, **CONFIRMED FROM SOURCE** of the same build guide: `-DGLFW_DLL`, `-lglfw3dll`, `-lopengl32`, `-lgdi32`. `AddLinkLib` of `glfw3dll` produces `-lglfw3dll`. Do not also link `glfw3`: that is the current Windows line and it selects `libglfw3.a`. Do not link `glu32`. The triangle does not call GLU. Do not set the working directory to the SDK directory. Copy `glfw3.dll` beside the executable; the build guide says that is how the process finds it. The copy follows section 2.

Any 64-bit or 32-bit GCC whose `WinSdkCompiler` access is `works` may fetch the matching `WIN64` or `WIN32` zip. GLFW is not held to the SFML 14.2.0 UCRT test. A compiler that fails `dumpmachine` (`x86_64-w64-mingw32` or `i686-w64-mingw32`) does not download; the existing location page remains.

| Mac milestone | Windows difference |
|---------------|--------------------|
| M1 Plan | This section. |
| M2 Client | No location page when the tree validates. Sample is `glfw/win/main.cpp`, not `glfw/files/main.cpp`. Button **Install GLFW**. The script passes the 3.5.1 URL for the probed architecture and the SHA-256 recorded for that file at implementation. |
| M3 Link and run | No `otool`. The import library is `libglfw3dll.a`. The running process loads `glfw3.dll` from its own directory. Every requested entry point resolved before the draw. The context reports 4.1 core. The triangle is visible. Escape or the close control ends it. A missing entry point exits before drawing and names that symbol. |
| M4 Debugger | Ordinary Windows debugger. Break in `main.cpp`, continue, stop. No new GDB work. |

**Check.** First run downloads nothing until **Install GLFW**. The `.cbp` contains the unpacked include dir, `lib-mingw-w64`, `-DGLFW_DLL`, `-lglfw3dll`, `-lopengl32`, `-lgdi32`. It does not contain `$(#glfw)`, `glu32`, or `lib-vc2022`. The historical `glBegin` sample is not the file that was copied.

**Stop.** Do not add a loader library. Do not keep the fixed-function sample in order to avoid `glfwGetProcAddress`.

---

## 6. SDL3 — v3.2’s milestones on Windows

The sample stays `sdl3/files/main.cpp`. It is already the 3.4.18 program: `<SDL3/SDL.h>`, `<SDL3/SDL_main.h>`, `SDL_Init` as `bool`, `SDL_CreateWindowAndRenderer("SDL3", 640, 480, 0, ...)`, `SDL_FRect`, poll until quit or Escape. One sample for every platform. There is no newer SDL3 release than 3.4.18 on the day of this plan, so the library version does not move.

**CONFIRMED FROM WEB** (SDL’s MinGW introduction): the development zip extracts to two trees, `i686-w64-mingw32` and `x86_64-w64-mingw32`. Each has `include`, `lib`, and `bin`. The documented compile line is `-I <arch>/include -L <arch>/lib -lSDL3`, then copy `<arch>/bin/SDL3.dll` beside the executable. `dumpmachine` selects the tree. The current wizard text tells the user to pick that folder, and the post-build line is `XCOPY $(#sdl3)\bin\*.dll`. Both go away on the automatic path. The include directory is `<arch>/include` so that `<SDL3/SDL.h>` resolves. The library directory is `<arch>/lib`. The copy source is `<arch>/bin/SDL3.dll`, written as that path, not as `$(#sdl3)`. The copy follows section 2.

Suitability: `<arch>/include/SDL3/SDL.h` and `<arch>/lib/libSDL3.dll.a` and `<arch>/bin/SDL3.dll`. The VC development zip is not this file and is not suitable.

Link `SDL3` only. The current Windows branch already does. Do not put back `mingw32`, `SDL2main`, `SDL2.dll`, `user32`, `gdi32`, `winmm`, or `dxguid`. `SDL_main.h` is already included; on Windows that header is what supplies the entry point, and the SDL3 migration notes say the SDLmain library is gone. Do not add `-mwindows` unless the link of this sample reports that it is required. The debug target stays `ttConsoleOnly`.

Any matching `dumpmachine` may fetch the one mingw zip. SDL is not held to the SFML 14.2.0 UCRT test. A compiler outside those two machine names does not download; the existing location page remains, and its checks stay `SDL3/SDL.h` and the `SDL3` library stem.

| Mac milestone | Windows difference |
|---------------|--------------------|
| M1 Plan | This section. The SDL2 wizard is already gone in this tree. |
| M2 Client | No location page when `<arch>` validates. Button **Install SDL3**. The script passes the 3.4.18 MinGW URL and the SHA-256 recorded for that file at implementation. Same `sdl3/files/main.cpp`. |
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
2. **Second run.** The saved path is only a candidate. The same suitability checks run again. No dependency page and no location page only when they pass. A missing, moved, edited, or quarantined tree is not treated as installed.
3. **SFML compiler is not 14.2.0 UCRT of the matching machine.** No SFML zip is fetched. The existing SFML location page still works. GLFW and SDL3 are judged on their own rules, not on this SFML rule.
4. **Non-zero download, hash mismatch, unsafe zip entry, failed unzip, or failed validation.** Nothing is published. The saved-tree record is unchanged. No success text and no project. Staging is not a tree a later run can accept.
5. **Cancel after the transfer starts.** That transfer stops. Any previously published tree stays the saved tree. Staging may remain. No rollback claim, no project. The next attempt looks at the machine again.
6. **Generated projects.** Absolute include and lib directories of the published versioned tree, not of a shared `sdk\sfml`, `sdk\glfw`, or `sdk\sdl3` directory that a later install would overwrite. SFML: `-std=c++17`, dynamic `sfml-graphics` / `sfml-window` / `sfml-system`, the existing Win32 libs, the recorded runtime DLL set beside the executable, sample `sfml/sfml3`. GLFW: `-DGLFW_DLL`, `-lglfw3dll`, `-lopengl32`, `-lgdi32`, `glfw3.dll` beside the executable, sample `glfw/win`. SDL3: `-lSDL3`, `SDL3.dll` beside the executable, sample `sdl3/files`. Each copy command quotes its source and its destination, names that target’s own output directory, and does not depend on the current directory. A profile path containing spaces still copies, and a missing required DLL fails the build visibly. No `$(#sfml)`, `$(#glfw)`, or `$(#sdl3)`. No `PATH` change and no new global variable.
7. **Run.** Each program starts from the IDE with no added `PATH` entry and with no SDK directory added to `PATH`. SFML opens its window with the recorded DLLs beside the executable. GLFW resolves every requested entry point before drawing, then shows the triangle in a 4.1 core context. A missing entry point exits before drawing and names that symbol. SDL3 draws the bars and stays open until close or Escape.
8. **Debugger.** One ordinary session per generated project: break in `main`, continue, stop.
9. **Left in place.** macOS behaviour of the three wizards. Other Unix, including **SDL project**. The official tree `D:\Program Files\CodeBlocks` is not modified. Windows does not list **SDL project** and does not install `templates/wizard/sdl`.
10. **Publication.** The completed archive matches the SHA-256 the wizard supplied. The published directory is a new versioned tree. The saved record changes only after that publication. The provenance record stored with the tree has the library id, version, URL, verified SHA-256, the compiler identity where the archive depended on it, and the installed root. It is not a wizard page.

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
