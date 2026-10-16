/*
 * This file is part of the Code::Blocks IDE and licensed under the GNU Lesser General Public License, version 3
 * http://www.gnu.org/licenses/lgpl-3.0.html
 */

#include "sdk_precomp.h"

#ifndef CB_PRECOMP
    #include <wx/filename.h>
    #include <wx/intl.h>
    #include "cbeditor.h"
    #include "editormanager.h"
    #include "manager.h"
    #include "projectmanager.h"
    #include "compiler.h"
    #include "compileroptions.h"
    #include "globals.h"
    #include "filefilters.h"
#endif

#include "debuggeedescription.h"
#include "filefilters.h"

namespace
{
CompOption* FindCompOptionByBaseName(Compiler* compiler, const wxString& baseName)
{
    if (!compiler || baseName.empty())
        return nullptr;

    const wxString translated = wxGetTranslation(baseName);
    const CompilerOptions& opts = compiler->GetOptions();
    for (unsigned int i = 0; i < opts.GetCount(); ++i)
    {
        CompOption* copt = const_cast<CompilerOptions&>(opts).GetOption(i);
        if (!copt)
            continue;
        // Catalogue stores "Translated name  [flag]" (see CompilerOptions::AddOption).
        if (copt->name.StartsWith(translated) || copt->name.StartsWith(baseName))
            return copt;
    }
    return nullptr;
}

void EnsureFlagPresent(wxArrayString& arr, const wxString& flag)
{
    if (flag.empty())
        return;
    if (arr.Index(flag) == wxNOT_FOUND)
        arr.Add(flag);
}

void EnsureFlagAbsent(wxArrayString& arr, const wxString& flag)
{
    if (flag.empty())
        return;
    int idx = arr.Index(flag);
    while (idx != wxNOT_FOUND)
    {
        arr.RemoveAt(idx);
        idx = arr.Index(flag);
    }
}
} // namespace

bool DebuggeeDescriptionUtil::IsStandaloneDebugCandidate()
{
    if (Manager::Get()->GetProjectManager()->GetActiveProject())
        return false;

    cbEditor* ed = Manager::Get()->GetEditorManager()->GetBuiltinActiveEditor();
    if (!ed)
        return false;

    // Never-saved buffers are candidates only after Save/Save As succeeds later.
    if (!ed->IsOK())
        return true;

    const wxString filename = ed->GetFilename();
    if (filename.empty())
        return false;

    return FileTypeOf(filename) == ftSource;
}

bool DebuggeeDescriptionUtil::SaveActiveSourceForStandaloneDebug(wxString& sourcePath)
{
    sourcePath.clear();
    cbEditor* ed = Manager::Get()->GetEditorManager()->GetBuiltinActiveEditor();
    if (!ed)
        return false;

    if (!ed->Save())
        return false;

    if (!ed->IsOK())
        return false;

    sourcePath = ed->GetFilename();
    if (sourcePath.empty())
        return false;

    return FileTypeOf(sourcePath) == ftSource;
}

bool DebuggeeDescriptionUtil::MakeStandalone(const wxString& sourcePath, DebuggeeDescription& out)
{
    out = DebuggeeDescription();
    if (sourcePath.empty() || FileTypeOf(sourcePath) != ftSource)
        return false;

    wxFileName fname(sourcePath);
    if (!fname.IsOk())
        return false;

    out.workingDirectory = fname.GetPath(wxPATH_GET_VOLUME);
    if (out.workingDirectory.empty())
        out.workingDirectory = wxT(".");

    fname.SetExt(FileFilters::EXECUTABLE_EXT);
    out.executablePath = fname.GetFullPath();
    out.arguments.clear();
    out.isConsole = true;
    out.project = nullptr;
    out.target = nullptr;
    return out.IsValid();
}

DebuggeeDescriptionUtil::StandaloneDebugCompileOptionsGuard::StandaloneDebugCompileOptionsGuard(Compiler* compiler)
    : m_compiler(compiler),
      m_savedModified(false),
      m_ok(false),
      m_applied(false)
{
    if (!m_compiler)
    {
        m_error = _("No default compiler is configured.");
        return;
    }

    m_savedCompilerOptions = m_compiler->GetCompilerOptions();
    m_savedLinkerOptions = m_compiler->GetLinkerOptions();
    m_savedModified = m_compiler->GetModified();

    CompOption* debugOpt = FindCompOptionByBaseName(m_compiler, wxT("Produce debugging symbols"));
    if (!debugOpt || (debugOpt->option.empty() && debugOpt->additionalLibs.empty()))
    {
        m_error = wxString::Format(
            _("Compiler '%s' has no 'Produce debugging symbols' option in its catalogue."),
            m_compiler->GetName());
        return;
    }

    wxArrayString compilerOpts = m_savedCompilerOptions;
    wxArrayString linkerOpts = m_savedLinkerOptions;

    CompOption* stripOpt = FindCompOptionByBaseName(m_compiler,
                                                    wxT("Strip all symbols from binary (minimizes size)"));
    if (stripOpt)
    {
        EnsureFlagAbsent(compilerOpts, stripOpt->option);
        EnsureFlagAbsent(linkerOpts, stripOpt->option);
        EnsureFlagAbsent(compilerOpts, stripOpt->additionalLibs);
        EnsureFlagAbsent(linkerOpts, stripOpt->additionalLibs);
    }

    // Honour supersedes listed on the debug CompOption (often "-s") without assuming a fixed flag.
    if (!debugOpt->supersedes.empty())
    {
        const wxArrayString supersede = GetArrayFromString(debugOpt->supersedes, wxT(' '));
        for (size_t i = 0; i < supersede.GetCount(); ++i)
        {
            EnsureFlagAbsent(compilerOpts, supersede[i]);
            EnsureFlagAbsent(linkerOpts, supersede[i]);
        }
    }

    EnsureFlagPresent(compilerOpts, debugOpt->option);
    EnsureFlagPresent(linkerOpts, debugOpt->additionalLibs);

    m_compiler->SetCompilerOptions(compilerOpts);
    m_compiler->SetLinkerOptions(linkerOpts);
    m_applied = true;
    m_ok = true;
}

DebuggeeDescriptionUtil::StandaloneDebugCompileOptionsGuard::~StandaloneDebugCompileOptionsGuard()
{
    if (m_compiler && m_applied)
    {
        m_compiler->SetCompilerOptions(m_savedCompilerOptions);
        m_compiler->SetLinkerOptions(m_savedLinkerOptions);
        m_compiler->SetModified(m_savedModified);
    }
}
