# Code::Blocks v3.8 — modernise the OGRE application wizard on macOS
**Status: PLANNED / INVESTIGATIVE.** v3.8 investigates whether the existing Code::Blocks OGRE application wizard can become a genuinely beginner-usable starting point on modern Apple Silicon macOS while targeting current upstream OGRE rather than an obsolete compatibility version.
**Question this version answers:** can Code::Blocks hide the otherwise substantial OGRE setup burden well enough that a user can choose New Project -> OGRE application and reach an impressive, editable 3D C++ example without first learning OGRE's dependency and build infrastructure?
The evidence is now strong: pristine upstream OGRE **v14.6.2** builds from its official release tag on the tested Apple Silicon Mac without source patches; the upstream `SampleBrowser.app` runs and renders correctly; OGRE installs to a versioned Code::Blocks-owned SDK prefix; and a completely external probe project configures, builds, loads the installed macOS renderer plug-ins, consumes the installed Media resources, and renders a textured, illuminated, slowly rotating `ogrehead.mesh` without using the OGRE source/build tree.
The remaining question is no longer whether the installed SDK contract works. The remaining work is to formalise the cleanest generated-project conventions and automate the successful manual procedure without making Code::Blocks responsible for maintaining OGRE itself.
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
            +-- CMake helper: configure, build and install OGRE
            |
            +-- versioned installed upstream OGRE SDK
```
OGRE remains an external C++ graphics engine. Code::Blocks must not fork OGRE, carry private macOS patches for OGRE, or reproduce OGRE's build logic itself.
A source-build acquisition path is acceptable because the current upstream release has demonstrated a clean, unmodified build and external-SDK consumption path on the tested Mac. Code::Blocks may automate that proven upstream recipe; it must not repair upstream source.
The intended user experience is:
```text
New Project -> OGRE application
        |
        +-- verify prerequisite tools
        +-- offer explicit dependency setup if required
        +-- clone the official versioned OGRE release
        +-- configure, build and install it with CMake
        +-- generate a small ordinary C++ project
        |
        v
Build and Run
        |
        v
textured, illuminated, animated 3D example
```
The wizard should hide installation complexity, not the interesting C++ and 3D programming.
---
## 2. Proven manual build and install recipe
The following workflow has been exercised successfully on the target Apple Silicon macOS system:
```bash
cd ~/git
git clone --branch v14.6.2 --depth 1 \
    https://github.com/OGRECave/ogre.git ogre-14.6.2
cd ogre-14.6.2
brew install cmake pkgconf sdl2
cmake -S . -B build-codeblocks \
    -DCMAKE_BUILD_TYPE=Release \
    -DCMAKE_INSTALL_PREFIX="$HOME/Library/CodeBlocks/ogre-14.6.2"
cmake --build build-codeblocks \
    -j"$(sysctl -n hw.ncpu)"
cmake --install build-codeblocks
```
Before the dedicated `build-codeblocks` install experiment, a normal upstream build also produced and successfully ran:
```bash
open build/bin/SampleBrowser.app
```
The upstream `SampleBrowser.app` presented OGRE's rendering-system configuration dialog, initialised the OpenGL renderer with the tested defaults, and rendered an upstream 3D sample.
**Important M1 finding:** `CMAKE_INSTALL_PREFIX` must be supplied at **configure time**. Installing an already-configured default build using only:
```bash
cmake --install build --prefix <different-prefix>
```
copied an apparently complete SDK but left generated metadata referring to the original build-tree SDK paths. The v3.8 automation must choose the Code::Blocks SDK prefix before CMake configuration, then build and run `cmake --install` without changing the prefix afterwards.
The versioned SDK proven during M1 is:
```text
~/Library/CodeBlocks/ogre-14.6.2
```
Its installed layout contains:
```text
CMake/
Media/
bin/
include/
lib/
```
The source checkout under `~/git/ogre-14.6.2` remains an investigation location only. Generated projects must not depend on it.
---
## 3. M1 findings: legacy wizard and installed-SDK contract
### 3.1 Legacy wizard assumptions
The existing OGRE wizard uses a substantially older model:
- a user choice between an OGRE source tree and a pre-made SDK;
- source paths such as `OgreMain/include` and `Samples/Common/include`;
- installed paths such as `include/Ogre.h`, `samples/include` and `samples/refapp/include`;
- direct linking against `OgreMain`, `OIS` and system `GL`;
- `pkg-config --cflags OGRE` / `pkg-config --libs OGRE` for non-Windows SDKs;
- `gcc*` as the only non-Windows compiler class;
- executables placed in OGRE-owned sample/bin directories.
The modern macOS path should preserve the wizard entry and project concept, but replace this historical OgreMain/OIS/sample-directory model rather than merely adding Apple Silicon paths.
### 3.2 Installed CMake package and external build
The installed SDK provides:
```text
~/Library/CodeBlocks/ogre-14.6.2/CMake/OGREConfig.cmake
~/Library/CodeBlocks/ogre-14.6.2/CMake/OGREConfigVersion.cmake
~/Library/CodeBlocks/ogre-14.6.2/CMake/OgreTargets.cmake
```
A completely external project was created under:
```text
~/git/ogre-m1-external
```
and configured explicitly against the installed package:
```bash
export OGRE_PREFIX="$HOME/Library/CodeBlocks/ogre-14.6.2"
export OGRE_DIR="$OGRE_PREFIX/CMake"
cmake -S . -B build \
    -DCMAKE_BUILD_TYPE=Release \
    -DOGRE_DIR="$OGRE_DIR"
cmake --build build \
    -j"$(sysctl -n hw.ncpu)"
```
CMake found OGRE 14.6.2, the `Bites` component, the installed renderer/plugin set and the installed Media directory. The external application compiled and linked successfully.
Checks of the probe's generated `CMakeCache.txt` and linker command found no references to `~/git/ogre-14.6.2`. Compile and link therefore work against the installed SDK independently of the source/build checkout.
### 3.3 pkg-config is present but not the proven macOS contract
The install provides `.pc` metadata including `OGRE.pc`, `OGRE-Bites.pc`, `OGRE-RTShaderSystem.pc` and `OGRE-Overlay.pc`.
The first post-build `--prefix` installation exposed build-tree paths through pkg-config. The prefix-at-configure-time reinstall corrected the overall SDK generation, but M1 does not assume the legacy wizard's pkg-config path is authoritative merely because `.pc` files exist.
For macOS v3.8, `OGREConfig.cmake` plus the installed SDK layout is the proven compile/link contract. pkg-config may still be evaluated later as an implementation convenience.
### 3.4 Residual Bullet metadata reference
The correctly installed SDK still contains one observed build-tree reference in:
```text
lib/cmake/bullet/BulletConfig.cmake
```
where `BULLET_ROOT_DIR` refers to `build-codeblocks/Dependencies`.
This did not prevent the external OGRE/Bites application from configuring, compiling, linking or rendering. The Code::Blocks fork must not patch upstream OGRE/Bullet metadata merely to remove a non-operative source path. It becomes a blocker only if a supported generated application actually requires that build-tree directory.
### 3.5 macOS plug-in runtime contract
The generic installed metadata reports:
```text
OGRE_PLUGIN_DIR=<SDK>/lib/OGRE
```
and the generic installed `bin/plugins.cfg` likewise uses:
```text
PluginFolder=<SDK>/lib/OGRE
```
However, the macOS Release frameworks are installed under:
```text
<SDK>/lib/macosx/Release/
```
including `RenderSystem_GL.framework`, `RenderSystem_GL3Plus.framework`, scene-manager plug-ins and `Codec_STBI.framework`.
The working installed `SampleBrowser.app` instead uses:
```text
PluginFolder=Contents/Frameworks/
```
For the external probe, a project-local `plugins.cfg` was derived from the installed generic file and rebased to:
```text
PluginFolder=/Users/thomasbraschler/Library/CodeBlocks/ogre-14.6.2/lib/macosx/Release
```
With this configuration, the external probe loaded the installed renderer and scene-manager frameworks and reached OGRE renderer initialisation successfully.
This is key information for M4: generated applications can live outside OGRE and carry their own small plug-in configuration rather than running from OGRE's sample directories.
### 3.6 Resource and Media runtime contract
A project-local `resources.cfg` is the correct boundary for installed Media discovery. Resource-directory knowledge should not be hard-coded into beginner-facing C++.
A minimal hand-written resource list located `ogrehead.mesh` but failed to reproduce all resource groups and dependent materials correctly. In particular, the mesh referenced the material `Ogre/Eyes`, defined in:
```text
<SDK>/Media/materials/scripts/Ogre.material
```
The successful approach was to derive the project-local `resources.cfg` from OGRE's installed canonical file:
```text
<SDK>/bin/resources.cfg
```
The installed file's `../Media` entries were rebased to:
```text
/Users/thomasbraschler/Library/CodeBlocks/ogre-14.6.2/Media
```
and the unnecessary `[Tests]` section was removed for the external probe.
This preserved OGRE's own resource groups and ordering, including:
```text
[OgreInternal]
[Essential]
[General]
[BSPWorld]
```
After this change, the external probe resolved the mesh, material scripts and textures correctly.
The key generated-project convention is therefore:
```text
main.cpp
    -> interesting OGRE scene/animation code only
plugins.cfg
    -> derived from installed OGRE configuration
    -> PluginFolder rebased to installed macOS Release frameworks
resources.cfg
    -> derived from installed canonical resources.cfg
    -> Media paths rebased to the versioned installed SDK
```
### 3.7 External rendering proof
The M1 probe now renders OGRE's installed:
```text
Media/models/ogrehead.mesh
```
with its intended coloured materials and textures, ambient and point lighting, camera/viewport setup, and a frame-rate-independent `FrameListener` that rotates the Ogre head slowly around the vertical axis.
This proves the following external contract:
```text
official OGRE v14.6.2 source
        |
        +-- prefix-aware Release configuration/build/install
        v
~/Library/CodeBlocks/ogre-14.6.2
        |
        +-- installed CMake package
        +-- installed headers/libraries
        +-- installed macOS renderer/plugin frameworks
        +-- installed canonical Media resources
        v
~/git/ogre-m1-external
        |
        +-- external configure/build/link
        +-- project-local plugins.cfg
        +-- project-local resources.cfg
        +-- clean beginner-facing C++ scene code
        v
textured, illuminated, rotating 3D Ogre head
```
### 3.8 M1 closure
M1 is **DONE**. The legacy wizard is characterised and the external installed-SDK contract is proven end to end.
Completed evidence:
- pristine OGRE 14.6.2 build and prefix-aware install proven;
- external CMake package consumption and compile/link independence from the OGRE source/build tree proven;
- project-local macOS `plugins.cfg` proven against `<SDK>/lib/macosx/Release`;
- project-local `resources.cfg` proven by deriving OGRE's canonical `<SDK>/bin/resources.cfg`, rebasing `../Media` to `<SDK>/Media`, and omitting the upstream `[Tests]` section;
- textured, illuminated and frame-rate-independent animated rendering of installed `Media/models/ogrehead.mesh` proven.
The final external probe is frozen as the M4 technical baseline:
```text
~/git/ogre-m1-external/
    CMakeLists.txt
    main.cpp
    plugins.cfg
    resources.cfg
```
The runtime convention is likewise frozen conceptually: `main.cpp` contains only scene/animation code; `plugins.cfg` expresses the installed macOS plug-in location; `resources.cfg` expresses OGRE's canonical installed Media organisation.
The exact generated path representation, whether literal SDK paths, wizard substitutions/macros, or later application-bundle deployment, is **M4 design work**, not an M1 blocker. M1 proves which installed locations must be represented.
A residual build-tree reference remains in `<SDK>/lib/cmake/bullet/BulletConfig.cmake`, where `BULLET_ROOT_DIR` refers to `build-codeblocks/Dependencies`. The proven OGRE/Bites application configures, compiles, links, loads plug-ins/resources, renders and animates without using that location. It is therefore recorded as operationally irrelevant for the v3.8 starter path and will not be patched unless later work actually consumes Bullet through that metadata.
---
## 4. Reusable macOS helper architecture
v3.8 should not place a long chain of platform commands directly inside `wizard.script`. The macOS setup should use small reusable native helper modules following the Homebrew precedent.
### 4.1 Homebrew helper: prerequisites
Reuse:
```text
src/plugins/scriptedwizard/macbrew.cpp
src/plugins/scriptedwizard/macbrew.h
```
for Homebrew discovery, user choice and explicit installation of:
```text
cmake
pkgconf
sdl2
```
Code::Blocks does not install Homebrew.
### 4.2 Git helper: official release acquisition
Add a generic module of the form:
```text
src/plugins/scriptedwizard/git.cpp
src/plugins/scriptedwizard/git.h
```
It should locate/validate Git, let the user identify an existing executable, point to installation information if absent, **never install Git**, perform an explicit versioned shallow clone, refuse unsafe overwrite, and report output/cancellation clearly.
The proven acquisition command is:
```bash
git clone --branch v14.6.2 --depth 1 \
    https://github.com/OGRECave/ogre.git ogre-14.6.2
```
### 4.3 CMake helper: configure, build and install
Add a generic module of the form:
```text
src/plugins/scriptedwizard/cmake.cpp
src/plugins/scriptedwizard/cmake.h
```
It should discover/validate CMake, run source/build pairs, support explicit definitions, derive a bounded parallel-job count, report progress/output/status/cancellation, and keep source/build/install locations separate.
The proven commands are:
```bash
cmake -S . -B build-codeblocks \
    -DCMAKE_BUILD_TYPE=Release \
    -DCMAKE_INSTALL_PREFIX="$HOME/Library/CodeBlocks/ogre-14.6.2"
cmake --build build-codeblocks \
    -j"$(sysctl -n hw.ncpu)"
cmake --install build-codeblocks
```
Do not move the install prefix to `cmake --install --prefix`.
### 4.4 Scripted Wizard bridge
Expose narrow reporting APIs to `wizard.script`. The script should orchestrate user decisions and ordering, while native helpers own executable discovery, process launch, output capture and cancellation. The wizard should not become a general shell-script host.
---
## 5. Dependency locations and lifecycle
The current installed SDK prefix is:
```text
~/Library/CodeBlocks/ogre-14.6.2
```
The expected lifecycle is:
```text
versioned official source checkout
        |
        +-- versioned/disposable build directory
        |
        v
~/Library/CodeBlocks/ogre-14.6.2
        |
        v
Code::Blocks OGRE projects
```
Generated projects must depend only on the installed SDK contract, not on the source checkout or `build-codeblocks`.
The wizard should detect an already usable SDK, avoid unnecessary rebuilding, ask explicitly before download/build, preserve failure diagnostics, and keep version identity explicit.
No hand-built OGRE files should be installed into Homebrew-owned prefixes such as `/opt/homebrew`.
---
## 6. Generated beginner project
The upstream `SampleBrowser` proves OGRE works; it is not the desired beginner template.
The M1 rotating Ogre-head probe is now the leading technical baseline for the starter project:
```text
ordinary C++ source
    + OGRE initialisation
    + one camera
    + ambient and point lighting
    + textured ogrehead.mesh
    + slow vertical-axis rotation
```
The source demonstrates immediately editable concepts such as mesh selection, lighting, camera position, background colour and frame-rate-independent animation.
Generated runtime files should keep setup concerns outside `main.cpp`:
```text
plugins.cfg
resources.cfg
```
The first-run path should avoid unnecessary general-purpose renderer configuration if tested macOS defaults can safely be selected in code.
The generated application should remain an ordinary Code::Blocks C++ project with normal Debug/Release targets. OGRE may be built with CMake, but the user's project should not become a CMake or Xcode wrapper unless a hard requirement is demonstrated.
---
## 7. Expected files / areas
| File / area | Expected v3.8 role |
|---|---|
| Existing OGRE Scripted Wizard resources | Modernise registration, project generation, platform logic and starter sample. |
| Existing OGRE templates/assets | Replace obsolete OgreMain/OIS/sample-directory assumptions with current OGRE/Bites external-SDK usage. |
| `src/plugins/scriptedwizard/macbrew.cpp` / `.h` | Reuse Homebrew workflow for `cmake`, `pkgconf` and `sdl2`. |
| New `src/plugins/scriptedwizard/git.cpp` / `.h` | Discover/validate Git and clone the official tagged release; never install Git. |
| New `src/plugins/scriptedwizard/cmake.cpp` / `.h` | Configure, build and install OGRE using the proven prefix-aware commands. |
| OGRE `main.cpp` template | Small textured and slowly rotating Ogre-head example based on the proven external probe. |
| Generated `plugins.cfg` | Derive from installed OGRE configuration and rebase the macOS plug-in folder. |
| Generated `resources.cfg` | Derive from installed canonical configuration and rebase Media paths to the installed SDK. |
| Script bindings / wizard bridge | Expose only narrow generic operations and reports. |
Core changes outside the Scripted Wizard plugin require a demonstrated missing capability.
---
## 8. Milestones
| | Objective | Check | Status |
|---|---|---|---|
| M1 | Characterise the legacy wizard and prove the external installed-SDK contract. | Legacy assumptions and prefix-aware install are documented; an independent external project configures, builds, loads installed macOS plug-ins and canonical Media resources, and renders a textured, illuminated, slowly rotating Ogre head without requiring the OGRE source/build tree. The four-file probe is frozen as the M4 technical baseline. | **DONE** |
| M2 | Add reusable prerequisite/acquisition helpers. | Existing Homebrew support installs `cmake`, `pkgconf` and `sdl2`; the new generic Git helper validates Git and clones the official tagged release without installing Git. | **PLANNED** |
| M3 | Add the reusable CMake build/install workflow. | The generic CMake helper reproduces the proven prefix-aware Release configure/build/install commands, supports cancellation/diagnostics, and produces the tested SDK without modifying OGRE. | **PLANNED** |
| M4 | Modernise the OGRE wizard and starter project. | New Project -> OGRE application detects or establishes the SDK and generates the compact current-API rotating-Ogre project with correct macOS plug-in/resource/runtime configuration. | **PLANNED** |
| M5 | Validate the complete beginner path. | From a clean supported Mac, explicit setup completes and Build and Run displays the intended visually compelling 3D example without manual repair. | **PLANNED** |
---
## 9. Acceptance criteria
v3.8 is accepted only if:
1. The workflow targets official OGRE `v14.6.2`, or a deliberately validated later release.
2. Homebrew supplies only declared build dependencies.
3. Git is discovered or selected but never installed by Code::Blocks.
4. OGRE is cloned at an explicit release tag using a shallow versioned checkout.
5. CMake receives the final SDK prefix before building and installs through the supported install target.
6. OGRE source remains pristine; no Code::Blocks patchset or fork is required.
7. Generated projects consume the installed SDK without requiring the source/build checkout.
8. macOS plug-in lookup uses a reproducible generated-project or installed-SDK runtime contract.
9. Media/resources are configured reproducibly without hard-coded resource plumbing in beginner-facing C++.
10. A fresh project builds, links and runs without manual include, library, plug-in or runtime-path repair.
11. The starter renders a textured animated 3D scene with approachable C++ code.
12. Existing Windows/Linux OGRE behaviour is not gratuitously broken.
---
## 10. What stays rejected
- Maintaining an OGRE fork or private macOS patches.
- Patching installed OGRE/Bullet metadata merely to remove non-operative paths.
- Targeting obsolete OGRE solely because an old package formula exists.
- Pretending Homebrew supplies current OGRE.
- Installing Git automatically.
- Installing hand-built OGRE into Homebrew-owned prefixes.
- Making generated projects depend on `~/git/ogre-14.6.2` or `build-codeblocks`.
- Silently cloning/building without explicit user action.
- Embedding opaque clone/CMake shell chains directly in `wizard.script`.
- Hard-coding individual Media directories in beginner-facing `main.cpp`.
- Reusing the full SampleBrowser as the beginner template.
- Expanding v3.8 into Program Console ANSI or Darwin/GDB evaluator work.
---
## 11. Stop conditions
Stop and hide/disable the macOS OGRE wizard if a reliable workflow requires:
- modifying upstream OGRE source;
- maintaining a persistent patchset/fork;
- coupling projects to the source/build tree;
- recreating OGRE's build logic instead of invoking CMake;
- fragile plug-in/resource manipulation that cannot become a reproducible generated-project contract;
- setup complexity still comparable to the manual build after automation.
Failure remains acceptable. v3.8 may automate a working upstream product, but must not make Code::Blocks responsible for maintaining OGRE.
---
## 12. Done when
**PLANNED.** v3.8 is done when a user on supported Apple Silicon macOS can choose the OGRE wizard, explicitly establish the required prerequisites and official SDK, generate an ordinary Code::Blocks C++ project, and reach a textured, animated 3D application through Build and Run without manual dependency/path archaeology.
The intended end state is:
**Homebrew supplies build prerequisites; a reusable Git helper acquires the official release without installing Git; a reusable CMake helper performs the prefix-aware upstream build/install; OGRE remains upstream's engine; generated plug-in/resource files express the runtime SDK contract; and the starter project exposes the interesting 3D C++ code rather than the installation machinery.**

