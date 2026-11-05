# Code::Blocks v3.8.2 - OGRE tool integration

**Status: IN PROGRESS - native OgreTools plugin, managed macOS capability discovery, PATH fallback, live `Tools -> OGRE` refresh, SDK-folder action, and SampleBrowser action are proven. The install/repair architecture is now resolved in technical detail: the existing `ogre/wizard.script` will serve both the existing project wizard and a new setup-only custom wizard registration; implementing that bridge is next.**

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
        +-- PATH fallback for command-line tools
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
        +-- refresh whenever submenu opens
        +-- available actions enabled
        +-- unavailable actions disabled
        +-- action-time revalidation
        +-- install / repair launches setup-only OGRE custom wizard
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

It does **not** depend on creation by the OGRE project wizard for discovery.

Its responsibilities are:

1. expose `Tools -> OGRE`;
2. locate supported installed OGRE utilities;
3. prefer the Code::Blocks-managed installation;
4. fall back to `PATH` for missing command-line tools;
5. verify each utility independently;
6. enable actions whose requirements are satisfied;
7. disable actions whose requirements are missing;
8. refresh capability state whenever the OGRE submenu opens;
9. revalidate a capability immediately before invoking it;
10. delegate installation/repair to the existing scripted-wizard installation machinery through a setup-only custom wizard registration;
11. invoke command-line tools safely and present their output in Code::Blocks.

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

Runtime discovery is proven for all five expected capabilities:

```text
OgreAssimpConverter
OgreMeshUpgrader
OgreXMLConverter
VRMLConverter
SampleBrowser.app
```

### 4.2 PATH fallback - PROVEN

`ogretoolslocator.cpp` uses `wxGetEnv("PATH", ...)` plus `wxFindFileInPath()` for command-line tools absent from the managed installation.

PATH fallback applies only to:

```text
OgreAssimpConverter
OgreMeshUpgrader
OgreXMLConverter
VRMLConverter
```

It does not synthesize a managed tools directory and does not attempt to find `SampleBrowser.app` through `PATH`.

The verified precedence matrix is:

```text
managed    PATH    selected
-------    ----    -----------------
yes        yes     managed
yes        no      managed
no         yes     PATH
no         no      unavailable
```

A controlled runtime test with a fake `/tmp/ogre-path-test/VRMLConverter` proved that the managed executable wins when present, the PATH executable is selected when the managed executable is hidden, and the capability becomes unavailable when neither exists.

### 4.3 Discovery is capability-based

A missing tool affects only its own capability. One missing optional executable does not invalidate the other tools.

### 4.4 Search precedence - SELECTED AND PROVEN

```text
1. known Code::Blocks-managed OGRE installation
2. PATH fallback for command-line tools
```

No speculative additional filesystem locations are currently required. Any future fixed candidate location must be explicitly justified and remain finite.

The locator must not perform an unbounded filesystem search.

### 4.5 Verification

Executable actions are enabled only when the resolved executable exists and is executable.

`SampleBrowser.app` is verified as a directory at its platform-specific managed location.

Do not infer capability availability merely because an enclosing OGRE directory exists.

### 4.6 On-demand refresh - PROVEN

`OgreTools` retains the submenu and menu-item pointers and centralizes current state in `RefreshCapabilities()`.

The event table includes:

```cpp
EVT_MENU_OPEN(OgreTools::OnOgreMenuOpen)
```

The handler refreshes only when the opened menu is the OGRE submenu:

```cpp
if (event.GetMenu() == m_OgreMenu)
    RefreshCapabilities();
```

A live A/B test proved that hiding `VRMLConverter` while Code::Blocks remained running disabled only the VRML action on the next submenu open, and restoring the executable re-enabled it without restarting Code::Blocks.

Action handlers independently call `LocateOgreTools()` again before use, providing action-time revalidation in addition to menu-time refresh.

No wizard notification, timer, filesystem watcher, or persistent capability cache is required.

### 4.7 No global environment modification

Discovery must not:

- modify `.zshrc`, `.bashrc`, shell profiles, or equivalent;
- permanently alter `PATH`;
- require users to manually expose the Code::Blocks-managed OGRE directory globally.

---

## 5. Tools-menu integration

### 5.1 Menu seam - PROVEN

`OgreTools::BuildMenu(wxMenuBar*)` locates the existing top-level Tools menu and appends an OGRE submenu.

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

### 5.2 Why Tools?

OGRE utility availability is an IDE-level capability, not a property of the active project.

`Tools -> OGRE` remains meaningful when no project is open, the active project does not use OGRE, OGRE is not yet installed, or the wizard installs OGRE during the current session.

### 5.3 Independent enabled state - PROVEN

Initial state is derived from `LocateOgreTools()` during menu construction. Every subsequent submenu open runs `RefreshCapabilities()` and reflects the current filesystem/PATH state.

Unavailable tools remain visible but disabled.

### 5.4 Menu event plumbing - PROVEN

`OgreTools` uses the existing wxWidgets static event-table mechanism:

```cpp
DECLARE_EVENT_TABLE();

BEGIN_EVENT_TABLE(OgreTools, cbPlugin)
    EVT_MENU_OPEN(OgreTools::OnOgreMenuOpen)
    EVT_MENU(...)
END_EVENT_TABLE()
```

Known-working handlers exist for `Open SDK tools folder` and `Open SampleBrowser`.

---

## 6. Working simple actions

### 6.1 Open SDK tools folder - DONE

The handler freshly calls `LocateOgreTools()` and opens the verified managed tools directory with `wxLaunchDefaultApplication()`.

### 6.2 Open SampleBrowser - DONE, with bundle correction

The handler freshly calls `LocateOgreTools()` and launches the verified `SampleBrowser.app` with `wxLaunchDefaultApplication()`.

Testing exposed an OGRE-side packaging defect rather than an OgreTools launch defect. `Codec_Assimp.framework` was installed under `lib/macosx/Release` but missing from `SampleBrowser.app/Contents/Frameworks`.

Controlled A/B testing established:

```text
installed SampleBrowser without Codec_Assimp.framework in bundle   FAIL
same application with framework added to Contents/Frameworks       PASS
```

The upstream cause was a missing `Codec_Assimp` entry in the existing macOS `FRAMEWORKS` list in `Samples/Browser/CMakeLists.txt`. The enclosing `if(TARGET ${FWK})` guard already preserves builds without the Assimp codec target.

The one-line correction is included in the pinned local compatibility patch and has also been submitted upstream. Upstream disposition remains external to v3.8.2.

---

## 7. Install / repair bridge - DESIGN RESOLVED, IMPLEMENTATION NEXT

The existing OGRE wizard owns the tested installation lifecycle:

```text
Homebrew prerequisite checks
        -> native CMake validation + managed paths
        -> Git validation
        -> exact OGRE v14.6.3 source acquisition
        -> exact-version compatibility patch
        -> CMake configure
        -> build / rebuild
        -> install
        -> installed-SDK validation
```

`OgreTools` must not duplicate this lifecycle.

### 7.1 Why launching the existing OGRE project wizard directly is rejected

The existing OGRE registration is:

```text
wizProject + folder "ogre"
    -> ogre/wizard.script
```

`cbWizardPlugin::Launch(index)` correctly launches this wizard, but successful completion of a `wizProject` entry ultimately dispatches to `RunProjectWizard()` and creates a project.

The desired Tools action is different:

```text
Tools -> OGRE -> Install / Repair OGRE...
        |
        v
install / repair / validate SDK
        |
        v
STOP
```

Therefore the existing `wizProject` entry is **not** the correct callable unit for M3.3.

### 7.2 The OGRE installation machinery is already independent of project creation

The macOS installer functions in `ogre/wizard.script` operate on OGRE setup state and native helpers, not on a `cbProject`.

Established setup functions include:

```text
OgreMacOfferNextMissingInstall()
OgreMacResolvePaths()
OgreMacNativeCMakeBlockReason()
OgreMacSourceReady()
OgreMacCloneOgre()
OgreMacPatchProbe()
OgreMacApplyPatch()
OgreMacConfigureOgre()
OgreMacBuildOgre()
OgreMacInstallOgre()
OgreMacAllReady()
```

Their page-leave handlers perform the existing idempotent setup lifecycle:

```text
OnLeave_OgreMacPrerequisites
OnLeave_OgreMacCMake
OnLeave_OgreMacGit
OnLeave_OgreMacAcquire
OnLeave_OgreMacPatch
OnLeave_OgreMacConfigure
OnLeave_OgreMacBuild
OnLeave_OgreMacInstall
```

The project-specific boundary in `BeginWizard()` begins only after these setup pages, at:

```cpp
Wizard.AddProjectPathPage();
```

This establishes a clean conceptual split:

```text
shared OGRE setup pages
        |
        +-- prerequisites
        +-- CMake + managed paths
        +-- Git
        +-- source acquisition
        +-- compatibility patch
        +-- configure
        +-- build
        +-- install / validate
        |
        +--------------------------- project-specific pages begin here
```

### 7.3 Existing `wizCustom` behavior is exactly suitable for setup-only execution

`Wiz::RunCustomWizard()` calls:

```cpp
SetupCustom()
```

and then clears the wizard state and returns `nullptr`. It does not create a project.

Therefore a setup-only OGRE registration should use `wizCustom`.

Successful completion can validate the setup through:

```cpp
function SetupCustom()
{
    return OgreMacAllReady();
}
```

### 7.4 One script can support both registrations

No ordinary script-level include mechanism was found in the shipped scripted-wizard resources, so duplicating the large OGRE installation script into a second setup script is rejected.

Fortunately `Wizard.GetWizardType()` is already available to Squirrel and is already used by multiple existing wizards. `Wiz::GetWizardType()` resolves the current registration through `m_LaunchIndex`.

Therefore the same:

```text
ogre/wizard.script
```

can be registered twice and branch on:

```cpp
local wiz_type = Wizard.GetWizardType();
```

Selected structure:

```text
                    ogre/wizard.script
                           |
                  Wizard.GetWizardType()
                      /          \
                     /            \
           wizProject              wizCustom
               |                       |
       OGRE setup pages          OGRE setup pages
               |                       |
       project-specific pages          |
               |                       |
         SetupProject()           SetupCustom()
                                       |
                                OgreMacAllReady()
                                       |
                                      DONE
```

A helper such as:

```cpp
function AddOgreMacSetupPages()
```

should contain the existing macOS setup-page construction so both modes execute exactly the same pages and handlers.

Conceptually:

```cpp
function BeginWizard()
{
    local wiz_type = Wizard.GetWizardType();

    if (wiz_type == wizCustom)
    {
        Wizard.AddInfoPage(
            _T("OgreSetupIntro"),
            _("Install or repair the Code::Blocks-managed OGRE SDK."));

        AddOgreMacSetupPages();
        return;
    }

    Wizard.AddInfoPage(
        _T("OgreIntro"),
        _("Welcome to the OGRE project wizard!..."));

    AddOgreMacSetupPages();

    Wizard.AddProjectPathPage();
    // existing project-specific pages unchanged
}
```

### 7.5 Second registration does not pollute the Project template list

`NewFromTemplateDlg` explicitly separates template registrations by `TemplateOutputType`:

```cpp
BuildListFor(totProject, ...);
BuildListFor(totTarget, ...);
BuildListFor(totFiles, ...);
BuildListFor(totCustom, ...);
```

and each list filters entries using:

```cpp
if (plugin->GetOutputType(w) != otype)
    continue;
```

Therefore the two registrations appear in separate template groups:

```text
Project templates
    Ogre project          <- wizProject / totProject

Custom templates
    OGRE SDK setup        <- wizCustom / totCustom
```

The setup entry does **not** become an OGRE project template.

### 7.6 Selected registration strategy

The existing registration remains:

```cpp
RegisterWizard(
    wizProject,
    _T("ogre"),
    _("Ogre project"),
    _("2D/3D Graphics"));
```

A second registration will point at the same folder and therefore the same `ogre/wizard.script`:

```cpp
RegisterWizard(
    wizCustom,
    _T("ogre"),
    _("OGRE SDK setup"),
    _("2D/3D Graphics"));
```

`RegisterWizard()` already derives the shared script path as:

```text
folder + "/wizard.script"
```

No duplicated installer script is required.

### 7.7 OgreTools uses only the public wizard interface

`Wiz` implements:

```cpp
class Wiz : public cbWizardPlugin
```

Code::Blocks enumerates wizard plugins through:

```cpp
Manager::Get()
    ->GetPluginManager()
    ->GetOffersFor(ptWizard);
```

The public interface provides:

```cpp
GetCount()
GetOutputType(index)
GetTitle(index)
GetScriptFilename(index)
Launch(index)
```

`OgreTools` should not include or depend on `wiz.h`.

The setup registration is selected by the stable pair:

```text
GetScriptFilename(index) == "ogre/wizard.script"
GetOutputType(index)     == totCustom
```

The project registration has the same script filename but `totProject`, so the output type disambiguates the two entries without relying on translated title text.

### 7.8 Final M3.3 invocation path

```text
Tools -> OGRE -> Install / Repair OGRE...
        |
        v
PluginManager::GetOffersFor(ptWizard)
        |
        v
for each cbWizardPlugin
        |
        v
for each index
        |
        +-- GetScriptFilename(index) == "ogre/wizard.script"
        +-- GetOutputType(index)     == totCustom
                |
                v
            Launch(index)
                |
                v
same ogre/wizard.script
                |
                v
Wizard.GetWizardType() == wizCustom
                |
                v
shared OGRE setup pages only
                |
                v
SetupCustom()
                |
                v
OgreMacAllReady()
                |
                v
no project created
                |
                v
next Tools -> OGRE open
                |
                v
fresh capability discovery
```

This is the selected M3.3 architecture.

---

## 8. First high-level workflow: Import 3D asset

`Tools -> OGRE -> Import 3D asset...`

The first high-level action uses `OgreAssimpConverter` and should freshly verify the converter, select input and destination, execute through a structured argv boundary, capture output and exit status, validate expected results, and avoid modifying the source asset.

When an active project exists, the dialog may offer a convenient project-local destination. Converter availability itself does not depend on the project.

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

Existing `macgit.cpp` code proves the required wxWidgets mechanism through structured `wxExecute(argv, ...)`, `wxExecuteEnv`, `wxProcess::Redirect()`, stream capture, `wxEVT_END_PROCESS`, and `event.GetExitCode()`.

SDK `PipedProcess::Launch(const wxString&)` remains rejected for this boundary because it flattens invocation into a command string.

---

## 10. Logging

Cccc and CppCheck establish the native Code::Blocks log-window lifecycle through `TextCtrlLogger`, `LogManager`, and the add/switch/remove log-window events.

`OgreTools` should own a dedicated `OGRE Tools` log for command output, warnings, exit state, and validation results.

---

## 11. Conversion validation

Conversion success is **not** defined solely by `.mesh` existence.

The importer should distinguish process launch failure, non-zero exit, missing mesh, mesh generation, missing expected material output, fallback/default material symptoms, referenced-resource warnings, and successful mesh/material conversion.

---

## 12. Project-local resource workflow

The importer may use project context for destination convenience without making tool discovery project dependent.

The workflow should leave source assets unchanged, keep generated output outside the installed OGRE SDK tree, prefer a project-local destination when appropriate, allow an explicit destination with no active project, and avoid silently modifying `resources.cfg` or `.cbp` files.

---

## 13. Portability boundary

The first implementation may support only the verified macOS managed layout. PATH fallback for command-line capabilities is already structurally cross-platform.

Unsupported platforms must not receive guessed filesystem paths presented as verified installations.

---

## 14. Preserved project-context seam

The earlier project-context investigation remains preserved in `project_context_seam.md`. It is not part of v3.8.2 tool discovery.

---

## 15. Expected files / areas

| Area | v3.8.2 role |
|---|---|
| `src/plugins/ogretools/` | Native plugin implementation. |
| `ogretools.cpp/.h` | Plugin lifecycle, menu, refresh, simple actions, install/repair launcher. |
| `ogretoolslocator.cpp/.h` | Managed discovery, PATH fallback, verification. |
| `src/plugins/scriptedwizard/resources/config.script` | Adds setup-only `wizCustom` registration pointing at the existing OGRE script. |
| `src/plugins/scriptedwizard/resources/ogre/wizard.script` | Shared OGRE setup pages; branches between project and setup-only modes with `Wizard.GetWizardType()`. |
| `cbPlugin::BuildMenu(wxMenuBar*)` | Adds `Tools -> OGRE`. |
| `EVT_MENU_OPEN` / `wxMenuEvent` | Fresh capability refresh on submenu open. |
| `cbWizardPlugin` + `PluginManager::GetOffersFor(ptWizard)` | Public bridge from OgreTools to setup-only OGRE wizard registration. |
| `NewFromTemplateDlg::BuildListFor()` | Proven output-type filtering keeps `wizCustom` out of Project templates. |
| `Wiz::RunCustomWizard()` / `SetupCustom()` | Completes setup-only wizard without creating a project. |
| `wxLaunchDefaultApplication()` | Opens managed tools directory and SampleBrowser. |
| `wxExecute(argv, ...)`, `wxProcess`, `wxExecuteEnv` | Safe command-line execution. |
| `LogManager`, `TextCtrlLogger`, `CodeBlocksLogEvent` | Dedicated OGRE tool output. |
| Exact-version OGRE compatibility patch | Referenced-resource correction plus SampleBrowser Assimp bundle correction. |
| `project_context_seam.md` | Preserved deferred investigation. |

---

## 16. Milestones

| Milestone | Objective | Check | Status |
|---|---|---|---|
| M1.1 | Create native OgreTools plugin. | Plugin builds and packages through the macOS build. | **DONE** |
| M1.2 | Prove and cleanly rename plugin loading. | `OgreTools` appears among 15 loaded plugins; obsolete `ProjectTools` artifacts are absent. | **DONE** |
| M1.3 | Preserve abandoned project-context investigation. | Useful seams extracted to `project_context_seam.md`. | **DONE** |
| M2.1 | Define OGRE capability locator. | Managed installation contract, capability identities, verification, precedence defined. | **DONE** |
| M2.2 | Discover verified macOS OGRE tools. | Managed installation reports all five expected capabilities independently at runtime. | **DONE** |
| M2.3 | Add PATH fallback. | Managed tool wins over PATH; PATH is used only when managed command-line tool is absent; neither means unavailable. | **DONE** |
| M2.4 | Implement on-demand capability refresh. | Opening `Tools -> OGRE` rechecks availability; live filesystem changes are reflected without restart. | **DONE** |
| M3.1 | Add `Tools -> OGRE` submenu. | Submenu visible independently of project state. | **DONE** |
| M3.2 | Enable/disable individual actions. | Menu state follows current independent capability discovery on every open. | **DONE** |
| M3.3a | Establish install/repair reuse boundary. | Full project-wizard launch rejected; setup functions proven independent; `wizCustom`, `SetupCustom()`, `Wizard.GetWizardType()`, output-type filtering, and public `cbWizardPlugin` invocation path validated from source. | **DONE** |
| M3.3b | Add setup-only OGRE registration. | Same `ogre/wizard.script` registered as `wizCustom` without duplicating installer code; Custom template entry does not appear in Project templates. | **NEXT** |
| M3.3c | Split shared setup pages from project pages. | `BeginWizard()` branches on `Wizard.GetWizardType()`; both modes use identical OGRE setup pages; only `wizProject` adds project-specific pages. | **PLANNED** |
| M3.3d | Complete setup-only wizard. | `SetupCustom()` returns `OgreMacAllReady()`; successful completion creates no project. | **PLANNED** |
| M3.3e | Wire OgreTools install/repair action. | OgreTools finds `ogre/wizard.script` + `totCustom` through `cbWizardPlugin` and calls `Launch(index)`. | **PLANNED** |
| M3.3f | Validate install-during-session behavior. | Setup-only wizard installs/repairs OGRE; next submenu open reflects the result without restarting Code::Blocks. | **PLANNED** |
| M3.4 | Add tools-folder and SampleBrowser actions. | Verified local targets open without global PATH changes. | **DONE** |
| M3.5 | Correct SampleBrowser Assimp bundle. | `Codec_Assimp.framework` bundled when target exists; installed SampleBrowser runs. | **DONE LOCALLY / UPSTREAM PR OPEN** |
| M4.1 | Implement `Import 3D asset...` dialog. | Input, destination, and freshly resolved converter explicit. | **PLANNED** |
| M4.2 | Execute converter safely. | Explicit argv works with paths containing spaces. | **PLANNED** |
| M4.3 | Add dedicated OGRE log. | stdout, stderr, and actual exit status visible. | **PLANNED** |
| M4.4 | Validate converter outcome. | Geometry-only and material/resource failures surfaced. | **PLANNED** |
| M4.5 | Validate output workflow. | Project-local output offered when appropriate; source files and SDK media remain untouched. | **PLANNED** |
| M5.1 | Regression-test complete managed installation. | All expected actions become available. | **PLANNED** |
| M5.2 | Regression-test partial installation. | Missing tools independently disable only corresponding actions. | **PLANNED** |
| M5.3 | Regression-test no managed installation. | Managed-only actions unavailable; PATH fallbacks remain usable; install/repair remains available. | **PLANNED** |
| M5.4 | Regression-test PATH fallback. | Four-state managed/PATH precedence matrix remains correct across supported command-line tools. | **DONE FOR VRML PROOF / BROADER REGRESSION PLANNED** |
| M5.5 | Regression-test no-project and ordinary-project states. | `Tools -> OGRE` remains usable without classifying or mutating projects. | **PLANNED** |
| M5.6 | Review documentation and release boundary. | Discovery, setup-only wizard bridge, execution, compatibility corrections, and limitations documented. | **PLANNED** |
| M5.7 | Release v3.8.2. | Signed macOS package contains validated OgreTools implementation. | **PLANNED** |

---

## 17. Acceptance criteria

v3.8.2 is accepted only if:

1. The pinned OGRE v14.6.3 installation remains the known-good managed baseline.
2. `OgreTools` builds, packages, installs, and loads as a native Code::Blocks plugin.
3. `Tools -> OGRE` is available independently of project state.
4. The managed tools directory is discovered consistently with the wizard's install contract.
5. Managed command-line tools outrank same-named PATH tools.
6. PATH provides a fallback only for missing command-line capabilities.
7. No shell profile or global `PATH` is modified.
8. Each capability is verified independently.
9. Missing tools remain visible but disabled.
10. Every submenu open performs fresh capability discovery.
11. Action handlers revalidate required capabilities immediately before use.
12. The existing OGRE `wizProject` registration continues to create ordinary OGRE projects through the existing project path.
13. A setup-only `wizCustom` registration reuses the same `ogre/wizard.script` rather than duplicating installer code.
14. `Wizard.GetWizardType()` cleanly selects project versus setup-only behavior.
15. Shared OGRE setup pages and handlers are identical between the project and setup-only modes.
16. The setup-only wizard creates no project and completes through `SetupCustom()` plus `OgreMacAllReady()`.
17. The `wizCustom` setup entry is separated from Project templates by existing `TemplateOutputType` filtering.
18. OgreTools locates the setup entry through the public `cbWizardPlugin` interface using `ogre/wizard.script` + `totCustom`, without depending on `Wiz` internals.
19. Setup completion becomes visible on the next `Tools -> OGRE` open without restarting Code::Blocks.
20. `Open SDK tools folder` opens the freshly verified managed directory.
21. `Open SampleBrowser` launches the freshly verified managed application.
22. The managed macOS SampleBrowser includes `Codec_Assimp.framework` when the codec target exists.
23. No arbitrary shell-command execution is introduced.
24. Command-line executable paths and arguments remain separate.
25. stdout, stderr, and actual exit status are captured for converter execution.
26. Conversion success is not inferred solely from `.mesh` existence.
27. User source assets are not silently modified.
28. Generated output does not default into installed SDK media.
29. `resources.cfg` and project files are not silently rewritten.
30. Ordinary projects are not classified as OGRE projects merely because tools are installed.
31. Deferred project-context machinery is not reintroduced into discovery without a specific need.
32. The final macOS bundle passes the established packaging and code-signing workflow.

---

## 18. What stays rejected

- Permanent `isOgreProject` classification.
- Requiring wizard ancestry merely to expose tools.
- Parsing active build configuration merely to decide whether the OGRE menu exists.
- Global shell-startup modification or permanent PATH insertion.
- Unbounded filesystem searches.
- PATH overriding a valid managed tool.
- Persistent capability caching.
- Wizard-specific refresh notifications, timers, or filesystem watchers when menu-open refresh suffices.
- Launching the existing `wizProject` OGRE entry from Install / Repair, because successful completion also creates a project.
- Duplicating the OGRE installer inside OgreTools.
- Duplicating `ogre/wizard.script` solely to create a setup-only workflow.
- Creating new ScriptedWizard include machinery when `Wizard.GetWizardType()` plus dual registration already provides a one-script solution.
- Depending on concrete `Wiz` internals when `cbWizardPlugin` provides the required interface.
- Selecting the setup wizard by translated title when script filename + output type provides a stable identity.
- Patching SampleBrowser opportunistically at launch time.
- Shell-string converter execution.
- `.mesh` existence as the sole definition of conversion success.
- Copying user assets into installed SDK media.
- Hidden `resources.cfg` or project-setting rewrites.
- Converter machinery in generated `main.cpp`.
- Reintroducing project-context complexity merely because the seam exists.
- Expanding v3.8.2 into an asset database, package manager, or scene editor.

---

## 19. Current critical path

```text
managed capability locator                       DONE
        |
        +-- all five managed capabilities        DONE
        +-- PATH fallback + precedence            DONE
        +-- live submenu refresh                  DONE
        +-- action-time revalidation              ESTABLISHED PATTERN
        |
        v
Tools -> OGRE submenu                            DONE
        |
        +-- independent enablement                DONE
        +-- tools folder                          DONE
        +-- SampleBrowser                         DONE
        +-- SampleBrowser Assimp bundle fix       DONE LOCALLY
        |
        v
install / repair architecture                    RESOLVED
        |
        +-- full wizProject launch                REJECTED FOR SETUP
        +-- OGRE setup functions independent      PROVEN
        +-- wizCustom semantics                   PROVEN
        +-- SetupCustom() path                    PROVEN
        +-- Wizard.GetWizardType()                PROVEN
        +-- same script dual registration         SELECTED
        +-- Project/Custom template filtering     PROVEN
        +-- public cbWizardPlugin lookup          PROVEN
        |
        v
add setup-only OGRE registration                  NEXT
        |
        v
branch BeginWizard() + SetupCustom()
        |
        v
wire OgreTools Install / Repair action
        |
        v
validate install -> live refresh
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

## 20. Done when

**Status: IN PROGRESS.**

v3.8.2 is complete when Code::Blocks exposes installed OGRE utilities through the native `OgreTools` plugin under `Tools -> OGRE`, resolves current capabilities using managed-first/PATH-fallback precedence, refreshes availability on demand, provides setup-only OGRE installation/repair through the existing scripted-wizard machinery without creating a project or duplicating installer code, and safely runs `OgreAssimpConverter` through a logged asset-import workflow without requiring project classification or global `PATH` modification.

