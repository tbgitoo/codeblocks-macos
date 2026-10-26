/*
 * This file is part of the Code::Blocks IDE and licensed under the GNU General Public License, version 3
 * http://www.gnu.org/licenses/gpl-3.0.html
 *
 * $Revision: 14024 $
 * $Id: compilermessages.cpp 14024 2026-09-15 16:10:18Z wh11204 $
 * $HeadURL: https://svn.code.sf.net/p/codeblocks/code/trunk/src/plugins/compilergcc/compilermessages.cpp $
 */

#include "sdk.h"
#ifndef CB_PRECOMP
    #include <wx/intl.h>
    #include <wx/listctrl.h>
    #include <wx/menu.h>
    #include <wx/string.h>
    #include <wx/utils.h>
    #include "globals.h"
    #include "manager.h"
    #include "configmanager.h"
#endif
#include <wx/clipbrd.h>
#include <wx/dataobj.h>
#include "compilererrors.h"
#include "compilermessages.h"
#include "prep.h"

namespace
{
    int idList = wxNewId();
    int idMenuFit = wxNewId();
    int idMenuAutoFit = wxNewId();
    int idMenuStackOverflow = wxNewId();

    bool IsEdgePunct(wxChar c)
    {
        switch (c)
        {
            case '\'': case '"': case '`':
            case '(': case ')': case '[': case ']': case '{': case '}':
            case ',': case ';': case ':': case '.':
                return true;
            default:
                return false;
        }
    }

    wxString TrimSearchWord(const wxString& word)
    {
        size_t begin = 0;
        size_t end = word.length();
        while (begin < end && IsEdgePunct(word[begin]))
            ++begin;
        while (end > begin && IsEdgePunct(word[end - 1]))
            --end;
        return word.Mid(begin, end - begin);
    }

    bool AtTokenStart(const wxString& text, size_t i)
    {
        if (i == 0)
            return true;
        const wxChar prev = text[i - 1];
        return wxIsspace(prev) || prev == '\'' || prev == '"' || prev == '(' || prev == '<';
    }

    bool StartsAbsolutePath(const wxString& text, size_t i)
    {
        if (text[i] == '/' || text[i] == '\\')
            return true;
        if (text[i] == '~' && (i + 1 >= text.length() || text[i + 1] == '/' || text[i + 1] == '\\'))
            return true;
        if (i + 2 < text.length() && wxIsalpha(text[i]) && text[i + 1] == ':'
                && (text[i + 2] == '\\' || text[i + 2] == '/'))
            return true;
        return false;
    }

    // file.cpp:12 or file.cpp:12:3, not a bare header name such as string.h.
    bool IsFileLocation(const wxString& token)
    {
        const wxString lower = token.Lower();
        const wxChar* exts[] = {
            wxT(".cpp"), wxT(".cxx"), wxT(".cc"), wxT(".c"), wxT(".hpp"), wxT(".hxx"),
            wxT(".hh"), wxT(".h"), wxT(".ipp"), wxT(".inl"), wxT(".mm"), wxT(".m"),
            wxT(".tcc"), nullptr
        };
        for (int e = 0; exts[e]; ++e)
        {
            const wxString ext(exts[e]);
            int pos = 0;
            while (pos < (int)lower.length())
            {
                const int found = lower.Mid(pos).Find(ext);
                if (found == wxNOT_FOUND)
                    break;
                const size_t after = (size_t)pos + (size_t)found + ext.length();
                pos = (int)after;
                if (after >= lower.length())
                    break;
                if ((lower[after] == ':' || lower[after] == '(') && after + 1 < lower.length()
                        && wxIsdigit(lower[after + 1]))
                    return true;
            }
        }
        return false;
    }

    bool SegmentContinuesPath(const wxString& segment)
    {
        if (segment.empty())
            return false;
        if (segment.Find('/') != wxNOT_FOUND || segment.Find('\\') != wxNOT_FOUND)
            return true;
        return IsFileLocation(segment);
    }

    // Drop absolute paths, including ones with spaces such as "/Users/me/My Project/file.cpp:12:3:".
    wxString StripAbsolutePaths(const wxString& input)
    {
        wxString out;
        size_t i = 0;
        while (i < input.length())
        {
            if (AtTokenStart(input, i) && StartsAbsolutePath(input, i))
            {
                size_t end = i;
                while (end < input.length() && !wxIsspace(input[end])
                        && input[end] != '\'' && input[end] != '"')
                    ++end;
                for (;;)
                {
                    size_t space = end;
                    while (space < input.length() && wxIsspace(input[space]))
                        ++space;
                    if (space == end || space >= input.length())
                        break;
                    size_t segEnd = space;
                    while (segEnd < input.length() && !wxIsspace(input[segEnd])
                            && input[segEnd] != '\'' && input[segEnd] != '"')
                        ++segEnd;
                    if (!SegmentContinuesPath(input.Mid(space, segEnd - space)))
                        break;
                    end = segEnd;
                }
                if (!out.empty() && !wxIsspace(out.Last()))
                    out << wxT(' ');
                i = end;
                continue;
            }
            out << input[i];
            ++i;
        }
        return out;
    }

    bool IsPathToken(const wxString& raw)
    {
        const wxString core = TrimSearchWord(raw);
        if (core.empty())
            return false;
        if (core[0] == '/' || core[0] == '\\' || core.StartsWith(wxT("~/")))
            return true;
        if (core.length() >= 3 && wxIsalpha(core[0]) && core[1] == ':'
                && (core[2] == '/' || core[2] == '\\'))
            return true;
        if (IsFileLocation(core))
            return true;
        const int slash = core.Find('/');
        const int back = core.Find('\\');
        const int sep = slash == wxNOT_FOUND ? back : (back == wxNOT_FOUND ? slash : wxMin(slash, back));
        return sep > 0 && sep < (int)core.length() - 1;
    }

    bool IsLocationNoise(const wxString& raw)
    {
        bool digit = false;
        bool punct = false;
        if (raw.empty())
            return false;
        for (size_t i = 0; i < raw.length(); ++i)
        {
            const wxChar c = raw[i];
            if (wxIsdigit(c))
                digit = true;
            else if (c == ':' || c == '(' || c == ')')
                punct = true;
            else if (c == ',' || c == ';' || c == '.')
                continue;
            else
                return false;
        }
        return digit && punct;
    }

    wxString StripPathTokens(const wxString& text)
    {
        wxString out;
        wxString token;
        const auto flush = [&]()
        {
            if (!token.empty() && !IsPathToken(token) && !IsLocationNoise(token))
            {
                if (!out.empty())
                    out << wxT(' ');
                out << token;
            }
            token.clear();
        };
        for (size_t i = 0; i < text.length(); ++i)
        {
            if (wxIsspace(text[i]))
                flush();
            else
                token << text[i];
        }
        flush();
        return out;
    }

    bool KeywordBoundary(const wxString& text, size_t pos)
    {
        return pos == 0 || !wxIsalnum(text[pos - 1]);
    }

    // Earliest "error:", "warning:", "note:", "fatal error:", "error C2065:" and the same kind of introduction.
    bool FindIntro(const wxString& text, size_t& keywordAt, size_t& bodyAt)
    {
        const wxString lower = text.Lower();
        const wxChar* keys[] = {
            wxT("fatal error"), wxT("error"), wxT("warning"), wxT("notice"),
            wxT("note"), wxT("remark"), wxT("info"), wxT("message"), nullptr
        };
        bool found = false;
        keywordAt = wxString::npos;
        bodyAt = wxString::npos;
        for (int k = 0; keys[k]; ++k)
        {
            const wxString key(keys[k]);
            size_t from = 0;
            while (from < lower.length())
            {
                const int rel = lower.Mid(from).Find(key);
                if (rel == wxNOT_FOUND)
                    break;
                const size_t pos = from + (size_t)rel;
                from = pos + 1;
                if (!KeywordBoundary(lower, pos))
                    continue;
                const size_t afterKey = pos + key.length();
                if (afterKey < lower.length() && wxIsalnum(lower[afterKey]))
                    continue;

                size_t cursor = afterKey;
                while (cursor < lower.length() && wxIsspace(lower[cursor]))
                    ++cursor;
                if (cursor < lower.length() && lower[cursor] != ':')
                {
                    size_t code = cursor;
                    if (lower[code] == '[')
                    {
                        while (code < lower.length() && lower[code] != ']')
                            ++code;
                        if (code < lower.length() && lower[code] == ']')
                            ++code;
                    }
                    else if (wxIsalnum(lower[code]) || lower[code] == '_' || lower[code] == '-' || lower[code] == '+')
                    {
                        while (code < lower.length()
                                && (wxIsalnum(lower[code]) || lower[code] == '_' || lower[code] == '-' || lower[code] == '+'))
                            ++code;
                    }
                    else
                        continue;
                    while (code < lower.length() && wxIsspace(lower[code]))
                        ++code;
                    if (code >= lower.length() || lower[code] != ':')
                        continue;
                    cursor = code;
                }
                if (cursor < lower.length() && lower[cursor] == ':')
                {
                    if (!found || pos < keywordAt)
                    {
                        found = true;
                        keywordAt = pos;
                        bodyAt = cursor + 1;
                    }
                    break;
                }
            }
        }
        return found;
    }

    wxString FirstSearchWords(const wxString& text, int maxWords)
    {
        wxString result;
        int count = 0;
        wxString token;
        const auto flush = [&]()
        {
            if (maxWords >= 0 && count >= maxWords)
            {
                token.clear();
                return;
            }
            const wxString word = TrimSearchWord(token);
            token.clear();
            if (word.empty())
                return;
            if (count)
                result << wxT(' ');
            result << word;
            ++count;
        };
        for (size_t i = 0; i < text.length(); ++i)
        {
            if (maxWords >= 0 && count >= maxWords)
                break;
            if (wxIsspace(text[i]))
                flush();
            else
                token << text[i];
        }
        flush();
        return result;
    }

    wxString MakeStackOverflowQuery(const wxString& message, bool limitWords)
    {
        wxString cleaned = StripPathTokens(StripAbsolutePaths(message));
        cleaned.Trim(true).Trim(false);
        if (cleaned.empty())
            return wxEmptyString;

        size_t keywordAt = 0;
        size_t bodyAt = 0;
        if (FindIntro(cleaned, keywordAt, bodyAt))
        {
            size_t content = 0;
            while (content < cleaned.length() && wxIsspace(cleaned[content]))
                ++content;
            // The line under the cursor always starts after the introduction.
            // Selected text does too when the selection itself starts with one.
            if (limitWords || keywordAt == content)
                cleaned = cleaned.Mid(bodyAt);
        }
        cleaned.Trim(true).Trim(false);
        return FirstSearchWords(cleaned, limitWords ? 5 : -1);
    }
}

BEGIN_EVENT_TABLE(CompilerMessages, wxEvtHandler)
END_EVENT_TABLE()

CompilerMessages::CompilerMessages(const wxArrayString& titles_in, const wxArrayInt& widths_in)
    : ListCtrlLogger(titles_in, widths_in, true),
      m_rightClickItem(-1),
      m_clickItem(-1),
      m_clickFromList(false)
{
    m_autoFit = Manager::Get()->GetConfigManager("compiler")->ReadBool("/autofit_during_build", false);
    //ctor
}

CompilerMessages::~CompilerMessages()
{
	//dtor
	Disconnect(idList, -1, wxEVT_COMMAND_LIST_ITEM_SELECTED,
            (wxObjectEventFunction) (wxEventFunction) (wxCommandEventFunction)
            &CompilerMessages::OnClick);
    Disconnect(idList, -1, wxEVT_COMMAND_LIST_ITEM_ACTIVATED,
            (wxObjectEventFunction) (wxEventFunction) (wxCommandEventFunction)
            &CompilerMessages::OnDoubleClick);
    Disconnect(idMenuFit, -1, wxEVT_COMMAND_MENU_SELECTED,
            (wxObjectEventFunction) (wxEventFunction) (wxCommandEventFunction)
            &CompilerMessages::OnFit);
    Disconnect(idMenuAutoFit, -1, wxEVT_COMMAND_MENU_SELECTED,
            (wxObjectEventFunction) (wxEventFunction) (wxCommandEventFunction)
            &CompilerMessages::OnAutoFit);
    Disconnect(idMenuStackOverflow, -1, wxEVT_COMMAND_MENU_SELECTED,
            (wxObjectEventFunction) (wxEventFunction) (wxCommandEventFunction)
            &CompilerMessages::OnSearchStackOverflow);
    if (control)
    {
        control->Disconnect(wxEVT_RIGHT_DOWN, wxMouseEventHandler(CompilerMessages::OnRightDown), nullptr, this);
        control->Disconnect(wxEVT_CONTEXT_MENU, wxContextMenuEventHandler(CompilerMessages::OnListContextMenu), nullptr, this);
    }
}

wxWindow* CompilerMessages::CreateControl(wxWindow* parent)
{
    ListCtrlLogger::CreateControl(parent);
    control->SetId(idList);
    Connect(idList, -1, wxEVT_COMMAND_LIST_ITEM_SELECTED,
            (wxObjectEventFunction) (wxEventFunction) (wxCommandEventFunction)
            &CompilerMessages::OnClick);
    Connect(idList, -1, wxEVT_COMMAND_LIST_ITEM_ACTIVATED,
            (wxObjectEventFunction) (wxEventFunction) (wxCommandEventFunction)
            &CompilerMessages::OnDoubleClick);
    Connect(idMenuFit, -1, wxEVT_COMMAND_MENU_SELECTED,
            (wxObjectEventFunction) (wxEventFunction) (wxCommandEventFunction)
            &CompilerMessages::OnFit);
    Connect(idMenuAutoFit, -1, wxEVT_COMMAND_MENU_SELECTED,
            (wxObjectEventFunction) (wxEventFunction) (wxCommandEventFunction)
            &CompilerMessages::OnAutoFit);
    Connect(idMenuStackOverflow, -1, wxEVT_COMMAND_MENU_SELECTED,
            (wxObjectEventFunction) (wxEventFunction) (wxCommandEventFunction)
            &CompilerMessages::OnSearchStackOverflow);
    control->Connect(wxEVT_RIGHT_DOWN, wxMouseEventHandler(CompilerMessages::OnRightDown), nullptr, this);
    control->Connect(wxEVT_CONTEXT_MENU, wxContextMenuEventHandler(CompilerMessages::OnListContextMenu), nullptr, this);
    Manager::Get()->GetAppWindow()->PushEventHandler(this);
    return control;
}

void CompilerMessages::DestroyControls()
{
    if ( !Manager::Get()->IsAppShuttingDown() )
    {
        Manager::Get()->GetAppWindow()->RemoveEventHandler(this);
    }
}

bool CompilerMessages::HasFeature(Feature::Enum feature) const
{
    if (feature == Feature::Additional)
        return true;
    else
        return ListCtrlLogger::HasFeature(feature);
}

void CompilerMessages::AppendAdditionalMenuItems(wxMenu &menu)
{
    m_stackOverflowQuery = QueryForMenu();
    m_clickFromList = false;

    const wxString copySelection = _("Copy selection to clipboard");
    size_t insertAt = menu.GetMenuItemCount();
    size_t index = 0;
    wxMenuItemList& items = menu.GetMenuItems();
    for (wxMenuItemList::iterator it = items.begin(); it != items.end(); ++it, ++index)
    {
        wxMenuItem* item = *it;
        if (item && item->GetItemLabelText() == copySelection)
        {
            insertAt = index + 1;
            break;
        }
    }
    wxMenuItem* searchItem = menu.Insert(insertAt, idMenuStackOverflow, _("Search Stackoverflow"),
                                         _("Search this build message on Stack Overflow"));
    if (searchItem)
        searchItem->Enable(!m_stackOverflowQuery.empty());

    menu.Append(idMenuFit, _("Fit text"), _("Makes the whole text visible"));
    menu.AppendCheckItem(idMenuAutoFit, _("Fit automatically"),
                         _("Automatically makes the whole text visible during compilation"));
    menu.Check(idMenuAutoFit, m_autoFit);
}

void CompilerMessages::FocusError(int nr)
{
    if (nr < 0 or nr >= control->GetItemCount())
        return;
    control->SetItemState(nr, wxLIST_STATE_SELECTED | wxLIST_STATE_FOCUSED, wxLIST_STATE_SELECTED | wxLIST_STATE_FOCUSED);
    control->EnsureVisible(nr);
}

void CompilerMessages::OnClick(cb_unused wxCommandEvent& event)
{
    // single and double-click, behave the same

    // a compiler message has been clicked
    // go to the relevant file/line
    if (control->GetSelectedItemCount() == 0 || !m_pErrors)
        return;

    // find selected item index
    int index = control->GetNextItem(-1,
                                     wxLIST_NEXT_ALL,
                                     wxLIST_STATE_SELECTED);

    // call the CompilerErrors* ptr; it 'll do all the hard work ;)
    m_pErrors->GotoError(index);
}

void CompilerMessages::OnDoubleClick(wxCommandEvent& event)
{
    // single and double-click, behave the same
    OnClick(event);
    return;
}

void CompilerMessages::OnFit(wxCommandEvent& WXUNUSED(event))
{
    FitColumns();
}

void CompilerMessages::OnAutoFit(wxCommandEvent& event)
{
    m_autoFit = event.IsChecked();
    Manager::Get()->GetConfigManager("compiler")->Write("/autofit_during_build", m_autoFit);
}

void CompilerMessages::AutoFitColumns(int cb_unused column)
{
    if (m_autoFit)
        FitColumns();
}

void CompilerMessages::FitColumns()
{
    if (!control)
        return;
    int count = control->GetColumnCount();
    for (int ii = 0; ii < count; ++ii)
        control->SetColumnWidth(ii, wxLIST_AUTOSIZE);
    int ctrlWidth = control->GetClientSize().x;
    for (int ii = 1; ii < count; ++ii)
        ctrlWidth -= control->GetColumnWidth(ii);
    int width = control->GetColumnWidth(0);
    if (width > ctrlWidth)
        control->SetColumnWidth(0, ctrlWidth);
}

int CompilerMessages::MessageColumn() const
{
    for (size_t i = 0; i < titles.GetCount(); ++i)
    {
        if (titles[i] == _("Message"))
            return (int)i;
    }
    if (!titles.IsEmpty())
        return (int)titles.GetCount() - 1;
    return 2;
}

wxString CompilerMessages::MessageText(long item) const
{
    if (!control || item < 0 || item >= control->GetItemCount())
        return wxEmptyString;
    return control->GetItemText(item, MessageColumn());
}

wxString CompilerMessages::SelectedMessageText() const
{
    if (!control)
        return wxEmptyString;

    if (wxTextCtrl* editor = control->GetEditControl())
    {
        long from = 0;
        long to = 0;
        editor->GetSelection(&from, &to);
        if (to > from)
            return editor->GetStringSelection();
    }

    for (wxWindowList::compatibility_iterator it = control->GetChildren().GetFirst(); it; it = it->GetNext())
    {
        wxTextCtrl* text = wxDynamicCast(it->GetData(), wxTextCtrl);
        if (!text)
            continue;
        long from = 0;
        long to = 0;
        text->GetSelection(&from, &to);
        if (to > from)
            return text->GetStringSelection();
    }
    return wxEmptyString;
}

long CompilerMessages::FirstWarningItem() const
{
    if (!control || control->GetItemCount() < 1)
        return -1;

    const long count = control->GetItemCount();
    long first = -1;
    for (long item = 0; item < count; ++item)
    {
        const wxString text = MessageText(item);
        if (text.empty())
            continue;
        if (first < 0)
            first = item;
        if (text.Lower().Contains(wxT("warning")))
            return item;
    }
    return first;
}

void CompilerMessages::CaptureListContext(long item)
{
    m_clickFromList = true;
    m_clickItem = item;
    m_clickSelection = SelectedMessageText();
}

wxString CompilerMessages::QueryForMenu() const
{
    if (m_clickFromList && !m_clickSelection.empty())
    {
        const wxString selected = MakeStackOverflowQuery(m_clickSelection, false);
        if (!selected.empty())
            return selected;
    }

    long item = -1;
    if (m_clickFromList && m_clickItem >= 0)
        item = m_clickItem;
    else
        item = FirstWarningItem();
    if (item < 0)
        return wxEmptyString;
    return MakeStackOverflowQuery(MessageText(item), true);
}

void CompilerMessages::OnRightDown(wxMouseEvent& event)
{
    m_rightClickItem = -1;
    if (control)
    {
        int flags = 0;
        const long hit = control->HitTest(event.GetPosition(), flags);
        if (hit >= 0)
            m_rightClickItem = hit;
    }
    event.Skip();
}

void CompilerMessages::OnListContextMenu(wxContextMenuEvent& event)
{
    long item = -1;
    const bool keyboard = !control || event.GetPosition() == wxDefaultPosition;
    if (!keyboard)
    {
        int flags = 0;
        const long hit = control->HitTest(control->ScreenToClient(event.GetPosition()), flags);
        if (hit >= 0)
            item = hit;
        else if (m_rightClickItem >= 0)
            item = m_rightClickItem;
    }
    else if (control)
    {
        const long focused = control->GetNextItem(-1, wxLIST_NEXT_ALL, wxLIST_STATE_FOCUSED);
        if (focused >= 0)
            item = focused;
    }
    m_rightClickItem = -1;
    CaptureListContext(item);
    event.Skip();
}

void CompilerMessages::OnSearchStackOverflow(wxCommandEvent& WXUNUSED(event))
{
    if (m_stackOverflowQuery.empty())
        return;
    wxLaunchDefaultBrowser(wxT("https://stackoverflow.com/search?q=") + URLEncode(m_stackOverflowQuery));
}
