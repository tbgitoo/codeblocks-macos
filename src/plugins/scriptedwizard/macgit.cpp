/*
 * This file is part of the Code::Blocks IDE and licensed under the GNU General Public License, version 3
 * http://www.gnu.org/licenses/gpl-3.0.html
 *
 * v3.8 M2: locate Git, revalidate one executable, and perform one
 * explicitly approved shallow tagged clone.
 *
 * This is deliberately not a general Git command API.
 * The helper does not install Git and does not know anything about OGRE.
 */

#include <sdk.h>

#ifndef CB_PRECOMP
    #include <configmanager.h>
    #include <manager.h>
#endif

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

    const wxChar* argv[3];
    argv[0] = executable.wx_str();
    argv[1] = wxT("--version");
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

bool ConfirmClone(const wxString& explanation,
                  const wxString& confirmLabel)
{
    wxDialog dlg(
        Manager::Get()->GetAppWindow(),
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
                      const wxString& tag,
                      const wxString& destination)
        : wxDialog(
              parent,
              wxID_ANY,
              _("Git"),
              wxDefaultPosition,
              wxDefaultSize,
              wxDEFAULT_DIALOG_STYLE | wxRESIZE_BORDER),
          m_text(nullptr),
          m_process(nullptr),
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
            wxString::Format(
                _("Git is cloning tag %s into:\n%s\n\n"
                  "There is no progress percentage."),
                tag,
                destination));

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
    }

    bool Start(const wxString& executable,
               const wxString& url,
               const wxString& tag,
               const wxString& destination)
    {
        m_executable = executable;
        m_url = url;
        m_tag = tag;
        m_destination = destination;

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

        const wxChar* argv[9];
        argv[0] = m_executable.wx_str();
        argv[1] = wxT("clone");
        argv[2] = wxT("--branch");
        argv[3] = m_tag.wx_str();
        argv[4] = wxT("--depth");
        argv[5] = wxT("1");
        argv[6] = m_url.wx_str();
        argv[7] = m_destination.wx_str();
        argv[8] = nullptr;

        m_pid = wxExecute(
            argv,
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

    void OnProcessEnd(wxProcessEvent& event)
    {
        ReadPending();

        m_exit = event.GetExitCode();
        m_finished = true;
        m_pid = 0;

        Finish();
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

    wxString m_executable;
    wxString m_url;
    wxString m_tag;
    wxString m_destination;
    wxString m_all;

    long m_pid;
    long m_exit;

    bool m_cancelled;
    bool m_finished;
    bool m_closed;

    wxTimer m_timer;
};

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
    if (!ConfirmClone(explanation, confirmLabel))
        return result;

    MacGitCloneDialog dlg(
        Manager::Get()->GetAppWindow(),
        tag,
        destination);

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

#endif // __WXMAC__
