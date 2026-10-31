# Code::Blocks v3.8 — modernise the OGRE application wizard on macOS
**Status: PLANNED / INVESTIGATIVE.** v3.8 investigates whether the existing Code::Blocks OGRE application wizard can become a genuinely beginner-usable starting point on modern Apple Silicon macOS while targeting a current upstream OGRE release rather than an obsolete compatibility version.
**Question this version answers:** can Code::Blocks hide the otherwise substantial OGRE setup burden well enough that a user can choose New Project -> OGRE application and reach an impressive, editable 3D C++ example without first learning OGRE's dependency and build infrastructure?
The starting evidence is unusually strong: pristine upstream OGRE **v14.6.2** was cloned at its release tag, configured with CMake, built from source on the tested Apple Silicon Mac without modifying OGRE source, and the resulting `SampleBrowser.app` launched successfully. Its OpenGL rendering subsystem initialised and rendered an upstream 3D sample correctly.
The remaining question is therefore not whether current OGRE runs on this Mac. The question is whether the successful manual procedure can be converted into a bounded, explicit and reusable Code::Blocks wizard workflow without making Code::Blocks responsible for maintaining OGRE itself.
---
## 1. Scope and design boundary
The intended architecture is:
```text
Code::Blocks
    |
    +-- existing OGRE scripted wizard, modernised
    +-- ordinary C++ compiler profile
    +-- generated OGRE application
    |
    +-- macOS dependency workflow
            |
            +-- Homebrew helper: build dependencies
            +-- Git helper: official OGRE release checkout
            +-- CMake helper: configure and build OGRE
            |
            +-- stable current upstream OGRE SDK
```
OGRE remains an external C++ graphics engine. Code::Blocks must not fork OGRE, carry private macOS patches for OGRE, or reproduce OGRE's build logic itself.
A source-build acquisition path is acceptable because the exact current upstream release has already demonstrated a clean, unmodified build on the tested Mac. Code::Blocks may automate that proven upstream recipe; it must not repair upstream source.
The intended user experience is:
```text
New Project -> OGRE application
        |
        +-- verify prerequisite tools
        +-- offer explicit dependency setup if required
        +-- clone the official versioned OGRE release
        +-- configure and build it with CMake
        +-- generate a small ordinary C++ project
        |
        v
Build and Run
        |
        v
visually compelling 3D example
```
The wizard should hide installation complexity, not the interesting C++ and 3D programming.
---
## 2. Proven manual build recipe
The following exact procedure was tested successfully on the target Apple Silicon macOS system:
```bash
cd ~/git

git clone --branch v14.6.2 --depth 1 \
    https://github.com/OGRECave/ogre.git ogre-14.6.2

cd ogre-14.6.2

brew install cmake pkgconf sdl2

cmake -S . -B build-codeblocks \
    -DCMAKE_BUILD_TYPE=Release \
    -DCMAKE_INSTALL_PREFIX="$HOME/Library/CodeBlocks/ogre-14.6.2"

cmake --build build-codeblocks -j"$(sysctl -n hw.ncpu)"

open build/bin/SampleBrowser.app
```
The checkout entered detached-HEAD state at the `v14.6.2` release tag, which is appropriate for testing and consuming an immutable upstream release.
The procedure produced a complete build, including the native macOS `SampleBrowser.app`. The application launched, presented OGRE's normal rendering-system configuration dialog, initialised the OpenGL rendering subsystem with the defaults, and rendered a working upstream sample with live rendering statistics.
This exact recipe is the technical baseline for v3.8. The automated workflow should initially mirror the known-good commands rather than inventing a different installation process prematurely.
The location `~/git/ogre-14.6.2` is a developer test location only. The final wizard-managed source will be `~/Library/CodeBlocks/ogre-14.6.2`
---
## 3. Existing wizard investigation
Before implementation, the legacy Code::Blocks OGRE wizard must be characterised rather than patched blindly.
The investigation must establish:
- which OGRE generation and API the existing sample targets;
- what source files and assets it generates;
- which headers, libraries, plugins and runtime resources it assumes;
- which platform-specific paths and renderer assumptions are obsolete;
- whether the existing sample can reasonably be adapted to OGRE 14.6.x;
- whether replacing the sample while preserving the wizard entry is cleaner;
- which parts of the successful upstream build are required by an external application;
- whether `cmake --install` produces a stable SDK layout that can be consumed independently of the source/build tree;
- which runtime plugin and resource configuration must accompany a generated macOS application;
- whether the renderer configuration dialog can safely be avoided by selecting known-working defaults in the generated application.
The next technical gate is:
```text
clean OGRE v14.6.2 checkout and build
        |
        v
install into a temporary test prefix
        |
        v
minimal external C++ application outside OGRE source tree
        |
        v
compile + link + run successfully
```
Only after that external application works should the wizard be taught the resulting include, library, plugin and runtime configuration.
---
## 4. Reusable macOS helper architecture
v3.8 should not place a long chain of platform commands directly inside `wizard.script`. The macOS setup should be split into small reusable native helper modules following the existing Homebrew precedent.
### 4.1 Homebrew helper: prerequisites
The existing helper path:
```text
src/plugins/scriptedwizard/macbrew.cpp
src/plugins/scriptedwizard/macbrew.h
```
should remain responsible for Homebrew discovery, user choice and explicit formula installation.
For the proven OGRE recipe, Homebrew supplies:
```text
cmake
pkgconf
sdl2
```
Code::Blocks does not install Homebrew. The established behaviour remains: locate a usable `brew`, let the user identify one if necessary, or direct the user to Homebrew's installation information.
### 4.2 Git helper: official release acquisition
Add a small reusable module of the form:
```text
src/plugins/scriptedwizard/git.cpp
src/plugins/scriptedwizard/git.h
```
The Git helper should provide the same style of bounded capability as the Homebrew helper:
- locate and validate a usable `git` executable;
- report whether Git is available to Code::Blocks;
- let the user identify an existing Git executable if automatic discovery fails;
- point the user towards Git installation information if Git is absent;
- **not install Git**;
- execute an explicit, versioned shallow clone of the official upstream repository;
- report command, destination, completion status and useful output to the wizard;
- refuse to overwrite an unrelated non-empty destination;
- support cancellation and clear failure reporting.
The initial OGRE acquisition command is based on the proven manual operation:
```bash
git clone --branch v14.6.2 --depth 1 \
    https://github.com/OGRECave/ogre.git ogre-14.6.2
```
The module should be generic rather than named specifically for OGRE.
### 4.3 CMake helper: configure and build
Add a separate reusable module of the form:
```text
src/plugins/scriptedwizard/cmake.cpp
src/plugins/scriptedwizard/cmake.h
```
The CMake helper should:
- locate and validate a usable `cmake` executable;
- normally rely on the Homebrew prerequisite workflow to install CMake when absent;
- run a configured source/build pair without embedding shell syntax in the wizard;
- support explicit CMake definitions such as `CMAKE_BUILD_TYPE=Release`;
- run the build with a bounded parallel-job count derived from the host CPU count;
- expose command, progress/output, exit status and cancellation to the wizard;
- keep source, build and eventual install locations distinct;
- remain generic enough for later source-built dependencies.
The first supported commands should reproduce the successful manual recipe:
```bash
cmake -S . -B build-codeblocks \
    -DCMAKE_BUILD_TYPE=Release \
    -DCMAKE_INSTALL_PREFIX="$HOME/Library/CodeBlocks/ogre-14.6.2"

cmake --build build-codeblocks \
    -j"$(sysctl -n hw.ncpu)"
```
The proven install step is:
```bash
cmake --install build-codeblocks
```

### 4.4 Scripted Wizard bridge
The helper modules should expose narrow reporting APIs to `wizard.script`, similar in spirit to the current Homebrew report strings. The script should orchestrate user decisions and workflow order, while the native helpers own executable discovery, process launch, output capture and cancellation.
The wizard should not become a general shell-script host.
---
## 5. Dependency locations and lifecycle
The final wizard-managed locations remain deliberately undecided until the installed-SDK experiment is complete.
The expected separation is:
```text
official versioned source checkout
        |
        +-- disposable/versioned build directory
        |
        v
versioned installed SDK prefix
        |
        v
Code::Blocks OGRE projects
```
Generated projects must depend only on the stable SDK contract, not on `~/git/ogre-14.6.2` or another developer checkout.
The wizard should:
1. detect an already usable supported OGRE SDK;
2. avoid rebuilding when the expected version and SDK are valid;
3. ask explicitly before downloading or building OGRE;
4. preserve useful build diagnostics on failure;
5. keep version identity explicit so later OGRE upgrades do not silently alter existing projects.
No hand-built OGRE files should be installed into Homebrew-owned prefixes such as `/opt/homebrew`.
---
## 6. Generated beginner project
The upstream `SampleBrowser` proves that OGRE works; it is not the desired beginner template.
The generated project should be small and visually rewarding:
```text
ordinary C++ source
    + OGRE initialisation
    + one camera
    + obvious lighting/material
    + one recognisable 3D object or small scene
    + visible motion or interaction
```
The sample should demonstrate why a full 3D engine is interesting while remaining compact enough that a beginner can identify and edit the important lines.
The first-run path should avoid unnecessary general-purpose renderer configuration if the tested macOS renderer can be selected safely in code. Any defaults must be based on behaviour verified against OGRE 14.6.2.
The generated application should remain an ordinary Code::Blocks C++ project with normal Debug and Release targets. OGRE may be built with CMake, but the user's starter project should not become a CMake or Xcode wrapper unless investigation demonstrates a hard technical requirement.
---
## 7. Expected files / areas
| File / area | Expected v3.8 role |
|---|---|
| Existing OGRE Scripted Wizard resources | Inspect and modernise registration, project generation, platform logic and starter sample. |
| Existing OGRE templates/assets | Adapt or replace obsolete API usage for current OGRE 14.6.x. |
| `src/plugins/scriptedwizard/macbrew.cpp` / `.h` | Reuse the established explicit Homebrew workflow for `cmake`, `pkgconf` and `sdl2`. |
| New `src/plugins/scriptedwizard/git.cpp` / `.h` | Discover/validate Git and perform the explicit official release clone; never install Git. |
| New `src/plugins/scriptedwizard/cmake.cpp` / `.h` | Configure, build and potentially install the versioned upstream OGRE source using the proven commands. |
| Script bindings / wizard bridge | Expose only the narrow generic operations and reports required to orchestrate the workflow. |
Core changes outside the Scripted Wizard plugin require a demonstrated missing capability rather than speculative redesign.
---
## 8. Milestones
| | Objective | Check | Status |
|---|---|---|---|
| M1 | Characterise the legacy wizard and prove the external installed-SDK contract. | Current wizard assumptions are documented; a minimal external application builds and runs against a temporary installed OGRE 14.6.2 SDK without using the source/build tree. | **PLANNED** |
| M2 | Add reusable prerequisite/acquisition helpers. | Existing Homebrew support installs `cmake`, `pkgconf` and `sdl2`; the new generic Git helper validates Git and clones the official tagged release without installing Git. | **PLANNED** |
| M3 | Add the reusable CMake build/install workflow. | The generic CMake helper reproduces the proven Release configure/build commands, supports cancellation/diagnostics, and produces the tested versioned SDK without modifying OGRE. | **PLANNED** |
| M4 | Modernise the OGRE wizard and starter project. | New Project -> OGRE application detects or explicitly establishes the SDK and generates a compact current-API Code::Blocks C++ project with correct build/runtime configuration. | **PLANNED** |
| M5 | Validate the complete beginner path. | From a clean supported Mac, the explicit setup completes and Build and Run displays the intended visually compelling 3D example without manual path or dependency repair. | **PLANNED** |
---
## 9. Acceptance criteria
v3.8 is accepted only if:
1. The workflow targets the official OGRE `v14.6.2` release, or a deliberately documented later current release after repeating the same pristine-build validation.
2. The manual baseline commands are reproducible through the helper architecture.
3. Homebrew supplies only the declared build dependencies; Code::Blocks does not claim a non-existent current OGRE formula.
4. Git is discovered or selected by the user but never installed by Code::Blocks.
5. The official OGRE repository is cloned at an explicit release tag using a shallow versioned checkout.
6. CMake configuration/build runs through a reusable native helper with visible status, failure output and cancellation.
7. OGRE source remains pristine; no Code::Blocks patchset or fork is required.
8. Generated projects consume a stable versioned SDK rather than the source/build checkout.
9. A fresh generated project builds, links and runs without manual include, library, plugin or runtime-path repair.
10. The starter application renders a useful 3D scene and exposes approachable C++ code.
11. Existing Windows/Linux OGRE behaviour is not gratuitously broken while modernising the macOS path.
---
## 10. What stays rejected
- Maintaining a Code::Blocks fork of OGRE or carrying private macOS patches.
- Targeting an obsolete OGRE release merely because an old package-manager formula exists.
- Pretending Homebrew supplies current OGRE when it only supplies the build dependencies.
- Installing Git automatically.
- Installing hand-built OGRE into `/opt/homebrew` or another Homebrew-owned prefix.
- Making generated projects depend on `~/git/ogre-14.6.2`.
- Silently cloning or building a large dependency without explicit user action.
- Embedding the full clone/CMake operation as opaque shell text directly in `wizard.script`.
- Turning the user's generated project into the OGRE engine build.
- Reusing the full upstream SampleBrowser as the beginner template.
- Expanding v3.8 into Program Console ANSI rendering or Darwin/GDB expression-evaluator work.
---
## 11. Stop conditions
Stop and hide/disable the OGRE wizard on macOS rather than escalating scope if a reliable workflow requires:
- modifying upstream OGRE source;
- maintaining a persistent OGRE patchset or fork;
- coupling generated projects to the source/build tree;
- recreating OGRE's build logic rather than invoking its supported CMake interface;
- fragile plugin/runtime manipulation that cannot be expressed as a stable installed SDK contract;
- a setup path still comparable in difficulty to the manual build after automation.
Failure remains an acceptable investigative result. v3.8 may automate a working upstream product, but must not make the Code::Blocks fork responsible for maintaining OGRE.
---
## 12. Done when
**PLANNED.** v3.8 is done when a user on the supported Apple Silicon macOS configuration can choose the OGRE application wizard, explicitly establish the required Homebrew prerequisites and current official OGRE SDK, generate an ordinary Code::Blocks C++ project, and reach a working, visually compelling 3D application through Build and Run without manual dependency or path archaeology.
The intended end state is:
**Homebrew supplies the build prerequisites; a reusable Git helper acquires the official release without installing Git; a reusable CMake helper performs the proven upstream build/install; OGRE remains upstream's engine; and the generated Code::Blocks project exposes the interesting 3D C++ code rather than the installation machinery.**

