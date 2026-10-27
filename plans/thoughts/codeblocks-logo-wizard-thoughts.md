# Thoughts: Build-Level Integration for a Logo Wizard in Code::Blocks

**Date:** 27 October 2026  
**Status:** Architectural proposal and investigation note, not an implementation plan

## 1. Architectural premise

Logo should not be forced through Code::Blocks' conventional C/C++ compiler model.

A Logo program does not normally follow this pipeline:

```text
source -> object files -> linker -> executable
```

The native workflow is closer to:

```text
Logo source -> Logo interpreter -> program output
```

The appropriate integration level is therefore the **project build and execution layer**, not the compiler-definition layer.

Code::Blocks should manage the project, source files and commands. The selected Logo implementation should remain responsible for parsing and executing Logo.

## 2. Responsibility boundary

### Code::Blocks owns

- project creation and project metadata;
- source-file presentation;
- Logo syntax highlighting and editing;
- configurable build and run commands;
- working-directory management;
- environment-variable configuration;
- presentation of interpreter output;
- diagnostic navigation where the backend emits usable file and line information;
- persistent, editable backend settings.

### The Logo backend owns

- language parsing;
- language semantics;
- execution;
- turtle graphics;
- implementation-specific diagnostics;
- implementation-specific command-line behaviour.

Code::Blocks should not provide:

- a Logo interpreter;
- a turtle-graphics implementation;
- a compatibility layer between Logo dialects;
- a fabricated object-file or linker pipeline.

## 3. Build-level contract

The wizard should configure an explicit contract between the project and the selected interpreter:

```text
Logo source
    + interpreter executable
    + interpreter arguments
    + working directory
    + environment
    + optional validation command
    + run command
    = executable project workflow
```

The configured commands must remain visible and editable.

A generic run pattern might be:

```text
<interpreter> <arguments> <source-file>
```

The wizard must not assume that all Logo implementations accept the same command-line syntax.

## 4. Build and Run semantics

For an interpreted language, **Build** and **Run** should remain semantically honest.

### Run

Run invokes the selected Logo interpreter with the project's main source file.

### Build

Build may use one of three backend-dependent behaviours:

1. **No build action**  
   The project runs directly through the interpreter.

2. **Validation action**  
   If the backend supports a non-executing load or syntax check, Build invokes it.

3. **Project preparation action**  
   Build invokes a configurable script or tool that prepares files, resources or tests.

The wizard should select a suitable default for a recognised backend without representing interpretation as native compilation.

## 5. Backend abstraction

UCBLogo is a plausible first reference backend, but the architecture should not equate Logo support with one interpreter.

The backend definition should be able to describe:

- executable discovery rules;
- executable path;
- version query, if available;
- default arguments;
- validation command, if available;
- run command;
- working-directory rules;
- environment variables;
- terminal or graphics-window behaviour;
- diagnostic format, if usable.

A custom interpreter path and custom commands should remain available even when a known backend is detected.

## 6. Wizard scope

The wizard should collect and configure only what is required to create a runnable project:

1. project name and location;
2. main Logo source filename;
3. interpreter backend;
4. interpreter executable path;
5. optional arguments and environment settings;
6. working directory;
7. Build command;
8. Run command;
9. minimal starter source;
10. configuration verification.

Before project creation, the wizard should verify as much of the selected contract as the backend permits, including executable availability and command completeness.

Automatic detection must never prevent manual configuration.

## 7. Generated project

A minimal generated project could be:

```text
MyLogoProject/
    MyLogoProject.cbp
    main.logo
    README.md
```

The generated `README.md` should record:

- the selected backend;
- the expected interpreter command;
- the configured Build and Run behaviour;
- where backend settings can be changed;
- any backend-specific compatibility notes.

Interpreter paths should preferably remain user-specific or platform-specific rather than becoming mandatory absolute paths shared by every project user.

## 8. Platform integration

The architecture should not encode:

```text
Logo = Homebrew
```

It should encode:

```text
Logo project
      |
      +-- detect known interpreter installations
      +-- permit an explicit executable path
      +-- configure commands for the selected backend
```

Homebrew can be one macOS discovery mechanism. Equivalent package-manager, application-bundle, manual and source-built installations should remain configurationally possible.

The same project model should support macOS, Windows and Linux even when backend availability, executable paths and command lines differ.

## 9. Failure modes to avoid

The implementation should explicitly avoid:

- treating `.logo` files as sources that produce `.o` files;
- invoking GCC or Clang to satisfy a compiler-oriented project model;
- hard-coding one interpreter or one Logo dialect into the general architecture;
- hard-coding one Homebrew prefix;
- embedding architecture-specific macOS paths;
- assuming identical commands across platforms;
- hiding generated commands so completely that failures cannot be diagnosed;
- creating projects that require repair across several unrelated settings dialogs;
- placing runtime, graphics or package-management responsibilities inside Code::Blocks;
- claiming cross-platform support without testing each backend and command path.

## 10. Bounded proof of concept

A first proof of concept should establish only the responsibility boundary:

1. Select one known-working Logo interpreter on one platform.
2. Confirm a `.logo` file runs from the command line.
3. Create a Code::Blocks project that invokes the identical command.
4. Confirm Run uses the correct source file and working directory.
5. Confirm interpreter output remains visible.
6. Confirm a missing interpreter produces a useful, actionable error.
7. Test project paths containing spaces.
8. Encode that known-good configuration in a minimal wizard.

Backend selection, additional templates, package-manager integration and cross-platform expansion should follow only after the basic build-level contract is proven.

## 11. General Code::Blocks pattern

Logo belongs to a broader class of integrations in which Code::Blocks should delegate to a native external workflow:

```text
Arduino -> Arduino CLI owns Arduino builds and uploads
Java    -> the JDK or Java build tool owns Java builds and execution
Logo    -> the selected Logo interpreter owns Logo execution

                 Code::Blocks
                      |
        owns project and command integration
```

The broader architectural principle is:

> **Integrate each environment at its native build or execution level instead of forcing every language through the C/C++ compiler model.**

For Logo, that level is the project command layer.

## 12. Provisional conclusion

A Logo wizard is architecturally credible if it creates a transparent, fully configured interpreter-backed project.

The proposition is not:

> Add Logo as an internally modelled Code::Blocks compiler.

It is:

> **Configure Code::Blocks as a project and command front end for an external Logo interpreter, while keeping the complete backend contract visible and editable.**

## References

- [Berkeley Logo official page](https://people.eecs.berkeley.edu/~bh/logo.html)
- [Maintained UCBLogo source repository](https://github.com/jrincayc/ucblogo-code)

