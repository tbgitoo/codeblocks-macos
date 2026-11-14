/*
 * This file is part of the Code::Blocks IDE and licensed under the GNU General Public License, version 3
 * http://www.gnu.org/licenses/gpl-3.0.html
 *
 * v3.8 M2: locate Git, revalidate one executable, and perform one
 * explicitly approved shallow tagged clone.
 *
 * v3.9 M2 adds one further operation: check out a compiled-in source
 * id. The only ids are ogre-next-deps and ogre-next. Their URLs,
 * commits, destinations, and the SDL2 submodule pin are not arguments.
 *
 * This is deliberately not a general Git command API.
 * The helper does not install Git.
 */

#ifndef MACGIT_H
#define MACGIT_H

#include <wx/string.h>

class wxWindow;

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
    wxWindow* parent,
    const wxString& url,
    const wxString& tag,
    const wxString& destination,
    const wxString& explanation,
    const wxString& confirmLabel);

// Report string returned to wizard.script.
wxString FormatMacGitClone(const MacGitCloneResult& result);


// Result of checking out one compiled-in source id.
// This is not a general clone result: the script cannot supply a URL,
// a commit, a destination, or a Git command.
struct MacGitAcquireResult
{
    bool launched;
    bool cancelled;
    bool alreadyPresent;
    long exitCode;
    bool success;

    wxString executable;
    wxString sourceId;
    wxString url;
    wxString commit;
    wxString destination;
    wxString submoduleCommit;
    wxString output;

    MacGitAcquireResult()
        : launched(false),
          cancelled(false),
          alreadyPresent(false),
          exitCode(-1),
          success(false)
    {}
};

// Consent first, unless the destination already is that exact checkout.
//
// ogre-next-deps checks out f7e23191f4fd89ed41134996bc47f9ee23d009df
// and then initialises submodules. Success requires src/SDL2 to be
// 5d249570393f7a37e037abf22cd6012a4cc56a71.
//
// ogre-next checks out 4859016ad2cc956a653dfc4e4a859505696c6509.
//
// Both destinations are under the user's Library/CodeBlocks directory.
// An existing directory that is not that checkout is left untouched.
// Cancel before launch does not start Git.
MacGitAcquireResult AcquireMacGitSource(
    wxWindow* parent,
    const wxString& sourceId,
    const wxString& explanation,
    const wxString& confirmLabel);

// Report string returned to wizard.script.
wxString FormatMacGitAcquire(const MacGitAcquireResult& result);

#endif // __WXMAC__

#endif // MACGIT_H
