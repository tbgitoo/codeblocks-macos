# Code::Blocks v3.8.1 - correct OGRE asset conversion on macOS

**Status: IN PROGRESS.** v3.8.1 follows the released v3.8 OGRE-wizard baseline and corrects the remaining asset-conversion problem exposed after Assimp was added as a declared Homebrew prerequisite.

**Question this version answers:** why can `OgreAssimpConverter` successfully serialise OBJ geometry while failing to preserve referenced materials, what is the smallest safe correction for the pinned OGRE `v14.6.2` workflow, and how should that correction be validated and eventually proposed upstream?

The investigation is now substantially simpler than the original diagnosis. The actual compatibility defect is in `OgreAssimpConverter`: it registers the directory containing the input model as a non-recursive OGRE `FileSystem` resource location. Assimp sidecars are then resolved through OGRE's resource system, but qualified resources beneath that directory are not exposed recursively. Changing that single registration to `recursive=true` makes the converter treat the already-authorised model directory as a recursive resource root and restores material import for the tested boundary-respecting layout.

The earlier `PlugIns/Assimp/src/AssimpLoader.cpp` absolute-path translation was a plausible and experimentally useful intermediate correction, but it is no longer part of the intended v3.8.1 production patch. It fixed one observed `v14.6.2` flat manifestation while the investigation was still incomplete. The final compatibility patch is now narrowed to `Tools/AssimpConverter/main.cpp`; `AssimpLoader.cpp` stays pristine.

The Git-backed `macpatch` mechanism remains fully justified and remains part of v3.8.1. Only the known OGRE patch resource and its explanatory text need to be replaced with the corrected one-file patch.

v3.8 remains the known-good released baseline. The v3.8.1 implementation order is now:

1. preserve the retained regression evidence and corrected root-cause record;
2. keep the completed optional `macpatch` infrastructure unchanged;
3. replace the provisional `AssimpLoader.cpp` patch with a one-line `OgreAssimpConverter` recursive-resource patch;
4. regenerate the clean exact-`v14.6.2` Git patch from pristine source;
5. update the installed Scripted Wizard patch resource and user-facing explanation;
6. validate patched and declined wizard branches with flat and boundary-respecting child-material fixtures;
7. create a synthetic public reproducer and test the same correction against current master before upstream submission;
8. retire the local OGRE compatibility patch when the pinned official OGRE release contains the correction, while retaining `macpatch` as reusable infrastructure.

---

## 1. Known-good starting point from v3.8

The released v3.8 workflow establishes:

```text
Homebrew
    |
    +-- cmake
    +-- pkgconf
    +-- sdl2
    +-- assimp
            |
            v
official OGRE v14.6.2 source
            |
            +-- configure
            +-- build
            +-- install
            |
            v
~/Library/CodeBlocks/ogre-14.6.2/
            |
            +-- working OGRE SDK
            +-- OgreAssimpConverter
            +-- Codec_Assimp.framework
            |
            v
ordinary generated Code::Blocks OGRE project
```

The general v3.8 architecture remains valid. The wizard owns prerequisite establishment, exact source acquisition, configure/build/install orchestration and project generation.

The remaining issue is narrow: `OgreAssimpConverter` establishes the model directory as an OGRE filesystem resource location, but does so non-recursively. The v3.8.1 compatibility patch changes only that resource-registration policy.

No diagnostic instrumentation is part of the production patch.

---

## 2. Regression asset and publication boundary

The failure was investigated locally with a third-party model bundle containing:

```text
full-resolution .dae input
full-resolution .obj input
matching .mtl sidecar
multiple PNG textures
additional lower-detail model variants
```

The third-party bundle, its extracted files, textures, converted outputs and screenshots are **not part of the Code::Blocks repository, release artefacts or public regression-data distribution**.

Private regression work is retained under:

```text
/Users/thomasbraschler/git/ogre-charizard-test/test_matrix/
```

Each meaningful test result is stored in a state-specific evidence directory together with:

```text
OgreAssimp.log
conversion.txt
generated .mesh
generated .material
manifest.txt
```

The manifest records the producing OGRE version/revision, patch state, Assimp version, converter path, invocation, fixture layout and result. This replaces ambiguous historical `before` / `after` output naming.

A future public reproducer must be newly authored and legally redistributable. It must contain no geometry, textures, screenshots, filenames or other derivatives from the private third-party bundle.

---

## 3. Confirmed end-to-end behaviour

### 3.1 OGRE converter installation

A fresh `v14.6.2` build performed after Assimp installation produces:

```text
~/Library/CodeBlocks/ogre-14.6.2/bin/macosx/OgreAssimpConverter
~/Library/CodeBlocks/ogre-14.6.2/lib/macosx/Release/Codec_Assimp.framework
```

The converter requires its normal macOS runtime configuration and Assimp codec plug-in.

### 3.2 COLLADA control

The locally tested COLLADA input was rejected by Assimp because it contained an invalid reference form. This remains a source-asset failure and is not evidence for the OBJ/MTL resource-registration defect.

### 3.3 Pristine `v14.6.2` material-loss behaviour

A fresh wizard-created exact `v14.6.2` checkout can successfully import and serialise the OBJ geometry while failing to preserve the MTL-defined materials.

Observed geometry output remains substantial:

```text
6 meshes
6000 faces
17763 post-processed vertices
successful OGRE mesh serialisation
```

The material failure has the private regression fingerprint:

```text
.mesh      approximately 819 KB
.material  116 bytes
one generated fallback material
no retained diffuse texture references
```

Typical log evidence includes failed MTL lookup followed by fallback-material creation.

`.mesh` existence is therefore not a sufficient conversion success criterion.

### 3.4 Successful material conversion fingerprint

A successful conversion of the same private model produces:

```text
.mesh      approximately 819 KB
.material  640 bytes
four effective materials
four retained diffuse texture references
```

The successful material script contains the expected effective materials and texture references. A completely external OGRE consumer previously rendered a correctly converted model; the initial dark-blue-window episode in that rendering proof was a camera-framing issue, not a resource-loading defect.

### 3.5 Historical `AssimpLoader.cpp` detour

An intermediate patch to `PlugIns/Assimp/src/AssimpLoader.cpp` translated absolute Assimp requests beneath the source directory into resource-relative OGRE names. That patch changed one tested flat `v14.6.2` case from failure to success and was therefore a legitimate diagnostic step.

Subsequent matrix testing showed that the patch was not a general resource-layout solution, did not make child resources visible, and is not required in the final v3.8.1 compatibility patch.

The historical `AssimpLoader.cpp` patch is retained only as investigation history. Production `AssimpLoader.cpp` returns to pristine upstream state.

### 3.6 Boundary-respecting child-material fixture

A security-relevant fixture isolates resource recursion without allowing resource-root escape:

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

The MTL sits below the exact directory registered by `OgreAssimpConverter`. There is no `..`, no sibling traversal and no model-controlled absolute path outside the registered root.

Pristine current master and current master with the historical `AssimpLoader.cpp` patch both fail this fixture, demonstrating that path translation in `OgreIOSystem` does not address child-resource visibility.

### 3.7 Actual production correction

`OgreAssimpConverter` currently registers the source directory as:

```cpp
ResourceGroupManager::getSingleton().addResourceLocation(
    path, "FileSystem", RGN_DEFAULT);
```

The v3.8.1 correction is:

```cpp
ResourceGroupManager::getSingleton().addResourceLocation(
    path, "FileSystem", RGN_DEFAULT, true);
```

Only `Tools/AssimpConverter/main.cpp` is changed.

Testing exact pristine `v14.6.2` with this converter change and an untouched `AssimpLoader.cpp` makes the boundary-respecting child-material fixture succeed with the full 640-byte / four-material / four-texture fingerprint.

The intended final local patch therefore consists solely of recursive registration of the already-authorised source resource directory.

---

## 4. Confirmed technical cause

The root cause is the resource-location policy established by `OgreAssimpConverter`.

The converter determines the directory containing the input model and registers it as an OGRE `FileSystem` resource location. In pristine `v14.6.2`, that registration is non-recursive:

```text
model directory
        |
        +-- model.obj
        +-- model.mtl
        +-- materials/
        |       +-- model.mtl
        +-- textures/
                +-- diffuse.png
```

A non-recursive registration does not provide the converter with the resource topology required for qualified child resources such as:

```text
materials/model.mtl
```

The production correction makes the same already-authorised source directory recursive. It does not add a new filesystem root and does not implement direct unrestricted filesystem fallback.

### 4.1 Security boundary

The compatibility contract is:

```text
registered source directory
        |
        +-- model.obj                  allowed
        +-- model.mtl                  allowed
        +-- materials/model.mtl        allowed
        +-- textures/diffuse.png       allowed

../outside/...                         not a required supported case
/absolute/unrelated/path               not authorised by this patch
```

The model may identify resources within the filesystem resource root established by the converter. The model does not gain authority to broaden that root.

### 4.2 Why the old `AssimpLoader.cpp` patch is removed

The old patch acted on one path spelling presented to `OgreIOSystem`; it did not address how the converter exposed the underlying resource hierarchy. Once the investigation isolated resource registration directly, the smaller and more architectural correction was to register the intended source resource root recursively.

The final patch therefore leaves `PlugIns/Assimp/src/AssimpLoader.cpp` untouched.

---

## 5. M1 - reproduce and instrument the material-loss failure

**Status: DONE.**

Temporary diagnostics in `Tools/AssimpConverter/main.cpp` and `PlugIns/Assimp/src/AssimpLoader.cpp` established that geometry conversion could succeed while MTL resolution failed.

Technical lessons:

- successful primary-model opening does not prove sidecar-resource visibility;
- Assimp can try multiple path forms and fallbacks;
- an individual failed path probe is not sufficient evidence of the final import result;
- `.mesh` serialisation can succeed while material import has failed;
- generated `.material` content, texture references and retained logs are necessary regression evidence;
- historical outputs require exact provenance before causal conclusions are drawn.

---

## 6. M2 - isolate resource-layout behaviour

**Status: DONE.**

Testing expanded from the original flat bundle into controlled layouts.

The retained four-state matrix established:

| OGRE state | Flat | Deep (`../materials/...`) |
|---|---|---|
| exact `v14.6.2`, pristine | FAIL | FAIL |
| exact `v14.6.2`, historical `AssimpLoader.cpp` patch | PASS | FAIL |
| current master, pristine | PASS | FAIL |
| current master, historical `AssimpLoader.cpp` patch | PASS | FAIL |

This proved that the intermediate patch had a real but narrow effect and that the deep sibling case represented a different boundary.

A later boundary-respecting child fixture removed the `../` ambiguity and demonstrated a failure entirely inside the converter's registered source root.

---

## 7. M3 - identify the actual root cause and smallest correction

**Status: DONE.**

The decisive experiment changed one converter argument:

```cpp
addResourceLocation(path, "FileSystem", RGN_DEFAULT)
```

to:

```cpp
addResourceLocation(path, "FileSystem", RGN_DEFAULT, true)
```

using exact pristine `v14.6.2` with `AssimpLoader.cpp` untouched.

The boundary-respecting child-material fixture changed from the fallback-material state to:

```text
818713-byte .mesh
640-byte .material
four effective materials
four diffuse texture references
```

This establishes the intended production fix:

```text
Tools/AssimpConverter/main.cpp
    make the existing source FileSystem resource location recursive
```

The historical `AssimpLoader.cpp` patch is superseded and must not be included in the final compatibility artefact.

Technical lessons:

- fix the resource namespace at the point where the converter establishes it;
- do not broaden model-controlled filesystem access;
- do not retain an intermediate compatibility workaround once the smaller root-cause correction is known;
- a one-line architectural correction is preferable to maintaining path-rewriting logic in the Assimp I/O adapter.

---

## 8. M4 - regenerate the clean `v14.6.2` compatibility patch

**Status: NEXT.**

The existing patch artefact:

```text
ogre-14.6.2-assimp-sidecar-path.patch
```

contains obsolete investigation-era `AssimpLoader.cpp` changes and must be replaced.

The new compatibility patch must be generated from pristine exact OGRE `v14.6.2` and modify only:

```text
Tools/AssimpConverter/main.cpp
```

with the behavioural change:

```diff
-ResourceGroupManager::getSingleton().addResourceLocation(path, "FileSystem", RGN_DEFAULT);
+ResourceGroupManager::getSingleton().addResourceLocation(path, "FileSystem", RGN_DEFAULT, true);
```

### 8.1 Recreate pristine source and final delta

**Status: NEXT.**

Use the normal exact shallow `v14.6.2` wizard checkout or an equivalent pristine exact-tag checkout.

Apply only the recursive-registration change. `PlugIns/Assimp/src/AssimpLoader.cpp` must remain byte-for-byte upstream.

### 8.2 Clean patch requirements

The regenerated patch must:

- target exact OGRE `v14.6.2`;
- modify only `Tools/AssimpConverter/main.cpp`;
- contain no diagnostic logging;
- contain no private asset references;
- contain no `AssimpLoader.cpp` changes;
- contain no arbitrary filesystem opening logic;
- leave the registered root unchanged and only make it recursive;
- pass `git diff --check`;
- pass `git apply --check` against pristine exact `v14.6.2`;
- pass `git apply --reverse --check` after application.

### 8.3 Patch identity and storage

The existing known patch ID and resource mapping may remain stable if convenient, but the installed patch contents must be replaced.

The development patch artefact remains outside:

```text
~/Library/CodeBlocks/ogre-source-14.6.2
~/Library/CodeBlocks/ogre-source-14.6.2/build-codeblocks
~/Library/CodeBlocks/ogre-14.6.2
```

so disposable wizard-owned trees can be recreated without losing the patch source.

---

## 9. M5 - Git-backed optional patch handler

**Status: DONE.**

M5 remains in place unchanged architecturally. The final OGRE patch contents change; the native patch-delivery mechanism does not.

Git remains the patch engine. Code::Blocks does not implement a bespoke source rewriter.

Implemented API:

```text
Wizard.MacPatchProbe(patchId, sourceRoot)
Wizard.MacPatchApply(
    patchId,
    sourceRoot,
    explanation,
    confirmLabel)
```

The native helper:

1. recognises only compiled-in patch identifiers;
2. resolves the patch internally as an installed Scripted Wizard resource;
3. validates the wizard-owned source root and exact supported OGRE contract;
4. reuses validated Git discovery;
5. executes Git through argument vectors without a shell;
6. uses `git apply --check` for applicability;
7. uses `git apply --reverse --check` for already-applied detection;
8. verifies post-apply state;
9. reports structured states;
10. leaves accepted changes visible as uncommitted working-tree modifications;
11. never runs destructive recovery commands or creates commits.

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

### 9.1 Native implementation

**Status: DONE.**

```text
src/plugins/scriptedwizard/macpatch.h
src/plugins/scriptedwizard/macpatch.cpp
```

remain the constrained native implementation.

### 9.2 Squirrel binding

**Status: DONE.**

The binding exposes only known-patch operations. Squirrel cannot provide arbitrary patch contents, paths or commands.

### 9.3 OGRE wizard integration

**Status: DONE, CONTENT UPDATE REQUIRED.**

The optional patch stage remains after exact source acquisition and before CMake configuration.

The page must be updated to describe the actual correction:

```text
OgreAssimpConverter registers the source resource location recursively
so sidecars in child directories beneath the model directory are visible.
```

It must no longer describe an `AssimpLoader.cpp` absolute-path correction.

### 9.4 Lifecycle validation

**Status: PATCH MECHANISM DONE; FINAL PATCH REVALIDATION REQUIRED.**

Already proven for `macpatch`:

```text
applicable
user declines
already applied
apply succeeds
post-apply reverse check succeeds
normal configure/build/install proceeds
```

After replacing the known patch resource, repeat these state checks against the new one-line patch.

### 9.5 Fixed design decisions

- Retain `macpatch` as reusable infrastructure.
- Use `git apply`, not custom textual rewriting.
- Keep the known patch exact-version and human-readable.
- Keep source modification visible and optional.
- Keep patch consent separate from clone/configure/build consent.
- Do not permit arbitrary patches from Squirrel.
- Treat the wizard-owned source checkout as disposable.

---

## 10. M6 - retained evidence, final local regression and upstream disposition

**Status: IN PROGRESS.**

M6 now has two distinct purposes: preserve the investigation evidence and validate the corrected one-file local compatibility patch before deciding separately what to submit upstream.

### 10.1 Original matrix retained as investigation evidence

**Status: DONE.**

The flat/deep four-state matrix is retained exactly because it documents why the intermediate `AssimpLoader.cpp` theory was insufficient.

The deep `../materials/...` fixture is exploratory only. Its failure is not an acceptance failure because the reference leaves the converter's registered source root.

### 10.2 Boundary-respecting child-material evidence

**Status: DONE.**

Current master pristine and current master with the historical `AssimpLoader.cpp` patch both fail the child-material fixture.

Exact pristine `v14.6.2` with only recursive converter registration succeeds.

This is the decisive evidence for replacing the old patch with the converter-only patch.

### 10.3 Final `v14.6.2` patch regression

**Status: PLANNED AFTER M4.**

Using a fresh exact `v14.6.2` wizard lifecycle, validate at minimum:

```text
patch declined / flat
patch accepted / flat
patch declined / child-material
patch accepted / child-material
already-applied state
```

Acceptance requires the converter-only patch to produce the successful material/texture outcome for the legitimate supported layouts while the declined path remains pristine upstream.

The private expected fingerprints remain:

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

These byte sizes remain private-fixture fingerprints, not generic OGRE thresholds.

### 10.4 Current master

**Status: CURRENT BUG REPRODUCED; FIX VALIDATION PLANNED.**

Current pristine master fails the boundary-respecting child-material fixture. Because the local correction is now understood as converter resource registration rather than an `AssimpLoader.cpp` workaround, the same one-line `recursive=true` change is the natural current-master candidate to validate separately.

Do not infer upstream suitability solely from the `v14.6.2` result. Current master must receive its own pristine-versus-corrected evidence.

### 10.5 Public synthetic fixture

**Status: PLANNED.**

Create a minimal newly authored fixture for upstream work using only original test data. A likely structure is:

```text
fixture/
    model.obj
    texture.png
    materials/
        model.mtl
```

The OBJ must reference only a child resource beneath the registered fixture directory. No `..` traversal is required for the public bug demonstration.

The fixture must:

- fail on pristine current master;
- pass with the proposed current-master correction;
- contain no private third-party derivatives;
- be small enough to inspect manually;
- be legally redistributable.

### 10.6 Upstream submission

**Status: BLOCKED ON CURRENT-MASTER CORRECTED CONTROL.**

If the one-line recursive registration change also changes the synthetic/current-master reproducer from failure to success, prepare a focused upstream commit against the correct current branch.

Expected upstream change:

```text
Tools/AssimpConverter/main.cpp
    add recursive=true to the source FileSystem resource location
```

Expected evidence:

- current master pristine failure;
- identical current master plus one-line change success;
- synthetic boundary-respecting fixture;
- explanation that the authorised resource root is unchanged;
- no Code::Blocks-specific paths or private regression data.

---

## 11. M7 - final regression and retirement

**Status: PLANNED.**

The final v3.8.1 regression must prove:

```text
patch declined -> source and normal Ogre-head workflow remain pristine
patch accepted -> converter-only change is applied
patch accepted -> flat regression passes
patch accepted -> child-material regression passes
already patched -> no duplicate mutation
normal configure/build/install -> succeeds
beginner Ogre-head starter -> unchanged
```

Retirement condition:

```text
when the pinned official OGRE version contains the recursive converter correction:
    remove this OGRE patch offer
    remove its patch mapping/resource
    update the supported version contract
    retain macpatch infrastructure
    retain historical regression documentation
```

---

## 12. Expected files / areas

| File / area | v3.8.1 role |
|---|---|
| OGRE `Tools/AssimpConverter/main.cpp` | Sole final OGRE compatibility patch target. |
| OGRE `PlugIns/Assimp/src/AssimpLoader.cpp` | Remains pristine; historical diagnostic patch is retired. |
| `ogre-14.6.2-assimp-sidecar-path.patch` | Replace contents with clean converter-only patch, or rename if clearer. |
| Scripted Wizard patch resource directory | Install the revised known patch. |
| `src/plugins/scriptedwizard/macpatch.cpp` / `.h` | Completed reusable constrained patch mechanism. |
| `wiz.cpp`, `Makefile.am`, maintained project files | Completed patch API binding/build integration. |
| OGRE wizard script/page resources | Keep optional patch workflow; revise purpose text. |
| `test_matrix` private fixtures/results | Retained investigation/regression evidence only. |
| Future synthetic fixture | Public current-master reproducer. |
| External OGRE probe | Historical installed-SDK rendering proof. |
| Public releases | Exclude private regression files and derivatives. |

---

## 13. Milestones

| | Objective | Check | Status |
|---|---|---|---|
| M1 | Reproduce and instrument material loss. | Geometry success versus material failure established. | **DONE** |
| M2 | Map version/patch/layout behaviour. | Flat/deep matrix retained. | **DONE** |
| M3 | Identify actual root cause. | Pristine `v14.6.2` + `recursive=true` succeeds with `AssimpLoader.cpp` untouched. | **DONE** |
| M4.1 | Remove historical `AssimpLoader.cpp` delta. | Final source state leaves Assimp loader pristine. | **NEXT** |
| M4.2 | Generate converter-only patch. | One-line `main.cpp` diff from pristine exact `v14.6.2`. | **PLANNED** |
| M4.3 | Validate patch mechanics. | `diff --check`, forward check and reverse check succeed. | **PLANNED** |
| M5.1 | Implement `macpatch.h/.cpp`. | Known-patch Git mechanism works. | **DONE** |
| M5.2 | Bind native operations. | Installed plug-in exposes only known-patch methods. | **DONE** |
| M5.3 | Integrate optional patch page. | Patch is visible, optional and before configuration. | **DONE** |
| M5.4 | Replace patch resource/explanation. | Wizard offers converter-only recursive-registration patch. | **PLANNED** |
| M6.1 | Preserve investigation matrix. | Historical outputs/logs/manifests retained. | **DONE** |
| M6.2 | Validate final local patch lifecycle. | Flat and child-material accepted path passes; declined/already-applied states behave correctly. | **PLANNED** |
| M6.3 | Build public synthetic fixture. | Original boundary-respecting reproducer created. | **PLANNED** |
| M6.4 | Validate current-master correction. | Pristine FAIL versus recursive-registration PASS. | **PLANNED** |
| M6.5 | Submit upstream if confirmed. | Focused converter-only PR with synthetic evidence. | **BLOCKED** |
| M7 | Final regression and release. | Patched/pristine branches and starter contracts pass. | **PLANNED** |

---

## 14. Acceptance criteria

v3.8.1 is accepted only if:

1. v3.8 remains the known-good baseline.
2. `macpatch` remains implemented, constrained and reusable.
3. The final OGRE patch starts from pristine exact `v14.6.2`.
4. The final OGRE patch modifies only `Tools/AssimpConverter/main.cpp`.
5. `PlugIns/Assimp/src/AssimpLoader.cpp` remains pristine upstream.
6. The production behaviour change is limited to recursive registration of the existing source `FileSystem` resource location.
7. The helper continues to use Git argument vectors without a shell.
8. Applicability and already-applied detection work with the revised patch.
9. The patch remains visible and optional.
10. Declining preserves pristine OGRE and the Ogre-head workflow.
11. Accepting produces correct material conversion for the supported flat regression fixture.
12. Accepting produces correct material conversion for a child MTL beneath the registered source root.
13. The compatibility patch does not intentionally authorise parent/sibling traversal outside the registered root.
14. No direct unrestricted filesystem fallback is introduced.
15. Repeated wizard runs remain idempotent.
16. User assets are not rewritten or renamed by the wizard.
17. Third-party fixtures and derivatives remain outside public artefacts.
18. Squirrel cannot provide arbitrary patch contents or commands.
19. Any upstream fixture is newly authored and legally redistributable.
20. Upstream submission requires separate current-master pristine-versus-corrected proof and an explicit local-patch retirement condition.

---

## 15. What stays rejected

- Silent OGRE source modification.
- Claiming patched OGRE is pristine.
- A Code::Blocks-owned OGRE fork.
- A bespoke source rewriter.
- Arbitrary patch paths, contents, Git commands or shell commands from Squirrel.
- Destructive Git recovery or wizard-created local commits.
- Retaining the historical `AssimpLoader.cpp` workaround in the final patch.
- Direct unrestricted filesystem fallback from model-controlled paths.
- Treating `../` escape outside the registered resource root as a required feature.
- Treating `.mesh` existence as full conversion success.
- Publishing proprietary regression assets, screenshots or derivatives.
- Ambiguous regression provenance such as unqualified `before` / `after` files.
- Assuming a historical `v14.6.2` observation is sufficient evidence for an upstream PR.
- Keeping the local converter patch after the pinned official version includes the correction.

---

## 16. Stop conditions

Stop if local delivery requires a permanent fork, accumulating patchset, destructive source recovery, arbitrary patch execution, source-asset rewriting, unrestricted model-controlled filesystem access, publication of private assets or destabilisation of the v3.8 starter.

A one-line, optional, exact-version patch that makes the converter's already-registered source directory recursive does not trigger these conditions. `macpatch` remains useful infrastructure for future constrained compatibility corrections.

---

## 17. Done when

**IN PROGRESS.** v3.8.1 is complete when the obsolete `AssimpLoader.cpp` compatibility patch is replaced by a clean converter-only exact-`v14.6.2` patch; the existing optional `macpatch` workflow delivers that patch safely; flat and boundary-respecting child-material regressions pass after acceptance; decline and already-applied states remain correct; private regression evidence stays outside released artefacts; the v3.8 Ogre-head starter remains unchanged; and current-upstream submission is either supported by a separate synthetic pristine-versus-corrected proof or explicitly deferred without blocking the local release.

```text
v3.8 known-good wizard                         DONE
        |
        v
material-loss investigation                    DONE
        |
        v
flat/deep evidence matrix                      DONE
        |
        v
historical AssimpLoader patch evaluated        DONE
        |
        v
boundary-respecting child fixture              DONE
        |
        v
recursive registration root cause              DONE
        |
        v
retire AssimpLoader patch                      NEXT
        |
        v
clean one-line main.cpp patch
        |
        v
replace known patch resource/text
        |
        v
revalidate macpatch lifecycle
        |
        v
final v14.6.2 flat + child regression
        |
        v
v3.8.1 release
        |
        +-- synthetic current-master fixture
        |       |
        |       v
        |   pristine vs recursive=true proof
        |       |
        |       v
        |   upstream PR if confirmed
        |
        v
future pinned OGRE includes correction
        |
        v
retire OGRE patch, retain macpatch
```

