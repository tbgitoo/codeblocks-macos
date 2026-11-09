# Code::Blocks v3.8.2 - OGRE tool integration

**Status: IN PROGRESS - the native OgreTools plugin, managed macOS capability discovery, PATH fallback, live `Tools -> OGRE` refresh, SDK-folder action, SampleBrowser action, and the complete setup-only Install / Repair bridge are proven. A clean menu-launched reacquisition, configure, build, install, validation, and immediate in-session capability refresh succeeded without creating a project. Native Git/CMake dialogs now retain the wizard correctly through explicit parent ownership. M4.1, the `Import 3D asset...` dialog, is implemented. M4.2 runs `OgreAssimpConverter` with an explicit argument vector, a `plugins.cfg` working directory, captured stdout, stderr, and exit status. M4.3 records that run in a dedicated `OGRE Tools` log. Generated mesh and material files stay in the chosen destination. `resources.cfg` and the project file stay unchanged unless the user confirms the registration in section 12. M4.5 asks before adding the destination directory to an existing `resources.cfg`, creates a starter `resources.cfg` when that directory has none, and asks before listing the generated files in an open project. M4.4, conversion validation beyond exit status plus written mesh and material files, is next.**

v3.8.2 follows the released v3.8.1 OGRE baseline and makes the OGRE tools installed by the Code::Blocks OGRE workflow discoverable and usable directly from the IDE.

**Question this version answers:** how can Code::Blocks expose installed OGRE utilities such as `OgreAssimpConverter` without requiring users to know their filesystem locations or modify their shell `PATH`?

The feature does **not** determine whether the active project is an OGRE project. OGRE utilities are IDE-level tools. `OgreTools` exposes them under the global `Tools` menu, while individual actions are enabled only when their required executable or application is currently available.

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
        +-- setup-only install / repair wizard
        +-- correct wizard-owned modal hierarchy
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

During v3.8.2 development, launching the installed `SampleBrowser.app` exposed an additional macOS bundle-completeness defect: `Codec_Assimp.framework` was installed under `lib/macosx/Release` but omitted from `SampleBrowser.app/Contents/Frameworks`. Adding `Codec_Assimp` to the existing SampleBrowser `FRAMEWORKS` list fixed the application.

The correction is included in the pinned local OGRE v14.6.3 compatibility patch and has now been **merged upstream into the current OGRE master branch**. It is not yet part of a tagged OGRE release, so the local v14.6.3 patch hunk remains required until Code::Blocks deliberately adopts a later tagged OGRE release containing the fix and reruns the clean regression matrix.

---

## 2. Core architecture

`OgreTools` is an ordinary native Code::Blocks plugin.

It does **not**:

- classify projects;
- infer OGRE use from compiler or linker settings;
- require creation by the OGRE project wizard for discovery;
- modify global shell startup or `PATH`;
- duplicate the OGRE installation implementation.

Its responsibilities are:

1. expose `Tools -> OGRE`;
2. locate supported OGRE utilities;
3. prefer the Code::Blocks-managed installation;
4. fall back to `PATH` for missing command-line tools;
5. verify each capability independently;
6. refresh current state whenever the OGRE submenu opens;
7. revalidate a capability immediately before use;
8. delegate installation/repair to the existing scripted-wizard machinery through a setup-only custom wizard;
9. execute command-line tools safely and report their output.

Project context may later be used by a specific operation, such as suggesting a project-local output directory. It is not part of discovery or menu visibility.

---

## 3. Plugin baseline - PROVEN

Source area:

```text
src/plugins/ogretools/
        ogretools.h
        ogretools.cpp
        ogretoolslocator.h
        ogretoolslocator.cpp
        manifest.xml
        Makefile.am
```

Build integration:

```text
configure.ac
m4/acinclude.m4
src/plugins/Makefile.am
src/plugins/ogretools/Makefile.am
```

Build identities:

```text
--enable-ogretools
BUILD_OGRETOOLS
MAYBE_OGRETOOLS
libogretools.la
```

Installed package:

```text
share/codeblocks/plugins/libogretools.dylib
share/codeblocks/ogretools.zip
```

Runtime loading is proven, and obsolete `ProjectTools` build, staging, and installed artifacts are removed.

---

## 4. Capability discovery

### 4.1 Managed macOS installation - PROVEN

The wizard-managed contract is:

```text
root    = wxGetHomeDir()/Library/CodeBlocks
install = root/ogre-14.6.3
tools   = install/bin/macosx
```

`LocateOgreTools()` reconstructs that location and verifies independently:

```text
OgreAssimpConverter
OgreMeshUpgrader
OgreXMLConverter
VRMLConverter
SampleBrowser.app
```

### 4.2 PATH fallback - PROVEN

`ogretoolslocator.cpp` uses:

```cpp
wxGetEnv("PATH", ...)
wxFindFileInPath(...)
wxFileName::IsFileExecutable(...)
```

PATH fallback applies only to command-line tools:

```text
OgreAssimpConverter
OgreMeshUpgrader
OgreXMLConverter
VRMLConverter
```

It does not synthesize a managed tools directory and does not search for `SampleBrowser.app` on `PATH`.

Verified precedence:

```text
managed    PATH    selected
-------    ----    -----------------
yes        yes     managed
yes        no      managed
no         yes     PATH
no         no      unavailable
```

A controlled runtime test with `/tmp/ogre-path-test/VRMLConverter` proved both fallback and managed-over-PATH precedence.

### 4.3 Verification and independence

- Executables must exist and be executable.
- `SampleBrowser.app` is verified as a directory in the managed installation.
- A missing tool affects only its own capability.
- The locator performs no unbounded filesystem search.

### 4.4 On-demand refresh - PROVEN

`OgreTools` retains its submenu/menu-item pointers and centralizes state in `RefreshCapabilities()`.

```cpp
EVT_MENU_OPEN(OgreTools::OnOgreMenuOpen)
```

```cpp
if (event.GetMenu() == m_OgreMenu)
    RefreshCapabilities();
```

A live A/B test proved that hiding and restoring `VRMLConverter` while Code::Blocks remained running immediately changed only the corresponding menu item on the next submenu open.

The clean reinstall test additionally proved that a running Code::Blocks instance can move from no managed OGRE source/install tree to a complete installation and expose the restored capabilities on the next submenu open without restart.

Action handlers call `LocateOgreTools()` again before execution. No timer, watcher, wizard callback, restart, or persistent capability cache is required.

---

## 5. Tools-menu integration - PROVEN

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

The menu exists independently of project state. Unavailable capabilities remain visible but disabled.

Working handlers:

```text
Import 3D asset...
Open SDK tools folder
Open SampleBrowser
Install / Repair OGRE tools...
```

The simple launch handlers freshly revalidate their target before use.

---

## 6. SampleBrowser bundle correction - DONE LOCALLY / MERGED UPSTREAM

Direct launch exposed:

```text
SampleBrowser.app/Contents/Frameworks/Codec_Assimp.framework/Codec_Assimp
```

as required but absent.

The installed framework existed at:

```text
~/Library/CodeBlocks/ogre-14.6.3/lib/macosx/Release/Codec_Assimp.framework
```

Controlled A/B result:

```text
installed SampleBrowser without bundled Codec_Assimp.framework   FAIL
same app with framework in Contents/Frameworks                    PASS
```

The upstream cause was a missing `Codec_Assimp` entry in `Samples/Browser/CMakeLists.txt`. The existing `if(TARGET ${FWK})` guard preserves builds without that target.

The one-line correction is:

```diff
         Codec_STBI
+        Codec_Assimp
         OgreBites
```

Status:

```text
local OGRE v14.6.3 compatibility patch    DONE
upstream pull request                     MERGED
current OGRE master including fix         YES
tagged OGRE release including fix         NOT YET
local patch retirement                    DEFERRED UNTIL VERSION UPGRADE
```

The local patch remains necessary because the pinned official `v14.6.3` tag is immutable and does not contain the merged correction.

---

## 7. Install / repair bridge - COMPLETE AND PROVEN

### 7.1 Existing project wizard is not the correct setup action

The original registration is:

```text
wizProject + folder "ogre"
    -> ogre/wizard.script
```

Launching it through `cbWizardPlugin::Launch(index)` runs installation pages but also dispatches to `RunProjectWizard()` and creates a project.

The Tools action must instead perform:

```text
install / repair / validate SDK
        -> stop
        -> no project
```

### 7.2 Installation machinery is independent of project generation

The established OGRE setup sequence is:

```text
Homebrew prerequisites
        -> native CMake + managed paths
        -> Git
        -> exact source acquisition
        -> compatibility patch
        -> configure
        -> build / rebuild
        -> install
        -> validation
```

Relevant script functions include:

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

Project-specific UI begins only at:

```cpp
Wizard.AddProjectPathPage();
```

### 7.3 Setup-only custom registration - IMPLEMENTED AND PROVEN

The same script is registered twice:

```cpp
RegisterWizard(
    wizProject,
    _T("ogre"),
    _("Ogre project"),
    _("2D/3D Graphics"));

RegisterWizard(
    wizCustom,
    _T("ogre"),
    _("OGRE SDK setup"),
    _("2D/3D Graphics"));
```

Both registrations point to:

```text
ogre/wizard.script
```

The New dialog proves correct output-type separation:

```text
Projects tab
    Ogre project

Custom tab
    OGRE SDK setup
```

The setup entry does not appear among project templates.

### 7.4 Shared setup pages - IMPLEMENTED AND PROVEN

The existing macOS setup-page construction has been extracted to:

```cpp
function AddOgreMacSetupPages()
```

`BeginWizard()` uses:

```cpp
local wiz_type = Wizard.GetWizardType();
```

and branches:

```text
wizProject
    -> project-specific intro
    -> AddOgreMacSetupPages()
    -> project path
    -> compiler/project pages
    -> SetupProject()

wizCustom
    -> setup-specific intro
    -> AddOgreMacSetupPages()
    -> return before project pages
    -> SetupCustom()
```

The running application proves that the setup wizard displays the setup-specific introduction and that the project wizard retains its project-specific introduction.

### 7.5 Setup-only completion - IMPLEMENTED AND PROVEN

`Wiz::RunCustomWizard()` calls `SetupCustom()`, clears wizard state, and returns `nullptr` without project creation.

```cpp
function SetupCustom()
{
    return OgreMacAllReady();
}
```

Validation against an existing installation succeeded:

```text
setup pages completed
SetupCustom() returned success
no "Wizard failed..." message
no .cbp project created
no new project opened
existing project state unchanged
```

### 7.6 Squirrel string-literal constraint - DOCUMENTED

Squirrel does not concatenate adjacent quoted literals as C/C++ does. Code such as:

```cpp
_("first"
  "second")
```

is interpreted as multiple arguments and produces a runtime error such as:

```text
Wrong number of arguments to '_' - expected 2 given 5
```

Wizard text passed to `_()` must use one literal or explicit Squirrel concatenation.

### 7.7 Public invocation seam - IMPLEMENTED

`Wiz` implements:

```cpp
class Wiz : public cbWizardPlugin
```

Code::Blocks obtains wizard providers through:

```cpp
Manager::Get()
    ->GetPluginManager()
    ->GetOffersFor(ptWizard);
```

The setup entry is selected by the stable pair:

```text
GetScriptFilename(index) == "ogre/wizard.script"
GetOutputType(index)     == totCustom
```

`OgreTools` does not depend on `wiz.h` or translated title text.

### 7.8 Final menu invocation path - IMPLEMENTED AND PROVEN

```text
Tools -> OGRE -> Install / Repair OGRE tools...
        |
        v
PluginManager::GetOffersFor(ptWizard)
        |
        v
for each cbWizardPlugin and index
        |
        +-- script == "ogre/wizard.script"
        +-- type   == totCustom
                |
                v
            Launch(index)
                |
                v
shared setup-only wizard
                |
                v
SetupCustom() -> OgreMacAllReady()
                |
                v
no project created
```

The menu action has been tested directly and opens `OGRE SDK setup`, not the New Project dialog and not the project registration.

### 7.9 Clean bootstrap / repair validation - PROVEN

The complete source tree and installed SDK tree were removed while Code::Blocks remained running:

```text
~/Library/CodeBlocks/ogre-source-14.6.3
~/Library/CodeBlocks/ogre-14.6.3
```

The menu-launched setup-only wizard then successfully performed:

```text
source acquisition
        -> compatibility patch handling
        -> CMake configure
        -> CMake build
        -> CMake install
        -> installed-SDK validation
        -> SetupCustom() success
```

Results:

```text
clean source acquisition                 PASS
configure                                PASS
build                                    PASS
install                                  PASS
installed-SDK validation                 PASS
setup-only completion                    PASS
no project created                       PASS
existing project state unaffected        PASS
next-open capability refresh             PASS
Code::Blocks restart required            NO
```

This proves the real M3.3f recovery contract: a running Code::Blocks instance can go from no managed OGRE source/install state to a complete usable managed OGRE environment solely through `Tools -> OGRE -> Install / Repair OGRE tools...`.

### 7.10 Native modal ownership correction - IMPLEMENTED AND PROVEN

The clean bootstrap initially exposed a UI defect: after Git clone or a CMake operation completed, the OGRE wizard appeared to vanish because the Code::Blocks main frame came to the front while the wizard remained behind it.

Root cause:

```cpp
MacGitCloneDialog dlg(
    Manager::Get()->GetAppWindow(), ...);

MacCMakeConfigureDialog dlg(
    Manager::Get()->GetAppWindow(), ...);

MacCMakeBuildDialog dlg(
    Manager::Get()->GetAppWindow(), ...);

MacCMakeInstallDialog dlg(
    Manager::Get()->GetAppWindow(), ...);
```

The progress dialogs and wizard were siblings owned by the main frame:

```text
Code::Blocks main frame
    +-- OGRE wizard
    +-- Git/CMake operation dialog
```

When an operation dialog closed, macOS correctly returned focus to its parent, the Code::Blocks main frame, not the wizard.

The correction passes the active `wxWizard` through the native helper boundary:

```text
Wiz::GetWizardWindow()
        -> Squirrel binding
        -> CloneMacGitRepository(parent, ...)
        -> ConfigureMacCMake(parent, ...)
        -> BuildMacCMake(parent, ...)
        -> InstallMacCMake(parent, ...)
```

The confirmation dialogs and progress dialogs now use the supplied parent:

```text
Code::Blocks main frame
    +-- OGRE wizard
        +-- Git/CMake confirmation dialog
        +-- Git/CMake operation dialog
```

Implementation areas:

```text
wiz.h / wiz.cpp
    +-- public GetWizardWindow() accessor

macgit.h / macgit.cpp
    +-- wxWindow* parent passed through clone helper
    +-- ConfirmClone(parent, ...)
    +-- MacGitCloneDialog(parent, ...)

maccmake.h / maccmake.cpp
    +-- wxWindow* parent passed through configure/build/install helpers
    +-- ConfirmConfigure(parent, ...)
    +-- all operation dialogs parented to the wizard
```

No post-hoc `Raise()`, `SetFocus()`, or `wxCallAfter()` workaround is required. Correct ownership restores the wizard naturally after each native modal operation.

Runtime result:

```text
operation dialog closes
        -> OGRE wizard remains/returns frontmost
        -> workflow is visibly continuous
```

This behavior is proven.

---

## 8. First high-level workflow: Import 3D asset

`Tools -> OGRE -> Import 3D asset...`

The first high-level action uses `OgreAssimpConverter` and should:

1. freshly verify the converter;
2. select an input asset;
3. select or confirm a destination;
4. show the actual converter path;
5. execute through an explicit argument vector;
6. capture stdout and stderr;
7. capture the actual exit status;
8. report results in a dedicated Code::Blocks log;
9. inspect expected outputs;
10. distinguish partial conversion from genuine success;
11. avoid modifying the source asset.

### 8.1 Import dialog - IMPLEMENTED

`Tools -> OGRE -> Import 3D asset...` calls `ShowOgreImportDialog()` after a fresh `OgreAssimpConverter` check. A missing converter produces a warning and does not open the dialog.

The dialog shows the verified converter path, asks for a source asset, and asks for a destination directory. The active editor supplies the initial destination when its file belongs to no project and that file's directory lies outside the managed OGRE prefix. Otherwise an open project supplies that project's directory when it lies outside the managed prefix. Directories inside the managed OGRE prefix are never offered. Selecting a source fills an empty destination with the source directory only when that directory is outside the managed prefix.

Import checks the converter again, requires an existing source file and an existing destination directory, and refuses a destination inside the managed OGRE prefix.

### 8.2 Converter execution and log - IMPLEMENTED

A confirmed Import runs `OgreAssimpConverter` through `wxExecute` with separate arguments: `-log`, the source file, and the destination directory. The working directory is the directory that contains `plugins.cfg`, which for the managed layout is `bin/` rather than `bin/macosx/`. The converter log is a temporary file, so the SDK directory is not used for output. Stdout, stderr, the converter log, and the exit status are written to a dedicated `OGRE Tools` log.

The run reports a mesh only when that file was created or rewritten, and likewise for the material script. Exit status 0 together with both written files is the successful result. The source asset is left unchanged. `resources.cfg` and the project file are also left unchanged until the user confirms the registration in section 12. Deeper material-content checks remain M4.4.

```text
src/plugins/ogretools/ogreimportdlg.h
src/plugins/ogretools/ogreimportdlg.cpp
```

---

## 9. Safe execution boundary

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

Existing `macgit.cpp` and `maccmake.cpp` code prove the required structured `wxExecute(argv, ...)`, `wxExecuteEnv`, `wxProcess::Redirect()`, stream-capture, asynchronous-process, and `wxEVT_END_PROCESS` lifecycle.

`PipedProcess::Launch(const wxString&)` remains rejected because it flattens invocation into a command string.

---

## 10. Logging

`OgreTools` should own a dedicated `OGRE Tools` log using the established `TextCtrlLogger` / `LogManager` lifecycle.

---

## 11. Conversion validation

Conversion success is **not** defined solely by `.mesh` existence.

The importer should distinguish launch failure, non-zero exit, missing mesh, mesh generation, absent expected material output, fallback/default-material symptoms, referenced-resource warnings, and successful mesh/material conversion.

---

## 12. Project-local resource workflow - IMPLEMENTED

The importer may use project context for destination convenience without making discovery project dependent.

The workflow leaves the source asset unchanged, keeps output outside the installed SDK, offers a project-local destination when useful, and allows an explicit destination without an active project. It does not classify the active project as an OGRE project, and it does not change menu visibility.

After a successful conversion, registration is a separate explicit choice. Declining it leaves `resources.cfg` and the project file unchanged.

`resources.cfg` records resource directories, not mesh filenames. The offered change is one `FileSystem` entry for the destination directory. A mesh filename is not a valid entry. When that directory is already listed, no change is offered.

The file to consider is `resources.cfg` in the destination directory. An open editor file in some other directory does not select a different file. For a project whose destination is the project directory, this is the `resources.cfg` beside the executable, which is the file `OgreBites::ApplicationContext` already searches. A second `resources.cfg` elsewhere is not loaded by that running program.

When that file exists, the import asks before adding the destination directory. When it does not exist, the import creates one beside the mesh and tells the user that the new file is a starter. OGRE loads it automatically only when it sits in a directory the program already searches.

When the source is an OBJ file, the import reads each `mtllib` entry and copies the texture files named by that material library into the destination. The source files are not modified. A texture that is already in the destination is left in place. A referenced texture that cannot be found is reported and does not fail the mesh conversion. Those copied textures are included in the project-tree confirmation below.

When a project is open, the import asks before adding the generated `.mesh`, `.material`, and copied textures to the project tree. A `resources.cfg` created by this same import is included in that confirmation. The added files are visible in the tree and are not compiled. When no project is open, there is no project-tree question.

---

## 13. Portability boundary

The first managed-layout implementation may remain macOS-specific. PATH fallback for command-line capabilities is structurally cross-platform.

Unsupported platforms must not receive guessed filesystem paths presented as verified installations.

---

## 14. Preserved project-context seam

The earlier project-context investigation remains preserved in `project_context_seam.md`. It is not part of v3.8.2 discovery.

---

## 15. Expected files / areas

| Area | v3.8.2 role |
|---|---|
| `src/plugins/ogretools/` | Native plugin implementation. |
| `ogretools.cpp/.h` | Menu, refresh, simple actions, import dialog launch, and implemented install/repair launcher. |
| `ogreimportdlg.cpp/.h` | Import 3D asset dialog. |
| `ogretoolslocator.cpp/.h` | Managed discovery, PATH fallback, and verification. |
| `src/plugins/scriptedwizard/resources/config.script` | Project and setup-only registrations pointing at the same OGRE script. |
| `src/plugins/scriptedwizard/resources/ogre/wizard.script` | Shared setup pages, type branch, `SetupProject()`, and `SetupCustom()`. |
| `src/plugins/scriptedwizard/wiz.h/.cpp` | Wizard-type API, active wizard-window accessor, and native binding parent propagation. |
| `src/plugins/scriptedwizard/macgit.h/.cpp` | Git probe/clone plus wizard-owned confirmation and progress dialogs. |
| `src/plugins/scriptedwizard/maccmake.h/.cpp` | CMake probe/paths/configure/build/install plus wizard-owned dialogs. |
| `cbWizardPlugin` / `GetOffersFor(ptWizard)` | Public menu-to-wizard bridge. |
| `NewFromTemplateDlg::BuildListFor()` | Output-type filtering separating Project and Custom entries. |
| `Wiz::RunCustomWizard()` | Calls `SetupCustom()` without project creation. |
| `wxLaunchDefaultApplication()` | Opens managed directory and SampleBrowser. |
| `wxExecute(argv, ...)`, `wxProcess`, `wxExecuteEnv` | Safe command-line execution. |
| `LogManager`, `TextCtrlLogger`, `CodeBlocksLogEvent` | Planned dedicated OGRE output. |
| Exact-version OGRE compatibility patch | Assimp sidecar correction plus SampleBrowser framework correction. |

---

## 16. Milestones

| Milestone | Objective | Status |
|---|---|---|
| M1.1 | Create native OgreTools plugin. | **DONE** |
| M1.2 | Prove rename, packaging, and loading; remove obsolete ProjectTools artifacts. | **DONE** |
| M1.3 | Preserve deferred project-context investigation. | **DONE** |
| M2.1 | Define OGRE capability locator. | **DONE** |
| M2.2 | Discover verified managed macOS tools independently. | **DONE** |
| M2.3 | Add and prove managed-first PATH fallback. | **DONE** |
| M2.4 | Implement and prove live submenu refresh. | **DONE** |
| M3.1 | Add `Tools -> OGRE` submenu. | **DONE** |
| M3.2 | Enable/disable independent actions from current availability. | **DONE** |
| M3.3a | Establish install/repair reuse boundary. | **DONE** |
| M3.3b | Register the shared script as setup-only `wizCustom`; prove separate Custom/Projects placement. | **DONE** |
| M3.3c | Extract shared setup pages and branch `BeginWizard()` by `Wizard.GetWizardType()`. | **DONE** |
| M3.3d | Implement and prove `SetupCustom() -> OgreMacAllReady()` with no project creation. | **DONE** |
| M3.3e | Wire OgreTools Install / Repair action through `cbWizardPlugin`. | **DONE** |
| M3.3f | Validate clean menu-launched reacquisition/configure/build/install and next-open capability refresh. | **DONE** |
| M3.3g | Correct native Git/CMake modal ownership so the OGRE wizard remains frontmost. | **DONE** |
| M3.4 | Add SDK-folder and SampleBrowser actions. | **DONE** |
| M3.5 | Correct SampleBrowser Assimp bundle. | **DONE LOCALLY / MERGED UPSTREAM / NOT YET TAGGED** |
| M4.1 | Implement Import 3D asset dialog. | **DONE** |
| M4.2 | Execute converter safely with explicit argv. | **DONE** |
| M4.3 | Add dedicated OGRE log. | **DONE** |
| M4.4 | Validate converter outcome beyond `.mesh` existence. | **NEXT** |
| M4.5 | Confirm destination registration in `resources.cfg` and, when a project is open, in the project tree. | **DONE** |
| M5.1 | Regression-test complete managed installation. | **PLANNED** |
| M5.2 | Regression-test partial/no managed installation. | **PARTLY COVERED BY CLEAN REINSTALL / BROADER MATRIX PLANNED** |
| M5.3 | Broaden PATH fallback regression beyond the proven VRML case. | **PLANNED** |
| M5.4 | Regression-test no-project and ordinary-project states. | **PARTLY PROVEN / BROADER REGRESSION PLANNED** |
| M5.5 | Review documentation and release boundary. | **PLANNED** |
| M5.6 | Release signed v3.8.2 macOS package. | **PLANNED** |

---

## 17. Acceptance criteria

v3.8.2 is accepted only if:

1. The pinned OGRE v14.6.3 installation remains the managed baseline.
2. `OgreTools` builds, packages, installs, and loads natively.
3. `Tools -> OGRE` is independent of project state.
4. Managed command-line tools outrank same-named PATH tools.
5. PATH is used only for missing command-line capabilities.
6. No shell profile or global `PATH` is modified.
7. Every capability is independently verified.
8. Every submenu open performs fresh discovery.
9. Action handlers revalidate required capabilities.
10. The existing `wizProject` registration continues to create ordinary OGRE projects.
11. The `wizCustom` registration shares the same `ogre/wizard.script` without duplicated installer code.
12. `Wizard.GetWizardType()` cleanly selects project versus setup-only behavior.
13. Both modes use identical setup pages and handlers.
14. Setup-only completion creates no project and leaves existing project state unchanged.
15. Project and Custom registrations remain separated by output-type filtering.
16. OgreTools locates the setup entry through `cbWizardPlugin` using script filename plus `totCustom`.
17. A clean menu-launched setup can reacquire source, configure, build, install, and validate OGRE without restart.
18. Setup completion becomes visible on the next `Tools -> OGRE` open without restart.
19. Native Git/CMake confirmation and progress dialogs are owned by the active wizard.
20. Closing a native operation dialog restores the OGRE wizard rather than the main frame.
21. SDK-folder and SampleBrowser actions freshly verify their targets.
22. The managed SampleBrowser contains `Codec_Assimp.framework` when the target exists.
23. The upstream-merged SampleBrowser correction remains locally patched for v14.6.3 until a containing tagged release is adopted and retested.
24. Command-line tool execution uses separated executable/arguments and captures output and exit status.
25. Conversion success is not inferred solely from `.mesh` existence.
26. The source asset is not modified. `resources.cfg` and the project file change only after the user confirms that specific change. A confirmed `resources.cfg` change adds the destination directory, not the mesh filename. A confirmed project change lists the generated files without compiling them. Declining either change leaves that file untouched.
27. Generated output does not default into installed SDK media.
28. Ordinary projects are not classified as OGRE projects merely because tools are installed.
29. The final macOS bundle passes packaging and code-signing validation.

---

## 18. What stays rejected

- Permanent `isOgreProject` classification.
- Project/configuration parsing for menu visibility.
- Global shell-startup modification or permanent PATH insertion.
- Unbounded filesystem searches.
- PATH overriding a valid managed tool.
- Persistent capability caching.
- Timers, watchers, or wizard refresh notifications when submenu-open refresh suffices.
- Launching the project wizard from Install / Repair.
- Duplicating installer logic in OgreTools.
- Duplicating `ogre/wizard.script` for setup-only use.
- New include machinery when dual registration plus `Wizard.GetWizardType()` solves script sharing.
- Dependency on concrete `Wiz` internals from OgreTools when `cbWizardPlugin` suffices.
- Translated-title matching when script filename plus output type is stable.
- Parenting wizard-launched native dialogs unconditionally to `Manager::Get()->GetAppWindow()`.
- Post-hoc `Raise()` / `SetFocus()` hacks when correct modal ownership solves the problem.
- Patching SampleBrowser at launch time.
- Removing the local SampleBrowser fix merely because it is present on untagged upstream master.
- Shell-string converter execution.
- `.mesh` existence as sole success criterion.
- Copying user assets into installed SDK media.
- Hidden resource/project rewrites.
- A `resources.cfg` entry that names a mesh file instead of its destination directory.
- Editing a `resources.cfg` that is not in the destination directory.
- Adding generated files to the project tree without confirmation.
- Marking generated `.mesh`, `.material`, or `resources.cfg` files for compilation.
- Converter machinery in generated `main.cpp`.
- Expansion into an asset database, package manager, or scene editor.

---

## 19. Current critical path

```text
managed locator + PATH fallback                  DONE
live submenu refresh                             DONE
independent Tools -> OGRE actions                DONE
SampleBrowser bundle correction                  DONE LOCALLY + MERGED UPSTREAM
        |
        v
setup-only OGRE wizard                           DONE
        |
        +-- dual registration                    DONE
        +-- Project/Custom separation            DONE
        +-- shared setup-page helper             DONE
        +-- wizard-type branch                   DONE
        +-- SetupCustom / OgreMacAllReady         DONE
        +-- no-project completion                PROVEN
        +-- public menu invocation               PROVEN
        +-- clean reacquire/configure/build       PROVEN
        +-- install + validation                 PROVEN
        +-- live capability refresh              PROVEN
        +-- native modal ownership               FIXED + PROVEN
        |
        v
Import 3D asset dialog                           DONE
safe converter process + OGRE log                DONE
        |
        v
conversion validation                            NEXT
explicit resources.cfg and project-tree registration   DONE
        |
        v
regression + signed v3.8.2 release
```

---

## 20. Done when

**Status: IN PROGRESS.**

v3.8.2 is complete when Code::Blocks exposes installed OGRE utilities through the native `OgreTools` plugin under `Tools -> OGRE`, resolves capabilities using managed-first/PATH-fallback precedence, refreshes availability on demand, launches and visibly maintains setup-only OGRE installation/repair through the existing shared scripted-wizard machinery without creating a project or duplicating installer code, and safely runs `OgreAssimpConverter` through a logged asset-import workflow without requiring project classification or global `PATH` modification. After a successful conversion, the user can confirm adding the destination directory to the `resources.cfg` in that directory and, when a project is open, listing the generated files in the project tree. Declining those choices leaves both files unchanged.
