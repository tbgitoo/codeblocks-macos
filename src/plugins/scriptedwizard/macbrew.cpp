/*
 * This file is part of the Code::Blocks IDE and licensed under the GNU General Public License, version 3
 * http://www.gnu.org/licenses/gpl-3.0.html
 *
 * v3.0 M2–M3. argv execution of a verified brew path.
 * Formula query is {brew, "--prefix", "--installed", formula} only.
 * No shell string, no PATH edit, no install.
 */

#include <sdk.h>

#ifndef CB_PRECOMP
    #include <configmanager.h>
    #include <manager.h>
#endif

#include <wx/button.h>
#include <wx/dialog.h>
#include <wx/sizer.h>
#include <wx/stattext.h>
#include <wx/textctrl.h>
#include <wx/timer.h>

#include "macbrew.h"

#if defined(__WXMAC__)

namespace
{

const wxString kSavedExecutableKey = wxT("/macos/homebrew/executable");
const wxString kOptBrew = wxT("/opt/homebrew/bin/brew");
const wxString kUsrLocalBrew = wxT("/usr/local/bin/brew");

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

// Runs brew with up to three fixed arguments. Returns false when wxExecute cannot start it.
// arg3 is the formula token for the installed-prefix query. There is no general argv entry.
bool RunArgv(const wxString& executable, const wxChar* arg1, const wxChar* arg2,
             long& exitCode, wxString& output, wxString& error, const wxChar* arg3 = nullptr)
{
    wxProcess process(wxPROCESS_REDIRECT);
    const wxChar* argv[5];
    int n = 0;
    argv[n++] = executable.wx_str();
    if (arg1)
        argv[n++] = arg1;
    if (arg2)
        argv[n++] = arg2;
    if (arg3)
        argv[n++] = arg3;
    argv[n] = nullptr;

    exitCode = wxExecute(argv, wxEXEC_SYNC, &process);
    output = ReadAll(process.GetInputStream());
    error = ReadAll(process.GetErrorStream());
    return exitCode != -1;
}

// One token: letters, digits, and . _ + - @ / . Rejected before it is placed in argv.
bool FormulaTokenOk(const wxString& formula)
{
    if (formula.empty())
        return false;
    for (size_t i = 0; i < formula.length(); ++i)
    {
        const wxChar c = formula[i];
        const bool ok = (c >= wxT('a') && c <= wxT('z'))
                     || (c >= wxT('A') && c <= wxT('Z'))
                     || (c >= wxT('0') && c <= wxT('9'))
                     || c == wxT('.') || c == wxT('_') || c == wxT('+')
                     || c == wxT('-') || c == wxT('@') || c == wxT('/');
        if (!ok)
            return false;
    }
    return true;
}

wxString FirstLine(const wxString& text)
{
    wxString line = text.BeforeFirst(wxT('\n'));
    line.Trim(true).Trim(false);
    if (line.EndsWith(wxT("\r")))
        line.RemoveLast();
    return line;
}

void ApplyVersionAndPrefix(MacBrewProbe& probe, bool wantPrefix)
{
    wxString versionError;
    if (!RunArgv(probe.executable, wxT("--version"), nullptr, probe.versionExit, probe.versionOutput, versionError))
    {
        probe.access = MacBrewPresentUnusable;
        probe.errorOutput = versionError;
        return;
    }
    probe.errorOutput = versionError;

    const bool versionOk = probe.versionExit == 0 && probe.versionOutput.Contains(wxT("Homebrew"));
    if (!versionOk)
    {
        probe.access = MacBrewPresentUnusable;
        return;
    }

    probe.access = MacBrewWorks;
    if (!wantPrefix)
        return;

    wxString prefixOut;
    wxString prefixErr;
    if (!RunArgv(probe.executable, wxT("--prefix"), nullptr, probe.prefixExit, prefixOut, prefixErr))
    {
        if (!prefixErr.empty())
            probe.errorOutput += prefixErr;
        return;
    }
    if (!prefixErr.empty())
        probe.errorOutput += prefixErr;

    const wxString line = FirstLine(prefixOut);
    if (probe.prefixExit == 0 && line.StartsWith(wxT("/")))
        probe.prefix = line;
}

} // namespace

MacBrewProbe ProbeMacBrew(bool wantPrefix)
{
    MacBrewProbe probe;
    ConfigManager* cfg = Manager::Get()->GetConfigManager(wxT("scripts"));
    const wxString saved = cfg ? cfg->Read(kSavedExecutableKey, wxEmptyString) : wxString();

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
        if (wxFileExists(kOptBrew))
            candidate = kOptBrew;
        else if (wxFileExists(kUsrLocalBrew))
            candidate = kUsrLocalBrew;
    }

    if (candidate.empty())
        return probe;

    probe.executable = candidate;
    ApplyVersionAndPrefix(probe, wantPrefix);
    return probe;
}

MacBrewProbe ProbeMacBrewPath(const wxString& executable, bool wantPrefix)
{
    MacBrewProbe probe;
    probe.executable = executable;
    if (executable.empty() || !wxFileExists(executable))
    {
        probe.errorOutput = wxT("file not found");
        return probe;
    }
    ApplyVersionAndPrefix(probe, wantPrefix);
    return probe;
}

wxString FormatMacBrewProbe(const MacBrewProbe& probe)
{
    const wxString access = probe.access == MacBrewWorks
                          ? wxT("works")
                          : (probe.access == MacBrewPresentUnusable ? wxT("present_unusable")
                                                                    : wxT("not_present"));
    return wxString::Format(
        wxT("access=%s\nexecutable=%s\nversion_exit=%ld\nversion=%s\nprefix_exit=%ld\nprefix=%s\nsaved_missing=%d\nerror=%s"),
        access,
        probe.executable,
        probe.versionExit,
        FirstLine(probe.versionOutput),
        probe.prefixExit,
        probe.prefix,
        probe.savedPathMissing ? 1 : 0,
        FirstLine(probe.errorOutput));
}

int MacBrewChoice(const wxString& message, const wxString& firstLabel, const wxString& secondLabel)
{
    wxDialog dlg(Manager::Get()->GetAppWindow(), wxID_ANY, _("Homebrew"),
                 wxDefaultPosition, wxDefaultSize, wxDEFAULT_DIALOG_STYLE | wxRESIZE_BORDER);
    wxBoxSizer* outer = new wxBoxSizer(wxVERTICAL);
    wxStaticText* text = new wxStaticText(&dlg, wxID_ANY, message);
    text->Wrap(440);
    outer->Add(text, 1, wxALL | wxEXPAND, 12);

    wxBoxSizer* buttons = new wxBoxSizer(wxHORIZONTAL);
    buttons->AddStretchSpacer();
    buttons->Add(new wxButton(&dlg, wxID_YES, firstLabel), 0, wxRIGHT, 8);
    buttons->Add(new wxButton(&dlg, wxID_NO, secondLabel), 0, wxRIGHT, 8);
    buttons->Add(new wxButton(&dlg, wxID_CANCEL, _("Cancel")), 0);
    outer->Add(buttons, 0, wxLEFT | wxRIGHT | wxBOTTOM | wxEXPAND, 12);

    dlg.SetSizer(outer);
    dlg.SetInitialSize(wxSize(520, -1));
    dlg.CentreOnParent();

    const int id = dlg.ShowModal();
    if (id == wxID_YES)
        return 1;
    if (id == wxID_NO)
        return 2;
    return 0;
}

MacBrewFormulaQuery QueryMacBrewFormula(const wxString& formula)
{
    MacBrewFormulaQuery query;
    query.formula = formula;
    if (!FormulaTokenOk(formula))
    {
        query.state = MacBrewFormulaRejected;
        query.errorOutput = wxT("invalid formula");
        return query;
    }

    const MacBrewProbe probe = ProbeMacBrew(false);
    query.brewAccess = probe.access;
    query.executable = probe.executable;
    query.errorOutput = probe.errorOutput;
    if (probe.access != MacBrewWorks)
    {
        query.state = MacBrewFormulaBrewUnavailable;
        return query;
    }

    wxString output;
    wxString error;
    if (!RunArgv(probe.executable, wxT("--prefix"), wxT("--installed"),
                 query.queryExit, output, error, formula.wx_str()))
    {
        query.state = MacBrewFormulaFailed;
        if (!error.empty())
            query.errorOutput = error;
        return query;
    }
    query.errorOutput = error.empty() ? output : error;

    const wxString directory = FirstLine(output);
    const wxString combined = output + wxT("\n") + error;
    if (query.queryExit == 0 && directory.StartsWith(wxT("/")))
    {
        query.state = MacBrewFormulaInstalled;
        query.prefix = directory;
        query.errorOutput = error;
        return query;
    }
    // Exit 1 with Homebrew's "not installed" text is the only missing result.
    // A different exit, including "no available formula", is a failed query.
    if (query.queryExit == 1 && combined.Contains(wxT("not installed")))
    {
        query.state = MacBrewFormulaMissing;
        return query;
    }

    query.state = MacBrewFormulaFailed;
    return query;
}

wxString FormatMacBrewFormula(const MacBrewFormulaQuery& query)
{
    const wxString access = query.brewAccess == MacBrewWorks
                          ? wxT("works")
                          : (query.brewAccess == MacBrewPresentUnusable ? wxT("present_unusable")
                             : (query.state == MacBrewFormulaRejected ? wxT("not_checked")
                                                                      : wxT("not_present")));
    const wxChar* state = wxT("failed");
    switch (query.state)
    {
        case MacBrewFormulaRejected:        state = wxT("rejected"); break;
        case MacBrewFormulaBrewUnavailable: state = wxT("brew_unavailable"); break;
        case MacBrewFormulaMissing:         state = wxT("missing"); break;
        case MacBrewFormulaInstalled:       state = wxT("installed"); break;
        case MacBrewFormulaFailed:          state = wxT("failed"); break;
    }
    return wxString::Format(
        wxT("access=%s\nexecutable=%s\nformula=%s\nquery_exit=%ld\nstate=%s\nprefix=%s\nerror=%s"),
        access,
        query.executable,
        query.formula,
        query.queryExit,
        state,
        query.prefix,
        FirstLine(query.errorOutput));
}

namespace
{

bool LooksLikePasswordPrompt(const wxString& text)
{
    const wxString lower = text.Lower();
    return lower.Contains(wxT("password:")) || lower.Contains(wxT("[sudo]"));
}

wxString LastLine(const wxString& text)
{
    wxString trimmed = text;
    trimmed.Trim(true).Trim(false);
    const int nl = trimmed.Find(wxT('\n'), true);
    if (nl == wxNOT_FOUND)
        return trimmed;
    wxString line = trimmed.Mid(nl + 1);
    line.Trim(true).Trim(false);
    if (line.Length() > 200)
        line = line.Mid(line.Length() - 200);
    return line;
}

bool ConfirmInstall(const wxString& explanation, const wxString& confirmLabel)
{
    wxDialog dlg(Manager::Get()->GetAppWindow(), wxID_ANY, _("Homebrew"),
                 wxDefaultPosition, wxDefaultSize, wxDEFAULT_DIALOG_STYLE | wxRESIZE_BORDER);
    wxBoxSizer* outer = new wxBoxSizer(wxVERTICAL);
    wxStaticText* text = new wxStaticText(&dlg, wxID_ANY, explanation);
    text->Wrap(440);
    outer->Add(text, 1, wxALL | wxEXPAND, 12);

    wxBoxSizer* buttons = new wxBoxSizer(wxHORIZONTAL);
    buttons->AddStretchSpacer();
    buttons->Add(new wxButton(&dlg, wxID_OK, confirmLabel), 0, wxRIGHT, 8);
    buttons->Add(new wxButton(&dlg, wxID_CANCEL, _("Cancel")), 0);
    outer->Add(buttons, 0, wxLEFT | wxRIGHT | wxBOTTOM | wxEXPAND, 12);
    dlg.SetSizer(outer);
    dlg.SetInitialSize(wxSize(520, -1));
    dlg.CentreOnParent();
    return dlg.ShowModal() == wxID_OK;
}

class MacBrewRunDialog : public wxDialog
{
public:
    MacBrewRunDialog(wxWindow* parent, const wxString& formula)
        : wxDialog(parent, wxID_ANY, _("Homebrew"),
                   wxDefaultPosition, wxDefaultSize,
                   wxDEFAULT_DIALOG_STYLE | wxRESIZE_BORDER),
          m_text(nullptr),
          m_process(nullptr),
          m_pid(0),
          m_exit(-1),
          m_cancelled(false),
          m_password(false),
          m_finished(false),
          m_closed(false),
          m_timer(this)
    {
        wxBoxSizer* outer = new wxBoxSizer(wxVERTICAL);
        wxStaticText* label = new wxStaticText(this, wxID_ANY,
            wxString::Format(_("Homebrew is installing %s. There is no progress percentage."), formula));
        outer->Add(label, 0, wxALL | wxEXPAND, 12);
        m_text = new wxTextCtrl(this, wxID_ANY, wxEmptyString,
                                wxDefaultPosition, wxSize(560, 280),
                                wxTE_MULTILINE | wxTE_READONLY);
        outer->Add(m_text, 1, wxLEFT | wxRIGHT | wxEXPAND, 12);
        outer->Add(new wxButton(this, wxID_CANCEL, _("Cancel")), 0, wxALL | wxALIGN_RIGHT, 12);
        SetSizer(outer);
        SetInitialSize(wxSize(600, 420));
        CentreOnParent();

        Bind(wxEVT_TIMER, &MacBrewRunDialog::OnTimer, this);
        Bind(wxEVT_END_PROCESS, &MacBrewRunDialog::OnProcessEnd, this);
        Bind(wxEVT_BUTTON, &MacBrewRunDialog::OnCancel, this, wxID_CANCEL);
        Bind(wxEVT_CLOSE_WINDOW, &MacBrewRunDialog::OnCloseWindow, this);
    }

    ~MacBrewRunDialog() override
    {
        m_timer.Stop();
        delete m_process;
    }

    bool Start(const wxString& executable, const wxString& formula)
    {
        m_executable = executable;
        m_formula = formula;
        m_process = new wxProcess(this);
        m_process->Redirect();

        wxExecuteEnv env;
        if (!wxGetEnvMap(&env.env) || env.env.empty())
        {
            delete m_process;
            m_process = nullptr;
            Append(wxT("Could not copy the environment, so Homebrew was not started.\n"));
            return false;
        }
        env.env[wxT("NONINTERACTIVE")] = wxT("1");
        env.env[wxT("HOMEBREW_NO_AUTO_UPDATE")] = wxT("1");

        const wxChar* argv[4];
        argv[0] = m_executable.wx_str();
        argv[1] = wxT("install");
        argv[2] = m_formula.wx_str();
        argv[3] = nullptr;

        m_pid = wxExecute(argv, wxEXEC_ASYNC, m_process, &env);
        if (m_pid <= 0)
        {
            delete m_process;
            m_process = nullptr;
            m_pid = 0;
            Append(wxT("Homebrew did not start.\n"));
            return false;
        }
        m_timer.Start(200);
        return true;
    }

    bool Cancelled() const { return m_cancelled; }
    bool PasswordPrompt() const { return m_password; }
    long ExitCode() const { return m_exit; }
    wxString OutputTail() const { return LastLine(m_all); }

private:
    void Append(const wxString& chunk)
    {
        m_all += chunk;
        if (m_text)
            m_text->AppendText(chunk);
        if (!m_password && LooksLikePasswordPrompt(m_all))
        {
            m_password = true;
            const wxString note = wxT("\nHomebrew asked for a password. Stopping. This is not a successful install.\n");
            m_all += note;
            if (m_text)
                m_text->AppendText(note);
            StopChild();
        }
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
        ReadStream(m_process->GetInputStream(), m_process->IsInputAvailable());
        if (m_process)
            ReadStream(m_process->GetErrorStream(), m_process->IsErrorAvailable());
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
            Append(wxT("\nCancel requested. Stopping the Homebrew process Code::Blocks started. This does not undo files that process already wrote.\n"));
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
                Append(wxT("\nCancel requested. Stopping the Homebrew process Code::Blocks started. This does not undo files that process already wrote.\n"));
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
    wxString m_formula;
    wxString m_all;
    long m_pid;
    long m_exit;
    bool m_cancelled;
    bool m_password;
    bool m_finished;
    bool m_closed;
    wxTimer m_timer;
};

} // namespace

MacBrewInstallResult InstallMacBrewFormula(const wxString& formula,
                                           const wxString& explanation,
                                           const wxString& confirmLabel)
{
    MacBrewInstallResult result;
    if (!FormulaTokenOk(formula))
    {
        result.output = wxT("invalid formula");
        return result;
    }

    const MacBrewProbe probe = ProbeMacBrew(false);
    if (probe.access != MacBrewWorks)
    {
        result.output = wxT("brew unavailable");
        return result;
    }

    // Cancel here is before wxExecute. No package operation is started.
    if (!ConfirmInstall(explanation, confirmLabel))
        return result;

    MacBrewRunDialog dlg(Manager::Get()->GetAppWindow(), formula);
    if (!dlg.Start(probe.executable, formula))
    {
        result.output = dlg.OutputTail();
        return result;
    }

    dlg.ShowModal();
    result.launched = true;
    result.cancelled = dlg.Cancelled();
    result.passwordPrompt = dlg.PasswordPrompt();
    result.exitCode = dlg.ExitCode();
    result.output = dlg.OutputTail();
    result.success = result.launched
                  && !result.cancelled
                  && !result.passwordPrompt
                  && result.exitCode == 0;
    return result;
}

wxString FormatMacBrewInstall(const MacBrewInstallResult& result)
{
    return wxString::Format(
        wxT("launched=%d\ncancelled=%d\npassword=%d\nexit=%ld\nsuccess=%d\noutput=%s"),
        result.launched ? 1 : 0,
        result.cancelled ? 1 : 0,
        result.passwordPrompt ? 1 : 0,
        result.exitCode,
        result.success ? 1 : 0,
        result.output);
}

#endif // __WXMAC__
