# Code::Blocks v3.8 - modernise the OGRE application wizard on macOS

**Status: PLANNED / INVESTIGATIVE.** v3.8 investigates whether the existing Code::Blocks OGRE application wizard can become a genuinely beginner-usable starting point on modern Apple Silicon macOS while targeting current upstream OGRE rather than an obsolete compatibility version.

**Question this version answers:** can Code::Blocks hide the otherwise substantial OGRE setup burden well enough that a user can choose New Project -> OGRE application and reach an impressive, editable 3D C++ example without first learning OGRE's dependency and build infrastructure?

The evidence is now strong: pristine upstream OGRE **v14.6.2** builds from its official release tag on the tested Apple Silicon Mac without source patches; the upstream `SampleBrowser.app` runs and renders correctly; OGRE installs to a versioned Code::Blocks-owned SDK prefix; a completely external probe renders a textured, illuminated, slowly rotating `ogrehead.mesh`; and the wizard now verifies its Homebrew prerequisites, validates Git without installing it, and acquires the exact official `v14.6.2` source release through an explicitly approved shallow clone.

The remaining work is to automate the proven CMake configure/build/install procedure and then generate the modern beginner project without making Code::Blocks responsible for maintaining OGRE itself.

---

## 1. Scope and design boundary

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
            +-- macgit helper: official tagged source checkout
            +-- CMake helper: configure, build and install OGRE
            |
            +-- versioned installed upstream OGRE SDK
```

OGRE remains an external C++ graphics engine. Code::Blocks must not fork OGRE, carry private macOS patches, or reproduce OGRE's build logic.

The intended user experience is:

```text
New Project -> OGRE application
        |
        +-- verify prerequisite tools
        +-- offer explicit dependency setup if required
        +-- clone the official versioned OGRE release
        +-- configure, build and install it with CMake
        +-- generate a small ordinary C++ project
        v
Build and Run
        v
textured, illuminated, animated 3D example
```

The wizard should hide installation complexity, not the interesting C++ and 3D programming.

---

## 2. Proven manual build and install recipe

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

A normal upstream build also produced and successfully ran `SampleBrowser.app` with the OpenGL renderer.

**Important M1 finding:** `CMAKE_INSTALL_PREFIX` must be supplied at configure time. Using only `cmake --install build --prefix <different-prefix>` copied an apparently complete SDK but left generated metadata referring to the former build-tree SDK paths.

The installed SDK contract proven during M1 is:

```text
~/Library/CodeBlocks/ogre-14.6.2/
    CMake/
    Media/
    bin/
    include/
    lib/
```

---

## 3. M1 findings and closure

### 3.1 Legacy wizard assumptions

The historical wizard assumes old source/SDK layouts, `OgreMain`, `OIS`, direct `GL` linkage, `gcc*`, and executables placed in OGRE-owned sample directories. The modern macOS path preserves the wizard entry but replaces that model rather than adding Apple Silicon paths to it.

### 3.2 External installed-SDK proof

The installed SDK provides:

```text
~/Library/CodeBlocks/ogre-14.6.2/CMake/OGREConfig.cmake
~/Library/CodeBlocks/ogre-14.6.2/CMake/OGREConfigVersion.cmake
~/Library/CodeBlocks/ogre-14.6.2/CMake/OgreTargets.cmake
```

The external probe under `~/git/ogre-m1-external` configures and links against this installation without source/build-tree references in its CMake cache or linker command.

### 3.3 Runtime plug-ins and resources

The generic installed plug-in metadata points to `<SDK>/lib/OGRE`, while the macOS Release frameworks are actually installed under:

```text
<SDK>/lib/macosx/Release
```

A project-local `plugins.cfg` rebased to that directory successfully loads the installed renderer and scene-manager frameworks.

A project-local `resources.cfg` derived from `<SDK>/bin/resources.cfg`, with `../Media` rebased to `<SDK>/Media` and the unnecessary `[Tests]` section removed, successfully resolves the installed model, materials, textures and resource-group ordering.

The M4 technical baseline is frozen as:

```text
~/git/ogre-m1-external/
    CMakeLists.txt
    main.cpp
    plugins.cfg
    resources.cfg
```

The probe renders the coloured, textured and illuminated `Media/models/ogrehead.mesh` and rotates it slowly around the vertical axis with a frame-rate-independent `FrameListener`.

A residual path in `<SDK>/lib/cmake/bullet/BulletConfig.cmake` refers to `build-codeblocks/Dependencies`, but the proven OGRE/Bites path does not use it. Code::Blocks will not patch this non-operative upstream metadata.

**M1 status: DONE.**

---

## 4. Reusable macOS helper architecture

### 4.1 Homebrew prerequisites

**M2 step 1: DONE.** The OGRE wizard now has OGRE-specific prerequisite UI based on:

```text
src/plugins/scriptedwizard/macbrew.cpp
src/plugins/scriptedwizard/macbrew.h
```

It verifies and, with explicit consent, installs:

```text
cmake
pkgconf
sdl2
```

Code::Blocks does not install Homebrew. macOS no longer asks the obsolete source-tree-versus-SDK question.

### 4.2 macgit helper and source acquisition

**M2 step 2: DONE.** The new helper is:

```text
src/plugins/scriptedwizard/macgit.cpp
src/plugins/scriptedwizard/macgit.h
```

It is integrated through `wiz.cpp`, Squirrel wrappers, `BindMethod(...)`, and `Makefile.am` and exposes only:

```text
Wizard.MacGitProbe()
Wizard.MacGitTry(path)
Wizard.MacGitClone(url, tag, destination, explanation, confirmLabel)
```

There is no generic Git command API.

The helper:

- revalidates Git with `git --version`;
- treats a saved Git path only as a hint;
- permits selection and validation of an existing Git executable;
- never installs Git, including through Homebrew;
- asks for explicit consent before acquisition;
- constructs the clone argv natively rather than accepting shell syntax;
- rejects invalid/control-character inputs;
- requires a safe absolute destination;
- refuses `/`, existing files and non-empty destinations;
- captures output, cancellation and exit status;
- sets `GIT_TERMINAL_PROMPT=0` for the clone process.

The successful operation is equivalent to:

```bash
git clone --branch v14.6.2 --depth 1 \
    https://github.com/OGRECave/ogre.git \
    "$HOME/Library/CodeBlocks/ogre-source-14.6.2"
```

The resulting checkout was independently verified:

```text
not currently on any branch
working tree clean
exact tag: v14.6.2
```

Detached HEAD is the intended state for this immutable SDK-source checkout.

### 4.3 M2 lessons and open path issue

The Git UI, native process launch, output capture, cancellation and safe tagged clone all work end to end. The implementation also proved that a long-running clone can remain responsive even under unstable network conditions.

One path issue remains open for M3:

```text
/Users/thomasbraschler/Library/CodeBlocks/ogre-source-14.6.2
```

was temporarily hard-coded in `wizard.script` because `wxGetHomeDir()` is not exposed to the Squirrel environment. That path is acceptable only as the M2 test wiring. It must not remain user-specific.

M3 must establish a native, reusable and user-independent way to resolve the Code::Blocks-owned root directory and derive all three locations consistently:

```text
source:  <CodeBlocks user root>/ogre-source-14.6.2
build:   <source>/build-codeblocks
install: <CodeBlocks user root>/ogre-14.6.2
```

Possible implementation forms include returning a native resolved root/path in the CMake helper report or adding a narrow general Scripted Wizard path helper. M3 should choose the smallest reusable capability and must not add another username-specific script literal.

**M2 status: DONE.** M2 stops after safe exact-tag source acquisition. It does not configure, build or install OGRE.

### 4.4 M3 CMake helper: next

Add a narrow reusable macOS CMake helper, expected to use platform-specific names analogous to `macbrew` and `macgit`, and automate the proven sequence:

```bash
cmake -S <source> -B <build> \
    -DCMAKE_BUILD_TYPE=Release \
    -DCMAKE_INSTALL_PREFIX=<install>

cmake --build <build> -j<bounded-host-job-count>

cmake --install <build>
```

The helper must expose progress/output, cancellation and clear configure/build/install results while keeping source, build and install paths distinct. It must set the install prefix during configuration, not override it only during installation.

---

## 5. Dependency locations and lifecycle

The now-proven acquisition/install model is:

```text
Code::Blocks-owned versioned source checkout
        |
        +-- versioned/disposable build directory
        v
~/Library/CodeBlocks/ogre-14.6.2
        v
Code::Blocks OGRE projects
```

The exact native resolution of the Code::Blocks user root is an M3 requirement. Generated projects must depend only on the installed SDK contract, never on the source checkout or build directory.

No hand-built OGRE files should be installed into Homebrew-owned prefixes such as `/opt/homebrew`.

---

## 6. Generated beginner project

The M1 rotating Ogre-head probe remains the leading M4 technical baseline:

```text
ordinary C++ source
    + OGRE initialisation
    + one camera
    + ambient and point lighting
    + textured ogrehead.mesh
    + slow vertical-axis rotation
```

Setup concerns remain outside beginner-facing C++ in project-local `plugins.cfg` and `resources.cfg`.

The generated application remains an ordinary Code::Blocks C++ project with normal Debug/Release targets. The user's project should not become an OGRE source-build, CMake wrapper or Xcode project unless a hard technical requirement is demonstrated.

---

## 7. Expected files / areas

| File / area | Expected v3.8 role |
|---|---|
| Existing OGRE Scripted Wizard resources | Modernise registration, platform workflow and starter generation. |
| `src/plugins/scriptedwizard/macbrew.cpp` / `.h` | Proven prerequisite discovery and explicit formula installation. |
| `src/plugins/scriptedwizard/macgit.cpp` / `.h` | Proven Git discovery/validation and safe shallow tagged clone. |
| New macOS CMake helper | Resolve reusable source/build/install locations and perform configure/build/install. |
| `wiz.cpp`, `Makefile.am`, maintained project files | Bind and compile the narrow native helper APIs. |
| OGRE `main.cpp` template | Textured, illuminated and slowly rotating Ogre-head example. |
| Generated `plugins.cfg` | Rebase the installed macOS Release plug-in directory. |
| Generated `resources.cfg` | Derive canonical resource groups and rebase installed Media paths. |

---

## 8. Milestones

| | Objective | Check | Status |
|---|---|---|---|
| M1 | Characterise the legacy wizard and prove the external installed-SDK contract. | Independent external project builds and renders the textured, illuminated, rotating Ogre head using the installed SDK. | **DONE** |
| M2 | Add reusable prerequisite/acquisition helpers. | Homebrew prerequisites are verified/installed; `macgit` validates existing Git without installing it and safely clones the official exact `v14.6.2` tag into a Code::Blocks-owned test location. | **DONE** |
| M3 | Add the reusable CMake build/install workflow. | Resolve non-user-specific Code::Blocks-owned source/build/install paths; reproduce the proven prefix-aware Release configure/build/install sequence with cancellation and diagnostics. | **PLANNED** |
| M4 | Modernise the OGRE wizard and starter project. | Generate the compact current-API rotating-Ogre project with correct macOS plug-in/resource/runtime configuration. | **PLANNED** |
| M5 | Validate the complete beginner path. | From a clean supported Mac, explicit setup completes and Build and Run displays the 3D example without manual repair. | **PLANNED** |

---

## 9. Acceptance criteria

v3.8 is accepted only if:

1. The workflow targets official OGRE `v14.6.2`, or a deliberately revalidated later release.
2. Homebrew supplies only declared build dependencies.
3. Git is discovered or selected but never installed by Code::Blocks.
4. OGRE is cloned at an explicit release tag using a shallow checkout.
5. Source/build/install locations are resolved without a username-specific path literal.
6. CMake receives the final SDK prefix before building and installs through the supported target.
7. OGRE source remains pristine; no Code::Blocks patchset or fork is required.
8. Generated projects consume the installed SDK without requiring source/build checkout access.
9. Plug-ins and Media resources use reproducible generated-project runtime configuration.
10. A fresh project builds, links and runs without manual path repair.
11. The starter renders a textured animated 3D scene with approachable C++ code.
12. Existing Windows/Linux behaviour is not gratuitously broken.

---

## 10. What stays rejected

- Maintaining an OGRE fork or private macOS patches.
- Patching installed upstream metadata merely to remove non-operative paths.
- Installing Git automatically.
- Exposing a generic Git command API to wizard scripts.
- Leaving Thomas-specific or any username-specific filesystem literals in the final workflow.
- Installing hand-built OGRE into Homebrew-owned prefixes.
- Making generated projects depend on source/build directories.
- Silently cloning/building without explicit user action.
- Embedding opaque clone/CMake shell chains in `wizard.script`.
- Hard-coding resource plumbing in beginner-facing `main.cpp`.
- Reusing the full SampleBrowser as the beginner template.

---

## 11. Stop conditions

Stop and hide/disable the macOS OGRE wizard if a reliable workflow requires modifying upstream OGRE, maintaining a patchset/fork, coupling generated projects to the source/build tree, recreating OGRE's build logic, or fragile runtime manipulation that cannot become a reproducible generated-project contract.

---

## 12. Done when

**PLANNED.** v3.8 is done when a user on supported Apple Silicon macOS can choose the OGRE wizard, explicitly establish the prerequisites and official SDK, generate an ordinary Code::Blocks C++ project, and reach a textured, animated 3D application through Build and Run without manual dependency/path archaeology.

The intended end state is:

**Homebrew supplies build prerequisites; `macgit` safely acquires the official tagged release without installing Git; a reusable native CMake/path helper performs the prefix-aware upstream build/install into user-independent Code::Blocks-owned locations; OGRE remains upstream's engine; and the generated project exposes the interesting 3D C++ code rather than the installation machinery.**

