/*
 * This file is part of the Code::Blocks IDE and licensed under
 * the GNU General Public License, version 3.
 * http://www.gnu.org/licenses/gpl-3.0.html
 *
 * v3.8 M3: resolve Code::Blocks-owned source/build/install paths
 * and locate/revalidate CMake on macOS.
 *
 * This is deliberately not a general CMake command API.
 */

#ifndef MACCMAKE_H
#define MACCMAKE_H

#include <wx/string.h>

#if defined(__WXMAC__)

enum MacCMakeAccess
{
    MacCMakeNotPresent = 0,
    MacCMakePresentUnusable = 1,
    MacCMakeWorks = 2
};

struct MacCMakeProbe
{
    int access;
    wxString executable;
    wxString versionOutput;
    wxString errorOutput;
    long versionExit;
    bool savedPathMissing;

    MacCMakeProbe()
        : access(MacCMakeNotPresent),
          versionExit(-1),
          savedPathMissing(false)
    {}
};

MacCMakeProbe ProbeMacCMake();

MacCMakeProbe ProbeMacCMakePath(
    const wxString& executable);

wxString FormatMacCMakeProbe(
    const MacCMakeProbe& probe);

struct MacCMakePaths
{
    bool valid;
    wxString root;
    wxString source;
    wxString build;
    wxString install;
    wxString errorOutput;

    MacCMakePaths()
        : valid(false)
    {}
};

MacCMakePaths ResolveMacCMakePaths(
    const wxString& sourceLeaf,
    const wxString& installLeaf);

wxString FormatMacCMakePaths(
    const MacCMakePaths& paths);


struct MacCMakeRunResult
{
    bool launched;
    bool cancelled;
    long exitCode;
    bool success;
    wxString operation;
    wxString output;

    MacCMakeRunResult()
        : launched(false),
          cancelled(false),
          exitCode(-1),
          success(false)
    {}
};

MacCMakeRunResult ConfigureMacCMake(
    const wxString& source,
    const wxString& build,
    const wxString& install,
    const wxString& buildType,
    const wxString& explanation,
    const wxString& confirmLabel);


MacCMakeRunResult BuildMacCMake(
    const wxString& build,
    const wxString& explanation,
    const wxString& confirmLabel);

MacCMakeRunResult InstallMacCMake(
    const wxString& build,
    const wxString& install,
    const wxString& explanation,
    const wxString& confirmLabel);

wxString FormatMacCMakeRun(
    const MacCMakeRunResult& result);

#endif // __WXMAC__

#endif // MACCMAKE_H
