# Code::Blocks v3.8.1 - correct OGRE asset conversion on macOS

**Status: IN PROGRESS.** v3.8.1 follows the released v3.8 OGRE-wizard baseline and corrects the remaining macOS OGRE installation and Assimp sidecar-resolution problems reproduced from a clean exact OGRE `v14.6.3` checkout.

**Question this version answers:** how can the Code::Blocks OGRE wizard produce a usable installed `OgreAssimpConverter` on macOS, and what are the smallest safe corrections required for Assimp sidecars located either beside the model or in child directories beneath the model directory?

The current diagnosis is based only on reproducible clean controls. Ambiguous historical installations are not used as evidence.

Three independent problems are established:

1. vanilla OGRE `v14.6.3` installs Release frameworks below `lib/macosx/Release`, but the installed converter receives only `<prefix>/lib` from OGRE's default install RPATH policy;
2. vanilla installed `plugins.cfg` points to `<prefix>/lib/OGRE`, while the macOS framework plug-ins are installed below `lib/macosx/Release`;
3. after the runtime layout is corrected, OGRE's custom Assimp `OgreIOSystem` receives absolute sidecar paths from Assimp and passes them directly to `ResourceGroupManager`, although OGRE resource lookup requires names relative to the registered resource location. After this resource-name translation is corrected, nested sidecars additionally require the converter's source `FileSystem` resource location to be recursive.

The proposed v3.8.1 solution has two layers:

```text
Code::Blocks CMake configuration
    +-- final macOS framework install RPATH
    +-- final macOS OGRE plug-in path

exact-version optional OGRE compatibility patch
    +-- Assimp absolute-sidecar path -> OGRE resource-name translation
    +-- recursive registration of the existing source resource root
```

The Git-backed `macpatch` mechanism remains part of v3.8.1. It remains constrained to compiled-in patch identifiers and exact supported source contracts.

---

## 1. Known-good architectural starting point from v3.8

The v3.8 wizard architecture remains valid:

```text
Homebrew
    |
    +-- cmake
    +-- pkgconf
    +-- sdl2
    +-- assimp
            |
            v
exact official OGRE source
            |
            +-- configure
            +-- build default/all target
            +-- install
            |
            v
~/Library/CodeBlocks/ogre-<version>/
            |
            +-- OGRE SDK
            +-- OgreAssimpConverter
            +-- Codec_Assimp.framework
            |
            v
ordinary generated Code::Blocks OGRE project
```

The native `MacCMakeConfigure` helper currently invokes the equivalent of:

```bash
cmake \
    -S "$SOURCE" \
    -B "$BUILD" \
    -DCMAKE_BUILD_TYPE=Release \
    -DCMAKE_INSTALL_PREFIX="$SDK"
```

The native build helper performs the default/all build:

```bash
cmake --build "$BUILD" --parallel <bounded-job-count>
```

Installation is equivalent to:

```bash
cmake --install "$BUILD"
```

The wizard does **not** set `OGRE_BUILD_DEPENDENCIES=OFF`. The reference build must not introduce that option.

---

## 2. Reference source and publication boundary

The controlled baseline is exact OGRE `v14.6.3`:

```text
commit e3ee8850ecf5e0038b6d850ec4c865c2039342d5
tag    v14.6.3
```

Private regression work remains under:

```text
/Users/thomasbraschler/git/ogre-charizard-test/test_matrix/
```

The third-party model bundle, extracted geometry, MTL files, textures, converted outputs and screenshots are **not part of the Code::Blocks repository, release artefacts or public regression-data distribution**.

Each meaningful result should retain:

```text
OgreAssimp.log
conversion.txt
generated .mesh
generated .material
manifest.txt
```

A future public reproducer must be newly authored and legally redistributable.

---

## 3. Confirmed clean `v14.6.3` build and runtime behaviour

### 3.1 Canonical configure/build/install procedure

The clean manual control that mirrors the wizard and adds the two validated macOS runtime-layout corrections is:

```bash
cmake \
    -S "$SOURCE" \
    -B "$BUILD" \
    -DCMAKE_BUILD_TYPE=Release \
    -DCMAKE_INSTALL_PREFIX="$SDK" \
    -DCMAKE_INSTALL_RPATH="$SDK/lib/macosx/Release" \
    -DOGRE_PLUGINS_PATH="lib/macosx/Release"

cmake --build "$BUILD" --parallel 8
cmake --install "$BUILD"
```

The complete default build is required. `OgreAssimpConverter` must not be treated as an isolated build target.

### 3.2 macOS installed framework RPATH issue

Pristine OGRE installs Release frameworks under:

```text
<prefix>/lib/macosx/Release
```

but vanilla installed tools receive only:

```text
<prefix>/lib
```

from OGRE's install RPATH policy.

Adding at configure time:

```text
CMAKE_INSTALL_RPATH=<prefix>/lib/macosx/Release
```

makes the installed converter resolve directly linked frameworks without post-install mutation or `DYLD_*` overrides.

### 3.3 Installed `plugins.cfg` issue

OGRE's default non-Windows plug-in path produces:

```text
PluginFolder=<prefix>/lib/OGRE
```

while the tested macOS framework plug-ins are installed under:

```text
<prefix>/lib/macosx/Release
```

Configuring with:

```text
OGRE_PLUGINS_PATH=lib/macosx/Release
```

makes installed `plugins.cfg` match the actual framework plug-in directory.

With both configure-time corrections, the installed converter:

```text
starts successfully
resolves directly linked OGRE frameworks
loads Codec_Assimp.framework
initialises Assimp 6.0.0
reports supported formats
```

### 3.4 Wizard-generated project configuration is separate

The Code::Blocks project template's `plugins.cfg` already substitutes the installed macOS Release framework directory. The failing file was OGRE's own installed `bin/plugins.cfg`, used by `OgreAssimpConverter`.

The generated-project `resources.cfg` is independent of the converter bootstrap issue.

---

## 4. Confirmed Assimp sidecar-resolution defect

### 4.1 Native Assimp control

Homebrew's native Assimp command-line tool successfully imports the same private flat OBJ fixture:

```text
6 meshes
6000 faces
17763 vertices
4 materials
4 diffuse texture references
```

The material loss is therefore introduced by the OGRE/Assimp integration path, not by the source asset or native Assimp OBJ importer.

### 4.2 Pristine OGRE flat failure

With corrected runtime configuration but pristine OGRE conversion source, the flat fixture fails MTL resolution while geometry serialisation succeeds:

```text
.mesh       818717 bytes
.material      116 bytes
1 fallback material
0 retained diffuse texture references
```

`.mesh` existence is not sufficient evidence of successful conversion.

### 4.3 Instrumented `OgreIOSystem` result

Temporary diagnostics established that Assimp requests the sidecar using an absolute path:

```text
/absolute/model-directory/pm0006_00_00.mtl
```

Pristine `OgreIOSystem` passes that spelling to `ResourceGroupManager`, although the registered resource location expects:

```text
pm0006_00_00.mtl
```

The primary OBJ succeeds only because `OgreIOSystem` special-cases an exact match with `source->getName()`.

### 4.4 Resource-name translation correction

If an Assimp request is beneath the primary source directory, translating the absolute path into the corresponding resource-relative name changes the flat trace from:

```text
normalized=[/absolute/model-directory/pm0006_00_00.mtl]
opened=[false]
```

to:

```text
normalized=[pm0006_00_00.mtl]
opened=[true]
```

The flat fixture then succeeds.

The instrumented investigation tree is evidence only. It is **not** the source from which the production patch will be cleaned or derived.

### 4.5 Boundary-respecting child-material fixture

```text
child-material/
    pm0006_00_00.obj
    texture files...
    materials/
        pm0006_00_00.mtl
```

The OBJ contains:

```text
mtllib materials/pm0006_00_00.mtl
```

With resource-name translation alone:

```text
normalized=[materials/pm0006_00_00.mtl]
opened=[false]
```

This isolates nested-resource visibility as a separate issue.

### 4.6 Recursive source resource registration

Pristine converter registration:

```cpp
ResourceGroupManager::getSingleton().addResourceLocation(
    path, "FileSystem", RGN_DEFAULT);
```

Correction:

```cpp
ResourceGroupManager::getSingleton().addResourceLocation(
    path, "FileSystem", RGN_DEFAULT, true);
```

With both corrections:

```text
normalized=[materials/pm0006_00_00.mtl]
opened=[true]
```

and the private fingerprint becomes:

```text
.mesh       818713 bytes
.material      640 bytes
4 effective materials
4 diffuse texture references
```

Controlled matrix:

```text
pristine
    flat            FAIL
    child-material  FAIL

resource-name translation only
    flat            PASS
    child-material  FAIL

resource-name translation + recursive=true
    flat            PASS
    child-material  PASS
```

---

## 5. Security and resource boundary

```text
registered source directory
        |
        +-- model.obj                  allowed
        +-- model.mtl                  allowed
        +-- materials/model.mtl        allowed
        +-- textures/diffuse.png       allowed

../outside/...                         not a required supported case
/absolute/unrelated/path               not authorised by the fix
```

Path translation must apply only to requests demonstrably beneath the primary source directory. No unrestricted direct filesystem fallback is introduced.

Recursive registration broadens visibility only within the already-authorised source root.

---

## 6. M1 - establish a reproducible clean baseline

**Status: DONE.**

Established rules:

- preserve exact tag and commit;
- use clean source and build directories;
- build the complete default/all target;
- mirror the wizard's actual CMake options;
- clear exported `DYLD_*` variables;
- separate build-tree diagnostics from installed-SDK acceptance testing;
- do not use ambiguous historical installations as causal evidence.

---

## 7. M2 - make the installed macOS converter runnable

**Status: DONE, WIZARD INTEGRATION REQUIRED.**

Validated configure additions:

```text
CMAKE_INSTALL_RPATH=<prefix>/lib/macosx/Release
OGRE_PLUGINS_PATH=lib/macosx/Release
```

### M2 production task

Update the constrained macOS CMake path so the wizard supplies these values using the final known SDK prefix.

Do not add `OGRE_BUILD_DEPENDENCIES=OFF`, post-install `install_name_tool` surgery, hand-edited `plugins.cfg`, or `DYLD_*` workarounds.

---

## 8. M3 - isolate Assimp sidecar namespace behaviour

**Status: DONE.**

Temporary tracing proved:

- Assimp requests absolute sidecar paths;
- OGRE resource lookup expects names relative to the source resource root;
- bounded resource-name translation fixes the flat fixture;
- nested sidecars require recursive registration of the source root.

The instrumented source tree and its binaries are retained only as investigation evidence.

---

## 9. M4 - reconstruct the clean exact-version OGRE compatibility patch

**Status: NEXT.**

The production patch must **not** be produced by deleting diagnostics from the instrumented working tree. That subtractive approach risks retaining blank-line churn, whitespace changes, altered comments, experimental branches or other spurious artefacts.

The production delta must instead be reconstructed additively from a fresh exact checkout.

### 9.1 Create a fresh exact source checkout

Use a new source directory distinct from the investigation tree, for example:

```text
~/git/ogre-v14.6.3-patch-source
```

Clone or check out exact:

```text
commit e3ee8850ecf5e0038b6d850ec4c865c2039342d5
tag    v14.6.3
```

Before editing, require:

```bash
git status --short
git diff --check
git rev-parse HEAD
git describe --tags --exact-match HEAD
```

Expected:

```text
empty status
empty diff check
e3ee8850ecf5e0038b6d850ec4c865c2039342d5
v14.6.3
```

Do not copy either modified source file from the investigation tree.

### 9.2 Reintroduce only the minimal `AssimpLoader.cpp` behaviour

Manually implement only the bounded resource-name translation established by the experiment.

Requirements:

- map an Assimp request below the primary source directory to its resource-relative name;
- preserve the primary source-stream special case;
- leave unrelated or out-of-root paths unchanged;
- continue using `ResourceGroupManager` for sidecar lookup;
- introduce no direct arbitrary filesystem opening;
- include no tracing, private paths, fixture names or diagnostic comments;
- avoid unrelated reformatting and blank-line changes.

### 9.3 Reintroduce only the minimal `main.cpp` behaviour

Apply only:

```diff
-ResourceGroupManager::getSingleton().addResourceLocation(path, "FileSystem", RGN_DEFAULT);
+ResourceGroupManager::getSingleton().addResourceLocation(path, "FileSystem", RGN_DEFAULT, true);
```

No other converter source change belongs in the production patch.

### 9.4 Review the fresh-tree delta before building

The first production review is the complete Git diff itself:

```bash
git status --short

git diff --check

git diff --stat

git diff -- \
    PlugIns/Assimp/src/AssimpLoader.cpp \
    Tools/AssimpConverter/main.cpp
```

`git status --short` must name only:

```text
PlugIns/Assimp/src/AssimpLoader.cpp
Tools/AssimpConverter/main.cpp
```

The diff must contain only the two intentionally reintroduced behaviours.

### 9.5 Build and test from the fresh production tree

Configure a new build and SDK from the fresh source using the two macOS runtime-layout options:

```bash
cmake \
    -S "$PATCH_SOURCE" \
    -B "$PATCH_BUILD" \
    -DCMAKE_BUILD_TYPE=Release \
    -DCMAKE_INSTALL_PREFIX="$PATCH_SDK" \
    -DCMAKE_INSTALL_RPATH="$PATCH_SDK/lib/macosx/Release" \
    -DOGRE_PLUGINS_PATH="lib/macosx/Release"

cmake --build "$PATCH_BUILD" --parallel 8
cmake --install "$PATCH_BUILD"
```

Re-run flat and child-material against this newly reconstructed build. The investigation-tree binary is not an acceptance artefact.

### 9.6 Generate and validate the patch

Only after the fresh production tree passes both fixtures:

```bash
git diff --check

git diff --binary -- \
    PlugIns/Assimp/src/AssimpLoader.cpp \
    Tools/AssimpConverter/main.cpp \
    > "$PATCH_FILE"
```

Validate against another pristine exact checkout:

```bash
git apply --check "$PATCH_FILE"
git apply "$PATCH_FILE"
git apply --reverse --check "$PATCH_FILE"
```

Patch requirements:

- exact supported OGRE version;
- exactly two target files;
- no trace logging;
- no private references;
- no unrestricted filesystem fallback;
- no unrelated whitespace or blank-line churn;
- authorised root unchanged;
- forward and reverse checks succeed.

### 9.7 Version contract

Controlled evidence currently targets exact `v14.6.3`. Do not label the final artefact as a `v14.6.2` patch unless the final reconstructed two-change delta is separately reproduced and validated against exact `v14.6.2`.

---

## 10. M5 - Git-backed optional patch handler

**Status: DONE; PATCH CONTENT/VERSION UPDATE REQUIRED.**

The `macpatch` architecture remains unchanged.

Implemented API:

```text
Wizard.MacPatchProbe(patchId, sourceRoot)
Wizard.MacPatchApply(
    patchId,
    sourceRoot,
    explanation,
    confirmLabel)
```

The helper recognises only known patch identifiers, validates exact source contracts, invokes Git without a shell, checks forward and reverse applicability, verifies post-apply state, leaves accepted changes visible, and performs no destructive recovery or commits.

Supported states remain:

```text
applicable
already_applied
unsupported_version
git_unavailable
cancelled
apply_failed
applied
```

### 10.1 Wizard explanation update

```text
The OGRE Assimp bridge translates sidecar requests under the selected model
resource root into OGRE resource names, and OgreAssimpConverter registers
that existing source resource root recursively so child sidecars are visible.
```

The page must not describe the defect as recursion alone.

---

## 11. M6 - final local regression

**Status: PLANNED AFTER M4/M5 CONTENT UPDATE.**

Validate:

```text
patch declined / flat
patch accepted / flat
patch declined / child-material
patch accepted / child-material
already-applied state
```

Also validate:

```text
installed converter LC_RPATH includes <prefix>/lib/macosx/Release
installed plugins.cfg points to <prefix>/lib/macosx/Release
Codec_Assimp.framework exists there
converter starts without DYLD_* overrides
Codec_Assimp loads
```

Private fingerprints:

```text
FAIL:
    approximately 819 KB .mesh
    116-byte .material
    fallback material only
    no retained diffuse texture references

PASS:
    approximately 819 KB .mesh
    640-byte .material
    four effective materials
    four retained diffuse texture references
```

---

## 12. M7 - current upstream and public synthetic reproducer

**Status: PLANNED.**

Create newly authored redistributable data:

```text
fixture/
    model.obj
    texture.png
    materials/
        model.mtl
```

Current upstream requires its own pristine-versus-corrected proof. Potential upstream concerns may be separated into:

```text
macOS install-layout configuration
Assimp resource-name mapping
recursive converter resource registration
```

---

## 13. M8 - final regression and retirement

**Status: PLANNED.**

Prove:

```text
patch declined -> source remains pristine
patch accepted -> only known compatibility delta is applied
patch accepted -> flat regression passes
patch accepted -> child-material regression passes
already patched -> no duplicate mutation
normal configure/build/install -> succeeds
installed converter -> launches without environment workaround
installed Codec_Assimp -> loads
beginner Ogre-head starter -> unchanged
```

When the pinned official OGRE version contains the required corrections, retire the corresponding local patch/configuration workaround as applicable while retaining `macpatch` infrastructure and regression documentation.

---

## 14. Expected files / areas

| File / area | v3.8.1 role |
|---|---|
| Fresh exact OGRE source checkout | Sole source for reconstructing the production delta. |
| Instrumented OGRE investigation tree | Evidence only; never cleaned into the production patch. |
| OGRE `PlugIns/Assimp/src/AssimpLoader.cpp` | Minimal bounded resource-name translation. |
| OGRE `Tools/AssimpConverter/main.cpp` | Recursive existing source resource root. |
| OGRE compatibility `.patch` resource | Clean two-file exact-version patch generated from the fresh tree. |
| `src/plugins/scriptedwizard/maccmake.cpp` / `.h` | Add two macOS configure-time runtime-layout values. |
| `src/plugins/scriptedwizard/macpatch.cpp` / `.h` | Existing constrained patch mechanism. |
| OGRE wizard script/page resources | Revise patch purpose, version and explanation. |
| Wizard-generated `plugins.cfg` | Separate project config; no change required for converter bootstrap defect. |
| Wizard-generated `resources.cfg` | No change required for converter sidecar defect. |
| `test_matrix` private fixtures/results | Private evidence only. |
| Future synthetic fixture | Public upstream reproducer. |

---

## 15. Milestones

| Milestone | Objective | Check | Status |
|---|---|---|---|
| M1 | Establish clean exact-version baseline. | Exact `v14.6.3`, complete build, controlled environment. | **DONE** |
| M2.1 | Diagnose installed framework lookup. | Vanilla RPATH mismatch proven. | **DONE** |
| M2.2 | Validate install RPATH correction. | Installed converter resolves direct frameworks. | **DONE** |
| M2.3 | Diagnose installed plugin lookup. | `lib/OGRE` vs `lib/macosx/Release` mismatch proven. | **DONE** |
| M2.4 | Validate plugin-path correction. | `Codec_Assimp` loads from installed SDK. | **DONE** |
| M2.5 | Integrate runtime-layout configure corrections. | Wizard install launches without hand edits or `DYLD_*`. | **NEXT** |
| M3.1 | Establish native Assimp control. | Native Assimp reads flat MTL and four textures. | **DONE** |
| M3.2 | Trace OGRE Assimp IO bridge. | Absolute sidecar/resource-name mismatch proven. | **DONE** |
| M3.3 | Validate resource-name translation. | Flat changes from FAIL to PASS. | **DONE** |
| M3.4 | Isolate nested resource visibility. | Child MTL remains FAIL without recursion. | **DONE** |
| M3.5 | Validate recursive source root. | Child MTL opens and 640-byte material is produced. | **DONE** |
| M4.1 | Create fresh exact production source tree. | Pristine status, exact commit and tag verified. | **NEXT** |
| M4.2 | Reintroduce minimal two-file behaviour. | No copied instrumented files or unrelated edits. | **PLANNED** |
| M4.3 | Review fresh-tree delta. | Only two target files; no whitespace/blank-line churn or tracing. | **PLANNED** |
| M4.4 | Build and regress fresh production tree. | Flat and child-material both pass. | **PLANNED** |
| M4.5 | Generate and validate patch. | `diff --check`, forward check and reverse check succeed. | **PLANNED** |
| M5.1 | Keep `macpatch` implementation. | Known-patch Git mechanism works. | **DONE** |
| M5.2 | Update patch mapping/version/explanation. | Wizard offers correct two-part patch. | **PLANNED** |
| M6 | Validate final wizard lifecycle. | Declined, accepted and already-applied branches behave correctly. | **PLANNED** |
| M7.1 | Build public synthetic fixture. | Original redistributable reproducer exists. | **PLANNED** |
| M7.2 | Validate current upstream. | Pristine versus corrected evidence exists. | **PLANNED** |
| M7.3 | Submit upstream if confirmed. | Focused upstream evidence prepared. | **BLOCKED** |
| M8 | Final regression and release. | Runtime, conversion and starter contracts pass. | **PLANNED** |

---

## 16. Acceptance criteria

v3.8.1 is accepted only if:

1. v3.8 remains the known-good architectural baseline.
2. `macpatch` remains constrained and reusable.
3. The final supported OGRE version is explicit and exact.
4. The installed converter RPATH contains `<prefix>/lib/macosx/Release`.
5. Installed `plugins.cfg` points to the actual macOS Release plug-in directory.
6. No `DYLD_*` workaround is required.
7. `Codec_Assimp.framework` loads from the installed SDK.
8. The production patch is reconstructed from a new pristine exact checkout.
9. Instrumented source files are not copied into the production tree.
10. Only the two intended OGRE source files are modified.
11. The patch contains no tracing, private references or unrelated formatting churn.
12. Resource-name translation applies only beneath the primary source root.
13. No unrestricted filesystem fallback is introduced.
14. The existing source resource root is registered recursively.
15. The authorised root is not broadened.
16. Flat conversion passes after patch acceptance.
17. Child-material conversion passes after patch acceptance.
18. Declining preserves pristine OGRE source.
19. Forward and already-applied detection work.
20. Repeated runs remain idempotent.
21. User assets are not rewritten or renamed.
22. Private fixtures remain outside public artefacts.
23. Squirrel cannot provide arbitrary patch contents, paths or commands.
24. Upstream fixtures are newly authored and redistributable.
25. Current-upstream submission has separate pristine-versus-corrected proof.
26. Local compatibility corrections have explicit retirement conditions.

---

## 17. What stays rejected

- Cleaning the instrumented investigation tree into the production patch.
- Copying modified investigation files into the fresh production checkout.
- Silent OGRE source modification.
- Claiming patched OGRE is pristine.
- A Code::Blocks-owned OGRE fork.
- A bespoke source rewriter.
- Arbitrary patch paths, contents, Git commands or shell commands from Squirrel.
- Destructive Git recovery or wizard-created commits.
- Direct unrestricted filesystem fallback.
- Treating parent escape as a required feature.
- Treating `.mesh` existence as full conversion success.
- Publishing private regression assets or derivatives.
- Ambiguous regression provenance.
- Adding `OGRE_BUILD_DEPENDENCIES=OFF` contrary to the actual wizard workflow.
- Hand-editing installed `plugins.cfg` as the product solution.
- Relying on `DYLD_*` as the product solution.
- Building only `OgreAssimpConverter` instead of the complete tree.
- Reducing the asset defect to recursion alone.
- Removing resource-name translation despite the clean flat evidence.
- Keeping local compatibility changes after the pinned official version includes them.

---

## 18. Stop conditions

Stop if delivery requires a permanent fork, uncontrolled accumulating patchset, destructive recovery, arbitrary patch execution, asset rewriting, unrestricted model-controlled filesystem access, publication of private assets or destabilisation of the v3.8 starter.

The proposed solution remains:

```text
configure-time macOS runtime-layout corrections
+ fresh-tree exact-version two-file OGRE compatibility patch
+ existing constrained macpatch delivery
```

---

## 19. Done when

**IN PROGRESS.** v3.8.1 is complete when:

- the two macOS runtime-layout configure corrections are integrated;
- a fresh exact OGRE checkout is created for production reconstruction;
- only the minimal resource-name translation and recursive registration are reintroduced;
- the fresh-tree diff contains no instrumentation or spurious artefacts;
- the fresh-tree build passes flat and child-material regressions;
- the generated patch passes forward and reverse validation;
- `macpatch` delivers the revised patch safely;
- declined, accepted and already-applied branches are validated;
- the installed converter runs without environment or config hand edits;
- private evidence stays outside released artefacts;
- the v3.8 starter remains unchanged;
- upstream work is separately proven or explicitly deferred.

```text
v3.8 known-good architecture                      DONE
        |
        v
clean exact v14.6.3 reference build               DONE
        |
        +-- installed RPATH defect                 DONE
        +-- installed plugin-path defect           DONE
        |
        v
native Assimp flat control                         PASS
        |
        v
OGRE pristine flat                                 FAIL
        |
        v
instrumented diagnosis                             DONE
        |
        +-- resource-name translation              flat PASS
        +-- recursive source root                  child PASS
        |
        v
freeze investigation tree as evidence              DONE
        |
        v
create fresh exact production checkout             NEXT
        |
        v
reintroduce only two proven behaviours
        |
        v
review minimal fresh-tree diff
        |
        v
build and test fresh production tree
        |
        v
generate clean exact-version patch
        |
        v
integrate CMake runtime-layout corrections
        |
        v
update patch resource/version/text
        |
        v
revalidate macpatch lifecycle
        |
        v
final flat + child regression
        |
        v
v3.8.1 release
        |
        +-- synthetic current-upstream fixture
        |       |
        |       v
        |   pristine vs corrected proof
        |       |
        |       v
        |   upstream PR(s) if confirmed
        |
        v
future official OGRE contains corrections
        |
        v
retire local compatibility pieces as applicable
retain macpatch infrastructure
```

