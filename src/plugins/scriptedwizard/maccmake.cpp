/*
 * This file is part of the Code::Blocks IDE and licensed under
 * the GNU General Public License, version 3.
 * http://www.gnu.org/licenses/gpl-3.0.html
 *
 * v3.8 M3: resolve Code::Blocks-owned source/build/install paths
 * and locate/revalidate CMake on macOS.
 */

#include <sdk.h>

#ifndef CB_PRECOMP
    #include <configmanager.h>
    #include <manager.h>
#endif

#include <wx/filename.h>
#include <wx/process.h>
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
            out += wxString(
                buf,
                wxConvISO8859_1,
                n);
        else
            out += chunk;
    }

    return out;
}

wxString FirstLine(const wxString& text)
{
    wxString line =
        text.BeforeFirst(wxT('\n'));

    line.Trim(true).Trim(false);

    if (line.EndsWith(wxT("\r")))
        line.RemoveLast();

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

    exitCode =
        wxExecute(
            argv,
            wxEXEC_SYNC,
            &process);

    output =
        ReadAll(process.GetInputStream());

    error =
        ReadAll(process.GetErrorStream());

    return exitCode != -1;
}

bool LeafNameOk(const wxString& leaf)
{
    if (leaf.empty())
        return false;

    if (leaf == wxT(".") ||
        leaf == wxT(".."))
    {
        return false;
    }

    for (size_t i = 0;
         i < leaf.Length();
         ++i)
    {
        const wxChar c = leaf[i];

        if (c < wxT(' ') ||
            c == wxT('\x7f') ||
            c == wxT('/') ||
            c == wxT('\\'))
        {
            return false;
        }
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
        probe.access =
            MacCMakePresentUnusable;

        probe.errorOutput =
            versionError;

        return;
    }

    probe.errorOutput =
        versionError;

    const wxString version =
        FirstLine(probe.versionOutput);

    if (probe.versionExit != 0 ||
        !version.Lower().StartsWith(
            wxT("cmake version")))
    {
        probe.access =
            MacCMakePresentUnusable;

        return;
    }

    probe.access =
        MacCMakeWorks;
}

} // namespace


MacCMakeProbe ProbeMacCMake()
{
    MacCMakeProbe probe;

    ConfigManager* cfg =
        Manager::Get()->GetConfigManager(
            wxT("scripts"));

    const wxString saved =
        cfg
            ? cfg->Read(
                  kSavedExecutableKey,
                  wxEmptyString)
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

    if (executable.empty() ||
        !wxFileExists(executable))
    {
        probe.errorOutput =
            wxT("file not found");

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
            : (probe.access ==
                       MacCMakePresentUnusable
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
        paths.errorOutput =
            wxT("invalid source leaf");

        return paths;
    }

    if (!LeafNameOk(installLeaf))
    {
        paths.errorOutput =
            wxT("invalid install leaf");

        return paths;
    }

    paths.root =
        ConfigManager::GetFolder(sdDataUser);

    if (paths.root.empty())
    {
        paths.errorOutput =
            wxT("Code::Blocks user data folder is empty");

        return paths;
    }

    wxFileName root =
        wxFileName::DirName(paths.root);

    if (!root.IsAbsolute())
    {
        paths.errorOutput =
            wxT("Code::Blocks SDK root is not absolute");

        return paths;
    }

    paths.root =
        wxGetHomeDir();

    if (paths.root.empty())
    {
        paths.errorOutput =
            wxT("user home directory is empty");

        return paths;
    }

    paths.root += wxFILE_SEP_PATH;
    paths.root += wxT("Library");
    paths.root += wxFILE_SEP_PATH;
    paths.root += wxT("CodeBlocks");

    paths.source =
        paths.root +
        wxFILE_SEP_PATH +
        sourceLeaf;

    paths.build =
        paths.source +
        wxFILE_SEP_PATH +
        wxT("build-codeblocks");

    paths.install =
        paths.root +
        wxFILE_SEP_PATH +
        installLeaf;

    if (paths.source == paths.install ||
        paths.build == paths.install)
    {
        paths.errorOutput =
            wxT("resolved paths overlap");

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

#endif // __WXMAC__
