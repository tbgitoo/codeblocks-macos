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
#include <wx/ffile.h>
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
#include <bcrypt.h>

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
    // Empty hash skips the check. SFML leaves both unset.
    wxString expectedSha256;
    bool checkEntries;

    FetchJob()
        : cancel(false),
          finished(false),
          percent(-1),
          success(false),
          cancelled(false),
          checkEntries(false)
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
            // The body is complete. Another availability query waits on a
            // keep-alive connection until the receive timeout.
            if (got >= contentLength)
                break;
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

bool ArchiveEntriesSafe(const wxString& tar, const wxString& zip, std::string& error);
bool VerifyDownload(FetchJob& job);

void RunFetch(FetchJob* job)
{
    if (DownloadHttps(*job) && job->error.empty() && !job->cancelled)
    {
        if (!job->expectedSha256.empty() || job->checkEntries)
        {
            job->SetStatus("Checking the download.");
            VerifyDownload(*job);
        }
    }
    job->finished.store(true);
}

bool Sha256Hex(const wxString& path, wxString& hex)
{
    BCRYPT_ALG_HANDLE alg = nullptr;
    if (BCryptOpenAlgorithmProvider(&alg, BCRYPT_SHA256_ALGORITHM, nullptr, 0) < 0)
        return false;
    DWORD objLen = 0;
    DWORD cb = 0;
    if (BCryptGetProperty(alg, BCRYPT_OBJECT_LENGTH, reinterpret_cast<PUCHAR>(&objLen),
                          sizeof(objLen), &cb, 0) < 0
        || objLen == 0 || objLen > 4096)
    {
        BCryptCloseAlgorithmProvider(alg, 0);
        return false;
    }
    std::vector<UCHAR> obj(objLen);
    BCRYPT_HASH_HANDLE hash = nullptr;
    if (BCryptCreateHash(alg, &hash, obj.data(), objLen, nullptr, 0, 0) < 0)
    {
        BCryptCloseAlgorithmProvider(alg, 0);
        return false;
    }
    FILE* file = _wfopen(path.wc_str(), L"rb");
    if (!file)
    {
        BCryptDestroyHash(hash);
        BCryptCloseAlgorithmProvider(alg, 0);
        return false;
    }
    std::vector<unsigned char> buf(65536);
    bool ok = true;
    while (ok)
    {
        const size_t n = fread(buf.data(), 1, buf.size(), file);
        if (n > 0 && BCryptHashData(hash, buf.data(), static_cast<ULONG>(n), 0) < 0)
            ok = false;
        if (n < buf.size())
            break;
    }
    if (ferror(file))
        ok = false;
    fclose(file);
    UCHAR digest[32];
    if (ok && BCryptFinishHash(hash, digest, sizeof(digest), 0) < 0)
        ok = false;
    BCryptDestroyHash(hash);
    BCryptCloseAlgorithmProvider(alg, 0);
    if (!ok)
        return false;
    hex.clear();
    for (size_t i = 0; i < sizeof(digest); ++i)
        hex += wxString::Format(wxT("%02x"), digest[i]);
    return true;
}

// A zip entry that is absolute, drive-qualified, or walks out with ".."
// is not unpacked. A blank line is ignored by the caller.
bool EntryNameSafe(const wxString& raw)
{
    wxString name = raw;
    name.Trim(true).Trim(false);
    if (name.empty())
        return true;
    if (name[0] == wxT('/') || name[0] == wxT('\\'))
        return false;
    if (name.Find(wxT(':')) != wxNOT_FOUND)
        return false;
    name.Replace(wxT("\\"), wxT("/"));
    wxString remaining = name;
    while (!remaining.empty())
    {
        const int slash = remaining.Find(wxT('/'));
        const wxString part = slash == wxNOT_FOUND ? remaining : remaining.Mid(0, slash);
        if (part == wxT(".."))
            return false;
        if (slash == wxNOT_FOUND)
            break;
        remaining = remaining.Mid(slash + 1);
    }
    return true;
}

// Runs on the download thread. wxExecute is not used here: a synchronous
// execute from the dialog's timer does not return while that dialog is modal.
bool ListArchive(const wxString& tar, const wxString& flag, const wxString& zip, wxArrayString& lines)
{
    lines.Clear();
    SECURITY_ATTRIBUTES inherit;
    memset(&inherit, 0, sizeof(inherit));
    inherit.nLength = sizeof(inherit);
    inherit.bInheritHandle = TRUE;
    HANDLE outRead = nullptr;
    HANDLE outWrite = nullptr;
    if (!CreatePipe(&outRead, &outWrite, &inherit, 0))
        return false;
    SetHandleInformation(outRead, HANDLE_FLAG_INHERIT, 0);

    STARTUPINFOW si;
    memset(&si, 0, sizeof(si));
    si.cb = sizeof(si);
    si.dwFlags = STARTF_USESTDHANDLES | STARTF_USESHOWWINDOW;
    si.wShowWindow = SW_HIDE;
    si.hStdOutput = outWrite;
    si.hStdError = outWrite;

    wxString command = wxT("\"") + tar + wxT("\" ") + flag + wxT(" \"") + zip + wxT("\"");
    std::wstring mutableCmd(command.wc_str());
    PROCESS_INFORMATION pi;
    memset(&pi, 0, sizeof(pi));
    const BOOL started = CreateProcessW(nullptr, &mutableCmd[0], nullptr, nullptr, TRUE,
                                        CREATE_NO_WINDOW, nullptr, nullptr, &si, &pi);
    CloseHandle(outWrite);
    if (!started)
    {
        CloseHandle(outRead);
        return false;
    }

    std::string bytes;
    char buf[8192];
    DWORD got = 0;
    while (ReadFile(outRead, buf, sizeof(buf), &got, nullptr) && got > 0)
        bytes.append(buf, buf + got);
    CloseHandle(outRead);

    const DWORD wait = WaitForSingleObject(pi.hProcess, 60000);
    DWORD code = 1;
    if (wait == WAIT_OBJECT_0)
        GetExitCodeProcess(pi.hProcess, &code);
    else
        TerminateProcess(pi.hProcess, 1);
    CloseHandle(pi.hProcess);
    CloseHandle(pi.hThread);
    if (wait != WAIT_OBJECT_0 || code != 0)
        return false;

    size_t start = 0;
    for (size_t i = 0; i <= bytes.size(); ++i)
    {
        if (i == bytes.size() || bytes[i] == '\n')
        {
            std::string line = bytes.substr(start, i - start);
            if (!line.empty() && line.back() == '\r')
                line.pop_back();
            if (!line.empty())
                lines.Add(wxString::FromUTF8(line.c_str()));
            start = i + 1;
        }
    }
    return true;
}

bool VerifyDownload(FetchJob& job)
{
    if (!job.expectedSha256.empty())
    {
        wxString got;
        if (!Sha256Hex(job.zipPath, got) || got.CmpNoCase(job.expectedSha256) != 0)
        {
            job.error = "the download did not match";
            return false;
        }
    }
    if (!job.checkEntries)
        return true;
    wxString sys;
    if (!wxGetEnv(wxT("SystemRoot"), &sys) || sys.empty())
    {
        job.error = "the archive could not be read";
        return false;
    }
    const wxString tar = sys + wxT("\\System32\\tar.exe");
    if (!wxFileExists(tar))
    {
        job.error = "the archive could not be read";
        return false;
    }
    std::string why;
    if (!ArchiveEntriesSafe(tar, job.zipPath, why))
    {
        job.error = why;
        return false;
    }
    return true;
}

// Names from tar -tf, types from tar -tvf. A link, or any type other than
// a file or directory, is rejected before unpack.
bool ArchiveEntriesSafe(const wxString& tar, const wxString& zip, std::string& error)
{
    wxArrayString names;
    wxArrayString verbose;
    if (!ListArchive(tar, wxT("-tf"), zip, names) || !ListArchive(tar, wxT("-tvf"), zip, verbose))
    {
        error = "the archive could not be read";
        return false;
    }
    int count = 0;
    for (size_t i = 0; i < names.GetCount(); ++i)
    {
        wxString name = names[i];
        name.Trim(true).Trim(false);
        if (name.empty())
            continue;
        ++count;
        if (!EntryNameSafe(name))
        {
            error = "the archive was rejected";
            return false;
        }
    }
    if (count == 0)
    {
        error = "the archive was rejected";
        return false;
    }
    for (size_t i = 0; i < verbose.GetCount(); ++i)
    {
        wxString line = verbose[i];
        line.Trim(false);
        if (line.empty())
            continue;
        const wxChar kind = line[0];
        if (kind != wxT('-') && kind != wxT('d'))
        {
            error = "the archive was rejected";
            return false;
        }
    }
    return true;
}

wxArrayString SplitMarkers(const wxString& text)
{
    wxArrayString out;
    wxString rest = text;
    while (!rest.empty())
    {
        const int semi = rest.Find(wxT(';'));
        wxString one = semi == -1 ? rest : rest.Mid(0, semi);
        one.Trim(true).Trim(false);
        if (!one.empty())
            out.Add(one);
        if (semi == -1)
            break;
        rest = rest.Mid(semi + 1);
    }
    return out;
}

bool DirHasMarkers(const wxString& root, const wxArrayString& markers)
{
    if (markers.IsEmpty() || !wxDirExists(root))
        return false;
    for (size_t i = 0; i < markers.GetCount(); ++i)
    {
        wxString rel = markers[i];
        rel.Replace(wxT("/"), wxT("\\"));
        while (rel.StartsWith(wxT("\\")))
            rel = rel.Mid(1);
        if (!wxFileExists(root + wxT("\\") + rel))
            return false;
    }
    return true;
}

wxString FindMarkedRoot(const wxString& unpack, const wxArrayString& markers)
{
    if (DirHasMarkers(unpack, markers))
        return unpack;
    wxDir dir(unpack);
    if (!dir.IsOpened())
        return wxString();
    wxString name;
    bool more = dir.GetFirst(&name, wxEmptyString, wxDIR_DIRS);
    while (more)
    {
        const wxString child = unpack + wxT("\\") + name;
        if (DirHasMarkers(child, markers))
            return child;
        more = dir.GetNext(&name);
    }
    return wxString();
}

bool CopyTree(const wxString& from, const wxString& to)
{
    wxFileName dest;
    dest.AssignDir(to);
    if (!wxDirExists(to) && !dest.Mkdir(wxS_DIR_DEFAULT, wxPATH_MKDIR_FULL))
        return false;
    wxDir dir(from);
    if (!dir.IsOpened())
        return false;
    wxString name;
    bool more = dir.GetFirst(&name, wxEmptyString, wxDIR_FILES | wxDIR_DIRS);
    while (more)
    {
        const wxString src = from + wxT("\\") + name;
        const wxString dst = to + wxT("\\") + name;
        if (wxDirExists(src))
        {
            if (!CopyTree(src, dst))
                return false;
        }
        else if (!wxCopyFile(src, dst, true))
            return false;
        more = dir.GetNext(&name);
    }
    return true;
}

bool Hex64(const wxString& text)
{
    if (text.length() != 64)
        return false;
    for (size_t i = 0; i < text.length(); ++i)
    {
        const wxChar c = text[i];
        const bool hex = (c >= wxT('0') && c <= wxT('9'))
                      || (c >= wxT('a') && c <= wxT('f'))
                      || (c >= wxT('A') && c <= wxT('F'));
        if (!hex)
            return false;
    }
    return true;
}

wxString NormalizedDir(const wxString& path)
{
    wxFileName name;
    name.AssignDir(path);
    name.Normalize(wxPATH_NORM_ABSOLUTE | wxPATH_NORM_DOTS | wxPATH_NORM_ENV_VARS | wxPATH_NORM_TILDE);
    wxString full = name.GetFullPath();
    if (!full.EndsWith(wxT("\\")) && !full.EndsWith(wxT("/")))
        full += wxT("\\");
    return full;
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

WinSdkFetch StageWinSdk(const wxString& url,
                        const wxString& sha256,
                        const wxString& libraryId,
                        const wxString& markersText,
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
    if (!Hex64(sha256))
    {
        result.output = wxT("invalid hash");
        return result;
    }
    const wxArrayString markers = SplitMarkers(markersText);
    if (markers.IsEmpty())
    {
        result.output = wxT("invalid markers");
        return result;
    }
    for (size_t i = 0; i < markers.GetCount(); ++i)
    {
        if (!EntryNameSafe(markers[i]) || markers[i].Find(wxT(':')) != wxNOT_FOUND)
        {
            result.output = wxT("invalid markers");
            return result;
        }
    }
    const wxString root = SdkRoot();
    if (root.empty())
    {
        result.output = wxT("no local data directory");
        return result;
    }
    if (root.Lower().Contains(wxT("\\program files\\codeblocks")))
    {
        result.output = wxT("refusing the Code::Blocks install directory");
        return result;
    }
    if (!ConfirmFetch(explanation, confirmLabel))
        return result;

    wxString stage;
    for (int n = 0; n < 100; ++n)
    {
        stage = root + wxT("\\_cb_stage\\") + libraryId + wxT("\\")
              + wxString::Format(wxT("%lld-%d"),
                                 static_cast<long long>(wxGetUTCTimeMillis().GetValue()), n);
        if (!wxDirExists(stage))
            break;
    }
    const wxString unpack = stage + wxT("\\unpack");
    wxFileName unpackName;
    unpackName.AssignDir(unpack);
    if (!unpackName.Mkdir(wxS_DIR_DEFAULT, wxPATH_MKDIR_FULL))
    {
        result.output = wxT("could not create the library directory");
        return result;
    }

    FetchJob job;
    job.url = url;
    job.destDir = unpack;
    job.zipPath = stage + wxT("\\_cb_download.zip");
    job.expectedSha256 = sha256;
    job.checkEntries = true;
    result.launched = true;

    FetchDialog dlg(Manager::Get()->GetAppWindow(), &job);
    dlg.ShowModal();

    result.cancelled = job.cancelled;
    result.success = job.success && !job.cancelled;
    if (!job.error.empty())
        result.output = wxString::FromUTF8(job.error.c_str());
    else if (!result.success)
        result.output = wxT("the download did not finish");
    if (!result.success)
        return result;

    const wxString marked = FindMarkedRoot(unpack, markers);
    if (marked.empty())
    {
        result.success = false;
        result.prefix.Clear();
        result.output = wxT("the archive did not contain the expected files");
        return result;
    }
    result.prefix = marked;
    return result;
}

WinSdkFetch PublishWinSdk(const wxString& libraryId,
                          const wxString& edition,
                          const wxString& stagingPrefix,
                          const wxString& note)
{
    WinSdkFetch result;
    if (!LibraryIdOk(libraryId) || !LibraryIdOk(edition))
    {
        result.output = wxT("invalid library");
        return result;
    }
    const wxString root = SdkRoot();
    if (root.empty())
    {
        result.output = wxT("no local data directory");
        return result;
    }
    if (root.Lower().Contains(wxT("\\program files\\codeblocks")))
    {
        result.output = wxT("refusing the Code::Blocks install directory");
        return result;
    }
    if (stagingPrefix.empty() || !wxDirExists(stagingPrefix))
    {
        result.output = wxT("the staged directory is missing");
        return result;
    }
    const wxString stageRoot = NormalizedDir(root + wxT("\\_cb_stage\\") + libraryId);
    const wxString staged = NormalizedDir(stagingPrefix);
    if (!staged.Lower().StartsWith(stageRoot.Lower()))
    {
        result.output = wxT("refusing a directory outside the staging area");
        return result;
    }
    const wxString dest = root + wxT("\\") + libraryId + wxT("\\") + edition;
    if (dest.Lower().Contains(wxT("\\program files\\codeblocks")))
    {
        result.output = wxT("refusing the Code::Blocks install directory");
        return result;
    }
    if (!CopyTree(stagingPrefix, dest))
    {
        result.output = wxT("could not publish the library directory");
        return result;
    }

    wxFFile record(dest + wxT("\\_cb_provenance.txt"), wxT("w"));
    const wxString body = note + wxT("\nroot=") + dest + wxT("\n");
    if (!record.IsOpened() || !record.Write(body, wxConvUTF8))
    {
        result.output = wxT("could not record the library directory");
        return result;
    }
    record.Close();

    ConfigManager* cfg = Manager::Get()->GetConfigManager(wxT("scripts"));
    if (!cfg)
    {
        result.output = wxT("could not remember the library directory");
        return result;
    }
    cfg->Write(SavedKey(libraryId), dest);
    result.success = true;
    result.prefix = dest;
    return result;
}

#endif // __WXMSW__
