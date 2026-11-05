# Code::Blocks v3.8.2 - OGRE tool integration

**Status: IN PROGRESS - native OgreTools plugin, managed macOS capability discovery, Tools -> OGRE menu, independent initial enablement, SDK-folder action, and SampleBrowser action are proven. On-demand capability refresh is next.**

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
        +-- finite additional supported locations if justified
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
        +-- exact-version OGRE compatibility patch
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

The command-line tools work, but their location below `~/Library/CodeBlocks/...` is not naturally discoverable by the user.

v3.8.2 exposes those capabilities from Code::Blocks without inserting the SDK tool directory into the user's global shell `PATH`.

During v3.8.2 development, launching the installed `SampleBrowser.app` exposed an additional macOS bundle-completeness defect: `Codec_Assimp.framework` was installed under `lib/macosx/Release` but omitted from `SampleBrowser.app/Contents/Frameworks`. Adding `Codec_Assimp` to the existing SampleBrowser `FRAMEWORKS` list fixed the installed application. The correction has been added to the pinned local OGRE compatibility patch and submitted independently upstream.

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
8. revalidate a capability immediately before invoking it;
9. invoke command-line tools safely and present their output in Code::Blocks.

Project context is consulted only by an individual operation when that operation genuinely benefits from it, for example to choose a convenient project-local output directory. Project context is not part of OGRE tool discovery or menu visibility.

The project-context APIs investigated during early v3.8.2 work are preserved separately in `project_context_seam.md`.

---

## 3. Plugin baseline - PROVEN

The native `OgreTools` plugin has been implemented and proven on macOS.

Current source area:

```text
src/plugins/ogretools/
        ogretools.h
        ogretools.cpp
        ogretoolslocator.h
        ogretoolslocator.cpp
        manifest.xml
        Makefile.am
```

Build-system integration is established through:

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

Runtime loading is proven:

```text
Loaded 15 plugins
Loading:
OgreTools
...
```

The former `ProjectTools` name and stale build, staging, and installed artifacts have been removed.

---

## 4. Tool discovery

### 4.1 Managed macOS installation - PROVEN

The wizard's managed-install contract was traced to `ResolveMacCMakePaths()`:

```text
root    = wxGetHomeDir()/Library/CodeBlocks
install = root/ogre-14.6.3
```

The primary macOS tools directory is therefore:

```text
~/Library/CodeBlocks/ogre-14.6.3/bin/macosx
```

`LocateOgreTools()` reconstructs this location directly and verifies capabilities independently.

Runtime discovery has been proven for all five expected v3.8.1 capabilities:

```text
OgreAssimpConverter
OgreMeshUpgrader
OgreXMLConverter
VRMLConverter
SampleBrowser.app
```

Observed runtime paths were:

```text
~/Library/CodeBlocks/ogre-14.6.3/bin/macosx/OgreAssimpConverter
~/Library/CodeBlocks/ogre-14.6.3/bin/macosx/OgreMeshUpgrader
~/Library/CodeBlocks/ogre-14.6.3/bin/macosx/OgreXMLConverter
~/Library/CodeBlocks/ogre-14.6.3/bin/macosx/VRMLConverter
~/Library/CodeBlocks/ogre-14.6.3/bin/macosx/SampleBrowser.app
```

### 4.2 Discovery is capability-based

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

### 4.3 Search precedence

Initial precedence:

```text
1. known Code::Blocks-managed OGRE installation
2. finite explicit set of additional supported locations, only if justified
3. PATH fallback for command-line tools
```

No speculative additional filesystem locations are currently required.

The v3.8.1-managed installation is preferred because it is the installation controlled and tested by the Code::Blocks OGRE workflow.

`PATH` remains planned as a fallback for independently installed command-line tools. It is not a reason to alter the user's shell configuration.

The locator must not perform an unbounded filesystem search.

### 4.4 Verification

Executable actions are enabled only when the resolved executable exists and is executable.

`SampleBrowser.app` is verified as a directory at its platform-specific managed location.

Do not infer capability availability merely because an enclosing OGRE directory exists.

### 4.5 On-demand refresh - NEXT

Capability discovery is cheap and repeatable. Installation state must not be retained as a permanent cache.

At minimum, refresh when:

```text
Tools -> OGRE is opened
```

and revalidate the specific capability immediately before an action is invoked.

This supports the runtime transition:

```text
OGRE absent
    -> tool actions disabled
    -> Install / Repair enabled

OGRE wizard installs or repairs OGRE
    -> next Tools -> OGRE refresh
    -> newly installed tools enabled
```

A Code::Blocks restart must not be required merely to observe tools installed by the OGRE wizard during the current session.

### 4.6 No global environment modification

Discovery must not:

- modify `.zshrc`, `.bashrc`, shell profiles or equivalent;
- permanently alter `PATH`;
- require users to manually expose the Code::Blocks-managed OGRE directory globally.

---

## 5. Tools-menu integration

### 5.1 Menu seam - PROVEN

`OgreTools::BuildMenu(wxMenuBar*)` locates the existing top-level Tools menu and appends an OGRE submenu.

The current menu is:

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

The submenu has been proven visually in the running macOS application.

### 5.2 Why Tools?

OGRE utility availability is an IDE-level capability, not a property of the active project.

`Tools -> OGRE` remains meaningful when:

- no project is open;
- the active project does not use OGRE;
- OGRE is not yet installed;
- the OGRE wizard installs the toolkit during the current session.

Individual operations may still use the active project when useful. For example, `Import 3D asset...` may offer a project-local destination when a project is open. That does not make the plugin or its discovery mechanism project-context dependent.

### 5.3 Initial enabled state - PROVEN

The menu currently derives its initial enabled/disabled state from a fresh `LocateOgreTools()` result during menu construction.

The complete managed v3.8.1 installation enables all verified tool entries.

The remaining requirement is live refresh when the user subsequently opens `Tools -> OGRE`.

Unavailable tools should remain visible but disabled rather than disappearing.

### 5.4 Menu event plumbing - PROVEN

`OgreTools` uses the existing wxWidgets static event-table pattern already used by in-tree Code::Blocks plugins:

```cpp
DECLARE_EVENT_TABLE();

BEGIN_EVENT_TABLE(OgreTools, cbPlugin)
    EVT_MENU(...)
END_EVENT_TABLE()
```

Known-working handlers exist for:

```text
Open SDK tools folder
Open SampleBrowser
```

---

## 6. Working simple actions

### 6.1 Open SDK tools folder - DONE

The handler freshly calls `LocateOgreTools()` and opens the verified tools directory with `wxLaunchDefaultApplication()`.

On the current macOS baseline this opens:

```text
~/Library/CodeBlocks/ogre-14.6.3/bin/macosx
```

The behavior has been tested successfully in the running application.

### 6.2 Open SampleBrowser - DONE, with bundle correction

The handler freshly calls `LocateOgreTools()` and launches the verified `SampleBrowser.app` with `wxLaunchDefaultApplication()`.

Initial testing exposed an OGRE-side packaging defect rather than an OgreTools launch defect.

Direct terminal launch showed:

```text
SampleBrowser.app/Contents/Frameworks/Codec_Assimp.framework/Codec_Assimp
```

was required but missing from the application bundle.

The installed framework itself existed at:

```text
~/Library/CodeBlocks/ogre-14.6.3/lib/macosx/Release/Codec_Assimp.framework
```

Controlled A/B testing established:

```text
installed SampleBrowser without bundled Codec_Assimp.framework   FAIL
manually add the installed framework to Contents/Frameworks      PASS
```

The upstream OGRE source cause was then located in:

```text
Samples/Browser/CMakeLists.txt
```

where `Codec_Assimp` was omitted from the existing macOS `FRAMEWORKS` list. The loop already contains:

```cmake
if(TARGET ${FWK})
```

so adding `Codec_Assimp` preserves builds in which the Assimp codec target does not exist.

The one-line correction is now included in the pinned local OGRE compatibility patch. It has also been submitted upstream as a separate focused pull request. Upstream disposition is external and does not block v3.8.2.

The OgreTools SampleBrowser launch code itself remains simple and unchanged.

---

## 7. Installation boundary

The existing OGRE wizard owns the tested OGRE installation procedure unless implementation work identifies a cleaner reusable installation component.

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

After successful installation or repair, the next menu refresh enables tools that now verify successfully without restarting Code::Blocks.

---

## 8. First high-level workflow: Import 3D asset

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

## 9. Safe execution boundary

Command-line tool execution uses:

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

`wxProcess::Redirect()`, redirected streams, `wxEVT_END_PROCESS`, and `event.GetExitCode()` provide the process lifecycle.

SDK `PipedProcess::Launch(const wxString&)` remains rejected for this boundary because it flattens the invocation into a command string.

---

## 10. Logging

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

Converter output, warnings, exit state, and validation results should be reported there.

---

## 11. Conversion validation

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

## 12. Project-local resource workflow

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

## 13. Portability boundary

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

## 14. Preserved project-context seam

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

## 15. Expected files / areas

| Area | v3.8.2 role |
|---|---|
| `src/plugins/ogretools/` | Native plugin implementation. |
| `ogretools.cpp/.h` | Plugin lifecycle, `Tools -> OGRE`, command handlers, and refresh trigger. |
| `ogretoolslocator.cpp/.h` | Current capability discovery and verification. |
| `cbPlugin::BuildMenu(wxMenuBar*)` | Adds the `Tools -> OGRE` submenu. |
| wx menu-open/update handling | Refreshes capability state on demand. |
| `wxLaunchDefaultApplication()` | Opens the verified SDK tools directory and SampleBrowser application. |
| `wxExecute(argv, ...)`, `wxProcess`, `wxExecuteEnv` | Safe command-line tool execution. |
| `LogManager`, `TextCtrlLogger`, `CodeBlocksLogEvent` | Dedicated OGRE tool output. |
| Existing OGRE wizard/install logic | Installation or repair mechanism to reuse or cleanly factor rather than duplicate. |
| Exact-version OGRE compatibility patch | Referenced-resource correction plus macOS SampleBrowser Assimp bundle correction. |
| `project_context_seam.md` | Preserved deferred investigation. |
| Regression fixtures | Minimal legally distributable conversion inputs and expected outputs. |

---

## 16. Milestones

| Milestone | Objective | Check | Status |
|---|---|---|---|
| M1.1 | Create native OgreTools plugin. | Plugin builds and packages through the macOS build. | **DONE** |
| M1.2 | Prove and cleanly rename plugin loading. | `OgreTools` appears among 15 loaded plugins; obsolete `ProjectTools` artifacts are absent. | **DONE** |
| M1.3 | Preserve abandoned project-context investigation. | Useful seams extracted to `project_context_seam.md`. | **DONE** |
| M2.1 | Define OGRE capability locator. | Managed installation contract, capability identities, verification rules, and precedence are defined. | **DONE** |
| M2.2 | Discover verified macOS OGRE tools. | Managed v3.8.1 installation reports all five expected capabilities independently at runtime. | **DONE** |
| M2.3 | Add PATH fallback. | PATH-only command-line tool can be detected without modifying the user environment. | **PLANNED** |
| M2.4 | Implement on-demand capability refresh. | Opening `Tools -> OGRE` rechecks availability; individual actions revalidate before execution; wizard-installed tools become available without restart. | **DONE** |
| M3.1 | Add `Tools -> OGRE` submenu. | Submenu is visible independently of project state. | **DONE** |
| M3.2 | Enable/disable individual actions. | Initial state follows independent capability discovery; live refresh remains covered by M2.4. | **DONE - INITIAL STATE** |
| M3.3 | Add install/repair action. | Missing or incomplete managed installation remains recoverable from `Tools -> OGRE`. | **PLANNED** |
| M3.4 | Add tools-folder and SampleBrowser actions. | Verified local targets open without global PATH changes; SampleBrowser runs with corrected installed bundle. | **DONE** |
| M3.5 | Correct SampleBrowser Assimp bundle. | `Codec_Assimp.framework` is bundled when the `Codec_Assimp` CMake target exists; installed SampleBrowser runs. | **DONE LOCALLY / UPSTREAM PR OPEN** |
| M4.1 | Implement `Import 3D asset...` dialog. | Input, destination, and freshly resolved converter are explicit. | **PLANNED** |
| M4.2 | Execute converter safely. | Explicit argv works with paths containing spaces. | **PLANNED** |
| M4.3 | Add dedicated OGRE log. | stdout, stderr, and actual exit status are visible. | **PLANNED** |
| M4.4 | Validate converter outcome. | Geometry-only and material/resource failures are surfaced. | **PLANNED** |
| M4.5 | Validate output workflow. | Project-local output is offered when appropriate; user assets remain outside SDK media and source files remain untouched. | **PLANNED** |
| M5.1 | Regression-test complete managed OGRE installation. | All expected actions become available. | **PLANNED** |
| M5.2 | Regression-test partial installation. | Individual missing tools produce individual disabled actions. | **PLANNED** |
| M5.3 | Regression-test no OGRE installation. | Tool actions are disabled and install/repair remains available. | **PLANNED** |
| M5.4 | Regression-test install-during-session refresh. | OGRE wizard installation becomes visible on the next refresh without restarting Code::Blocks. | **PLANNED** |
| M5.5 | Regression-test PATH fallback. | Supported PATH discovery works without environment modification. | **PLANNED** |
| M5.6 | Regression-test no-project and ordinary-project states. | `Tools -> OGRE` remains usable without classifying or mutating projects. | **PLANNED** |
| M5.7 | Review documentation and release boundary. | Discovery, refresh, execution, local OGRE compatibility corrections, and macOS limitations are documented. | **PLANNED** |
| M5.8 | Release v3.8.2. | Signed macOS package contains the validated OgreTools implementation. | **PLANNED** |

---

## 17. Acceptance criteria

v3.8.2 is accepted only if:

1. The pinned OGRE v14.6.3 installation remains the known-good managed baseline.
2. `OgreTools` builds, packages, installs, and loads as a native Code::Blocks plugin.
3. The plugin appears as `OgreTools`; obsolete `ProjectTools` build, staging, and installed artifacts are absent.
4. `Tools -> OGRE` is available independently of whether a project is open or uses OGRE.
5. The known Code::Blocks-managed OGRE tools directory is discovered.
6. The managed prefix is reconstructed consistently with the wizard's `~/Library/CodeBlocks/ogre-14.6.3` contract.
7. Any additional fixed candidate locations are explicit and finite.
8. `PATH` may be used only as a fallback for suitable command-line tools.
9. No shell profile or global `PATH` is modified.
10. Each OGRE capability is verified independently.
11. One missing optional tool does not disable unrelated tools.
12. Missing tools remain visible but disabled rather than silently disappearing.
13. An install/repair action remains available when the expected managed installation is absent or incomplete.
14. Opening `Tools -> OGRE` performs a fresh capability check.
15. Invoking an action revalidates its required capability before execution.
16. After the OGRE wizard installs or repairs OGRE, the next capability refresh reflects newly installed tools without restarting Code::Blocks.
17. `Open SDK tools folder` opens the freshly verified managed tools directory.
18. `Open SampleBrowser` launches the freshly verified application bundle.
19. The managed macOS SampleBrowser bundle contains `Codec_Assimp.framework` when the codec target is built.
20. The SampleBrowser bundle correction remains exact-version/local until an adopted upstream OGRE release makes it unnecessary.
21. The existing OGRE installation implementation is reused or cleanly factored rather than substantially duplicated.
22. No arbitrary shell-command execution is introduced.
23. Command-line executable paths and arguments remain separate.
24. Paths containing spaces work.
25. stdout and stderr are captured for command-line tools.
26. Actual process exit status is captured.
27. Conversion results are visible in Code::Blocks.
28. Conversion success is not inferred solely from `.mesh` existence.
29. Material and referenced-resource failures are surfaced where detectable.
30. User source assets are not silently modified.
31. Generated output does not default into the installed OGRE SDK media tree.
32. When an active project is suitable, a project-local output destination may be offered without affecting tool discovery.
33. When no project is active, OGRE tools that do not require project context remain usable.
34. `resources.cfg` is not silently rewritten.
35. Project files are not silently rewritten.
36. Beginner-facing generated `main.cpp` remains free of converter machinery.
37. Ordinary projects are not assigned an OGRE identity merely because OGRE tools are installed.
38. The deferred project-context mechanism is not reintroduced into tool discovery without a specific requirement.
39. macOS v3.8.2 is regression-tested against the pinned OGRE v14.6.3 baseline.
40. The final application bundle passes the established macOS packaging and code-signing workflow.

---

## 18. What stays rejected

- Permanent `isOgreProject`.
- Requiring creation by the OGRE wizard merely to expose tools.
- Parsing active build configuration merely to decide whether the OGRE menu exists.
- Project-name, directory-name, or source scanning for menu visibility.
- Putting globally available OGRE utilities behind project classification.
- Global shell-startup modification.
- Permanent insertion of the OGRE tools directory into user `PATH`.
- Unbounded filesystem searches for OGRE installations.
- Selecting an executable without verifying it exists.
- Caching tool availability indefinitely.
- Requiring a Code::Blocks restart after the wizard installs OGRE merely to refresh tool availability.
- One missing tool disabling the complete OGRE feature.
- Patching SampleBrowser opportunistically at launch time inside OgreTools.
- Duplicating the entire OGRE installation implementation in `OgreTools`.
- Shell-string converter execution.
- Arbitrary user-entered command execution.
- `.mesh` existence as the sole definition of conversion success.
- Copying generated user assets into installed SDK media.
- Hidden `resources.cfg` or project-setting rewrites.
- Converter machinery in generated `main.cpp`.
- Reintroducing project-context complexity merely because the seam is available.
- Expanding v3.8.2 into an asset database, package manager, or scene editor.

---

## 19. Stop conditions

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

## 20. Current critical path

```text
managed macOS locator                            DONE
        |
        +-- all five expected capabilities       DONE
        +-- initial menu enablement               DONE
        +-- SDK tools folder action               DONE
        +-- SampleBrowser action                  DONE
        +-- SampleBrowser bundle correction       DONE LOCALLY
        |
        v
on-demand menu refresh                           NEXT
        |
        v
PATH fallback
        |
        v
install / repair bridge
        |
        v
command-line tool handlers
        |
        v
Import 3D asset dialog
        |
        v
safe converter process + OGRE log
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

---

## 21. Done when

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
managed capability locator                       DONE
        |
        v
Tools -> OGRE submenu                            DONE
        |
        +-- initial capability enablement         DONE
        +-- tools folder                          DONE
        +-- SampleBrowser                         DONE
        +-- SampleBrowser Assimp bundle fix       DONE LOCALLY / UPSTREAM PR OPEN
        |
        v
fresh menu-open capability refresh               NEXT
        |
        v
PATH fallback + installation bridge
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

