# Code::Blocks v3.8.2 - project-context tool providers

**Status: IN PROGRESS - Project-menu, lifecycle, passive configuration, safe process/logging, and toolkit-detection seams located.** v3.8.2 follows the released v3.8.1 OGRE conversion baseline and makes installed ecosystem tools discoverable and usable from the active Code::Blocks project without tying the feature to a particular wizard, project template, or single-library project.

**Question this version answers:** how can Code::Blocks detect that an active project or target uses a toolkit such as OGRE, locate the matching toolkit executables from the project's effective configuration, and expose safe project-context actions while remaining valid for manually configured and multi-toolkit projects such as SDL3 + OGRE?

The first provider and proving use case is OGRE. The architecture must nevertheless be generic enough that a project may acquire more than one independent provider from its effective build configuration.

```text
active Code::Blocks project / target
        |
        +-- compiler search directories
        +-- linker search directories
        +-- linked libraries / frameworks
        +-- project and target variables
        +-- configured executable search paths
        +-- process PATH as a final fallback
        |
        v
project-context provider discovery
        |
        +-- OGRE provider
        +-- future SDL provider
        +-- future toolkit providers
        |
        v
verified, version-compatible project actions
```

The v3.8.2 feature is not an OGRE-project flag and does not depend on project ancestry. A hand-authored project qualifies when its current effective configuration supplies sufficient verifiable evidence of a usable toolkit.

---

## 1. Released starting point

The completed v3.8.1 baseline provides:

```text
Code::Blocks OGRE wizard
        |
        +-- exact OGRE v14.6.3 installation
        +-- final macOS framework RPATH
        +-- correct installed plug-in path
        +-- optional exact-version Assimp sidecar patch
        |
        v
~/Library/CodeBlocks/ogre-14.6.3/
        |
        +-- include/OGRE/...
        +-- lib/macosx/Release/...
        +-- bin/macosx/OgreAssimpConverter
        +-- bin/macosx/OgreMeshUpgrader
        +-- bin/macosx/OgreXMLConverter
        +-- bin/macosx/VRMLConverter
        +-- bin/macosx/SampleBrowser.app
```

The generated project already records OGRE through ordinary Code::Blocks configuration, including compiler and linker search directories. The installed tools work, but their location below `~/Library/CodeBlocks/...` is not naturally discoverable by the user.

v3.8.2 must expose the capability without modifying the v3.8.1 installation contract and without inserting the SDK tool directory into the user's global shell `PATH`.

---

## 2. Core architectural decision

The feature is a **project-context tool provider**, not an OGRE-wizard add-on.

A provider:

1. inspects the active project's effective configuration;
2. identifies evidence that a toolkit is in use;
3. derives one or more candidate toolkit roots;
4. verifies the candidates against provider-specific files and executables;
5. resolves ambiguity explicitly;
6. contributes context-sensitive project actions only when a usable result exists;
7. re-evaluates when the active project, target or relevant configuration changes.

The generic provider boundary is:

```text
project configuration
        |
        v
toolkit discovery
        |
        +-- no toolkit detected
        +-- one coherent installation
        +-- installation with some tools unavailable
        +-- multiple plausible installations
        +-- conflicting / incomplete evidence
        |
        v
context actions or actionable diagnostic
```

The provider must not infer toolkit identity merely from the project title, wizard identifier, folder name, source text, or existence of a single vaguely named directory.

---

## 3. Required project behavior

### 3.1 Wizard-generated OGRE project

A v3.8.1-generated project contains search paths such as:

```text
<OGRE_PREFIX>/include/OGRE
<OGRE_PREFIX>/include/OGRE/Bites
<OGRE_PREFIX>/include/OGRE/Overlay
<OGRE_PREFIX>/include/OGRE/RTShaderSystem
```

The OGRE provider may derive:

```text
OGRE_PREFIX=<OGRE_PREFIX>
```

but must verify the derived root before offering actions.

### 3.2 Manually configured OGRE project

A project created without the OGRE wizard must qualify when the active project or target configuration supplies a verifiable OGRE installation. No wizard marker is required.

### 3.3 Multi-toolkit project

Provider discovery is additive. Detecting OGRE must not require OGRE to be the project's exclusive or primary framework. Future SDL or other providers must coexist rather than compete for a single project type.

### 3.4 Per-target variation

Debug and Release targets may inherit different paths or even refer to different toolkit installations. Discovery must operate on the current effective project/target configuration rather than on project-level settings alone. If target configurations resolve to incompatible installations, the ambiguity must be reported rather than silently hidden.

---

## 4. Discovery inputs

Candidate inputs are:

1. compiler include search directories;
2. linker search directories;
3. compiler/linker options containing framework or library paths;
4. linked libraries and macOS frameworks;
5. project and target custom variables;
6. Code::Blocks global variables used by project settings;
7. compiler/toolchain executable search directories, where applicable;
8. environment variables available to the active target;
9. process `PATH`, used only as a final fallback;
10. provider-specific explicit override, if later justified.

The provider must distinguish raw project-file entries from their expanded effective values. Paths containing Code::Blocks macros, environment variables, relative segments or target inheritance must be resolved using existing Code::Blocks expansion behavior rather than by a separate ad hoc parser.

---

## 5. OGRE provider discovery contract

### 5.1 Strong evidence

The primary structural OGRE signal is an effective include path of the form:

```text
<PREFIX>/include/OGRE
```

This deterministically yields candidate `<PREFIX>`. Sibling paths such as:

```text
<PREFIX>/include/OGRE/Bites
<PREFIX>/include/OGRE/Overlay
<PREFIX>/include/OGRE/RTShaderSystem
```

and linker/framework evidence below `<PREFIX>/lib/...` corroborate the candidate and can expose conflicting roots. Optional OGRE components are not required merely to identify an OGRE project.

### 5.2 Candidate-root derivation

```text
<PREFIX>/include/OGRE
        -> candidate <PREFIX>

<PREFIX>/include/OGRE/Bites
        -> candidate <PREFIX>

<PREFIX>/lib/macosx/Release
        -> corroborating candidate <PREFIX>
```

Path normalization must account for relative project paths, Code::Blocks macros, user-home expansion, symbolic links, redundant separators, `.` / `..` components and macOS path layout. Canonicalization must not rewrite user configuration merely to perform discovery.

### 5.3 Candidate verification and capabilities

Toolkit identity and installed tool capability are separate concerns.

A candidate derived from project configuration must first be verified as a coherent OGRE installation, with `<PREFIX>/include/OGRE` as the primary verified structural path. Installed capabilities are then checked individually, including:

```text
<PREFIX>/bin/macosx/OgreAssimpConverter
<PREFIX>/bin/macosx/OgreMeshUpgrader
<PREFIX>/bin/macosx/OgreXMLConverter
<PREFIX>/bin/macosx/VRMLConverter
<PREFIX>/bin/macosx/SampleBrowser.app
```

An OGRE installation that lacks `OgreAssimpConverter` remains an OGRE installation; only the asset-conversion capability is unavailable. The absence of `VRMLConverter` likewise must not suppress other valid capabilities.

### 5.4 Search precedence

```text
1. installation derived from the active target's effective configuration
2. installation derived from effective project-wide configuration
3. explicit project/provider override, if implemented
4. configured Code::Blocks toolkit search directories, if implemented
5. executable found through PATH
```

`PATH` is deliberately last. A globally available converter may belong to a different OGRE version from the SDK used to compile the active project.

### 5.5 Ambiguity and conflict

If several verified installations remain plausible, the provider must not silently select one merely because it appears first. Include evidence resolving to prefix A while OGRE framework evidence resolves to prefix B is an inconsistent or ambiguous configuration and must remain visible.

---

## 6. Configuration changes and invalidation

Discovery must not be treated as a permanent project label.

Relevant changes include active project, active target, build options, search directories, linked libraries/frameworks, custom/global variables, environment or `PATH`, project reload and workspace reopen.

The safest initial implementation is on-demand resolution when the Project menu opens or when a provider action is invoked. Caching may be added only after invalidation rules are established and tested. A stale cached OGRE prefix is unacceptable.

---

## 7. Project-menu integration

Target UX:

```text
Project
    Add files...
    Add files recursively...
    ...
    ------------------------------
    OGRE
        Import 3D asset...
        Tools
            OgreAssimpConverter...
            OgreMeshUpgrader...
            OgreXMLConverter...
            VRMLConverter...
        Open SDK tools folder
        Open SampleBrowser
        Show detected OGRE installation...
    ------------------------------
    Build options...
    Properties...
```

### 7.1 Visibility policy

- no active project: no provider submenu;
- no credible OGRE evidence: no OGRE submenu;
- OGRE detected but installation incomplete: diagnostic entry or disabled submenu with explanation;
- one verified installation: OGRE submenu enabled;
- multiple plausible installations: selection or ambiguity must be exposed;
- OGRE + SDL3: both providers may contribute independently.

### 7.2 Why Project rather than global Tools

The intent is not merely to run an executable. It is to use the toolkit associated with the current project on an asset belonging to that project, with project-appropriate output.

---

## 8. First OGRE action - Import 3D asset

`Project -> OGRE -> Import 3D asset...`

The first implementation should resolve and display the OGRE installation, verify `OgreAssimpConverter`, allow input selection and project-local destination choice, invoke the converter without shell interpolation, capture stdout/stderr/exit status, display results in a dedicated Code::Blocks log tab, validate output beyond `.mesh` existence, avoid modifying source assets, and provide a clear path to generated files.

Conversion success must distinguish process launch failure, non-zero exit, missing mesh, missing material output where expected, fallback/default material symptoms, referenced-resource warnings, and successful mesh/material conversion.

---

## 9. Safe execution boundary

Provider actions must use a native process-launch API with:

```text
verified executable path
explicit argument vector
no arbitrary shell command field
no concatenated shell script
working directory chosen deliberately
captured stdout and stderr
captured exit status
cancellation support where permitted
clear user-visible command summary
```

The existing macOS `macgit.cpp` implementation already proves structured `wxExecute(argv, ...)` with `wxProcess`, `wxExecuteEnv`, redirected streams, actual exit code and cancellation.

---

## 10. Generic provider interface investigation

### 10.1 Project-menu extension seam - LOCATED

Ordinary Code::Blocks plugins contribute to the existing Project menu through `cbPlugin::BuildMenu(wxMenuBar*)`. In-tree precedents establish:

```cpp
int pos = menuBar->FindMenu(_("&Project"));
if (pos != wxNOT_FOUND)
{
    wxMenu* menu = menuBar->GetMenu(pos);
    // Insert(), Append(), InsertSeparator(), AppendSeparator()
}
```

CodeCompletion retains `m_ProjectMenu` and later enables/disables an item. CompilerGCC locates `Properties...` and inserts `Build options...` immediately before it. The future `Project -> OGRE` UI therefore needs no Code::Blocks core-menu modification.

### 10.2 Project/target lifecycle seam - LOCATED

`ProjectManager::SetProject()` updates `m_pActiveProject` and then emits `cbEVT_PROJECT_ACTIVATE`. Plugins subscribe with `Manager::Get()->RegisterEventSink(...)`; the event exposes the active `cbProject*` through `event.GetProject()`.

`cbEVT_PROJECT_OPEN` is distinct: it indicates project load completion and is deferred during workspace loading. It is not synonymous with active-project change.

`cbProject::SetActiveBuildTarget()` updates `m_ActiveTarget` and emits `cbEVT_BUILDTARGET_SELECTED`. Existing DebuggerManager code treats this as an invalidation notification without consuming event payload.

Initial lifecycle contract:

```text
cbEVT_PROJECT_ACTIVATE
        -> re-read current context

cbEVT_BUILDTARGET_SELECTED
        -> re-read current context

cbEVT_PROJECT_CLOSE
        -> re-read / clear current context
```

### 10.3 Effective active-target build configuration - LOCATED

Active objects:

```cpp
cbProject* project =
    Manager::Get()->GetProjectManager()->GetActiveProject();

ProjectBuildTarget* target = project
    ? project->GetBuildTarget(project->GetActiveBuildTarget())
    : nullptr;
```

Project/target settings use `GetIncludeDirs()`, `GetLibDirs()` and `GetLinkLibs()`. Target-parent ordering must honor `GetOptionRelation(ortIncludeDirs)` and `GetOptionRelation(ortLibDirs)`:

```text
orUseParentOptionsOnly   -> project
orUseTargetOptionsOnly   -> target
orPrependToParentOptions -> target + project
orAppendToParentOptions  -> project + target
```

No side-effect-free public ordering helper was found outside protected `CompilerCommandGenerator::GetOrderedOptions()`. v3.8.2 therefore selects a small provider-local helper mirroring these four public semantics.

Selected paths then undergo target-aware `MacrosManager::ReplaceMacros(path, target)` and project-base relative-path resolution.

`Compiler::GetCommandGenerator(project)` / `CompilerCommandGenerator::Init(project)` is explicitly rejected for passive detection because initialization changes the working directory, emits `cbEVT_COMPILER_SET_BUILD_OPTIONS`, executes `SetBuildOptions` build scripts, and prepares broader command-generation state.

### 10.4 Safe process execution and log integration - LOCATED

The existing `macgit.cpp` code already performs:

```cpp
const wxChar* argv[] = { executable.wx_str(), /* args */, nullptr };
m_pid = wxExecute(argv, wxEXEC_ASYNC, m_process, &env);
```

The macOS build uses wxWidgets 3.2.10, whose `wxExecute` supports structured `char**` / `wchar_t**` argv overloads. `wxExecuteEnv` provides explicit working directory and environment.

`wxProcess::Redirect()` plus stdout/stderr streams, `wxEVT_END_PROCESS`, `event.GetExitCode()` and retained PID cancellation provide the required execution lifecycle. SDK `PipedProcess` is useful precedent for stream/event behavior but its current `Launch(const wxString&)` flattens the command and is not the selected launch boundary.

Cccc and CppCheck establish the plugin log-page lifecycle using `TextCtrlLogger`, `LogManager::SetLog()`, `cbEVT_ADD_LOG_WINDOW`, `LogManager::Log()`, `cbEVT_SWITCH_TO_LOG_WINDOW` and `cbEVT_REMOVE_LOG_WINDOW`.

### 10.5 Remaining M1 implementation/lifecycle work

```text
configuration-change notifications beyond project/target selection
plugin lifetime and unload behavior for provider-owned menu/process/log state
minimal runtime proof of the located M1.1-M1.4 seams
```

### 10.6 Toolkit detection from project configuration - LOCATED

M2.1 is concrete: given the effective active-project configuration, determine whether the project uses a toolkit, derive its installation root, verify that installation independently, and report installed-tool capabilities separately from toolkit identity.

The known-good v3.8.1-generated OGRE project is the baseline. Its OGRE configuration is project-level and inherited by both Debug and Release targets. Independent signals converge on:

```text
PREFIX=/Users/thomasbraschler/Library/CodeBlocks/ogre-14.6.3
```

The project contains the four OGRE include paths under that prefix, framework search/RPATH settings under `PREFIX/lib/macosx/Release`, and linked `Ogre.framework`, `OgreBites.framework`, `OgreOverlay.framework` and `OgreRTShaderSystem.framework` binaries under the same prefix.

The installed prefix independently verifies:

```text
PREFIX/include/OGRE
PREFIX/bin/macosx/OgreAssimpConverter
PREFIX/bin/macosx/OgreMeshUpgrader
PREFIX/bin/macosx/OgreXMLConverter
PREFIX/bin/macosx/VRMLConverter
PREFIX/bin/macosx/SampleBrowser.app
```

The detection sequence is therefore:

```text
project configuration says toolkit is in use
        |
        v
derive candidate installation root
        |
        v
verify installation independently
        |
        v
inspect individual installed capabilities
```

Plain outcomes:

```text
no toolkit detected
one coherent toolkit installation detected
one installation detected with some tools unavailable
multiple plausible toolkit installations
conflicting / incomplete toolkit evidence
```

The exact C++ type carrying this information remains an implementation detail. v3.8.2 does not require an abstraction named `ProbeResult`.

---

## 11. Diagnostics and transparency

`Show detected OGRE installation...` should report active project, active target, selected prefix, selection reason, converter path/status and rejected/fallback candidates.

Diagnostics should distinguish no OGRE evidence, derived root with missing headers, OGRE detected but converter absent, converter found only through `PATH`, multiple coherent installations, unresolved macros, target-vs-project differences, and conflicting roots.

---

## 12. Portability and platform boundary

The architecture separates generic current-project configuration collection, candidate ranking, ambiguity handling, menu/action lifecycle and diagnostics from OGRE-specific evidence, root derivation, capability verification, platform-specific executable layouts and actions.

The first release may support only the verified macOS OGRE layout for execution. Unsupported platforms must fail explicitly rather than pretending that a guessed layout is valid.

---

## 13. Expected files / areas

| Area | v3.8.2 role |
|---|---|
| `cbPlugin::BuildMenu(wxMenuBar*)` + Project `wxMenu` | **Located.** `FindMenu(_("&Project"))` + `GetMenu()` expose the existing Project menu. |
| `cbEVT_PROJECT_ACTIVATE`, `cbEVT_BUILDTARGET_SELECTED`, `cbEVT_PROJECT_CLOSE` | **Located.** Explicit context invalidation events. |
| `cbProject`, `ProjectBuildTarget`, `CompileOptionsBase`, `CompileTargetBase` | **Located.** Active target and ordered project/target build configuration. |
| `MacrosManager::ReplaceMacros(path, target)` + project-base normalization | **Located.** Target-aware expansion and relative-path handling. |
| `wxExecute(argv, ...)`, `wxProcess`, `wxExecuteEnv`, `macgit.cpp` | **Located.** Structured launch, explicit env/CWD, capture, exit status, cancellation. |
| `LogManager`, `TextCtrlLogger`, `CodeBlocksLogEvent` | **Located.** Plugin-owned log page lifecycle. |
| OGRE toolkit detection | **Located conceptually.** Structural project evidence -> verified prefix -> independent capabilities. |
| Configuration persistence | Optional override or remembered ambiguity choice, only if justified. |
| Regression fixtures | Synthetic project configurations and legally distributable minimal model fixtures. |

The OGRE scripted wizard remains responsible for installing and configuring OGRE. The provider consumes ordinary project configuration and must not depend on wizard internals.

---

## 14. Milestones

| Milestone | Objective | Check | Status |
|---|---|---|---|
| M1.1 | Locate and prove the Project-menu extension seam. | `BuildMenu()` -> `FindMenu(_("&Project"))` -> `GetMenu()` -> `Insert()` / `Append()` established; minimal runtime submenu test remains. | **IN PROGRESS - SEAM LOCATED** |
| M1.2 | Locate active project/target lifecycle events. | Activation, target selection and project-open distinction established. | **LOCATED** |
| M1.3 | Read effective build configuration without build-time side effects. | Active target lookup, option arrays, `OptionsRelation`, macro expansion and path normalization established; command-generator shortcut rejected. | **LOCATED** |
| M1.4 | Locate safe process/log infrastructure. | `macgit.cpp` proves structured argv execution and Cccc/CppCheck prove log-window lifecycle. | **LOCATED** |
| M2.1 | Define toolkit detection from project configuration. | Structural project evidence derives roots; installations are verified separately; installed tools are independent capabilities; conflicting roots remain visible. | **LOCATED** |
| M2.2 | Implement current-project toolkit detection. | Active project/target produces a fresh detection result from current effective configuration without stale identity. | **NEXT** |
| M2.3 | Implement provider diagnostics. | Selected candidate and evidence are visible. | **PLANNED** |
| M2.4 | Prove multi-provider coexistence. | Synthetic mixed project exposes two independent providers. | **PLANNED** |
| M3.1 | Derive OGRE candidates from include paths. | Wizard-generated and manually configured projects resolve intended prefix. | **PLANNED** |
| M3.2 | Add linker/framework and variable evidence. | Nonstandard coherent configurations can be corroborated and conflicting roots detected. | **PLANNED** |
| M3.3 | Verify macOS OGRE capabilities individually. | Converter, upgrader, XML converter, VRML converter and SampleBrowser reported independently. | **PLANNED** |
| M3.4 | Implement deterministic precedence and ambiguity UI. | Project-derived OGRE wins over unrelated `PATH`; multiple credible roots remain visible. | **PLANNED** |
| M3.5 | Validate configuration-change behavior. | Changing search directories or target changes next detection result. | **PLANNED** |
| M4.1 | Add `Project -> OGRE` submenu. | Visibility/enabled state follows verified context. | **PLANNED** |
| M4.2 | Add installation diagnostic action. | Prefix, evidence, tools and rejected fallbacks visible. | **PLANNED** |
| M4.3 | Add tools-folder and SampleBrowser actions. | Verified local targets open without guessing or global `PATH` changes. | **PLANNED** |
| M5.1 | Implement `Import 3D asset...` dialog. | Input, destination and resolved converter explicit. | **PLANNED** |
| M5.2 | Execute converter safely with logged output. | Paths with spaces work and no shell command is constructed. | **PLANNED** |
| M5.3 | Validate conversion outcome. | Geometry-only false positives and material-reference failures surfaced. | **PLANNED** |
| M5.4 | Validate project-local resource workflow. | Converted output loads without SDK-tree modification. | **PLANNED** |
| M6.1 | Regression-test wizard-generated OGRE project. | v3.8.1 project detected; build/run unchanged. | **PLANNED** |
| M6.2 | Regression-test manual OGRE project. | Detection works without wizard metadata. | **PLANNED** |
| M6.3 | Regression-test SDL3 + OGRE project. | Providers coexist without exclusive project classification. | **PLANNED** |
| M6.4 | Regression-test unrelated projects. | No misleading OGRE menu. | **PLANNED** |
| M6.5 | Review documentation and release boundary. | Rules, precedence and limitations documented. | **PLANNED** |
| M6.6 | Release v3.8.2. | macOS package/source contain validated provider implementation. | **PLANNED** |

---

## 15. Acceptance criteria

v3.8.2 is accepted only if:

1. v3.8.1 OGRE installation/conversion remains the known-good baseline.
2. Toolkit identity derives from current effective project/target configuration, not wizard ancestry.
3. Manual OGRE projects can activate OGRE detection.
4. SDL3 + OGRE projects can activate OGRE without exclusive classification.
5. Multiple providers can coexist.
6. Compiler/linker evidence honors `OptionsRelation`, target-aware macro expansion and project-relative paths without invoking build scripts.
7. Candidate roots are verified before actions are enabled.
8. Project-derived toolkits outrank unrelated `PATH` executables.
9. `PATH` remains a lower-confidence fallback.
10. Multiple credible roots are reported, not silently collapsed.
11. Active-project/target/configuration changes affect the next result.
12. Stale cached toolkit paths cannot survive unnoticed.
13. Individual OGRE tools are exposed only when independently verified.
14. Missing optional tools do not erase otherwise valid OGRE identity.
15. No global shell profile or user `PATH` is modified.
16. No arbitrary shell-command execution is introduced.
17. Tool execution uses an explicit executable and separated arguments.
18. Tool output and exit status are visible in Code::Blocks.
19. Asset import uses the OGRE installation associated with the active project/target.
20. Conversion success is not inferred solely from `.mesh` existence.
21. Material and referenced-resource failures are surfaced.
22. User source assets are not silently modified.
23. Generated output defaults to project-local workflow, not SDK media.
24. `resources.cfg` and project files are not silently rewritten.
25. Ordinary non-OGRE projects receive no misleading OGRE actions.
26. Beginner-facing `main.cpp` remains free of converter machinery.
27. The OGRE wizard does not acquire a private duplicated discovery implementation.
28. Diagnostic information explains selected root and rejected alternatives.
29. macOS v3.8.2 is tested with the v3.8.1 OGRE v14.6.3 baseline.

---

## 16. What stays rejected

- Permanent `isOgreProject` as primary detection.
- Requiring creation by the OGRE wizard.
- Project name, folder name or source scanning as authoritative evidence.
- Exclusive single-toolkit project classification.
- First-path-wins without verification/precedence.
- Global `PATH` converter preferred over active-project SDK.
- Permanent toolkit-root caching without complete invalidation.
- `CompilerCommandGenerator::Init(project)` merely for passive discovery.
- Blind project+target path concatenation ignoring `OptionsRelation`.
- Shell startup-file modification or global SDK-tool `PATH` insertion.
- Hard-coding `~/Library/CodeBlocks/ogre-14.6.3`.
- Shell-string converter execution.
- `.mesh` existence as the definition of conversion success.
- Copying generated assets into installed SDK media.
- Hidden `resources.cfg` / project-settings rewrites.
- Converter machinery in generated `main.cpp`.
- Coupling generic provider core to macOS-specific OGRE paths.
- Expanding v3.8.2 into an asset database, scene editor or package manager.

---

## 17. Stop conditions

Stop and preserve the investigation if the feature would require invasive OGRE hard-wiring into unrelated Code::Blocks core logic; inability to distinguish active-target configuration; unreliable path reconstruction outside Code::Blocks macro facilities; silent toolkit-version mismatch; arbitrary shell execution as the only launch mechanism; project mutation merely to detect a toolkit; exclusive classification that breaks mixed projects; or an opaque cache whose invalidation cannot be made reliable.

If the existing plugin architecture cannot support the complete generic provider in one release, v3.8.2 may narrow execution to a verified OGRE plugin implementation only if the provider boundary remains explicit and no wizard-specific project identity is introduced.

---

## 18. Done when

**Status: IN PROGRESS.** v3.8.2 is complete when Code::Blocks can derive toolkit context from the active project's effective configuration, verify the matching OGRE installation, expose project-specific OGRE actions without requiring wizard ancestry, respond correctly to search-directory and target changes, coexist with other toolkit providers, and safely run the matching `OgreAssimpConverter` through a logged project-local asset-import workflow.

```text
v3.8.1 correct installed OGRE toolchain              DONE
        |
        v
Project-menu/plugin seam                             LOCATED
        |
        v
project/target lifecycle events                      LOCATED
        |
        v
effective active-target configuration                LOCATED
        |
        v
safe process/log infrastructure                      LOCATED
        |
        v
toolkit detection from project configuration         LOCATED
        |
        +-- structural root derivation               CONFIRMED
        +-- independent installation verification    CONFIRMED
        +-- independent tool capabilities            CONFIRMED
        +-- conflicting roots remain visible         SELECTED
        |
        v
implement current-project toolkit detection          NEXT
        |
        v
OGRE candidate derivation + corroboration
        |
        v
Project -> OGRE context submenu
        |
        v
safe converter launch + logged result
        |
        v
project-local resource workflow
        |
        v
regression, documentation and v3.8.2 release
```

