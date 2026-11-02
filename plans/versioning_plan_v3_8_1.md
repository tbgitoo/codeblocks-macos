# Code::Blocks v3.8.1 - correct OGRE asset conversion on macOS
<br>
**<strong class="___1qwroh2 fl43uef f19n0e5" data-lexical-text="true">Status: IN PROGRESS.</strong>** v3.8.1 follows the released v3.8 OGRE-wizard baseline and corrects the remaining asset-conversion problem exposed after Assimp was added as a declared Homebrew prerequisite.
<br>
**<strong class="___1qwroh2 fl43uef f19n0e5" data-lexical-text="true">Question this version answers:</strong>** why did upstream OGRE `OgreAssimpConverter` fail to preserve materials from an otherwise-readable `.obj` / `.mtl` asset bundle on macOS, how can the failure be corrected immediately for the pinned OGRE `v14.6.2` workflow, and how should that correction later return upstream?
<br>
The defect has been fully localised and corrected end to end. `OgreAssimpConverter` registered the model directory correctly, and OGRE could find the MTL under its resource-relative name. Assimp requested the sidecar through `OgreIOSystem` using an absolute path. `OgreIOSystem` passed that absolute path unchanged to `ResourceGroupManager`, whose registered filesystem location expected a resource-relative name. Converting requests beneath the source directory back to resource-relative names allowed the MTL to load, restored the expected material and texture references, and produced a converted model that rendered correctly in a completely external OGRE application.
<br>
A clean, minimal Git patch has now been produced from pristine OGRE `v14.6.2` source. The patch changes only `PlugIns/Assimp/src/AssimpLoader.cpp`, contains no diagnostics or local asset references, and is stored outside the disposable OGRE source/build/install hierarchy. The visible, optional and narrowly constrained Git-backed `macpatch` mechanism has been implemented and validated for the supported wizard-owned source lifecycle.
<br>
v3.8 remains the known-good released baseline. The v3.8.1 implementation order is now:
<br>
1. preserve the confirmed diagnosis, correction and rendering proof;
2. preserve the clean version-specific Git patch for OGRE `v14.6.2`;
3. recreate the pristine source/build/install workflow through the v3.8 wizard;
4. implement a visible, optional and narrowly constrained Git-backed patch mechanism;
5. validate pristine, applicable, already-applied and declined states;
6. submit the clean generic correction upstream to OGRE;
7. retire the local compatibility patch once the pinned official OGRE release includes the correction.
<br>
---
<br>
## 1. Known-good starting point from v3.8
<br>
The released v3.8 workflow establishes:
<br>
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
<br>
The general v3.8 architecture remains valid. The wizard owns prerequisite establishment, exact source acquisition, configure/build/install orchestration and project generation. The new issue is narrower: the pinned upstream release contains a defect in OGRE's Assimp sidecar-resource path.
<br>
No diagnostic instrumentation is part of the clean patch. The compatibility change contains only the generic correction required to restore valid resource-name handling.
<br>
---
<br>
## 2. Regression asset and publication boundary
<br>
The initial failure was reproduced locally with a third-party model bundle containing:
<br>
```text
full-resolution .dae input
full-resolution .obj input
matching .mtl sidecar
multiple PNG textures
additional lower-detail model variants
```
<br>
The third-party bundle, its extracted files, textures, converted outputs and screenshots are **<strong class="___1qwroh2 fl43uef f19n0e5" data-lexical-text="true">not part of the Code::Blocks repository, release artefacts or public regression-data distribution</strong>**.
<br>
The public plan records only the technical behaviour observed during local testing. Public reproduction should use a separately obtained asset that the tester is legally permitted to use and that presents an equivalent structure:
<br>
```text
model.obj
model.mtl
one or more texture files referenced by model.mtl
```
<br>
The original local test bundle remains unmodified. Diagnostic and rendering work uses disposable staging directories outside the public repository.
<br>
---
<br>
## 3. Confirmed end-to-end outcome
<br>
### 3.1 OGRE converter installation
<br>
A fresh OGRE build performed after Assimp installation produces:
<br>
```text
~/Library/CodeBlocks/ogre-14.6.2/bin/macosx/OgreAssimpConverter
~/Library/CodeBlocks/ogre-14.6.2/lib/macosx/Release/Codec_Assimp.framework
```
<br>
The converter requires the known macOS framework location and usable OGRE plug-in configuration.
<br>
### 3.2 COLLADA control
<br>
The locally tested COLLADA input was rejected by Assimp because it contained an invalid reference form. This is treated as a source-asset failure, not evidence of the OGRE sidecar-resource defect addressed by v3.8.1.
<br>
### 3.3 Original OBJ behaviour
<br>
Before correction, the OBJ input produced:
<br>
```text
6 meshes
6000 faces
17763 post-processed vertices
successful OGRE mesh serialisation
```
<br>
The matching `.mtl` sidecar was nevertheless lost, so the mesh did not preserve the intended materials faithfully.
<br>
### 3.4 Standalone Assimp control
<br>
Homebrew Assimp `6.0.5` imported the same staged OBJ/MTL bundle correctly. `otool` confirmed that OGRE's `Codec_Assimp.framework` links to Homebrew's current `libassimp.6.dylib`; the discrepancy was not caused by a separate obsolete Assimp copy.
<br>
### 3.5 Corrected conversion and rendering
<br>
After correcting `OgreIOSystem` path handling, conversion produced:
<br>
```text
successful OGRE mesh serialisation
four effective generated materials
four retained diffuse texture references
```
<br>
A completely external probe configured against the installed SDK loaded the converted mesh, material script and all expected textures. After camera framing was adjusted to the model's measured bounds, the model rendered correctly.
<br>
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
<br>
The initial dark-blue render window was a camera-framing issue, not a conversion or resource-loading failure. The original probe camera was approximately 150 units from a model only a few units across.
<br>
---
<br>
## 4. Confirmed technical cause
<br>
The DataStream-based `AssimpLoader` path installs `OgreIOSystem`. `OgreAssimpConverter` registers the source directory as a filesystem resource location, opens the primary model and delegates decoding through the Assimp codec.
<br>
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
<br>
Instrumentation proved:
<br>
```text
resourceExists("model.mtl") = true
Open("/absolute/path/to/model.mtl") = unresolved
```
<br>
After correction:
<br>
```text
request=[/absolute/source/path/model.mtl]
resource=[model.mtl]
resolved=[1]
```
<br>
---
<br>
## 5. M1 - instrument sidecar-resource resolution
<br>
**<strong class="___1qwroh2 fl43uef f19n0e5" data-lexical-text="true">Status: DONE.</strong>**
<br>
Temporary diagnostics in `Tools/AssimpConverter/main.cpp` and `PlugIns/Assimp/src/AssimpLoader.cpp` established the name mismatch precisely.
<br>
Technical lessons:
<br>
- Resource visibility before codec execution did not prove that the codec would request the same name.
- Instrumenting both the converter and `OgreIOSystem` was necessary.
- Assimp's repeated fallback probes were secondary symptoms; the first absolute MTL request exposed the failure.
- The OGRE build tree was sufficient for diagnosis; the installed SDK was required for the external-consumer proof.
<br>
---
<br>
## 6. M2 - identify the exact failure boundary
<br>
**<strong class="___1qwroh2 fl43uef f19n0e5" data-lexical-text="true">Status: DONE.</strong>**
<br>
The investigation ruled out missing files, permissions, quarantine attributes, wrong staging, wrong resource group, obsolete Assimp linkage and malformed OBJ/MTL relationships.
<br>
The exact defect is:
<br>
```text
absolute Assimp sidecar filename
        versus
resource-relative ResourceGroupManager name
```
<br>
Technical lessons:
<br>
- `ResourceGroupManager` resolved the MTL when queried as `model.mtl`.
- `OgreIOSystem` failed when it queried the same registered location using the full absolute path.
- The custom `OgreIOSystem`, not standalone Assimp filesystem I/O, explains the discrepancy.
- `Root::openFileStream()` handled the primary model, while secondary resources followed different name semantics through `OgreIOSystem`.
<br>
---
<br>
## 7. M3 - prove the smallest correction
<br>
**<strong class="___1qwroh2 fl43uef f19n0e5" data-lexical-text="true">Status: DONE.</strong>**
<br>
The correction stores the source DataStream directory and converts absolute requests beneath that directory into resource-relative names before using `ResourceGroupManager`.
<br>
Relative subpaths are preserved. Requests are not blindly reduced to basenames, because valid bundles may use:
<br>
```text
materials/model.mtl
textures/diffuse.png
```
<br>
Technical lessons:
<br>
- The generic correction belongs in OGRE's Assimp resource-I/O path.
- Prefix removal must be constrained to the source-directory boundary.
- Source assets require no rewriting or renaming.
- `.mesh` existence alone was not sufficient; materials, textures and visible rendering were required.
- Camera framing is a separate application concern and must not be confused with conversion correctness.
<br>
---
<br>
## 8. M4 - clean local compatibility patch
<br>
**<strong class="___1qwroh2 fl43uef f19n0e5" data-lexical-text="true">Status: DONE.</strong>**
<br>
The experimental source tree was restored to exact upstream state before recreating the final correction. This avoided subtractive clean-up of diagnostic edits and guaranteed that the final diff represented only the intended production change.
<br>
The final diff is:
<br>
```text
PlugIns/Assimp/src/AssimpLoader.cpp | 22 +++++++++++++++++++---
1 file changed, 19 insertions(+), 3 deletions(-)
```
<br>
The patch:
<br>
- modifies only `PlugIns/Assimp/src/AssimpLoader.cpp`;
- adds `_sourceDirectory`;
- derives and normalises the source directory once in the constructor;
- adds a compact `getResourceName()` helper;
- uses the helper in `Exists()` and `Open()`;
- preserves existing upstream formatting wherever behaviour is unchanged;
- contains no `&lt;iostream&gt;` addition;
- contains no debug output;
- contains no hard-coded asset names;
- contains no converter-side diagnostic probe;
- leaves the copyright and licence header unchanged.
<br>
`git diff --check` completed without output.
<br>
The patch was exported as:
<br>
```text
ogre-14.6.2-assimp-sidecar-path.patch
```
<br>
and preserved in a separate development folder outside:
<br>
```text
~/Library/CodeBlocks/ogre-source-14.6.2
~/Library/CodeBlocks/ogre-source-14.6.2/build-codeblocks
~/Library/CodeBlocks/ogre-14.6.2
```
<br>
This separation allows the complete experimental source/build/install hierarchy to be deleted without losing the patch artefact.
<br>
### 8.1 Proven Git state detection
<br>
Against the corrected source tree:
<br>
```bash
git apply --reverse --check "$PATCH"
```
<br>
returned exit status `0`.
<br>
This proves the `already_applied` half of the native state machine:
<br>
```text
reverse --check succeeds
        -&gt; already_applied
```
<br>
Applicability against the genuinely pristine shallow checkout recreated by the wizard was subsequently proven during M5 validation.
<br>
### 8.2 Technical lessons from M4
<br>
- Reverting to pristine upstream and reapplying only the necessary behaviour produced a much more reviewable diff than cleaning the instrumented file in place.
- Upstream-style formatting matters: unchanged signatures, comments, bracing and line wrapping should remain untouched.
- The patch artefact must live independently from every source/build/install tree that the wizard may replace.
- Development storage outside the OGRE tree is appropriate for now, but the released wizard cannot depend on an external user folder.
- The final patch must become an installed Scripted Wizard resource resolved internally by patch identity.
- `git apply --reverse --check` provides clean, non-mutating already-applied detection.
- No local commit is required to preserve or apply the patch.
<br>
---
<br>
## 9. M5 - Git-backed optional patch handler
<br>
**<strong class="___1qwroh2 fl43uef f19n0e5" data-lexical-text="true">Status: DONE.</strong>**
<br>
Git is the patch engine. Code::Blocks does not implement a bespoke textual source rewriter.
<br>
The implementation was validated against the normal exact shallow `v14.6.2` checkout created by the v3.8 wizard:
<br>
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
<br>
The production patch is installed as a Code::Blocks Scripted Wizard resource and resolved internally by patch identity.
<br>
Implemented API:
<br>
```text
Wizard.MacPatchProbe(patchId, sourceRoot)
Wizard.MacPatchApply(
    patchId,
    sourceRoot,
    explanation,
    confirmLabel)
```
<br>
Squirrel cannot supply arbitrary patch files, patch contents, target files or command lines.
<br>
### 9.1 M5 step 1 - implement `macpatch.h` and `macpatch.cpp`
<br>
**<strong class="___1qwroh2 fl43uef f19n0e5" data-lexical-text="true">Status: DONE.</strong>**
<br>
Added:
<br>
```text
src/plugins/scriptedwizard/macpatch.h
src/plugins/scriptedwizard/macpatch.cpp
```
<br>
The native helper:
<br>
1. recognises only compiled-in patch identifiers;
2. maps the OGRE patch ID to an internally resolved installed resource;
3. validates that `sourceRoot` is absolute and matches the expected wizard-owned source layout;
4. reuses the validated Git discovery established by `macgit`;
5. verifies the exact expected OGRE tag/commit contract;
6. runs argument-vector equivalents of:
<br>
```bash
git -C &lt;sourceRoot&gt; apply --check &lt;knownPatch&gt;
git -C &lt;sourceRoot&gt; apply --reverse --check &lt;knownPatch&gt;
git -C &lt;sourceRoot&gt; apply &lt;knownPatch&gt;
```
<br>
7. captures output and exit status without invoking a shell;
8. distinguishes:
<br>
```text
applicable
already_applied
unsupported_version
git_unavailable
cancelled
apply_failed
applied
```
<br>
9. verifies post-apply state using reverse `--check`;
10. returns a structured key/value report consistent with `macgit`, `macbrew` and `maccmake`;
11. leaves application visible as an uncommitted working-tree modification.
<br>
The helper does not run `git reset`, `git checkout`, `git clean`, `git commit` or destructive recovery commands.
<br>
The source checkout is wizard-owned and disposable. A locally altered checkout is outside the supported lifecycle and is recovered by deleting it and allowing the wizard to recreate the exact pinned source, rather than by maintaining a dedicated modified-source regression branch.
<br>
### 9.2 M5 step 2 - bind the native helper
<br>
**<strong class="___1qwroh2 fl43uef f19n0e5" data-lexical-text="true">Status: DONE.</strong>**
<br>
Completed work:
<br>
```text
include macpatch.h where required
add native wrappers
add BindMethod(...) entries
add source/header files to Makefile.am
add files to maintained project files
rebuild and reinstall the Scripted Wizard plug-in
restart Code::Blocks before runtime validation
```
<br>
The binding layer exposes only known-patch operations.
<br>
### 9.3 M5 step 3 - update the OGRE wizard script
<br>
**<strong class="___1qwroh2 fl43uef f19n0e5" data-lexical-text="true">Status: DONE.</strong>**
<br>
An optional patch stage is present after exact source acquisition and before CMake configuration.
<br>
The page shows:
<br>
- pinned OGRE version;
- known Assimp sidecar limitation;
- patch identity and purpose;
- applicable, already-applied or unsupported state;
- that accepting modifies the upstream checkout;
- that declining keeps pristine upstream OGRE and the Ogre-head workflow;
- that unpatched external OBJ/MTL conversion remains unsupported;
- and that the patch is temporary pending an official corrected OGRE release.
<br>
The page exists in a clean same-run clone/configure workflow because source state changes after `BeginWizard()` constructs the page sequence.
<br>
### 9.4 M5 step 4 - validation
<br>
**<strong class="___1qwroh2 fl43uef f19n0e5" data-lexical-text="true">Status: DONE.</strong>**
<br>
Validated:
<br>
#### Clean applicable checkout
<br>
```text
exact shallow v14.6.2 checkout
apply --check succeeds
user accepts
patch applies
reverse --check succeeds afterwards
normal configure/build/install succeeds
```
<br>
#### User declines
<br>
```text
source remains pristine
normal v3.8 configure/build/install succeeds
Ogre-head starter builds and runs
```
<br>
#### Already applied
<br>
```text
reverse --check succeeds
wizard reports already applied
no second mutation occurs
```
<br>
#### Patched end-to-end branch
<br>
```text
patch accepted
OGRE configured, built and installed
converter installed
OBJ + MTL + textures converted
mesh, materials and textures verified
converted model rendered externally
```
<br>
The wizard does not infer that an existing SDK contains the correction merely because the source tree is patched.
<br>
### 9.5 M5 fixed design decisions
<br>
- Use `git apply`, not a custom source rewriter.
- Keep the patch human-readable and version-specific.
- Use `git apply --check` for applicability.
- Use `git apply --reverse --check` for already-applied detection.
- Leave the modification visible in `git status --short`.
- Do not create a local commit.
- Do not cherry-pick a private commit merely to deliver the patch.
- Keep patch consent separate from clone/configure/build consent.
- Resolve the production patch internally as a Code::Blocks resource, not from an external development path.
- Treat the wizard-owned source checkout as disposable; delete and recreate it if it is altered outside the supported workflow.
<br>
---
<br>
## 10. M6 - upstream submission
<br>
**<strong class="___1qwroh2 fl43uef f19n0e5" data-lexical-text="true">Status: NEXT.</strong>**
<br>
M5 has proven the clean patch and local delivery mechanism. The next work is the focused upstream submission.
<br>
Expected upstream content:
<br>
- concise description of absolute Assimp sidecar paths versus resource-relative names;
- evidence that the registered resource location already contains the MTL;
- standalone Assimp control;
- minimal generic correction;
- preservation of relative subdirectories;
- legally redistributable generic or synthetic reproducer;
- external installed-SDK rendering proof.
<br>
The upstream patch must contain no Code::Blocks paths, local asset names or diagnostic logging.
<br>
---
<br>
## 11. M7 - final regression and retirement
<br>
**<strong class="___1qwroh2 fl43uef f19n0e5" data-lexical-text="true">Status: PLANNED.</strong>**
<br>
The final regression must prove patched, declined and already-patched branches.
<br>
Retirement condition:
<br>
```text
when the pinned official OGRE version contains the correction:
    remove the patch offer
    remove the patch mapping
    update the supported version contract
    retain only historical documentation
```
<br>
---
<br>
## 12. Expected files / areas
<br>
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
<br>
---
<br>
## 13. Milestones
<br>
| | Objective | Check | Status |
|---|---|---|---|
| M1 | Instrument resource lookup. | MTL lookup mismatch identified. | **<strong class="___1qwroh2 fl43uef f19n0e5" data-lexical-text="true">DONE</strong>** |
| M2 | Localise the defect. | Absolute versus resource-relative path demonstrated. | **<strong class="___1qwroh2 fl43uef f19n0e5" data-lexical-text="true">DONE</strong>** |
| M3 | Prove correction end to end. | Geometry, materials, textures and rendering succeed. | **<strong class="___1qwroh2 fl43uef f19n0e5" data-lexical-text="true">DONE</strong>** |
| M4 | Produce clean Git patch. | Minimal one-file patch, clean diff, no diagnostics, reverse-check succeeds. | **<strong class="___1qwroh2 fl43uef f19n0e5" data-lexical-text="true">DONE</strong>** |
| M5.1 | Implement `macpatch.h/.cpp`. | Probe/apply return correct structured states. | **<strong class="___1qwroh2 fl43uef f19n0e5" data-lexical-text="true">DONE</strong>** |
| M5.2 | Bind native operations. | Installed plug-in exposes only known-patch methods. | **<strong class="___1qwroh2 fl43uef f19n0e5" data-lexical-text="true">DONE</strong>** |
| M5.3 | Update OGRE wizard. | Patch is visible, optional and before configuration. | **<strong class="___1qwroh2 fl43uef f19n0e5" data-lexical-text="true">DONE</strong>** |
| M5.4 | Validate lifecycle. | Applicable, declined, applied and already-applied states pass. | **<strong class="___1qwroh2 fl43uef f19n0e5" data-lexical-text="true">DONE</strong>** |
| M6 | Submit upstream. | Focused issue/PR with generic evidence. | **<strong class="___1qwroh2 fl43uef f19n0e5" data-lexical-text="true">NEXT</strong>** |
| M7 | Final regression and release. | Patched and pristine branches preserve contracts. | **<strong class="___1qwroh2 fl43uef f19n0e5" data-lexical-text="true">PLANNED</strong>** |
<br>
---
<br>
## 14. Acceptance criteria
<br>
v3.8.1 is accepted only if:
<br>
1. v3.8 remains the known-good baseline.
2. The patch contains no diagnostics or local asset references.
3. The patch applies only to exact supported upstream state.
4. The helper uses Git argument vectors without a shell.
5. `git apply --check` identifies applicability.
6. `git apply --reverse --check` identifies already-applied state.
7. Unsupported OGRE versions are not modified.
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
<br>
---
<br>
## 15. What stays rejected
<br>
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
<br>
---
<br>
## 16. Stop conditions
<br>
Stop if delivery requires a permanent fork, accumulating patchset, destructive source recovery, arbitrary patch execution, source-asset rewriting, build-tree coupling or destabilisation of the v3.8 starter.
<br>
One narrow, optional, version-validated Git patch submitted upstream does not trigger these conditions. Failure to constrain, validate or retire it would.
<br>
---
<br>
## 17. Done when
<br>
**<strong class="___1qwroh2 fl43uef f19n0e5" data-lexical-text="true">IN PROGRESS.</strong>** v3.8.1 is complete when the clean OGRE correction is delivered through a visible optional Git-backed compatibility patch for exact upstream OGRE `v14.6.2`; `macpatch` is constrained, state-aware and idempotent; patched and declined branches are validated; the converted asset renders correctly; the v3.8 Ogre-head starter remains intact; and the correction is submitted upstream with an explicit retirement path.
<br>
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
fresh wizard-created pristine checkout   DONE
        |
        v
macpatch implementation                  DONE
        |
        v
Squirrel binding                         DONE
        |
        v
visible optional wizard page             DONE
        |
        v
supported lifecycle validation           DONE
        |
        v
upstream submission                      NEXT
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
<br>
<br>

