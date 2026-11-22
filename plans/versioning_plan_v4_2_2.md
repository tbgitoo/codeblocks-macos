# Code::Blocks v4.2.2 — OGRE tools on Windows

**Status: Planning.** v3.8.2 already provides `Tools -> OGRE` on macOS. v4.2 already installed the Windows SDK. This version loads that plugin in the Windows IDE and points its locator at that SDK. The menu, import dialog, converter run, and setup wizard stay as they are.

**Question this version answers:** after the Windows IDE is rebuilt, does `Tools -> OGRE` import a 3D asset, open the installed tools folder, launch SampleBrowser, and open the existing setup wizard, using `%LOCALAPPDATA%\codeblocks\ogre-14.6.3`?

```text
Tools -> OGRE
        |
        +-- Import 3D asset...                 OgreAssimpConverter.exe
        +-- Open SDK tools folder              managed bin
        +-- Open SampleBrowser                 SampleBrowser.exe
        +-- Install / Repair OGRE tools...     existing setup wizard
```

The menu is an IDE tool. It does not ask whether the open project is an OGRE project. A missing file disables only the action that needs it. Install / Repair stays enabled. `OgreMeshUpgrader`, `OgreXMLConverter`, and `VRMLConverter` stay in the tools folder. Import is the only action that runs a converter.

---

## 1. Code to write

Three edits. Nothing in the wizard, the installer, or `update.bat`.

### 1.1 Windows plugin target

`src/plugins/ogretools/` is built on macOS by `Makefile.am`. `src/CodeBlocks_wx33_64.cbp` has no OgreTools target, so a Windows build of this IDE produces no `ogretools.dll`. `PluginManager` loads every `*.dll` in the plugins directory and refuses a DLL whose `<basename>.zip` cannot be found. The zip has to contain `manifest.xml`.

The closest targets are Open files list and XP look & feel: one DLL, `manifest.xml` only, no XRC, no extra libraries. Copy the Open files list target.

```text
title        OGRE Tools
output       devel33_64/share/CodeBlocks/plugins/ogretools
             prefix_auto and extension_auto, type dynamic library
             -> devel33_64/share/CodeBlocks/plugins/ogretools.dll
objects      .objs33_64
dependency   devel33_64/libcodeblocks.a
compiler     -DBUILDING_PLUGIN, $(#CB_CPP_STD), include directory "include"
linker       the same five -Wl options, -mthreads,
             libraries codeblocks and wxmsw$(WX_VERSION)$(WX_SUFFIX)
post-build   zip -jq9 devel33_64\share\CodeBlocks\ogretools.zip
             plugins\ogretools\manifest.xml
             Mode after=always
```

Units, each restricted to this target so they are not compiled into every other target:

```text
plugins/ogretools/ogretools.cpp
plugins/ogretools/ogretools.h
plugins/ogretools/ogreimportdlg.cpp
plugins/ogretools/ogreimportdlg.h
plugins/ogretools/ogretoolslocator.cpp
plugins/ogretools/ogretoolslocator.h
plugins/ogretools/manifest.xml
```

Add `OGRE Tools` to the `All` alias and to the `Core app & plugins` alias. Both aliases list every other plugin.

`src/update.bat` copies `plugins\*.dll` and `*.zip` with wildcards. Once the target has written `ogretools.dll` and `ogretools.zip`, that copy includes them. `packaging/windows/stage.bat` copies the output tree and walks every DLL already in `share\CodeBlocks\plugins`. Neither file names the existing plugins one by one for that copy. `update.bat` also has its own explicit zip commands, and those repeat the post-build commands of the plugins that already exist. A new line there is a second writer of the same zip. It is not required to load or stage the plugin.

`Makefile.am` stays the autotools build. The NSIS script stays as it is.

The plugin is loaded on the next start of the IDE that uses this `devel33_64` tree. A different installed copy of Code::Blocks does not see the new DLL.

### 1.2 Managed Windows root

`LocateOgreTools()` fills `managedPrefix`, `toolsDirectory`, `assimpConverter`, and `sampleBrowser` only inside `__WXOSX__`. The PATH fallback below that block already runs on every platform, and only for the converter.

The Windows install root is the one the wizard uses. `CodeBlocksRoot()` in `wincmake.cpp` is:

```text
%LOCALAPPDATA%\codeblocks
```

That function sits in the anonymous namespace of `wincmake.cpp`. `ogretools` does not link `scriptedwizard.dll` and cannot call it. `ResolveWinCMakePaths()` is visible to the wizard script, and it also creates the directory and rejects forbidden locations. The locator does not call it.

`wxStandardPaths::GetUserLocalDataDir()` is a different lookup. With `SetAppName("codeblocks")` and the default `UseAppInfo(AppInfo_AppName)`, it appends `codeblocks` to the folder returned by `SHGetFolderPath(CSIDL_LOCAL_APPDATA)`. The wizard reads the `LOCALAPPDATA` environment variable instead. `wincmake.cpp` already records that this directory may be a junction. The locator uses the environment variable so it names the same directory the wizard installed.

The Windows branch is the macOS branch with this root and these file names:

```text
managedPrefix      %LOCALAPPDATA%\codeblocks\ogre-14.6.3
tools directory    <managedPrefix>\bin
converter          OgreAssimpConverter.exe in that bin
SampleBrowser      SampleBrowser.exe in that bin
```

`SampleBrowser.exe` is a file. The existing `ExistingExecutable` helper applies. The macOS check for a `SampleBrowser.app` directory stays inside `__WXOSX__`. An absent `bin` clears `toolsDirectory` and leaves `managedPrefix` set, as the macOS branch does.

The PATH fallback stays after the managed lookup and still runs only when the managed converter is empty. `wxFindFileInPath` appends the given name to each PATH entry and calls `wxFileExists`. It does not consult `PATHEXT`. On Windows the looked-up name is `OgreAssimpConverter.exe`. The macOS name stays `OgreAssimpConverter`.

```text
managed    PATH    selected
yes        yes     managed
yes        no      managed
no         yes     PATH
no         no      Import disabled
```

The tools directory and SampleBrowser are never taken from `PATH`.

### 1.3 Case-only path comparison

Once `managedPrefix` is set, two comparisons in `ogreimportdlg.cpp` decide whether a destination is inside that prefix and whether `resources.cfg` already lists it. Both are case-sensitive outside macOS. Windows paths that differ only by case are the same directory.

`DirectoryIsInside()` already compares `Lower()` copies under `__WXOSX__`. `CanonicalDirectory()` already returns a `Lower()` copy under `__WXOSX__`, and its callers use that copy only with `==`:

```text
DestinationAlreadyListed()     FileSystem= value against the destination
texture copy                   source directory against the destination
```

Apply those two existing folds under `__WXMSW__` as well. The strings passed to `wxExecute`, written as `FileSystem=`, and stored as the accepted destination stay the original spelling. `AppendDestinationLocation()` writes `destination`, not the comparison copy.

`wincmake.cpp` has a separate `Canonical()` that lowercases, rewrites slashes, and is followed by junction resolution. That function is private to the wizard and is not the comparison used here.

---

## 2. Already implemented

These run once the plugin is loaded and the locator returns Windows paths. This version does not rewrite them.

| Behavior | Where |
|---|---|
| `Tools -> OGRE` and the four items | `OgreTools::BuildMenu` |
| Enable Import, the folder, and SampleBrowser from the locator. Leave Install / Repair enabled. | `RefreshCapabilities` |
| Refresh on submenu open, and again in each handler | `OnOgreMenuOpen`, `OnImportAsset`, `OnOpenToolsFolder`, `OnOpenSampleBrowser` |
| Import dialog | `PromptOgreImport` |
| Arguments `-log`, source, destination. No shell string. | `RunOgreAssimpImport` |
| Stdout, stderr, exit status, and the converter log in the `OGRE Tools` log | `wxExecute` with `wxPROCESS_REDIRECT`, `WriteLog` |
| Working directory is the directory that contains `plugins.cfg`, else its parent | `ConverterWorkingDirectory` |
| Mesh, material, referenced-file, and texture outcome | `RunOgreAssimpImport` |
| Ask before editing `resources.cfg`. Ask before listing files in an open project. A declined answer leaves that file unchanged. | `RegisterOgreImport` |
| Open the managed directory and SampleBrowser | `wxLaunchDefaultApplication` |
| Launch the custom wizard whose script is `ogre/wizard.script` | `OnInstallOgre` |
| Setup pages, then return. `SetupCustom()` returns `OgreWinAllReady()`. No project is created. | `BeginWizard`, `AddOgreWinSetupPages`, `SetupCustom` |
| Git and CMake dialogs use the wizard window as parent | `Wiz_WinGitClone` and the WinCMake bindings pass `GetWizardWindow()` |

On this install, `plugins.cfg` is in the same `bin` as `OgreAssimpConverter.exe`, so the existing working-directory walk uses that `bin`. The macOS walk still uses the parent of `bin/macosx`.

The v4.2 install already placed `SDL2.dll`, `libfreetype.dll`, `libassimp-5.dll`, and the selected MinGW runtime beside those executables. Windows searches the executable directory before `PATH`. This version does not copy those DLLs again.

---

## 3. Context

v4.2 showed that a `libstdc++-6.dll` found on `PATH` can be a different build from the one SampleBrowser was linked with, and that the executable directory is searched first. That is why SampleBrowser is taken only from the managed `bin`, which already contains the matching runtime. It is not a new copy step.

The four command-line tools print usage and exit when started with no source file. Import is how this version supplies a source file to `OgreAssimpConverter.exe`. The other three are reached by opening the tools folder.

---

## 4. Milestones

| | Objective | Check |
|---|---|---|
| M1 | Build and load OgreTools in the Windows IDE. | **Done.** `OGRE Tools` was added to `src/CodeBlocks_wx33_64.cbp`, including the `All` and `Core app & plugins` aliases. The bootstrap IDE built that target with exit 0. It produced `devel33_64/share/CodeBlocks/plugins/ogretools.dll` (704978 bytes) and `devel33_64/share/CodeBlocks/ogretools.zip` (530 bytes). The zip contains `manifest.xml` only. `src/devel33_64/codeblocks.exe` was started, and `Tools -> OGRE` showed Import 3D asset..., Open SDK tools folder, Open SampleBrowser, and Install / Repair OGRE tools.... No M1 check remains. `Makefile.am` was not changed. |
| M2 | Resolve the managed Windows SDK. | With the v4.2 SDK present, Import and SampleBrowser enable and name the executables in `%LOCALAPPDATA%\codeblocks\ogre-14.6.3\bin`. A same-named converter elsewhere on `PATH` is not the one Import runs. With that `bin` absent, Import follows `PATH` only for `OgreAssimpConverter.exe`, and SampleBrowser stays disabled. A destination inside the managed prefix is refused when it differs only by case. A `resources.cfg` entry that already names the destination, differing only by case, is not offered again. |
| M3 | Exercise the existing actions. | Import writes a `.mesh` and a `.material` outside the SDK and logs the command, both streams, and the exit status. The folder action opens the managed `bin`. SampleBrowser opens. Install / Repair opens `OGRE SDK setup` and creates no project. An SDK that already matches is left in place. `resources.cfg` and the project file change only after the matching confirmation. |

---

## 5. Acceptance

1. The menu items are Import 3D asset..., Open SDK tools folder, Open SampleBrowser, and Install / Repair OGRE tools....
2. The managed `bin` wins over a converter of the same name on `PATH`.
3. SampleBrowser and the tools folder come only from that `bin`.
4. The user's `PATH` and shell startup stay unchanged.
5. Opening the submenu discovers the files again.
6. Import uses separate arguments, captures both streams and the exit status, and writes the `OGRE Tools` log.
7. A destination inside the managed prefix is refused, including a path that differs only by case. The path that is executed or written keeps its original spelling.
8. `resources.cfg` and the project file change only after the matching confirmation.
9. Install / Repair runs the existing Windows setup pages and creates no project.
10. The wizard, its patches, its options file, its DLL copies, `update.bat`, and the NSIS script stay as they are.
11. The macOS locator and the macOS `SampleBrowser.app` check stay as they are.

---

## 6. Stays out

- A menu item that launches `OgreAssimpConverter`, `OgreMeshUpgrader`, `OgreXMLConverter`, or `VRMLConverter` directly.
- Classifying the open project as an OGRE project.
- Inserting the SDK `bin` into `PATH`.
- A disk search, `OGRE_HOME`, a new environment variable, or a saved discovery result.
- Calling `ResolveWinCMakePaths()`, `GetUserLocalDataDir()`, or the wizard's private `Canonical()`.
- Moving `CodeBlocksRoot()` into a shared library.
- A second copy of `ogre/wizard.script`.
- Deleting the installed SDK to prove the wizard.
- Another DLL copy into the SDK `bin`.
- An edit to `update.bat`, `stage.bat`, or the NSIS script.
- A new OGRE or Assimp pull request. v4.2.1 remains that track.

---

## 7. Done when

On Windows, `Tools -> OGRE` offers Import 3D asset..., Open SDK tools folder, Open SampleBrowser, and Install / Repair OGRE tools.... Import runs the managed `OgreAssimpConverter.exe` on a file the user chooses and logs the result. The tools folder and SampleBrowser open from the v4.2 `bin`. Install / Repair opens the existing setup wizard and does not create a project. The macOS menu is unchanged.

---

## 8. Investigation findings

Files inspected:

```text
src/CodeBlocks_wx33_64.cbp
src/update.bat
packaging/windows/stage.bat
src/sdk/pluginmanager.cpp
src/src/app.cpp
src/plugins/ogretools/ogretools.cpp
src/plugins/ogretools/ogretools.h
src/plugins/ogretools/ogretoolslocator.cpp
src/plugins/ogretools/ogretoolslocator.h
src/plugins/ogretools/ogreimportdlg.cpp
src/plugins/ogretools/ogreimportdlg.h
src/plugins/ogretools/manifest.xml
src/plugins/ogretools/Makefile.am
src/plugins/scriptedwizard/wincmake.cpp
src/plugins/scriptedwizard/wincmake.h
src/plugins/scriptedwizard/winsdk.cpp
src/plugins/scriptedwizard/wiz.cpp
src/plugins/scriptedwizard/resources/config.script
src/plugins/scriptedwizard/resources/ogre/wizard.script
wxWidgets 3.3.4 src/common/filefn.cpp
wxWidgets 3.3.4 src/common/stdpbase.cpp
wxWidgets 3.3.4 src/msw/stdpaths.cpp
```

What requires modification:

```text
src/CodeBlocks_wx33_64.cbp                         M1 target, units, both aliases
src/plugins/ogretools/ogretoolslocator.cpp         M2 Windows root, .exe names, PATH name
src/plugins/ogretools/ogreimportdlg.cpp            M2 case-only comparison in DirectoryIsInside and CanonicalDirectory
```

What is already implemented: menu construction, submenu refresh, the import dialog, separate converter arguments, stdout/stderr/exit-status logging, converter-log handling, the `plugins.cfg` working directory, output validation, both registration confirmations, Open SDK tools folder, Open SampleBrowser, Install / Repair, and the Windows setup-only wizard. WinGit and WinCMake already parent their dialogs to `GetWizardWindow()`.

Assumptions in the previous draft that were wrong:

- `CodeBlocksRoot()` was described as a helper the locator could repeat by convention. It is private to `wincmake.cpp`. The repeat is required because no shared declaration exists, not because a shared helper was rejected.
- `GetUserLocalDataDir()` was set aside without saying why. It uses the shell folder, not the `LOCALAPPDATA` variable the wizard reads.
- `update.bat` was described as something a later installer happens to pick up. Its DLL and zip copies are wildcards, and `stage.bat` walks every staged plugin DLL. No packaging edit is required. The explicit per-plugin zip lines in `update.bat` are a second writer for plugins that already have a post-build zip.
- The draft gave M3, M4, and M5 to opening the folder, importing, and Install / Repair. Those handlers already exist. They are one regression pass.
- The draft treated the OpenGL 3+ sample and the MinGW runtime copies as work for this version. Both belong to the v4.2 install.

Unresolved question: none. Implementation can start from section 1.
