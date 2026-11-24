#  Windows OGRE-Next wizard

We are adding the Windows half of the OGRE-Next project wizard. The Mac wizard already exists.

The wizard performs the work: it downloads the pinned sources, configures and builds OGRE-Next, installs it, and then generates an ordinary Code::Blocks project that uses that installation.

## Steps:

Do these in order. Check each step in Code::Blocks with `File -> New -> Project -> OGRE-Next project`.

```text
1. The menu entry appears
2. The welcome page appears
3. The download page checks out the pinned sources
4. The patch page applies the two MinGW patches
5. The build page configures and builds
6. The install page fills the SDK folders
7. The wizard creates an ordinary Code::Blocks project
```

## Source:

OGRE-Next has two source trees here: the dependency tree and OGRE-Next itself. This repository already contains the two MinGW patches needed on Windows. The existing Apple patches continue to belong to the Mac wizard.

Use these exact versions:

```text
ogre-next-deps   f7e23191f4fd89ed41134996bc47f9ee23d009df
SDL2             5d249570393f7a37e037abf22cd6012a4cc56a71
OGRE-Next        4859016ad2cc956a653dfc4e4a859505696c6509
```

The compiler for ogre-next building lives here

```text
D:\Program Files\CodeBlocks\MinGW
```

We are going to instruct the wizard to put downloaed and compiled files here

```text
%LOCALAPPDATA%\codeblocks
```

%LOCALAPPDATA% here means `C:\Users\<you>\AppData`. This makes that the wizard works for the currently logged user.


## 0. Use the OGRE-Next and Windows-specific helpers

Our main wizard file to be updated is the Squirrel file:

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

Those are the patches we got from manual testing. We will them apply here with `WinPatchApply` using `ogre-next-deps-mingw` and `ogre-next-v3-0-mingw` as ID's, without the `.patch` extension. Don't touch the apple patches in `src/plugins/scriptedwizard/resources/patches`, otherwise we break the Mac installation.


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

This is squirrel syntax: almost c++, but some things like the strings are different.

The aim is to make this appear in Code::Blocks on Windows.

```text
File -> New -> Project -> OGRE-Next project
```
## 2. Time to check

This is supposed to build the whole IDE. If it fails, use `README.windows.md`. Good luck. 

- Quit Code::Blocks if already open.
- Open a command prompt window, with Windows key + R, then enter cmd, hit enter. 
- In the black command prompt window, navigate to the root of the repo using `C:` or `D:`for drive letters, `cd + <dirname>` to change directories, and `cd ..` to move up a directory.

```bat
cd /d src
set "PATH=D:\Program Files\CodeBlocks\MinGW\bin;%PATH%"
start /wait "" "D:\Program Files\CodeBlocks\codeblocks.exe" --no-splash-screen --no-dde --multiple-instance --build --target=All CodeBlocks_wx33_64.cbp
update.bat 33_64
cd ..
```
- Wait until that is done.
- Start `src\output33_64\codeblocks.exe` again.

- In the running Code::Blocks application:

`File -> New -> Project` lists `OGRE-Next project`. Clicking it opens the mac pages, because we haven't changed the actual wizard. Indeed, we haven't touched `src/plugins/scriptedwizard/resources/ogre-next/wizard.script`, we've only enabled it.


## 3. Welcome page

Now we are going for the actual Wizard. Open `src/plugins/scriptedwizard/resources/ogre-next/wizard.script`. At the top, the comment says `macOS only.` Change that to `macOS and Windows.`

At the top of `BeginWizard()`, before `local intro`, insert:

```javascript
if (PLATFORM == PLATFORM_MSW)
{
    BeginWinOgreNext();
    return;
}
```

Windows then returns before the Mac code wanders off into mac world.

Above `BeginWizard()`, add this function. For now, that's our wizard:

```javascript
function BeginWinOgreNext()
{
    local intro = _("Welcome to the OGRE-Next project wizard.\n\nThis creates a C++ project for OGRE-Next 3.0.0.\n\nI'm not doing anything useful yet. Your wizard truly\n\nPress Cancel.");
    Wizard.AddInfoPage(_T("OgreNextWinIntro"), intro);
}
```

There's a second place where the Wizard tries to run Mac stuff. We need to prevent it from doing so.

At the top of `SetupProject()`, before `if (!OgreNextAllReady())`, insert:

```javascript
if (PLATFORM == PLATFORM_MSW)
{
    ShowError(_("The Windows project is added in a later step. Press Cancel."));
    return false;
}
```

Time to look. Apply the build recipe in Section 2 and take a look at `File -> New -> Project -> OGRE-Next project`. It shows the welcome text. Press Cancel.


## 4. wingit.h and wingit.cpp: Enable the wizard to drive git for ogre-next 

The wizard will do what we did manually before. The first step is to teach it to use git for downloading ogre-next-deps and ogre-next.

Navigate to `src/plugins/scriptedwizard`. We will modify files in that folder.

The next step is copying over git-related functions and adapting them for Windows.
 
### Declaration

Get the mac stuff that we will adapt to windows:

- From `macgit.h`, copy `MacGitAcquireResult`, `AcquireMacGitSource`, and `FormatMacGitAcquire`
- paste into `wingit.h`, still inside 
   `#if defined(__WXMSW__)`
   , and place them after `FormatWinGitClone`. 
   
Rename those function declarations:

`MacGitAcquireResult`   -> `WinGitAcquireResult`
`AcquireMacGitSource`   -> `AcquireWinOgreNext`
`FormatMacGitAcquire`   -> `FormatWinGitAcquire`

Leave the fields and the comments as they are. The functions have no body {}. Normal: we are in header ("hashtag") file.


### Table and checkout commands

From `macgit.cpp`, copy implementations: 

```text
struct KnownGitSource and kKnownSources (lines 699–727)
FindKnownSource
StripTrailingSeparator
KnownSourceDestination
RunGitSync
RevParseHead
EnsureParentDirectory
CheckoutCommands
```

Paste them into the anonymous namespace of `wingit.cpp`.

Note: that anonymous namespace is like the 'using namespace std'. Except for it's anonymous, meaning whatever is init is reachable only from `wingit.cpp`

The function `KnownSourceDestination` in `macgit.cpp` starts from wxGetHomeDir() and appends Library, then CodeBlocks, then source.leaf. The Windows procedure is different. So in `wincmake.cpp` replace the lines of the block from `wxGetHomeDir` through appending `source.leaf` by the following code: 

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
Go back to `macgit.cpp`and copy `AcquireMacGitSource` (from line 1100) and `FormatMacGitAcquire` (from line 1289) to the bottom of `wingit.cpp`, but still above `#endif // __WXMSW__`. 

Rename the types and the two functions as in the header (like they are in `wingit.h`). Then change only these calls inside the copied acquire function:


ProbeMacGit()     -> ProbeWinGit()
MacGitWorks       -> WinGitWorks
MacGitCloneDialog -> WinGitCloneDialog


### Running a command list

A class, in c++ is a collection of both functions and variables (called members).

Go to `macgit.cpp`, and find the class MacGitCloneDialog. Within that class, locate the StartCommands and LaunchStep functions, and copy them over into WinGitCloneDialog (in `wingit.cpp`; as we are in .cpp, don't forget the function body {}).
Go back to `macgit.cpp`,  look for the class members  `m_commands`, `m_step`, and `m_retired` and copy them over to the corresponding `WinGitCloneDialog` class in wingit.cpp. Class members are like regular variables and have a type, for instance `std::vector<wxArrayString> m_commands;` Don't forget the type, nor the semi-colon. Also, these are class members, so they need to be inside the class, but not inside a function.

Next, go back to `macgit.cpp` and find the `WinGitCloneDialog::OnProcessEnd` function. Copy the body of this function, that is only the stuff between { and }. Go back to `wingit.cpp` and replace the body of the `onProcessEnd` function with the one coming from `macgit.cpp`



### Squirrel binding

That c++ work doesn't change the wizard yet. Squirrel binding is needed to use these functions. Those bindings live in `wiz.cpp`, which is located in `src/plugins/scriptedwizard`.

Open `wiz.cpp` and find the function `Wiz_MacGitAcquire` (lines 2074–2097). Copy the entire function. Find the function definition for `Wiz_WinGitClone` that is something like `SQInteger Wiz_WinGitClone(HSQUIRRELVM v){ ... }`. Just after the body {} of `Wiz_WinGitClone`, paste the `Wiz_MacGitAcquire` function definition. Rename the pasted function to `Wiz_WinOgreNextAcquire`. Also, inside the function body of `Wiz_WinOgreNextAcquire`, there are calls to the mac functions `AcquireMacGitSource` and  `FormatMacGitAcquire`. Change those calls to `AcquireWinOgreNext` respectively `FormatWinGitAcquire`. Attention, one calls is quite unusual because it passes the function name instead of calling it directly. If you want to make it easy on people doing debugging, you change the error message string "Wiz::MacGitAcquire" to "Wiz::WinOgreNextAcquire"), but if you don't it will work but remain a trap.

Define the new binding along with the other windows binding:

```cpp
BindMethod(v, _SC("WinOgreNextAcquire"),Wiz_WinOgreNextAcquire,_SC("Wiz::WinOgreNextAcquire"));
```

Concretely, a good idea would be to open a new ogre-next section in the Windows binding block. For example, but the new binding after:

```cpp
BindMethod(v, _SC("WinOgreInstall"), Wiz_WinOgreInstall, _SC("Wiz::WinOgreInstall"));
```

Check whether everything compiles, using steps in section 2. You want succesful compilation here. In the IDE nothing changes yet, we only made functions available but haven't changed the Wizard itself yet.


## 5. Teach the wizard to do git repo download

Open `wizard.script` again. Copy these functions and paste the copies just above `BeginWinOgreNext`. Rename each copy and change the call inside it.

```text
OgreNextResolvePaths       -> OgreNextWinResolvePaths
    Wizard.MacCMakePaths           -> Wizard.WinCMakePaths

OgreNextGitBlock           -> OgreNextWinGitBlock
    Wizard.MacGitProbe             -> Wizard.WinGitProbe

OgreNextAcquire            -> OgreNextWinAcquire
    Wizard.MacGitAcquire           -> Wizard.WinOgreNextAcquire

OgreNextCheckoutReady      -> OgreNextWinCheckoutReady
    Wizard.MacGitAcquire           -> Wizard.WinOgreNextAcquire
```

Copy `OnLeave_OgreNextAcquireDeps` to `OnLeave_OgreNextWinAcquireDeps`. In the copy, `OgreNextCheckoutReady` becomes `OgreNextWinCheckoutReady` and `OgreNextAcquire` becomes `OgreNextWinAcquire`.

Copy `OnLeave_OgreNextAcquireEngine` to `OnLeave_OgreNextWinAcquireEngine` and make the same two renames.

In `BeginWinOgreNext`, after the welcome `AddInfoPage` add:

```javascript
local reason = OgreNextWinResolvePaths();
if (!reason.IsEmpty())
{
    Wizard.AddInfoPage(_T("OgreNextWinPaths"), reason);
    return;
}

local gitReason = OgreNextWinGitBlock();
if (!gitReason.IsEmpty())
{
    Wizard.AddInfoPage(_T("OgreNextWinGit"), gitReason);
    return;
}

if (!OgreNextWinCheckoutReady(_T("ogre-next-deps"), OgreNextDepsSource))
{
    Wizard.AddInfoPage(
        _T("OgreNextWinAcquireDeps"),
        _("The pinned dependency checkout is not present.\n\nDestination:\n    ") + OgreNextDepsSource + _("\n\nPress Next to download it."));
}

if (!OgreNextWinCheckoutReady(_T("ogre-next"), OgreNextEngineSource))
{
    Wizard.AddInfoPage(
        _T("OgreNextWinAcquireEngine"),
        _("The pinned OGRE-Next checkout is not present.\n\nDestination:\n    ") + OgreNextEngineSource + _("\n\nPress Next to download it."));
}
```

Check, using the script copy in section 2. If everything went well, the welcome page is followed by a download page. Next starts Git. Cancel on the Git question leaves the destination uncreated. 

Check also whether in C:/Users/<current user>/AppData/CodeBlocks we have the ogre-next-deps and ogre-next source downloaded.

If you feel like it, update the welcome message so the wizard doesn't say anymore that it doesn't do anything ...


## 6. Teach the Wizard to apply

Copy these functions and paste them above `BeginWinOgreNext`. Rename the new copies:

```text
OgreNextPatchState         -> OgreNextWinPatchState
    Wizard.MacPatchProbe           -> Wizard.WinPatchProbe

OgreNextApplyRequired      -> OgreNextWinApplyRequired
    OgreNextPatchState             -> OgreNextWinPatchState
    Wizard.MacPatchProbe           -> Wizard.WinPatchProbe
    Wizard.MacPatchApply           -> Wizard.WinPatchApply
```

Copy `OnLeave_OgreNextPatchDeps` to `OnLeave_OgreNextWinPatchDeps`. In the copy, use the `Win` names from the list above. Delete the second `OgreNextApplyRequired` call, the one whose id is `ogre-next-deps-apple`. The function returns after `ogre-next-deps-mingw`.

Copy `OnLeave_OgreNextPatchEngine` to `OnLeave_OgreNextWinPatchEngine`. Delete the call whose id is `ogre-apple`. The function returns after `ogre-next-v3-0-mingw`.

`already_applied` is already treated as success inside the copied `OgreNextApplyRequired`. A cancel returns false, so the next page does not start.

In `BeginWinOgreNext`, after the dependency download `AddInfoPage`, insert:

```javascript
if (!OgreNextWinPatchState(_T("ogre-next-deps-mingw"), OgreNextDepsSource).Matches(_T("already_applied")))
{
    Wizard.AddInfoPage(
        _T("OgreNextWinPatchDeps"),
        _("The dependency checkout needs the MinGW patch.\n\nPress Next to review it. Declining does not start configuration."));
}
```

After the engine download `AddInfoPage`, insert:

```javascript
if (!OgreNextWinPatchState(_T("ogre-next-v3-0-mingw"), OgreNextEngineSource).Matches(_T("already_applied")))
{
    Wizard.AddInfoPage(
        _T("OgreNextWinPatchEngine"),
        _("The engine checkout needs the MinGW patch.\n\nPress Next to review it. Declining does not start configuration."));
}
```

Check using the build instructions in section 2: after each download page comes a patch page.


## 7. `wincmake.h` and `wincmake.cpp` - configure and build

We are still operating in `src/plugins/scriptedwizard`. The method is still the same: get useful code from the mac implementation and transform it into the Windows code by applying Operating-System specific edits.

Open `maccmake.h`. Copy the declaration of the function `RunMacCMakeOgreNext`  (lines 117–126). Paste it into `wincmake.h`, still inside `#if defined(__WXMSW__)`, after the declaration of `InstallWinOgre`.

Rename:

`RunMacCMakeOgreNext` -> `RunWinCMakeOgreNext`

Add one argument directly after `parent`:

```cpp
const wxString& compilerId,
```

The wizard's selected compiler is that argument. Squirrel will not pass it; the binding below reads it in C++.

The copied function already accepts only `ogre-next-deps` and `ogre-next`, and only `configure`, `build`, and `install`. Leave those checks. An unknown value returns before CMake starts.

### Helpers to copy into `wincmake.cpp`

Open `maccmake.cpp`. Copy these into the anonymous namespace of `wincmake.cpp`, above the line `} // namespace` that sits just before `WinCMakeProbe ProbeWinCMake()`.

```text
struct ExpectedEntry                         line 1699
kOgreNextDepsFiles                           line 1705
kOgreNextEngineFiles                         line 1718
enum OgreNextCacheState                      line 1741
struct OgreNextTree                          line 1749
CacheValue                                   line 1787
ResolveOgreNextTree                          line 1875
FilesPresent                                 line 1927
OgreNextFilesPresent                         line 1956
ReadOgreNextCache                            line 1976
OgreNextConfigureArguments                   line 2066
OgreNextConfigureDescription                 line 2093
```

`CacheValue` opens `CMakeCache.txt` with `wxTextFile`. Add `#include <wx/textfile.h>` next to the other `#include <wx/...>` lines at the top of `wincmake.cpp`.

Leave these where they are. `wincmake.cpp` already has them:

```text
CodeBlocksRoot()          line 376
BoundedJobCount()         line 309
SamePath()                line 241
MingwTools()              line 981
RunCMake()                line 1028
FormatWinCMakeRun()       line 2906
```

`RunMacCMakeOgreNext` itself is not part of this paste. It goes outside the anonymous namespace, in the next subsection, because `wiz.cpp` has to call it.

### Folder paths

In the pasted `ResolveOgreNextTree`, search for:

```cpp
const wxString root = CodeBlocksRoot(error);

if (root.empty())
    return false;
```

Replace it with:

```cpp
const wxString root = CodeBlocksRoot();

if (root.empty())
{
    error = wxT("LOCALAPPDATA is empty");
    return false;
}
```

The Windows `CodeBlocksRoot()` takes no error argument. The rest of `ResolveOgreNextTree` already builds the six folders, so leave it:

```text
ogre-next-deps-source-3.0.0
ogre-next-deps-source-3.0.0\build-codeblocks
ogre-next-deps-source-3.0.0\build-codeblocks\ogredeps
ogre-next-source-3.0.0
ogre-next-source-3.0.0\build-codeblocks
ogre-next-3.0.0
```

### Files the install must contain

The pasted arrays name Mac files (`.dylib`, `.framework`). Replace the whole brace list of `kOgreNextDepsFiles` with:

```cpp
{ wxT("include/FreeImage.h"), false },
{ wxT("include/SDL2/SDL.h"), false },
{ wxT("bin/Release/SDL2.dll"), false },
```

Replace the whole brace list of `kOgreNextEngineFiles` with:

```cpp
{ wxT("include/OGRE/OgreRoot.h"), false },
{ wxT("include/OGRE/Hlms/Unlit/OgreHlmsUnlit.h"), false },
{ wxT("include/OGRE/Hlms/Pbs/OgreHlmsPbs.h"), false },
{ wxT("lib/Release/libOgreMain.dll.a"), false },
{ wxT("lib/Release/libOgreHlmsPbs.dll.a"), false },
{ wxT("lib/Release/libOgreHlmsUnlit.dll.a"), false },
{ wxT("bin/Release/OgreMain.dll"), false },
{ wxT("bin/Release/RenderSystem_Direct3D11.dll"), false },
{ wxT("bin/Release/RenderSystem_GL3Plus.dll"), false },
{ wxT("bin/Release/Plugin_ParticleFX.dll"), false },
{ wxT("Media/Hlms/Unlit"), true },
{ wxT("Media/Hlms/Pbs"), true },
{ wxT("Media/materials/textures"), true },
```

`false` means a file. `true` means a directory. Keep the forward slashes inside the strings. `FilesPresent` joins them with `wxFILE_SEP_PATH`.

### Cache check

In the pasted `ReadOgreNextCache`, delete the second parameter `const wxString& sdk`.

Search for `SameDirectory` in that function and replace each call with `SamePath`. `SamePath` is the comparison already in this file.

Delete the block that reads `CMAKE_OSX_ARCHITECTURES` and returns `OgreNextCacheDiffers`. Delete the `wxString sysroot` lines and both blocks that mention `CMAKE_OSX_SYSROOT`. Those three checks are the Mac SDK pin.

Leave the checks that remain: `CMakeCache.txt` exists, `CMAKE_INSTALL_PREFIX` matches `tree.install`, `CMAKE_BUILD_TYPE` is `Release`, and, for the engine only, `OGRE_DEPENDENCIES_DIR` matches `tree.dependencies` and both `OGRE_BUILD_SAMPLES2` and `OGRE_BUILD_TESTS` are `OFF`.

Every call of `ReadOgreNextCache(tree, sdk, error)` in the pasted code becomes `ReadOgreNextCache(tree, error)`.

### Configure arguments

In the pasted `OgreNextConfigureArguments`, replace the parameter `const wxString& sdk` with:

```cpp
const wxString& gcc,
const wxString& gxx,
const wxString& make,
const wxString& windres
```

Delete these two lines:

```cpp
args.Add(wxT("-DCMAKE_OSX_ARCHITECTURES=arm64"));
args.Add(wxT("-DCMAKE_OSX_SYSROOT=") + sdk);
```

After the `-B` / `tree.build` pair, paste the generator and compiler lines from `ConfigureWinCMake` in this same file (around line 1276). Stop before `-DCMAKE_INSTALL_PREFIX` and before every `-DFLTK_` line. The lines to add are:

```cpp
args.Add(wxT("-G"));
args.Add(wxT("MinGW Makefiles"));
args.Add(wxT("-DCMAKE_C_COMPILER=") + gcc);
args.Add(wxT("-DCMAKE_CXX_COMPILER=") + gxx);
args.Add(wxT("-DCMAKE_MAKE_PROGRAM=") + make);
args.Add(wxT("-DCMAKE_RC_COMPILER=") + windres);
```

Leave the comment and the `if (tree.engine)` block. That block is what adds `-DCMAKE_INSTALL_PREFIX`, `-DOGRE_DEPENDENCIES_DIR`, `-DOGRE_BUILD_SAMPLES2=OFF`, and `-DOGRE_BUILD_TESTS=OFF` for the engine only. The dependency configure must not pass `CMAKE_INSTALL_PREFIX`, because that project then writes `build-codeblocks\ogredeps` by itself.

In `OgreNextConfigureDescription`, delete the `sdk` parameter and delete the lines that append `Architecture:` and `SDK:`. The dialog text no longer has a Mac SDK to show.

### The run function

Go back to `maccmake.cpp` and copy `RunMacCMakeOgreNext` (from line 2599) to the bottom of `wincmake.cpp`, after `FormatWinCMakeRun`, still above `#endif // __WXMSW__`. This one stays outside the anonymous namespace.

Rename `MacCMakeRunResult` to `WinCMakeRunResult` and `RunMacCMakeOgreNext` to `RunWinCMakeOgreNext`. Add `const wxString& compilerId` after `parent`, matching the header.

Delete the `ReadMacSdkPath` block. That function runs `xcrun`. In its place, paste the `MingwTools` call from `ConfigureWinCMake` (around line 1260):

```cpp
wxString gcc;
wxString gxx;
wxString make;
wxString bin;

if (!MingwTools(compilerId, gcc, gxx, make, bin, error))
{
    result.output = error;
    return result;
}

const wxString windres = bin + wxT("\\windres.exe");

if (!wxFileExists(windres))
{
    result.output = wxT("windres.exe is not beside gcc.exe");
    return result;
}
```

`MingwTools` fills `gcc`, `gxx`, `mingw32-make`, and the compiler `bin` for the wizard's compiler. `windres.exe` is that `bin` plus the file name, so the dependencies, the engine, and the generated project use one MinGW.

Search the pasted function for `ConfirmConfigure` and replace that name with `ConfirmAction`. `ConfirmAction` is the yes/no dialog already in `wincmake.cpp`.

Delete the `ProbeMacCMake` / `MacCMakeWorks` block. `RunCMake`, already in this file, checks that `cmake.exe` works.

The three Mac dialog launches do not exist on Windows. Replace each one with `RunCMake`. Pass `wxEmptyString` as its confirm-label argument, because `ConfirmAction` already asked.

Configure branch. Delete from `MacCMakeConfigureDialog dlg` through `result.success = true; return result;` and put this there:

```cpp
result = RunCMake(
    parent,
    wxT("configure"),
    OgreNextConfigureArguments(tree, gcc, gxx, make, windres),
    bin,
    OgreNextConfigureDescription(tree),
    explanation,
    wxEmptyString);

if (!result.success)
    return result;

wxString cacheError;

if (ReadOgreNextCache(tree, cacheError) != OgreNextCacheMatches)
{
    result.success = false;
    result.output = cacheError;
    return result;
}

result.success = true;
return result;
```

Build branch. `BoundedJobCount()` is already in this file. Delete from `MacCMakeBuildDialog dlg` through the build `return result;` and put this there:

```cpp
const int jobs = BoundedJobCount();
wxArrayString buildArgs;
buildArgs.Add(wxT("--build"));
buildArgs.Add(tree.build);
buildArgs.Add(wxT("--parallel"));
buildArgs.Add(wxString::Format(wxT("%d"), jobs));

return RunCMake(
    parent,
    wxT("build"),
    buildArgs,
    wxEmptyString,
    wxString::Format(
        _("CMake is building.\n\nBuild:\n%s\n\nParallel jobs: %d"),
        tree.build,
        jobs),
    explanation,
    wxEmptyString);
```

Install branch. Delete from `MacCMakeInstallDialog dlg` through the final `return result;` of the function. Put this stub there. Section 10 replaces it with the real install. The build page must not install yet.

```cpp
result.operation = wxT("install");
result.output = wxT("install comes in a later step");
return result;
```

Search the pasted function for `Mac`, `sdk`, and `xcrun`. Those words should be gone.

`FormatWinCMakeRun` already prints `launched`, `cancelled`, `exit`, `success`, `operation`, and `output`. The binding below returns that string. Do not add a second formatter.

### Squirrel binding for `RunWinCMakeOgreNext`

Open `wiz.cpp`. Copy `Wiz_MacCMakeOgreNext` (lines 2304–2332). Paste it immediately after the closing `}` of `Wiz_WinCMakeInstall`. That function is already inside `#if defined(__WXMSW__)`.

Rename the pasted function to `Wiz_WinCMakeOgreNext`.

Inside it, change:

```text
"Wiz::MacCMakeOgreNext"   -> "Wiz::WinCMakeOgreNext"
MacCMakeRunResult         -> WinCMakeRunResult
RunMacCMakeOgreNext       -> RunWinCMakeOgreNext
FormatMacCMakeRun         -> FormatWinCMakeRun
```

The string is only the text Squirrel shows when the call has the wrong arguments. `FormatWinCMakeRun` is the report string.

The Mac call passes `GetWizardWindow()` and then the four script strings. Insert the compiler between the window and the first string, copying the line from `Wiz_WinOgreConfigure`:

```cpp
extractor.p0->GetCompilerID(),
```

The call becomes:

```cpp
const WinCMakeRunResult result =
    RunWinCMakeOgreNext(
        extractor.p0->GetWizardWindow(),
        extractor.p0->GetCompilerID(),
        *extractor.p1,
        *extractor.p2,
        *extractor.p3,
        *extractor.p4);
```

`ExtractParams5` stays as it is. Squirrel still passes `projectId`, `operation`, `explanation`, and `confirmLabel`. `GetCompilerID()` is the compiler selected on the wizard page.

After the `WinOgreNextAcquire` binding from section 4, add:

```cpp
BindMethod(v, _SC("WinCMakeOgreNext"),
           Wiz_WinCMakeOgreNext,
           _SC("Wiz::WinCMakeOgreNext"));
```

Look, using the C++ steps in section 2: the IDE starts, and the wizard still ends at the patch pages. `Wizard.WinCMakeOgreNext` exists, and no page calls it yet.




## 9. Build pages

Copy these functions and paste them above `BeginWinOgreNext`.

```text
OgreNextCMakeBlock         -> OgreNextWinCMakeBlock
    Wizard.MacCMakeProbe           -> Wizard.WinCMakeProbe
    OgreNextResolvePaths()         -> OgreNextWinResolvePaths()

OgreNextRun                -> OgreNextWinRun
    Wizard.MacCMakeOgreNext        -> Wizard.WinCMakeOgreNext

OgreNextBuildTree          -> OgreNextWinBuildTree
    OgreNextRun                    -> OgreNextWinRun
```

In `OgreNextWinBuildTree`, delete the third call, the one whose operation is `_T("install")`. This page configures and builds. Section 11 adds the install page.

Copy `OnLeave_OgreNextBuildDeps` to `OnLeave_OgreNextWinBuildDeps`. In the copy, delete both `OgreNextDepsInstalled` checks and delete the final `ShowError` about the file list. `OgreNextBuildTree` becomes `OgreNextWinBuildTree`. The function returns true when that call returns true. The file list is the install page's check, in section 11.

Copy `OnLeave_OgreNextBuildEngine` to `OnLeave_OgreNextWinBuildEngine`. Delete the `OgreNextDepsInstalled` check, the `OgreNextEngineInstalled` checks, and the final file-list `ShowError`. `OgreNextBuildTree` becomes `OgreNextWinBuildTree`.

In `BeginWinOgreNext`, after the dependency patch page, insert:

```javascript
local cmakeReason = OgreNextWinCMakeBlock();
if (!cmakeReason.IsEmpty())
{
    Wizard.AddInfoPage(_T("OgreNextWinCMake"), cmakeReason);
    return;
}

Wizard.AddInfoPage(
    _T("OgreNextWinBuildDeps"),
    _("The dependency SDK is not built yet.\n\nPress Next to configure and build it. This page does not install."));
```

After the engine patch page, insert:

```javascript
Wizard.AddInfoPage(
    _T("OgreNextWinBuildEngine"),
    _("OGRE-Next is not built yet.\n\nPress Next to configure and build it. This page does not install."));
```

`OgreNextWinCMakeBlock` calls `Wizard.WinCMakeProbe`. A missing CMake shows one page and returns. The wizard does not install CMake.

The build can take 1 to 2 hours. Compiler lines in the CMake window mean it is still working.

Look, using the script copy in section 2: after the patch pages come the two build pages. Next runs configure, then build. The install folders stay empty. Cancel stops the page and leaves files already written in place.


## 10. Install in C++

Open `RunWinCMakeOgreNext` in `wincmake.cpp`. Search for:

```cpp
result.output = wxT("install comes in a later step");
```

Replace that stub, from `result.operation = wxT("install");` through its `return result;`, with the install command. There is no `--prefix` argument. The prefix is already in `CMakeCache.txt` from configure.

```cpp
wxArrayString installArgs;
installArgs.Add(wxT("--install"));
installArgs.Add(tree.build);

result = RunCMake(
    parent,
    wxT("install"),
    installArgs,
    wxEmptyString,
    _("CMake is installing.\n\nBuild:\n") + tree.build + _("\n\nInstall:\n") + tree.install,
    explanation,
    wxEmptyString);

if (!result.success)
    return result;

if (!OgreNextFilesPresent(tree, filesError))
{
    result.success = false;
    result.output = filesError;
    return result;
}

result.success = true;
return result;
```

The file arrays from section 8 are what `OgreNextFilesPresent` checks. No new binding. `Wizard.WinCMakeOgreNext(..., "install", ...)` now does this branch.

Look, using the C++ steps in section 2: the wizard is unchanged, and it still ends at the build pages.


## 11. Install pages

Copy `OgreNextDepsInstalled` to `OgreNextWinDepsInstalled`. Replace the `return IO.FileExists...` chain with:

```javascript
return IO.FileExists(OgreNextJoin(root, _T("include/FreeImage.h")))
    && IO.FileExists(OgreNextJoin(root, _T("include/SDL2/SDL.h")))
    && IO.FileExists(OgreNextJoin(root, _T("bin/Release/SDL2.dll")));
```

Copy `OgreNextEngineInstalled` to `OgreNextWinEngineInstalled`. Replace its `return` chain with:

```javascript
return IO.FileExists(OgreNextJoin(root, _T("include/OGRE/OgreRoot.h")))
    && IO.FileExists(OgreNextJoin(root, _T("include/OGRE/Hlms/Unlit/OgreHlmsUnlit.h")))
    && IO.FileExists(OgreNextJoin(root, _T("include/OGRE/Hlms/Pbs/OgreHlmsPbs.h")))
    && IO.FileExists(OgreNextJoin(root, _T("lib/Release/libOgreMain.dll.a")))
    && IO.FileExists(OgreNextJoin(root, _T("lib/Release/libOgreHlmsPbs.dll.a")))
    && IO.FileExists(OgreNextJoin(root, _T("lib/Release/libOgreHlmsUnlit.dll.a")))
    && IO.FileExists(OgreNextJoin(root, _T("bin/Release/OgreMain.dll")))
    && IO.FileExists(OgreNextJoin(root, _T("bin/Release/RenderSystem_Direct3D11.dll")))
    && IO.FileExists(OgreNextJoin(root, _T("bin/Release/RenderSystem_GL3Plus.dll")))
    && IO.FileExists(OgreNextJoin(root, _T("bin/Release/Plugin_ParticleFX.dll")))
    && IO.DirectoryExists(OgreNextJoin(root, _T("Media/Hlms/Unlit")))
    && IO.DirectoryExists(OgreNextJoin(root, _T("Media/Hlms/Pbs")))
    && IO.DirectoryExists(OgreNextJoin(root, _T("Media/materials/textures")));
```

The Mac lists stay in `OgreNextDepsInstalled` and `OgreNextEngineInstalled`.

Add this function above `BeginWinOgreNext`:

```javascript
function OgreNextWinInstall(projectId, title)
{
    local install = _("Code::Blocks will install ") + title + _(" with cmake --install.\n\nNo replacement prefix is passed. The prefix from configuration is used.");
    return OgreNextWinRun(projectId, _T("install"), install, _("Install ") + title);
}
```

Copy is not required for the leave handlers. Add them above `BeginWinOgreNext`:

```javascript
function OnLeave_OgreNextWinInstallDeps(fwd)
{
    if (!fwd)
        return true;
    if (OgreNextWinDepsInstalled())
        return true;
    if (!OgreNextWinInstall(_T("ogre-next-deps"), _("the OGRE-Next dependencies")))
        return false;
    if (OgreNextWinDepsInstalled())
        return true;
    ShowError(_("CMake finished, but the dependency SDK file list is not present."));
    return false;
}

function OnLeave_OgreNextWinInstallEngine(fwd)
{
    if (!fwd)
        return true;
    if (OgreNextWinEngineInstalled())
        return true;
    if (!OgreNextWinDepsInstalled())
    {
        ShowError(_("The dependency SDK is not installed."));
        return false;
    }
    if (!OgreNextWinInstall(_T("ogre-next"), _("OGRE-Next")))
        return false;
    if (OgreNextWinEngineInstalled())
        return true;
    ShowError(_("CMake finished, but the OGRE-Next SDK file list is not present."));
    return false;
}
```

In `OnLeave_OgreNextWinBuildDeps`, put the skip back at the top of the forward path:

```javascript
if (OgreNextWinDepsInstalled())
    return true;
```

In `OnLeave_OgreNextWinBuildEngine`, put both skips back:

```javascript
if (OgreNextWinEngineInstalled())
    return true;
if (!OgreNextWinDepsInstalled())
{
    ShowError(_("The dependency SDK is not installed."));
    return false;
}
```

In `BeginWinOgreNext`, wrap the dependency build page so it is skipped once the SDK files exist, and add the install page after it:

```javascript
if (!OgreNextWinDepsInstalled())
{
    Wizard.AddInfoPage(
        _T("OgreNextWinBuildDeps"),
        _("The dependency SDK is not built yet.\n\nPress Next to configure and build it. This page does not install."));
    Wizard.AddInfoPage(
        _T("OgreNextWinInstallDeps"),
        _("The dependency SDK is not installed yet.\n\nDestination:\n    ") + OgreNextDepsInstall + _("\n\nPress Next to install it."));
}
```

Do the same for the engine, using `OgreNextWinEngineInstalled`, `OgreNextWinBuildEngine`, `OgreNextWinInstallEngine`, and `OgreNextInstall` in the destination line.

Look, using the script copy in section 2: after each build page comes an install page. Next runs `cmake --install`. A second run skips a pair whose file list is already present.


## 12. The ordinary Code::Blocks project

Create this file. Leave `files/plugins.cfg` unchanged. That file is the Mac plugin list.

```text
src/plugins/scriptedwizard/resources/ogre-next/files/plugins-windows.cfg
```

The contents are exactly:

```text
PluginFolder=[OGRE_NEXT_BIN]
PluginOptional=RenderSystem_Direct3D11
PluginOptional=RenderSystem_GL3Plus
PluginOptional=RenderSystem_Vulkan
Plugin=Plugin_ParticleFX
```

Leave `files/resources2.cfg` and `files/main.cpp` unchanged. They already contain `[OGRE_NEXT_MEDIA]` and `[OGRE_NEXT_TEXTURES]`.

In `GetGeneratedFile`, case 1 currently sets `templateFile` to `ogre-next/files/plugins.cfg`. Change that case to:

```javascript
case 1:
    filename = _T("plugins.cfg");
    templateFile = _T("ogre-next/files/plugins.cfg");
    if (PLATFORM == PLATFORM_MSW)
        templateFile = _T("ogre-next/files/plugins-windows.cfg");
    break;
```

After the two existing `buffer.Replace` lines for `[OGRE_NEXT_MEDIA]` and `[OGRE_NEXT_TEXTURES]`, add:

```javascript
buffer.Replace(_T("[OGRE_NEXT_BIN]"), OgreNextJoin(OgreNextInstall, _T("bin/Release")));
```

`OgreNextJoin` writes `/`. The Mac template has no `[OGRE_NEXT_BIN]`, so this extra `Replace` changes nothing on Mac. OGRE loads the render-system DLLs from that folder. `OgreMain.dll` is copied beside the exe below, because Windows loads that DLL from the exe's own folder.

Copy `OgreNextAllReady` to `OgreNextWinAllReady`. In the copy:

- Delete the `OgreNextBrewBlock()` block.
- `OgreNextCMakeBlock` -> `OgreNextWinCMakeBlock`
- `OgreNextGitBlock` -> `OgreNextWinGitBlock`
- `OgreNextCheckoutReady` -> `OgreNextWinCheckoutReady`
- `OgreNextDepsInstalled` -> `OgreNextWinDepsInstalled`
- `OgreNextEngineInstalled` -> `OgreNextWinEngineInstalled`
- `OgreNextPatchState` -> `OgreNextWinPatchState`
- Delete the two lines that mention `ogre-next-deps-apple` and `ogre-apple`.

`OnLeave_ProjectPathPage` and `OnLeave_CompilerPage` currently call `OgreNextAllReady()`. Change each body to:

```javascript
if (PLATFORM == PLATFORM_MSW)
    return !fwd || OgreNextWinAllReady();
return !fwd || OgreNextAllReady();
```

Those two pages are shared. The branch keeps the Mac check on Mac.

In `BeginWinOgreNext`, after the engine install page, add:

```javascript
Wizard.AddProjectPathPage();
Wizard.AddCompilerPage(_T(""), _T("*"), true, true);
```

In `SetupProject()`, replace the Windows guard from section 3 with:

```javascript
if (PLATFORM == PLATFORM_MSW)
    return SetupWinOgreNextProject(project);
```

Copy `SetupProject` and rename the copy `SetupWinOgreNextProject`. In the copy, change `OgreNextAllReady()` to `OgreNextWinAllReady()`.

Delete these Mac link lines:

```javascript
project.AddLinkerOption(_T("-F") + frameworkDir);
project.AddLinkerOption(_T("-framework Ogre"));
project.AddLinkerOption(_T("-framework OgreHlmsPbs"));
project.AddLinkerOption(_T("-framework OgreHlmsUnlit"));
project.AddLinkerOption(_T("-Wl,-search_paths_first"));
project.AddLinkerOption(_T("-Wl,-headerpad_max_install_names"));
```

Delete the `local afterBuild = ...` block and its `project.AddCommandsAfterBuild(afterBuild);`. That block builds a Mac `.app` and a Frameworks symlink.

In their place, after `project.AddCompilerOption(_T("-std=gnu++17"));`, add:

```javascript
project.AddLibDir(OgreNextJoin(OgreNextInstall, _T("lib/Release")));
project.AddLinkLib(_T("OgreHlmsPbs"));
project.AddLinkLib(_T("OgreHlmsUnlit"));
project.AddLinkLib(_T("OgreMain"));
```

Those three link names are `libOgreHlmsPbs.dll.a`, `libOgreHlmsUnlit.dll.a`, and `libOgreMain.dll.a` in `lib/Release`. `AddLinkLib` on the project applies to both targets. The include lines and `-std=gnu++17` above them stay.

Search for `BeforeLast(_T("/"))` and replace it with `BeforeLast(_T("\\"))`. `GetProjectFullFilename()` separates Windows folders with a backslash.

On both targets, `SetTargetType` must be `ttConsoleOnly`. The release target currently says `ttExecutable`. Change that one. The console window is what keeps `Light show rendered and closed cleanly.` visible.

Delete both `target.SetOutputFilename(...)` lines. Those lines name the Mac `.app`. Without them, Code::Blocks keeps `bin\Debug` and `bin\Release`.

Open `src/plugins/scriptedwizard/resources/ogre/wizard.script`. Copy `OgreWinQueueDll` (lines 1966–1977) into the OGRE-Next script and rename it `OgreNextWinQueueDll`. Do not edit the OGRE 1.x script. The copied function adds one `cmd /c copy /Y` after the link, into `$(TARGET_OUTPUT_DIR)`.

In `SetupWinOgreNextProject`, after the target setup, add:

```javascript
local releaseBin = OgreNextJoin(OgreNextInstall, _T("bin/Release"));
local compilerBin = OgreNextField(Wizard.WinOgreTools(Wizard.GetCompilerID()), _T("bin"));

OgreNextWinQueueDll(project, releaseBin, _T("OgreMain.dll"));
OgreNextWinQueueDll(project, releaseBin, _T("OgreHlmsPbs.dll"));
OgreNextWinQueueDll(project, releaseBin, _T("OgreHlmsUnlit.dll"));
OgreNextWinQueueDll(project, compilerBin, _T("libgcc_s_seh-1.dll"));
OgreNextWinQueueDll(project, compilerBin, _T("libstdc++-6.dll"));
OgreNextWinQueueDll(project, compilerBin, _T("libwinpthread-1.dll"));

project.AddCommandsAfterBuild(
    _T("cmd /c copy /Y \"")
    + OgreNextJoin(OgreNextInstall, _T("Media/models/ogrehead.mesh"))
    + _T("\" \"$(PROJECT_DIR)/ogrehead.mesh\""));
```

`bin=` comes from `Wizard.WinOgreTools`. The three MinGW DLLs then belong to the compiler selected in the wizard. `ogrehead.mesh` is copied into the project folder because `main.cpp` opens it there.

Look, using the script copy in section 2: the wizard now ends with the project-path page and the compiler page. Finish creates the project. Build it. Running it opens a window titled `OGRE-Next Light Show`, shows the rotating Ogre head, and after the window is closed prints `Light show rendered and closed cleanly.`

If a trial edit of this repository should be thrown away before any of it is kept:

```bat
git restore .
```
