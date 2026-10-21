/*
 * This file is part of the Code::Blocks IDE and licensed under the GNU General Public License, version 3
 * http://www.gnu.org/licenses/gpl-3.0.html
 *
 * v3.0 M2: locate Homebrew, revalidate it, and read its prefix.
 * No formula query and no install. The helper does not know about SFML.
 */

#ifndef MACBREW_H
#define MACBREW_H

#include <wx/string.h>

#if defined(__WXMAC__)

// A saved path is only a location hint. Every probe runs --version again.
enum MacBrewAccess
{
    MacBrewNotPresent = 0,
    MacBrewPresentUnusable = 1,
    MacBrewWorks = 2
};

struct MacBrewProbe
{
    int access;
    wxString executable;
    wxString versionOutput;
    wxString errorOutput;
    long versionExit;
    wxString prefix;
    long prefixExit;
    bool savedPathMissing;

    MacBrewProbe()
        : access(MacBrewNotPresent),
          versionExit(-1),
          prefixExit(-1),
          savedPathMissing(false)
    {}
};

// wantPrefix runs "brew --prefix" only after --version has succeeded.
MacBrewProbe ProbeMacBrew(bool wantPrefix);

// One report string for the wizard. This is not a general brew command API.
wxString FormatMacBrewProbe(const MacBrewProbe& probe);

#endif // __WXMAC__

#endif // MACBREW_H
