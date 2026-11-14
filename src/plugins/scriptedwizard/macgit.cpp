/*
 * This file is part of the Code::Blocks IDE and licensed under the GNU General Public License, version 3
 * http://www.gnu.org/licenses/gpl-3.0.html
 *
 * v3.8 M2: locate Git, revalidate one executable, and perform one
 * explicitly approved shallow tagged clone.
 *
 * v3.9 M2: check out one of two compiled-in OGRE-Next source ids.
 *
 * This is deliberately not a general Git command API.
 * The helper does not install Git.
 */

#include <sdk.h>

#ifndef CB_PRECOMP
    #include <configmanager.h>
    #include <manager.h>
#endif

#include <vector>

#include <wx/arrstr.h>
#include <wx/button.h>
#include <wx/dialog.h>
#include <wx/dir.h>
#include <wx/filename.h>
#include <wx/process.h>
#include <wx/sizer.h>
#include <wx/stattext.h>
#include <wx/textctrl.h>
#include <wx/timer.h>
#include <wx/utils.h>

#include "macgit.h"

#if defined(__WXMAC__)

namespace
{

const wxString kSavedExecutableKey = wxT("/macos/git/executable");
const wxString kSystemGit = wxT("/usr/bin/git");
const wxString kOptGit = wxT("/opt/homebrew/bin/git");
const wxString kUsrLocalGit = wxT("/usr/local/bin/git");

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

wxString LastLine(const wxString& text)
{
    wxString trimmed = text;
    trimmed.Trim(true).Trim(false);

    const int nl = trimmed.Find(wxT('\n'), true);

    if (nl == wxNOT_FOUND)
    {
        if (trimmed.Length() > 400)
            return trimmed.Mid(trimmed.Length() - 400);

        return trimmed;
    }

    wxString line = trimmed.Mid(nl + 1);
    line.Trim(true).Trim(false);

    if (line.Length() > 400)
        line = line.Mid(line.Length() - 400);

    return line;
}

bool RunVersion(const wxString& executable,
                long& exitCode,
                wxString& output,
                wxString& error)
{
    wxProcess process(wxPROCESS_REDIRECT);

    // wxWidgets 3.0 declares wxExecute(wchar_t**). The strings are not modified.
    wxChar* argv[3];
    argv[0] = const_cast<wxChar*>(executable.wx_str());
    argv[1] = const_cast<wxChar*>(wxT("--version"));
    argv[2] = nullptr;

    exitCode = wxExecute(argv, wxEXEC_SYNC, &process);
    output = ReadAll(process.GetInputStream());
    error = ReadAll(process.GetErrorStream());

    return exitCode != -1;
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

bool TagTokenOk(const wxString& tag)
{
    if (tag.empty())
        return false;

    for (size_t i = 0; i < tag.Length(); ++i)
    {
        const wxChar c = tag[i];

        const bool ok =
               (c >= wxT('a') && c <= wxT('z'))
            || (c >= wxT('A') && c <= wxT('Z'))
            || (c >= wxT('0') && c <= wxT('9'))
            || c == wxT('.')
            || c == wxT('_')
            || c == wxT('-')
            || c == wxT('+')
            || c == wxT('/');

        if (!ok)
            return false;
    }

    return true;
}

bool UrlOk(const wxString& url)
{
    if (url.empty() || HasControlCharacters(url))
        return false;

    // M2 only needs an authenticated transport decision, not arbitrary
    // local paths, ssh commands, or shell-style Git URL syntax.
    return url.StartsWith(wxT("https://"));
}

bool DirectoryIsEmpty(const wxString& path)
{
    wxDir dir(path);

    if (!dir.IsOpened())
        return false;

    wxString entry;

    return !dir.GetFirst(
        &entry,
        wxEmptyString,
        wxDIR_FILES | wxDIR_DIRS | wxDIR_HIDDEN);
}

bool DestinationOk(const wxString& destination, wxString& error)
{
    if (destination.empty())
    {
        error = wxT("destination is empty");
        return false;
    }

    if (HasControlCharacters(destination))
    {
        error = wxT("destination contains a control character");
        return false;
    }

    wxFileName destinationName = wxFileName::DirName(destination);

    if (!destinationName.IsAbsolute())
    {
        error = wxT("destination is not absolute");
        return false;
    }

    destinationName.Normalize(wxPATH_NORM_DOTS | wxPATH_NORM_ABSOLUTE);

    const wxString normalised = destinationName.GetFullPath();

    if (normalised.empty() || normalised == wxT("/"))
    {
        error = wxT("destination is unsafe");
        return false;
    }

    if (wxFileExists(normalised))
    {
        error = wxT("destination is an existing file");
        return false;
    }

    if (wxDirExists(normalised) && !DirectoryIsEmpty(normalised))
    {
        error = wxT("destination directory is not empty");
        return false;
    }

    return true;
}

void ApplyVersion(MacGitProbe& probe)
{
    wxString versionError;

    if (!RunVersion(
            probe.executable,
            probe.versionExit,
            probe.versionOutput,
            versionError))
    {
        probe.access = MacGitPresentUnusable;
        probe.errorOutput = versionError;
        return;
    }

    probe.errorOutput = versionError;

    const wxString firstLine = FirstLine(probe.versionOutput);

    const bool versionOk =
           probe.versionExit == 0
        && firstLine.Lower().StartsWith(wxT("git version"));

    if (!versionOk)
    {
        probe.access = MacGitPresentUnusable;
        return;
    }

    probe.access = MacGitWorks;
}

bool ConfirmClone(wxWindow* parent,
                  const wxString& explanation,
                  const wxString& confirmLabel)
{
    wxDialog dlg(
        parent,
        wxID_ANY,
        _("Git"),
        wxDefaultPosition,
        wxDefaultSize,
        wxDEFAULT_DIALOG_STYLE | wxRESIZE_BORDER);

    wxBoxSizer* outer = new wxBoxSizer(wxVERTICAL);

    wxStaticText* text =
        new wxStaticText(&dlg, wxID_ANY, explanation);

    text->Wrap(440);
    outer->Add(text, 1, wxALL | wxEXPAND, 12);

    wxBoxSizer* buttons = new wxBoxSizer(wxHORIZONTAL);
    buttons->AddStretchSpacer();

    buttons->Add(
        new wxButton(&dlg, wxID_OK, confirmLabel),
        0,
        wxRIGHT,
        8);

    buttons->Add(
        new wxButton(&dlg, wxID_CANCEL, _("Cancel")),
        0);

    outer->Add(
        buttons,
        0,
        wxLEFT | wxRIGHT | wxBOTTOM | wxEXPAND,
        12);

    dlg.SetSizer(outer);
    dlg.SetInitialSize(wxSize(520, -1));
    dlg.CentreOnParent();

    return dlg.ShowModal() == wxID_OK;
}

class MacGitCloneDialog : public wxDialog
{
public:
    MacGitCloneDialog(wxWindow* parent,
                      const wxString& summary)
        : wxDialog(
              parent,
              wxID_ANY,
              _("Git"),
              wxDefaultPosition,
              wxDefaultSize,
              wxDEFAULT_DIALOG_STYLE | wxRESIZE_BORDER),
          m_text(nullptr),
          m_process(nullptr),
          m_step(0),
          m_pid(0),
          m_exit(-1),
          m_cancelled(false),
          m_finished(false),
          m_closed(false),
          m_timer(this)
    {
        wxBoxSizer* outer = new wxBoxSizer(wxVERTICAL);

        wxStaticText* label = new wxStaticText(
            this,
            wxID_ANY,
            summary);

        label->Wrap(560);
        outer->Add(label, 0, wxALL | wxEXPAND, 12);

        m_text = new wxTextCtrl(
            this,
            wxID_ANY,
            wxEmptyString,
            wxDefaultPosition,
            wxSize(620, 300),
            wxTE_MULTILINE | wxTE_READONLY);

        outer->Add(
            m_text,
            1,
            wxLEFT | wxRIGHT | wxEXPAND,
            12);

        outer->Add(
            new wxButton(this, wxID_CANCEL, _("Cancel")),
            0,
            wxALL | wxALIGN_RIGHT,
            12);

        SetSizer(outer);
        SetInitialSize(wxSize(660, 460));
        CentreOnParent();

        Bind(
            wxEVT_TIMER,
            &MacGitCloneDialog::OnTimer,
            this);

        Bind(
            wxEVT_END_PROCESS,
            &MacGitCloneDialog::OnProcessEnd,
            this);

        Bind(
            wxEVT_BUTTON,
            &MacGitCloneDialog::OnCancel,
            this,
            wxID_CANCEL);

        Bind(
            wxEVT_CLOSE_WINDOW,
            &MacGitCloneDialog::OnCloseWindow,
            this);
    }

    ~MacGitCloneDialog() override
    {
        m_timer.Stop();
        delete m_process;

        for (size_t i = 0; i < m_retired.size(); ++i)
            delete m_retired[i];
    }

    bool Start(const wxString& executable,
               const wxString& url,
               const wxString& tag,
               const wxString& destination)
    {
        wxArrayString command;
        command.Add(wxT("clone"));
        command.Add(wxT("--branch"));
        command.Add(tag);
        command.Add(wxT("--depth"));
        command.Add(wxT("1"));
        command.Add(url);
        command.Add(destination);

        std::vector<wxArrayString> commands;
        commands.push_back(command);

        return StartCommands(executable, commands);
    }

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

    bool Cancelled() const
    {
        return m_cancelled;
    }

    long ExitCode() const
    {
        return m_exit;
    }

    wxString OutputTail() const
    {
        return LastLine(m_all);
    }

private:
    void Append(const wxString& chunk)
    {
        m_all += chunk;

        if (m_text)
            m_text->AppendText(chunk);
    }

    void ReadStream(wxInputStream* in, bool available)
    {
        if (!in || !available)
            return;

        char buf[4096];
        in->Read(buf, sizeof(buf));

        const size_t n = in->LastRead();

        if (n == 0)
            return;

        const wxString chunk = wxString::FromUTF8(buf, n);

        if (chunk.empty())
            Append(wxString(buf, wxConvISO8859_1, n));
        else
            Append(chunk);
    }

    void ReadPending()
    {
        if (!m_process)
            return;

        ReadStream(
            m_process->GetInputStream(),
            m_process->IsInputAvailable());

        if (m_process)
        {
            ReadStream(
                m_process->GetErrorStream(),
                m_process->IsErrorAvailable());
        }
    }

    void StopChild()
    {
        if (m_pid <= 0)
            return;

        const long pid = m_pid;

        wxKill(
            pid,
            wxSIGTERM,
            NULL,
            wxKILL_NOCHILDREN);
    }

    void Finish()
    {
        if (m_closed)
            return;

        m_closed = true;
        m_timer.Stop();
        EndModal(wxID_OK);
    }

    void OnTimer(wxTimerEvent&)
    {
        ReadPending();
    }

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

    void OnCancel(wxCommandEvent&)
    {
        if (m_finished || m_pid <= 0)
        {
            Finish();
            return;
        }

        if (!m_cancelled)
        {
            m_cancelled = true;

            Append(
                wxT("\nCancel requested. Stopping the Git process "
                    "Code::Blocks started. This does not undo files "
                    "that process already wrote.\n"));

            StopChild();
        }
    }

    void OnCloseWindow(wxCloseEvent& event)
    {
        if (!m_finished && m_pid > 0)
        {
            if (!m_cancelled)
            {
                m_cancelled = true;

                Append(
                    wxT("\nCancel requested. Stopping the Git process "
                        "Code::Blocks started. This does not undo files "
                        "that process already wrote.\n"));

                StopChild();
            }

            event.Veto();
            return;
        }

        Finish();
    }

    wxTextCtrl* m_text;
    wxProcess* m_process;
    std::vector<wxProcess*> m_retired;
    std::vector<wxArrayString> m_commands;

    wxString m_executable;
    wxString m_all;

    size_t m_step;

    long m_pid;
    long m_exit;

    bool m_cancelled;
    bool m_finished;
    bool m_closed;

    wxTimer m_timer;
};

struct KnownGitSource
{
    const wxChar* id;
    const wxChar* url;
    const wxChar* commit;
    const wxChar* leaf;
    const wxChar* submodulePath;
    const wxChar* submoduleCommit;
};

const KnownGitSource kKnownSources[] =
{
    {
        wxT("ogre-next-deps"),
        wxT("https://github.com/OGRECave/ogre-next-deps.git"),
        wxT("f7e23191f4fd89ed41134996bc47f9ee23d009df"),
        wxT("ogre-next-deps-source-3.0.0"),
        wxT("src/SDL2"),
        wxT("5d249570393f7a37e037abf22cd6012a4cc56a71")
    },
    {
        wxT("ogre-next"),
        wxT("https://github.com/OGRECave/ogre-next.git"),
        wxT("4859016ad2cc956a653dfc4e4a859505696c6509"),
        wxT("ogre-next-source-3.0.0"),
        NULL,
        NULL
    }
};

const KnownGitSource* FindKnownSource(const wxString& sourceId)
{
    for (size_t i = 0; i < WXSIZEOF(kKnownSources); ++i)
    {
        if (sourceId == kKnownSources[i].id)
            return &kKnownSources[i];
    }

    return nullptr;
}

wxString StripTrailingSeparator(wxString path)
{
    while (path.Length() > 1
           && (path.Last() == wxT('/') || path.Last() == wxT('\\')))
    {
        path.RemoveLast();
    }

    return path;
}

wxString KnownSourceDestination(const KnownGitSource& source)
{
    wxFileName dest = wxFileName::DirName(wxGetHomeDir());
    dest.AppendDir(wxT("Library"));
    dest.AppendDir(wxT("CodeBlocks"));
    dest.AppendDir(source.leaf);
    dest.Normalize(
        wxPATH_NORM_DOTS | wxPATH_NORM_ABSOLUTE | wxPATH_NORM_TILDE);

    return StripTrailingSeparator(dest.GetFullPath());
}

bool RunGitSync(const wxString& executable,
                const wxArrayString& args,
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

    std::vector<wxChar*> argv;
    argv.reserve(args.GetCount() + 2);
    argv.push_back(const_cast<wxChar*>(executable.wx_str()));

    for (size_t i = 0; i < args.GetCount(); ++i)
        argv.push_back(const_cast<wxChar*>(args.Item(i).wx_str()));

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

bool RevParseHead(const wxString& executable,
                  const wxString& directory,
                  wxString& hash,
                  wxString& error)
{
    wxArrayString args;
    args.Add(wxT("-C"));
    args.Add(directory);
    args.Add(wxT("rev-parse"));
    args.Add(wxT("HEAD"));

    long exitCode = -1;
    wxString output;

    if (!RunGitSync(
            executable,
            args,
            exitCode,
            output,
            error))
    {
        return false;
    }

    hash = FirstLine(output);

    return exitCode == 0 && hash.Length() == 40;
}

bool EnsureParentDirectory(const wxString& destination, wxString& error)
{
    wxFileName parent = wxFileName::DirName(destination);
    parent.RemoveLastDir();

    if (parent.DirExists())
        return true;

    if (!parent.Mkdir(0777, wxPATH_MKDIR_FULL))
    {
        error = wxT("could not create the parent directory");
        return false;
    }

    return true;
}

std::vector<wxArrayString> CheckoutCommands(
    const KnownGitSource& source,
    const wxString& destination,
    bool submodulesOnly)
{
    std::vector<wxArrayString> commands;

    if (!submodulesOnly)
    {
        wxArrayString init;
        init.Add(wxT("init"));
        init.Add(destination);
        commands.push_back(init);

        wxArrayString remote;
        remote.Add(wxT("-C"));
        remote.Add(destination);
        remote.Add(wxT("remote"));
        remote.Add(wxT("add"));
        remote.Add(wxT("origin"));
        remote.Add(source.url);
        commands.push_back(remote);

        wxArrayString fetch;
        fetch.Add(wxT("-C"));
        fetch.Add(destination);
        fetch.Add(wxT("fetch"));
        fetch.Add(wxT("--depth"));
        fetch.Add(wxT("1"));
        fetch.Add(wxT("origin"));
        fetch.Add(source.commit);
        commands.push_back(fetch);

        wxArrayString checkout;
        checkout.Add(wxT("-C"));
        checkout.Add(destination);
        checkout.Add(wxT("checkout"));
        checkout.Add(wxT("--detach"));
        checkout.Add(wxT("FETCH_HEAD"));
        commands.push_back(checkout);
    }

    if (source.submodulePath)
    {
        wxArrayString submodule;
        submodule.Add(wxT("-C"));
        submodule.Add(destination);
        submodule.Add(wxT("submodule"));
        submodule.Add(wxT("update"));
        submodule.Add(wxT("--init"));
        submodule.Add(wxT("--recursive"));
        submodule.Add(wxT("--depth"));
        submodule.Add(wxT("1"));
        commands.push_back(submodule);
    }

    return commands;
}

} // namespace

MacGitProbe ProbeMacGit()
{
    MacGitProbe probe;

    ConfigManager* cfg =
        Manager::Get()->GetConfigManager(wxT("scripts"));

    const wxString saved =
        cfg
            ? cfg->Read(kSavedExecutableKey, wxEmptyString)
            : wxString();

    wxString candidate;

    if (!saved.empty())
    {
        if (wxFileExists(saved))
            candidate = saved;
        else
            probe.savedPathMissing = true;
    }

    if (candidate.empty())
    {
        if (wxFileExists(kSystemGit))
            candidate = kSystemGit;
        else if (wxFileExists(kOptGit))
            candidate = kOptGit;
        else if (wxFileExists(kUsrLocalGit))
            candidate = kUsrLocalGit;
    }

    if (candidate.empty())
        return probe;

    probe.executable = candidate;
    ApplyVersion(probe);

    return probe;
}

MacGitProbe ProbeMacGitPath(const wxString& executable)
{
    MacGitProbe probe;
    probe.executable = executable;

    if (executable.empty() || !wxFileExists(executable))
    {
        probe.errorOutput = wxT("file not found");
        return probe;
    }

    ApplyVersion(probe);

    return probe;
}

wxString FormatMacGitProbe(const MacGitProbe& probe)
{
    const wxString access =
        probe.access == MacGitWorks
            ? wxT("works")
            : (probe.access == MacGitPresentUnusable
                   ? wxT("present_unusable")
                   : wxT("not_present"));

    return wxString::Format(
        wxT("access=%s\n"
            "executable=%s\n"
            "version_exit=%ld\n"
            "version=%s\n"
            "saved_missing=%d\n"
            "error=%s"),
        access,
        probe.executable,
        probe.versionExit,
        FirstLine(probe.versionOutput),
        probe.savedPathMissing ? 1 : 0,
        FirstLine(probe.errorOutput));
}

MacGitCloneResult CloneMacGitRepository(
    wxWindow* parent,
    const wxString& url,
    const wxString& tag,
    const wxString& destination,
    const wxString& explanation,
    const wxString& confirmLabel)
{
    MacGitCloneResult result;

    result.url = url;
    result.tag = tag;
    result.destination = destination;

    if (!UrlOk(url))
    {
        result.output = wxT("invalid URL");
        return result;
    }

    if (!TagTokenOk(tag))
    {
        result.output = wxT("invalid tag");
        return result;
    }

    wxString destinationError;

    if (!DestinationOk(destination, destinationError))
    {
        result.output = destinationError;
        return result;
    }

    const MacGitProbe probe = ProbeMacGit();

    result.executable = probe.executable;

    if (probe.access != MacGitWorks)
    {
        result.output = wxT("git unavailable");

        if (!probe.errorOutput.empty())
            result.output += wxT(": ") + FirstLine(probe.errorOutput);

        return result;
    }

    // Cancel here is before wxExecute. No clone is started.
    if (!ConfirmClone(parent, explanation, confirmLabel))
        return result;

    MacGitCloneDialog dlg(
        parent,
        wxString::Format(
            _("Git is cloning tag %s into:\n%s\n\n"
              "There is no progress percentage."),
            tag,
            destination));

    if (!dlg.Start(
            probe.executable,
            url,
            tag,
            destination))
    {
        result.output = dlg.OutputTail();
        return result;
    }

    dlg.ShowModal();

    result.launched = true;
    result.cancelled = dlg.Cancelled();
    result.exitCode = dlg.ExitCode();
    result.output = dlg.OutputTail();

    result.success =
           result.launched
        && !result.cancelled
        && result.exitCode == 0;

    return result;
}

wxString FormatMacGitClone(const MacGitCloneResult& result)
{
    return wxString::Format(
        wxT("launched=%d\n"
            "cancelled=%d\n"
            "exit=%ld\n"
            "success=%d\n"
            "executable=%s\n"
            "url=%s\n"
            "tag=%s\n"
            "destination=%s\n"
            "output=%s"),
        result.launched ? 1 : 0,
        result.cancelled ? 1 : 0,
        result.exitCode,
        result.success ? 1 : 0,
        result.executable,
        result.url,
        result.tag,
        result.destination,
        result.output);
}

MacGitAcquireResult AcquireMacGitSource(
    wxWindow* parent,
    const wxString& sourceId,
    const wxString& explanation,
    const wxString& confirmLabel)
{
    MacGitAcquireResult result;
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

    const MacGitProbe probe = ProbeMacGit();
    result.executable = probe.executable;

    if (probe.access != MacGitWorks)
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

    MacGitCloneDialog dlg(
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

wxString FormatMacGitAcquire(const MacGitAcquireResult& result)
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

#endif // __WXMAC__
