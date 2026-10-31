/*
 * This file is part of the Code::Blocks IDE and licensed under
 * the GNU General Public License, version 3.
 * http://www.gnu.org/licenses/gpl-3.0.html
 *
 * v3.8 M3: resolve Code::Blocks-owned source/build/install paths,
 * locate/revalidate CMake, and perform an explicitly approved
 * CMake configure operation on macOS.
 *
 * This is deliberately not a general CMake command API.
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
#include <wx/textctrl.h>
#include <wx/thread.h>
#include <wx/timer.h>
#include <wx/utils.h>



#include "maccmake.h"

#if defined(__WXMAC__)

namespace
{

const wxString kSavedExecutableKey =
    wxT("/macos/cmake/executable");

const wxString kOptCMake =
    wxT("/opt/homebrew/bin/cmake");

const wxString kUsrLocalCMake =
    wxT("/usr/local/bin/cmake");

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

        const wxString chunk =
            wxString::FromUTF8(buf, n);

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

bool RunVersion(
    const wxString& executable,
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

bool LeafNameOk(const wxString& leaf)
{
    if (leaf.empty())
        return false;

    if (leaf == wxT(".") || leaf == wxT(".."))
        return false;

    for (size_t i = 0; i < leaf.Length(); ++i)
    {
        const wxChar c = leaf[i];

        if (c < wxT(' ')
            || c == wxT('\x7f')
            || c == wxT('/')
            || c == wxT('\\'))
        {
            return false;
        }
    }

    return true;
}

bool BuildTypeOk(const wxString& buildType)
{
    return buildType == wxT("Release")
        || buildType == wxT("Debug")
        || buildType == wxT("RelWithDebInfo")
        || buildType == wxT("MinSizeRel");
}

int BoundedJobCount()
{
    int jobs = wxThread::GetCPUCount();

    if (jobs < 1)
        jobs = 1;

    if (jobs > 8)
        jobs = 8;

    return jobs;
}

bool NormaliseAbsoluteDirectory(
    const wxString& input,
    wxString& output)
{
    if (input.empty())
        return false;

    wxFileName path = wxFileName::DirName(input);

    if (!path.IsAbsolute())
        return false;

    path.Normalize(
        wxPATH_NORM_DOTS | wxPATH_NORM_ABSOLUTE,
        wxEmptyString,
        wxPATH_NATIVE);

    output = path.GetFullPath();

    return !output.empty();
}

bool ValidateConfigureInputs(
    const wxString& sourceInput,
    const wxString& buildInput,
    const wxString& installInput,
    const wxString& buildType,
    wxString& source,
    wxString& build,
    wxString& install,
    wxString& error)
{
    if (!BuildTypeOk(buildType))
    {
        error = wxT("invalid CMake build type");
        return false;
    }

    if (!NormaliseAbsoluteDirectory(sourceInput, source))
    {
        error = wxT("source directory is not absolute");
        return false;
    }

    if (!NormaliseAbsoluteDirectory(buildInput, build))
    {
        error = wxT("build directory is not absolute");
        return false;
    }

    if (!NormaliseAbsoluteDirectory(installInput, install))
    {
        error = wxT("install directory is not absolute");
        return false;
    }

    if (!wxFileExists(
            source
            + wxFILE_SEP_PATH
            + wxT("CMakeLists.txt")))
    {
        error = wxT("source/CMakeLists.txt was not found");
        return false;
    }

    if (source == build)
    {
        error = wxT("source and build directories are identical");
        return false;
    }

    if (source == install)
    {
        error = wxT("source and install directories are identical");
        return false;
    }

    if (build == install)
    {
        error = wxT("build and install directories are identical");
        return false;
    }

    wxFileName expectedBuildPath =
        wxFileName::DirName(source);

    expectedBuildPath.AppendDir(
        wxT("build-codeblocks"));

    expectedBuildPath.Normalize(
        wxPATH_NORM_DOTS | wxPATH_NORM_ABSOLUTE,
        wxEmptyString,
        wxPATH_NATIVE);

    const wxString expectedBuild =
        expectedBuildPath.GetFullPath();

    wxFileName actualBuildPath =
        wxFileName::DirName(build);

    actualBuildPath.Normalize(
        wxPATH_NORM_DOTS | wxPATH_NORM_ABSOLUTE,
        wxEmptyString,
        wxPATH_NATIVE);

    build =
        actualBuildPath.GetFullPath();

    if (build != expectedBuild)
    {
        error =
            wxT("build directory is not source/build-codeblocks");

        return false;
    }

    if (wxFileExists(build))
    {
        error = wxT("build path is an existing file");
        return false;
    }

    if (wxFileExists(install))
    {
        error = wxT("install path is an existing file");
        return false;
    }

    return true;
}

bool ValidateBuildInput(
    const wxString& buildInput,
    wxString& build,
    wxString& error)
{
    if (!NormaliseAbsoluteDirectory(
            buildInput,
            build))
    {
        error =
            wxT("build directory is not absolute");

        return false;
    }

    if (!wxDirExists(build))
    {
        error =
            wxT("build directory does not exist");

        return false;
    }

    if (!wxFileExists(
            build
            + wxFILE_SEP_PATH
            + wxT("CMakeCache.txt")))
    {
        error =
            wxT("build/CMakeCache.txt was not found");

        return false;
    }

    wxFileName buildPath =
        wxFileName::DirName(build);

    if (buildPath.GetDirs().IsEmpty()
        || buildPath.GetDirs().Last()
               != wxT("build-codeblocks"))
    {
        error =
            wxT("build directory is not named build-codeblocks");

        return false;
    }

    return true;
}

void ApplyVersion(MacCMakeProbe& probe)
{
    wxString versionError;

    if (!RunVersion(
            probe.executable,
            probe.versionExit,
            probe.versionOutput,
            versionError))
    {
        probe.access = MacCMakePresentUnusable;
        probe.errorOutput = versionError;
        return;
    }

    probe.errorOutput = versionError;

    const wxString version = FirstLine(probe.versionOutput);

    if (probe.versionExit != 0
        || !version.Lower().StartsWith(wxT("cmake version")))
    {
        probe.access = MacCMakePresentUnusable;
        return;
    }

    probe.access = MacCMakeWorks;
}

bool ConfirmConfigure(
    const wxString& explanation,
    const wxString& confirmLabel)
{
    wxDialog dlg(
        Manager::Get()->GetAppWindow(),
        wxID_ANY,
        _("CMake"),
        wxDefaultPosition,
        wxDefaultSize,
        wxDEFAULT_DIALOG_STYLE | wxRESIZE_BORDER);

    wxBoxSizer* outer = new wxBoxSizer(wxVERTICAL);

    wxStaticText* text =
        new wxStaticText(&dlg, wxID_ANY, explanation);

    text->Wrap(520);

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
    dlg.SetInitialSize(wxSize(600, -1));
    dlg.CentreOnParent();

    return dlg.ShowModal() == wxID_OK;
}

class MacCMakeConfigureDialog : public wxDialog
{
public:
    MacCMakeConfigureDialog(
        wxWindow* parent,
        const wxString& source,
        const wxString& build,
        const wxString& install,
        const wxString& buildType)
        : wxDialog(
              parent,
              wxID_ANY,
              _("CMake"),
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

        const wxString description =
            _("CMake is configuring the OGRE source.\n\n")
            + _("Source:\n")
            + source
            + _("\n\nBuild:\n")
            + build
            + _("\n\nInstall:\n")
            + install
            + _("\n\nBuild type:\n")
            + buildType
            + _("\n\nThere is no progress percentage.");

        wxStaticText* label =
            new wxStaticText(this, wxID_ANY, description);

        label->Wrap(620);

        outer->Add(label, 0, wxALL | wxEXPAND, 12);

        m_text = new wxTextCtrl(
            this,
            wxID_ANY,
            wxEmptyString,
            wxDefaultPosition,
            wxSize(680, 320),
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
        SetInitialSize(wxSize(720, 560));
        CentreOnParent();

        Bind(
            wxEVT_TIMER,
            &MacCMakeConfigureDialog::OnTimer,
            this);
        Bind(
            wxEVT_END_PROCESS,
            &MacCMakeConfigureDialog::OnProcessEnd,
            this);
        Bind(
            wxEVT_BUTTON,
            &MacCMakeConfigureDialog::OnCancel,
            this,
            wxID_CANCEL);
        Bind(
            wxEVT_CLOSE_WINDOW,
            &MacCMakeConfigureDialog::OnCloseWindow,
            this);
    }

    ~MacCMakeConfigureDialog() override
    {
        m_timer.Stop();
        delete m_process;
    }

    bool Start(
        const wxString& executable,
        const wxString& source,
        const wxString& build,
        const wxString& install,
        const wxString& buildType)
    {
        m_process = new wxProcess(this);
        m_process->Redirect();

        wxExecuteEnv env;

        if (!wxGetEnvMap(&env.env) || env.env.empty())
        {
            delete m_process;
            m_process = nullptr;

            Append(
                wxT("Could not copy the environment, "
                    "so CMake was not started.\n"));

            return false;
        }

        const wxString buildTypeArgument =
            wxT("-DCMAKE_BUILD_TYPE=") + buildType;

        const wxString installArgument =
            wxT("-DCMAKE_INSTALL_PREFIX=") + install;

        const wxChar* argv[8];

        argv[0] = executable.wx_str();
        argv[1] = wxT("-S");
        argv[2] = source.wx_str();
        argv[3] = wxT("-B");
        argv[4] = build.wx_str();
        argv[5] = buildTypeArgument.wx_str();
        argv[6] = installArgument.wx_str();
        argv[7] = nullptr;

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

            Append(wxT("CMake did not start.\n"));
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

        wxKill(
            m_pid,
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

        wxWindow* parent = GetParent();

        EndModal(wxID_OK);

        if (parent)
        {
            parent->Raise();
            parent->SetFocus();
        }
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
                wxT("\nCancel requested. "
                    "Stopping the CMake process "
                    "Code::Blocks started. "
                    "This does not remove files "
                    "CMake already wrote.\n"));

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
                    wxT("\nCancel requested. "
                        "Stopping the CMake process "
                        "Code::Blocks started. "
                        "This does not remove files "
                        "CMake already wrote.\n"));

                StopChild();
            }

            event.Veto();
            return;
        }

        Finish();
    }

    wxTextCtrl* m_text;
    wxProcess* m_process;
    wxString m_all;

    long m_pid;
    long m_exit;

    bool m_cancelled;
    bool m_finished;
    bool m_closed;

    wxTimer m_timer;
};


class MacCMakeBuildDialog : public wxDialog
{
public:
    MacCMakeBuildDialog(
        wxWindow* parent,
        const wxString& build,
        int jobs)
        : wxDialog(
              parent,
              wxID_ANY,
              _("CMake"),
              wxDefaultPosition,
              wxDefaultSize,
              wxDEFAULT_DIALOG_STYLE
              | wxRESIZE_BORDER),
          m_text(nullptr),
          m_process(nullptr),
          m_pid(0),
          m_exit(-1),
          m_cancelled(false),
          m_finished(false),
          m_closed(false),
          m_timer(this)
    {
        wxBoxSizer* outer =
            new wxBoxSizer(wxVERTICAL);

        const wxString description =
            _("CMake is building OGRE.\n\n")
            + _("Build directory:\n")
            + build
            + _("\n\nParallel jobs:\n")
            + wxString::Format(
                  wxT("%d"),
                  jobs)
            + _("\n\nThere is no progress percentage.");

        wxStaticText* label =
            new wxStaticText(
                this,
                wxID_ANY,
                description);

        label->Wrap(620);

        outer->Add(
            label,
            0,
            wxALL | wxEXPAND,
            12);

        m_text =
            new wxTextCtrl(
                this,
                wxID_ANY,
                wxEmptyString,
                wxDefaultPosition,
                wxSize(680, 320),
                wxTE_MULTILINE
                | wxTE_READONLY);

        outer->Add(
            m_text,
            1,
            wxLEFT
            | wxRIGHT
            | wxEXPAND,
            12);

        outer->Add(
            new wxButton(
                this,
                wxID_CANCEL,
                _("Cancel")),
            0,
            wxALL | wxALIGN_RIGHT,
            12);

        SetSizer(outer);
        SetInitialSize(
            wxSize(720, 520));
        CentreOnParent();

        Bind(
            wxEVT_TIMER,
            &MacCMakeBuildDialog::OnTimer,
            this);

        Bind(
            wxEVT_END_PROCESS,
            &MacCMakeBuildDialog::OnProcessEnd,
            this);

        Bind(
            wxEVT_BUTTON,
            &MacCMakeBuildDialog::OnCancel,
            this,
            wxID_CANCEL);

        Bind(
            wxEVT_CLOSE_WINDOW,
            &MacCMakeBuildDialog::OnCloseWindow,
            this);
    }

    ~MacCMakeBuildDialog() override
    {
        m_timer.Stop();
        delete m_process;
    }

    bool Start(
        const wxString& executable,
        const wxString& build,
        int jobs)
    {
        m_process =
            new wxProcess(this);

        m_process->Redirect();

        wxExecuteEnv env;

        if (!wxGetEnvMap(&env.env)
            || env.env.empty())
        {
            delete m_process;
            m_process = nullptr;

            Append(
                wxT("Could not copy the environment, "
                    "so CMake was not started.\n"));

            return false;
        }

        const wxString jobsArgument =
            wxString::Format(
                wxT("%d"),
                jobs);

        const wxChar* argv[6];

        argv[0] = executable.wx_str();
        argv[1] = wxT("--build");
        argv[2] = build.wx_str();
        argv[3] = wxT("--parallel");
        argv[4] = jobsArgument.wx_str();
        argv[5] = nullptr;

        m_pid =
            wxExecute(
                argv,
                wxEXEC_ASYNC,
                m_process,
                &env);

        if (m_pid <= 0)
        {
            delete m_process;
            m_process = nullptr;
            m_pid = 0;

            Append(
                wxT("CMake did not start.\n"));

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

    void ReadStream(
        wxInputStream* in,
        bool available)
    {
        if (!in || !available)
            return;

        char buf[4096];

        in->Read(
            buf,
            sizeof(buf));

        const size_t n =
            in->LastRead();

        if (n == 0)
            return;

        const wxString chunk =
            wxString::FromUTF8(
                buf,
                n);

        if (chunk.empty())
        {
            Append(
                wxString(
                    buf,
                    wxConvISO8859_1,
                    n));
        }
        else
        {
            Append(chunk);
        }
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

        wxKill(
            m_pid,
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

    void OnProcessEnd(
        wxProcessEvent& event)
    {
        ReadPending();

        m_exit =
            event.GetExitCode();

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
                wxT("\nCancel requested. "
                    "Stopping the CMake build "
                    "Code::Blocks started. "
                    "This does not remove files "
                    "already produced by the build.\n"));

            StopChild();
        }
    }

    void OnCloseWindow(
        wxCloseEvent& event)
    {
        if (!m_finished && m_pid > 0)
        {
            if (!m_cancelled)
            {
                m_cancelled = true;

                Append(
                    wxT("\nCancel requested. "
                        "Stopping the CMake build "
                        "Code::Blocks started. "
                        "This does not remove files "
                        "already produced by the build.\n"));

                StopChild();
            }

            event.Veto();
            return;
        }

        Finish();
    }

    wxTextCtrl* m_text;
    wxProcess* m_process;
    wxString m_all;

    long m_pid;
    long m_exit;

    bool m_cancelled;
    bool m_finished;
    bool m_closed;

    wxTimer m_timer;
};

bool ValidateInstallInput(
    const wxString& buildInput,
    const wxString& installInput,
    wxString& build,
    wxString& install,
    wxString& error)
{
    if (!ValidateBuildInput(
            buildInput,
            build,
            error))
    {
        return false;
    }

    if (!NormaliseAbsoluteDirectory(
            installInput,
            install))
    {
        error =
            wxT("install directory is not absolute");

        return false;
    }

    if (build == install)
    {
        error =
            wxT("build and install directories are identical");

        return false;
    }

    return true;
}

bool ValidateInstalledOgreSdk(
    const wxString& install,
    wxString& error)
{
    if (!wxFileExists(
            install
            + wxT("/CMake/OGREConfig.cmake")))
    {
        error =
            wxT("installed OGREConfig.cmake was not found");

        return false;
    }

    if (!wxDirExists(
            install
            + wxT("/include")))
    {
        error =
            wxT("installed include directory was not found");

        return false;
    }

    if (!wxDirExists(
            install
            + wxT("/lib")))
    {
        error =
            wxT("installed lib directory was not found");

        return false;
    }

    if (!wxDirExists(
            install
            + wxT("/Media")))
    {
        error =
            wxT("installed Media directory was not found");

        return false;
    }

    if (!wxFileExists(
            install
            + wxT("/bin/resources.cfg")))
    {
        error =
            wxT("installed resources.cfg was not found");

        return false;
    }

    if (!wxDirExists(
            install
            + wxT("/lib/macosx/Release/RenderSystem_GL.framework")))
    {
        error =
            wxT("installed RenderSystem_GL.framework was not found");

        return false;
    }

    if (!wxDirExists(
            install
            + wxT("/lib/macosx/Release/RenderSystem_GL3Plus.framework")))
    {
        error =
            wxT("installed RenderSystem_GL3Plus.framework was not found");

        return false;
    }

    return true;
}

class MacCMakeInstallDialog : public wxDialog
{
public:
    MacCMakeInstallDialog(
        wxWindow* parent,
        const wxString& build,
        const wxString& install)
        : wxDialog(
              parent,
              wxID_ANY,
              _("CMake"),
              wxDefaultPosition,
              wxDefaultSize,
              wxDEFAULT_DIALOG_STYLE
              | wxRESIZE_BORDER),
          m_text(nullptr),
          m_process(nullptr),
          m_pid(0),
          m_exit(-1),
          m_cancelled(false),
          m_finished(false),
          m_closed(false),
          m_timer(this)
    {
        wxBoxSizer* outer =
            new wxBoxSizer(wxVERTICAL);

        const wxString description =
            _("CMake is installing OGRE.\n\n")
            + _("Build directory:\n")
            + build
            + _("\n\nInstall prefix:\n")
            + install
            + _("\n\nThe install prefix was fixed during "
                "CMake configuration.");

        wxStaticText* label =
            new wxStaticText(
                this,
                wxID_ANY,
                description);

        label->Wrap(620);

        outer->Add(
            label,
            0,
            wxALL | wxEXPAND,
            12);

        m_text =
            new wxTextCtrl(
                this,
                wxID_ANY,
                wxEmptyString,
                wxDefaultPosition,
                wxSize(680, 320),
                wxTE_MULTILINE
                | wxTE_READONLY);

        outer->Add(
            m_text,
            1,
            wxLEFT
            | wxRIGHT
            | wxEXPAND,
            12);

        outer->Add(
            new wxButton(
                this,
                wxID_CANCEL,
                _("Cancel")),
            0,
            wxALL | wxALIGN_RIGHT,
            12);

        SetSizer(outer);
        SetInitialSize(
            wxSize(720, 520));
        CentreOnParent();

        Bind(
            wxEVT_TIMER,
            &MacCMakeInstallDialog::OnTimer,
            this);

        Bind(
            wxEVT_END_PROCESS,
            &MacCMakeInstallDialog::OnProcessEnd,
            this);

        Bind(
            wxEVT_BUTTON,
            &MacCMakeInstallDialog::OnCancel,
            this,
            wxID_CANCEL);

        Bind(
            wxEVT_CLOSE_WINDOW,
            &MacCMakeInstallDialog::OnCloseWindow,
            this);
    }

    ~MacCMakeInstallDialog() override
    {
        m_timer.Stop();
        delete m_process;
    }

    bool Start(
        const wxString& executable,
        const wxString& build)
    {
        m_process =
            new wxProcess(this);

        m_process->Redirect();

        wxExecuteEnv env;

        if (!wxGetEnvMap(&env.env)
            || env.env.empty())
        {
            delete m_process;
            m_process = nullptr;

            Append(
                wxT("Could not copy the environment, "
                    "so CMake was not started.\n"));

            return false;
        }

        const wxChar* argv[4];

        argv[0] = executable.wx_str();
        argv[1] = wxT("--install");
        argv[2] = build.wx_str();
        argv[3] = nullptr;

        m_pid =
            wxExecute(
                argv,
                wxEXEC_ASYNC,
                m_process,
                &env);

        if (m_pid <= 0)
        {
            delete m_process;
            m_process = nullptr;
            m_pid = 0;

            Append(
                wxT("CMake did not start.\n"));

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

    void ReadStream(
        wxInputStream* in,
        bool available)
    {
        if (!in || !available)
            return;

        char buf[4096];

        in->Read(
            buf,
            sizeof(buf));

        const size_t n =
            in->LastRead();

        if (n == 0)
            return;

        const wxString chunk =
            wxString::FromUTF8(
                buf,
                n);

        if (chunk.empty())
        {
            Append(
                wxString(
                    buf,
                    wxConvISO8859_1,
                    n));
        }
        else
        {
            Append(chunk);
        }
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

        wxKill(
            m_pid,
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

        wxWindow* parent =
            GetParent();

        EndModal(wxID_OK);

        if (parent)
            parent->Raise();
    }

    void OnTimer(wxTimerEvent&)
    {
        ReadPending();
    }

    void OnProcessEnd(
        wxProcessEvent& event)
    {
        ReadPending();

        m_exit =
            event.GetExitCode();

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
                wxT("\nCancel requested. "
                    "Stopping the CMake install "
                    "Code::Blocks started. "
                    "This does not remove files "
                    "already installed.\n"));

            StopChild();
        }
    }

    void OnCloseWindow(
        wxCloseEvent& event)
    {
        if (!m_finished && m_pid > 0)
        {
            if (!m_cancelled)
            {
                m_cancelled = true;

                Append(
                    wxT("\nCancel requested. "
                        "Stopping the CMake install "
                        "Code::Blocks started. "
                        "This does not remove files "
                        "already installed.\n"));

                StopChild();
            }

            event.Veto();
            return;
        }

        Finish();
    }

    wxTextCtrl* m_text;
    wxProcess* m_process;
    wxString m_all;

    long m_pid;
    long m_exit;

    bool m_cancelled;
    bool m_finished;
    bool m_closed;

    wxTimer m_timer;
};

} // namespace

MacCMakeProbe ProbeMacCMake()
{
    MacCMakeProbe probe;

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
        if (wxFileExists(kOptCMake))
            candidate = kOptCMake;
        else if (wxFileExists(kUsrLocalCMake))
            candidate = kUsrLocalCMake;
    }

    if (candidate.empty())
        return probe;

    probe.executable = candidate;
    ApplyVersion(probe);

    return probe;
}

MacCMakeProbe ProbeMacCMakePath(
    const wxString& executable)
{
    MacCMakeProbe probe;

    probe.executable = executable;

    if (executable.empty() || !wxFileExists(executable))
    {
        probe.errorOutput = wxT("file not found");
        return probe;
    }

    ApplyVersion(probe);

    return probe;
}

wxString FormatMacCMakeProbe(
    const MacCMakeProbe& probe)
{
    const wxString access =
        probe.access == MacCMakeWorks
            ? wxT("works")
            : (probe.access == MacCMakePresentUnusable
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

MacCMakePaths ResolveMacCMakePaths(
    const wxString& sourceLeaf,
    const wxString& installLeaf)
{
    MacCMakePaths paths;

    if (!LeafNameOk(sourceLeaf))
    {
        paths.errorOutput = wxT("invalid source leaf");
        return paths;
    }

    if (!LeafNameOk(installLeaf))
    {
        paths.errorOutput = wxT("invalid install leaf");
        return paths;
    }

    paths.root = wxGetHomeDir();

    if (paths.root.empty())
    {
        paths.errorOutput = wxT("user home directory is empty");
        return paths;
    }

    paths.root += wxFILE_SEP_PATH;
    paths.root += wxT("Library");
    paths.root += wxFILE_SEP_PATH;
    paths.root += wxT("CodeBlocks");

    wxFileName root = wxFileName::DirName(paths.root);

    if (!root.IsAbsolute())
    {
        paths.errorOutput = wxT("Code::Blocks SDK root is not absolute");
        return paths;
    }

    paths.root = root.GetFullPath();

    paths.source =
        paths.root
        + wxFILE_SEP_PATH
        + sourceLeaf;

    paths.build =
        paths.source
        + wxFILE_SEP_PATH
        + wxT("build-codeblocks");

    paths.install =
        paths.root
        + wxFILE_SEP_PATH
        + installLeaf;

    if (paths.source == paths.install
        || paths.build == paths.install)
    {
        paths.errorOutput = wxT("resolved paths overlap");
        return paths;
    }

    paths.valid = true;

    return paths;
}

wxString FormatMacCMakePaths(
    const MacCMakePaths& paths)
{
    return wxString::Format(
        wxT("valid=%d\n"
            "root=%s\n"
            "source=%s\n"
            "build=%s\n"
            "install=%s\n"
            "error=%s"),
        paths.valid ? 1 : 0,
        paths.root,
        paths.source,
        paths.build,
        paths.install,
        paths.errorOutput);
}

MacCMakeRunResult BuildMacCMake(
    const wxString& buildInput,
    const wxString& explanation,
    const wxString& confirmLabel)
{
    MacCMakeRunResult result;

    result.operation =
        wxT("build");

    wxString build;
    wxString validationError;

    if (!ValidateBuildInput(
            buildInput,
            build,
            validationError))
    {
        result.output =
            validationError;

        return result;
    }

    const MacCMakeProbe probe =
        ProbeMacCMake();

    if (probe.access != MacCMakeWorks)
    {
        result.output =
            wxT("cmake unavailable");

        if (!probe.errorOutput.empty())
        {
            result.output +=
                wxT(": ")
                + FirstLine(
                    probe.errorOutput);
        }

        return result;
    }

    if (!ConfirmConfigure(
            explanation,
            confirmLabel))
    {
        return result;
    }

    const int jobs =
        BoundedJobCount();

    MacCMakeBuildDialog dlg(
        Manager::Get()->GetAppWindow(),
        build,
        jobs);

    if (!dlg.Start(
            probe.executable,
            build,
            jobs))
    {
        result.output =
            dlg.OutputTail();

        return result;
    }

    dlg.ShowModal();

    result.launched = true;
    result.cancelled =
        dlg.Cancelled();
    result.exitCode =
        dlg.ExitCode();
    result.output =
        dlg.OutputTail();

    result.success =
           result.launched
        && !result.cancelled
        && result.exitCode == 0;

    return result;
}

MacCMakeRunResult InstallMacCMake(
    const wxString& buildInput,
    const wxString& installInput,
    const wxString& explanation,
    const wxString& confirmLabel)
{
    MacCMakeRunResult result;

    result.operation =
        wxT("install");

    wxString build;
    wxString install;
    wxString validationError;

    if (!ValidateInstallInput(
            buildInput,
            installInput,
            build,
            install,
            validationError))
    {
        result.output =
            validationError;

        return result;
    }

    const MacCMakeProbe probe =
        ProbeMacCMake();

    if (probe.access != MacCMakeWorks)
    {
        result.output =
            wxT("cmake unavailable");

        if (!probe.errorOutput.empty())
        {
            result.output +=
                wxT(": ")
                + FirstLine(
                    probe.errorOutput);
        }

        return result;
    }

    if (!ConfirmConfigure(
            explanation,
            confirmLabel))
    {
        return result;
    }

    MacCMakeInstallDialog dlg(
        Manager::Get()->GetAppWindow(),
        build,
        install);

    if (!dlg.Start(
            probe.executable,
            build))
    {
        result.output =
            dlg.OutputTail();

        return result;
    }

    dlg.ShowModal();

    result.launched = true;
    result.cancelled =
        dlg.Cancelled();
    result.exitCode =
        dlg.ExitCode();
    result.output =
        dlg.OutputTail();

    if (!result.launched
        || result.cancelled
        || result.exitCode != 0)
    {
        return result;
    }

    wxString sdkError;

    if (!ValidateInstalledOgreSdk(
            install,
            sdkError))
    {
        result.output =
            sdkError;

        return result;
    }

    result.success = true;

    return result;
}

MacCMakeRunResult ConfigureMacCMake(
    const wxString& sourceInput,
    const wxString& buildInput,
    const wxString& installInput,
    const wxString& buildType,
    const wxString& explanation,
    const wxString& confirmLabel)
{
    MacCMakeRunResult result;

    result.operation = wxT("configure");

    wxString source;
    wxString build;
    wxString install;
    wxString validationError;

    if (!ValidateConfigureInputs(
            sourceInput,
            buildInput,
            installInput,
            buildType,
            source,
            build,
            install,
            validationError))
    {
        result.output = validationError;
        return result;
    }

    const MacCMakeProbe probe = ProbeMacCMake();

    if (probe.access != MacCMakeWorks)
    {
        result.output = wxT("cmake unavailable");

        if (!probe.errorOutput.empty())
        {
            result.output +=
                wxT(": ")
                + FirstLine(probe.errorOutput);
        }

        return result;
    }

    // Cancel here occurs before wxExecute.
    // No CMake operation has started.
    if (!ConfirmConfigure(explanation, confirmLabel))
        return result;

    MacCMakeConfigureDialog dlg(
        Manager::Get()->GetAppWindow(),
        source,
        build,
        install,
        buildType);

    if (!dlg.Start(
            probe.executable,
            source,
            build,
            install,
            buildType))
    {
        result.output = dlg.OutputTail();
        return result;
    }

    dlg.ShowModal();

    result.launched = true;
    result.cancelled = dlg.Cancelled();
    result.exitCode = dlg.ExitCode();
    result.output = dlg.OutputTail();

    const wxString cacheFile =
        build
        + wxFILE_SEP_PATH
        + wxT("CMakeCache.txt");

    result.success =
           result.launched
        && !result.cancelled
        && result.exitCode == 0
        && wxFileExists(cacheFile);

    if (!result.success
        && result.exitCode == 0
        && !wxFileExists(cacheFile))
    {
        result.output =
            wxT("CMake exited successfully, "
                "but CMakeCache.txt was not created");
    }

    return result;
}

wxString FormatMacCMakeRun(
    const MacCMakeRunResult& result)
{
    return wxString::Format(
        wxT("operation=%s\n"
            "launched=%d\n"
            "cancelled=%d\n"
            "exit=%ld\n"
            "success=%d\n"
            "output=%s"),
        result.operation,
        result.launched ? 1 : 0,
        result.cancelled ? 1 : 0,
        result.exitCode,
        result.success ? 1 : 0,
        result.output);
}

#endif // __WXMAC__

