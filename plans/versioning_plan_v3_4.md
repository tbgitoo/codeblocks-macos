# Code::Blocks v3.4 — replace the legacy Irrlicht new-project wizard with raylib
  
**Status: DONE.** The Scripted Wizard Irrlicht new-project entry has been replaced by a functional raylib wizard. A generated raylib project has been built and run successfully on macOS. This change does **not** deprecate Irrlicht projects and does **not** remove Irrlicht support from Code::Blocks generally.

**Question this version answers:** what is the smallest change that replaces an obsolete beginner-facing Irrlicht new-project starting point with a current, code-oriented graphics/game library while preserving the ordinary Code::Blocks C/C++ build, run, and debugger workflow?

The answer implemented in v3.4 is deliberately narrow: replace the Scripted Wizard resource, registration, starter source, and artwork with raylib equivalents. Existing Irrlicht `.cbp` projects and the separate legacy `src/templates` Irrlicht material remain outside this change.

---

## 1. Scope and responsibility boundary

raylib remains an ordinary C/C++ library from the perspective of Code::Blocks. The generated project is therefore a normal executable project:

```text
Code::Blocks
    |
    +-- ordinary C/C++ project
    +-- native compiler/linker
    +-- native debugger
    +-- installed raylib
    |
    +-- executable game/application
```

No custom build system, interpreter layer, game framework, asset manager, scene editor, or Code::Blocks-specific raylib abstraction is added.

The change replaces only:

```text
New project: Irrlicht  ->  New project: raylib
```

It does not migrate, rewrite, disable, or remove existing Irrlicht projects.

---

## 2. Implementation completed

### 2.1 Scripted Wizard resource rename

The wizard resource directory was renamed:

```text
src/plugins/scriptedwizard/resources/irrlicht
```

to:

```text
src/plugins/scriptedwizard/resources/raylib
```

The renamed resource contains the raylib `wizard.script`, starter source, build metadata, icon, and wizard artwork.

### 2.2 Build-system and registration references

The resource rename required corresponding updates to the Scripted Wizard packaging/build metadata and registration, including:

```text
configure.ac
src/plugins/scriptedwizard/Makefile.am
src/plugins/scriptedwizard/resources/config.scpt
src/plugins/scriptedwizard/resources/raylib/Makefile.am
```

The Code::Blocks project files that explicitly listed the old Scripted Wizard path were also updated from:

```text
plugins/scriptedwizard/resources/irrlicht/wizard.script
```

to:

```text
plugins/scriptedwizard/resources/raylib/wizard.script
```

in:

```text
src/CodeBlocks_Windows.cbp
src/CodeBlocks_wx30-unix.cbp
src/CodeBlocks_wx32-unix.cbp
src/CodeBlocks_wx32.cbp
src/CodeBlocks_wx32_64.cbp
src/CodeBlocks_wx33.cbp
src/CodeBlocks_wx33_64.cbp
src/CodeBlocks_wx33_arm.cbp
```

### 2.3 Deliberately retained Irrlicht material

A repository-wide `git grep -ni irrlicht` identified unrelated and legacy Irrlicht references. These were deliberately not globally replaced.

In particular:

- Irrlicht paths occurring in comments/examples in `src/plugins/debuggergdb/gdb_driver.cpp` are unrelated to the wizard and remain unchanged.
- The separate legacy material under `src/templates/common`, `src/templates/unix`, and `src/templates/win32` remains unchanged.

This is intentional. v3.4 removes the Irrlicht **Scripted Wizard new-project entry**, not Irrlicht compatibility or historical template material generally.

---

## 3. raylib wizard behaviour

The raylib wizard follows the recently modernised SDL3/GLFW conventions rather than mechanically translating Irrlicht-specific assumptions.

### macOS

The wizard uses the existing Code::Blocks Homebrew helper machinery for the `raylib` formula. It:

- detects Homebrew access;
- checks whether `raylib` is installed;
- can offer the established Homebrew installation flow when the formula is missing;
- validates the accepted prefix through `include/raylib.h` and the raylib library under `lib`;
- records the accepted include/library locations for project generation.

The generated project links raylib and the required native macOS framework options used by the implementation.

### Windows

The first implementation deliberately does not invent an automatic raylib SDK download contract.

The wizard instead uses the existing generic path-selection/global-variable machinery. It validates:

- an include directory containing `raylib.h`;
- a library directory accepted for `raylib`.

The exact include and library directories accepted during validation are retained and used by `SetupProject()`. This avoids validating one location and later linking against another.

The project also adds the Windows system libraries required by the implemented raylib configuration.

### Other Unix-like platforms

The wizard uses the existing command-style project options:

```text
pkg-config --cflags raylib
pkg-config --libs raylib
```

No new package-management architecture is introduced.

---

## 4. Generated starter project

The old Irrlicht demonstration source was completely replaced.

The generated `main.cpp` is a small raylib program that opens an 800 x 450 window, runs the ordinary raylib drawing loop, renders simple text and a red circle, and closes normally.

The starter deliberately remains ordinary, readable C/C++. It demonstrates immediate graphical behaviour without introducing a framework on top of raylib.

The generated project remains a normal Code::Blocks project with normal Debug and Release targets. The Debug target retains debugging symbols and the native Code::Blocks debugger path; the Release target retains the ordinary optimised executable workflow.

---

## 5. Wizard graphics and licensing

The legacy Irrlicht artwork was replaced after the functional wizard was proven.

### `logo.png`

`logo.png` is a downsampled version of the official raylib logo published on Wikimedia Commons under the Creative Commons Attribution-ShareAlike 4.0 International licence. The asset notice records the source, author/licence, and the resizing modification.

### `wizard.png`

`wizard.png` is new artwork created for the Code::Blocks raylib wizard. It uses a black-and-white, square-based Snake composition, a raylib-style heading, and the caption:

```text
CODE. BUILD. PLAY.
```

The upper raylib name/logo element is treated conservatively as derivative of the raylib logo. The combined wizard artwork is therefore distributed under CC BY-SA 4.0 with attribution to the original raylib logo author and the new composition's author.

The image was rendered from a small raylib program consisting of ordinary raylib drawing primitives. The source used to produce the image is retained with the asset/licence information for provenance and reproducibility.

---

## 6. Files changed

| File / area | v3.4 role |
|---|---|
| `src/plugins/scriptedwizard/resources/raylib/` | Replaces the former Scripted Wizard Irrlicht resource directory. |
| `src/plugins/scriptedwizard/resources/raylib/wizard.script` | New raylib wizard logic based primarily on the current SDL3/GLFW conventions. |
| `src/plugins/scriptedwizard/resources/raylib/files/main.cpp` | Minimal raylib starter application replacing the Irrlicht demonstration. |
| `src/plugins/scriptedwizard/resources/raylib/Makefile.am` | Installs/packages the renamed raylib wizard resources. |
| `src/plugins/scriptedwizard/resources/config.scpt` | Registers raylib instead of Irrlicht in the Scripted Wizard project list. |
| `src/plugins/scriptedwizard/Makefile.am` | Includes the raylib resource directory instead of the former Irrlicht resource directory. |
| `configure.ac` | Generates the raylib resource Makefile instead of the Irrlicht resource Makefile. |
| `src/CodeBlocks_*.cbp` files listed in section 2.2 | Replace explicit references to `resources/irrlicht/wizard.script` with `resources/raylib/wizard.script`. |
| raylib wizard `logo.png`, `wizard.png`, and asset/licence note | Replace Irrlicht branding while recording logo provenance/licensing. |

Not modified for this work: the separate legacy Irrlicht material under `src/templates`, Irrlicht-related debugger comments, the general Code::Blocks build model, compiler infrastructure, debugger engine, or existing Irrlicht `.cbp` projects.

---

## 7. Milestones

| | Objective | Check | Status |
|---|---|---|---|
| M1 | Bound the change to replacement of the Irrlicht Scripted Wizard, not global Irrlicht removal. | Repository search identifies wizard references separately from unrelated debugger/template material. | **DONE** |
| M2 | Rename the wizard resource and update build/registration references. | Code::Blocks builds and the New Project collection exposes raylib from the renamed resource. | **DONE** |
| M3 | Replace Irrlicht-specific wizard logic with raylib configuration. | macOS Homebrew raylib is accepted; generated project contains valid raylib include/link configuration. Windows remains manual rather than gaining an unverified download mechanism. | **DONE** |
| M4 | Replace the generated Irrlicht demonstration with a minimal raylib program. | A fresh generated project contains raylib source rather than `irrlicht.h`/Irrlicht demonstration code. | **DONE** |
| M5 | Build and run the generated project on macOS. | The generated project builds without manual project repair; Run opens the raylib window and renders the starter text and red circle. raylib runtime information is visible in the Code::Blocks Program Console. | **DONE** |
| M6 | Replace temporary Irrlicht branding and record licensing/provenance. | raylib icon and black-and-white Snake wizard artwork replace Irrlicht imagery; asset notice records the CC BY-SA 4.0 logo basis and derivative artwork treatment. | **DONE** |

---

## 8. Acceptance

v3.4 is accepted when all of the following hold:

1. The source tree configures/builds with `src/plugins/scriptedwizard/resources/raylib` in place of the former scripted-wizard Irrlicht resource.
2. New Project presents raylib rather than Irrlicht through the Scripted Wizard registration.
3. A fresh raylib project contains the new raylib starter source, not residual Irrlicht source.
4. On the tested macOS installation, the wizard accepts the installed raylib configuration without post-generation project repair.
5. The generated project compiles and links through the ordinary Code::Blocks C/C++ project model.
6. Run opens the raylib application window and renders the starter output.
7. The project remains compatible with the ordinary native Code::Blocks debugging workflow; no raylib-specific debugger integration is introduced.
8. The wizard collection icon and larger wizard graphic are raylib-specific and their provenance/licensing is documented.
9. Existing Irrlicht `.cbp` projects are not migrated or disabled by this change.
10. Unrelated Irrlicht references and the separate material under `src/templates` are not mechanically renamed to raylib.

The implementation completed in v3.4 satisfies the functional build/run path and the wizard replacement described above.

---

## 9. What stays rejected

- Global removal or deprecation of Irrlicht support.
- Migration of existing Irrlicht projects to raylib.
- Mechanical global replacement of every repository occurrence of `irrlicht`.
- Reworking the separate legacy `src/templates` Irrlicht material as part of this change.
- A raylib-specific build system.
- A Code::Blocks game framework or abstraction over the raylib API.
- Asset management, a visual scene editor, or complex starter games.
- Bundling raylib into Code::Blocks.
- A new package manager solely for raylib.
- Inventing a Windows raylib download/archive convention without first establishing and testing a specific distribution contract.
- Changes to the debugger engine merely because the generated application uses raylib.
- Expanding the work into general graphics-wizard architecture changes.

---

## 10. Risks and maintenance boundary

**raylib installation remains external.** The wizard locates/configures the library; raylib itself is not made part of Code::Blocks.

**Platform layouts can change.** The wizard should continue to validate the actual header/library paths it intends to write into the generated project. Windows automatic dependency installation remains deliberately outside v3.4 until a concrete distribution layout is selected and verified.

**Legacy Irrlicht material still exists elsewhere in the repository.** That is intentional and must not be mistaken for a failed wizard rename. This version changes the Scripted Wizard new-project offering only.

**Artwork has a separate licensing boundary.** The official logo source and derivative wizard image carry their recorded CC BY-SA 4.0 treatment; this is separate from raylib's software licence.

---

## 11. Done when

**DONE.** A user can choose raylib from the Code::Blocks New Project collection, create an ordinary C/C++ raylib project, build it, and run a graphical raylib application without repairing the generated project by hand on the tested macOS configuration.

The legacy Irrlicht Scripted Wizard is no longer the offered new-project starting point. This does not declare Irrlicht unsupported: existing Irrlicht projects remain ordinary Code::Blocks projects, and unrelated legacy/template material is left in place.

The result is intentionally small: **the wizard removes setup friction; raylib supplies the graphical/game functionality; Code::Blocks retains the ordinary C/C++ programming, build, run, and debug experience.**

## References

- [raylib official website](https://www.raylib.com/)
- [raylib Homebrew formula](https://formulae.brew.sh/formula/raylib)
- [raylib macOS wiki](https://github.com/raysan5/raylib/wiki/Working-on-macOS)
- [Official raylib logo on Wikimedia Commons](https://commons.wikimedia.org/wiki/File:Raylib_logo.png)
- [Creative Commons Attribution-ShareAlike 4.0 International](https://creativecommons.org/licenses/by-sa/4.0/)
