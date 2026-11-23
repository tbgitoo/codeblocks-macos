#  Windows OGRE-Next wizard

We are adding the Windows half of the OGRE-Next project wizard. The Mac wizard already exists.

The wizard performs the work: it downloads the pinned sources, configures and builds OGRE-Next, installs it, and then generates an ordinary Code::Blocks project that uses that installation.

## Steps:

```text
find tools
download pinned source
configure with the install folder already chosen
build
install
generate an ordinary Code::Blocks project
```

## Source:

OGRE-Next has two source trees here: the dependency tree and OGRE-Next itself. This repository already contains the two MinGW patches needed on Windows. The existing Apple patches continue to belong to the Mac wizard.

Use these exact versions:

```text
ogre-next-deps   f7e23191f4fd89ed41134996bc47f9ee23d009df
SDL2             5d249570393f7a37e037abf22cd6012a4cc56a71
OGRE-Next        4859016ad2cc956a653dfc4e4a859505696c6509
```

Pass the MinGW selected in Code::Blocks to CMake. On this machine it is:

```text
D:\Program Files\CodeBlocks\MinGW
```

Use this install root:

```text
%LOCALAPPDATA%\codeblocks
```

`CodeBlocksRoot()` in `wincmake.cpp` already constructs that root. Building it from `LOCALAPPDATA` makes the wizard work for the currently logged user.


## 0. Use the OGRE-Next and Windows-specific helpers

The main file is (from repository root):

```text
src/plugins/scriptedwizard/resources/ogre-next/wizard.script
```

But we will work on some accessory files around that as well.

Attention:

`src/plugins/scriptedwizard/resources/ogre/wizard.script` looks similar but don't break it: That's your ogre wizard.

Patches to apply: The patches that we will apply live in `src/plugins/scriptedwizard/resources/patches`

They are:

```text
ogre-next-deps-mingw.patch
ogre-next-v3-0-mingw.patch
```

Those are the patches derived from your manual work.

`WinPatchProbe` and `WinPatchApply` already know them, although the use the `ogre-next-deps-mingw` and `ogre-next-v3-0-mingw` as ID's, without the `.patch` extension. Don't touch the apple patches, otherwise we break the Mac installation `ogre-next-deps-apple` and `ogre-apple` patches continue to serve the Mac wizard.


## 1. `src/plugins/scriptedwizard/resources/config.script` - make the wizard appear on Windows

The OGRE-Next registration is currently inside:

```javascript
if (PLATFORM == PLATFORM_MAC)
```

Change it to:

```javascript
if (PLATFORM == PLATFORM_MAC || PLATFORM == PLATFORM_MSW)
    RegisterWizard(wizProject, _T("ogre-next"), _("OGRE-Next project"), _("2D/3D Graphics"));
```

After copying the wizard resources and restarting Code::Blocks, this registration makes the following entry appear on Windows:

```text
File -> New -> Project -> OGRE-Next project
```
## 2. Test
You can test by rebuilding the Windows application. If the build generally runs, run this from repo root to update the current exe (if this doesn't work, see README.windows.md):

```bat
cd /d src
set "PATH=D:\Program Files\CodeBlocks\MinGW\bin;%PATH%"
start /wait "" "D:\Program Files\CodeBlocks\codeblocks.exe" --no-splash-screen --no-dde --multiple-instance --build --target=All CodeBlocks_wx33_64.cbp
update.bat 33_64
cd ..
```
With CodeBlocks built, run from repository root
```bat
src\output33_64\codeblocks.exe
```


## 3. wingit.h and wingit.cpp 

We are talking here about the folder `src/plugins/scriptedwizard` and some files residing in that folder.

The use of git by the wizard is nearly the same on mac and windows, except adjustment for folder locations. So we can copy over most of the necessary functions with a few specific modifications.
 
### Declaration

Get the mac stuff that we will adapt to windows:

- From `macgit.h`, copy `MacGitAcquireResult`, `AcquireMacGitSource`, and `FormatMacGitAcquire`
- paste into `wingit.h`, still inside 
   `#if defined(__WXMSW__)`
   , and place them after `FormatWinGitClone`. 
   
Rename those functions declarations:

`MacGitAcquireResult`   -> `WinGitAcquireResult`
`AcquireMacGitSource`   -> `AcquireWinOgreNext`
`FormatMacGitAcquire`   -> `FormatWinGitAcquire`

Leave the fields and the comments as they are. The functions have no body {}. Normal: we are in header ("hashtag") file.


### Table and checkout commands

From macgit.cpp, copy: 

struct KnownGitSource and kKnownSources (lines 699–727)
FindKnownSource
StripTrailingSeparator
KnownSourceDestination
RunGitSync
RevParseHead
EnsureParentDirectory
CheckoutCommands

Paste them into the anonymous namespace of wingit.cpp.

Note: that anonymous namespace is like the `use namespace std`. Except for it's anonymous, so nobody can call it from the outside and things remain private within `wingit.cpp`

Folder location edit.

KnownSourceDestination in macgit.cpp starts from wxGetHomeDir() and appends Library, then CodeBlocks, then source.leaf. This is different in Windows because the directory are different. So in `wincmake.cpp`:

```cpp
wxString local;
if (!wxGetEnv(wxT("LOCALAPPDATA"), &local) || local.empty())
    return wxString();
wxFileName dest = wxFileName::DirName(local);
dest.AppendDir(wxT("codeblocks"));
dest.AppendDir(source.leaf);
```

Keep the Normalize and StripTrailingSeparator calls that follow in the Mac function.

### The acquire functions
Go back to `macgit.cpp`and copy `AcquireMacGitSource` (from line 1100) and `FormatMacGitAcquire` (from line 1289) to the bottom of `wingit.cpp`, above `#endif // __WXMSW__`. 

Rename the types and the two functions as in the header (like they are in `wingit.h`). Then change only these calls inside the copied acquire function:


ProbeMacGit()     -> ProbeWinGit()
MacGitWorks       -> WinGitWorks
MacGitCloneDialog -> WinGitCloneDialog


### Running a command list

A class, in c++ is a collection of both functions and variables (called members).

Go to macgit.cpp, and find the class MacGitCloneDialog. Within that class, locate the StartCommands and LaunchStep functions, and copy them over into WinGitCloneDialog (in wingit.cpp; we are in .cpp, don't forget the function body {}).
Go back to macgit.cpp,  look for the class members  m_commands, m_step, and m_retired and copy them over to the corresponding WinGitCloneDialog class in wingit.cpp. Class members are like regular variables and have a type, for instance `std::vector<wxArrayString> m_commands;` Don't forget the type! And as they are members, they need to be inside the class, but not inside a function.

Next, go back to macgit.cpp and find the WinGitCloneDialog::OnProcessEnd function. Copy the body of this function, that is only the stuff between { and }. Go back to wingit.cpp and replace the body of the onProcessEnd function with the one coming from macgit.cpp

Test here whether everything still compiles (see section 2). Expect nothing new here, this is only preparatory work.

## 4. Squirrel Bindings

Squirrel is a separate language, and a brigde called "bindings" is needed for it to use our cpp functions. Those bindings live in `wiz.cpp`, which is located in `src/plugins/scriptedwizard`. For now, these are still cpp edits, not squirrel

Open `wiz.cpp` and find the function `Wiz_MacGitAcquire` (lines 2074–2097). Copy the entire function. Find the function definition for `Wiz_WinGitClone` that is something like `SQInteger Wiz_WinGitClone(HSQUIRRELVM v){ ... }`. Just after the body {} of `Wiz_WinGitClone`, paste the `Wiz_MacGitAcquire` function definition. Rename the pasted function to `Wiz_WinOgreNextAcquire`. Also, inside the function body of `Wiz_WinOgreNextAcquire`, there are calls to the mac functions `AcquireMacGitSource` and  `FormatMacGitAcquire`. Change those calls to `AcquireWinOgreNext` respectively `FormatWinGitAcquire`. Attention, one calls is quite unusual because it passes the function name instead of calling it directly. If you want to make it easy on people doing debugging, you change the error message string "Wiz::MacGitAcquire" to "Wiz::WinOgreNextAcquire"), but if you don't it will work but remain a trap.

Define the new binding along with the other windows binding:

```cpp
BindMethod(v, _SC("WinOgreNextAcquire"),Wiz_WinOgreNextAcquire,_SC("Wiz::WinOgreNextAcquire"));
```

Concretely, a good idea would be to open a new ogre-next section in the Windows binding block. For example, but the new binding after:

```cpp
BindMethod(v, _SC("WinOgreInstall"), Wiz_WinOgreInstall, _SC("Wiz::WinOgreInstall"));
```

## 5. `wincmake.h` and `wincmake.cpp` - configure, build, and install

Add:

```cpp
wxString RunWinCMakeOgreNext(wxWindow* parent,
                             const wxString& projectId,
                             const wxString& operation,
                             const wxString& explanation,
                             const wxString& confirmLabel);
```

Accept these `projectId` values:

```text
ogre-next-deps
ogre-next
```

Accept these `operation` values:

```text
configure
build
install
```

For any other value, return `success=0` without starting CMake.

Squirrel can therefore request one known operation while C++ constructs the exact command.

### Folders

Use the same `%LOCALAPPDATA%\codeblocks` root as section 2:

```text
deps source     %LOCALAPPDATA%\codeblocks\ogre-next-deps-source-3.0.0
deps build      <deps source>\build-codeblocks
deps install    <deps build>\ogredeps

engine source   %LOCALAPPDATA%\codeblocks\ogre-next-source-3.0.0
engine build    <engine source>\build-codeblocks
engine install  %LOCALAPPDATA%\codeblocks\ogre-next-3.0.0
```

These folders have different jobs:

```text
source   code downloaded from Git
build    files created while CMake and the compiler build it
install  files the generated student's program uses
```

The generated project uses the install tree rather than the source or build tree.

### Compiler tools

Take these from `MingwTools` for the compiler selected in the wizard:

```text
gcc
g++
mingw32-make
compiler bin
```

Construct the resource compiler path as the compiler `bin` plus:

```text
windres.exe
```

This makes OGRE-Next, its dependencies, and the generated project use the same MinGW.

### Configure the dependencies

Use:

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

Keep the dependency project's CMake install prefix at its default. This pinned project then creates:

```text
<deps build>\ogredeps
```

which is the dependency SDK used below.

### Configure OGRE-Next

Use the same generator and compiler arguments, plus:

```text
-DCMAKE_INSTALL_PREFIX=<engine install>
-DOGRE_DEPENDENCIES_DIR=<deps install>
-DOGRE_BUILD_SAMPLES2=OFF
-DOGRE_BUILD_TESTS=OFF
```

Here `-D` sets a CMake configuration variable. The two `OFF` values keep this build focused on the engine needed by the generated project instead of also building the OGRE samples and tests.

Use the Windows arguments above rather than the Mac variables `CMAKE_OSX_ARCHITECTURES` and `CMAKE_OSX_SYSROOT`, or the `FLTK_` variables belonging to another wizard.

Supplying `CMAKE_INSTALL_PREFIX` during configure stores the final engine destination in `CMakeCache.txt` before the build starts.

### Build

For `build`, run:

```text
cmake --build <build> --parallel <N>
```

where `<N>` is `BoundedJobCount()`.

### Install

For `install`, run:

```text
cmake --install <build>
```

The engine prefix already comes from the configure cache, so this command requires no additional `--prefix` argument.

### Verify configure

Configure succeeds only when:

1. the process exits with `0`; and
2. `<build>\CMakeCache.txt` exists.

For OGRE-Next, also verify that the cache records the install prefix and `OGRE_DEPENDENCIES_DIR` passed above.

### Verify the dependency installation

Require all of these:

```text
<deps install>\include\FreeImage.h
<deps install>\include\SDL2\SDL.h
<deps install>\bin\Release\SDL2.dll
```

### Verify the OGRE-Next installation

Require all of these:

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

These checks confirm that the pieces needed by the generated program actually appeared.

### Return value and Squirrel binding

Return:

```text
success
cancelled
operation
output
```

in the same `key=value` format as `FormatWinCMakeRun`.

In `wiz.cpp`, next to `WinCMakeInstall`, add:

```cpp
BindMethod(v, _SC("WinCMakeOgreNext"),
           Wiz_WinCMakeOgreNext,
           _SC("Wiz::WinCMakeOgreNext"));
```

Add its Squirrel wrapper as for the Git function. After rebuilding the plugin, Squirrel can call `Wizard.WinCMakeOgreNext(...)`.


## 4. `resources/ogre-next/wizard.script` - add the Windows pages

Keep the existing Mac functions.

At the top of `BeginWizard()`, add a Windows branch that calls `BeginWinOgreNext()` and returns when `PLATFORM == PLATFORM_MSW`.

At the top of `SetupProject()`, return `SetupWinOgreNextProject(project)` when `PLATFORM == PLATFORM_MSW`.

This sends Windows into its own pages before the existing Mac code looks for frameworks and `~/Library/CodeBlocks`.

### Create `BeginWinOgreNext()`

Use:

- the two new bound helpers;
- the existing WinGit helpers;
- the existing WinPatch helpers.

### Resolve paths

Call:

```text
Wizard.WinCMakePaths("ogre-next-deps-source-3.0.0", "ogre-next-deps-source-3.0.0")
Wizard.WinCMakePaths("ogre-next-source-3.0.0", "ogre-next-3.0.0")
```

For the second call, the `install` field is the engine install directory.

For the dependencies, form the SDK path as:

```text
<first call's build>\ogredeps
```

Store the paths in the existing `OgreNext*` variables. Use `OgreNextField` to parse the returned `key=value` lines.

Going through `WinCMakePaths` also rejects a leaf containing a slash or drive letter, keeping the paths below the intended Code::Blocks root.

### Check Git and CMake

Call:

```text
Wizard.WinCMakeProbe()
Wizard.WinGitProbe()
```

Both must return:

```text
access=works
```

When a tool is missing, show one information page naming it and stop before source acquisition. The Windows wizard finds Git and CMake; installation of those tools remains outside the wizard.

### Acquire both source trees

First probe the dependency source with:

```text
Wizard.WinOgreNextAcquire("ogre-next-deps", "unused", "unused")
```

When it already returns `success=1`, continue without adding a download page. Otherwise add an information page whose `OnLeave` calls the same function with the real explanation and confirm label.

Repeat for:

```text
ogre-next
```

Keep this order:

```text
ogre-next-deps
ogre-next
```

### Apply the MinGW patches

Probe and, when the state is `applicable`, apply exactly these ids in this order:

```text
ogre-next-deps-mingw     dependency source
ogre-next-v3-0-mingw     engine source
```

Use:

```text
Wizard.WinPatchProbe
Wizard.WinPatchApply
```

Treat `already_applied` as success. A declined patch application stops the flow before configuration.

### Build the dependencies

Add one page for the dependencies. Its `OnLeave` calls:

```text
Wizard.WinCMakeOgreNext(... "configure" ...)
Wizard.WinCMakeOgreNext(... "build" ...)
Wizard.WinCMakeOgreNext(... "install" ...)
```

in that order. After each call, inspect `success` and stop on the first value other than `1`.

Skip the page when the complete dependency installation check from section 3 already passes.

### Build OGRE-Next

Add one page for the engine and perform the same sequence:

```text
configure
build
install
```

Again, stop on the first failure and skip the page when the complete engine installation check already passes.

The build can take 1 to 2 hours. Continued compiler output means it is working, even though this operation is much longer than an ordinary wizard page.

### Add the ordinary project pages

Then add:

```text
Wizard.AddProjectPathPage()
Wizard.AddCompilerPage
```

Their `OnLeave` handlers return false unless every required install file from section 3 exists. The generated project is therefore created only after the necessary SDK is present.

### Generate the Windows plugin configuration

In `GetGeneratedFile`, case 1, when `PLATFORM == PLATFORM_MSW`, read:

```text
ogre-next/files/plugins-windows.cfg
```

Replace `[OGRE_NEXT_BIN]` with:

```text
<engine install>\bin\Release
```

written with forward slashes.

Keep the existing replacements of:

```text
[OGRE_NEXT_MEDIA]
[OGRE_NEXT_TEXTURES]
```

and write those paths with forward slashes too. `main.cpp` uses `/` when joining the HLMS folder, and the separate Windows plugin file supplies Windows render systems instead of Metal.

### Create `SetupWinOgreNextProject`

Configure the generated project with:

```text
include    <install>\include\OGRE
           <install>\include\OGRE\Hlms\Common
           <install>\include\OGRE\Hlms\Unlit
           <install>\include\OGRE\Hlms\Pbs
compiler   -std=gnu++17
lib dir    <install>\lib\Release
link       OgreHlmsPbs, OgreHlmsUnlit, OgreMain
```

The link names correspond to:

```text
libOgreHlmsPbs.dll.a
libOgreHlmsUnlit.dll.a
libOgreMain.dll.a
```

Use `ttConsoleOnly` for both targets. Set the working directory to the folder containing the project file. Keep the ordinary output folders:

```text
bin\Debug
bin\Release
```

Use the Windows link configuration above. The Mac `-framework` and `-F` options remain in the Mac setup.

The console target keeps the final message visible:

```text
Light show rendered and closed cleanly.
```

### Copy runtime files after linking

After the link, copy these files into `$(TARGET_OUTPUT_DIR)`:

```text
<install>\bin\Release\OgreMain.dll
<install>\bin\Release\OgreHlmsPbs.dll
<install>\bin\Release\OgreHlmsUnlit.dll
<compiler bin>\libgcc_s_seh-1.dll
<compiler bin>\libstdc++-6.dll
<compiler bin>\libwinpthread-1.dll
```

`$(TARGET_OUTPUT_DIR)` is a Code::Blocks variable. Code::Blocks replaces it with the current target's output directory, so the same copy command works for Debug and Release.

Read `<compiler bin>` from the `bin=` line returned by:

```text
Wizard.WinOgreTools(Wizard.GetCompilerID())
```

Putting the corresponding MinGW runtime DLLs beside the executable makes Windows load the runtime belonging to the selected compiler instead of finding another `libstdc++-6.dll` through `PATH`.

Also copy:

```text
<install>\Media\models\ogrehead.mesh
```

to:

```text
$(PROJECT_DIR)\ogrehead.mesh
```

The generated program then finds the model in its project directory.


## 5. `resources/ogre-next/files/plugins-windows.cfg`

Create:

```text
resources/ogre-next/files/plugins-windows.cfg
```

with:

```text
PluginFolder=[OGRE_NEXT_BIN]
PluginOptional=RenderSystem_Direct3D11
PluginOptional=RenderSystem_GL3Plus
PluginOptional=RenderSystem_Vulkan
Plugin=Plugin_ParticleFX
```

Keep `files/plugins.cfg` as the Mac plugin list.

The render-system DLLs remain in:

```text
<engine install>\bin\Release
```

OGRE loads them as plugins, and `PluginFolder` tells OGRE where to find them. This differs from `OgreMain.dll`, which Windows needs beside the executable when the program starts.

Keep the existing placeholders in:

```text
files/resources2.cfg
files/main.cpp
```

They already contain:

```text
[OGRE_NEXT_MEDIA]
[OGRE_NEXT_TEXTURES]
```

Section 4 substitutes the Windows install paths.


## 6. Build the Scripted wizard plugin and copy it to the IDE you run

The C++ changes belong to the Scripted wizard target in:

```text
src\CodeBlocks_wx33_64.cbp
```

`wingit.cpp` and `wincmake.cpp` are already units of that target.

From `src`, with the Code::Blocks MinGW `bin` and `zip.exe` on `PATH`, run:

```bat
"D:\Program Files\CodeBlocks\codeblocks.exe" --no-splash-screen --no-dde --multiple-instance --build --target="Scripted wizard" CodeBlocks_wx33_64.cbp
```

The build produces:

```text
devel33_64\share\CodeBlocks\plugins\scriptedwizard.dll
devel33_64\share\CodeBlocks\scriptedwizard.zip
```

`scriptedwizard.zip` contains only `manifest.xml`. The scripts are copied separately.

Copy the rebuilt plugin:

```bat
copy /Y devel33_64\share\CodeBlocks\plugins\scriptedwizard.dll output33_64\share\CodeBlocks\plugins\scriptedwizard.dll
```

Copy the wizard resources into both trees:

```bat
xcopy /Y /S plugins\scriptedwizard\resources\* devel33_64\share\CodeBlocks\templates\wizard\
xcopy /Y /S plugins\scriptedwizard\resources\* output33_64\share\CodeBlocks\templates\wizard\
```

In particular, `config.script` and `ogre-next\` must land below `templates\wizard`.

Quit `output33_64\codeblocks.exe` completely and start it again.

The running IDE loads its plugin and wizard resources from `output33_64`. When a script change appears to have no effect, first verify that the resources were copied to that tree and that this Code::Blocks instance was restarted.


## 7. Finished result

Open:

```text
File -> New -> Project
```

`OGRE-Next project` must be listed.

On the first run, the wizard downloads into:

```text
%LOCALAPPDATA%\codeblocks
```

The full OGRE build can take 1 to 2 hours. A later run skips a tree that is already at the correct commit, already patched, and already installed.

Create and build the generated Code::Blocks project.

Running it must:

1. open a window titled `OGRE-Next Light Show`;
2. show the rotating Ogre head;
3. close normally;
4. print `Light show rendered and closed cleanly.`

At that point, the Windows OGRE-Next wizard works.

When an experiment changes tracked files in an unhelpful way before there is anything worth keeping, the repository still has Mathis's favorite exit:

```bat
git restore .
```

