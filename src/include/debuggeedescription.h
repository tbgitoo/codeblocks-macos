/*
 * This file is part of the Code::Blocks IDE and licensed under the GNU Lesser General Public License, version 3
 * http://www.gnu.org/licenses/lgpl-3.0.html
 */

#ifndef DEBUGGEEDESCRIPTION_H
#define DEBUGGEEDESCRIPTION_H

#include "settings.h"
#include <wx/string.h>

class cbProject;
class ProjectBuildTarget;
class Compiler;

/** Narrow description of what is being debugged.
  *
  * Used primarily for the no-project (standalone) Debug path so the debugger
  * need not invent a fake cbProject. Project Debug may keep using existing
  * target-based code; converting that path is optional and not required.
  */
struct DLLIMPORT DebuggeeDescription
{
    wxString executablePath;
    wxString workingDirectory;
    wxString arguments;           ///< program args (empty for standalone)
    bool     isConsole = true;    ///< console classification for existing Debug console branches
    cbProject* project = nullptr;
    ProjectBuildTarget* target = nullptr;

    bool IsValid() const { return !executablePath.empty(); }
};

namespace DebuggeeDescriptionUtil
{
    /** Active builtin editor is a saved/saveable ftSource file and there is no active project. */
    DLLIMPORT bool IsStandaloneDebugCandidate();

    /** Save active editor if needed (existing CB Save / Save As). On success, fill @a sourcePath. */
    DLLIMPORT bool SaveActiveSourceForStandaloneDebug(wxString& sourcePath);

    /** Build description from a filesystem source path using single-file Run heuristics. */
    DLLIMPORT bool MakeStandalone(const wxString& sourcePath, DebuggeeDescription& out);

    /** Temporarily ensure the compiler's "Produce debugging symbols" CompOption is applied
      * and strip CompOption is not. Restores previous option arrays in the destructor.
      * Does not hard-code "-g" / "-s"; resolves flags from the compiler's option catalogue.
      */
    class DLLIMPORT StandaloneDebugCompileOptionsGuard
    {
        public:
            explicit StandaloneDebugCompileOptionsGuard(Compiler* compiler);
            ~StandaloneDebugCompileOptionsGuard();

            bool IsOk() const { return m_ok; }
            wxString ErrorMessage() const { return m_error; }

        private:
            Compiler* m_compiler;
            wxArrayString m_savedCompilerOptions;
            wxArrayString m_savedLinkerOptions;
            bool m_savedModified;
            bool m_ok;
            bool m_applied;
            wxString m_error;
    };
}

#endif // DEBUGGEEDESCRIPTION_H
