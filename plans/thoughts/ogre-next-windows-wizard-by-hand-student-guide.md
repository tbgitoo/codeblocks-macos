Students write the Windows OGRE-Next wizard

You are adding the Windows half of the OGRE-Next project wizard. The Mac wizard already exists. You do not rebuild OGRE-Next by hand, and you do not create the user's project file yourself.

The stages are the ones in `plans/versioning_plan_v3_8.md`: find the tools, download the pinned source, configure with the install folder chosen first, build, install, then generate an ordinary Code::Blocks program that uses only that install. The engine is OGRE-Next, so there are two source trees and the two MinGW patches already in this repository. The Apple patches stay on the Mac wizard.

The pins are:

```text
ogre-next-deps   f7e23191f4fd89ed41134996bc47f9ee23d009df
SDL2             5d249570393f7a37e037abf22cd6012a4cc56a71
OGRE-Next        4859016ad2cc956a653dfc4e4a859505696c6509
```

The compiler the wizard must pass to CMake is the MinGW selected in Code::Blocks, `D:\Program Files\CodeBlocks\MinGW` on this machine. The install root is `%LOCALAPPDATA%\codeblocks`, which `CodeBlocksRoot()` in `wincmake.cpp` already builds. Do not write `D:\Users\thoma\...` into the wizard.


0. Leave these alone ====================================================

Do not edit `resources/ogre/wizard.script`. That is the OGRE 1.x wizard.

Do not call `Wizard.WinCMakeConfigure`. It adds the FLTK `-D` flags and tells the user it is configuring FLTK.

Do not call `Wizard.WinGitClone` for these two pins. It runs `git clone --branch <name> --depth 1`, and these pins are commits, not branch tips.

Do not apply `ogre-next-deps-apple` or `ogre-apple`. Those are the Mac patches. `WinPatchProbe` and `WinPatchApply` already know the two MinGW patch ids, so you do not add a patch table.

Do not give the Squirrel script a way to pass its own `-D` flag, URL, or commit. The Mac rule is the same: the C++ side owns those values.


1. `resources/config.script` ============================================

The registration is currently inside `if (PLATFORM == PLATFORM_MAC)`.

Change it so both Mac and Windows register the same wizard name:

```javascript
if (PLATFORM == PLATFORM_MAC || PLATFORM == PLATFORM_MSW)
    RegisterWizard(wizProject, _T("ogre-next"),  _("OGRE-Next project"),     _("2D/3D Graphics"));
```

Reason: Windows never shows the wizard until this list contains it.


2. `wingit.h` and `wingit.cpp` — download the two pins ==================

Add a public function next to the existing WinGit declarations:

```cpp
wxString AcquireWinOgreNext(wxWindow* parent,
                            const wxString& sourceId,
                            const wxString& explanation,
                            const wxString& confirmLabel);
```

`sourceId` may only be `ogre-next-deps` or `ogre-next`. Any other id returns `success=0` and does not start Git.

Copy the known-source table from `macgit.cpp` (`kKnownSources`): same ids, URLs, commits, leaf names, and the SDL2 submodule pin. The destination is not `~/Library/CodeBlocks`. Build it as `%LOCALAPPDATA%\codeblocks\` plus the leaf:

```text
ogre-next-deps-source-3.0.0
ogre-next-source-3.0.0
```

`CodeBlocksRoot()` is private inside `wincmake.cpp`, so repeat that `LOCALAPPDATA` lookup here. An empty `LOCALAPPDATA` is a failure and does not start Git.

Reason: the script must not be able to choose a different commit or a different folder.

Git commands, in this order, only after the user confirms. Use `git -C` the way `CheckoutCommands` in `macgit.cpp` does:

```text
git init <destination>
git -C <destination> remote add origin <url>
git -C <destination> fetch --depth 1 origin <commit>
git -C <destination> checkout --detach FETCH_HEAD
```

For `ogre-next-deps` only, one more command:

```text
git -C <destination> submodule update --init --recursive --depth 1
```

Then require `src/SDL2` to be `5d249570393f7a37e037abf22cd6012a4cc56a71`. If it is not, `success=0`.

Reason: `git clone --branch` cannot fetch these commits, and the dependency tree is unusable until that SDL2 commit is present.

If the destination is already that commit, and for the dependencies also that SDL2 commit, return `success=1` and do not run Git. If the directory exists and is a different checkout, leave it untouched and return `success=0`. Cancel before Git starts does not create the directory.

Return one report string, the same shape as `FormatMacGitAcquire`:

```text
success=0|1
cancelled=0|1
output=<one line>
```

In `wiz.cpp`, next to `BindMethod(..., "WinGitClone", ...)`, add:

```cpp
BindMethod(v, _SC("WinOgreNextAcquire"), Wiz_WinOgreNextAcquire, _SC("Wiz::WinOgreNextAcquire"));
```

The Squirrel wrapper takes the three strings and passes `GetWizardWindow()` as the parent, the same way `Wiz_WinGitClone` does.

Reason: a new C++ function is invisible to `wizard.script` until it is bound.


3. `wincmake.h` and `wincmake.cpp` — configure, build, install ===========

Add:

```cpp
wxString RunWinCMakeOgreNext(wxWindow* parent,
                             const wxString& projectId,
                             const wxString& operation,
                             const wxString& explanation,
                             const wxString& confirmLabel);
```

`projectId` is only `ogre-next-deps` or `ogre-next`. `operation` is only `configure`, `build`, or `install`. Anything else returns `success=0` and does not start CMake.

Resolve the folders with the same root as section 2:

```text
deps source     %LOCALAPPDATA%\codeblocks\ogre-next-deps-source-3.0.0
deps build      <deps source>\build-codeblocks
deps install    <deps build>\ogredeps

engine source   %LOCALAPPDATA%\codeblocks\ogre-next-source-3.0.0
engine build    <engine source>\build-codeblocks
engine install  %LOCALAPPDATA%\codeblocks\ogre-next-3.0.0
```

Take `gcc`, `g++`, `mingw32-make`, and the compiler `bin` from `MingwTools` for the wizard's selected compiler. `windres.exe` is that `bin` plus `windres.exe`.

Reason: a dependency built by a different MinGW will not link with the program this wizard generates.

Configure arguments for the dependencies:

```text
-S <deps source>
-B <deps build>
-G MinGW Makefiles
-DCMAKE_BUILD_TYPE=Release
-DCMAKE_C_COMPILER=<gcc>
-DCMAKE_CXX_COMPILER=<g++>
-DCMAKE_MAKE_PROGRAM=<mingw32-make>
-DCMAKE_RC_COMPILER=<windres>
```

Do not pass `CMAKE_INSTALL_PREFIX` for the dependencies.

Reason: that project writes `ogredeps` only while the prefix is still CMake's default.

Configure arguments for the engine. Same generator and compilers, plus:

```text
-DCMAKE_INSTALL_PREFIX=<engine install>
-DOGRE_DEPENDENCIES_DIR=<deps install>
-DOGRE_BUILD_SAMPLES2=OFF
-DOGRE_BUILD_TESTS=OFF
```

Do not pass `-DCMAKE_OSX_ARCHITECTURES` or `-DCMAKE_OSX_SYSROOT`. Do not pass any `FLTK_` flag.

Reason: those Apple and FLTK flags are for the other wizards, and the install prefix has to be stored in the cache before the build.

`build` runs `cmake --build <build> --parallel <N>`, where `<N>` is `BoundedJobCount()`. `install` runs `cmake --install <build>` with no `--prefix`.

Configure is successful only when the process exits 0 and `<build>\CMakeCache.txt` exists. For the engine, the cache must also record the install prefix and `OGRE_DEPENDENCIES_DIR` you passed. Install is successful only when these files exist:

```text
<engine install>\include\OGRE\OgreRoot.h
<engine install>\include\OGRE\Hlms\Unlit\OgreHlmsUnlit.h
<engine install>\include\OGRE\Hlms\Pbs\OgreHlmsPbs.h
<engine install>\lib\Release\libOgreMain.dll.a
<engine install>\lib\Release\libOgreHlmsPbs.dll.a
<engine install>\lib\Release\libOgreHlmsUnlit.dll.a
<engine install>\bin\Release\OgreMain.dll
<engine install>\bin\Release\RenderSystem_Direct3D11.dll
<engine install>\bin\Release\RenderSystem_GL3Plus.dll
<engine install>\bin\Release\Plugin_ParticleFX.dll
<engine install>\Media\Hlms\Unlit
<engine install>\Media\Hlms\Pbs
<engine install>\Media\materials\textures
```

The dependency install is successful when `<deps install>\include\FreeImage.h`, `include\SDL2\SDL.h`, and `bin\Release\SDL2.dll` exist.

Return `success`, `cancelled`, `operation`, and `output` in the same `key=value` shape as `FormatWinCMakeRun`.

Bind it in `wiz.cpp` next to `WinCMakeInstall`:

```cpp
BindMethod(v, _SC("WinCMakeOgreNext"), Wiz_WinCMakeOgreNext, _SC("Wiz::WinCMakeOgreNext"));
```

Reason: the script is not allowed to assemble this command line itself.


4. `resources/ogre-next/wizard.script` — Windows pages ================

Keep the Mac functions. At the top of `BeginWizard()`, if `PLATFORM == PLATFORM_MSW`, call a new `BeginWinOgreNext()` and return. At the top of `SetupProject()`, if `PLATFORM == PLATFORM_MSW`, return `SetupWinOgreNextProject(project)`.

Reason: the Mac pages link frameworks and look in `~/Library/CodeBlocks`, which is the wrong platform.

`BeginWinOgreNext()` uses the helpers you just bound, and the WinGit and WinPatch helpers that already exist.

Paths. Call `Wizard.WinCMakePaths` twice:

```text
Wizard.WinCMakePaths("ogre-next-deps-source-3.0.0", "ogre-next-deps-source-3.0.0")
Wizard.WinCMakePaths("ogre-next-source-3.0.0", "ogre-next-3.0.0")
```

The second call's `install` field is the engine prefix. The dependency SDK is the first call's `build` field plus `\ogredeps`, not its `install` field. Store the paths in the existing `OgreNext*` variables. `OgreNextField` already parses `key=value` lines.

Reason: `WinCMakePaths` is what rejects a leaf that contains a slash or a drive letter.

Tools. `Wizard.WinCMakeProbe()` must report `access=works`. `Wizard.WinGitProbe()` must report `access=works`. There is no Homebrew page. If either tool is missing, show one info page that names the missing tool and stop before a download.

Reason: the wizard finds Git and CMake, and it does not install them.

Download. For each tree, if `Wizard.WinOgreNextAcquire(sourceId, "unused", "unused")` is not already `success=1`, add an info page. `OnLeave` of that page calls `WinOgreNextAcquire` with a real explanation and confirm label. `sourceId` is `ogre-next-deps` first, then `ogre-next`.

Patches. Probe and, only when the state is `applicable`, apply exactly these ids, in this order:

```text
ogre-next-deps-mingw     on the dependency source
ogre-next-v3-0-mingw     on the engine source
```

Use `Wizard.WinPatchProbe` and `Wizard.WinPatchApply`. `already_applied` is success. A declined apply does not continue to configure.

Reason: those two patch files are the MinGW edits, and applying the Mac patch on Windows would change the wrong files.

Build. One page for the dependencies, then one page for the engine. Each `OnLeave` calls `Wizard.WinCMakeOgreNext` for `configure`, then `build`, then `install`, and stops on the first `success` that is not `1`. Skip a page when the install check from section 3 would already pass.

Then `Wizard.AddProjectPathPage()` and `Wizard.AddCompilerPage`. `OnLeave` of those pages returns false unless every install file from section 3 exists.

`GetGeneratedFile`, case 1, when `PLATFORM == PLATFORM_MSW`: read `ogre-next/files/plugins-windows.cfg` instead of `plugins.cfg`. Replace `[OGRE_NEXT_BIN]` with the engine `bin\Release` path written with forward slashes. Keep the existing replacement of `[OGRE_NEXT_MEDIA]` and `[OGRE_NEXT_TEXTURES]`, and write those two paths with forward slashes as well.

Reason: `main.cpp` looks for a forward slash when it joins the HLMS folder, and the Mac `plugins.cfg` names Metal.

`SetupWinOgreNextProject`:

```text
include    <install>\include\OGRE
           <install>\include\OGRE\Hlms\Common
           <install>\include\OGRE\Hlms\Unlit
           <install>\include\OGRE\Hlms\Pbs
compiler   -std=gnu++17
lib dir    <install>\lib\Release
link       OgreHlmsPbs, OgreHlmsUnlit, OgreMain
```

Both targets are `ttConsoleOnly`. The working directory is the folder that contains the project file. Output stays the normal `bin\Debug` and `bin\Release` names. There is no `-framework` option and no `-F` option.

Reason: this SDK publishes `libOgreMain.dll.a`, `libOgreHlmsPbs.dll.a`, and `libOgreHlmsUnlit.dll.a`, and the closing sentence is printed to the console.

After the link, copy these files into `$(TARGET_OUTPUT_DIR)`:

```text
<install>\bin\Release\OgreMain.dll
<install>\bin\Release\OgreHlmsPbs.dll
<install>\bin\Release\OgreHlmsUnlit.dll
<compiler bin>\libgcc_s_seh-1.dll
<compiler bin>\libstdc++-6.dll
<compiler bin>\libwinpthread-1.dll
```

The compiler `bin` is the `bin=` line from `Wizard.WinOgreTools(Wizard.GetCompilerID())`. Also copy `<install>\Media\models\ogrehead.mesh` to `$(PROJECT_DIR)\ogrehead.mesh`.

Reason: Windows loads those DLLs from the program folder before `PATH`, so a different `libstdc++-6.dll` cannot be picked up.


5. `resources/ogre-next/files/plugins-windows.cfg` ======================

Create this file:

```text
PluginFolder=[OGRE_NEXT_BIN]
PluginOptional=RenderSystem_Direct3D11
PluginOptional=RenderSystem_GL3Plus
PluginOptional=RenderSystem_Vulkan
Plugin=Plugin_ParticleFX
```

Do not edit `files/plugins.cfg`. That file is the Mac list.

Reason: the render-system DLLs stay in the installed `bin\Release`, and `PluginFolder` is how the program finds them without being started inside that folder.

`files/resources2.cfg` and `files/main.cpp` already contain `[OGRE_NEXT_MEDIA]` and `[OGRE_NEXT_TEXTURES]`. Leave them. Section 4 substitutes the Windows paths.


6. Build the plugin and copy it to the IDE you run =====================

The C++ changes are the Scripted wizard target in `src\CodeBlocks_wx33_64.cbp`. `wingit.cpp` and `wincmake.cpp` are already units of that target. From `src`, with the Code::Blocks MinGW `bin` and `zip.exe` on `PATH`:

```bat
"D:\Program Files\CodeBlocks\codeblocks.exe" --no-splash-screen --no-dde --multiple-instance --build --target="Scripted wizard" CodeBlocks_wx33_64.cbp
```

The target writes `devel33_64\share\CodeBlocks\plugins\scriptedwizard.dll` and `devel33_64\share\CodeBlocks\scriptedwizard.zip`. The zip is only `manifest.xml`. The scripts are not inside it.

Copy the plugin and the wizard scripts into both trees. `config.script` and `ogre-next\` must land in `templates\wizard`:

```bat
copy /Y devel33_64\share\CodeBlocks\plugins\scriptedwizard.dll output33_64\share\CodeBlocks\plugins\scriptedwizard.dll
xcopy /Y /S plugins\scriptedwizard\resources\* devel33_64\share\CodeBlocks\templates\wizard\
xcopy /Y /S plugins\scriptedwizard\resources\* output33_64\share\CodeBlocks\templates\wizard\
```

Quit `output33_64\codeblocks.exe` and start it again.

Reason: that IDE loads plugins and wizard scripts from `output33_64`, and a script edit is not picked up from the source tree.

File, New, Project must list OGRE-Next project. The first run downloads into `%LOCALAPPDATA%\codeblocks` and can take 1 to 2 hours. A later run skips a tree that is already the right commit, already patched, and already installed. The generated program opens a window titled `OGRE-Next Light Show`, shows the rotating head, and prints `Light show rendered and closed cleanly.` when the window closes.
