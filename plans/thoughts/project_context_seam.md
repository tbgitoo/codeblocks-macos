# Code::Blocks project-context seam

## Status

**Investigation preserved; implementation deferred.**

The original v3.8.2 design proposed deriving toolkit identity and matching tool installations from the active project's effective build configuration. That architecture is no longer required for the immediate OGRE-tools feature. The simpler implementation may expose the OGRE menu whenever the plugin is available, discover each installed tool from a small ordered set of expected locations plus `PATH`, disable unavailable tool actions individually, and retain an enabled installation action when tools are missing.

The project-context investigation remains useful for future features that genuinely need active-project or active-target configuration. This note records the seams and constraints established during that investigation.

---

## 1. Project-menu extension seam

Ordinary Code::Blocks plugins can contribute to the existing **Project** menu through `cbPlugin::BuildMenu(wxMenuBar*)`.

In-tree precedents use:

```cpp
int pos = menuBar->FindMenu(_("&Project"));
if (pos != wxNOT_FOUND)
{
    wxMenu* menu = menuBar->GetMenu(pos);
    // menu->Insert(...);
    // menu->Append(...);
    // menu->InsertSeparator(...);
    // menu->AppendSeparator(...);
}
```

Relevant precedents:

- CodeCompletion retains the project-menu pointer and later enables or disables an item.
- CompilerGCC finds `Properties...` and inserts `Build options...` immediately before it.

**Conclusion:** a `Project -> OGRE` submenu requires no modification of the Code::Blocks core menu implementation.

---

## 2. Active-project and active-target lifecycle seams

### Active project

`ProjectManager::SetProject()` updates `m_pActiveProject` and emits:

```text
cbEVT_PROJECT_ACTIVATE
```

Plugins subscribe through:

```cpp
Manager::Get()->RegisterEventSink(...);
```

The event exposes the active `cbProject*` through:

```cpp
event.GetProject()
```

### Project open

```text
cbEVT_PROJECT_OPEN
```

is distinct from activation. It indicates completion of project loading and may be deferred while a workspace is loading. It must not be treated as synonymous with active-project change.

### Active build target

`cbProject::SetActiveBuildTarget()` updates `m_ActiveTarget` and emits:

```text
cbEVT_BUILDTARGET_SELECTED
```

Existing DebuggerManager code treats this as an invalidation notification without requiring a target payload.

### Project close

```text
cbEVT_PROJECT_CLOSE
```

provides the natural point to discard or recompute project-dependent state.

### Minimal lifecycle contract

```text
cbEVT_PROJECT_ACTIVATE
        -> re-read current context

cbEVT_BUILDTARGET_SELECTED
        -> re-read current context

cbEVT_PROJECT_CLOSE
        -> re-read or clear current context
```

For a future project-context feature, on-demand reconstruction remains safer than permanent toolkit identity or incompletely invalidated caching.

---

## 3. Reading the effective active-project configuration

### Active objects

```cpp
cbProject* project =
    Manager::Get()->GetProjectManager()->GetActiveProject();

ProjectBuildTarget* target = project
    ? project->GetBuildTarget(project->GetActiveBuildTarget())
    : nullptr;
```

A passive reader can obtain configuration through the public project and target APIs, including:

```cpp
GetIncludeDirs()
GetLibDirs()
GetLinkLibs()
```

### Project-target ordering

Target inheritance must honor `OptionsRelation` independently for each option class:

```text
orUseParentOptionsOnly   -> project
orUseTargetOptionsOnly   -> target
orPrependToParentOptions -> target + project
orAppendToParentOptions  -> project + target
```

The confirmed relations are:

```text
GetIncludeDirs() -> ortIncludeDirs
GetLibDirs()     -> ortLibDirs
GetLinkLibs()    -> ortLinkerOptions
```

`compilercommandgenerator.cpp` confirms that linked libraries use `ortLinkerOptions`:

```cpp
libs = GetOrderedOptions(
    target,
    ortLinkerOptions,
    target->GetParentProject()->GetLinkLibs(),
    target->GetLinkLibs());
```

No side-effect-free public ordering helper was found outside the protected `CompilerCommandGenerator::GetOrderedOptions()`. A passive consumer may therefore use a small local helper that mirrors the four public `OptionsRelation` semantics exactly.

### Macro expansion

After selecting the effective values, use target-aware Code::Blocks expansion:

```cpp
MacrosManager::ReplaceMacros(value, target);
```

This preserves Code::Blocks' own handling of project, target, global and environment-derived macros instead of introducing a second parser.

### Relative paths

Expanded relative paths should be resolved against:

```cpp
project->GetBasePath()
```

Lexical normalization may handle home expansion, redundant separators and `.` / `..` components. Passive discovery must not rewrite project configuration.

### Important rejected shortcut

Do not call:

```cpp
Compiler::GetCommandGenerator(project)
CompilerCommandGenerator::Init(project)
```

merely to inspect effective configuration.

`CompilerCommandGenerator::Init(project)` is not passive. It changes the working directory, emits `cbEVT_COMPILER_SET_BUILD_OPTIONS`, executes `SetBuildOptions` build scripts, and initializes broader command-generation state.

---

## 4. Snapshot boundary established during the prototype

The prototype used a value-only snapshot rather than retaining `cbProject*` or `ProjectBuildTarget*` pointers.

The useful shape was:

```cpp
enum class ProjectToolSettingScope
{
    Project,
    Target
};

struct ProjectToolPath
{
    wxString rawValue;
    wxString expandedValue;
    wxString normalizedPath;
    ProjectToolSettingScope scope;
};

struct ProjectToolValue
{
    wxString rawValue;
    wxString expandedValue;
    ProjectToolSettingScope scope;
};

struct ProjectToolContext
{
    wxString projectTitle;
    wxString projectFilename;
    wxString projectBasePath;
    wxString targetTitle;

    wxVector<ProjectToolPath> includeDirectories;
    wxVector<ProjectToolPath> libraryDirectories;
    wxVector<ProjectToolValue> linkedLibraries;
};
```

Keeping raw, expanded and normalized values separately supports transparent diagnostics and avoids erasing unresolved or invalid configuration evidence.

The prototype deliberately did not retain project or target pointers in the returned snapshot. Reconstructing the snapshot on demand avoids stale identity after a project closes or the active target changes.

---

## 5. OGRE evidence confirmed during the investigation

The known-good v3.8.1-generated OGRE project contains project-level settings inherited by both Debug and Release targets. Independent configuration signals converge on:

```text
PREFIX=/Users/thomasbraschler/Library/CodeBlocks/ogre-14.6.3
```

Observed project evidence includes:

```text
PREFIX/include/OGRE
PREFIX/include/OGRE/Bites
PREFIX/include/OGRE/Overlay
PREFIX/include/OGRE/RTShaderSystem
PREFIX/lib/macosx/Release
```

The project also links OGRE frameworks from the same prefix, including:

```text
Ogre.framework
OgreBites.framework
OgreOverlay.framework
OgreRTShaderSystem.framework
```

The installed prefix independently contains:

```text
PREFIX/include/OGRE
PREFIX/bin/macosx/OgreAssimpConverter
PREFIX/bin/macosx/OgreMeshUpgrader
PREFIX/bin/macosx/OgreXMLConverter
PREFIX/bin/macosx/VRMLConverter
PREFIX/bin/macosx/SampleBrowser.app
```

If project-context detection is revisited, an effective include path ending in `/include/OGRE` is a strong structural signal from which `PREFIX` can be derived. Sibling component includes and library/framework paths can corroborate or contradict that candidate.

Toolkit identity and individual tool availability should remain separate concepts. Missing `VRMLConverter`, for example, must not erase otherwise valid OGRE identity or disable unrelated tools.

---

## 6. Safe process-execution seam

The existing macOS `macgit.cpp` implementation proves the required structured process boundary:

```cpp
const wxChar* argv[] = {
    executable.wx_str(),
    /* separate arguments */
    nullptr
};

m_pid = wxExecute(argv, wxEXEC_ASYNC, m_process, &env);
```

The macOS build uses wxWidgets 3.2.10, whose `wxExecute` overloads support structured `char**` / `wchar_t**` argument vectors.

`wxExecuteEnv` provides an explicit working directory and environment. Together with `wxProcess`, the available execution lifecycle includes:

```text
verified executable path
separate argument vector
explicit working directory
explicit environment
redirected stdout
redirected stderr
wxEVT_END_PROCESS
actual exit status through event.GetExitCode()
retained PID for cancellation where appropriate
```

### Rejected execution boundary

SDK `PipedProcess` is useful precedent for redirected-stream and event behavior, but its current:

```cpp
Launch(const wxString& command)
```

flattens execution into a command string. It is not the selected boundary for converter invocation.

No user-controlled shell command should be constructed.

---

## 7. Plugin-owned log seam

Cccc and CppCheck establish the lifecycle for a dedicated plugin log page using:

```text
TextCtrlLogger
LogManager::SetLog()
cbEVT_ADD_LOG_WINDOW
LogManager::Log()
cbEVT_SWITCH_TO_LOG_WINDOW
cbEVT_REMOVE_LOG_WINDOW
```

A future OGRE conversion action can therefore capture process output and present it in a dedicated Code::Blocks log tab without adding logging machinery to generated user code.

---

## 8. Native plugin packaging seam proven on macOS

A minimal in-tree `ProjectTools` plugin was built and loaded successfully.

The relevant pieces were:

```text
src/plugins/projecttools/
    projecttools.h
    projecttools.cpp
    projecttoolcontext.h
    projecttoolcontext.cpp
    toolkitprovider.h
    toolkitdetection.h
    toolkitdetection.cpp
    manifest.xml
    Makefile.am
```

Build-system integration required:

- an `AC_CONFIG_FILES` entry for `src/plugins/projecttools/Makefile`;
- a `BUILD_PROJECTTOOLS` Automake conditional in `m4/acinclude.m4`;
- `MAYBE_PROJECTTOOLS` in `src/plugins/Makefile.am`;
- `projecttools` in `DIST_SUBDIRS`;
- `libprojecttools.la` in the plugin's `Makefile.am`;
- a `projecttools.zip` resource package containing `manifest.xml`.

The plugin registration was:

```cpp
namespace
{
    PluginRegistrant<ProjectTools> reg(_T("ProjectTools"));
}
```

On macOS the installed pairing follows the established convention:

```text
share/codeblocks/plugins/libprojecttools.dylib
share/codeblocks/projecttools.zip
```

After the normal macOS bundle and code-signing workflow, Code::Blocks reported:

```text
Loaded 15 plugins
Loading:
...
ProjectTools
ProjectTools: plugin attached
...
```

This proves dynamic discovery, manifest acceptance, registration and `OnAttach()` execution.

Changing a `Makefile.am` resource name required rerunning `./bootstrap` and the normal configure step before rebuilding. Otherwise the generated Makefile retained stale `pkgdata_DATA` values.

---

## 9. Simplified v3.8.2 direction

The immediate feature no longer needs to infer whether the active project uses OGRE.

A simpler implementation can use:

```text
Project menu
    -> OGRE
        -> Import 3D asset...        enabled only if converter found
        -> Tools
            -> OgreAssimpConverter   enabled only if found
            -> OgreMeshUpgrader      enabled only if found
            -> OgreXMLConverter      enabled only if found
            -> VRMLConverter         enabled only if found
        -> Open SampleBrowser        enabled only if found
        -> Open SDK tools folder     enabled when a tools directory exists
        -> Install OGRE tools...     enabled when required tools are missing
```

Recommended discovery model:

```text
1. expected v3.8.1 installation layout
2. a short list of explicit platform-specific candidate layouts
3. PATH as a fallback
4. verify every executable or application independently
5. enable or disable each action independently
```

For the verified macOS baseline, the primary candidate is:

```text
~/Library/CodeBlocks/ogre-14.6.3/bin/macosx/
```

The menu may be shown regardless of active project context. If tools are unavailable, their actions remain visible but disabled, while an installation action remains available.

The safe process and logging seams documented above remain directly applicable.

---

## 10. Preserved rules for any future project-context feature

If project-context discovery is revived, retain these constraints:

- derive context from current effective project and target configuration, not wizard ancestry;
- honor `OptionsRelation` separately for include directories, library directories and linked libraries;
- use `MacrosManager::ReplaceMacros(value, target)` rather than an ad hoc macro parser;
- resolve relative paths against the project base path;
- do not invoke `CompilerCommandGenerator::Init(project)` for passive inspection;
- reconstruct on demand or define complete invalidation before caching;
- preserve conflicting roots rather than silently selecting the first;
- keep toolkit identity separate from individual tool capabilities;
- allow multiple independent providers rather than assigning one exclusive project type;
- avoid project mutation merely to detect context;
- retain raw, expanded and normalized evidence for diagnostics.

---

## 11. Current disposition

```text
Project-menu plugin seam                 PROVEN
Native plugin build/package/load         PROVEN
Project/target lifecycle events          LOCATED
Passive effective configuration          LOCATED
OptionsRelation semantics                CONFIRMED
Target-aware macro expansion             LOCATED
Safe structured process launch           LOCATED
Plugin-owned logging                     LOCATED
OGRE structural project evidence         CONFIRMED
Full project-context provider             DEFERRED
Simpler installed-tool discovery          SELECTED
```

The project-context work is therefore not discarded. It is preserved as a tested seam for a future feature whose behavior actually depends on the active project's effective build configuration.
