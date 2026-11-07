/*
 * This file is part of the Code::Blocks IDE and licensed under the GNU General Public License, version 3
 * http://www.gnu.org/licenses/gpl-3.0.html
 *
 * v3.6b. Resolve the Code::Blocks-owned FLTK source/build/install paths
 * and configure, build, and install that one static MinGW library.
 *
 * This is deliberately not a general CMake command API.
 */

#ifndef WINCMAKE_H
#define WINCMAKE_H

#include <wx/string.h>

class wxWindow;

#if defined(__WXMSW__)

enum WinCMakeAccess
{
    WinCMakeNotPresent = 0,
    WinCMakePresentUnusable = 1,
    WinCMakeWorks = 2
};

struct WinCMakeProbe
{
    int access;
    wxString executable;
    wxString versionOutput;
    wxString errorOutput;
    long versionExit;
    bool savedPathMissing;

    WinCMakeProbe()
        : access(WinCMakeNotPresent),
          versionExit(-1),
          savedPathMissing(false)
    {}
};

WinCMakeProbe ProbeWinCMake();

WinCMakeProbe ProbeWinCMakePath(const wxString& executable);

wxString FormatWinCMakeProbe(const WinCMakeProbe& probe);

struct WinCMakePaths
{
    bool valid;
    wxString root;
    wxString source;
    wxString build;
    wxString install;
    wxString errorOutput;

    WinCMakePaths()
        : valid(false)
    {}
};

// sourceLeaf and installLeaf are directory names, not paths.
// They are resolved under %LOCALAPPDATA%\codeblocks.
WinCMakePaths ResolveWinCMakePaths(
    const wxString& sourceLeaf,
    const wxString& installLeaf);

wxString FormatWinCMakePaths(const WinCMakePaths& paths);

struct WinCMakeRunResult
{
    bool launched;
    bool cancelled;
    long exitCode;
    bool success;
    wxString operation;
    wxString output;

    WinCMakeRunResult()
        : launched(false),
          cancelled(false),
          exitCode(-1),
          success(false)
    {}
};

// compilerId is the wizard's selected compiler. The script does not pass it.
// The MinGW generator, compiler paths, and FLTK options are fixed here.
WinCMakeRunResult ConfigureWinCMake(
    wxWindow* parent,
    const wxString& compilerId,
    const wxString& source,
    const wxString& build,
    const wxString& install,
    const wxString& buildType,
    const wxString& explanation,
    const wxString& confirmLabel);

WinCMakeRunResult BuildWinCMake(
    wxWindow* parent,
    const wxString& build,
    const wxString& explanation,
    const wxString& confirmLabel);

WinCMakeRunResult InstallWinCMake(
    wxWindow* parent,
    const wxString& build,
    const wxString& install,
    const wxString& explanation,
    const wxString& confirmLabel);

wxString FormatWinCMakeRun(const WinCMakeRunResult& result);

#endif // __WXMSW__

#endif // WINCMAKE_H
