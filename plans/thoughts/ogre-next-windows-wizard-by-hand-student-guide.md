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

Note: that anonymous namespace is like the 'using namespace std'. Except for it's anonymous, meaning whatever is in it is reachable only from `wingit.cpp`

For `KnownSourceDestination` we need to adapt the implementation, because Mac and Windows directory organization is different.
 In `macgit.cpp` it starts at the home directory and appends `Library`, then `CodeBlocks`. On Windows the root is `%LOCALAPPDATA%\codeblocks`. After you have pasted the helpers into `wingit.cpp`, replace that whole function with:

```cpp
wxString KnownSourceDestination(const KnownGitSource& source)
{
    wxString local;
    if (!wxGetEnv(wxT("LOCALAPPDATA"), &local) || local.empty())
        return wxString();
    wxFileName dest = wxFileName::DirName(local);
    dest.AppendDir(wxT("codeblocks"));
    dest.AppendDir(source.leaf);
    dest.Normalize(
        wxPATH_NORM_DOTS | wxPATH_NORM_ABSOLUTE | wxPATH_NORM_TILDE);

    return StripTrailingSeparator(dest.GetFullPath());
}
```

### The acquire functions

`wingit.cpp` does not include `<vector>` yet. Add `#include <vector>` next to the other includes, it's need for the list of git processes.

 Paste this at the bottom of `wingit.cpp`, still above `#endif // __WXMSW__`.

```cpp
WinGitAcquireResult AcquireWinOgreNext(
    wxWindow* parent,
    const wxString& sourceId,
    const wxString& explanation,
    const wxString& confirmLabel)
{
    WinGitAcquireResult result;
    result.sourceId = sourceId;

    const KnownGitSource* source = FindKnownSource(sourceId);

    if (!source)
    {
        result.output = wxT("unknown source identifier");
        return result;
    }

    result.url = source->url;
    result.commit = source->commit;
    result.destination = KnownSourceDestination(*source);

    if (result.destination.empty() || result.destination == wxT("/"))
    {
        result.output = wxT("destination is unsafe");
        return result;
    }

    const WinGitProbe probe = ProbeWinGit();
    result.executable = probe.executable;

    if (probe.access != WinGitWorks)
    {
        result.output = wxT("git unavailable");

        if (!probe.errorOutput.empty())
            result.output += wxT(": ") + FirstLine(probe.errorOutput);

        return result;
    }

    const bool exists = wxDirExists(result.destination);
    const bool empty = exists && DirectoryIsEmpty(result.destination);

    wxFileName gitMarker(result.destination, wxT(".git"));
    const bool isGit =
           exists
        && (wxDirExists(gitMarker.GetFullPath())
            || wxFileExists(gitMarker.GetFullPath()));

    wxString head;
    wxString revError;
    const bool headMatches =
           isGit
        && RevParseHead(
               probe.executable,
               result.destination,
               head,
               revError)
        && head == source->commit;

    bool submoduleMatches = source->submodulePath == NULL;

    if (source->submodulePath && headMatches)
    {
        const wxString submoduleDir =
            result.destination
            + wxFILE_SEP_PATH
            + source->submodulePath;

        wxString submoduleHash;

        if (RevParseHead(
                probe.executable,
                submoduleDir,
                submoduleHash,
                revError)
            && submoduleHash == source->submoduleCommit)
        {
            submoduleMatches = true;
            result.submoduleCommit = submoduleHash;
        }
    }

    if (headMatches && submoduleMatches)
    {
        result.alreadyPresent = true;
        result.success = true;
        result.exitCode = 0;
        result.output = wxT("pinned checkout already present");
        return result;
    }

    const bool submoduleOnly =
           headMatches
        && source->submodulePath != NULL
        && !submoduleMatches;

    if (exists && !empty && !submoduleOnly)
    {
        result.output = wxT("destination is not the pinned checkout");
        return result;
    }

    if (!submoduleOnly)
    {
        wxString parentError;

        if (!EnsureParentDirectory(result.destination, parentError))
        {
            result.output = parentError;
            return result;
        }
    }

    if (!ConfirmClone(parent, explanation, confirmLabel))
    {
        result.cancelled = true;
        result.output = wxT("checkout cancelled before launch");
        return result;
    }

    WinGitCloneDialog dlg(
        parent,
        wxString::Format(
            _("Git is checking out %s into:\n%s\n\n"
              "There is no progress percentage."),
            source->commit,
            result.destination));

    if (!dlg.StartCommands(
            probe.executable,
            CheckoutCommands(*source, result.destination, submoduleOnly)))
    {
        result.output = dlg.OutputTail();
        return result;
    }

    dlg.ShowModal();

    result.launched = true;
    result.cancelled = dlg.Cancelled();
    result.exitCode = dlg.ExitCode();
    result.output = dlg.OutputTail();

    if (result.cancelled || result.exitCode != 0)
        return result;

    wxString checked;

    if (!RevParseHead(
            probe.executable,
            result.destination,
            checked,
            revError)
        || checked != source->commit)
    {
        result.success = false;
        result.output = wxT("checked out revision does not match the pin");
        return result;
    }

    if (source->submodulePath)
    {
        const wxString submoduleDir =
            result.destination
            + wxFILE_SEP_PATH
            + source->submodulePath;

        wxString submoduleHash;

        if (!RevParseHead(
                probe.executable,
                submoduleDir,
                submoduleHash,
                revError)
            || submoduleHash != source->submoduleCommit)
        {
            result.success = false;
            result.output = wxT("SDL2 submodule does not match the pin");
            return result;
        }

        result.submoduleCommit = submoduleHash;
    }

    result.success = true;
    return result;
}

wxString FormatWinGitAcquire(const WinGitAcquireResult& result)
{
    return wxString::Format(
        wxT("launched=%d\n"
            "cancelled=%d\n"
            "already_present=%d\n"
            "exit=%ld\n"
            "success=%d\n"
            "executable=%s\n"
            "source_id=%s\n"
            "url=%s\n"
            "commit=%s\n"
            "destination=%s\n"
            "submodule_commit=%s\n"
            "output=%s"),
        result.launched ? 1 : 0,
        result.cancelled ? 1 : 0,
        result.alreadyPresent ? 1 : 0,
        result.exitCode,
        result.success ? 1 : 0,
        result.executable,
        result.sourceId,
        result.url,
        result.commit,
        result.destination,
        result.submoduleCommit,
        FirstLine(result.output));
}
```

### Giz dialog

Navigate to the class `WinGitCloneDialog` in `wingit.cpp`. 

A constructor in C++ is there to initialize objects constructed using a class (here, the series of little dialogs that announce and confirm the git steps). A constructor in C++ is a special function with the class name. The function body is preceded by an initializer list.

In the `WinGitCloneDialog` constructor, the initializer list has `m_process(nullptr),`. Add `m_step(0),` immediately after that using the same syntax.

In C++ a dstructor is called to clean up an object when it is not needed anymore. It is also special function, and has the name `~classname`. Find the destructor of `WinGitCloneDialog`, and replace the destructor body with:

```cpp
m_timer.Stop();
delete m_process;

for (size_t i = 0; i < m_retired.size(); ++i)
    delete m_retired[i];
```

In C++ classes have member functions, known as methods.

Paste `StartCommands` as a public method, directly after `Start`:

```cpp
bool StartCommands(const wxString& executable,
                   const std::vector<wxArrayString>& commands)
{
    m_executable = executable;
    m_commands = commands;
    m_step = 0;

    if (m_commands.empty())
    {
        Append(wxT("Git was not given a command.\n"));
        return false;
    }

    if (!LaunchStep())
        return false;

    m_timer.Start(200);
    return true;
}
```

`LaunchStep` is private. Paste it in the private section, just before `OnProcessEnd`, and replace the whole `OnProcessEnd` with the one here. Next to `wxProcess* m_process;` add the two members at the bottom of this block.

```cpp
bool LaunchStep()
{
    if (m_step >= m_commands.size())
        return false;

    m_process = new wxProcess(this);
    m_process->Redirect();

    wxExecuteEnv env;

    if (!wxGetEnvMap(&env.env) || env.env.empty())
    {
        delete m_process;
        m_process = nullptr;

        Append(
            wxT("Could not copy the environment, "
                "so Git was not started.\n"));

        return false;
    }

    // Do not allow Git to ask for credentials interactively.
    env.env[wxT("GIT_TERMINAL_PROMPT")] = wxT("0");

    wxArrayString& args = m_commands[m_step];

    // wxWidgets 3.0 declares wxExecute(wchar_t**). The strings are not modified.
    std::vector<wxChar*> argv;
    argv.reserve(args.GetCount() + 2);
    argv.push_back(const_cast<wxChar*>(m_executable.wx_str()));

    for (size_t i = 0; i < args.GetCount(); ++i)
        argv.push_back(const_cast<wxChar*>(args.Item(i).wx_str()));

    argv.push_back(nullptr);

    Append(
        wxString::Format(
            wxT("\n$ git %s\n"),
            args.Item(0)));

    m_pid = wxExecute(
        argv.data(),
        wxEXEC_ASYNC,
        m_process,
        &env);

    if (m_pid <= 0)
    {
        delete m_process;
        m_process = nullptr;
        m_pid = 0;

        Append(wxT("Git did not start.\n"));
        return false;
    }

    return true;
}

void OnProcessEnd(wxProcessEvent& event)
{
    ReadPending();

    m_exit = event.GetExitCode();
    m_pid = 0;

    if (m_process)
    {
        m_retired.push_back(m_process);
        m_process = nullptr;
    }

    if (m_cancelled || m_exit != 0)
    {
        m_finished = true;
        Finish();
        return;
    }

    ++m_step;

    if (m_step >= m_commands.size())
    {
        m_finished = true;
        Finish();
        return;
    }

    if (!LaunchStep())
    {
        m_exit = -1;
        m_finished = true;
        Finish();
    }
}

std::vector<wxProcess*> m_retired;
std::vector<wxArrayString> m_commands;
size_t m_step;
```

The two vectors and `m_step` are members, so they go with the other members, not inside a function. 


### Squirrel binding

Open `wiz.cpp`. Find `Wiz_WinGitClone`. Paste this immediately after its closing brace:

```cpp
SQInteger Wiz_WinOgreNextAcquire(HSQUIRRELVM v)
{
    // this, sourceId, explanation, confirmLabel
    ExtractParams4<
        Wiz*,
        const wxString *,
        const wxString *,
        const wxString *
    > extractor(v);

    if (!extractor.Process("Wiz::WinOgreNextAcquire"))
        return extractor.ErrorMessage();

    const WinGitAcquireResult result =
        AcquireWinOgreNext(
            extractor.p0->GetWizardWindow(),
            *extractor.p1,
            *extractor.p2,
            *extractor.p3);

    return ConstructAndReturnInstance(
        v,
        FormatWinGitAcquire(result));
}
```

Find the Windows `BindMethod` for `WinOgreInstall`. Paste this on the next line:

```cpp
BindMethod(v, _SC("WinOgreNextAcquire"), Wiz_WinOgreNextAcquire, _SC("Wiz::WinOgreNextAcquire"));
```

Check whether everything compiles, using the steps in section 2. You just want a successful compilation here. In Code::Blocks, nothing changes as yet.


## 5. Teach the wizard to do git repo download

Open `wizard.script`. Paste these four functions just above `BeginWinOgreNext`. They are the Mac functions with Windows names already substituted. The Mac copies stay where they are.

```javascript
function OgreNextWinResolvePaths()
{
    OgreNextPathsValid = false;
    OgreNextRoot = _T("");
    OgreNextDepsSource = _T("");
    OgreNextDepsInstall = _T("");
    OgreNextEngineSource = _T("");
    OgreNextEngineBuild = _T("");
    OgreNextInstall = _T("");

    local report = Wizard.WinCMakePaths(
        _T("ogre-next-source-3.0.0"),
        _T("ogre-next-3.0.0"));

    if (!OgreNextField(report, _T("valid")).Matches(_T("1")))
        return OgreNextField(report, _T("error"));

    OgreNextRoot = OgreNextField(report, _T("root"));
    OgreNextEngineSource = OgreNextField(report, _T("source"));
    OgreNextEngineBuild = OgreNextField(report, _T("build"));
    OgreNextInstall = OgreNextField(report, _T("install"));
    OgreNextDepsSource = OgreNextJoin(OgreNextRoot, _T("ogre-next-deps-source-3.0.0"));
    OgreNextDepsInstall = OgreNextJoin(
        OgreNextJoin(OgreNextDepsSource, _T("build-codeblocks")),
        _T("ogredeps"));

    if (OgreNextRoot.IsEmpty() || OgreNextEngineSource.IsEmpty() || OgreNextInstall.IsEmpty())
        return _("CMake path resolution returned an incomplete result.");

    OgreNextPathsValid = true;
    return _T("");
}

function OgreNextWinGitBlock()
{
    local report = Wizard.WinGitProbe();
    OgreNextGitAccess = OgreNextField(report, _T("access"));
    OgreNextGitExecutable = OgreNextField(report, _T("executable"));
    OgreNextGitVersion = OgreNextField(report, _T("version"));
    if (OgreNextGitAccess.Matches(_T("works")))
        return _T("");
    if (OgreNextGitAccess.Matches(_T("present_unusable")))
        return _("Git was found but could not be used from Code::Blocks.");
    return _("Git could not be found. Code::Blocks does not install Git.");
}

function OgreNextWinAcquire(sourceId, explanation, confirmLabel)
{
    local report = Wizard.WinOgreNextAcquire(sourceId, explanation, confirmLabel);
    if (OgreNextField(report, _T("success")).Matches(_T("1")))
        return true;
    if (OgreNextField(report, _T("cancelled")).Matches(_T("1")))
    {
        ShowMessage(_("The checkout was stopped. Partial files are left in place."));
        return false;
    }
    ShowError(_("The pinned checkout was not acquired.\n\n") + OgreNextField(report, _T("output")));
    return false;
}

function OgreNextWinCheckoutReady(sourceId, path)
{
    local gitDir = OgreNextJoin(path, _T(".git"));
    if (!IO.DirectoryExists(gitDir) && !IO.FileExists(gitDir))
        return false;
    if (!IO.FileExists(OgreNextJoin(path, _T("CMakeLists.txt"))))
        return false;
    // The directory already exists, so this does not ask to clone.
    // A different commit is refused and left untouched.
    local report = Wizard.WinOgreNextAcquire(sourceId, _T("unused"), _T("unused"));
    return OgreNextField(report, _T("success")).Matches(_T("1"));
}

function OnLeave_OgreNextWinAcquireDeps(fwd)
{
    if (!fwd)
        return true;
    if (OgreNextWinCheckoutReady(_T("ogre-next-deps"), OgreNextDepsSource))
        return true;
    local explanation = _("Code::Blocks will check out the pinned OGRE-Next dependency sources.\n\nDestination:\n") + OgreNextDepsSource + _("\n\nThe commit and the SDL2 submodule pin are fixed. This step downloads source only.");
    return OgreNextWinAcquire(_T("ogre-next-deps"), explanation, _("Download OGRE-Next dependencies"));
}

function OnLeave_OgreNextWinAcquireEngine(fwd)
{
    if (!fwd)
        return true;
    if (OgreNextWinCheckoutReady(_T("ogre-next"), OgreNextEngineSource))
        return true;
    local explanation = _("Code::Blocks will check out the pinned OGRE-Next engine sources.\n\nDestination:\n") + OgreNextEngineSource + _("\n\nThe commit is fixed. This step downloads source only.");
    return OgreNextWinAcquire(_T("ogre-next"), explanation, _("Download OGRE-Next"));
}
```

In `BeginWinOgreNext`, after the welcome `AddInfoPage`, add:

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

Check, using section 2. If everything went well, the welcome page is followed by a download page. Next starts the Git download.

After the wizard git step, check also whether `C:\Users\<you>\AppData\Local\codeblocks` contains `ogre-next-deps-source-3.0.0` and `ogre-next-source-3.0.0`.

If you want, update the welcome message so it no longer says the wizard does nothing.


## 6. Teach the Wizard to apply the Patches

Open `OgreNextApplyRequired` in `wizard.script`. It probes whether the patch actually fits the downloaded code, and if the patch is applicable it asks whether it should be applied, then applies. The Windows copies call `Wizard.WinPatchProbe` and `Wizard.WinPatchApply`. Paste these above `BeginWinOgreNext`. The leave handlers apply only the MinGW patch. The Apple patch ids stay in the Mac functions.

```javascript
function OgreNextWinPatchState(patchId, source)
{
    local report = Wizard.WinPatchProbe(patchId, source);
    return OgreNextField(report, _T("state"));
}

function OgreNextWinApplyRequired(patchId, source, explanation, confirmLabel)
{
    local state = OgreNextWinPatchState(patchId, source);
    if (state.Matches(_T("already_applied")))
        return true;
    if (!state.Matches(_T("applicable")))
    {
        local report = Wizard.WinPatchProbe(patchId, source);
        ShowError(_("This patch cannot be applied, so configuration will not start.\n\n") + OgreNextField(report, _T("error")));
        return false;
    }
    local report = Wizard.WinPatchApply(patchId, source, explanation, confirmLabel);
    if (OgreNextField(report, _T("success")).Matches(_T("1")))
        return true;
    if (OgreNextField(report, _T("cancelled")).Matches(_T("1")))
    {
        ShowMessage(_("The patch was not applied. Configuration will not start."));
        return false;
    }
    local error = OgreNextField(report, _T("error"));
    if (error.IsEmpty())
        error = OgreNextField(report, _T("output"));
    ShowError(_("The patch was not applied.\n\n") + error);
    return false;
}

function OnLeave_OgreNextWinPatchDeps(fwd)
{
    if (!fwd)
        return true;
    if (!OgreNextWinCheckoutReady(_T("ogre-next-deps"), OgreNextDepsSource))
    {
        ShowError(_("The dependency checkout is not the pinned revision."));
        return false;
    }
    local mingw = _("Code::Blocks will apply the packaged MinGW dependency patch to the pinned checkout.\n\nDeclining leaves the checkout unchanged and does not start configuration.");
    return OgreNextWinApplyRequired(_T("ogre-next-deps-mingw"), OgreNextDepsSource, mingw, _("Apply dependency patch"));
}

function OnLeave_OgreNextWinPatchEngine(fwd)
{
    if (!fwd)
        return true;
    if (!OgreNextWinCheckoutReady(_T("ogre-next"), OgreNextEngineSource))
    {
        ShowError(_("The engine checkout is not the pinned revision."));
        return false;
    }
    local mingw = _("Code::Blocks will apply the packaged OGRE-Next engine patch to the pinned checkout.\n\nDeclining leaves the checkout unchanged and does not start configuration.");
    return OgreNextWinApplyRequired(_T("ogre-next-v3-0-mingw"), OgreNextEngineSource, mingw, _("Apply engine patch"));
}
```


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

Check using the build instructions in section 2: after each download page there should now be a patch page.


## 7. `wincmake.h` and `wincmake.cpp` - configure and build

Still in `src/plugins/scriptedwizard`. Open `maccmake.cpp` and find the anonymous-namespace helpers that describe an OGRE-Next tree. Copy these into the anonymous namespace of `wincmake.cpp`, above the `} // namespace` that sits just before `WinCMakeProbe ProbeWinCMake()`.

```text
struct ExpectedEntry
kOgreNextDepsFiles
kOgreNextEngineFiles
enum OgreNextCacheState
struct OgreNextTree
CacheValue
ResolveOgreNextTree
FilesPresent
OgreNextFilesPresent
```



Still in `wincmake.cpp`: Add `#include <wx/textfile.h>` next to the other `#include <wx/...>` lines at the top of `wincmake.cpp`. That hashtag file is needed by the new code.

In the copied `ResolveOgreNextTree`, the Mac call is `CodeBlocksRoot(error)`. The Windows function takes no argument. Replace that call and the empty-root test with:

```cpp
const wxString root = CodeBlocksRoot();

if (root.empty())
{
    error = wxT("LOCALAPPDATA is empty");
    return false;
}
```

Leave the six folders that follow:

```text
ogre-next-deps-source-3.0.0
ogre-next-deps-source-3.0.0\build-codeblocks
ogre-next-deps-source-3.0.0\build-codeblocks\ogredeps
ogre-next-source-3.0.0
ogre-next-source-3.0.0\build-codeblocks
ogre-next-3.0.0
```

The copied file lists name Mac libraries (`.dylib`, `.framework`). Replace the brace list of `kOgreNextDepsFiles` with:

```cpp
{ wxT("include/FreeImage.h"), false },
{ wxT("include/SDL2/SDL.h"), false },
{ wxT("bin/Release/SDL2.dll"), false },
```

This is correct: instead of the 9 entries in the mac case, only 3 are need under windows.

Replace the brace list of `kOgreNextEngineFiles` with:

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

Keep the forward slashes even though we are under Windows. `FilesPresent` joins them with `wxFILE_SEP_PATH`.

Paste these three into that same anonymous namespace of `wincmake.cpp`:

```cpp
OgreNextCacheState ReadOgreNextCache(
    const OgreNextTree& tree,
    wxString& error)
{
    const wxString cachePath =
        tree.build
        + wxFILE_SEP_PATH
        + wxT("CMakeCache.txt");

    if (!wxFileExists(cachePath))
    {
        error = wxT("CMakeCache.txt was not found");
        return OgreNextCacheMissing;
    }

    wxString prefix;

    if (!CacheValue(cachePath, wxT("CMAKE_INSTALL_PREFIX"), prefix)
        || !SamePath(prefix, tree.install))
    {
        error =
            tree.engine
                ? wxT("CMAKE_INSTALL_PREFIX does not match the pinned install directory")
                : wxT("CMAKE_INSTALL_PREFIX is not the dependency SDK");

        return OgreNextCachePrefixWrong;
    }

    wxString buildType;

    if (!CacheValue(cachePath, wxT("CMAKE_BUILD_TYPE"), buildType)
        || buildType != wxT("Release"))
    {
        error = wxT("CMAKE_BUILD_TYPE is not Release");
        return OgreNextCacheDiffers;
    }

    if (tree.engine)
    {
        wxString dependencies;

        if (!CacheValue(cachePath, wxT("OGRE_DEPENDENCIES_DIR"), dependencies)
            || !SamePath(dependencies, tree.dependencies))
        {
            error = wxT("OGRE_DEPENDENCIES_DIR does not match the dependency SDK");
            return OgreNextCacheDiffers;
        }

        wxString samples;
        wxString tests;

        if (!CacheValue(cachePath, wxT("OGRE_BUILD_SAMPLES2"), samples)
            || samples != wxT("OFF")
            || !CacheValue(cachePath, wxT("OGRE_BUILD_TESTS"), tests)
            || tests != wxT("OFF"))
        {
            error = wxT("samples or tests are not off");
            return OgreNextCacheDiffers;
        }
    }

    return OgreNextCacheMatches;
}

wxArrayString OgreNextConfigureArguments(
    const OgreNextTree& tree,
    const wxString& gcc,
    const wxString& gxx,
    const wxString& make,
    const wxString& windres)
{
    wxArrayString args;

    args.Add(wxT("-S"));
    args.Add(tree.source);
    args.Add(wxT("-B"));
    args.Add(tree.build);
    args.Add(wxT("-DCMAKE_BUILD_TYPE=Release"));
    args.Add(wxT("-G"));
    args.Add(wxT("MinGW Makefiles"));
    args.Add(wxT("-DCMAKE_C_COMPILER=") + gcc);
    args.Add(wxT("-DCMAKE_CXX_COMPILER=") + gxx);
    args.Add(wxT("-DCMAKE_MAKE_PROGRAM=") + make);
    args.Add(wxT("-DCMAKE_RC_COMPILER=") + windres);

    // The dependency project writes <build>/ogredeps only while the
    // prefix is still CMake's default. Do not pass CMAKE_INSTALL_PREFIX.
    if (tree.engine)
    {
        args.Add(wxT("-DCMAKE_INSTALL_PREFIX=") + tree.install);
        args.Add(wxT("-DOGRE_DEPENDENCIES_DIR=") + tree.dependencies);
        args.Add(wxT("-DOGRE_BUILD_SAMPLES2=OFF"));
        args.Add(wxT("-DOGRE_BUILD_TESTS=OFF"));
    }

    return args;
}

wxString OgreNextConfigureDescription(const OgreNextTree& tree)
{
    wxString text =
        tree.engine
            ? _("CMake is configuring OGRE-Next.\n\n")
            : _("CMake is configuring the OGRE-Next dependencies.\n\n");

    text +=
        _("Source:\n")
        + tree.source
        + _("\n\nBuild:\n")
        + tree.build
        + _("\n\nInstall:\n")
        + tree.install;

    if (tree.engine)
    {
        text +=
            _("\n\nDependencies:\n")
            + tree.dependencies
            + _("\n\nSamples and tests are off. "
                "The install prefix is fixed now.");
    }
    else
    {
        text +=
            _("\n\nThe dependency project selects this install "
              "directory itself. Code::Blocks does not pass "
              "CMAKE_INSTALL_PREFIX.");
    }

    text += _("\n\nThere is no progress percentage.");

    return text;
}
```


### The run function

In `wincmake.h`,  inside `#if defined(__WXMSW__)`, after the declaration of `InstallWinOgre`, paste:

```cpp
WinCMakeRunResult RunWinCMakeOgreNext(
    wxWindow* parent,
    const wxString& compilerId,
    const wxString& projectId,
    const wxString& operation,
    const wxString& explanation,
    const wxString& confirmLabel);
```

`compilerId` is the wizard's selected compiler.

Paste this at the bottom of `wincmake.cpp`, after `FormatWinCMakeRun`, still above `#endif // __WXMSW__`. It stays outside the anonymous namespace, because `wiz.cpp` calls it. It follows the example of cmake and can do configure, build and install.

```cpp
WinCMakeRunResult RunWinCMakeOgreNext(
    wxWindow* parent,
    const wxString& compilerId,
    const wxString& projectId,
    const wxString& operation,
    const wxString& explanation,
    const wxString& confirmLabel)
{
    WinCMakeRunResult result;

    const bool configure = operation == wxT("configure");
    const bool build = operation == wxT("build");
    const bool install = operation == wxT("install");

    if (!configure && !build && !install)
    {
        result.output = wxT("unknown operation");
        return result;
    }

    result.operation = operation;

    OgreNextTree tree;
    wxString error;

    if (!ResolveOgreNextTree(projectId, tree, error))
    {
        result.output = error;
        return result;
    }

    if (!wxDirExists(tree.source))
    {
        result.output = wxT("source checkout is not present");
        return result;
    }

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

    const OgreNextCacheState cache = ReadOgreNextCache(tree, error);

    // Reconfiguring cannot repair a dependency prefix. The project
    // keeps a prefix that is already in the cache, and passing a new
    // one would skip the project's own ogredeps default.
    if (!tree.engine && cache == OgreNextCachePrefixWrong)
    {
        result.output =
            wxT("CMAKE_INSTALL_PREFIX is not the dependency SDK; "
                "the build directory was left unchanged");

        return result;
    }

    wxString filesError;
    const bool filesOk = OgreNextFilesPresent(tree, filesError);

    if (configure && cache == OgreNextCacheMatches)
    {
        result.success = true;
        result.output = wxT("configure already matches");
        return result;
    }

    if ((build || install)
        && cache == OgreNextCacheMatches
        && filesOk)
    {
        result.success = true;
        result.output =
            install
                ? wxT("install already valid")
                : wxT("build already installed");

        return result;
    }

    if ((build || install) && cache != OgreNextCacheMatches)
    {
        result.output =
            error.empty()
                ? wxString(wxT("configure does not match the pinned options"))
                : error;

        return result;
    }

    if (tree.engine && (configure || build))
    {
        OgreNextTree dependencies;

        dependencies.engine = false;
        dependencies.install = tree.dependencies;

        wxString dependencyError;

        if (!OgreNextFilesPresent(dependencies, dependencyError))
        {
            result.output = wxT("dependency SDK is not installed");
            return result;
        }
    }

    if (!ConfirmAction(parent, explanation, confirmLabel))
    {
        result.cancelled = true;
        result.output = wxT("cancelled before launch");
        return result;
    }

    if (configure)
    {
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
    }

    if (build)
    {
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
    }

    result.operation = wxT("install");
    result.output = wxT("install comes in a later step");
    return result;
}
```

Quality check: Search this function for `Mac`, `sdk`, and `xcrun`. Those words should be gone.

## 8. Squirrel binding for `RunWinCMakeOgreNext`

Open `wiz.cpp`. Find `Wiz_WinCMakeInstall`. Paste this immediately after its closing `}`.

```cpp
SQInteger Wiz_WinCMakeOgreNext(HSQUIRRELVM v)
{
    // this, projectId, operation, explanation, confirmLabel
    // projectId is ogre-next-deps or ogre-next.
    // operation is configure, build, or install.
    // Paths and -D flags are not script arguments.
    // The compiler id is read here, not passed by Squirrel.
    ExtractParams5<
        Wiz*,
        const wxString *,
        const wxString *,
        const wxString *,
        const wxString *
    > extractor(v);

    if (!extractor.Process("Wiz::WinCMakeOgreNext"))
        return extractor.ErrorMessage();

    const WinCMakeRunResult result =
        RunWinCMakeOgreNext(
            extractor.p0->GetWizardWindow(),
            extractor.p0->GetCompilerID(),
            *extractor.p1,
            *extractor.p2,
            *extractor.p3,
            *extractor.p4);

    return ConstructAndReturnInstance(
        v,
        FormatWinCMakeRun(result));
}
```


After the `WinOgreNextAcquire` binding from section 4, add:

```cpp
BindMethod(v, _SC("WinCMakeOgreNext"),
           Wiz_WinCMakeOgreNext,
           _SC("Wiz::WinCMakeOgreNext"));
```

Check whether everything is fine, using the steps in section 2: the IDE starts, and the wizard still ends at the patch pages. Nothing new yet, because `Wizard.WinCMakeOgreNext` exists, but no page calls it. But definitely, if there is an error, it won't compile.


## 9. Build pages

Back to the wizard (`src\plugins\scriptedwizard\resources\ogre-next\wizard.script`). Paste these above `BeginWinOgreNext`. `OgreNextWinBuildTree` configures and builds. It does not install. Section 11 adds that page.

```javascript
function OgreNextWinCMakeBlock()
{
    local report = Wizard.WinCMakeProbe();
    OgreNextNativeCMakeAccess = OgreNextField(report, _T("access"));
    OgreNextNativeCMakeExecutable = OgreNextField(report, _T("executable"));
    OgreNextNativeCMakeVersion = OgreNextField(report, _T("version"));
    if (OgreNextNativeCMakeAccess.Matches(_T("works")))
        return OgreNextWinResolvePaths();
    if (OgreNextNativeCMakeAccess.Matches(_T("present_unusable")))
        return _("CMake was found but failed cmake --version.");
    return _("CMake could not be found by the native CMake helper.");
}

function OgreNextWinRun(projectId, operation, explanation, confirmLabel)
{
    local report = Wizard.WinCMakeOgreNext(projectId, operation, explanation, confirmLabel);
    if (OgreNextField(report, _T("success")).Matches(_T("1")))
        return true;
    if (OgreNextField(report, _T("cancelled")).Matches(_T("1")))
    {
        ShowMessage(_("CMake was stopped. Files already written are left in place."));
        return false;
    }
    ShowError(_("OGRE-Next ") + operation + _(" did not succeed.\n\n") + OgreNextField(report, _T("output")));
    return false;
}

function OgreNextWinBuildTree(projectId, title)
{
    local configure = _("Code::Blocks will configure ") + title + _(".\n\nThe options are fixed by Code::Blocks. This page does not accept extra CMake arguments.");
    if (projectId.Matches(_T("ogre-next")))
    {
        configure += _("\n\nInstall prefix:\n") + OgreNextInstall;
        configure += _("\n\nDependencies:\n") + OgreNextDepsInstall;
        configure += _("\n\nSamples and tests stay off. The prefix is fixed before the build.");
    }
    else
    {
        configure += _("\n\nThe dependency project selects its own ogredeps directory. Code::Blocks does not pass CMAKE_INSTALL_PREFIX.");
    }
    if (!OgreNextWinRun(projectId, _T("configure"), configure, _("Configure ") + title))
        return false;

    local build = _("Code::Blocks will build ") + title + _(".\n\nThis step does not install it.");
    return OgreNextWinRun(projectId, _T("build"), build, _("Build ") + title);
}

function OnLeave_OgreNextWinBuildDeps(fwd)
{
    if (!fwd)
        return true;
    return OgreNextWinBuildTree(_T("ogre-next-deps"), _("the OGRE-Next dependencies"));
}

function OnLeave_OgreNextWinBuildEngine(fwd)
{
    if (!fwd)
        return true;
    return OgreNextWinBuildTree(_T("ogre-next"), _("OGRE-Next"));
}
```

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

The file arrays from section 7 are what `OgreNextFilesPresent` checks. No new binding. `Wizard.WinCMakeOgreNext(..., "install", ...)` now does this branch.

Look, using the steps in section 2: the wizard is unchanged, and it still ends at the build pages.


## 11. Install pages

Paste these above `BeginWinOgreNext`. The Mac lists stay in `OgreNextDepsInstalled` and `OgreNextEngineInstalled`.

```javascript
function OgreNextWinDepsInstalled()
{
    if (!OgreNextPathsValid)
        return false;
    local root = OgreNextDepsInstall;
    return IO.FileExists(OgreNextJoin(root, _T("include/FreeImage.h")))
        && IO.FileExists(OgreNextJoin(root, _T("include/SDL2/SDL.h")))
        && IO.FileExists(OgreNextJoin(root, _T("bin/Release/SDL2.dll")));
}

function OgreNextWinEngineInstalled()
{
    if (!OgreNextPathsValid)
        return false;
    local root = OgreNextInstall;
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
}

function OgreNextWinInstall(projectId, title)
{
    local install = _("Code::Blocks will install ") + title + _(" with cmake --install.\n\nNo replacement prefix is passed. The prefix from configuration is used.");
    return OgreNextWinRun(projectId, _T("install"), install, _("Install ") + title);
}

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

Replace `OnLeave_OgreNextWinBuildDeps` and `OnLeave_OgreNextWinBuildEngine` with these. A tree that is already installed is skipped, and the engine refuses to build when the dependency SDK is missing.

```javascript
function OnLeave_OgreNextWinBuildDeps(fwd)
{
    if (!fwd)
        return true;
    if (OgreNextWinDepsInstalled())
        return true;
    return OgreNextWinBuildTree(_T("ogre-next-deps"), _("the OGRE-Next dependencies"));
}

function OnLeave_OgreNextWinBuildEngine(fwd)
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
    return OgreNextWinBuildTree(_T("ogre-next"), _("OGRE-Next"));
}
```

In `BeginWinOgreNext`, replace the dependency build page so it is skipped once the SDK files exist, and add the install page after it:

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

Replace the engine build page the same way:

```javascript
if (!OgreNextWinEngineInstalled())
{
    Wizard.AddInfoPage(
        _T("OgreNextWinBuildEngine"),
        _("OGRE-Next is not built yet.\n\nPress Next to configure and build it. This page does not install."));
    Wizard.AddInfoPage(
        _T("OgreNextWinInstallEngine"),
        _("OGRE-Next is not installed yet.\n\nDestination:\n    ") + OgreNextInstall + _("\n\nPress Next to install it."));
}
```

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

In `GetGeneratedFile`, case 1 currently sets `templateFile` to `ogre-next/files/plugins.cfg`. Replace that case with:

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

Open `src/plugins/scriptedwizard/resources/ogre/wizard.script` and find `OgreWinQueueDll`. It adds one `cmd /c copy /Y` after the link, into `$(TARGET_OUTPUT_DIR)`. Do not edit that file. Paste this copy into the OGRE-Next script, above `BeginWinOgreNext`:

```javascript
function OgreNextWinQueueDll(project, dir, name)
{
    if (dir.IsEmpty())
        return;

    local path = dir + wxFILE_SEP_PATH + name;

    if (!IO.FileExists(path))
        return;

    project.AddCommandsAfterBuild(_T("cmd /c copy /Y \"") + path + _T("\" \"$(TARGET_OUTPUT_DIR)") + name + _T("\""));
}
```

Paste `OgreNextWinAllReady` above `BeginWinOgreNext`. Compare it with `OgreNextAllReady`. The Homebrew block is gone, and so are the two Apple patch ids.

```javascript
function OgreNextWinAllReady()
{
    local reason = OgreNextWinCMakeBlock();
    if (!reason.IsEmpty())
    {
        ShowError(reason);
        return false;
    }
    reason = OgreNextWinGitBlock();
    if (!reason.IsEmpty())
    {
        ShowError(reason);
        return false;
    }
    if (!OgreNextWinCheckoutReady(_T("ogre-next-deps"), OgreNextDepsSource))
    {
        ShowError(_("The pinned dependency checkout is not present."));
        return false;
    }
    if (!OgreNextWinPatchState(_T("ogre-next-deps-mingw"), OgreNextDepsSource).Matches(_T("already_applied")))
    {
        ShowError(_("The dependency patches are not applied."));
        return false;
    }
    if (!OgreNextWinDepsInstalled())
    {
        ShowError(_("The dependency SDK is not installed."));
        return false;
    }
    if (!OgreNextWinCheckoutReady(_T("ogre-next"), OgreNextEngineSource))
    {
        ShowError(_("The pinned OGRE-Next checkout is not present."));
        return false;
    }
    if (!OgreNextWinPatchState(_T("ogre-next-v3-0-mingw"), OgreNextEngineSource).Matches(_T("already_applied")))
    {
        ShowError(_("The engine patches are not applied."));
        return false;
    }
    if (!OgreNextWinEngineInstalled())
    {
        ShowError(_("The OGRE-Next SDK is not installed."));
        return false;
    }
    return true;
}
```

`OnLeave_ProjectPathPage` and `OnLeave_CompilerPage` currently call `OgreNextAllReady()`. Replace each function with:

```javascript
function OnLeave_ProjectPathPage(fwd)
{
    if (PLATFORM == PLATFORM_MSW)
        return !fwd || OgreNextWinAllReady();
    return !fwd || OgreNextAllReady();
}

function OnLeave_CompilerPage(fwd)
{
    if (PLATFORM == PLATFORM_MSW)
        return !fwd || OgreNextWinAllReady();
    return !fwd || OgreNextAllReady();
}
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

Open `SetupProject` and read the linker block. It is `-F` and `-framework`, then a shell line that builds a Mac `.app` and a Frameworks symlink. The release target is `ttExecutable`, and both targets set an output filename inside that `.app`. None of that belongs on Windows. Paste this function above `BeginWinOgreNext`. The three link names are `libOgreHlmsPbs.dll.a`, `libOgreHlmsUnlit.dll.a`, and `libOgreMain.dll.a` in `lib/Release`. `AddLinkLib` on the project applies to both targets. Both targets are `ttConsoleOnly`, so the console window keeps `Light show rendered and closed cleanly.` visible. Without `SetOutputFilename`, Code::Blocks keeps `bin\Debug` and `bin\Release`.

```javascript
function SetupWinOgreNextProject(project)
{
    if (!OgreNextWinAllReady())
        return false;

    local includeRoot = OgreNextJoin(OgreNextInstall, _T("include/OGRE"));

    project.AddIncludeDir(includeRoot);
    project.AddIncludeDir(OgreNextJoin(includeRoot, _T("Hlms/Common")));
    project.AddIncludeDir(OgreNextJoin(includeRoot, _T("Hlms/Unlit")));
    project.AddIncludeDir(OgreNextJoin(includeRoot, _T("Hlms/Pbs")));
    project.AddCompilerOption(_T("-std=gnu++17"));

    project.AddLibDir(OgreNextJoin(OgreNextInstall, _T("lib/Release")));
    project.AddLinkLib(_T("OgreHlmsPbs"));
    project.AddLinkLib(_T("OgreHlmsUnlit"));
    project.AddLinkLib(_T("OgreMain"));

    WarningsOn(project, Wizard.GetCompilerID());
    CppExceptionsOn(project, Wizard.GetCompilerID());

    local projectDir = Wizard.GetProjectFullFilename().BeforeLast(_T("\\"));
    local target = project.GetBuildTarget(Wizard.GetDebugName());
    if (!IsNull(target))
    {
        target.SetTargetType(ttConsoleOnly);
        target.SetWorkingDir(projectDir);
        DebugSymbolsOn(target, Wizard.GetCompilerID());
    }

    target = project.GetBuildTarget(Wizard.GetReleaseName());
    if (!IsNull(target))
    {
        target.SetTargetType(ttConsoleOnly);
        target.SetWorkingDir(projectDir);
        target.AddCompilerOption(_T("-DNDEBUG"));
        OptimizationsOn(target, Wizard.GetCompilerID());
    }

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

    return true;
}
```

`bin` comes from `Wizard.WinOgreTools`. The three MinGW DLLs then belong to the compiler selected in the wizard. `ogrehead.mesh` is copied into the project folder because `main.cpp` opens it there.

Look, using the script copy in section 2: the wizard now ends with the project-path page and the compiler page. Finish creates the project. Build it. Running it opens a window titled `OGRE-Next Light Show`, shows the rotating Ogre head, and after the window is closed prints `Light show rendered and closed cleanly.`

If a trial edit of this repository should be thrown away before any of it is kept:

```bat
git restore .
```
