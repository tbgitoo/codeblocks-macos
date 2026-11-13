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

bool SamePath(const wxString& left, const wxString& right)
{
    return Canonical(left) == Canonical(right);
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
    const wxString child = Canonical(path);
    const wxString parent = Canonical(root);

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

    bool Start(const wxArrayString& args, const wxString& compilerBin)
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
    const wxString& confirmLabel)
{
    WinCMakeRunResult result;
    result.operation = operation;

    const WinCMakeProbe probe = ProbeWinCMake();

    if (probe.access != WinCMakeWorks)
    {
        result.output = wxT("cmake unavailable");
        return result;
    }

    if (!ConfirmAction(parent, explanation, confirmLabel))
        return result;

    wxArrayString command;
    command.Add(probe.executable);

    for (size_t i = 0; i < args.GetCount(); ++i)
        command.Add(args[i]);

    WinCMakeRunDialog dlg(parent, summary);

    if (!dlg.Start(command, compilerBin))
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
