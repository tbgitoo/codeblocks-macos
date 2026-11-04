# Code::Blocks v3.8.1 - correct OGRE asset conversion on macOS

**Status: DONE - v3.8.1 released; upstream PR #3788 remains open as an external follow-up.**

v3.8.1 follows the released v3.8 OGRE-wizard baseline and corrects the remaining macOS OGRE installation and Assimp referenced-resource resolution problems reproduced from exact OGRE `v14.6.3`.

## Final outcome

Three independent problems were established and corrected:

1. OGRE `v14.6.3` installs Release frameworks below `lib/macosx/Release`, while the installed converter's default install RPATH does not include that directory.
2. Installed `plugins.cfg` defaults to `<prefix>/lib/OGRE`, while the tested macOS framework plug-ins are under `lib/macosx/Release`.
3. Assimp can derive absolute paths for files referenced relatively by a model. `OgreIOSystem` passes those paths into `ResourceGroupManager`, which expects resource names relative to the registered resource location. Nested referenced files additionally require the converter's existing source `FileSystem` resource location to be recursive.

The implemented solution has two layers:

```text
Code::Blocks CMake configuration
    +-- final macOS framework install RPATH
    +-- final macOS OGRE plug-in path

exact-version optional OGRE compatibility patch
    +-- absolute Assimp referenced path -> OGRE resource name
    +-- recursive registration of the existing source resource root
```

The compatibility path remains constrained to a known patch identifier and exact supported source contract.

The work is complete for Code::Blocks v3.8.1. Upstream review and disposition of PR #3788 are external follow-up activities and do not block or qualify the release.

---

## 1. v3.8 architecture retained

The v3.8 architecture remains valid:

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
            +-- configure for final install destination
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

The important technical learning is that the final macOS runtime layout must be supplied during **CMake configuration, before building**.

The effective configure operation is:

```bash
cmake \
    -S "$SOURCE" \
    -B "$BUILD" \
    -DCMAKE_BUILD_TYPE=Release \
    -DCMAKE_INSTALL_PREFIX="$SDK" \
    -DCMAKE_INSTALL_RPATH="$SDK/lib/macosx/Release" \
    -DOGRE_PLUGINS_PATH="lib/macosx/Release"
```

The wizard continues to build the complete default/all target and then installs from that configured build tree.

---

## 2. Exact local compatibility baseline

```text
OGRE tag:    v14.6.3
commit:      e3ee8850ecf5e0038b6d850ec4c865c2039342d5
```

The local patch contract remains pinned to exact `v14.6.3`.

---

## 3. macOS installed-runtime correction - DONE

Validated configure additions:

```text
CMAKE_INSTALL_RPATH=<prefix>/lib/macosx/Release
OGRE_PLUGINS_PATH=lib/macosx/Release
```

Wizard integration was validated end to end. The installed converter:

```text
resolves installed OGRE frameworks
loads Codec_Assimp.framework
initialises Assimp 6.0.0
requires no DYLD_* workaround
requires no post-install plugins.cfg edit
requires no install_name_tool repair
```

The generated Code::Blocks project's own `plugins.cfg` remains a separate configuration and already targets the installed Release framework directory.

---

## 4. Assimp referenced-resource defect - DONE

### 4.1 Diagnosis

Assimp may receive the primary model by absolute name and resolve relatively referenced files into absolute paths. For example:

```text
primary source:
    /absolute/model-directory/model.obj

model reference:
    mtllib materials/model.mtl

Assimp request:
    /absolute/model-directory/materials/model.mtl
```

OGRE resource lookup instead needs:

```text
materials/model.mtl
```

The production correction translates an Assimp request only when it is demonstrably beneath the primary model source directory. Requests outside that directory are left unchanged.

### 4.2 Nested resources

After translation, a nested request can correctly become:

```text
materials/model.mtl
```

The converter therefore registers its existing model source resource location recursively:

```cpp
ResourceGroupManager::getSingleton().addResourceLocation(
    path, "FileSystem", RGN_DEFAULT, true);
```

This broadens visibility only within the already registered source root.

### 4.3 Controlled behavior matrix

```text
pristine
    flat            FAIL
    nested          FAIL

resource-name translation only
    flat            PASS
    nested          FAIL

resource-name translation + recursive resource root
    flat            PASS
    nested          PASS
```

---

## 5. Production compatibility patch - DONE

The production delta was reconstructed from a fresh exact checkout rather than cleaned from an instrumented investigation tree.

Only these OGRE files are modified:

```text
PlugIns/Assimp/src/AssimpLoader.cpp
Tools/AssimpConverter/main.cpp
```

The packaged patch is:

```text
src/plugins/scriptedwizard/resources/patches/
    ogre-14.6.3-assimp-sidecar-path.patch
```

The patch contains the two proven behaviors only:

1. bounded translation of absolute Assimp referenced paths into OGRE resource names relative to the model source directory;
2. recursive registration of the converter's existing source resource root.

---

## 6. Git-backed patch delivery - DONE

The existing constrained `macpatch` mechanism is retained:

```text
Wizard.MacPatchProbe(patchId, sourceRoot)
Wizard.MacPatchApply(patchId, sourceRoot, explanation, confirmLabel)
```

The patch mapping, exact-version guard and wizard explanation are aligned on OGRE `v14.6.3`.

Validated lifecycle behavior includes:

```text
patch declined       -> tracked source remains pristine
patch accepted       -> exact two-file delta applied
patch accepted       -> rebuild and reinstall triggered
already applied      -> no duplicate mutation
repeated invocation  -> idempotent
```

No reconfiguration is needed merely because the source patch was applied after CMake configuration.

---

## 7. Wizard-delivered regression - DONE

The actual intended delivery path was tested rather than only a manual OGRE build.

### Declined patch

```text
runtime bootstrap        PASS
Codec_Assimp loading     PASS
flat referenced MTL      FAIL as expected
material output          fallback
```

### Accepted patch

```text
flat referenced MTL      PASS
nested referenced MTL    PASS
material definitions     retained
texture references       retained in the larger regression fixture
```

The ordinary generated OGRE starter project was also revalidated and remains functional.

---

## 8. Current-upstream independent proof - DONE

A minimal independently authored reproducer was created outside the OGRE source tree:

```text
model.obj
materials/
    model.mtl
```

`model.obj` contains one triangle with UV coordinates and references:

```text
mtllib materials/model.mtl
```

`materials/model.mtl` contains:

```text
newmtl TestMaterial
Kd 1.0 0.0 0.0
```

### 8.1 Pristine current upstream

A clean current-upstream build reproduces the defect independently of Code::Blocks:

```text
OBJ loading             PASS
mesh serialization      PASS
materials/model.mtl     FAIL
fallback material       generated
```

### 8.2 Corrected current upstream

The same two-source-file correction applies cleanly to current upstream.

With the correction:

```text
materials/model.mtl     PASS
material lookup errors  none
```

The generated OGRE material contains:

```text
diffuse 1 0 0 1
```

which directly corresponds to the reproducer's:

```text
Kd 1.0 0.0 0.0
```

This is positive evidence that the referenced MTL was consumed.

### 8.3 Upstream submission

The OGRE correction was separated into two reviewable commits:

```text
Assimp: translate absolute sidecar paths to resource names relative to model file
AssimpConverter: enable referenced files in nested source folders
```

The upstream pull request is open as:

```text
PR #3788
Assimp: resolve referenced files relative to the model source
```

Upstream CI, review and disposition are external follow-up activities. Upstream acceptance is **not a prerequisite for Code::Blocks v3.8.1**, because the exact-version compatibility patch already provides the validated local solution for OGRE `v14.6.3`.

---

## 9. Security/resource boundary

```text
registered source directory
        |
        +-- model.obj                  allowed
        +-- model.mtl                  allowed
        +-- materials/model.mtl        allowed
        +-- textures/diffuse.png       allowed

../outside/...                         not a required supported case
/absolute/unrelated/path               not translated by the fix
```

No unrestricted direct filesystem fallback is introduced.

---

## 10. Milestones

| Milestone | Objective | Status |
|---|---|---|
| M1 | Exact-version clean baseline and controlled build procedure | **DONE** |
| M2.1 | Diagnose installed framework lookup | **DONE** |
| M2.2 | Validate install RPATH correction | **DONE** |
| M2.3 | Diagnose installed plugin lookup | **DONE** |
| M2.4 | Validate plugin-path correction | **DONE** |
| M2.5 | Integrate configure-time runtime-layout corrections | **DONE** |
| M3.1 | Establish native Assimp control | **DONE** |
| M3.2 | Trace OGRE Assimp IO bridge | **DONE** |
| M3.3 | Validate resource-name translation | **DONE** |
| M3.4 | Isolate nested resource visibility | **DONE** |
| M3.5 | Validate recursive existing source root | **DONE** |
| M4.1 | Create fresh exact production source tree | **DONE** |
| M4.2 | Reintroduce minimal two-file behavior | **DONE** |
| M4.3 | Review minimal production delta | **DONE** |
| M4.4 | Generate packaged exact-version patch | **DONE** |
| M5.1 | Retain constrained `macpatch` mechanism | **DONE** |
| M5.2 | Align patch mapping/version/explanation | **DONE** |
| M6.1 | Validate patch-declined wizard control | **DONE** |
| M6.2 | Validate accepted flat regression | **DONE** |
| M6.3 | Validate accepted nested regression | **DONE** |
| M6.4 | Validate rebuild/reinstall after patch | **DONE** |
| M6.5 | Validate already-applied/idempotent path | **DONE** |
| M7.1 | Build minimal synthetic reproducer | **DONE** |
| M7.2 | Validate current upstream pristine vs corrected | **DONE** |
| M7.3 | Submit focused upstream correction | **DONE - PR #3788 OPEN / EXTERNAL** |
| M8.1 | Revalidate ordinary generated starter | **DONE** |
| M8.2 | Final Code::Blocks source/documentation review | **DONE** |
| M8.3 | Release v3.8.1 | **DONE** |

---

## 11. Acceptance criteria - SATISFIED

1. v3.8 remains the architectural baseline.
2. `macpatch` remains constrained and reusable.
3. The local supported OGRE version is exact `v14.6.3`.
4. The final SDK prefix and macOS runtime layout are fixed during CMake configuration.
5. Installed converter RPATH includes the macOS Release framework directory.
6. Installed `plugins.cfg` points to the actual macOS Release plug-in directory.
7. No `DYLD_*` workaround is required.
8. `Codec_Assimp.framework` loads from the installed SDK.
9. The production patch originates from a fresh pristine exact checkout.
10. Only the two intended OGRE source files are modified by the compatibility patch.
11. Resource-name translation is bounded to the primary source directory.
12. No unrestricted filesystem fallback is introduced.
13. The existing source resource root is registered recursively.
14. Patch-declined conversion reproduces the pristine referenced-material failure.
15. Patch-accepted flat conversion passes.
16. Patch-accepted nested conversion passes.
17. Patch application after configuration rebuilds/reinstalls without requiring reconfiguration.
18. Declining preserves pristine tracked OGRE source.
19. Already-applied detection and repeated runs are idempotent.
20. The ordinary generated OGRE starter remains functional.
21. Current-upstream pristine-versus-corrected proof exists independently of Code::Blocks.
22. A focused upstream pull request has been submitted.
23. Final Code::Blocks source diff and user-facing documentation were reviewed as clean and coherent.
24. Code::Blocks v3.8.1 was released with the macOS arm64 DMG and source archives.

All Code::Blocks v3.8.1 acceptance criteria are satisfied. No remaining internal implementation or release action is planned.

---

## 12. Release record

```text
release:       v3.8.1 Ogre patch
status:        released / latest
release tag:   v3.8.1
release commit: b3cb64c
binary asset:  CodeBlocks-macos-arm64.dmg
binary size:   25.7 MB
```

The v3.8.1 release provides the validated configure-time macOS runtime-layout corrections and the optional exact-version OGRE `v14.6.3` compatibility patch through the Code::Blocks wizard delivery path.

---

## 13. Retirement condition

When a pinned official OGRE version incorporates the relevant corrections:

```text
remove the corresponding local patch offer
remove its patch mapping/resource
update the supported version contract
remove configuration workarounds only if upstream behavior makes them unnecessary
retain macpatch infrastructure
retain regression knowledge
```

Upstream PR disposition does not affect the completed status of v3.8.1. The v3.8.1 compatibility patch remains the controlled solution for its pinned OGRE version until an updated official version is deliberately adopted and revalidated.

---

## 14. Final a posteriori path

```text
v3.8 known-good architecture                       DONE
        |
        v
exact v14.6.3 clean baseline                       DONE
        |
        +-- macOS framework RPATH correction       DONE
        +-- installed plugin-path correction       DONE
        |
        v
configure final macOS layout before build          DONE
        |
        v
Assimp path/resource diagnosis                     DONE
        |
        +-- bounded resource-name translation      DONE
        +-- recursive existing source root         DONE
        |
        v
fresh exact production patch                       DONE
        |
        v
Code::Blocks patch packaging/delivery              DONE
        |
        v
wizard declined/accepted/idempotent lifecycle      DONE
        |
        v
ordinary generated starter regression              DONE
        |
        +-- separate upstream track
        |       |
        |       +-- synthetic reproducer           DONE
        |       +-- current upstream A/B            DONE
        |       +-- PR #3788                       OPEN / EXTERNAL
        |
        v
final Code::Blocks diff/documentation review       DONE
        |
        v
v3.8.1 release                                     DONE
```

## Final status

**Code::Blocks v3.8.1 is complete and released.**

The remaining upstream response to PR #3788 is deliberately outside the completed v3.8.1 plan. No further work is required unless upstream review creates a new, separately scoped follow-up.

