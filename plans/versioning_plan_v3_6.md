# Code::Blocks v3.6 — modernise the FLTK application wizard on macOS

**Status: DONE.** The Scripted Wizard FLTK application entry now creates a working FLTK project on the tested Apple Silicon macOS configuration. The generated project builds and runs successfully against the Homebrew-provided FLTK installation.

**Question this version answers:** what is the smallest change required to turn the existing FLTK wizard into a working out-of-the-box starting point on a modern macOS installation?

The answer implemented in v3.6 is deliberately small: correct the existing FLTK wizard in a single file so that it can use the Homebrew FLTK installation on macOS. No new build system, compiler profile, or dependency framework was introduced.

---

## 1. Scope

The FLTK wizard remains an ordinary library-dependent C++ project wizard:

```text
Code::Blocks
    |
    +-- ordinary C++ compiler profile
    +-- generated FLTK project
    +-- external Homebrew FLTK installation
    |
    +-- native FLTK application
```

FLTK is an external C++ library, not a compiler toolchain. The wizard therefore continues to use a normal Code::Blocks C++ compiler profile and only supplies the FLTK-specific build configuration.

Homebrew remains an external dependency provider. Code::Blocks does not bundle FLTK or Homebrew.

---

## 2. Implementation

v3.6 changes only the existing FLTK wizard script.

The macOS path now uses the installed Homebrew FLTK package rather than relying on stale or manually configured library paths. The generated project remains a conventional Code::Blocks project with normal Debug and Release targets.

The wizard does not:

- create an FLTK-specific compiler profile;
- introduce CMake, Meson, or Xcode project generation;
- hard-code a separate compiler toolchain;
- bundle FLTK into Code::Blocks;
- add a general-purpose package-management abstraction.

This is intentionally a one-file repair of the existing wizard rather than an architectural change.

---

## 3. Generated project behaviour

The generated sample remains a small FLTK application using:

```cpp
#include <FL/Fl.H>
#include <FL/Fl_Window.H>
#include <FL/Fl_Box.H>
```

The sample creates an `Fl_Window` containing an `Fl_Box` with a visible **Hello World!** label.

The tested workflow is:

```text
New Project → FLTK application → Finish → Build → Run
```

The generated application compiled, linked, launched, and displayed the expected native FLTK window on the tested Apple Silicon macOS configuration.

---

## 4. File changed

| File / area | v3.6 role |
|---|---|
| Existing Scripted Wizard FLTK application script | Corrects the macOS FLTK configuration so the generated project can build and link against the Homebrew installation. |

No changes were required in the compiler framework, project model, build-command generator, or native compiler auto-detection bridge.

The script file is `src/plugins/scriptedwizard/resources/fltk/wizard.script`

---

## 5. Milestones

| | Objective | Check | Status |
|---|---|---|---|
| M1 | Repair the existing FLTK wizard for the tested Homebrew/macOS setup. | A fresh FLTK project is generated without manual project repair. | **DONE** |
| M2 | Validate the complete generated-project path. | The project builds, links, runs, and displays the expected FLTK window. | **DONE** |

---

## 6. Acceptance

v3.6 is accepted because:

1. New Project → FLTK application generates a conventional Code::Blocks C++ project.
2. The generated project uses the external Homebrew FLTK installation on the tested Apple Silicon macOS configuration.
3. The project builds and links without manual post-generation changes.
4. The resulting application launches and displays the expected FLTK window.
5. The implementation is confined to the existing wizard script.
6. No new build system, compiler profile, or dependency subsystem was introduced.

---

## 7. What stays rejected

- Bundling FLTK or Homebrew into Code::Blocks.
- Creating an FLTK-specific compiler profile.
- Replacing the ordinary Code::Blocks project with CMake, Meson, or Xcode generation.
- Adding project-local absolute compiler paths.
- Expanding this one-file wizard repair into a general dependency-management redesign.
- Folding the separately observed Fortran compiler-selection problem into v3.6.

---

## 8. Done when

**DONE.** A user can select the FLTK application wizard on the tested Apple Silicon macOS configuration, generate the project, build it, and run the resulting native FLTK application without manually repairing the generated project.

The final implementation is intentionally modest:

**one existing wizard script was corrected; Homebrew supplies FLTK; the normal Code::Blocks C++ compiler and build machinery remain authoritative; and the generated application remains an ordinary Code::Blocks project.**
