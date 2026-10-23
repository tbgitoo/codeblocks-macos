/*
 * This file is part of the Code::Blocks IDE and licensed under the GNU General Public License, version 3
 * http://www.gnu.org/licenses/gpl-3.0.html
 *
 * v3.3. Identify the compiler the wizard will use, remember one unpacked
 * directory per library id, and download one https zip after consent.
 * The helper does not know SFML headers, library names, or versions.
 */

#ifndef WINSDK_H
#define WINSDK_H

#include <wx/string.h>

#if defined(__WXMSW__)

struct WinSdkCompiler
{
    bool works;
    wxString master;
    wxString executable;
    wxString version;
    wxString machine;
    bool ucrt;
    wxString error;

    WinSdkCompiler()
        : works(false),
          ucrt(false)
    {}
};

// compilerId empty: the IDE's default compiler. gcc.exe is taken from that
// compiler's master path, not from PATH.
WinSdkCompiler ProbeWinSdkCompiler(const wxString& compilerId);
wxString FormatWinSdkCompiler(const WinSdkCompiler& probe);

struct WinSdkTree
{
    // "rejected", "missing", or "installed". installed means the saved
    // directory exists. This helper does not look inside it.
    wxString state;
    wxString prefix;
    // The one child directory, when the saved directory has exactly one.
    // Empty otherwise. The wizard script checks prefix, then this.
    wxString nested;

    WinSdkTree()
        : state(wxT("rejected"))
    {}
};

WinSdkTree QueryWinSdkTree(const wxString& libraryId);
wxString FormatWinSdkTree(const WinSdkTree& tree);

struct WinSdkFetch
{
    bool launched;
    bool cancelled;
    bool success;
    wxString prefix;
    wxString output;

    WinSdkFetch()
        : launched(false),
          cancelled(false),
          success(false)
    {}
};

// Consent dialog first. Cancel there does not connect. The button label is
// what the user sees; the URL is not shown. Unpacks straight into
// sdk\<id> and remembers that directory. SFML uses this.
WinSdkFetch FetchWinSdk(const wxString& url,
                        const wxString& libraryId,
                        const wxString& explanation,
                        const wxString& confirmLabel);
wxString FormatWinSdkFetch(const WinSdkFetch& result);

// Consent, then download into a disposable staging directory. The hash is
// checked before unpack. Archive names that are absolute, drive-qualified,
// contain "..", or are links are rejected and nothing is published.
// prefix is the unpacked directory that contains every marker (separated by
// ';'). This does not remember the directory.
WinSdkFetch StageWinSdk(const wxString& url,
                        const wxString& sha256,
                        const wxString& libraryId,
                        const wxString& markers,
                        const wxString& explanation,
                        const wxString& confirmLabel);

// Copy a staged directory to sdk\<id>\<edition> and remember that path.
// stagingPrefix must sit under this library's staging area. A note is stored
// beside the tree; the resolved directory is appended to it.
WinSdkFetch PublishWinSdk(const wxString& libraryId,
                          const wxString& edition,
                          const wxString& stagingPrefix,
                          const wxString& note);

#endif // __WXMSW__

#endif // WINSDK_H
