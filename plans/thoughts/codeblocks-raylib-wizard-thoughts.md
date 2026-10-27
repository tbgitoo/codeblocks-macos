# Thoughts: Replace the Legacy Irrlicht Wizard with raylib

**Date:** 27 October 2026  
**Status:** Architectural thoughts for a future Code::Blocks wizard, not an implementation plan

## Motivation

Code::Blocks contains an Irrlicht project wizard dating from an era when Irrlicht was a reasonable lightweight route from C++ to a graphical 3D application.

The wizard is now better viewed as a historical artefact than as the obvious beginner-facing game and graphics option. Irrlicht itself still exists, but the current stable SDK remains 1.8.5.

A modern replacement worth investigating is **raylib**.

raylib deliberately describes itself as a simple, easy-to-use library for videogame programming, without a large visual IDE surrounding it. It supports Windows, Linux and macOS, among other platforms.

That philosophy fits Code::Blocks remarkably well.

## Proposed responsibility boundary

The wizard should **not** attempt to provide a game engine, asset editor or project ecosystem.

It should do something much smaller:

```text
Code::Blocks
    |
    +-- raylib project wizard
    |
    +-- ordinary C/C++ source files
    |
    +-- platform compiler
    |
    +-- installed raylib
    |
    +-- executable game
```

The result remains recognisably a C or C++ project.

For a beginner, that is valuable because the distance between source code and visible behaviour can be extremely small:

```cpp
#include "raylib.h"

int main(void)
{
    InitWindow(800, 450, "My game");

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

There is no scene editor, scripting layer or engine-specific development environment between the learner and the program.

## macOS integration

Homebrew currently provides raylib directly:

```bash
brew install raylib
```

The Homebrew formula provides an Apple Silicon macOS bottle.

raylib's macOS documentation also describes using its `pkg-config` metadata to obtain the required compiler and linker flags.

That suggests a modern Code::Blocks wizard need not hard-code an elaborate model of a Homebrew installation.

Conceptually, configuration could be derived from:

```bash
pkg-config --cflags raylib
pkg-config --libs raylib
```

The exact Code::Blocks implementation should be investigated rather than presumed. In particular, the existing GLFW, SFML and SDL3 work may already establish useful conventions for Homebrew prefix detection and Apple Silicon versus Intel paths.

## Why raylib rather than Blender or Godot?

This is not really the same product category.

Blender would bring an asset-production environment into the discussion.

Godot is a complete contemporary game engine with its own development environment. C++ integration exists through GDExtension bindings.

That makes Godot powerful, but much less interesting as a **Code::Blocks wizard**. Code::Blocks and Godot would begin competing over ownership of the development workflow.

raylib does almost the opposite.

Its deliberately code-oriented design leaves Code::Blocks with a meaningful job:

> raylib supplies game functionality; Code::Blocks supplies the C/C++ development environment.

## Educational value

This is probably the strongest reason to do it.

The existing SFML work has already shown the attraction of letting a beginner write real C++ and very quickly obtain something graphical.

raylib pushes this further towards games while keeping the underlying programming conspicuous.

A learner can encounter:

```cpp
Vector2 position;
```

then:

```cpp
position.x += velocity.x;
```

then collision detection, arrays of enemies, functions, structs, classes, resource lifetime and eventually larger program organisation.

The graphics provide motivation without hiding the C/C++ program underneath them.

That fits the philosophy behind the recent Code::Blocks beginner work rather nicely: **reduce incidental setup complexity, not programming itself.**

## What the first wizard should not become

This is important, especially given what the Arduino archaeology demonstrated.

A raylib wizard should probably **not** initially attempt to provide:

- an asset manager;
- a visual scene editor;
- a package manager;
- an engine configuration system;
- raylib installation itself;
- complex game templates;
- abstractions over the raylib API;
- a Code::Blocks-specific game framework.

Its job can be boring:

1. Create a valid minimal project.
2. Locate or configure raylib.
3. Configure compilation and linking correctly.
4. Generate a tiny working source file.
5. Build and run it.

**Boring wizard, exciting program.**

## Cross-platform consideration

Homebrew makes macOS particularly attractive for the first implementation, but raylib itself is multiplatform.

Therefore, the architecture should avoid equating:

```text
raylib = Homebrew
```

Instead:

```text
raylib wizard
      |
      +-- macOS installation discovery
      |       Homebrew is an excellent case
      |
      +-- Windows installation discovery
      |
      +-- Linux installation discovery
```

Homebrew is a convenient provider, not part of the project's conceptual architecture.

That is the same distinction that emerged during the Arduino discussion.

## Relationship to the existing Irrlicht wizard

It would be unwise to frame the work as:

> Upgrade the Irrlicht wizard into a raylib wizard.

They are different libraries with different assumptions.

Instead, the old wizard is useful **evidence that Code::Blocks historically considered game-engine project creation within scope**.

The cleaner investigation would be:

1. Inspect the existing Irrlicht wizard.
2. Identify generic wizard machinery worth reusing.
3. Identify assumptions specific to Irrlicht and discard them.
4. Inspect the recently modernised SFML, GLFW and SDL3 wizards for current conventions.
5. Implement raylib as a new, minimal integration.
6. Only afterwards decide whether Irrlicht should remain available, become explicitly legacy, or disappear.

That avoids accidentally importing twenty years of fossil strata into something intended to be simple.

## Provisional conclusion

This feels much more defensible than trying to rejuvenate Irrlicht merely because an Irrlicht wizard already exists.

The proposition is not:

> Code::Blocks should gain a modern game engine.

It is:

> **Code::Blocks should make it exceptionally easy to start a small, real C/C++ game using a modern library designed around direct programming.**

raylib is unusually well matched to that proposition.

There is also a pleasing architectural pattern emerging from the surrounding wizard archaeology:

```text
Arduino  -> Arduino CLI owns Arduino
Java     -> JDK owns Java
raylib   -> raylib owns graphics/game functionality

              Code::Blocks
                   |
        owns the programming experience
```

That may be the larger modernisation principle hiding underneath these apparently unrelated fossilised wizards:

> **Code::Blocks becomes better by owning less, not more.**

## References

- [raylib official website](https://www.raylib.com/)
- [raylib on Homebrew Formulae](https://formulae.brew.sh/formula/raylib)
- [raylib wiki: Working on macOS](https://github.com/raysan5/raylib/wiki/Working-on-macOS)
- [Irrlicht Engine downloads](https://irrlicht.sourceforge.io/?page_id=10)
- [Godot Engine documentation: GDExtension C++ example](https://docs.godotengine.org/en/stable/tutorials/scripting/cpp/gdextension_cpp_example.html)
