/*
 * This file is part of the Code::Blocks IDE and licensed under the GNU General Public License, version 3
 * http://www.gnu.org/licenses/gpl-3.0.html
 *
 * v3.0 M2–M3: locate Homebrew, revalidate it, read its prefix, and query one formula.
 * No install. The helper does not know SFML headers, library names, or SFML 2 versus 3.
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

// Result of {brew, "--prefix", "--installed", formula}. Not a general argv result.
enum MacBrewFormulaState
{
    MacBrewFormulaRejected = 0,          // token refused; brew was not started
    MacBrewFormulaBrewUnavailable = 1,   // locate or --version failed; formula was not queried
    MacBrewFormulaMissing = 2,           // exit 1 and Homebrew's "not installed" error
    MacBrewFormulaInstalled = 3,         // exit 0 and one directory line
    MacBrewFormulaFailed = 4             // any other exit, including a name Homebrew does not know
};

struct MacBrewFormulaQuery
{
    int brewAccess;
    wxString executable;
    wxString formula;
    long queryExit;
    int state;
    wxString prefix;
    wxString errorOutput;

    MacBrewFormulaQuery()
        : brewAccess(MacBrewNotPresent),
          queryExit(-1),
          state(MacBrewFormulaRejected)
    {}
};

// Revalidates brew, then queries one formula. A failed --version does not become "missing".
MacBrewFormulaQuery QueryMacBrewFormula(const wxString& formula);

wxString FormatMacBrewFormula(const MacBrewFormulaQuery& query);

#endif // __WXMAC__

#endif // MACBREW_H
