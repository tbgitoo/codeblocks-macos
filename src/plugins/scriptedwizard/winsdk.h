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
// what the user sees; the URL is not shown.
WinSdkFetch FetchWinSdk(const wxString& url,
                        const wxString& libraryId,
                        const wxString& explanation,
                        const wxString& confirmLabel);
wxString FormatWinSdkFetch(const WinSdkFetch& result);

#endif // __WXMSW__

#endif // WINSDK_H
