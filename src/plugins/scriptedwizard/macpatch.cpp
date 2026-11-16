/*
 * This file is part of the Code::Blocks IDE and licensed under the GNU General Public License, version 3
 * http://www.gnu.org/licenses/gpl-3.0.html
 *
 * v3.8.1 M5 and v3.9 M2: probe and apply one known compatibility
 * patch to an explicitly supported source checkout using Git.
 *
 * This is deliberately not a general patch or Git command API.
 * The helper accepts only known patch identifiers and never accepts
 * arbitrary patch paths, patch contents, target files or Git commands
 * from wizard.script.
 */

#include <sdk.h>

#ifndef CB_PRECOMP
    #include <configmanager.h>
    #include <manager.h>
#endif

#include <wx/button.h>
#include <wx/dialog.h>
#include <wx/filename.h>
#include <wx/process.h>
#include <wx/sizer.h>
#include <wx/stattext.h>
#include <wx/utils.h>
#include <wx/window.h>

#include "macpatch.h"

#if defined(__WXMAC__)
    #include "macgit.h"
#elif defined(__WXMSW__)
    #include "wingit.h"
#endif

#if defined(__WXMAC__) || defined(__WXMSW__)

namespace
{

enum MacPatchRevisionKind
{
    MacPatchRevisionExactTag = 0,
    MacPatchRevisionExactCommit = 1
};

struct KnownPatch
{
    const wxChar* id;
    const wxChar* file;
    const wxChar* sentinel;
    int revisionKind;
    const wxChar* revision;
};

const KnownPatch kKnownPatches[] =
{
    {
        wxT("ogre-14.6.3-assimp-sidecar"),
        wxT("ogre-14.6.3-assimp-sidecar-path.patch"),
        wxT("PlugIns/Assimp/src/AssimpLoader.cpp"),
        MacPatchRevisionExactTag,
        wxT("v14.6.3")
    },
    {
        wxT("ogre-next-deps-mingw"),
        wxT("ogre-next-deps-mingw.patch"),
        wxT("src/zziplib/zzip/plugin.c"),
        MacPatchRevisionExactCommit,
        wxT("f7e23191f4fd89ed41134996bc47f9ee23d009df")
    },
    {
        wxT("ogre-next-deps-apple"),
        wxT("ogre-next-deps-apple.patch"),
        wxT("src/Remotery/CMakeLists.txt"),
        MacPatchRevisionExactCommit,
        wxT("f7e23191f4fd89ed41134996bc47f9ee23d009df")
    },
    {
        wxT("ogre-next-v3-0-mingw"),
        wxT("ogre-next-v3-0-mingw.patch"),
        wxT("RenderSystems/Direct3D11/src/OgreD3D11Device.cpp"),
        MacPatchRevisionExactCommit,
        wxT("4859016ad2cc956a653dfc4e4a859505696c6509")
    },
    {
        wxT("ogre-apple"),
        wxT("ogre-apple.patch"),
        wxT("RenderSystems/Metal/src/OgreMetalWindow.mm"),
        MacPatchRevisionExactCommit,
        wxT("4859016ad2cc956a653dfc4e4a859505696c6509")
    }
};

const KnownPatch* FindKnownPatch(const wxString& patchId)
{
    for (size_t i = 0; i < WXSIZEOF(kKnownPatches); ++i)
    {
        if (patchId == kKnownPatches[i].id)
            return &kKnownPatches[i];
    }

    return nullptr;
}

wxString ReadAll(wxInputStream* in)
{
    wxString out;

    if (!in)
        return out;

    char buf[4096];

    while (!in->Eof())
    {
        in->Read(buf, sizeof(buf));
        const size_t n = in->LastRead();

        if (n == 0)
            break;

        const wxString chunk = wxString::FromUTF8(buf, n);

        if (chunk.empty())
            out += wxString(buf, wxConvISO8859_1, n);
        else
            out += chunk;
    }

    return out;
}

wxString FirstLine(const wxString& text)
{
    wxString line = text.BeforeFirst(wxT('\n'));
    line.Trim(true).Trim(false);

    if (line.EndsWith(wxT("\r")))
        line.RemoveLast();

    return line;
}

bool HasControlCharacters(const wxString& text)
{
    for (size_t i = 0; i < text.Length(); ++i)
    {
        const wxChar c = text[i];

        if (c < wxT(' ') || c == wxT('\x7f'))
            return true;
    }

    return false;
}

bool SourceRootOk(const wxString& sourceRoot,
                 const wxString& sentinel,
                 wxString& normalised,
                 wxString& error)
{
    if (sourceRoot.empty())
    {
        error = wxT("source root is empty");
        return false;
    }

    if (HasControlCharacters(sourceRoot))
    {
        error = wxT("source root contains a control character");
        return false;
    }

    wxFileName sourceName = wxFileName::DirName(sourceRoot);

    if (!sourceName.IsAbsolute())
    {
        error = wxT("source root is not absolute");
        return false;
    }

    sourceName.Normalize(wxPATH_NORM_DOTS | wxPATH_NORM_ABSOLUTE);
    normalised = sourceName.GetFullPath();

    if (normalised.empty() || normalised == wxT("/"))
    {
        error = wxT("source root is unsafe");
        return false;
    }

    if (!wxDirExists(normalised))
    {
        error = wxT("source root does not exist");
        return false;
    }

    const wxFileName gitMarker(normalised, wxT(".git"));

    if (!wxDirExists(gitMarker.GetFullPath())
        && !wxFileExists(gitMarker.GetFullPath()))
    {
        error = wxT("source root is not a Git checkout");
        return false;
    }

    wxFileName target(
        normalised + wxFILE_SEP_PATH + sentinel);

    target.Normalize(
        wxPATH_NORM_DOTS | wxPATH_NORM_ABSOLUTE);

    if (!target.FileExists())
    {
        error =
            wxT("expected patch target does not exist: ")
            + target.GetFullPath();

        return false;
    }

    return true;
}

bool ResolveKnownPatch(const wxString& patchId,
                       const KnownPatch*& patch,
                       wxString& patchFile,
                       wxString& error)
{
    patch = FindKnownPatch(patchId);

    if (!patch)
    {
        error = wxT("unknown patch identifier");
        return false;
    }

    const wxString dataRoot =
        ConfigManager::GetFolder(sdDataGlobal);

    wxFileName patchName = wxFileName::DirName(dataRoot);
    patchName.AppendDir(wxT("templates"));
    patchName.AppendDir(wxT("wizard"));
    patchName.AppendDir(wxT("patches"));
    patchName.SetFullName(patch->file);

    patchFile = patchName.GetFullPath();

    if (!wxFileExists(patchFile))
    {
        error =
            wxT("known patch resource was not found: ")
            + patchFile;

        return false;
    }

    return true;
}

bool RunGit(const wxString& executable,
            const wxString& sourceRoot,
            const wxArrayString& arguments,
            long& exitCode,
            wxString& output,
            wxString& error)
{
    wxProcess process(wxPROCESS_REDIRECT);

    wxExecuteEnv env;

    if (!wxGetEnvMap(&env.env) || env.env.empty())
    {
        exitCode = -1;
        error = wxT("could not copy the environment");
        return false;
    }

    env.env[wxT("GIT_TERMINAL_PROMPT")] = wxT("0");

    wxArrayString argvStrings;
    argvStrings.Add(executable);
    argvStrings.Add(wxT("-C"));
    argvStrings.Add(sourceRoot);

    for (size_t i = 0; i < arguments.GetCount(); ++i)
        argvStrings.Add(arguments[i]);

    // wxWidgets 3.0 declares wxExecute(wchar_t**). The strings are not modified.
    std::vector<wxChar*> argv;
    argv.reserve(argvStrings.GetCount() + 1);

    for (size_t i = 0; i < argvStrings.GetCount(); ++i)
        argv.push_back(const_cast<wxChar*>(argvStrings[i].wx_str()));

    argv.push_back(nullptr);

    exitCode =
        wxExecute(
            argv.data(),
            wxEXEC_SYNC,
            &process,
            &env);

    output = ReadAll(process.GetInputStream());
    error = ReadAll(process.GetErrorStream());

    return exitCode != -1;
}

bool ValidateRevision(const wxString& executable,
                     const wxString& sourceRoot,
                     const KnownPatch& patch,
                     wxString& versionOutput,
                     wxString& errorOutput)
{
    wxArrayString args;

    if (patch.revisionKind == MacPatchRevisionExactTag)
    {
        args.Add(wxT("describe"));
        args.Add(wxT("--tags"));
        args.Add(wxT("--exact-match"));
        args.Add(wxT("HEAD"));
    }
    else
    {
        args.Add(wxT("rev-parse"));
        args.Add(wxT("HEAD"));
    }

    long exitCode = -1;
    wxString output;
    wxString error;

    if (!RunGit(
            executable,
            sourceRoot,
            args,
            exitCode,
            output,
            error))
    {
        errorOutput = error;
        return false;
    }

    versionOutput = FirstLine(output);
    errorOutput = error;

    return exitCode == 0
        && versionOutput == patch.revision;
}

bool RunPatchCheck(const wxString& executable,
                   const wxString& sourceRoot,
                   const wxString& patchFile,
                   bool reverse,
                   long& exitCode,
                   wxString& output,
                   wxString& error)
{
    wxArrayString args;
    args.Add(wxT("apply"));

    if (reverse)
        args.Add(wxT("--reverse"));

    args.Add(wxT("--check"));
    args.Add(patchFile);

    return RunGit(
        executable,
        sourceRoot,
        args,
        exitCode,
        output,
        error);
}

bool RunPatchApply(const wxString& executable,
                   const wxString& sourceRoot,
                   const wxString& patchFile,
                   long& exitCode,
                   wxString& output,
                   wxString& error)
{
    wxArrayString args;
    args.Add(wxT("apply"));
    args.Add(patchFile);

    return RunGit(
        executable,
        sourceRoot,
        args,
        exitCode,
        output,
        error);
}

bool ConfirmPatch(const wxString& explanation,
                  const wxString& confirmLabel)
{
    wxWindow* parent = nullptr;

    wxWindow* focused = wxWindow::FindFocus();

    if (focused)
        parent = wxGetTopLevelParent(focused);

    if (!parent)
        parent = Manager::Get()->GetAppWindow();

    wxDialog dlg(
        parent,
        wxID_ANY,
        _("Compatibility patch"),
        wxDefaultPosition,
        wxDefaultSize,
        wxDEFAULT_DIALOG_STYLE | wxRESIZE_BORDER);

    wxBoxSizer* outer = new wxBoxSizer(wxVERTICAL);

    wxStaticText* text =
        new wxStaticText(
            &dlg,
            wxID_ANY,
            explanation);

    text->Wrap(500);
    outer->Add(text, 1, wxALL | wxEXPAND, 12);

    wxBoxSizer* buttons =
        new wxBoxSizer(wxHORIZONTAL);

    buttons->AddStretchSpacer();

    buttons->Add(
        new wxButton(
            &dlg,
            wxID_OK,
            confirmLabel),
        0,
        wxRIGHT,
        8);

    buttons->Add(
        new wxButton(
            &dlg,
            wxID_CANCEL,
            _("Cancel")),
        0);

    outer->Add(
        buttons,
        0,
        wxLEFT | wxRIGHT | wxBOTTOM | wxEXPAND,
        12);

    dlg.SetSizer(outer);
    dlg.SetInitialSize(wxSize(580, -1));
    dlg.CentreOnParent();

    const bool approved =
        dlg.ShowModal() == wxID_OK;

    if (parent)
    {
        parent->Raise();
        parent->SetFocus();
    }

    return approved;
}

wxString StateName(int state)
{
    switch (state)
    {
        case MacPatchApplicable:
            return wxT("applicable");

        case MacPatchAlreadyApplied:
            return wxT("already_applied");

        case MacPatchUnexpectedSource:
            return wxT("unexpected_source");

        case MacPatchUnsupportedVersion:
            return wxT("unsupported_version");

        case MacPatchGitUnavailable:
            return wxT("git_unavailable");

        default:
            return wxT("unknown");
    }
}

} // namespace

MacPatchProbe ProbeMacPatch(
    const wxString& patchId,
    const wxString& sourceRoot)
{
    MacPatchProbe result;

    result.patchId = patchId;
    result.sourceRoot = sourceRoot;

    wxString patchFile;
    wxString sourceError;
    const KnownPatch* patch = nullptr;

    if (!ResolveKnownPatch(
            patchId,
            patch,
            patchFile,
            sourceError))
    {
        result.state = MacPatchUnknown;
        result.errorOutput = sourceError;
        return result;
    }

    result.targetFile = patch->sentinel;

    wxString normalisedSource;

    if (!SourceRootOk(
            sourceRoot,
            patch->sentinel,
            normalisedSource,
            sourceError))
    {
        result.state = MacPatchUnexpectedSource;
        result.errorOutput = sourceError;
        return result;
    }

    result.sourceRoot = normalisedSource;

#if defined(__WXMAC__)
    const MacGitProbe gitProbe = ProbeMacGit();
    const int gitWorks = MacGitWorks;
#elif defined(__WXMSW__)
    const WinGitProbe gitProbe = ProbeWinGit();
    const int gitWorks = WinGitWorks;
#endif

    result.gitExecutable = gitProbe.executable;

    if (gitProbe.access != gitWorks)
    {
        result.state = MacPatchGitUnavailable;
        result.errorOutput =
            gitProbe.errorOutput.empty()
                ? wxT("git unavailable")
                : gitProbe.errorOutput;

        return result;
    }

    wxString versionError;

    if (!ValidateRevision(
            gitProbe.executable,
            normalisedSource,
            *patch,
            result.versionOutput,
            versionError))
    {
        result.state = MacPatchUnsupportedVersion;
        result.errorOutput =
            versionError.empty()
                ? wxString::Format(
                      patch->revisionKind == MacPatchRevisionExactTag
                          ? wxT("source checkout is not exact tag %s")
                          : wxT("source checkout is not commit %s"),
                      patch->revision)
                : versionError;

        return result;
    }

    wxString applyError;

    if (!RunPatchCheck(
            gitProbe.executable,
            normalisedSource,
            patchFile,
            false,
            result.applyCheckExit,
            result.applyCheckOutput,
            applyError))
    {
        result.state = MacPatchGitUnavailable;
        result.errorOutput = applyError;
        return result;
    }

    if (result.applyCheckExit == 0)
    {
        result.state = MacPatchApplicable;
        return result;
    }

    wxString reverseError;

    if (!RunPatchCheck(
            gitProbe.executable,
            normalisedSource,
            patchFile,
            true,
            result.reverseCheckExit,
            result.reverseCheckOutput,
            reverseError))
    {
        result.state = MacPatchGitUnavailable;
        result.errorOutput = reverseError;
        return result;
    }

    if (result.reverseCheckExit == 0)
    {
        result.state = MacPatchAlreadyApplied;
        return result;
    }

    result.state = MacPatchUnexpectedSource;

    result.errorOutput =
        FirstLine(applyError);

    if (result.errorOutput.empty())
        result.errorOutput = FirstLine(reverseError);

    if (result.errorOutput.empty())
        result.errorOutput =
            wxT("patch is neither applicable nor already applied");

    return result;
}

wxString FormatMacPatchProbe(
    const MacPatchProbe& probe)
{
    return wxString::Format(
        wxT("state=%s\n"
            "patch_id=%s\n"
            "source_root=%s\n"
            "target_file=%s\n"
            "git=%s\n"
            "version=%s\n"
            "apply_check_exit=%ld\n"
            "reverse_check_exit=%ld\n"
            "apply_output=%s\n"
            "reverse_output=%s\n"
            "error=%s"),
        StateName(probe.state),
        probe.patchId,
        probe.sourceRoot,
        probe.targetFile,
        probe.gitExecutable,
        FirstLine(probe.versionOutput),
        probe.applyCheckExit,
        probe.reverseCheckExit,
        FirstLine(probe.applyCheckOutput),
        FirstLine(probe.reverseCheckOutput),
        FirstLine(probe.errorOutput));
}

MacPatchApplyResult ApplyMacPatch(
    const wxString& patchId,
    const wxString& sourceRoot,
    const wxString& explanation,
    const wxString& confirmLabel)
{
    MacPatchApplyResult result;

    result.patchId = patchId;
    result.sourceRoot = sourceRoot;

    const MacPatchProbe initial =
        ProbeMacPatch(
            patchId,
            sourceRoot);

    result.initialState = initial.state;
    result.finalState = initial.state;
    result.sourceRoot = initial.sourceRoot;
    result.targetFile = initial.targetFile;
    result.gitExecutable = initial.gitExecutable;

    if (initial.state == MacPatchAlreadyApplied)
    {
        result.success = true;
        result.output = wxT("patch already applied");
        return result;
    }

    if (initial.state != MacPatchApplicable)
    {
        result.errorOutput = initial.errorOutput;

        if (result.errorOutput.empty())
            result.errorOutput =
                wxT("patch is not applicable");

        return result;
    }

    if (!ConfirmPatch(
            explanation,
            confirmLabel))
    {
        result.cancelled = true;
        result.output =
            wxT("patch application cancelled before launch");

        return result;
    }

    wxString patchFile;
    wxString patchError;
    const KnownPatch* patch = nullptr;

    if (!ResolveKnownPatch(
            patchId,
            patch,
            patchFile,
            patchError))
    {
        result.errorOutput = patchError;
        return result;
    }

    result.targetFile = patch->sentinel;

    result.launched = true;

    if (!RunPatchApply(
            result.gitExecutable,
            result.sourceRoot,
            patchFile,
            result.exitCode,
            result.output,
            result.errorOutput))
    {
        result.launched = false;
        return result;
    }

    if (result.exitCode != 0)
        return result;

    const MacPatchProbe finalProbe =
        ProbeMacPatch(
            patchId,
            result.sourceRoot);

    result.finalState = finalProbe.state;

    if (finalProbe.state != MacPatchAlreadyApplied)
    {
        result.errorOutput =
            finalProbe.errorOutput.empty()
                ? wxT("post-apply verification failed")
                : finalProbe.errorOutput;

        return result;
    }

    result.success = true;

    if (result.output.empty())
        result.output =
            wxT("patch applied");

    return result;
}

wxString FormatMacPatchApply(
    const MacPatchApplyResult& result)
{
    return wxString::Format(
        wxT("launched=%d\n"
            "cancelled=%d\n"
            "exit=%ld\n"
            "success=%d\n"
            "initial_state=%s\n"
            "final_state=%s\n"
            "patch_id=%s\n"
            "source_root=%s\n"
            "target_file=%s\n"
            "git=%s\n"
            "output=%s\n"
            "error=%s"),
        result.launched ? 1 : 0,
        result.cancelled ? 1 : 0,
        result.exitCode,
        result.success ? 1 : 0,
        StateName(result.initialState),
        StateName(result.finalState),
        result.patchId,
        result.sourceRoot,
        result.targetFile,
        result.gitExecutable,
        FirstLine(result.output),
        FirstLine(result.errorOutput));
}

#endif // defined(__WXMAC__) || defined(__WXMSW__)
