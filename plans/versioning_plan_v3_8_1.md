# Code::Blocks v3.8.1 - correct OGRE asset conversion on macOS

**Status: IN PROGRESS.** v3.8.1 follows the released v3.8 OGRE-wizard baseline and corrects the remaining asset-conversion problem exposed after Assimp was added as a declared Homebrew prerequisite.

**Question this version answers:** why did upstream OGRE `OgreAssimpConverter` fail to preserve materials from an otherwise-readable `.obj` / `.mtl` asset bundle on macOS, how can the failure be corrected immediately for the pinned OGRE `v14.6.2` workflow, and how should that correction later return upstream?

The defect has been fully localised and corrected end to end. `OgreAssimpConverter` registered the model directory correctly, and OGRE could find the MTL under its resource-relative name. Assimp requested the sidecar through `OgreIOSystem` using an absolute path. `OgreIOSystem` passed that absolute path unchanged to `ResourceGroupManager`, whose registered filesystem location expected a resource-relative name. Converting requests beneath the source directory back to resource-relative names allowed the MTL to load, restored the expected material and texture references, and produced a converted model that rendered correctly in a completely external OGRE application.

A clean, minimal Git patch has now been produced from pristine OGRE `v14.6.2` source. The patch changes only `PlugIns/Assimp/src/AssimpLoader.cpp`, contains no diagnostics or local asset references, and is stored outside the disposable OGRE source/build/install hierarchy. `git apply --reverse --check` succeeds against the corrected source tree, proving the `already_applied` detection needed by the future native helper.

v3.8 remains the known-good released baseline. The v3.8.1 implementation order is now:

1. preserve the confirmed diagnosis, correction and rendering proof;
2. preserve the clean version-specific Git patch for OGRE `v14.6.2`;
3. recreate the pristine source/build/install workflow through the v3.8 wizard;
4. implement a visible, optional and narrowly constrained Git-backed patch mechanism;
5. validate pristine, applicable, already-applied, declined and unexpected source states;
6. submit the clean generic correction upstream to OGRE;
7. retire the local compatibility patch once the pinned official OGRE release includes the correction.

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

The general v3.8 architecture remains valid. The wizard owns prerequisite establishment, exact source acquisition, configure/build/install orchestration and project generation. The new issue is narrower: the pinned upstream release contains a defect in OGRE's Assimp sidecar-resource path.

No diagnostic instrumentation is part of the clean patch. The compatibility change contains only the generic correction required to restore valid resource-name handling.

---

## 2. Regression asset and publication boundary

The initial failure was reproduced locally with a third-party model bundle containing:

```text
full-resolution .dae input
full-resolution .obj input
matching .mtl sidecar
multiple PNG textures
additional lower-detail model variants
```

The third-party bundle, its extracted files, textures, converted outputs and screenshots are **not part of the Code::Blocks repository, release artefacts or public regression-data distribution**.

The public plan records only the technical behaviour observed during local testing. Public reproduction should use a separately obtained asset that the tester is legally permitted to use and that presents an equivalent structure:

```text
model.obj
model.mtl
one or more texture files referenced by model.mtl
```

The original local test bundle remains unmodified. Diagnostic and rendering work uses disposable staging directories outside the public repository.

---

## 3. Confirmed end-to-end outcome

### 3.1 OGRE converter installation

A fresh OGRE build performed after Assimp installation produces:

```text
~/Library/CodeBlocks/ogre-14.6.2/bin/macosx/OgreAssimpConverter
~/Library/CodeBlocks/ogre-14.6.2/lib/macosx/Release/Codec_Assimp.framework
```

The converter requires the known macOS framework location and usable OGRE plug-in configuration.

### 3.2 COLLADA control

The locally tested COLLADA input was rejected by Assimp because it contained an invalid reference form. This is treated as a source-asset failure, not evidence of the OGRE sidecar-resource defect addressed by v3.8.1.

### 3.3 Original OBJ behaviour

Before correction, the OBJ input produced:

```text
6 meshes
6000 faces
17763 post-processed vertices
successful OGRE mesh serialisation
```

The matching `.mtl` sidecar was nevertheless lost, so the mesh did not preserve the intended materials faithfully.

### 3.4 Standalone Assimp control

Homebrew Assimp `6.0.5` imported the same staged OBJ/MTL bundle correctly. `otool` confirmed that OGRE's `Codec_Assimp.framework` links to Homebrew's current `libassimp.6.dylib`; the discrepancy was not caused by a separate obsolete Assimp copy.

### 3.5 Corrected conversion and rendering

After correcting `OgreIOSystem` path handling, conversion produced:

```text
successful OGRE mesh serialisation
four effective generated materials
four retained diffuse texture references
```

A completely external probe configured against the installed SDK loaded the converted mesh, material script and all expected textures. After camera framing was adjusted to the model's measured bounds, the model rendered correctly.

```text
OBJ + MTL + textures
        |
        v
corrected OgreAssimpConverter
        |
        v
.mesh + .material + texture references
        |
        v
installed OGRE SDK
        |
        v
completely external consumer
        |
        v
correct textured rendering
```

The initial dark-blue render window was a camera-framing issue, not a conversion or resource-loading failure. The original probe camera was approximately 150 units from a model only a few units across.

---

## 4. Confirmed technical cause

The DataStream-based `AssimpLoader` path installs `OgreIOSystem`. `OgreAssimpConverter` registers the source directory as a filesystem resource location, opens the primary model and delegates decoding through the Assimp codec.

```text
ResourceGroupManager sees model.mtl by relative name
        |
        v
Assimp requests /absolute/path/to/model.mtl
        |
        v
OgreIOSystem forwards the absolute path
        |
        v
ResourceGroupManager lookup fails
```

Instrumentation proved:

```text
resourceExists("model.mtl") = true
Open("/absolute/path/to/model.mtl") = unresolved
```

After correction:

```text
request=[/absolute/source/path/model.mtl]
resource=[model.mtl]
resolved=[1]
```

---

## 5. M1 - instrument sidecar-resource resolution

**Status: DONE.**

Temporary diagnostics in `Tools/AssimpConverter/main.cpp` and `PlugIns/Assimp/src/AssimpLoader.cpp` established the name mismatch precisely.

Technical lessons:

- Resource visibility before codec execution did not prove that the codec would request the same name.
- Instrumenting both the converter and `OgreIOSystem` was necessary.
- Assimp's repeated fallback probes were secondary symptoms; the first absolute MTL request exposed the failure.
- The OGRE build tree was sufficient for diagnosis; the installed SDK was required for the external-consumer proof.

---

## 6. M2 - identify the exact failure boundary

**Status: DONE.**

The investigation ruled out missing files, permissions, quarantine attributes, wrong staging, wrong resource group, obsolete Assimp linkage and malformed OBJ/MTL relationships.

The exact defect is:

```text
absolute Assimp sidecar filename
        versus
resource-relative ResourceGroupManager name
```

Technical lessons:

- `ResourceGroupManager` resolved the MTL when queried as `model.mtl`.
- `OgreIOSystem` failed when it queried the same registered location using the full absolute path.
- The custom `OgreIOSystem`, not standalone Assimp filesystem I/O, explains the discrepancy.
- `Root::openFileStream()` handled the primary model, while secondary resources followed different name semantics through `OgreIOSystem`.

---

## 7. M3 - prove the smallest correction

**Status: DONE.**

The correction stores the source DataStream directory and converts absolute requests beneath that directory into resource-relative names before using `ResourceGroupManager`.

Relative subpaths are preserved. Requests are not blindly reduced to basenames, because valid bundles may use:

```text
materials/model.mtl
textures/diffuse.png
```

Technical lessons:

- The generic correction belongs in OGRE's Assimp resource-I/O path.
- Prefix removal must be constrained to the source-directory boundary.
- Source assets require no rewriting or renaming.
- `.mesh` existence alone was not sufficient; materials, textures and visible rendering were required.
- Camera framing is a separate application concern and must not be confused with conversion correctness.

---

## 8. M4 - clean local compatibility patch

**Status: DONE.**

The experimental source tree was restored to exact upstream state before recreating the final correction. This avoided subtractive clean-up of diagnostic edits and guaranteed that the final diff represented only the intended production change.

The final diff is:

```text
PlugIns/Assimp/src/AssimpLoader.cpp | 22 +++++++++++++++++++---
1 file changed, 19 insertions(+), 3 deletions(-)
```

The patch:

- modifies only `PlugIns/Assimp/src/AssimpLoader.cpp`;
- adds `_sourceDirectory`;
- derives and normalises the source directory once in the constructor;
- adds a compact `getResourceName()` helper;
- uses the helper in `Exists()` and `Open()`;
- preserves existing upstream formatting wherever behaviour is unchanged;
- contains no `<iostream>` addition;
- contains no debug output;
- contains no hard-coded asset names;
- contains no converter-side diagnostic probe;
- leaves the copyright and licence header unchanged.

`git diff --check` completed without output.

The patch was exported as:

```text
ogre-14.6.2-assimp-sidecar-path.patch
```

and preserved in a separate development folder outside:

```text
~/Library/CodeBlocks/ogre-source-14.6.2
~/Library/CodeBlocks/ogre-source-14.6.2/build-codeblocks
~/Library/CodeBlocks/ogre-14.6.2
```

This separation allows the complete experimental source/build/install hierarchy to be deleted without losing the patch artefact.

### 8.1 Proven Git state detection

Against the corrected source tree:

```bash
git apply --reverse --check "$PATCH"
```

returned exit status `0`.

This proves the `already_applied` half of the future native state machine:

```text
reverse --check succeeds
        -> already_applied
```

The remaining `applicable` proof will be performed against the genuinely pristine shallow checkout recreated by the wizard, rather than against a manually manufactured worktree.

### 8.2 Technical lessons from M4

- Reverting to pristine upstream and reapplying only the necessary behaviour produced a much more reviewable diff than cleaning the instrumented file in place.
- Upstream-style formatting matters: unchanged signatures, comments, bracing and line wrapping should remain untouched.
- The patch artefact must live independently from every source/build/install tree that the wizard may replace.
- Development storage outside the OGRE tree is appropriate for now, but the released wizard cannot depend on an external user folder.
- The final patch must become an installed Scripted Wizard resource resolved internally by patch identity.
- `git apply --reverse --check` provides clean, non-mutating already-applied detection.
- No local commit is required to preserve or apply the patch.

---

## 9. M5 - Git-backed optional patch handler

**Status: NEXT.**

Git is the patch engine. Code::Blocks does not implement a bespoke textual source rewriter.

The next integration test deliberately starts by deleting the experimental OGRE source/build/install hierarchy and allowing the existing v3.8 wizard to recreate its normal exact shallow `v14.6.2` checkout. This creates the real state that `macpatch` must support:

```text
macgit shallow clone of exact v14.6.2
        |
        v
pristine wizard-owned source tree
        |
        v
MacPatchProbe(knownPatchId, sourceRoot)
        |
        v
applicable
```

The development copy of the patch remains outside that hierarchy until it is added to Code::Blocks resources.

Conceptual API:

```text
Wizard.MacPatchProbe(patchId, sourceRoot)
Wizard.MacPatchApply(
    patchId,
    sourceRoot,
    explanation,
    confirmLabel)
```

Squirrel must not supply arbitrary patch files, patch contents, target files or command lines.

### 9.1 M5 step 1 - implement `macpatch.h` and `macpatch.cpp`

**Status: PLANNED.**

Add:

```text
src/plugins/scriptedwizard/macpatch.h
src/plugins/scriptedwizard/macpatch.cpp
```

The native helper should:

1. recognise only compiled-in patch identifiers;
2. map the OGRE patch ID to an internally resolved installed resource;
3. validate that `sourceRoot` is absolute and matches the expected wizard-owned source layout;
4. reuse or refactor the validated Git discovery already established by `macgit`;
5. verify the exact expected OGRE tag/commit contract;
6. reject modifications to the target file or unsupported source state;
7. run argument-vector equivalents of:

```bash
git -C <sourceRoot> apply --check <knownPatch>
git -C <sourceRoot> apply --reverse --check <knownPatch>
git -C <sourceRoot> apply <knownPatch>
```

8. capture output and exit status without invoking a shell;
9. distinguish:

```text
applicable
already_applied
unexpected_source
unsupported_version
git_unavailable
cancelled
apply_failed
applied
```

10. verify post-apply state using reverse `--check`;
11. return a structured key/value report consistent with `macgit`, `macbrew` and `maccmake`;
12. leave application visible as an uncommitted working-tree modification.

The helper must not run `git reset`, `git checkout`, `git clean`, `git commit` or destructive recovery commands.

### 9.2 M5 step 2 - bind the native helper

**Status: PLANNED.**

Required work:

```text
include macpatch.h where required
add native wrappers
add BindMethod(...) entries
add source/header files to Makefile.am
add files to maintained project files
rebuild and reinstall the Scripted Wizard plug-in
restart Code::Blocks before runtime validation
```

The binding layer must expose only known-patch operations.

### 9.3 M5 step 3 - update the OGRE wizard script

**Status: PLANNED.**

Add an optional patch stage after exact source acquisition and before CMake configuration.

The page must show:

- pinned OGRE version;
- known Assimp sidecar limitation;
- patch identity and purpose;
- applicable/already-applied/unexpected/unsupported state;
- that accepting modifies the upstream checkout;
- that declining keeps pristine upstream OGRE and the Ogre-head workflow;
- that unpatched external OBJ/MTL conversion remains unsupported;
- and that the patch is temporary pending an official corrected OGRE release.

The page must exist in a clean same-run clone/configure workflow. It cannot be omitted because source state changes after `BeginWizard()` constructs the page sequence.

### 9.4 M5 step 4 - validation

**Status: PLANNED.**

Validate:

#### Clean applicable checkout

```text
exact shallow v14.6.2 checkout
apply --check succeeds
user accepts
patch applies
reverse --check succeeds afterwards
normal configure/build/install succeeds
```

#### User declines

```text
source remains pristine
normal v3.8 configure/build/install succeeds
Ogre-head starter builds and runs
```

#### Already applied

```text
reverse --check succeeds
wizard reports already applied
no second mutation occurs
```

#### Unexpected target-file state

```text
apply --check fails
reverse --check fails
wizard does not mutate source
```

#### Patched end-to-end branch

```text
patch accepted
OGRE configured, built and installed
converter installed
OBJ + MTL + textures converted
mesh, materials and textures verified
converted model rendered externally
```

#### Maintained-machine states

```text
source present, no build tree
source patched, build tree present
source patched, installed SDK absent
source patched, installed SDK present
source pristine, old unpatched build/install present
```

The wizard must not infer that an existing SDK contains the correction merely because the source tree is patched.

### 9.5 M5 fixed design decisions

- Use `git apply`, not a custom source rewriter.
- Keep the patch human-readable and version-specific.
- Use `git apply --check` for applicability.
- Use `git apply --reverse --check` for already-applied detection.
- Leave the modification visible in `git status --short`.
- Do not create a local commit.
- Do not cherry-pick a private commit merely to deliver the patch.
- Keep patch consent separate from clone/configure/build consent.
- Resolve the production patch internally as a Code::Blocks resource, not from an external development path.

---

## 10. M6 - upstream submission

**Status: PLANNED AFTER LOCAL PATCH VALIDATION.**

M6 will be planned more precisely after M5 proves the clean patch and delivery mechanism.

Expected upstream content:

- concise description of absolute Assimp sidecar paths versus resource-relative names;
- evidence that the registered resource location already contains the MTL;
- standalone Assimp control;
- minimal generic correction;
- preservation of relative subdirectories;
- legally redistributable generic or synthetic reproducer;
- external installed-SDK rendering proof.

The upstream patch must contain no Code::Blocks paths, local asset names or diagnostic logging.

---

## 11. M7 - final regression and retirement

**Status: PLANNED.**

The final regression must prove patched, declined, already-patched and invalid-state branches.

Retirement condition:

```text
when the pinned official OGRE version contains the correction:
    remove the patch offer
    remove the patch mapping
    update the supported version contract
    retain only historical documentation
```

---

## 12. Expected files / areas

| File / area | v3.8.1 role |
|---|---|
| OGRE `PlugIns/Assimp/src/AssimpLoader.cpp` | Upstream defect location and clean patch target. |
| `ogre-14.6.2-assimp-sidecar-path.patch` | Clean human-readable patch; M4 artefact complete. |
| Scripted Wizard patch resource directory | Production location installed with Code::Blocks. |
| `src/plugins/scriptedwizard/macpatch.cpp` / `.h` | Constrained state detection and `git apply` implementation. |
| `wiz.cpp`, `Makefile.am`, maintained project files | Bind and compile the patch API. |
| OGRE wizard script/page resources | Visible optional patch state and consent. |
| `macgit` internals | Git discovery reused or refactored. |
| `maccmake` workflow | Configure/build/install after optional patching. |
| External OGRE probe | Ogre-head and converted-asset regression. |
| Public releases | Exclude third-party regression files and derived assets. |

---

## 13. Milestones

| | Objective | Check | Status |
|---|---|---|---|
| M1 | Instrument resource lookup. | MTL lookup mismatch identified. | **DONE** |
| M2 | Localise the defect. | Absolute versus resource-relative path demonstrated. | **DONE** |
| M3 | Prove correction end to end. | Geometry, materials, textures and rendering succeed. | **DONE** |
| M4 | Produce clean Git patch. | Minimal one-file patch, clean diff, no diagnostics, reverse-check succeeds. | **DONE** |
| M5.1 | Implement `macpatch.h/.cpp`. | Probe/apply return correct structured states. | **NEXT** |
| M5.2 | Bind native operations. | Installed plug-in exposes only known-patch methods. | **PLANNED** |
| M5.3 | Update OGRE wizard. | Patch is visible, optional and before configuration. | **PLANNED** |
| M5.4 | Validate lifecycle. | Applicable, declined, applied, already-applied and unexpected states pass. | **PLANNED** |
| M6 | Submit upstream. | Focused issue/PR with generic evidence. | **PLANNED** |
| M7 | Final regression and release. | Patched and pristine branches preserve contracts. | **PLANNED** |

---

## 14. Acceptance criteria

v3.8.1 is accepted only if:

1. v3.8 remains the known-good baseline.
2. The patch contains no diagnostics or local asset references.
3. The patch applies only to exact supported upstream state.
4. The helper uses Git argument vectors without a shell.
5. `git apply --check` identifies applicability.
6. `git apply --reverse --check` identifies already-applied state.
7. Unexpected states fail before mutation.
8. Only the intended OGRE file is modified.
9. The patch is visible and optional.
10. Declining preserves pristine OGRE and Ogre-head workflow.
11. Accepting produces corrected conversion and rendering.
12. Repeated runs are idempotent.
13. Existing build/install states are not mistaken for patched outputs.
14. Relative sidecar subdirectories remain supported.
15. User assets are not modified.
16. Beginner-facing `main.cpp` remains free of conversion machinery.
17. Third-party assets remain outside public artefacts.
18. Squirrel cannot supply arbitrary patches or commands.
19. The correction is submitted upstream after local validation.
20. The local patch has an explicit retirement condition.

---

## 15. What stays rejected

- Silent OGRE source modification.
- Claiming patched OGRE is pristine.
- A Code::Blocks-owned OGRE fork.
- A bespoke source rewriter.
- Arbitrary patch paths or contents from Squirrel.
- Arbitrary Git or shell commands.
- Destructive Git recovery.
- Local commits created by the wizard.
- Cherry-picking a private commit to avoid a patch file.
- Patching without exact validation.
- Basename-only sidecar handling.
- Treating `.mesh` existence as full success.
- Publishing proprietary regression assets or derivatives.
- Depending on the external development patch folder in released code.
- Keeping the patch after an official pinned release includes the fix.

---

## 16. Stop conditions

Stop if delivery requires a permanent fork, accumulating patchset, destructive source recovery, arbitrary patch execution, source-asset rewriting, build-tree coupling or destabilisation of the v3.8 starter.

One narrow, optional, version-validated Git patch submitted upstream does not trigger these conditions. Failure to constrain, validate or retire it would.

---

## 17. Done when

**IN PROGRESS.** v3.8.1 is complete when the clean OGRE correction is delivered through a visible optional Git-backed compatibility patch for exact upstream OGRE `v14.6.2`; `macpatch` is constrained, state-aware and idempotent; patched and declined branches are validated; the converted asset renders correctly; the v3.8 Ogre-head starter remains intact; and the correction is submitted upstream with an explicit retirement path.

```text
v3.8 known-good wizard
        |
        v
diagnosis + correction + rendering       DONE
        |
        v
clean Git patch                           DONE
        |
        v
fresh wizard-created pristine checkout   NEXT TEST BASELINE
        |
        v
macpatch implementation                  NEXT
        |
        v
Squirrel binding
        |
        v
visible optional wizard page
        |
        v
full state validation
        |
        v
upstream submission
        |
        v
v3.8.1 release
        |
        v
future corrected OGRE release
        |
        v
retire local patch
```

