# Code::Blocks v3.8.2 - OGRE tool integration

**Status: IN PROGRESS - native OgreTools plugin builds, packages, installs and loads successfully; installed-tool discovery and Tools-menu integration are next.**

v3.8.2 follows the released v3.8.1 OGRE baseline and makes the OGRE tools installed by the Code::Blocks OGRE workflow discoverable and usable directly from the IDE.

**Question this version answers:** how can Code::Blocks expose installed OGRE utilities such as `OgreAssimpConverter` without requiring users to know their filesystem locations or modify their shell `PATH`?

The feature does **not** need to determine whether the active project is an OGRE project. OGRE utilities are IDE-level tools. `OgreTools` exposes them under the global `Tools` menu, while individual actions are enabled only when their required executable or application is currently available.

```text
OgreTools plugin
        |
        v
fresh capability discovery
        |
        +-- Code::Blocks-managed OGRE installation
        +-- finite additional supported locations
        +-- PATH fallback
        |
        v
verify each capability independently
        |
        +-- OgreAssimpConverter
        +-- OgreMeshUpgrader
        +-- OgreXMLConverter
        +-- VRMLConverter
        +-- SampleBrowser
        |
        v
Tools -> OGRE
        |
        +-- available actions enabled
        +-- unavailable actions disabled
        +-- install / repair available when needed
```

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

The tools work, but their location below `~/Library/CodeBlocks/...` is not naturally discoverable by the user.

v3.8.2 exposes those capabilities from Code::Blocks without changing the v3.8.1 installation contract and without inserting the SDK tool directory into the user's global shell `PATH`.

---

## 2. Core architectural decision

The feature is an ordinary native Code::Blocks plugin named `OgreTools`.

It does **not** classify projects.

It does **not** infer OGRE use from compiler or linker settings.

It does **not** depend on creation by the OGRE wizard.

Its immediate responsibilities are:

1. expose `Tools -> OGRE`;
2. locate supported installed OGRE utilities;
3. verify each utility independently;
4. enable actions whose requirements are satisfied;
5. disable actions whose requirements are missing;
6. offer installation or repair when the expected Code::Blocks-managed tools are unavailable;
7. refresh capability state on demand so an OGRE installation performed during the current Code::Blocks session becomes visible;
8. invoke tools safely and present their output in Code::Blocks.

Project context is consulted only by an individual operation when that operation genuinely benefits from it, for example to choose a convenient project-local output directory. Project context is not part of OGRE tool discovery or menu visibility.

The project-context APIs investigated during early v3.8.2 work are preserved separately in `project_context_seam.md`.

---

## 3. Plugin baseline

A minimal native `OgreTools` plugin has already been implemented and proven on macOS.

Current source area:

```text
src/plugins/ogretools/
        ogretools.h
        ogretools.cpp
        manifest.xml
        Makefile.am
```

Build-system integration has been established through:

```text
configure.ac
m4/acinclude.m4
src/plugins/Makefile.am
src/plugins/ogretools/Makefile.am
```

The build option and Automake identities are:

```text
--enable-ogretools
BUILD_OGRETOOLS
MAYBE_OGRETOOLS
libogretools.la
```

The macOS plugin package installs as:

```text
share/codeblocks/plugins/libogretools.dylib
share/codeblocks/ogretools.zip
```

The established macOS bundle workflow additionally places the required dynamic library in the application Frameworks area and performs the normal code-signing step.

Runtime loading has been proven:

```text
Loaded 15 plugins
Loading:
OgreTools
...
```

The former `ProjectTools` name and its stale installed artifacts have been removed. Basic plugin build, packaging, discovery and registration are therefore complete.

---

## 4. Tool discovery

### 4.1 Discovery is capability-based

Each supported tool is discovered independently.

For the verified macOS v3.8.1 installation:

```text
~/Library/CodeBlocks/ogre-14.6.3/bin/macosx/OgreAssimpConverter
~/Library/CodeBlocks/ogre-14.6.3/bin/macosx/OgreMeshUpgrader
~/Library/CodeBlocks/ogre-14.6.3/bin/macosx/OgreXMLConverter
~/Library/CodeBlocks/ogre-14.6.3/bin/macosx/VRMLConverter
~/Library/CodeBlocks/ogre-14.6.3/bin/macosx/SampleBrowser.app
```

A missing tool affects only its own capability.

For example:

```text
OgreAssimpConverter    present
OgreMeshUpgrader       present
OgreXMLConverter       present
VRMLConverter          absent
SampleBrowser          present
```

produces four available capabilities and one unavailable capability. It does not invalidate the other tools.

### 4.2 Search precedence

Initial macOS precedence:

```text
1. known Code::Blocks-managed OGRE installation
2. finite explicit set of additional supported locations, if justified
3. PATH fallback
```

The v3.8.1-managed installation is preferred because it is the installation controlled and tested by the Code::Blocks OGRE workflow.

`PATH` is a fallback, not a reason to alter the user's shell configuration.

The locator must not perform an unbounded filesystem search.

### 4.3 Verification

Executable actions are enabled only when the resolved executable exists and is executable.

Applications such as:

```text
SampleBrowser.app
```

are verified according to their platform-specific representation.

Do not infer capability availability merely because an enclosing OGRE directory exists.

### 4.4 On-demand refresh

Capability discovery must be cheap and repeatable. Do not retain installation state as a permanent cache.

At minimum, refresh when:

```text
Tools -> OGRE is opened
```

and revalidate the specific capability immediately before an action is invoked.

This supports the important runtime transition:

```text
OGRE absent
    -> tool actions disabled
    -> Install / Repair enabled

OGRE wizard installs or repairs OGRE
    -> next capability refresh
    -> newly installed tools enabled
```

A Code::Blocks restart must not be required merely to observe tools installed by the OGRE wizard during the current session.

### 4.5 No global environment modification

Discovery must not:

- modify `.zshrc`, `.bashrc`, shell profiles or equivalent;
- permanently alter `PATH`;
- require users to manually expose the Code::Blocks-managed OGRE directory globally.

---

## 5. Tools-menu integration

Target UX:

```text
Tools
    ...
    OGRE
        Import 3D asset...
        --------------------------
        OgreAssimpConverter...
        OgreMeshUpgrader...
        OgreXMLConverter...
        VRMLConverter...
        --------------------------
        Open SDK tools folder
        Open SampleBrowser
        --------------------------
        Install / Repair OGRE tools...
```

Exact wording and grouping may change as the implementation settles.

### 5.1 Why Tools?

OGRE utility availability is an IDE-level capability, not a property of the active project.

`Tools -> OGRE` therefore remains meaningful when:

- no project is open;
- the active project does not use OGRE;
- OGRE is not yet installed;
- the OGRE wizard installs the toolkit during the current session.

Individual operations may still use the active project when useful. For example, `Import 3D asset...` may offer a project-local destination when a project is open. That does not make the plugin or its discovery mechanism project-context dependent.

### 5.2 Visibility and enabled state

The `OGRE` submenu remains visible while the `OgreTools` plugin is active.

Opening the submenu triggers fresh capability discovery.

Individual actions follow current capability availability:

```text
tool found       -> corresponding action enabled
tool unavailable -> corresponding action disabled
```

`Install / Repair OGRE tools...` remains available when the expected Code::Blocks-managed installation is missing or incomplete.

Unavailable tools should remain visible rather than disappearing, so the user can see which capabilities exist conceptually and why an installation action is relevant.

---

## 6. Installation boundary

The existing OGRE wizard owns the tested v3.8.1 OGRE installation procedure unless implementation work identifies a cleaner reusable installation component.

v3.8.2 should not substantially duplicate that installation logic inside `OgreTools`.

The installation action therefore needs a deliberately defined bridge to the existing installer or a shared installation component factored from it.

Required outcome:

```text
Tools -> OGRE -> Install / Repair OGRE tools...
        |
        v
existing or shared OGRE installation mechanism
        |
        v
~/Library/CodeBlocks/ogre-14.6.3/
        |
        v
fresh OgreTools capability discovery
```

After successful installation or repair, the next refresh enables tools that now verify successfully without restarting Code::Blocks.

---

## 7. First workflow: Import 3D asset

`Tools -> OGRE -> Import 3D asset...`

The first high-level action uses `OgreAssimpConverter`.

It should:

1. freshly verify the converter;
2. allow selection of an input asset;
3. select or confirm a destination;
4. show the converter being used;
5. invoke it without shell interpolation;
6. capture stdout and stderr;
7. capture the actual exit status;
8. show results in a dedicated Code::Blocks log;
9. inspect the expected outputs;
10. distinguish partial conversion from genuine success;
11. avoid modifying the user's original source asset.

When an active project exists, the dialog may offer a convenient project-local destination. Converter availability itself does not depend on the project.

When no project is active, the importer may request an explicit destination rather than disabling the complete OGRE tools menu.

---

## 8. Safe execution boundary

Tool execution uses:

```text
verified executable
explicit argument vector
no shell interpolation
deliberately selected working directory
captured stdout
captured stderr
captured exit status
cancellation where appropriate
visible command summary
```

Existing `macgit.cpp` code proves the required wxWidgets mechanism:

```cpp
const wxChar* argv[] = {
    executable.wx_str(),
    /* arguments */
    nullptr
};

m_pid = wxExecute(argv, wxEXEC_ASYNC, m_process, &env);
```

`wxExecuteEnv` supplies explicit environment and working-directory handling.

`wxProcess::Redirect()`, redirected streams, `wxEVT_END_PROCESS` and `event.GetExitCode()` provide the process lifecycle.

SDK `PipedProcess::Launch(const wxString&)` remains rejected for this boundary because it flattens the invocation into a command string.

---

## 9. Logging

Cccc and CppCheck establish the native Code::Blocks log-window lifecycle using:

```text
TextCtrlLogger
LogManager::SetLog()
cbEVT_ADD_LOG_WINDOW
LogManager::Log()
cbEVT_SWITCH_TO_LOG_WINDOW
cbEVT_REMOVE_LOG_WINDOW
```

`OgreTools` should own a dedicated log such as:

```text
OGRE Tools
```

Converter output, warnings, exit state and validation results should be reported there.

---

## 10. Conversion validation

Conversion success is **not** defined as:

```text
a .mesh file exists
```

The importer should distinguish at least:

```text
process could not be launched
process returned non-zero
expected mesh absent
mesh generated
expected material output absent
fallback/default material symptoms
referenced-resource warnings
successful mesh/material conversion
```

The exact validation logic should be based on observed `OgreAssimpConverter` behavior and regression fixtures rather than assumptions about every source format.

---

## 11. Project-local resource workflow

The importer may use project context for destination convenience without making tool discovery project dependent.

The default workflow should:

- leave source assets unchanged;
- generate output outside the installed OGRE SDK tree;
- prefer a project-local destination when a suitable active project exists;
- allow an explicit destination when no project is active;
- clearly show the resulting files;
- avoid silently modifying `resources.cfg`;
- avoid silently modifying `.cbp` files.

The installed OGRE SDK remains software infrastructure, not a destination for user assets.

---

## 12. Portability boundary

The first implementation may support only the verified macOS layout.

Platform-specific discovery belongs behind a small explicit boundary:

```text
generic capability
        |
        +-- OgreAssimpConverter
        +-- OgreMeshUpgrader
        +-- OgreXMLConverter
        +-- VRMLConverter
        +-- SampleBrowser
        |
        v
platform-specific locator
```

Unsupported platforms must not receive guessed filesystem paths presented as verified installations.

Windows and Linux discovery can be added when their actual installed layouts and packaging behavior are established.

---

## 13. Preserved project-context seam

The earlier investigation established useful Code::Blocks APIs for a future feature that genuinely depends on the active project:

```text
cbEVT_PROJECT_ACTIVATE
cbEVT_BUILDTARGET_SELECTED
cbEVT_PROJECT_CLOSE

cbProject
ProjectBuildTarget

GetIncludeDirs()
GetLibDirs()
GetLinkLibs()

OptionsRelation

MacrosManager::ReplaceMacros(value, target)
```

It also established why:

```cpp
CompilerCommandGenerator::Init(project)
```

must not be used merely for passive configuration inspection.

These results are preserved in:

```text
project_context_seam.md
```

They are **not part of the v3.8.2 tool-discovery path**. Individual operations may use narrowly scoped project context when useful, such as determining a suggested output directory.

---

## 14. Expected files / areas

| Area | v3.8.2 role |
|---|---|
| `src/plugins/ogretools/` | Native plugin implementation. |
| `ogretools.cpp/.h` | Plugin lifecycle, `Tools -> OGRE`, commands and refresh trigger. |
| OGRE tool locator | Finds and independently verifies supported OGRE capabilities. |
| `cbPlugin::BuildMenu(wxMenuBar*)` | Adds the `Tools -> OGRE` submenu. |
| Menu-open/update handling | Refreshes capability state on demand. |
| `wxExecute(argv, ...)`, `wxProcess`, `wxExecuteEnv` | Safe tool execution. |
| `LogManager`, `TextCtrlLogger`, `CodeBlocksLogEvent` | Dedicated OGRE tool output. |
| Existing OGRE wizard/install logic | Installation or repair mechanism to reuse or cleanly factor rather than duplicate. |
| `project_context_seam.md` | Preserved deferred investigation. |
| Regression fixtures | Minimal legally distributable conversion inputs and expected outputs. |

---

## 15. Milestones

| Milestone | Objective | Check | Status |
|---|---|---|---|
| M1.1 | Create native OgreTools plugin. | Plugin builds and packages through the macOS build. | **DONE** |
| M1.2 | Prove and cleanly rename plugin loading. | `OgreTools` appears among 15 loaded plugins; obsolete `ProjectTools` artifacts are removed. | **DONE** |
| M1.3 | Preserve abandoned project-context investigation. | Useful seams extracted to `project_context_seam.md`. | **DONE** |
| M2.1 | Define OGRE capability locator. | Tool identities, finite candidate locations, verification rules and precedence are defined. | **NEXT** |
| M2.2 | Discover verified macOS OGRE tools. | v3.8.1 installation reports each tool independently. | **PLANNED** |
| M2.3 | Add PATH fallback. | PATH-only executable can be detected without modifying the user environment. | **PLANNED** |
| M2.4 | Implement on-demand capability refresh. | Opening `Tools -> OGRE` and invoking an action recheck availability; wizard-installed tools become available without restart. | **PLANNED** |
| M3.1 | Add `Tools -> OGRE` submenu. | Submenu is available independently of project state and refreshes capabilities when opened. | **PLANNED** |
| M3.2 | Enable/disable individual actions. | Missing tool disables only its corresponding action. | **PLANNED** |
| M3.3 | Add install/repair action. | Missing or incomplete managed installation remains recoverable from `Tools -> OGRE`. | **PLANNED** |
| M3.4 | Add tools-folder and SampleBrowser actions. | Verified local targets open without global PATH changes. | **PLANNED** |
| M4.1 | Implement `Import 3D asset...` dialog. | Input, destination and freshly resolved converter are explicit. | **PLANNED** |
| M4.2 | Execute converter safely. | Explicit argv works with paths containing spaces. | **PLANNED** |
| M4.3 | Add dedicated OGRE log. | stdout, stderr and actual exit status are visible. | **PLANNED** |
| M4.4 | Validate converter outcome. | Geometry-only and material/resource failures are surfaced. | **PLANNED** |
| M4.5 | Validate output workflow. | Project-local output is offered when appropriate; user assets remain outside SDK media and source files remain untouched. | **PLANNED** |
| M5.1 | Regression-test complete v3.8.1 OGRE installation. | All expected actions become available. | **PLANNED** |
| M5.2 | Regression-test partial installation. | Individual missing tools produce individual disabled actions. | **PLANNED** |
| M5.3 | Regression-test no OGRE installation. | Tool actions are disabled and install/repair remains available. | **PLANNED** |
| M5.4 | Regression-test install-during-session refresh. | OGRE wizard installation becomes visible on the next refresh without restarting Code::Blocks. | **PLANNED** |
| M5.5 | Regression-test PATH fallback. | Supported PATH discovery works without environment modification. | **PLANNED** |
| M5.6 | Regression-test no-project and ordinary-project states. | `Tools -> OGRE` remains usable without classifying or mutating projects. | **PLANNED** |
| M5.7 | Review documentation and release boundary. | Discovery, refresh, execution and macOS limitations are documented. | **PLANNED** |
| M5.8 | Release v3.8.2. | Signed macOS package contains the validated OgreTools implementation. | **PLANNED** |

---

## 16. Acceptance criteria

v3.8.2 is accepted only if:

1. The v3.8.1 OGRE v14.6.3 installation remains the known-good baseline.
2. `OgreTools` builds, packages, installs and loads as a native Code::Blocks plugin.
3. The plugin appears as `OgreTools`; obsolete `ProjectTools` build, staging and installed artifacts are absent.
4. `Tools -> OGRE` is available independently of whether a project is open or uses OGRE.
5. The known Code::Blocks-managed OGRE tool directory is discovered.
6. Any additional candidate locations are explicit and finite.
7. `PATH` may be used as a fallback.
8. No shell profile or global `PATH` is modified.
9. Each OGRE capability is verified independently.
10. One missing optional tool does not disable unrelated tools.
11. Missing tools remain visible but disabled rather than silently disappearing.
12. An install/repair action remains available when the expected managed installation is absent or incomplete.
13. Opening `Tools -> OGRE` performs a fresh capability check.
14. Invoking an action revalidates its required capability before execution.
15. After the OGRE wizard installs or repairs OGRE, the next capability refresh reflects the newly installed tools without restarting Code::Blocks.
16. The existing OGRE installation implementation is reused or cleanly factored rather than substantially duplicated.
17. No arbitrary shell-command execution is introduced.
18. Executable paths and arguments remain separate.
19. Paths containing spaces work.
20. stdout and stderr are captured.
21. Actual process exit status is captured.
22. Conversion results are visible in Code::Blocks.
23. Conversion success is not inferred solely from `.mesh` existence.
24. Material and referenced-resource failures are surfaced where detectable.
25. User source assets are not silently modified.
26. Generated output does not default into the installed OGRE SDK media tree.
27. When an active project is suitable, a project-local output destination may be offered without affecting tool discovery.
28. When no project is active, OGRE tools that do not require project context remain usable.
29. `resources.cfg` is not silently rewritten.
30. Project files are not silently rewritten.
31. Beginner-facing generated `main.cpp` remains free of converter machinery.
32. Ordinary projects are not assigned an OGRE identity merely because OGRE tools are installed.
33. The deferred project-context mechanism is not reintroduced into tool discovery without a specific requirement.
34. macOS v3.8.2 is regression-tested against the v3.8.1 OGRE v14.6.3 baseline.
35. The final application bundle passes the established macOS packaging and code-signing workflow.

---

## 17. What stays rejected

- Permanent `isOgreProject`.
- Requiring creation by the OGRE wizard merely to expose tools.
- Parsing active build configuration merely to decide whether the OGRE menu exists.
- Project-name, directory-name or source scanning for menu visibility.
- Putting globally available OGRE utilities behind project classification.
- Global shell-startup modification.
- Permanent insertion of the OGRE tool directory into user `PATH`.
- Unbounded filesystem searches for OGRE installations.
- Selecting an executable without verifying it exists.
- Caching tool availability indefinitely.
- Requiring a Code::Blocks restart after the wizard installs OGRE merely to refresh tool availability.
- One missing tool disabling the complete OGRE feature.
- Duplicating the entire OGRE installation implementation in `OgreTools`.
- Shell-string converter execution.
- Arbitrary user-entered command execution.
- `.mesh` existence as the sole definition of conversion success.
- Copying generated user assets into installed SDK media.
- Hidden `resources.cfg` or project-setting rewrites.
- Converter machinery in generated `main.cpp`.
- Reintroducing project-context complexity merely because the seam is available.
- Expanding v3.8.2 into an asset database, package manager or scene editor.

---

## 18. Stop conditions

Stop and preserve the implementation if:

- OGRE tools cannot be located without an unbounded filesystem search;
- tool execution requires arbitrary shell interpolation;
- installation requires uncontrolled modification of the user's shell environment;
- the only way to reuse the existing OGRE installer is to duplicate significant wizard implementation;
- converter execution cannot reliably expose exit status and output;
- the importer must modify source assets or SDK media to function;
- platform-specific assumptions cannot be isolated explicitly.

Project-context detection is **not** a stop condition. It is simply outside tool discovery. A narrowly scoped operation may still consult the active project when that operation genuinely benefits from it.

---

## 19. Done when

**Status: IN PROGRESS.**

v3.8.2 is complete when Code::Blocks exposes the OGRE utilities installed by its OGRE ecosystem through the native `OgreTools` plugin under `Tools -> OGRE`, verifies and refreshes individual capabilities independently, provides recovery when tools are unavailable, and safely runs `OgreAssimpConverter` through a logged asset-import workflow without requiring project classification or global `PATH` modification.

```text
v3.8.1 correct installed OGRE toolchain          DONE
        |
        v
native OgreTools plugin                          DONE
        |
        v
plugin rename / packaging / loading              DONE
        |
        v
project-context investigation                    PRESERVED + DEFERRED
        |
        v
OGRE capability locator                          NEXT
        |
        +-- Code::Blocks-managed installation
        +-- finite candidate locations
        +-- PATH fallback
        +-- independent capability checks
        +-- on-demand refresh
        |
        v
Tools -> OGRE submenu
        |
        +-- refresh on menu open
        +-- available tools enabled
        +-- missing tools disabled
        +-- install / repair available
        |
        v
safe converter execution + OGRE log
        |
        v
conversion validation
        |
        v
project-local output when useful
        |
        v
regression + signed v3.8.2 release
```

