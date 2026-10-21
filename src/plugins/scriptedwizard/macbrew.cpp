/*
 * This file is part of the Code::Blocks IDE and licensed under the GNU General Public License, version 3
 * http://www.gnu.org/licenses/gpl-3.0.html
 *
 * v3.0 M2. argv execution of a verified brew path. No shell string, no PATH edit.
 */

#include <sdk.h>

#ifndef CB_PRECOMP
    #include <configmanager.h>
    #include <manager.h>
#endif

#include <wx/process.h>
#include <wx/utils.h>

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

// Runs brew with one or two fixed arguments. Returns false when wxExecute cannot start it.
bool RunArgv(const wxString& executable, const wxChar* arg1, const wxChar* arg2,
             long& exitCode, wxString& output, wxString& error)
{
    wxProcess process(wxPROCESS_REDIRECT);
    const wxChar* argv[4];
    argv[0] = executable.wx_str();
    argv[1] = arg1;
    argv[2] = arg2;
    argv[3] = nullptr;

    exitCode = wxExecute(argv, wxEXEC_SYNC, &process);
    output = ReadAll(process.GetInputStream());
    error = ReadAll(process.GetErrorStream());
    return exitCode != -1;
}

wxString FirstLine(const wxString& text)
{
    wxString line = text.BeforeFirst(wxT('\n'));
    line.Trim(true).Trim(false);
    if (line.EndsWith(wxT("\r")))
        line.RemoveLast();
    return line;
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
    wxString versionError;
    if (!RunArgv(candidate, wxT("--version"), nullptr, probe.versionExit, probe.versionOutput, versionError))
    {
        probe.access = MacBrewPresentUnusable;
        probe.errorOutput = versionError;
        return probe;
    }
    probe.errorOutput = versionError;

    const bool versionOk = probe.versionExit == 0 && probe.versionOutput.Contains(wxT("Homebrew"));
    if (!versionOk)
    {
        probe.access = MacBrewPresentUnusable;
        return probe;
    }

    probe.access = MacBrewWorks;
    if (!wantPrefix)
        return probe;

    wxString prefixOut;
    wxString prefixErr;
    if (!RunArgv(candidate, wxT("--prefix"), nullptr, probe.prefixExit, prefixOut, prefixErr))
    {
        if (!prefixErr.empty())
            probe.errorOutput += prefixErr;
        return probe;
    }
    if (!prefixErr.empty())
        probe.errorOutput += prefixErr;

    const wxString line = FirstLine(prefixOut);
    if (probe.prefixExit == 0 && line.StartsWith(wxT("/")))
        probe.prefix = line;
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

#endif // __WXMAC__
