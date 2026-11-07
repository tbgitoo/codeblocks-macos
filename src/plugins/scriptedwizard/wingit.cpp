/*
 * This file is part of the Code::Blocks IDE and licensed under the GNU General Public License, version 3
 * http://www.gnu.org/licenses/gpl-3.0.html
 *
 * v3.6b. Locate Git, revalidate one executable, and perform one
 * explicitly approved shallow tagged clone.
 *
 * This is deliberately not a general Git command API.
 * The helper does not install Git and does not know which project is cloned.
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

#include "wingit.h"

#if defined(__WXMSW__)

namespace
{

const wxString kSavedExecutableKey = wxT("/windows/git/executable");

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

bool IsDriveRoot(const wxString& path)
{
    wxFileName name = wxFileName::DirName(path);
    return name.GetDirCount() == 0;
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

    if (normalised.empty() || IsDriveRoot(normalised))
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

wxString ProgramFilesGit(const wxString& relative)
{
    wxString programFiles;

    if (!wxGetEnv(wxT("ProgramFiles"), &programFiles) || programFiles.empty())
        return wxString();

    wxFileName file(programFiles, wxEmptyString);
    file.AppendDir(wxT("Git"));

    const wxString parts = relative;
    const int slash = parts.Find(wxT('\\'));

    if (slash == wxNOT_FOUND)
        file.SetFullName(parts);
    else
    {
        file.AppendDir(parts.Left(slash));
        file.SetFullName(parts.Mid(slash + 1));
    }

    return file.GetFullPath();
}

void ApplyVersion(WinGitProbe& probe)
{
    wxString versionError;

    if (!RunVersion(
            probe.executable,
            probe.versionExit,
            probe.versionOutput,
            versionError))
    {
        probe.access = WinGitPresentUnusable;
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
        probe.access = WinGitPresentUnusable;
        return;
    }

    probe.access = WinGitWorks;
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

class WinGitCloneDialog : public wxDialog
{
public:
    WinGitCloneDialog(wxWindow* parent,
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

        Bind(wxEVT_TIMER, &WinGitCloneDialog::OnTimer, this);
        Bind(wxEVT_END_PROCESS, &WinGitCloneDialog::OnProcessEnd, this);
        Bind(wxEVT_BUTTON, &WinGitCloneDialog::OnCancel, this, wxID_CANCEL);
        Bind(wxEVT_CLOSE_WINDOW, &WinGitCloneDialog::OnCloseWindow, this);
    }

    ~WinGitCloneDialog() override
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

        m_pid = wxExecute(argv, wxEXEC_ASYNC, m_process, &env);

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

        wxKill(pid, wxSIGTERM, NULL, wxKILL_NOCHILDREN);
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

WinGitProbe ProbeWinGit()
{
    WinGitProbe probe;

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
        const wxString cmd = ProgramFilesGit(wxT("cmd\\git.exe"));
        const wxString bin = ProgramFilesGit(wxT("bin\\git.exe"));

        if (!cmd.empty() && wxFileExists(cmd))
            candidate = cmd;
        else if (!bin.empty() && wxFileExists(bin))
            candidate = bin;
    }

    if (candidate.empty())
        return probe;

    probe.executable = candidate;
    ApplyVersion(probe);

    return probe;
}

WinGitProbe ProbeWinGitPath(const wxString& executable)
{
    WinGitProbe probe;
    probe.executable = executable;

    if (executable.empty() || !wxFileExists(executable))
    {
        probe.errorOutput = wxT("file not found");
        return probe;
    }

    ApplyVersion(probe);

    return probe;
}

wxString FormatWinGitProbe(const WinGitProbe& probe)
{
    const wxString access =
        probe.access == WinGitWorks
            ? wxT("works")
            : (probe.access == WinGitPresentUnusable
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

WinGitCloneResult CloneWinGitRepository(
    wxWindow* parent,
    const wxString& url,
    const wxString& tag,
    const wxString& destination,
    const wxString& explanation,
    const wxString& confirmLabel)
{
    WinGitCloneResult result;

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

    const WinGitProbe probe = ProbeWinGit();

    result.executable = probe.executable;

    if (probe.access != WinGitWorks)
    {
        result.output = wxT("git unavailable");

        if (!probe.errorOutput.empty())
            result.output += wxT(": ") + FirstLine(probe.errorOutput);

        return result;
    }

    if (!ConfirmClone(parent, explanation, confirmLabel))
        return result;

    WinGitCloneDialog dlg(parent, tag, destination);

    if (!dlg.Start(probe.executable, url, tag, destination))
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

wxString FormatWinGitClone(const WinGitCloneResult& result)
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

#endif // __WXMSW__
