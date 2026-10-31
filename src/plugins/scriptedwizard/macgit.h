/*
 * This file is part of the Code::Blocks IDE and licensed under the GNU General Public License, version 3
 * http://www.gnu.org/licenses/gpl-3.0.html
 *
 * v3.8 M2: locate Git, revalidate one executable, and perform one
 * explicitly approved shallow tagged clone.
 *
 * This is deliberately not a general Git command API.
 * The helper does not install Git and does not know anything about OGRE or other git repos.
 */

#ifndef MACGIT_H
#define MACGIT_H

#include <wx/string.h>

#if defined(__WXMAC__)

// A saved path is only a location hint. Every probe runs --version again.
enum MacGitAccess
{
    MacGitNotPresent = 0,
    MacGitPresentUnusable = 1,
    MacGitWorks = 2
};

struct MacGitProbe
{
    int access;
    wxString executable;
    wxString versionOutput;
    wxString errorOutput;
    long versionExit;
    bool savedPathMissing;

    MacGitProbe()
        : access(MacGitNotPresent),
          versionExit(-1),
          savedPathMissing(false)
    {}
};

// Locate Git and validate the selected executable using:
//     git --version
//
// A saved /macos/git/executable path is only a hint and must be
// revalidated before it is accepted.
MacGitProbe ProbeMacGit();

// Revalidate exactly this executable.
// Does not read or write the saved Git path.
MacGitProbe ProbeMacGitPath(const wxString& executable);

// Report string returned to wizard.script.
wxString FormatMacGitProbe(const MacGitProbe& probe);


// Result of one explicitly approved shallow tagged clone.
// This is deliberately not a general Git command result.
struct MacGitCloneResult
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

    MacGitCloneResult()
        : launched(false),
          cancelled(false),
          exitCode(-1),
          success(false)
    {}
};

// Consent first.
//
// On approval, performs the equivalent of:
//
//     git clone --branch <tag> --depth 1 <url> <destination>
//
// The implementation must:
//   - revalidate Git before use;
//   - reject unsafe inputs;
//   - require a safe absolute destination;
//   - refuse a non-empty destination;
//   - construct argv internally;
//   - never invoke a shell command supplied by wizard.script;
//   - never install Git.
//
// Cancel before launch does not start Git.
// Success means that Git finished with exit code 0.
MacGitCloneResult CloneMacGitRepository(
    const wxString& url,
    const wxString& tag,
    const wxString& destination,
    const wxString& explanation,
    const wxString& confirmLabel);

// Report string returned to wizard.script.
wxString FormatMacGitClone(const MacGitCloneResult& result);

#endif // __WXMAC__

#endif // MACGIT_H
