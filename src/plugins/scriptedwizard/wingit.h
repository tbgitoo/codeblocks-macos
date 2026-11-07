/*
 * This file is part of the Code::Blocks IDE and licensed under the GNU General Public License, version 3
 * http://www.gnu.org/licenses/gpl-3.0.html
 *
 * v3.6b. Locate Git, revalidate one executable, and perform one
 * explicitly approved shallow tagged clone.
 *
 * This is deliberately not a general Git command API.
 * The helper does not install Git and does not know which project is cloned.
 */

#ifndef WINGIT_H
#define WINGIT_H

#include <wx/string.h>

class wxWindow;

#if defined(__WXMSW__)

// A saved path is only a location hint. Every probe runs --version again.
enum WinGitAccess
{
    WinGitNotPresent = 0,
    WinGitPresentUnusable = 1,
    WinGitWorks = 2
};

struct WinGitProbe
{
    int access;
    wxString executable;
    wxString versionOutput;
    wxString errorOutput;
    long versionExit;
    bool savedPathMissing;

    WinGitProbe()
        : access(WinGitNotPresent),
          versionExit(-1),
          savedPathMissing(false)
    {}
};

// Locate git.exe and validate it with: git --version
// A saved /windows/git/executable path is only a hint.
WinGitProbe ProbeWinGit();

// Revalidate exactly this executable.
// Does not read or write the saved Git path.
WinGitProbe ProbeWinGitPath(const wxString& executable);

wxString FormatWinGitProbe(const WinGitProbe& probe);

// Result of one explicitly approved shallow tagged clone.
struct WinGitCloneResult
{
    bool launched;
    bool cancelled;
    long exitCode;
    bool success;

    wxString executable;
    wxString url;
    wxString tag;
    wxString destination;
    wxString output;

    WinGitCloneResult()
        : launched(false),
          cancelled(false),
          exitCode(-1),
          success(false)
    {}
};

// Consent first. On approval, runs:
//     git clone --branch <tag> --depth 1 <url> <destination>
// Cancel before launch does not start Git.
// Success means Git finished with exit code 0.
WinGitCloneResult CloneWinGitRepository(
    wxWindow* parent,
    const wxString& url,
    const wxString& tag,
    const wxString& destination,
    const wxString& explanation,
    const wxString& confirmLabel);

wxString FormatWinGitClone(const WinGitCloneResult& result);

#endif // __WXMSW__

#endif // WINGIT_H
