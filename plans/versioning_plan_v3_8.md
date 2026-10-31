# Code::Blocks v3.8 - modernise the OGRE application wizard on macOS

**Status: PLANNED / INVESTIGATIVE.** v3.8 investigates whether the existing Code::Blocks OGRE application wizard can become a genuinely beginner-usable starting point on modern Apple Silicon macOS while targeting current upstream OGRE rather than an obsolete compatibility version.

**Question this version answers:** can Code::Blocks hide the otherwise substantial OGRE setup burden well enough that a user can choose New Project -> OGRE application and reach an impressive, editable 3D C++ example without first learning OGRE's dependency and build infrastructure?

The evidence is now strong: pristine upstream OGRE **v14.6.2** builds from its official release tag on the tested Apple Silicon Mac without source patches; the upstream `SampleBrowser.app` runs and renders correctly; OGRE installs to a versioned Code::Blocks-owned SDK prefix; a completely external probe renders a textured, illuminated, slowly rotating `ogrehead.mesh`; the wizard verifies its Homebrew prerequisites, validates Git without installing it, acquires the exact official `v14.6.2` source release, resolves the native source/build/install hierarchy under `~/Library/CodeBlocks`, and now configures the Release build tree with the final install prefix fixed at configure time.

The remaining M3 work is to automate the proven CMake build and install operations using the configured build tree, with explicit consent, diagnostics and cancellation. M4 then generates the modern beginner project.

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
            +-- macbrew helper: build dependencies
            +-- macgit helper: official tagged source checkout
            +-- maccmake helper: paths, configure, build and install
            |
            +-- versioned installed upstream OGRE SDK
```

OGRE remains an external C++ graphics engine. Code::Blocks must not fork OGRE, carry private macOS patches, or reproduce OGRE's build logic.

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

**M2 step 1: DONE.** The OGRE wizard uses:

```text
src/plugins/scriptedwizard/macbrew.cpp
src/plugins/scriptedwizard/macbrew.h
```

to verify and, with explicit consent, install:

```text
cmake
pkgconf
sdl2
```

Code::Blocks does not install Homebrew. macOS no longer asks the obsolete source-tree-versus-SDK question.

### 4.2 macgit helper and source acquisition

**M2 step 2: DONE.** The helper:

```text
src/plugins/scriptedwizard/macgit.cpp
src/plugins/scriptedwizard/macgit.h
```

exposes only:

```text
Wizard.MacGitProbe()
Wizard.MacGitTry(path)
Wizard.MacGitClone(url, tag, destination, explanation, confirmLabel)
```

It revalidates Git with `git --version`, never installs Git, requires explicit consent, constructs argv natively, rejects unsafe clone destinations and supports responsive output/cancellation.

The successful operation is equivalent to:

```bash
git clone --branch v14.6.2 --depth 1 \
    https://github.com/OGRECave/ogre.git \
    "$HOME/Library/CodeBlocks/ogre-source-14.6.2"
```

The resulting checkout was independently verified as a clean detached checkout at exact tag `v14.6.2`.

**M2 status: DONE.**

### 4.3 M3.1 native CMake validation and path resolution

**M3.1: DONE.** The helper:

```text
src/plugins/scriptedwizard/maccmake.cpp
src/plugins/scriptedwizard/maccmake.h
```

currently exposes:

```text
Wizard.MacCMakeProbe()
Wizard.MacCMakeTry(path)
Wizard.MacCMakePaths(sourceLeaf, installLeaf)
Wizard.MacCMakeConfigure(source, build, install, buildType, explanation, confirmLabel)
```

`MacCMakeProbe()` and `MacCMakeTry()` validate CMake through `cmake --version`. On the tested Mac, the native probe resolves and validates:

```text
/opt/homebrew/bin/cmake
cmake version 4.4.5
```

`MacCMakePaths()` accepts constrained leaf names rather than arbitrary script-supplied paths. The final convention uses native `wxGetHomeDir()` in `maccmake.cpp` and constructs the dedicated external-SDK root:

```text
~/Library/CodeBlocks
```

The proven resolved layout is:

```text
root:
~/Library/CodeBlocks

source:
~/Library/CodeBlocks/ogre-source-14.6.2

build:
~/Library/CodeBlocks/ogre-source-14.6.2/build-codeblocks

install:
~/Library/CodeBlocks/ogre-14.6.2
```

Technical lessons from M3.1:

- `ConfigManager::GetUserDataFolder()` is protected and is not a plug-in API.
- `ConfigManager::GetFolder(sdDataUser)` is public but represents Code::Blocks application data, not the intended external-SDK warehouse.
- `wxGetHomeDir()` belongs in native C++, not Squirrel, for this macOS-specific convention.
- `wxFILE_SEP_PATH` should be appended to a `wxString` explicitly; beginning a `+` expression with it causes character/pointer arithmetic rather than string concatenation.
- Compiling a native helper is insufficient by itself: the Squirrel wrapper and `BindMethod(...)` entries must be in the installed Scripted Wizard plug-in, and Code::Blocks must be restarted before the new methods exist at runtime.
- The M2 checkout is naturally rediscovered when M3.1 resolves the intended source path, so acquisition is not repeated.

### 4.4 M3.2 configure

**M3.2: DONE.** `maccmake` now exposes the constrained configure operation:

```text
Wizard.MacCMakeConfigure(source, build, install, buildType, explanation, confirmLabel)
```

The helper validates absolute and distinct source/build/install locations, requires the OGRE `CMakeLists.txt`, accepts only recognised build types, requires explicit consent, constructs argv natively, redirects live output, supports cancellation and invokes no shell command.

The successful operation is equivalent to:

```bash
cmake \
    -S "$HOME/Library/CodeBlocks/ogre-source-14.6.2" \
    -B "$HOME/Library/CodeBlocks/ogre-source-14.6.2/build-codeblocks" \
    -DCMAKE_BUILD_TYPE=Release \
    -DCMAKE_INSTALL_PREFIX="$HOME/Library/CodeBlocks/ogre-14.6.2"
```

Configuration success requires:

```text
process launched
not cancelled
exit code 0
<build>/CMakeCache.txt exists
```

The resulting cache was independently verified to contain:

```text
CMAKE_BUILD_TYPE:STRING=Release
CMAKE_INSTALL_PREFIX:PATH=/Users/thomasbraschler/Library/CodeBlocks/ogre-14.6.2
```

This proves the crucial M1 requirement that the final install prefix is fixed during configuration rather than substituted later during installation. M3.2 does not build or install OGRE.

Technical lessons from M3.2:

- The native configure primitive also required a dedicated wizard page and an `OnLeave_OgreMacConfigure()` call path; implementing the C++ method alone did not execute configuration.
- `CMakeCache.txt` is the practical milestone marker and is checked both by the native result and by the wizard before proceeding.
- Literal comparison of separately normalised directory strings was too brittle. The first safety check rejected the correct build directory as not being `source/build-codeblocks`.
- The corrected guard constructs and normalises both expected and actual build paths with `wxFileName` before comparison.
- The failed path comparison occurred before consent/process launch, confirming that native invariant validation prevents unintended CMake execution.
- The final dialog saying that build/install and project generation are not implemented is the intentional M3.2 stop barrier reached after successful configuration, despite being presented by the wizard as a `Script error`. The UX should be improved later, but the current control-flow boundary is correct.

### 4.5 M3.3 build: next

With configuration now proven, add the constrained equivalent of:

```bash
cmake --build <build> -j<bounded-host-job-count>
```

Parallelism should be determined natively and bounded. The long-running build must retain responsive output and cancellation behaviour analogous to the proven `macgit` operation.

M3.3 success should require more than process launch. It should confirm successful exit and one or more build outputs that establish that the OGRE build completed sufficiently for the subsequent install target.

### 4.6 M3.4 install and installed-SDK validation

Finally add:

```bash
cmake --install <build>
```

Do not pass a replacement `--prefix`; the final prefix was fixed during M3.2 configuration.

After installation, verify the installed contract used by M1 rather than relying solely on process exit status. At minimum the validation should cover the CMake package, installed headers/libraries, Media/resources and the macOS Release renderer plug-in location needed by the proven external probe.

---

## 5. Dependency locations and lifecycle

```text
~/Library/CodeBlocks/
    |
    +-- ogre-source-14.6.2/
    |       |
    |       +-- build-codeblocks/
    |
    +-- ogre-14.6.2/
            |
            +-- CMake/
            +-- Media/
            +-- bin/
            +-- include/
            +-- lib/
```

This dedicated SDK root is separate from Code::Blocks' own `Application Support/.../share/codeblocks` application resources.

Generated projects must depend only on the installed SDK contract, never on the source checkout or build directory.

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
| `src/plugins/scriptedwizard/maccmake.cpp` / `.h` | Proven CMake discovery, native paths and configure; next add build/install with diagnostics and cancellation. |
| `wiz.cpp`, `Makefile.am`, maintained project files | Bind and compile the narrow native helper APIs. |
| OGRE `main.cpp` template | Textured, illuminated and slowly rotating Ogre-head example. |
| Generated `plugins.cfg` | Rebase the installed macOS Release plug-in directory. |
| Generated `resources.cfg` | Derive canonical resource groups and rebase installed Media paths. |

---

## 8. Milestones

| | Objective | Check | Status |
|---|---|---|---|
| M1 | Characterise the legacy wizard and prove the external installed-SDK contract. | Independent external project builds and renders the textured, illuminated, rotating Ogre head using the installed SDK. | **DONE** |
| M2 | Add reusable prerequisite/acquisition helpers. | Homebrew prerequisites are verified/installed; `macgit` validates existing Git without installing it and safely clones exact tag `v14.6.2`. | **DONE** |
| M3 | Add the reusable CMake build/install workflow. | M3.1 native path/CMake resolution and M3.2 prefix-aware Release configuration are proven; M3.3-M3.4 must perform build/install with cancellation, diagnostics and SDK-contract validation. | **IN PROGRESS** |
| M4 | Modernise the OGRE wizard and starter project. | Generate the compact current-API rotating-Ogre project with correct macOS plug-in/resource/runtime configuration. | **PLANNED** |
| M5 | Validate the complete beginner path. | From a clean supported Mac, explicit setup completes and Build and Run displays the 3D example without manual repair. | **PLANNED** |

---

## 9. Acceptance criteria

v3.8 is accepted only if:

1. The workflow targets official OGRE `v14.6.2`, or a deliberately revalidated later release.
2. Homebrew supplies only declared build dependencies.
3. Git is discovered or selected but never installed by Code::Blocks.
4. OGRE is cloned at an explicit release tag using a shallow checkout.
5. Source/build/install locations are resolved natively without a username-specific path literal and use the dedicated `~/Library/CodeBlocks` SDK root.
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
- Exposing generic Git or CMake command APIs to wizard scripts.
- Leaving username-specific filesystem literals in the final workflow.
- Storing source/build/install trees under Code::Blocks' `Application Support/.../share/codeblocks` application-data location.
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

**Homebrew supplies build prerequisites; `macgit` safely acquires the official tagged release without installing Git; `maccmake` resolves the dedicated `~/Library/CodeBlocks` SDK hierarchy and performs the prefix-aware upstream configure/build/install with bounded native operations; OGRE remains upstream's engine; and the generated project exposes the interesting 3D C++ code rather than the installation machinery.**
