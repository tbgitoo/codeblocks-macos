/*
 * This file is part of the Code::Blocks IDE and licensed under the GNU General Public License, version 3
 * http://www.gnu.org/licenses/gpl-3.0.html
 *
 * $Revision: 14024 $
 * $Id: astyleplugin.cpp 14024 2026-09-15 16:10:18Z wh11204 $
 * $HeadURL: https://svn.code.sf.net/p/codeblocks/code/trunk/src/plugins/astyle/astyleplugin.cpp $
 */

#include "sdk.h"

#ifndef CB_PRECOMP
    #include <algorithm>
    #include <sstream>
    #include <string>
    #include <vector>

    #include <wx/msgdlg.h>
    #include <wx/xrc/xmlres.h>
    #include <wx/fs_zip.h>
    #include <wx/strconv.h>

    #include "cbeditor.h"
    #include "cbexception.h"
    #include "cbproject.h"
    #include "configmanager.h"
    #include "editormanager.h"
    #include "manager.h"
    #include "projectmanager.h"
    #include "sdk_events.h"
#endif

#include <wx/progdlg.h>
#include "cbstyledtextctrl.h"

#include "astyleplugin.h"
#include "astyleconfigdlg.h"
#include "formattersettings.h"
#include "asstreamiterator.h"

using std::istringstream;
using std::string;

namespace
{
    const int idCodeFormatterReindent = wxNewId();
    const int idCodeFormatterActiveFile = wxNewId();
    const int idCodeFormatterProject = wxNewId();
}

BEGIN_EVENT_TABLE( AStylePlugin, cbPlugin )
    EVT_MENU( idCodeFormatterReindent, AStylePlugin::OnReindentActiveFile )
    EVT_UPDATE_UI( idCodeFormatterReindent, AStylePlugin::OnReindentUpdateUI )
    EVT_MENU( idCodeFormatterActiveFile, AStylePlugin::OnFormatActiveFile )
    EVT_MENU( idCodeFormatterProject, AStylePlugin::OnFormatProject )
END_EVENT_TABLE()

// this auto-registers the plugin

namespace
{
    PluginRegistrant<AStylePlugin> reg("AStylePlugin");
}

AStylePlugin::AStylePlugin()
{
    //ctor

    if (!Manager::LoadResource("Astyle.zip"))
        NotifyMissingFile("Astyle.zip");
}

AStylePlugin::~AStylePlugin()
{
    //dtor
}

void AStylePlugin::OnAttach()
{
    Manager::Get()->RegisterEventSink(cbEVT_MENUBAR_CREATE_END, new cbEventFunctor<AStylePlugin, CodeBlocksEvent>(this, &AStylePlugin::OnMenubarCreateEnd));
    // The Edit menu already exists: tool plugins are attached after the first menubar is built,
    // and that build does not call BuildMenu for them.
    InsertReindentEditMenuItem();
}

void AStylePlugin::OnRelease(bool appShutDown)
{
    if (!appShutDown)
        RemoveReindentEditMenuItem();
}

void AStylePlugin::OnMenubarCreateEnd(CodeBlocksEvent& /*event*/)
{
    InsertReindentEditMenuItem();
}

void AStylePlugin::InsertReindentEditMenuItem()
{
    if (!IsAttached())
        return;

    wxFrame* frame = Manager::Get()->GetAppFrame();
    if (!frame)
        return;

    wxMenuBar* menuBar = frame->GetMenuBar();
    if (!menuBar || menuBar->FindItem(idCodeFormatterReindent))
        return;

    const int editPos = menuBar->FindMenu(_("&Edit"));
    if (editPos == wxNOT_FOUND)
        return;

    wxMenu* editMenu = menuBar->GetMenu(editPos);
    if (!editMenu)
        return;

    size_t position = 0;
    if (!editMenu->FindChildItem(XRCID("idEditShowCallTip"), &position))
        return;

    editMenu->Insert(position, idCodeFormatterReindent, _("Re-indent"), _("Re-indent the selected source code (selected line) in the current file"));
}

void AStylePlugin::RemoveReindentEditMenuItem()
{
    wxFrame* frame = Manager::Get()->GetAppFrame();
    if (!frame)
        return;

    wxMenuBar* menuBar = frame->GetMenuBar();
    if (!menuBar)
        return;

    wxMenu* editMenu = nullptr;
    if (menuBar->FindItem(idCodeFormatterReindent, &editMenu) && editMenu)
        editMenu->Delete(idCodeFormatterReindent);
}


cbConfigurationPanel* AStylePlugin::GetConfigurationPanel(wxWindow* parent)
{
    AstyleConfigDlg* dlg = new AstyleConfigDlg(parent);
    // deleted by the caller

    return dlg;
}

void AStylePlugin::BuildModuleMenu( const ModuleType type, wxMenu* menu, const FileTreeData* data )
{
    if ( !menu || !IsAttached() )
        return;

    switch ( type )
    {
        case mtEditorManager:
        {
            const wxString reindentLabel = _("Re-indent");
            const wxString formatLabel = _("Format using AStyle");
            const int position = Manager::Get()->GetPluginManager()->FindSortedMenuItemPosition(*menu, formatLabel);

            menu->Insert(position, idCodeFormatterReindent, reindentLabel, _("Re-indent the selected source code (selected line) in the current file"));
            menu->Insert(position + 1, idCodeFormatterActiveFile, formatLabel, _( "Format the selected source code (selected line) in the current file" ) );
            break;
        }

        case mtProjectManager:
            if ( data ) switch ( data->GetKind() )
                {
                    case FileTreeData::ftdkProject:
                        menu->AppendSeparator();
                        menu->Append( idCodeFormatterProject, _( "Format this project (AStyle)" ), _( "Format the source code in this project" ) );
                        break;

                    case FileTreeData::ftdkFile:
                        menu->AppendSeparator();
                        menu->Append( idCodeFormatterProject, _( "Format this file (AStyle)" ), _( "Format the source code in this file" ) );
                        break;

                    default:
                        // Do nothing.
                        break;
                }
            break;

        default:
            break;
    }
}

void AStylePlugin::OnFormatProject( wxCommandEvent& /*event*/ )
{
    ProjectManager* manager = Manager::Get()->GetProjectManager();
    wxTreeCtrl *tree = manager->GetUI().GetTree();

    if ( !tree )
        return;

    wxTreeItemId treeItem =  manager->GetUI().GetTreeSelection();

    if ( !treeItem.IsOk() )
        return;

    const FileTreeData *data = static_cast<FileTreeData*>( tree->GetItemData( treeItem ) );

    if ( !data )
        return;

    switch ( data->GetKind() )
    {
        case FileTreeData::ftdkProject:
            {
                cbProject* prj = data->GetProject();
                wxProgressDialog progressDlg(_("Please wait"), _("Formatting..."), prj->GetFilesCount(), 0, wxPD_CAN_ABORT|wxPD_AUTO_HIDE|wxPD_SMOOTH );
                progressDlg.Show();
                int i = 0;
                for (FilesList::iterator it = prj->GetFilesList().begin(); it != prj->GetFilesList().end(); ++it)
                {
                    ProjectFile* pf = *it;
                    wxString filename = pf->file.GetFullPath();

                    FileType fileType = FileTypeOf( filename );
                    if ( fileType == ftSource || fileType == ftHeader || fileType == ftTemplateSource )
                    {
                        FormatFile( filename );
                        if (!progressDlg.Update(i++, wxString::Format(_("Formatting %s"), pf->relativeFilename)))
                            break;
                    }
                }
            }
            break;

        case FileTreeData::ftdkFile:
            {
                ProjectFile* f = data->GetProjectFile();
                if ( f )
                    FormatFile( f->file.GetFullPath() );
            }
            break;

        default:
            break;
    }
}

void AStylePlugin::OnFormatActiveFile( wxCommandEvent& /*event*/ )
{
        Execute();
}

void AStylePlugin::OnReindentActiveFile( wxCommandEvent& /*event*/ )
{
    if (!IsAttached())
        return;

    cbEditor *ed = Manager::Get()->GetEditorManager()->GetBuiltinActiveEditor();
    if (!ed)
        return;

    FormatEditor(ed, false);
}

void AStylePlugin::OnReindentUpdateUI( wxUpdateUIEvent& event )
{
    if (Manager::IsAppShuttingDown())
    {
        event.Enable(false);
        return;
    }

    event.Enable(Manager::Get()->GetEditorManager()->GetBuiltinActiveEditor() != nullptr);
}

int AStylePlugin::Execute()
{
    if (!IsAttached())
        return -1;

    cbEditor *ed = Manager::Get()->GetEditorManager()->GetBuiltinActiveEditor();

    if (!ed)
        return 0;

    FormatEditor( ed );

    return 0;
}

void AStylePlugin::FormatFile( const wxString &filename )
{
    cbEditor* ed = Manager::Get()->GetEditorManager()->IsBuiltinOpen( filename );

    if ( ed ) // File is already open
        FormatEditor( ed );
    else
    {
        // File is not open.  We must open it.
        ed = Manager::Get()->GetEditorManager()->Open( filename );

        if ( ed )
        {
            bool changed = FormatEditor( ed );

            if ( !changed ) // We opened a file and it didn't change.  Close it.
                Manager::Get()->GetEditorManager()->Close( filename );
        }
    }
}

namespace
{
    struct LineHunk
    {
        int oldStart;
        int oldCount;
        int newStart;
        int newCount;
    };

    // Scintilla keeps a final empty line when the buffer ends with an EOL.
    void SplitDocumentLines(const wxString& text, std::vector<wxString>& lines)
    {
        lines.clear();
        const size_t len = text.length();
        size_t start = 0;
        for (size_t i = 0; i < len; ++i)
        {
            const wxChar ch = text[i];
            if (ch != _T('\n') && ch != _T('\r'))
                continue;

            if (ch == _T('\r') && i + 1 < len && text[i + 1] == _T('\n'))
                ++i;

            lines.push_back(text.Mid(start, (i + 1) - start));
            start = i + 1;
        }
        lines.push_back(text.Mid(start));
    }

    void AddLineHunk(std::vector<LineHunk>& hunks, int oldStart, int oldCount, int newStart, int newCount)
    {
        if (oldCount == 0 && newCount == 0)
            return;

        if (!hunks.empty())
        {
            LineHunk& prev = hunks.back();
            if (prev.oldStart + prev.oldCount == oldStart && prev.newStart + prev.newCount == newStart)
            {
                prev.oldCount += oldCount;
                prev.newCount += newCount;
                return;
            }
        }

        hunks.push_back(LineHunk{oldStart, oldCount, newStart, newCount});
    }

    // A point on a shortest edit path, used to keep the diff in linear space.
    void LineDiffMidpoint(const std::vector<wxString>& oldLines, const std::vector<wxString>& newLines,
                          int oldBegin, int oldEnd, int newBegin, int newEnd,
                          int& splitOld, int& splitNew)
    {
        const int oldSpan = oldEnd - oldBegin;
        const int newSpan = newEnd - newBegin;
        const int max = oldSpan + newSpan;
        const int delta = oldSpan - newSpan;
        const bool odd = (delta & 1) != 0;
        const int size = 2 * max + 1;
        std::vector<int> forward(size, 0);
        std::vector<int> backward(size, 0);
        const auto index = [max](int diagonal) { return diagonal + max; };

        forward[index(1)] = 0;
        backward[index(1)] = 0;

        for (int depth = 0; depth <= (max + 1) / 2; ++depth)
        {
            for (int diagonal = -depth; diagonal <= depth; diagonal += 2)
            {
                int oldPos;
                if (diagonal == -depth || (diagonal != depth && forward[index(diagonal - 1)] < forward[index(diagonal + 1)]))
                    oldPos = forward[index(diagonal + 1)];
                else
                    oldPos = forward[index(diagonal - 1)] + 1;

                int newPos = oldPos - diagonal;
                while (oldPos < oldSpan && newPos < newSpan && oldLines[oldBegin + oldPos] == newLines[newBegin + newPos])
                {
                    ++oldPos;
                    ++newPos;
                }
                forward[index(diagonal)] = oldPos;

                if (odd && depth > 0 && (diagonal - delta) >= -(depth - 1) && (diagonal - delta) <= (depth - 1))
                {
                    if (oldPos + backward[index(delta - diagonal)] >= oldSpan)
                    {
                        splitOld = oldBegin + oldPos;
                        splitNew = newBegin + (oldPos - diagonal);
                        return;
                    }
                }
            }

            for (int diagonal = -depth; diagonal <= depth; diagonal += 2)
            {
                int oldPos;
                if (diagonal == -depth || (diagonal != depth && backward[index(diagonal - 1)] < backward[index(diagonal + 1)]))
                    oldPos = backward[index(diagonal + 1)];
                else
                    oldPos = backward[index(diagonal - 1)] + 1;

                int newPos = oldPos - diagonal;
                while (oldPos < oldSpan && newPos < newSpan
                       && oldLines[oldEnd - 1 - oldPos] == newLines[newEnd - 1 - newPos])
                {
                    ++oldPos;
                    ++newPos;
                }
                backward[index(diagonal)] = oldPos;

                if (!odd && (diagonal - delta) >= -depth && (diagonal - delta) <= depth)
                {
                    if (oldPos + forward[index(delta - diagonal)] >= oldSpan)
                    {
                        splitOld = oldBegin + (oldSpan - oldPos);
                        splitNew = newBegin + (newSpan - (oldPos - diagonal));
                        return;
                    }
                }
            }
        }

        splitOld = oldEnd;
        splitNew = newEnd;
    }

    void DiffLineRange(const std::vector<wxString>& oldLines, const std::vector<wxString>& newLines,
                       int oldBegin, int oldEnd, int newBegin, int newEnd, std::vector<LineHunk>& hunks)
    {
        while (oldBegin < oldEnd && newBegin < newEnd && oldLines[oldBegin] == newLines[newBegin])
        {
            ++oldBegin;
            ++newBegin;
        }
        while (oldBegin < oldEnd && newBegin < newEnd && oldLines[oldEnd - 1] == newLines[newEnd - 1])
        {
            --oldEnd;
            --newEnd;
        }

        if (oldBegin == oldEnd && newBegin == newEnd)
            return;
        if (oldBegin == oldEnd)
        {
            AddLineHunk(hunks, oldBegin, 0, newBegin, newEnd - newBegin);
            return;
        }
        if (newBegin == newEnd)
        {
            AddLineHunk(hunks, oldBegin, oldEnd - oldBegin, newBegin, 0);
            return;
        }

        int splitOld = oldBegin;
        int splitNew = newBegin;
        LineDiffMidpoint(oldLines, newLines, oldBegin, oldEnd, newBegin, newEnd, splitOld, splitNew);
        if (splitOld < oldBegin || splitOld > oldEnd || splitNew < newBegin || splitNew > newEnd
            || (splitOld == oldBegin && splitNew == newBegin)
            || (splitOld == oldEnd && splitNew == newEnd))
        {
            AddLineHunk(hunks, oldBegin, oldEnd - oldBegin, newBegin, newEnd - newBegin);
            return;
        }

        DiffLineRange(oldLines, newLines, oldBegin, splitOld, newBegin, splitNew, hunks);
        DiffLineRange(oldLines, newLines, splitOld, oldEnd, splitNew, newEnd, hunks);
    }

    void SplitContentAndEOL(const wxString& line, wxString& content, wxString& eol)
    {
        if (line.EndsWith(wxT("\r\n")))
        {
            content = line.Mid(0, line.length() - 2);
            eol = wxT("\r\n");
        }
        else if (!line.empty() && (line.Last() == _T('\n') || line.Last() == _T('\r')))
        {
            content = line.Mid(0, line.length() - 1);
            eol = line.Last();
        }
        else
        {
            content = line;
            eol.clear();
        }
    }

    // Replace the text of one existing line. Keeping its EOL in place marks only that
    // line and leaves a breakpoint on it where it is.
    void ReplaceLineContents(cbStyledTextCtrl* control, int line, const wxString& newLine)
    {
        const wxString current = control->GetLine(line);
        if (current == newLine)
            return;

        wxString oldContent, oldEol, newContent, newEol;
        SplitContentAndEOL(current, oldContent, oldEol);
        SplitContentAndEOL(newLine, newContent, newEol);

        const int start = control->PositionFromLine(line);
        if (oldEol == newEol)
        {
            const int end = control->GetLineEndPosition(line);
            if (end > start)
                control->DeleteRange(start, end - start);
            if (!newContent.empty())
                control->InsertText(start, newContent);
            return;
        }

        int end = control->GetLength();
        if (line + 1 < control->GetLineCount())
            end = control->PositionFromLine(line + 1);
        if (end > start)
            control->DeleteRange(start, end - start);
        if (!newLine.empty())
            control->InsertText(start, newLine);
    }

    void ApplyLineHunk(cbStyledTextCtrl* control, const LineHunk& hunk, const std::vector<wxString>& newLines)
    {
        const int common = hunk.oldCount < hunk.newCount ? hunk.oldCount : hunk.newCount;
        for (int i = 0; i < common; ++i)
            ReplaceLineContents(control, hunk.oldStart + i, newLines[hunk.newStart + i]);

        if (hunk.newCount > hunk.oldCount)
        {
            wxString inserted;
            for (int i = common; i < hunk.newCount; ++i)
                inserted += newLines[hunk.newStart + i];

            const int line = hunk.oldStart + common;
            const int pos = line >= control->GetLineCount() ? control->GetLength() : control->PositionFromLine(line);
            if (!inserted.empty())
                control->InsertText(pos, inserted);
        }
        else if (hunk.oldCount > hunk.newCount)
        {
            const int line = hunk.oldStart + common;
            const int count = hunk.oldCount - common;
            const int start = control->PositionFromLine(line);
            const int end = (line + count >= control->GetLineCount()) ? control->GetLength()
                                                                       : control->PositionFromLine(line + count);
            if (end > start)
                control->DeleteRange(start, end - start);
        }
    }

    // Edit only the lines that differ, so the change bar does not mark the whole buffer.
    // caretPos is restored by line and column; insertions above it move that line down.
    bool ApplyChangedLines(cbStyledTextCtrl* control, const wxString& desiredText, int caretPos)
    {
        std::vector<wxString> oldLines;
        const int lineCount = control->GetLineCount();
        oldLines.reserve(lineCount);
        wxString oldJoined;
        for (int line = 0; line < lineCount; ++line)
        {
            const wxString text = control->GetLine(line);
            oldLines.push_back(text);
            oldJoined += text;
        }
        if (oldJoined != control->GetText())
            return false;

        std::vector<wxString> newLines;
        SplitDocumentLines(desiredText, newLines);
        wxString newJoined;
        for (const wxString& line : newLines)
            newJoined += line;
        if (newJoined != desiredText)
            return false;

        const int caretLine = control->LineFromPosition(caretPos);
        const int caretCol = caretPos - control->PositionFromLine(caretLine);

        std::vector<LineHunk> hunks;
        DiffLineRange(oldLines, newLines, 0, static_cast<int>(oldLines.size()), 0, static_cast<int>(newLines.size()), hunks);

        int lineShift = 0;
        for (const LineHunk& hunk : hunks)
        {
            if (hunk.oldStart + hunk.oldCount <= caretLine)
                lineShift += hunk.newCount - hunk.oldCount;
            else
                break;
        }

        for (int i = static_cast<int>(hunks.size()) - 1; i >= 0; --i)
            ApplyLineHunk(control, hunks[i], newLines);

        int newLine = caretLine + lineShift;
        if (newLine < 0)
            newLine = 0;
        if (newLine >= control->GetLineCount())
            newLine = control->GetLineCount() - 1;
        const int lineStart = control->PositionFromLine(newLine);
        const int lineEnd = control->GetLineEndPosition(newLine);
        int newPos = lineStart + caretCol;
        if (newPos > lineEnd)
            newPos = lineEnd;
        control->GotoPos(newPos);
        return true;
    }
}

// Special code to compare strings which doesn't care
// about spaces leading up to the EOL.
static bool BuffersDiffer( const wxString &a, const wxString &b )
{
    const wxChar *aCurrent = a.c_str();
    const wxChar *bCurrent = b.c_str();
    const wxChar * const aEnd = aCurrent + a.Len();
    const wxChar * const bEnd = bCurrent + b.Len();

    while ( aCurrent != aEnd && bCurrent != bEnd )
    {
        if ( *aCurrent != *bCurrent )
        {
            // Check for varying space at EOL
            while ( *aCurrent == ' ' || *aCurrent == '\t' )
            {
                if ( ++aCurrent == aEnd )
                    break;
            }
            while ( *bCurrent == ' ' || *bCurrent == '\t' )
            {
                if ( ++bCurrent == bEnd )
                    break;
            }

            // Make sure it was at EOL
            if ( ( *aCurrent != '\r' && *aCurrent != '\n' ) || ( *bCurrent != '\r' && *bCurrent != '\n' ) )
                return true;
        }

        ++aCurrent;
        ++bCurrent;
    }

    while ( aCurrent != aEnd )
    {
        if ( *aCurrent != ' ' && *aCurrent != '\t' )
            return true;
        ++aCurrent;
    }

    while ( bCurrent != bEnd )
    {
        if ( *bCurrent != ' ' && *bCurrent != '\t' )
            return true;
        ++bCurrent;
    }

    return false;
}

bool AStylePlugin::FormatEditor( cbEditor *ed, bool applyConfiguredOptions )
{
    cbStyledTextCtrl* control = ed->GetControl();
    if (control->GetReadOnly())
    {
        cbMessageBox(_("The file is read-only!"), _("Error"), wxICON_ERROR);
        return false;
    }

    bool onlySelected = false;
    wxString edText;
    wxString selText;
    int leftBracesNumber = 0;
    const int pos = control->GetCurrentPos();
    int start = control->GetSelectionStart();
    int end = control->GetSelectionEnd();
    if (start != end)
    {
        onlySelected = true;
        start = control->GetLineIndentPosition(control->LineFromPosition(start));
        control->GotoPos(start);
        control->Home();
        start = control->GetCurrentPos();
        control->SetSelectionStart(start);
        end = control->GetLineEndPosition(control->LineFromPosition(end));
        control->SetSelectionEnd(end);
        selText = control->GetTextRange(start, end);

        wxChar ch;
        int findBracesPos = start;
        while (--findBracesPos > 0 )
        {
            ch = control->GetCharAt(findBracesPos);
            int style = control->GetStyleAt(findBracesPos);
            int lexer = control->GetLexer();
            if (lexer == wxSCI_LEX_CPP)
            {
                if (   style == wxSCI_C_COMMENT           || style == wxSCI_C_COMMENTDOC
                    || style == wxSCI_C_COMMENTDOCKEYWORD || style == wxSCI_C_COMMENTDOCKEYWORDERROR
                    || style == wxSCI_C_COMMENTLINE       || style == wxSCI_C_COMMENTLINEDOC
                    || style == wxSCI_C_STRING            || style == wxSCI_C_CHARACTER)
                    continue;
            }
            else if (lexer == wxSCI_LEX_D)
            {
                if (   style == wxSCI_D_COMMENT           || style == wxSCI_D_COMMENTDOC
                    || style == wxSCI_D_COMMENTDOCKEYWORD || style == wxSCI_D_COMMENTDOCKEYWORDERROR
                    || style == wxSCI_D_COMMENTLINE       || style == wxSCI_D_COMMENTLINEDOC
                    || style == wxSCI_D_STRING            || style == wxSCI_D_CHARACTER)
                    continue;
            }

            if      (ch == _T('}')) --leftBracesNumber;
            else if (ch == _T('{')) ++leftBracesNumber;
        }

        for (int i = leftBracesNumber; i > 0; --i)
            edText.Append(_T('{'));
        edText.Append(selText);
    }
    else
        edText = control->GetText();

    wxString formattedText;

    astyle::ASFormatter formatter;

    // Configured formatting reads the user's stored AStyle options.
    // Re-indent skips that and keeps the formatter's built-in defaults.
    // The stored configuration is not read or written on that path.
    if (applyConfiguredOptions)
    {
        FormatterSettings settings;
        settings.ApplyTo(formatter);
    }

    const wxString& eolChars = GetEOLStr(control->GetEOLMode());

    if (edText.size() && edText.Last() != _T('\r') && edText.Last() != _T('\n') && !onlySelected)
        edText += eolChars;

    ASStreamIterator *asi = new ASStreamIterator(ed, edText.wx_str());

    formatter.init(asi);

    int lineCounter = 0;
    std::vector<int> new_bookmark;
    std::vector<int> ed_breakpoints;

    // hack: we need to evaluate the special case of having a bookmark in the first line here
    if (!onlySelected)
    {
        if (ed->HasBookmark(0))
            new_bookmark.push_back(0);
        if (ed->HasBreakpoint(0))
            ed_breakpoints.push_back(0);
    }

    wxSetCursor(*wxHOURGLASS_CURSOR);

    while (formatter.hasMoreLines())
    {
        formattedText << cbC2U(formatter.nextLine().c_str());

        if (formatter.hasMoreLines())
            formattedText << eolChars;

        ++lineCounter;

        if (asi->FoundBookmark())
        {
            new_bookmark.push_back(lineCounter);
            asi->ClearFoundBookmark();
        }
        if (asi->FoundBreakpoint())
        {
            ed_breakpoints.push_back(lineCounter);
            asi->ClearFoundBreakpoint();
        }
    }

    if (onlySelected && leftBracesNumber > 0)
    {
        while (leftBracesNumber > 0)
        {
            --leftBracesNumber;
            formattedText = formattedText.SubString(formattedText.Find(_T('{')) + 1, formattedText.Length());
        }
        formattedText = formattedText.SubString(formattedText.Find(eolChars) + eolChars.Length(), formattedText.Length());
    }

    bool changed = BuffersDiffer( formattedText, !onlySelected ? edText : selText);

    if ( changed )
    {
        control->BeginUndoAction();
        // Replacing the whole buffer marks every line on the change bar. Apply the
        // line diff instead, and keep SetText only if that line model does not match.
        wxString desiredText = formattedText;
        if (onlySelected)
            desiredText = control->GetTextRange(0, start) + formattedText + control->GetTextRange(end, control->GetLength());

        const bool appliedInPlace = ApplyChangedLines(control, desiredText, pos);
        if (!appliedInPlace)
        {
            if (onlySelected)
                control->ReplaceSelection(formattedText);
            else
                control->SetText(formattedText);

            for (std::vector<int>::const_iterator i = new_bookmark.begin(); i != new_bookmark.end(); ++i)
                ed->ToggleBookmark(*i);

            for (std::vector<int>::const_iterator i = ed_breakpoints.begin(); i != ed_breakpoints.end(); ++i)
                ed->ToggleBreakpoint(*i);

            // SetText/ReplaceSelection drop the caret. Edits above keep it themselves.
            control->GotoPos(pos);
        }

        control->EndUndoAction();
        ed->SetModified(true);
    }

    wxSetCursor(wxNullCursor);

    return changed;
}
