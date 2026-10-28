# Replace the Legacy Irrlicht Wizard with raylib

**Date:** 28 October 2026  
**Status:** First-pass implementation note

## Objective

Replace the **new-project Irrlicht wizard** with a raylib wizard.

This does **not** deprecate Irrlicht projects and does **not** remove Irrlicht support from Code::Blocks generally. Existing `.cbp` projects using Irrlicht remain ordinary C/C++ projects and are outside the scope of this change. The change only replaces the Irrlicht entry offered for creation of new projects by the Scripted Wizard plugin.

raylib is a good fit because the resulting project remains an ordinary C/C++ executable using a library. Code::Blocks can therefore continue to own the normal edit, build, run and debug workflow without introducing a special build system or runtime integration.

## First-pass implementation

Use the current Irrlicht wizard as the physical starting point, but use the recently modernised GLFW and SDL3 wizards as implementation references for current macOS/Homebrew handling.

### 1. Rename the wizard resource directory

Rename:

```text
src/plugins/scriptedwizard/resources/irrlicht
```

to:

```text
src/plugins/scriptedwizard/resources/raylib
```

This is a wizard-resource rename, not a migration of Irrlicht projects to raylib.

### 2. Update build-system references

Review and update at least the following build metadata:

- `configure.ac`
  - replace `src/plugins/scriptedwizard/resources/irrlicht/Makefile`
  - with `src/plugins/scriptedwizard/resources/raylib/Makefile`

- `src/plugins/scriptedwizard/Makefile.am`
  - replace the Irrlicht resource subdirectory with the raylib resource subdirectory.

- `src/plugins/scriptedwizard/resources/raylib/Makefile.am`
  - update the package/resource directory and any Irrlicht-specific installed resource names.

After the textual changes, search the scripted-wizard/build metadata for remaining `irrlicht` references and distinguish intentionally retained legacy references from stale wizard references.

### 3. Change wizard registration

Update:

```text
src/plugins/scriptedwizard/resources/config.scpt
```

Replace the Irrlicht wizard registration with the raylib wizard registration, including the resource directory/name used by the registration.

Do not retain both wizard entries for the first pass. The intended user-facing change is:

```text
New project: Irrlicht  ->  New project: raylib
```

Existing Irrlicht projects are unaffected.

### 4. Replace the starter source

Completely replace:

```text
src/plugins/scriptedwizard/resources/raylib/files/main.cpp
```

with a minimal raylib program. The starter should demonstrate immediate visible output while remaining small enough for a beginner to understand, for example:

```cpp
#include "raylib.h"

int main(void)
{
    InitWindow(800, 450, "raylib project");

    while (!WindowShouldClose())
    {
        BeginDrawing();
        ClearBackground(RAYWHITE);
        DrawCircle(400, 225, 50, RED);
        EndDrawing();
    }

    CloseWindow();
    return 0;
}
```

The generated project should remain an ordinary C/C++ executable project so that normal Code::Blocks build, run and debugger behaviour remains available.

### 5. Rewrite `wizard.script`

Completely review:

```text
src/plugins/scriptedwizard/resources/raylib/wizard.script
```

Do not mechanically translate Irrlicht-specific setup.

Use the current GLFW and/or SDL3 wizard scripts as the primary local references for:

- compiler selection and validation;
- macOS handling;
- Homebrew detection conventions already used by this Code::Blocks distribution;
- include and library path configuration;
- useful failure messages when the dependency is missing.

For macOS, raylib is available through Homebrew with:

```bash
brew install raylib
```

The first implementation should configure an already installed raylib. It should not turn the wizard into a package manager unless the existing GLFW/SDL3 wizard convention already deliberately offers that behaviour.

Prefer discovery mechanisms exposed by the installed package, such as `pkg-config`, where they integrate cleanly with the existing scripted-wizard model; otherwise follow the established GLFW/SDL3 conventions rather than introducing a new dependency-discovery architecture solely for raylib.

### 6. Images and branding are second-pass work

The renamed resource directory will initially still contain Irrlicht artwork unless it is separately replaced.

That is acceptable for the first functional pass.

After project generation, build and execution work correctly:

- obtain an appropriate raylib logo/icon for the wizard collection;
- replace the larger wizard image;
- verify licences/attribution before redistributing third-party artwork.

Do not block functional implementation on artwork.

## First-pass acceptance criteria

The first pass is complete when:

1. Code::Blocks configures and builds with the renamed wizard resources.
2. The New Project dialogue shows **raylib** instead of **Irrlicht**.
3. The wizard creates a clean ordinary C/C++ project containing the raylib starter source.
4. On a Mac with raylib installed, the generated project builds without manual repair.
5. Run opens the raylib window and displays the starter graphic.
6. A breakpoint in `main.cpp` can be used through the normal Code::Blocks native debugger workflow.
7. Missing raylib installation/configuration produces a useful wizard or build-time diagnostic rather than a project requiring repairs across unrelated settings.
8. Existing Irrlicht `.cbp` projects remain unaffected by the removal of the Irrlicht **new-project wizard**.

## Explicit non-goals

The first pass does not need to:

- migrate existing Irrlicht projects to raylib;
- remove Irrlicht compatibility from Code::Blocks;
- provide an asset manager or scene editor;
- provide a Code::Blocks game framework;
- wrap or abstract the raylib API;
- add complex game templates;
- solve every Windows/Linux installation configuration before the macOS path is proven;
- perfect icons or wizard artwork before the generated project works.

## Implementation principle

Keep this deliberately small:

> **Replace an obsolete new-project starting point with a modern library while preserving the normal Code::Blocks C/C++ project model.**

The wizard removes setup friction. raylib owns graphics/game functionality, and Code::Blocks continues to provide the ordinary programming, build, run and debug experience.

## References

- [raylib official website](https://www.raylib.com/)
- [raylib Homebrew formula](https://formulae.brew.sh/formula/raylib)
- [raylib macOS wiki](https://github.com/raysan5/raylib/wiki/Working-on-macOS)

