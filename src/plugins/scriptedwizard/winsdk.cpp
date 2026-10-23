/*
 * This file is part of the Code::Blocks IDE and licensed under the GNU General Public License, version 3
 * http://www.gnu.org/licenses/gpl-3.0.html
 *
 * v3.3 Windows library fetch. One https zip, unpacked under
 * %LOCALAPPDATA%\codeblocks\sdk\<id>. Not a package manager.
 */

#include <sdk.h>

#ifndef CB_PRECOMP
    #include <configmanager.h>
    #include <manager.h>
#endif

#include <wx/button.h>
#include <wx/dir.h>
#include <wx/dialog.h>
#include <wx/filename.h>
#include <wx/process.h>
#include <wx/sizer.h>
#include <wx/stattext.h>
#include <wx/timer.h>
#include <wx/utils.h>

#include <stdio.h>

#include "winsdk.h"

#if defined(__WXMSW__)

#include <compilerfactory.h>

#include <atomic>
#include <mutex>
#include <thread>
#include <vector>

#include <windows.h>
#include <winhttp.h>

namespace
{

// A single directory name. Slash is not allowed: the token is not a path.
bool LibraryIdOk(const wxString& id)
{
    if (id.empty() || id == wxT(".") || id == wxT(".."))
        return false;
    for (size_t i = 0; i < id.length(); ++i)
    {
        const wxChar c = id[i];
        const bool ok = (c >= wxT('a') && c <= wxT('z'))
                     || (c >= wxT('A') && c <= wxT('Z'))
                     || (c >= wxT('0') && c <= wxT('9'))
                     || c == wxT('.') || c == wxT('_') || c == wxT('+')
                     || c == wxT('-') || c == wxT('@');
        if (!ok)
            return false;
    }
    return true;
}

wxString SavedKey(const wxString& id)
{
    return wxT("/windows/sdk/") + id + wxT("/root");
}

wxString SdkRoot()
{
    wxString local;
    if (!wxGetEnv(wxT("LOCALAPPDATA"), &local) || local.empty())
        return wxString();
    wxString root = local;
    if (!root.EndsWith(wxT("\\")) && !root.EndsWith(wxT("/")))
        root += wxT("\\");
    root += wxT("codeblocks\\sdk");
    return root;
}

wxString FirstLine(const wxString& text)
{
    wxString line = text.BeforeFirst(wxT('\n'));
    line.Trim(true).Trim(false);
    if (line.EndsWith(wxT("\r")))
        line.RemoveLast();
    return line;
}

bool RunGcc(const wxString& exe, const wxChar* flag, wxString& output, long& exitCode)
{
    wxProcess process(wxPROCESS_REDIRECT);
    const wxChar* argv[3];
    argv[0] = exe.wx_str();
    argv[1] = flag;
    argv[2] = nullptr;
    exitCode = wxExecute(argv, wxEXEC_SYNC, &process);
    wxString err;
    auto readAll = [](wxInputStream* in) -> wxString
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
    };
    output = readAll(process.GetInputStream());
    err = readAll(process.GetErrorStream());
    output += err;
    return exitCode != -1;
}

wxString OneChildDir(const wxString& root)
{
    wxDir dir;
    if (!dir.Open(root))
        return wxString();
    wxString name;
    int count = 0;
    wxString only;
    bool more = dir.GetFirst(&name, wxEmptyString, wxDIR_DIRS | wxDIR_HIDDEN);
    while (more)
    {
        ++count;
        only = root;
        if (!only.EndsWith(wxT("\\")))
            only += wxT("\\");
        only += name;
        if (count > 1)
            return wxString();
        more = dir.GetNext(&name);
    }
    if (count == 1)
        return only;
    return wxString();
}

struct FetchJob
{
    wxString url;
    wxString zipPath;
    wxString destDir;
    std::atomic<bool> cancel;
    std::atomic<bool> finished;
    std::atomic<int> percent;
    std::mutex mu;
    std::string status;
    bool success;
    bool cancelled;
    std::string error;

    FetchJob()
        : cancel(false),
          finished(false),
          percent(-1),
          success(false),
          cancelled(false)
    {}

    void SetStatus(const std::string& text)
    {
        std::lock_guard<std::mutex> lock(mu);
        status = text;
    }

    std::string Status()
    {
        std::lock_guard<std::mutex> lock(mu);
        return status;
    }
};

void CloseWinHttp(HINTERNET& handle)
{
    if (handle)
    {
        WinHttpCloseHandle(handle);
        handle = nullptr;
    }
}

bool DownloadHttps(FetchJob& job)
{
    URL_COMPONENTS parts;
    memset(&parts, 0, sizeof(parts));
    parts.dwStructSize = sizeof(parts);
    wchar_t host[256];
    wchar_t path[2048];
    host[0] = 0;
    path[0] = 0;
    parts.lpszHostName = host;
    parts.dwHostNameLength = sizeof(host) / sizeof(host[0]);
    parts.lpszUrlPath = path;
    parts.dwUrlPathLength = sizeof(path) / sizeof(path[0]);
    if (!WinHttpCrackUrl(job.url.wc_str(), 0, 0, &parts) || host[0] == 0)
    {
        job.error = "the address could not be read";
        return false;
    }
    if (parts.nScheme != INTERNET_SCHEME_HTTPS)
    {
        job.error = "the address is not https";
        return false;
    }

    HINTERNET session = WinHttpOpen(L"Code::Blocks",
                                    WINHTTP_ACCESS_TYPE_AUTOMATIC_PROXY,
                                    WINHTTP_NO_PROXY_NAME,
                                    WINHTTP_NO_PROXY_BYPASS,
                                    0);
    if (!session)
    {
        job.error = "the download could not start";
        return false;
    }
    // A stalled read fails. A slow transfer continues while data arrives.
    WinHttpSetTimeouts(session, 60000, 60000, 60000, 300000);

    HINTERNET connect = WinHttpConnect(session, host, parts.nPort, 0);
    if (!connect)
    {
        CloseWinHttp(session);
        job.error = "the download could not connect";
        return false;
    }

    const DWORD flags = WINHTTP_FLAG_SECURE;
    HINTERNET request = WinHttpOpenRequest(connect, L"GET", path,
                                           nullptr, WINHTTP_NO_REFERER,
                                           WINHTTP_DEFAULT_ACCEPT_TYPES, flags);
    if (!request)
    {
        CloseWinHttp(connect);
        CloseWinHttp(session);
        job.error = "the download could not start";
        return false;
    }
    DWORD redir = WINHTTP_OPTION_REDIRECT_POLICY_ALWAYS;
    WinHttpSetOption(request, WINHTTP_OPTION_REDIRECT_POLICY, &redir, sizeof(redir));

    job.SetStatus("Downloading.");
    const BOOL sent = WinHttpSendRequest(request, WINHTTP_NO_ADDITIONAL_HEADERS, 0,
                                         WINHTTP_NO_REQUEST_DATA, 0, 0, 0);
    if (!sent || !WinHttpReceiveResponse(request, nullptr))
    {
        CloseWinHttp(request);
        CloseWinHttp(connect);
        CloseWinHttp(session);
        if (job.cancel.load())
        {
            job.cancelled = true;
            job.error = "cancelled";
        }
        else
            job.error = "the download did not answer";
        return false;
    }

    DWORD status = 0;
    DWORD statusLen = sizeof(status);
    WinHttpQueryHeaders(request,
                        WINHTTP_QUERY_STATUS_CODE | WINHTTP_QUERY_FLAG_NUMBER,
                        WINHTTP_HEADER_NAME_BY_INDEX,
                        &status, &statusLen, WINHTTP_NO_HEADER_INDEX);
    if (status != 200)
    {
        CloseWinHttp(request);
        CloseWinHttp(connect);
        CloseWinHttp(session);
        job.error = "the download was refused";
        return false;
    }

    DWORD contentLength = 0;
    DWORD lengthSize = sizeof(contentLength);
    const bool haveLength = WinHttpQueryHeaders(request,
                               WINHTTP_QUERY_CONTENT_LENGTH | WINHTTP_QUERY_FLAG_NUMBER,
                               WINHTTP_HEADER_NAME_BY_INDEX,
                               &contentLength, &lengthSize, WINHTTP_NO_HEADER_INDEX)
                            && contentLength > 0;
    if (!haveLength)
        job.percent.store(-1);

    FILE* out = _wfopen(job.zipPath.wc_str(), L"wb");
    if (!out)
    {
        CloseWinHttp(request);
        CloseWinHttp(connect);
        CloseWinHttp(session);
        job.error = "the download could not be stored";
        return false;
    }

    unsigned long long got = 0;
    bool ok = true;
    for (;;)
    {
        if (job.cancel.load())
        {
            job.cancelled = true;
            job.error = "cancelled";
            ok = false;
            break;
        }
        DWORD available = 0;
        if (!WinHttpQueryDataAvailable(request, &available))
        {
            job.error = "the download stopped";
            ok = false;
            break;
        }
        if (available == 0)
            break;
        std::vector<char> buf(available);
        DWORD read = 0;
        if (!WinHttpReadData(request, buf.data(), available, &read))
        {
            job.error = "the download stopped";
            ok = false;
            break;
        }
        if (read == 0)
            break;
        if (fwrite(buf.data(), 1, read, out) != read)
        {
            job.error = "the download could not be stored";
            ok = false;
            break;
        }
        got += read;
        if (haveLength)
        {
            int pct = static_cast<int>((got * 100ull) / contentLength);
            if (pct > 100)
                pct = 100;
            job.percent.store(pct);
            job.SetStatus("Downloading. " + std::to_string(pct) + "%");
        }
    }
    fclose(out);
    CloseWinHttp(request);
    CloseWinHttp(connect);
    CloseWinHttp(session);
    if (!ok)
        return false;
    if (got == 0)
    {
        job.error = "the download was empty";
        return false;
    }
    return true;
}

void RunFetch(FetchJob* job)
{
    DownloadHttps(*job);
    job->finished.store(true);
}

bool ConfirmFetch(const wxString& explanation, const wxString& confirmLabel)
{
    wxDialog dlg(Manager::Get()->GetAppWindow(), wxID_ANY, confirmLabel,
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

class FetchDialog : public wxDialog
{
public:
    FetchDialog(wxWindow* parent, FetchJob* job)
        : wxDialog(parent, wxID_ANY, _("Download"),
                   wxDefaultPosition, wxDefaultSize,
                   wxDEFAULT_DIALOG_STYLE | wxRESIZE_BORDER),
          m_job(job),
          m_label(nullptr),
          m_process(nullptr),
          m_pid(0),
          m_unpacking(false),
          m_timer(this)
    {
        wxBoxSizer* outer = new wxBoxSizer(wxVERTICAL);
        m_label = new wxStaticText(this, wxID_ANY, _("Downloading."));
        outer->Add(m_label, 0, wxALL | wxEXPAND, 12);
        outer->Add(new wxButton(this, wxID_CANCEL, _("Cancel")), 0, wxALL | wxALIGN_RIGHT, 12);
        SetSizer(outer);
        SetInitialSize(wxSize(420, 140));
        CentreOnParent();
        Bind(wxEVT_TIMER, &FetchDialog::OnTimer, this);
        Bind(wxEVT_END_PROCESS, &FetchDialog::OnProcessEnd, this);
        Bind(wxEVT_BUTTON, &FetchDialog::OnCancel, this, wxID_CANCEL);
        Bind(wxEVT_CLOSE_WINDOW, &FetchDialog::OnCloseWindow, this);
        m_timer.Start(200);
        m_worker = std::thread(RunFetch, m_job);
    }

    ~FetchDialog() override
    {
        m_timer.Stop();
        m_job->cancel.store(true);
        if (m_pid > 0)
            wxKill(m_pid, wxSIGTERM, nullptr, wxKILL_CHILDREN);
        if (m_worker.joinable())
            m_worker.join();
        delete m_process;
    }

private:
    bool StartUnpack()
    {
        wxString sys;
        if (!wxGetEnv(wxT("SystemRoot"), &sys) || sys.empty())
        {
            m_job->error = "the archive could not be unpacked";
            return false;
        }
        m_tar = sys + wxT("\\System32\\tar.exe");
        if (!wxFileExists(m_tar))
        {
            m_job->error = "the archive could not be unpacked";
            return false;
        }

        m_job->percent.store(-1);
        m_job->SetStatus("Unpacking.");
        m_process = new wxProcess(this);
        m_process->Redirect();
        const wxChar* argv[6];
        argv[0] = m_tar.wx_str();
        argv[1] = wxT("-xf");
        argv[2] = m_job->zipPath.wx_str();
        argv[3] = wxT("-C");
        argv[4] = m_job->destDir.wx_str();
        argv[5] = nullptr;
        m_pid = wxExecute(argv, wxEXEC_ASYNC, m_process);
        if (m_pid <= 0)
        {
            delete m_process;
            m_process = nullptr;
            m_pid = 0;
            m_job->error = "the archive could not be unpacked";
            return false;
        }
        m_unpacking = true;
        return true;
    }

    void Finish()
    {
        if (m_worker.joinable())
            m_worker.join();
        m_timer.Stop();
        EndModal(wxID_OK);
    }

    void OnTimer(wxTimerEvent&)
    {
        const std::string status = m_job->Status();
        if (m_label && !status.empty())
            m_label->SetLabel(wxString::FromUTF8(status.c_str()));
        if (!m_job->finished.load() || m_unpacking)
            return;
        if (m_worker.joinable())
            m_worker.join();
        // DownloadHttps returns false on failure and leaves error set.
        // success is still false until tar exits 0.
        if (m_job->cancelled || !m_job->error.empty())
        {
            Finish();
            return;
        }
        if (!StartUnpack())
            Finish();
    }

    void OnProcessEnd(wxProcessEvent& event)
    {
        const long code = event.GetExitCode();
        m_pid = 0;
        if (m_job->cancelled)
            m_job->error = "cancelled";
        else if (code != 0)
            m_job->error = "the archive could not be unpacked";
        else
            m_job->success = true;
        Finish();
    }

    void OnCancel(wxCommandEvent&)
    {
        m_job->cancel.store(true);
        // During download the worker records cancelled. During unpack the
        // worker has already finished, so this thread records it.
        if (m_unpacking)
            m_job->cancelled = true;
        m_job->SetStatus("Stopping the download. Files already written are left in place.");
        if (m_pid > 0)
            wxKill(m_pid, wxSIGTERM, nullptr, wxKILL_CHILDREN);
    }

    void OnCloseWindow(wxCloseEvent& event)
    {
        if (!m_job->finished.load() || m_pid > 0)
        {
            m_job->cancel.store(true);
            if (m_unpacking)
                m_job->cancelled = true;
            m_job->SetStatus("Stopping the download. Files already written are left in place.");
            if (m_pid > 0)
                wxKill(m_pid, wxSIGTERM, nullptr, wxKILL_CHILDREN);
            event.Veto();
            return;
        }
        Finish();
    }

    FetchJob* m_job;
    wxStaticText* m_label;
    wxProcess* m_process;
    wxString m_tar;
    long m_pid;
    bool m_unpacking;
    wxTimer m_timer;
    std::thread m_worker;
};

} // namespace

WinSdkCompiler ProbeWinSdkCompiler(const wxString& compilerId)
{
    WinSdkCompiler probe;
    Compiler* compiler = nullptr;
    if (!compilerId.empty())
        compiler = CompilerFactory::GetCompiler(compilerId);
    if (!compiler)
        compiler = CompilerFactory::GetDefaultCompiler();
    if (!compiler)
    {
        probe.error = wxT("no compiler");
        return probe;
    }

    probe.master = compiler->GetMasterPath();
    wxFileName exe(probe.master, wxT("gcc.exe"));
    exe.AppendDir(wxT("bin"));
    probe.executable = exe.GetFullPath();
    if (!wxFileExists(probe.executable))
    {
        probe.error = wxT("gcc not found");
        return probe;
    }

    wxString versionText;
    long versionExit = -1;
    if (!RunGcc(probe.executable, wxT("-dumpversion"), versionText, versionExit) || versionExit != 0)
    {
        probe.error = wxT("gcc did not run");
        return probe;
    }
    probe.version = FirstLine(versionText);

    wxString machineText;
    long machineExit = -1;
    if (!RunGcc(probe.executable, wxT("-dumpmachine"), machineText, machineExit) || machineExit != 0)
    {
        probe.error = wxT("gcc did not report its target");
        return probe;
    }
    probe.machine = FirstLine(machineText);

    wxString verbose;
    long verboseExit = -1;
    if (RunGcc(probe.executable, wxT("-v"), verbose, verboseExit))
        probe.ucrt = verbose.Lower().Contains(wxT("ucrt"));

    probe.works = true;
    return probe;
}

wxString FormatWinSdkCompiler(const WinSdkCompiler& probe)
{
    return wxString::Format(
        wxT("access=%s\nmaster=%s\nexecutable=%s\nversion=%s\nmachine=%s\nucrt=%d\nerror=%s"),
        probe.works ? wxT("works") : wxT("not_present"),
        probe.master,
        probe.executable,
        probe.version,
        probe.machine,
        probe.ucrt ? 1 : 0,
        FirstLine(probe.error));
}

WinSdkTree QueryWinSdkTree(const wxString& libraryId)
{
    WinSdkTree tree;
    if (!LibraryIdOk(libraryId))
        return tree;

    ConfigManager* cfg = Manager::Get()->GetConfigManager(wxT("scripts"));
    const wxString saved = cfg ? cfg->Read(SavedKey(libraryId), wxEmptyString) : wxString();
    if (saved.empty() || !wxDirExists(saved))
    {
        tree.state = wxT("missing");
        return tree;
    }
    tree.state = wxT("installed");
    tree.prefix = saved;
    tree.nested = OneChildDir(saved);
    return tree;
}

wxString FormatWinSdkTree(const WinSdkTree& tree)
{
    return wxString::Format(
        wxT("state=%s\nprefix=%s\nnested=%s"),
        tree.state,
        tree.prefix,
        tree.nested);
}

WinSdkFetch FetchWinSdk(const wxString& url,
                        const wxString& libraryId,
                        const wxString& explanation,
                        const wxString& confirmLabel)
{
    WinSdkFetch result;
    if (!LibraryIdOk(libraryId))
    {
        result.output = wxT("invalid library");
        return result;
    }
    if (!url.StartsWith(wxT("https://")) || url.Contains(wxT(" ")) || url.Contains(wxT("\"")))
    {
        result.output = wxT("invalid address");
        return result;
    }
    const wxString root = SdkRoot();
    if (root.empty())
    {
        result.output = wxT("no local data directory");
        return result;
    }
    // Official Code::Blocks stays untouched.
    const wxString lowered = root.Lower();
    if (lowered.Contains(wxT("\\program files\\codeblocks")))
    {
        result.output = wxT("refusing the Code::Blocks install directory");
        return result;
    }

    if (!ConfirmFetch(explanation, confirmLabel))
        return result;

    const wxString dest = root + wxT("\\") + libraryId;
    wxFileName destName;
    destName.AssignDir(dest);
    if (!wxDirExists(dest) && !destName.Mkdir(wxS_DIR_DEFAULT, wxPATH_MKDIR_FULL))
    {
        result.output = wxT("could not create the library directory");
        return result;
    }

    FetchJob job;
    job.url = url;
    job.destDir = dest;
    job.zipPath = dest + wxT("\\_cb_download.zip");
    result.launched = true;

    FetchDialog dlg(Manager::Get()->GetAppWindow(), &job);
    dlg.ShowModal();

    result.cancelled = job.cancelled;
    result.success = job.success && !job.cancelled;
    result.prefix = result.success ? dest : wxString();
    if (!job.error.empty())
        result.output = wxString::FromUTF8(job.error.c_str());
    else if (!result.success)
        result.output = wxT("the download did not finish");

    if (result.success)
    {
        ConfigManager* cfg = Manager::Get()->GetConfigManager(wxT("scripts"));
        if (cfg)
            cfg->Write(SavedKey(libraryId), dest);
        else
        {
            result.success = false;
            result.prefix.Clear();
            result.output = wxT("could not remember the library directory");
        }
    }
    return result;
}

wxString FormatWinSdkFetch(const WinSdkFetch& result)
{
    return wxString::Format(
        wxT("launched=%d\ncancelled=%d\nsuccess=%d\nprefix=%s\noutput=%s"),
        result.launched ? 1 : 0,
        result.cancelled ? 1 : 0,
        result.success ? 1 : 0,
        result.prefix,
        FirstLine(result.output));
}

#endif // __WXMSW__
