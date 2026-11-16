/*
 * This file is part of the Code::Blocks IDE and licensed under the GNU General Public License, version 3
 * http://www.gnu.org/licenses/gpl-3.0.html
 *
 * v3.6b. Resolve the Code::Blocks-owned FLTK source/build/install paths
 * and configure, build, and install that one static MinGW library.
 *
 * This is deliberately not a general CMake command API.
 * Option names are those of FLTK 1.4.5 CMake/options.cmake.
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
#include <wx/stdpaths.h>
#include <wx/textctrl.h>
#include <wx/timer.h>
#include <wx/utils.h>

#include "wincmake.h"
#include "winsdk.h"

#if defined(__WXMSW__)

#define WIN32_LEAN_AND_MEAN
#include <windows.h>

namespace
{

const wxString kSavedExecutableKey = wxT("/windows/cmake/executable");
const int kMaxJobs = 8;

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

wxString ReadFile(const wxString& path)
{
    wxFile file(path);

    if (!file.IsOpened())
        return wxString();

    wxString text;
    char buf[4096];

    while (!file.Eof())
    {
        const ssize_t n = file.Read(buf, sizeof(buf));

        if (n <= 0)
            break;

        const wxString chunk = wxString::FromUTF8(buf, n);

        if (chunk.empty())
            text += wxString(buf, wxConvISO8859_1, n);
        else
            text += chunk;
    }

    return text;
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
    wxArrayString lines;

    // The final make line only says Error 2. Keep the recipe error above it.
    while (!trimmed.empty() && lines.GetCount() < 6)
    {
        const int nl = trimmed.Find(wxT('\n'), true);
        wxString line = nl == wxNOT_FOUND ? trimmed : trimmed.Mid(nl + 1);
        line.Trim(true).Trim(false);

        if (nl == wxNOT_FOUND)
            trimmed.clear();
        else
            trimmed = trimmed.Mid(0, nl);

        if (!line.empty())
            lines.Insert(line, 0);
    }

    // The script keeps one line of this field. Prefer the CMake or
    // compiler error over the feature summary that follows it.
    for (int i = static_cast<int>(lines.GetCount()) - 1; i >= 0; --i)
    {
        const wxString& line = lines[static_cast<size_t>(i)];

        if (line.Contains(wxT("CMake Error"))
            || line.Contains(wxT("Cannot find source"))
            || line.Contains(wxT("fatal error"))
            || line.Contains(wxT("error:"))
            || line.Contains(wxT("Error ")))
        {
            if (line.Length() > 500)
                return line.Mid(line.Length() - 500);

            return line;
        }
    }

    if (lines.IsEmpty())
        return wxString();

    const wxString& last = lines.Last();

    if (last.Length() > 500)
        return last.Mid(last.Length() - 500);

    return last;
}

wxString StripTrailingSlash(wxString path)
{
    while (path.Length() > 3
           && (path.EndsWith(wxT("\\")) || path.EndsWith(wxT("/"))))
    {
        path.RemoveLast();
    }

    return path;
}

wxString Canonical(wxString path)
{
    path.Replace(wxT("/"), wxT("\\"));
    path = StripTrailingSlash(path);
    return path.Lower();
}

// Follow a directory symlink or junction for the longest prefix that exists,
// then keep the not-yet-created tail. %LOCALAPPDATA%\codeblocks can point at
// another drive and still compare equal to the path CMake records.
wxString ResolvePath(wxString path)
{
    path.Replace(wxT("/"), wxT("\\"));
    path = StripTrailingSlash(path);

    wxString missing;

    while (path.Length() > 3
           && !wxFileExists(path)
           && !wxDirExists(path))
    {
        const int slash = path.Find(wxT('\\'), true);

        if (slash == wxNOT_FOUND || slash < 2)
            break;

        missing = path.Mid(slash) + missing;
        path = path.Mid(0, slash);
    }

    if (wxFileExists(path) || wxDirExists(path))
    {
        const HANDLE handle = CreateFileW(
            path.wc_str(),
            0,
            FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
            nullptr,
            OPEN_EXISTING,
            FILE_FLAG_BACKUP_SEMANTICS,
            nullptr);

        if (handle != INVALID_HANDLE_VALUE)
        {
            wchar_t resolved[32768];
            const DWORD length = GetFinalPathNameByHandleW(
                handle,
                resolved,
                WXSIZEOF(resolved),
                FILE_NAME_NORMALIZED);
            CloseHandle(handle);

            if (length > 0 && length < WXSIZEOF(resolved))
            {
                path = wxString(resolved, length);

                if (path.StartsWith(wxT("\\\\?\\UNC\\")))
                    path = wxT("\\\\") + path.Mid(8);
                else if (path.StartsWith(wxT("\\\\?\\")))
                    path = path.Mid(4);
            }
        }
    }

    return Canonical(path + missing);
}

bool SamePath(const wxString& left, const wxString& right)
{
    return ResolvePath(left) == ResolvePath(right);
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

bool LeafNameOk(const wxString& leaf)
{
    if (leaf.empty() || leaf == wxT(".") || leaf == wxT(".."))
        return false;

    for (size_t i = 0; i < leaf.Length(); ++i)
    {
        const wxChar c = leaf[i];

        if (c < wxT(' ')
            || c == wxT('\x7f')
            || c == wxT('/')
            || c == wxT('\\')
            || c == wxT(':'))
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

    if (jobs > kMaxJobs)
        jobs = kMaxJobs;

    return jobs;
}

bool IsDriveRoot(const wxString& path)
{
    wxFileName name = wxFileName::DirName(path);
    return name.GetDirCount() == 0;
}

bool PathIsUnder(const wxString& path, const wxString& root)
{
    const wxString child = ResolvePath(path);
    const wxString parent = ResolvePath(root);

    return child == parent || child.StartsWith(parent + wxT("\\"));
}

bool ForbiddenLocation(const wxString& path, wxString& error)
{
    if (IsDriveRoot(path))
    {
        error = wxT("path is a drive root");
        return true;
    }

    wxString programFiles;

    if (wxGetEnv(wxT("ProgramFiles"), &programFiles)
        && !programFiles.empty()
        && PathIsUnder(path, programFiles))
    {
        error = wxT("path is under Program Files");
        return true;
    }

    wxString programFilesX86;

    if (wxGetEnv(wxT("ProgramFiles(x86)"), &programFilesX86)
        && !programFilesX86.empty()
        && PathIsUnder(path, programFilesX86))
    {
        error = wxT("path is under Program Files");
        return true;
    }

    const wxFileName executable(wxStandardPaths::Get().GetExecutablePath());
    const wxString executableDir = executable.GetPath();

    if (!executableDir.empty() && PathIsUnder(path, executableDir))
    {
        error = wxT("path is beside codeblocks.exe");
        return true;
    }

    return false;
}

wxString CodeBlocksRoot()
{
    wxString local;

    if (!wxGetEnv(wxT("LOCALAPPDATA"), &local) || local.empty())
        return wxString();

    wxFileName root = wxFileName::DirName(local);
    root.AppendDir(wxT("codeblocks"));
    return StripTrailingSlash(root.GetFullPath());
}

bool NormaliseAbsoluteDirectory(const wxString& input, wxString& output)
{
    if (input.empty() || HasControlCharacters(input))
        return false;

    wxFileName path = wxFileName::DirName(input);

    if (!path.IsAbsolute())
        return false;

    path.Normalize(
        wxPATH_NORM_DOTS | wxPATH_NORM_ABSOLUTE,
        wxEmptyString,
        wxPATH_NATIVE);

    output = StripTrailingSlash(path.GetFullPath());
    return !output.empty();
}

void ApplyVersion(WinCMakeProbe& probe)
{
    wxString versionError;

    if (!RunVersion(
            probe.executable,
            probe.versionExit,
            probe.versionOutput,
            versionError))
    {
        probe.access = WinCMakePresentUnusable;
        probe.errorOutput = versionError;
        return;
    }

    probe.errorOutput = versionError;

    const wxString version = FirstLine(probe.versionOutput);

    if (probe.versionExit != 0
        || !version.Lower().StartsWith(wxT("cmake version")))
    {
        probe.access = WinCMakePresentUnusable;
        return;
    }

    probe.access = WinCMakeWorks;
}

wxString ProgramFilesCMake()
{
    wxString programFiles;

    if (!wxGetEnv(wxT("ProgramFiles"), &programFiles) || programFiles.empty())
        return wxString();

    wxFileName file(programFiles, wxT("cmake.exe"));
    file.AppendDir(wxT("CMake"));
    file.AppendDir(wxT("bin"));
    return file.GetFullPath();
}

bool ConfirmAction(wxWindow* parent,
                   const wxString& explanation,
                   const wxString& confirmLabel)
{
    wxDialog dlg(
        parent,
        wxID_ANY,
        _("CMake"),
        wxDefaultPosition,
        wxDefaultSize,
        wxDEFAULT_DIALOG_STYLE | wxRESIZE_BORDER);

    wxBoxSizer* outer = new wxBoxSizer(wxVERTICAL);
    wxStaticText* text = new wxStaticText(&dlg, wxID_ANY, explanation);
    text->Wrap(520);
    outer->Add(text, 1, wxALL | wxEXPAND, 12);

    wxBoxSizer* buttons = new wxBoxSizer(wxHORIZONTAL);
    buttons->AddStretchSpacer();
    buttons->Add(new wxButton(&dlg, wxID_OK, confirmLabel), 0, wxRIGHT, 8);
    buttons->Add(new wxButton(&dlg, wxID_CANCEL, _("Cancel")), 0);
    outer->Add(buttons, 0, wxLEFT | wxRIGHT | wxBOTTOM | wxEXPAND, 12);

    dlg.SetSizer(outer);
    dlg.SetInitialSize(wxSize(560, -1));
    dlg.CentreOnParent();

    return dlg.ShowModal() == wxID_OK;
}

void PrependCompilerBin(wxExecuteEnv& env, const wxString& bin)
{
    bool found = false;

    for (wxEnvVariableHashMap::iterator it = env.env.begin();
         it != env.env.end();
         ++it)
    {
        if (it->first.CmpNoCase(wxT("PATH")) == 0)
        {
            it->second = bin + wxT(";") + it->second;
            found = true;
            break;
        }
    }

    if (!found)
        env.env[wxT("PATH")] = bin;
}

class WinCMakeRunDialog : public wxDialog
{
public:
    WinCMakeRunDialog(wxWindow* parent, const wxString& summary)
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
        wxStaticText* label = new wxStaticText(this, wxID_ANY, summary);
        label->Wrap(640);
        outer->Add(label, 0, wxALL | wxEXPAND, 12);

        m_text = new wxTextCtrl(
            this,
            wxID_ANY,
            wxEmptyString,
            wxDefaultPosition,
            wxSize(700, 320),
            wxTE_MULTILINE | wxTE_READONLY);

        outer->Add(m_text, 1, wxLEFT | wxRIGHT | wxEXPAND, 12);
        outer->Add(
            new wxButton(this, wxID_CANCEL, _("Cancel")),
            0,
            wxALL | wxALIGN_RIGHT,
            12);

        SetSizer(outer);
        SetInitialSize(wxSize(740, 520));
        CentreOnParent();

        Bind(wxEVT_TIMER, &WinCMakeRunDialog::OnTimer, this);
        Bind(wxEVT_END_PROCESS, &WinCMakeRunDialog::OnProcessEnd, this);
        Bind(wxEVT_BUTTON, &WinCMakeRunDialog::OnCancel, this, wxID_CANCEL);
        Bind(wxEVT_CLOSE_WINDOW, &WinCMakeRunDialog::OnCloseWindow, this);
    }

    ~WinCMakeRunDialog() override
    {
        m_timer.Stop();
        delete m_process;
    }

    bool Start(const wxArrayString& args,
               const wxString& compilerBin,
               const wxString& pkgConfigLibDir = wxEmptyString)
    {
        m_args = args;

        if (m_args.IsEmpty())
        {
            Append(wxT("CMake was not given a command.\n"));
            return false;
        }

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

        if (!compilerBin.empty())
            PrependCompilerBin(env, compilerBin);

        if (!pkgConfigLibDir.empty())
        {
            env.env[wxT("PKG_CONFIG_PATH")] = pkgConfigLibDir;
            env.env[wxT("PKG_CONFIG_LIBDIR")] = pkgConfigLibDir;
        }

        // wxExecute needs a null-terminated argv. wxWidgets 3.0 takes wchar_t**.
        // The strings stay in m_args and are not modified.
        wxChar** argv = new wxChar*[m_args.GetCount() + 1];

        for (size_t i = 0; i < m_args.GetCount(); ++i)
            argv[i] = const_cast<wxChar*>(m_args[i].wx_str());

        argv[m_args.GetCount()] = nullptr;

        m_pid = wxExecute(argv, wxEXEC_ASYNC, m_process, &env);
        delete [] argv;

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

        ReadStream(m_process->GetInputStream(), m_process->IsInputAvailable());

        if (m_process)
            ReadStream(m_process->GetErrorStream(), m_process->IsErrorAvailable());
    }

    void StopChild()
    {
        if (m_pid <= 0)
            return;

        wxKill(m_pid, wxSIGTERM, NULL, wxKILL_NOCHILDREN);
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
                wxT("\nCancel requested. Stopping the CMake process "
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
                    wxT("\nCancel requested. Stopping the CMake process "
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
    wxArrayString m_args;
    wxString m_all;
    long m_pid;
    long m_exit;
    bool m_cancelled;
    bool m_finished;
    bool m_closed;
    wxTimer m_timer;
};

wxString ParentDirectory(const wxString& path)
{
    wxFileName name = wxFileName::DirName(path);
    name.RemoveLastDir();
    return StripTrailingSlash(name.GetFullPath());
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

    if (!NormaliseAbsoluteDirectory(sourceInput, source)
        || !NormaliseAbsoluteDirectory(buildInput, build)
        || !NormaliseAbsoluteDirectory(installInput, install))
    {
        error = wxT("source, build, and install directories must be absolute");
        return false;
    }

    const wxString root = CodeBlocksRoot();

    if (root.empty() || !SamePath(ParentDirectory(source), root)
        || !SamePath(ParentDirectory(install), root))
    {
        error = wxT("source and install must sit in the Code::Blocks directory");
        return false;
    }

    wxString forbidden;

    if (ForbiddenLocation(source, forbidden)
        || ForbiddenLocation(build, forbidden)
        || ForbiddenLocation(install, forbidden))
    {
        error = forbidden;
        return false;
    }

    if (SamePath(source, build) || SamePath(source, install) || SamePath(build, install))
    {
        error = wxT("source, build, and install directories must be distinct");
        return false;
    }

    wxFileName expectedBuild = wxFileName::DirName(source);
    expectedBuild.AppendDir(wxT("build-codeblocks"));

    if (!SamePath(build, expectedBuild.GetFullPath()))
    {
        error = wxT("build directory is not source/build-codeblocks");
        return false;
    }

    if (!wxFileExists(source + wxT("\\CMakeLists.txt")))
    {
        error = wxT("source/CMakeLists.txt was not found");
        return false;
    }

    if (wxFileExists(build) || wxFileExists(install))
    {
        error = wxT("build or install path is an existing file");
        return false;
    }

    return true;
}

bool ValidateBuildDirectory(const wxString& buildInput, wxString& build, wxString& error)
{
    if (!NormaliseAbsoluteDirectory(buildInput, build))
    {
        error = wxT("build directory is not absolute");
        return false;
    }

    if (!wxDirExists(build))
    {
        error = wxT("build directory does not exist");
        return false;
    }

    if (!wxFileExists(build + wxT("\\CMakeCache.txt")))
    {
        error = wxT("build/CMakeCache.txt was not found");
        return false;
    }

    wxFileName buildPath = wxFileName::DirName(build);

    if (buildPath.GetDirs().IsEmpty()
        || buildPath.GetDirs().Last() != wxT("build-codeblocks"))
    {
        error = wxT("build directory is not named build-codeblocks");
        return false;
    }

    return true;
}

// #define NAME value, allowing extra spaces. value runs up to whitespace.
bool DefineEquals(const wxString& text, const wxString& name, const wxString& value)
{
    wxString remaining = text;

    while (!remaining.empty())
    {
        wxString line = remaining.BeforeFirst(wxT('\n'));
        remaining = remaining.AfterFirst(wxT('\n'));

        if (line.EndsWith(wxT("\r")))
            line.RemoveLast();

        line.Trim(true).Trim(false);

        if (!line.StartsWith(wxT("#define")))
            continue;

        line = line.Mid(wxString(wxT("#define")).Length());
        line.Trim(false);

        if (line.Length() <= name.Length() || !line.StartsWith(name))
            continue;

        const wxChar separator = line[name.Length()];

        if (separator != wxT(' ') && separator != wxT('\t'))
            continue;

        wxString rest = line.Mid(name.Length());
        rest.Trim(false);

        if (rest == value
            || rest.StartsWith(value + wxT(" "))
            || rest.StartsWith(value + wxT("\t"))
            || rest.StartsWith(value + wxT("/")))
        {
            return true;
        }
    }

    return false;
}

// CMake records the same variable as FILEPATH or STRING. Match the name,
// then the value, whichever type the cache used.
bool CacheVariableIsPath(const wxString& cache, const wxString& name, const wxString& expected)
{
    wxString remaining = cache;
    const wxString prefix = name + wxT(":");

    while (!remaining.empty())
    {
        wxString line = remaining.BeforeFirst(wxT('\n'));
        remaining = remaining.AfterFirst(wxT('\n'));

        if (line.EndsWith(wxT("\r")))
            line.RemoveLast();

        if (!line.StartsWith(prefix))
            continue;

        const int equals = line.Find(wxT('='));

        if (equals == wxNOT_FOUND)
            continue;

        wxString value = line.Mid(equals + 1);
        value.Trim(true).Trim(false);

        if (SamePath(value, expected))
            return true;
    }

    return false;
}

bool InstalledContract(const wxString& install, wxString& error)
{
    const wxString header = install + wxT("\\include\\FL\\Fl.H");
    const wxString version = install + wxT("\\include\\FL\\Enumerations.H");
    const wxString library = install + wxT("\\lib\\libfltk.a");

    if (!wxFileExists(header))
    {
        error = wxT("include/FL/Fl.H was not found");
        return false;
    }

    if (!wxFileExists(library))
    {
        error = wxT("lib/libfltk.a was not found");
        return false;
    }

    const wxString text = ReadFile(version);

    if (text.empty())
    {
        error = wxT("include/FL/Enumerations.H was not found");
        return false;
    }

    if (!DefineEquals(text, wxT("FL_MAJOR_VERSION"), wxT("1"))
        || !DefineEquals(text, wxT("FL_MINOR_VERSION"), wxT("4"))
        || !DefineEquals(text, wxT("FL_PATCH_VERSION"), wxT("5")))
    {
        error = wxT("installed FLTK header is not 1.4.5");
        return false;
    }

    return true;
}

bool MingwTools(const wxString& compilerId,
                wxString& gcc,
                wxString& gxx,
                wxString& make,
                wxString& bin,
                wxString& error)
{
    const WinSdkCompiler compiler = ProbeWinSdkCompiler(compilerId);

    if (!compiler.works)
    {
        error = wxT("compiler unavailable");

        if (!compiler.error.empty())
            error += wxT(": ") + FirstLine(compiler.error);

        return false;
    }

    if (compiler.machine != wxT("x86_64-w64-mingw32"))
    {
        error = wxT("compiler is not x86_64-w64-mingw32");
        return false;
    }

    gcc = compiler.executable;
    wxFileName gccFile(gcc);

    if (!gccFile.IsAbsolute() || !wxFileExists(gcc))
    {
        error = wxT("gcc.exe was not found");
        return false;
    }

    bin = gccFile.GetPath();
    gxx = bin + wxT("\\g++.exe");
    make = bin + wxT("\\mingw32-make.exe");

    if (!wxFileExists(gxx) || !wxFileExists(make))
    {
        error = wxT("g++.exe or mingw32-make.exe is not beside gcc.exe");
        return false;
    }

    return true;
}

WinCMakeRunResult RunCMake(
    wxWindow* parent,
    const wxString& operation,
    const wxArrayString& args,
    const wxString& compilerBin,
    const wxString& summary,
    const wxString& explanation,
    const wxString& confirmLabel,
    const wxString& pkgConfigLibDir = wxEmptyString)
{
    WinCMakeRunResult result;
    result.operation = operation;

    const WinCMakeProbe probe = ProbeWinCMake();

    if (probe.access != WinCMakeWorks)
    {
        result.output = wxT("cmake unavailable");
        return result;
    }

    if (!confirmLabel.empty() && !ConfirmAction(parent, explanation, confirmLabel))
        return result;

    wxArrayString command;
    command.Add(probe.executable);

    for (size_t i = 0; i < args.GetCount(); ++i)
        command.Add(args[i]);

    WinCMakeRunDialog dlg(parent, summary);

    if (!dlg.Start(command, compilerBin, pkgConfigLibDir))
    {
        result.output = dlg.OutputTail();
        return result;
    }

    dlg.ShowModal();

    result.launched = true;
    result.cancelled = dlg.Cancelled();
    result.exitCode = dlg.ExitCode();
    result.output = dlg.OutputTail();
    result.success = result.launched && !result.cancelled && result.exitCode == 0;
    return result;
}

} // namespace

WinCMakeProbe ProbeWinCMake()
{
    WinCMakeProbe probe;

    ConfigManager* cfg = Manager::Get()->GetConfigManager(wxT("scripts"));
    const wxString saved =
        cfg ? cfg->Read(kSavedExecutableKey, wxEmptyString) : wxString();

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
        const wxString installed = ProgramFilesCMake();

        if (!installed.empty() && wxFileExists(installed))
            candidate = installed;
    }

    if (candidate.empty())
        return probe;

    probe.executable = candidate;
    ApplyVersion(probe);
    return probe;
}

WinCMakeProbe ProbeWinCMakePath(const wxString& executable)
{
    WinCMakeProbe probe;
    probe.executable = executable;

    if (executable.empty() || !wxFileExists(executable))
    {
        probe.errorOutput = wxT("file not found");
        return probe;
    }

    ApplyVersion(probe);
    return probe;
}

wxString FormatWinCMakeProbe(const WinCMakeProbe& probe)
{
    const wxString access =
        probe.access == WinCMakeWorks
            ? wxT("works")
            : (probe.access == WinCMakePresentUnusable
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

WinCMakePaths ResolveWinCMakePaths(
    const wxString& sourceLeaf,
    const wxString& installLeaf)
{
    WinCMakePaths paths;

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

    paths.root = CodeBlocksRoot();

    if (paths.root.empty())
    {
        paths.errorOutput = wxT("LOCALAPPDATA is empty");
        return paths;
    }

    wxString forbidden;

    if (ForbiddenLocation(paths.root, forbidden))
    {
        paths.errorOutput = forbidden;
        return paths;
    }

    if (!wxDirExists(paths.root))
    {
        wxFileName root = wxFileName::DirName(paths.root);

        if (!root.Mkdir(wxS_DIR_DEFAULT, wxPATH_MKDIR_FULL))
        {
            paths.errorOutput = wxT("could not create the Code::Blocks directory");
            return paths;
        }
    }

    paths.source = paths.root + wxT("\\") + sourceLeaf;
    paths.build = paths.source + wxT("\\build-codeblocks");
    paths.install = paths.root + wxT("\\") + installLeaf;

    if (SamePath(paths.source, paths.install)
        || SamePath(paths.build, paths.install))
    {
        paths.errorOutput = wxT("resolved paths overlap");
        return paths;
    }

    paths.valid = true;
    return paths;
}

wxString FormatWinCMakePaths(const WinCMakePaths& paths)
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

WinCMakeRunResult ConfigureWinCMake(
    wxWindow* parent,
    const wxString& compilerId,
    const wxString& sourceInput,
    const wxString& buildInput,
    const wxString& installInput,
    const wxString& buildType,
    const wxString& explanation,
    const wxString& confirmLabel)
{
    WinCMakeRunResult result;
    result.operation = wxT("configure");

    wxString source;
    wxString build;
    wxString install;
    wxString error;

    if (!ValidateConfigureInputs(
            sourceInput,
            buildInput,
            installInput,
            buildType,
            source,
            build,
            install,
            error))
    {
        result.output = error;
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

    wxArrayString args;
    args.Add(wxT("-S"));
    args.Add(source);
    args.Add(wxT("-B"));
    args.Add(build);
    args.Add(wxT("-G"));
    args.Add(wxT("MinGW Makefiles"));
    args.Add(wxT("-DCMAKE_BUILD_TYPE=") + buildType);
    args.Add(wxT("-DCMAKE_INSTALL_PREFIX=") + install);
    args.Add(wxT("-DCMAKE_C_COMPILER=") + gcc);
    args.Add(wxT("-DCMAKE_CXX_COMPILER=") + gxx);
    args.Add(wxT("-DCMAKE_MAKE_PROGRAM=") + make);
    args.Add(wxT("-DFLTK_BUILD_SHARED_LIBS=OFF"));
    args.Add(wxT("-DFLTK_BUILD_TEST=OFF"));
    args.Add(wxT("-DFLTK_BUILD_FLUID=OFF"));
    args.Add(wxT("-DFLTK_BUILD_EXAMPLES=OFF"));
    args.Add(wxT("-DFLTK_BUILD_FLTK_OPTIONS=OFF"));
    args.Add(wxT("-DFLTK_BUILD_FORMS=OFF"));
    args.Add(wxT("-DFLTK_BUILD_GL=OFF"));

    const wxString summary = wxString::Format(
        _("CMake is configuring FLTK.\n\nSource:\n%s\n\nBuild:\n%s\n\nInstall:\n%s"),
        source,
        build,
        install);

    result = RunCMake(
        parent,
        wxT("configure"),
        args,
        bin,
        summary,
        explanation,
        confirmLabel);

    if (!result.success)
        return result;

    const wxString cache = ReadFile(build + wxT("\\CMakeCache.txt"));

    if (cache.empty()
        || !CacheVariableIsPath(cache, wxT("CMAKE_INSTALL_PREFIX"), install)
        || !CacheVariableIsPath(cache, wxT("CMAKE_C_COMPILER"), gcc))
    {
        result.success = false;
        result.output = wxT("CMake finished, but the cache does not record this install prefix and gcc");
    }

    return result;
}

WinCMakeRunResult BuildWinCMake(
    wxWindow* parent,
    const wxString& buildInput,
    const wxString& explanation,
    const wxString& confirmLabel)
{
    WinCMakeRunResult result;
    result.operation = wxT("build");

    wxString build;
    wxString error;

    if (!ValidateBuildDirectory(buildInput, build, error))
    {
        result.output = error;
        return result;
    }

    const int jobs = BoundedJobCount();
    wxArrayString args;
    args.Add(wxT("--build"));
    args.Add(build);
    args.Add(wxT("--parallel"));
    args.Add(wxString::Format(wxT("%d"), jobs));

    const wxString summary = wxString::Format(
        _("CMake is building FLTK.\n\nBuild:\n%s\n\nParallel jobs: %d"),
        build,
        jobs);

    result = RunCMake(
        parent,
        wxT("build"),
        args,
        wxEmptyString,
        summary,
        explanation,
        confirmLabel);

    if (!result.success)
        return result;

    if (!wxFileExists(build + wxT("\\lib\\libfltk.a")))
    {
        result.success = false;
        result.output = wxT("CMake finished, but lib/libfltk.a was not found");
    }

    return result;
}

WinCMakeRunResult InstallWinCMake(
    wxWindow* parent,
    const wxString& buildInput,
    const wxString& installInput,
    const wxString& explanation,
    const wxString& confirmLabel)
{
    WinCMakeRunResult result;
    result.operation = wxT("install");

    wxString build;
    wxString install;
    wxString error;

    if (!ValidateBuildDirectory(buildInput, build, error)
        || !NormaliseAbsoluteDirectory(installInput, install))
    {
        result.output = error.empty() ? wxString(wxT("install directory is not absolute")) : error;
        return result;
    }

    const wxString cache = ReadFile(build + wxT("\\CMakeCache.txt"));

    if (!CacheVariableIsPath(cache, wxT("CMAKE_INSTALL_PREFIX"), install))
    {
        result.output = wxT("install directory is not the prefix fixed at configure time");
        return result;
    }

    wxArrayString args;
    args.Add(wxT("--install"));
    args.Add(build);

    const wxString summary = wxString::Format(
        _("CMake is installing FLTK.\n\nBuild:\n%s\n\nInstall:\n%s"),
        build,
        install);

    result = RunCMake(
        parent,
        wxT("install"),
        args,
        wxEmptyString,
        summary,
        explanation,
        confirmLabel);

    if (!result.success)
        return result;

    if (!InstalledContract(install, error))
    {
        result.success = false;
        result.output = error;
    }

    return result;
}

namespace
{

wxString JoinUnder(const wxString& root, const wxString& relative)
{
    wxString path = root;

    if (!path.EndsWith(wxT("\\")) && !path.EndsWith(wxT("/")))
        path += wxT("\\");

    wxString rest = relative;
    rest.Replace(wxT("/"), wxT("\\"));
    return path + rest;
}

bool FirstExistingFile(
    const wxString& root,
    const wxChar* const* relatives,
    wxString& found)
{
    for (size_t i = 0; relatives[i] != nullptr; ++i)
    {
        const wxString path = JoinUnder(root, relatives[i]);

        if (wxFileExists(path))
        {
            found = path;
            return true;
        }
    }

    return false;
}

bool CacheVariableEquals(
    const wxString& cache,
    const wxString& name,
    const wxString& expected)
{
    wxString remaining = cache;
    const wxString prefix = name + wxT(":");

    while (!remaining.empty())
    {
        wxString line = remaining.BeforeFirst(wxT('\n'));
        remaining = remaining.AfterFirst(wxT('\n'));

        if (line.EndsWith(wxT("\r")))
            line.RemoveLast();

        if (!line.StartsWith(prefix))
            continue;

        const int equals = line.Find(wxT('='));

        if (equals == wxNOT_FOUND)
            continue;

        wxString value = line.Mid(equals + 1);
        value.Trim(true).Trim(false);
        return value == expected;
    }

    return false;
}

// A CMakeLists.txt newer than CMakeCache.txt means configure has not seen
// the current source, including a compatibility patch applied after the
// last configure.
bool ListsNewerThanCache(const wxString& build)
{
    const wxString listsPath = ParentDirectory(build) + wxT("\\CMakeLists.txt");
    const wxString cachePath = build + wxT("\\CMakeCache.txt");

    if (!wxFileExists(listsPath) || !wxFileExists(cachePath))
        return false;

    const wxDateTime listsTime = wxFileName(listsPath).GetModificationTime();
    const wxDateTime cacheTime = wxFileName(cachePath).GetModificationTime();

    if (!listsTime.IsValid() || !cacheTime.IsValid())
        return false;

    return listsTime > cacheTime;
}

bool GeneratedMakefile(const wxString& build)
{
    return wxFileExists(build + wxT("\\Makefile"));
}

// A cache that does not mention the option is accepted. OGRE's failed
// configure records the option as ON, and that result is not reusable.
bool CacheOptionAccepts(const wxString& cache,
                        const wxString& name,
                        const wxString& expected)
{
    const wxString prefix = name + wxT(":");

    if (!cache.Contains(prefix))
        return true;

    return CacheVariableEquals(cache, name, expected);
}

bool FindOnPath(const wxString& fileName, wxString& found)
{
    wxString path;

    if (!wxGetEnv(wxT("PATH"), &path) || path.empty())
        return false;

    const wxArrayString dirs = wxSplit(path, wxT(';'));

    for (size_t i = 0; i < dirs.GetCount(); ++i)
    {
        wxString dir = dirs[i];
        dir.Trim(true).Trim(false);

        if (dir.empty())
            continue;

        const wxString candidate = JoinUnder(dir, fileName);

        if (wxFileExists(candidate))
        {
            found = candidate;
            return true;
        }
    }

    return false;
}

bool ToolRuns(const wxString& executable)
{
    long exitCode = -1;
    wxString output;
    wxString error;
    return RunVersion(executable, exitCode, output, error) && exitCode == 0;
}

struct OgreLayout
{
    bool valid;
    wxString pluginDir;
    wxString libDir;
    wxString runtimeDir;
    wxString media;
    wxString cmake;
    wxString header;
    wxString ogreMainDll;
    wxString error;

    OgreLayout()
        : valid(false)
    {}
};

OgreLayout InspectOgreTree(const wxString& root, bool installedContract)
{
    OgreLayout layout;

    wxString normalised;

    if (!NormaliseAbsoluteDirectory(root, normalised))
    {
        layout.error = wxT("directory is not absolute");
        return layout;
    }

    static const wxChar* pluginFiles[] = {
        wxT("bin\\RenderSystem_GL.dll"),
        wxT("bin\\Release\\RenderSystem_GL.dll"),
        wxT("lib\\OGRE\\RenderSystem_GL.dll"),
        wxT("lib\\Release\\RenderSystem_GL.dll"),
        wxT("lib\\RenderSystem_GL.dll"),
        nullptr
    };

    static const wxChar* importLibs[] = {
        wxT("lib\\libOgreMain.dll.a"),
        wxT("lib\\Release\\libOgreMain.dll.a"),
        wxT("lib\\OGRE\\libOgreMain.dll.a"),
        nullptr
    };

    static const wxChar* runtimeDlls[] = {
        wxT("bin\\OgreMain.dll"),
        wxT("bin\\Release\\OgreMain.dll"),
        wxT("lib\\OGRE\\OgreMain.dll"),
        wxT("lib\\Release\\OgreMain.dll"),
        wxT("lib\\OgreMain.dll"),
        nullptr
    };

    wxString pluginFile;

    if (!FirstExistingFile(normalised, pluginFiles, pluginFile))
    {
        layout.error = wxT("RenderSystem_GL.dll was not found");
        return layout;
    }

    layout.pluginDir = wxFileName(pluginFile).GetPath();

    wxString importLib;

    if (FirstExistingFile(normalised, importLibs, importLib))
        layout.libDir = wxFileName(importLib).GetPath();

    wxString runtime;

    if (FirstExistingFile(normalised, runtimeDlls, runtime))
    {
        layout.ogreMainDll = runtime;
        layout.runtimeDir = wxFileName(runtime).GetPath();
    }
    else if (wxFileExists(JoinUnder(layout.pluginDir, wxT("OgreMain.dll"))))
    {
        layout.ogreMainDll = JoinUnder(layout.pluginDir, wxT("OgreMain.dll"));
        layout.runtimeDir = layout.pluginDir;
    }

    if (!installedContract)
    {
        layout.valid = true;
        return layout;
    }

    static const wxChar* headers[] = {
        wxT("include\\OGRE\\Ogre.h"),
        nullptr
    };

    static const wxChar* mediaFiles[] = {
        wxT("Media\\models\\ogrehead.mesh"),
        nullptr
    };

    static const wxChar* cmakeFiles[] = {
        wxT("CMake\\OGREConfig.cmake"),
        wxT("lib\\cmake\\OGRE\\OGREConfig.cmake"),
        wxT("cmake\\OGREConfig.cmake"),
        nullptr
    };

    if (!FirstExistingFile(normalised, headers, layout.header))
    {
        layout.error = wxT("include/OGRE/Ogre.h was not found");
        return layout;
    }

    wxString mediaFile;

    if (!FirstExistingFile(normalised, mediaFiles, mediaFile))
    {
        layout.error = wxT("Media/models/ogrehead.mesh was not found");
        return layout;
    }

    wxFileName mediaDir = wxFileName::DirName(wxFileName(mediaFile).GetPath());
    mediaDir.RemoveLastDir();
    layout.media = StripTrailingSlash(mediaDir.GetFullPath());

    if (!FirstExistingFile(normalised, cmakeFiles, layout.cmake))
    {
        layout.error = wxT("OGREConfig.cmake was not found");
        return layout;
    }

    if (layout.libDir.empty())
    {
        layout.error = wxT("libOgreMain.dll.a was not found");
        return layout;
    }

    if (layout.ogreMainDll.empty())
    {
        layout.error = wxT("OgreMain.dll was not found");
        return layout;
    }

    layout.valid = true;
    return layout;
}

wxString FormatOgreLayout(const OgreLayout& layout)
{
    return wxString::Format(
        wxT("valid=%d\n"
            "plugin=%s\n"
            "lib=%s\n"
            "runtime=%s\n"
            "media=%s\n"
            "cmake=%s\n"
            "header=%s\n"
            "ogremain_dll=%s\n"
            "error=%s"),
        layout.valid ? 1 : 0,
        layout.pluginDir,
        layout.libDir,
        layout.runtimeDir,
        layout.media,
        layout.cmake,
        layout.header,
        layout.ogreMainDll,
        layout.error);
}

bool AdoptMingwCMake(const wxString& bin)
{
    const WinCMakeProbe current = ProbeWinCMake();

    if (current.access == WinCMakeWorks)
        return true;

    const wxString candidate = JoinUnder(bin, wxT("cmake.exe"));

    if (!wxFileExists(candidate))
        return false;

    const WinCMakeProbe probe = ProbeWinCMakePath(candidate);

    if (probe.access != WinCMakeWorks)
        return false;

    ConfigManager* cfg = Manager::Get()->GetConfigManager(wxT("scripts"));

    if (cfg)
        cfg->Write(kSavedExecutableKey, candidate);

    return ProbeWinCMake().access == WinCMakeWorks;
}

wxString PrefixList(const wxString& mingwRoot, const wxString& sdl2, const wxString& assimp)
{
    wxString list = mingwRoot;

    if (!sdl2.empty() && !SamePath(sdl2, mingwRoot))
        list += wxT(";") + sdl2;

    if (!assimp.empty() && !SamePath(assimp, mingwRoot) && !SamePath(assimp, sdl2))
        list += wxT(";") + assimp;

    return list;
}

wxString PkgConfigLibDir(const wxString& mingwRoot,
                         const wxString& sdl2,
                         const wxString& assimp)
{
    wxString list;
    const wxString prefixes[] = { mingwRoot, sdl2, assimp };

    for (size_t i = 0; i < WXSIZEOF(prefixes); ++i)
    {
        if (prefixes[i].empty())
            continue;

        const wxString dir = JoinUnder(prefixes[i], wxT("lib\\pkgconfig"));

        if (!wxDirExists(dir))
            continue;

        if (!list.empty())
            list += wxT(";");

        list += dir;
    }

    return list;
}

wxString DescribeOneTool(const wxString& compilerId, wxString& bin, wxString& mingwRoot)
{
    wxString gcc;
    wxString gxx;
    wxString make;
    wxString error;

    if (!MingwTools(compilerId, gcc, gxx, make, bin, error))
    {
        return wxString::Format(
            wxT("compiler=missing\n"
                "gcc=\n"
                "gxx=\n"
                "make=\n"
                "bin=\n"
                "cmake=missing\n"
                "cmake_exe=\n"
                "pkgconf=missing\n"
                "pkgconf_exe=\n"
                "sdl2=missing\n"
                "sdl2_prefix=\n"
                "assimp=missing\n"
                "assimp_prefix=\n"
                "error=%s"),
            FirstLine(error));
    }

    mingwRoot = ParentDirectory(bin);

    wxString cmakeState = wxT("missing");
    wxString cmakeExe;

    if (AdoptMingwCMake(bin))
    {
        const WinCMakeProbe cmake = ProbeWinCMake();
        cmakeState = wxT("works");
        cmakeExe = cmake.executable;
    }

    wxString pkgconfExe;
    wxString pkgconfState = wxT("missing");

    if (wxFileExists(JoinUnder(bin, wxT("pkgconf.exe"))))
        pkgconfExe = JoinUnder(bin, wxT("pkgconf.exe"));
    else if (wxFileExists(JoinUnder(bin, wxT("pkg-config.exe"))))
        pkgconfExe = JoinUnder(bin, wxT("pkg-config.exe"));
    else if (!FindOnPath(wxT("pkgconf.exe"), pkgconfExe))
        FindOnPath(wxT("pkg-config.exe"), pkgconfExe);

    if (!pkgconfExe.empty() && ToolRuns(pkgconfExe))
        pkgconfState = wxT("works");

    wxString sdl2Prefix;
    wxString sdl2State = wxT("missing");
    const wxString localRoot = CodeBlocksRoot();

    if (wxFileExists(JoinUnder(mingwRoot, wxT("include\\SDL2\\SDL.h"))))
    {
        sdl2Prefix = mingwRoot;
        sdl2State = wxT("works");
    }
    else if (!localRoot.empty()
             && wxFileExists(JoinUnder(localRoot, wxT("sdl2\\include\\SDL2\\SDL.h"))))
    {
        sdl2Prefix = JoinUnder(localRoot, wxT("sdl2"));
        sdl2State = wxT("works");
    }

    wxString assimpPrefix;
    wxString assimpState = wxT("missing");

    if (wxFileExists(JoinUnder(mingwRoot, wxT("include\\assimp\\Importer.hpp"))))
    {
        assimpPrefix = mingwRoot;
        assimpState = wxT("works");
    }
    else if (!localRoot.empty()
             && wxFileExists(JoinUnder(localRoot, wxT("assimp\\include\\assimp\\Importer.hpp"))))
    {
        assimpPrefix = JoinUnder(localRoot, wxT("assimp"));
        assimpState = wxT("works");
    }

    return wxString::Format(
        wxT("compiler=works\n"
            "gcc=%s\n"
            "gxx=%s\n"
            "make=%s\n"
            "bin=%s\n"
            "cmake=%s\n"
            "cmake_exe=%s\n"
            "pkgconf=%s\n"
            "pkgconf_exe=%s\n"
            "sdl2=%s\n"
            "sdl2_prefix=%s\n"
            "assimp=%s\n"
            "assimp_prefix=%s\n"
            "error="),
        gcc,
        gxx,
        make,
        bin,
        cmakeState,
        cmakeExe,
        pkgconfState,
        pkgconfExe,
        sdl2State,
        sdl2Prefix,
        assimpState,
        assimpPrefix);
}

} // namespace

wxString DescribeWinOgreTools(const wxString& compilerId)
{
    wxString bin;
    wxString mingwRoot;
    return DescribeOneTool(compilerId, bin, mingwRoot);
}

wxString DescribeWinOgreCache(const wxString& buildInput, const wxString& installInput)
{
    wxString build;
    wxString install;
    wxString error;
    bool ready = false;

    if (NormaliseAbsoluteDirectory(buildInput, build)
        && NormaliseAbsoluteDirectory(installInput, install))
    {
        const wxString cache = ReadFile(build + wxT("\\CMakeCache.txt"));
        ready = !cache.empty()
            && GeneratedMakefile(build)
            && CacheVariableIsPath(cache, wxT("CMAKE_INSTALL_PREFIX"), install)
            && CacheVariableEquals(cache, wxT("CMAKE_BUILD_TYPE"), wxT("Release"))
            && !ListsNewerThanCache(build)
            && CacheOptionAccepts(cache, wxT("OGRE_BUILD_DEPENDENCIES"), wxT("OFF"))
            && CacheOptionAccepts(cache, wxT("OGRE_BUILD_COMPONENT_OVERLAY_IMGUI"), wxT("OFF"))
            && CacheOptionAccepts(cache, wxT("OGRE_BUILD_SAMPLES"), wxT("OFF"));
    }
    else
    {
        error = wxT("build or install directory is not absolute");
    }

    return wxString::Format(
        wxT("ready=%d\nerror=%s"),
        ready ? 1 : 0,
        error);
}

wxString DescribeWinOgreBuilt(const wxString& build)
{
    const OgreLayout layout = InspectOgreTree(build, false);
    return FormatOgreLayout(layout);
}

wxString DescribeWinOgreLayout(const wxString& install)
{
    const OgreLayout layout = InspectOgreTree(install, true);
    return FormatOgreLayout(layout);
}

WinCMakeRunResult ConfigureWinOgre(
    wxWindow* parent,
    const wxString& compilerId,
    const wxString& sourceInput,
    const wxString& buildInput,
    const wxString& installInput,
    const wxString& explanation,
    const wxString& confirmLabel)
{
    WinCMakeRunResult result;
    result.operation = wxT("configure");

    wxString source;
    wxString build;
    wxString install;
    wxString error;

    if (!ValidateConfigureInputs(
            sourceInput,
            buildInput,
            installInput,
            wxT("Release"),
            source,
            build,
            install,
            error))
    {
        result.output = error;
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

    if (!AdoptMingwCMake(bin))
    {
        result.output = wxT("cmake unavailable");
        return result;
    }

    const wxString cache = ReadFile(build + wxT("\\CMakeCache.txt"));

    if (!cache.empty()
        && CacheVariableIsPath(cache, wxT("CMAKE_INSTALL_PREFIX"), install)
        && CacheVariableIsPath(cache, wxT("CMAKE_C_COMPILER"), gcc)
        && CacheVariableEquals(cache, wxT("CMAKE_BUILD_TYPE"), wxT("Release"))
        && !ListsNewerThanCache(build)
        && GeneratedMakefile(build)
        && CacheOptionAccepts(cache, wxT("OGRE_BUILD_DEPENDENCIES"), wxT("OFF"))
        && CacheOptionAccepts(cache, wxT("OGRE_BUILD_COMPONENT_OVERLAY_IMGUI"), wxT("OFF"))
        && CacheOptionAccepts(cache, wxT("OGRE_BUILD_SAMPLES"), wxT("OFF")))
    {
        result.success = true;
        result.output = wxT("CMake cache already matches this prefix, Release, and gcc");
        return result;
    }

    wxString ignoredBin;
    wxString mingwRoot;
    const wxString tools = DescribeOneTool(compilerId, ignoredBin, mingwRoot);
    wxString sdl2Prefix;
    wxString assimpPrefix;

    {
        const int sdlAt = tools.Find(wxT("sdl2_prefix="));
        if (sdlAt != wxNOT_FOUND)
        {
            sdl2Prefix = tools.Mid(sdlAt + wxString(wxT("sdl2_prefix=")).Length());
            sdl2Prefix = sdl2Prefix.BeforeFirst(wxT('\n'));
        }

        const int assimpAt = tools.Find(wxT("assimp_prefix="));
        if (assimpAt != wxNOT_FOUND)
        {
            assimpPrefix = tools.Mid(assimpAt + wxString(wxT("assimp_prefix=")).Length());
            assimpPrefix = assimpPrefix.BeforeFirst(wxT('\n'));
        }
    }

    wxString pathPrefix = bin;
    const int pkgAt = tools.Find(wxT("pkgconf_exe="));

    if (pkgAt != wxNOT_FOUND)
    {
        wxString pkgconfExe = tools.Mid(pkgAt + wxString(wxT("pkgconf_exe=")).Length());
        pkgconfExe = pkgconfExe.BeforeFirst(wxT('\n'));

        if (!pkgconfExe.empty())
        {
            const wxString pkgBin = wxFileName(pkgconfExe).GetPath();

            if (!pkgBin.empty() && !SamePath(pkgBin, bin))
                pathPrefix = pkgBin + wxT(";") + pathPrefix;
        }
    }

    wxArrayString args;
    args.Add(wxT("-S"));
    args.Add(source);
    args.Add(wxT("-B"));
    args.Add(build);
    args.Add(wxT("-G"));
    args.Add(wxT("MinGW Makefiles"));
    args.Add(wxT("-DCMAKE_BUILD_TYPE=Release"));
    args.Add(wxT("-DCMAKE_INSTALL_PREFIX=") + install);
    args.Add(wxT("-DCMAKE_C_COMPILER=") + gcc);
    args.Add(wxT("-DCMAKE_CXX_COMPILER=") + gxx);
    args.Add(wxT("-DCMAKE_MAKE_PROGRAM=") + make);
    args.Add(wxT("-DCMAKE_PREFIX_PATH=") + PrefixList(mingwRoot, sdl2Prefix, assimpPrefix));
    // OGRE otherwise downloads SDL2, Assimp 6, zlib, freetype, pugixml,
    // Bullet, and imgui during configure. This CMake cannot verify those
    // HTTPS certificates, so the archives are empty and generation stops.
    // SDL2 and Assimp 5.4.3 are already on CMAKE_PREFIX_PATH. The generated
    // project does not need OGRE's sample browser or the imgui overlay.
    args.Add(wxT("-DOGRE_BUILD_DEPENDENCIES=OFF"));
    args.Add(wxT("-DOGRE_BUILD_COMPONENT_OVERLAY_IMGUI=OFF"));
    args.Add(wxT("-DOGRE_BUILD_SAMPLES=OFF"));

    const wxString summary = wxString::Format(
        _("CMake is configuring OGRE 14.6.3.\n\nSource:\n%s\n\nBuild:\n%s\n\nInstall:\n%s"),
        source,
        build,
        install);

    result = RunCMake(
        parent,
        wxT("configure"),
        args,
        pathPrefix,
        summary,
        explanation,
        confirmLabel,
        PkgConfigLibDir(mingwRoot, sdl2Prefix, assimpPrefix));

    if (!result.success)
        return result;

    const wxString written = ReadFile(build + wxT("\\CMakeCache.txt"));

    if (written.empty()
        || !CacheVariableIsPath(written, wxT("CMAKE_INSTALL_PREFIX"), install)
        || !CacheVariableIsPath(written, wxT("CMAKE_C_COMPILER"), gcc)
        || !CacheVariableEquals(written, wxT("CMAKE_BUILD_TYPE"), wxT("Release")))
    {
        result.success = false;
        result.output = wxT("CMake finished, but the cache does not record this install prefix, Release, and gcc");
    }

    return result;
}

WinCMakeRunResult BuildWinOgre(
    wxWindow* parent,
    const wxString& buildInput,
    const wxString& explanation,
    const wxString& confirmLabel)
{
    WinCMakeRunResult result;
    result.operation = wxT("build");

    wxString build;
    wxString error;

    if (!ValidateBuildDirectory(buildInput, build, error))
    {
        result.output = error;
        return result;
    }

    const int jobs = BoundedJobCount();
    wxArrayString args;
    args.Add(wxT("--build"));
    args.Add(build);
    args.Add(wxT("--parallel"));
    args.Add(wxString::Format(wxT("%d"), jobs));

    const wxString summary = wxString::Format(
        _("CMake is building OGRE 14.6.3.\n\nBuild:\n%s\n\nParallel jobs: %d"),
        build,
        jobs);

    result = RunCMake(
        parent,
        wxT("build"),
        args,
        wxEmptyString,
        summary,
        explanation,
        confirmLabel);

    if (!result.success)
        return result;

    const OgreLayout layout = InspectOgreTree(build, false);

    if (!layout.valid)
    {
        result.success = false;
        result.output = layout.error.empty()
            ? wxString(wxT("CMake finished, but RenderSystem_GL.dll was not found"))
            : layout.error;
    }

    return result;
}

WinCMakeRunResult InstallWinOgre(
    wxWindow* parent,
    const wxString& buildInput,
    const wxString& installInput,
    const wxString& explanation,
    const wxString& confirmLabel)
{
    WinCMakeRunResult result;
    result.operation = wxT("install");

    wxString build;
    wxString install;
    wxString error;

    if (!ValidateBuildDirectory(buildInput, build, error)
        || !NormaliseAbsoluteDirectory(installInput, install))
    {
        result.output = error.empty() ? wxString(wxT("install directory is not absolute")) : error;
        return result;
    }

    const wxString cache = ReadFile(build + wxT("\\CMakeCache.txt"));

    if (!CacheVariableIsPath(cache, wxT("CMAKE_INSTALL_PREFIX"), install))
    {
        result.output = wxT("install directory is not the prefix fixed at configure time");
        return result;
    }

    wxArrayString args;
    args.Add(wxT("--install"));
    args.Add(build);

    const wxString summary = wxString::Format(
        _("CMake is installing OGRE 14.6.3.\n\nBuild:\n%s\n\nInstall:\n%s"),
        build,
        install);

    result = RunCMake(
        parent,
        wxT("install"),
        args,
        wxEmptyString,
        summary,
        explanation,
        confirmLabel);

    if (!result.success)
        return result;

    const OgreLayout layout = InspectOgreTree(install, true);

    if (!layout.valid)
    {
        result.success = false;
        result.output = layout.error.empty()
            ? wxString(wxT("installed OGRE SDK contract was not found"))
            : layout.error;
    }

    return result;
}

bool DepIdOk(const wxString& libraryId)
{
    return libraryId == wxT("sdl2") || libraryId == wxT("assimp");
}

bool DepInstalled(const wxString& libraryId, const wxString& install, wxString& error)
{
    const wxChar* header = libraryId == wxT("sdl2")
        ? wxT("include\\SDL2\\SDL.h")
        : wxT("include\\assimp\\Importer.hpp");
    const wxChar* importLib = libraryId == wxT("sdl2")
        ? wxT("lib\\libSDL2.dll.a")
        : wxT("lib\\libassimp.dll.a");

    if (!wxFileExists(JoinUnder(install, header)))
    {
        error = wxString(header) + wxT(" was not found");
        return false;
    }

    if (!wxFileExists(JoinUnder(install, importLib)))
    {
        error = wxString(importLib) + wxT(" was not found");
        return false;
    }

    if (libraryId == wxT("sdl2"))
    {
        if (!wxFileExists(JoinUnder(install, wxT("bin\\SDL2.dll"))))
        {
            error = wxT("bin/SDL2.dll was not found");
            return false;
        }

        return true;
    }

    wxDir bin(JoinUnder(install, wxT("bin")));
    wxString name;

    if (!bin.IsOpened()
        || !bin.GetFirst(&name, wxT("libassimp*.dll"), wxDIR_FILES))
    {
        error = wxT("bin/libassimp*.dll was not found");
        return false;
    }

    return true;
}

wxString IsolatedPkgConfigDir()
{
    const wxString root = CodeBlocksRoot();

    if (root.empty())
        return wxString();

    const wxString dir = JoinUnder(root, wxT("_pkgconfig"));
    wxFileName name = wxFileName::DirName(dir);

    if (!wxDirExists(dir) && !name.Mkdir(wxS_DIR_DEFAULT, wxPATH_MKDIR_FULL))
        return wxString();

    return dir;
}

WinCMakeRunResult ConfigureWinDep(
    wxWindow* parent,
    const wxString& compilerId,
    const wxString& libraryId,
    const wxString& sourceInput,
    const wxString& buildInput,
    const wxString& installInput,
    const wxString& explanation,
    const wxString& confirmLabel)
{
    WinCMakeRunResult result;
    result.operation = wxT("configure");

    if (!DepIdOk(libraryId))
    {
        result.output = wxT("unknown library");
        return result;
    }

    wxString source;
    wxString build;
    wxString install;
    wxString error;

    if (!ValidateConfigureInputs(
            sourceInput,
            buildInput,
            installInput,
            wxT("Release"),
            source,
            build,
            install,
            error))
    {
        result.output = error;
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

    if (!AdoptMingwCMake(bin))
    {
        result.output = wxT("cmake unavailable");
        return result;
    }

    const wxString cache = ReadFile(build + wxT("\\CMakeCache.txt"));

    if (!cache.empty()
        && GeneratedMakefile(build)
        && CacheVariableIsPath(cache, wxT("CMAKE_INSTALL_PREFIX"), install)
        && CacheVariableIsPath(cache, wxT("CMAKE_C_COMPILER"), gcc)
        && CacheVariableEquals(cache, wxT("CMAKE_BUILD_TYPE"), wxT("Release"))
        && !ListsNewerThanCache(build))
    {
        result.success = true;
        result.output = wxT("CMake cache already matches this prefix, Release, and gcc");
        return result;
    }

    const wxString mingwRoot = ParentDirectory(bin);
    wxString pkgDir = PkgConfigLibDir(mingwRoot, wxEmptyString, wxEmptyString);

    if (pkgDir.empty())
        pkgDir = IsolatedPkgConfigDir();

    wxArrayString args;
    args.Add(wxT("-S"));
    args.Add(source);
    args.Add(wxT("-B"));
    args.Add(build);
    args.Add(wxT("-G"));
    args.Add(wxT("MinGW Makefiles"));
    args.Add(wxT("-DCMAKE_BUILD_TYPE=Release"));
    args.Add(wxT("-DCMAKE_INSTALL_PREFIX=") + install);
    args.Add(wxT("-DCMAKE_C_COMPILER=") + gcc);
    args.Add(wxT("-DCMAKE_CXX_COMPILER=") + gxx);
    args.Add(wxT("-DCMAKE_MAKE_PROGRAM=") + make);
    args.Add(wxT("-DCMAKE_PREFIX_PATH=") + mingwRoot);

    const wxString title = libraryId == wxT("sdl2")
        ? wxString(_("SDL2 2.32.10"))
        : wxString(_("Assimp 5.4.3"));

    if (libraryId == wxT("sdl2"))
    {
        args.Add(wxT("-DSDL_SHARED=ON"));
        args.Add(wxT("-DSDL_STATIC=OFF"));
        args.Add(wxT("-DSDL_TEST=OFF"));
    }
    else
    {
        args.Add(wxT("-DBUILD_SHARED_LIBS=ON"));
        args.Add(wxT("-DASSIMP_BUILD_ASSIMP_TOOLS=OFF"));
        args.Add(wxT("-DASSIMP_BUILD_TESTS=OFF"));
        args.Add(wxT("-DASSIMP_WARNINGS_AS_ERRORS=OFF"));
        args.Add(wxT("-DASSIMP_BUILD_ZLIB=ON"));
    }

    const wxString summary = wxString::Format(
        _("CMake is configuring %s.\n\nSource:\n%s\n\nBuild:\n%s\n\nInstall:\n%s"),
        title,
        source,
        build,
        install);

    result = RunCMake(
        parent,
        wxT("configure"),
        args,
        bin,
        summary,
        explanation,
        confirmLabel,
        pkgDir);

    if (!result.success)
        return result;

    const wxString written = ReadFile(build + wxT("\\CMakeCache.txt"));

    if (written.empty()
        || !CacheVariableIsPath(written, wxT("CMAKE_INSTALL_PREFIX"), install)
        || !CacheVariableIsPath(written, wxT("CMAKE_C_COMPILER"), gcc)
        || !CacheVariableEquals(written, wxT("CMAKE_BUILD_TYPE"), wxT("Release")))
    {
        result.success = false;
        result.output = wxT("CMake finished, but the cache does not record this install prefix, Release, and gcc");
    }

    return result;
}

WinCMakeRunResult BuildWinDep(
    wxWindow* parent,
    const wxString& libraryId,
    const wxString& buildInput,
    const wxString& explanation,
    const wxString& confirmLabel)
{
    WinCMakeRunResult result;
    result.operation = wxT("build");

    if (!DepIdOk(libraryId))
    {
        result.output = wxT("unknown library");
        return result;
    }

    wxString build;
    wxString error;

    if (!ValidateBuildDirectory(buildInput, build, error))
    {
        result.output = error;
        return result;
    }

    const int jobs = BoundedJobCount();
    wxArrayString args;
    args.Add(wxT("--build"));
    args.Add(build);
    args.Add(wxT("--parallel"));
    args.Add(wxString::Format(wxT("%d"), jobs));

    const wxString title = libraryId == wxT("sdl2")
        ? wxString(_("SDL2 2.32.10"))
        : wxString(_("Assimp 5.4.3"));
    const wxString summary = wxString::Format(
        _("CMake is building %s.\n\nBuild:\n%s\n\nParallel jobs: %d"),
        title,
        build,
        jobs);

    return RunCMake(
        parent,
        wxT("build"),
        args,
        wxEmptyString,
        summary,
        explanation,
        confirmLabel);
}

WinCMakeRunResult InstallWinDep(
    wxWindow* parent,
    const wxString& libraryId,
    const wxString& buildInput,
    const wxString& installInput,
    const wxString& explanation,
    const wxString& confirmLabel)
{
    WinCMakeRunResult result;
    result.operation = wxT("install");

    if (!DepIdOk(libraryId))
    {
        result.output = wxT("unknown library");
        return result;
    }

    wxString build;
    wxString install;
    wxString error;

    if (!ValidateBuildDirectory(buildInput, build, error)
        || !NormaliseAbsoluteDirectory(installInput, install))
    {
        result.output = error.empty() ? wxString(wxT("install directory is not absolute")) : error;
        return result;
    }

    const wxString cache = ReadFile(build + wxT("\\CMakeCache.txt"));

    if (!CacheVariableIsPath(cache, wxT("CMAKE_INSTALL_PREFIX"), install))
    {
        result.output = wxT("install directory is not the prefix fixed at configure time");
        return result;
    }

    wxArrayString args;
    args.Add(wxT("--install"));
    args.Add(build);

    const wxString title = libraryId == wxT("sdl2")
        ? wxString(_("SDL2 2.32.10"))
        : wxString(_("Assimp 5.4.3"));
    const wxString summary = wxString::Format(
        _("CMake is installing %s.\n\nBuild:\n%s\n\nInstall:\n%s"),
        title,
        build,
        install);

    result = RunCMake(
        parent,
        wxT("install"),
        args,
        wxEmptyString,
        summary,
        explanation,
        confirmLabel);

    if (!result.success)
        return result;

    if (!DepInstalled(libraryId, install, error))
    {
        result.success = false;
        result.output = error;
    }

    return result;
}

wxString FormatWinCMakeRun(const WinCMakeRunResult& result)
{
    return wxString::Format(
        wxT("launched=%d\n"
            "cancelled=%d\n"
            "exit=%ld\n"
            "success=%d\n"
            "operation=%s\n"
            "output=%s"),
        result.launched ? 1 : 0,
        result.cancelled ? 1 : 0,
        result.exitCode,
        result.success ? 1 : 0,
        result.operation,
        result.output);
}

#endif // __WXMSW__
