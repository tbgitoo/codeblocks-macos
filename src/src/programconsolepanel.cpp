#include "sdk_precomp.h"

#ifndef CB_PRECOMP
    #include <wx/sizer.h>
    #include "cbeditor.h"
    #include "configmanager.h"
    #include "editorcolourset.h"
    #include "manager.h"
    #include "logmanager.h"
#endif

#include "programconsolepanel.h"
#include "programconsole.h"
#include "programconsole_internal.h"
#include "cbstyledtextctrl.h"
#include "infopane.h"
#include "sdk_events.h"

#include <wx/settings.h>
#include <algorithm>

namespace
{
ProgramConsolePanel* g_panel = nullptr;

void HookClear()
{
    if (g_panel)
        g_panel->ClearScrollback();
}

void HookAppend(std::uint64_t gen, const wxString& text)
{
    if (g_panel)
        g_panel->AppendBytes(gen, text);
}

void HookShow()
{
    if (!g_panel)
        return;
    wxWindow* p = g_panel->GetParent();
    InfoPane* pane = dynamic_cast<InfoPane*>(p);
    if (pane) {
        const int idx = pane->GetPageIndex(g_panel);
        if (idx != wxNOT_FOUND)
            pane->SetSelection(idx);
    }
    CodeBlocksLogEvent evtShow(cbEVT_SHOW_LOG_MANAGER);
    Manager::Get()->ProcessEvent(evtShow);
    g_panel->SetFocus();
}

void HookSetGen(std::uint64_t gen)
{
    if (g_panel)
        g_panel->SetActiveGeneration(gen);
}
} // namespace

BEGIN_EVENT_TABLE(ProgramConsolePanel, wxPanel)
    EVT_SIZE(ProgramConsolePanel::OnSize)
    EVT_IDLE(ProgramConsolePanel::OnIdle)
END_EVENT_TABLE()

ProgramConsolePanel::ProgramConsolePanel(wxWindow* parent)
    : wxPanel(parent, wxID_ANY)
{
    m_stc = new cbStyledTextCtrl(this, wxID_ANY);
    m_stc->SetReadOnly(false);
    m_stc->SetCaretLineVisible(false);
    m_stc->SetMarginWidth(0, 0);
    m_stc->SetMarginWidth(1, 0);
    m_stc->SetMarginWidth(2, 0);
    m_stc->SetUndoCollection(false);
    m_stc->SetLexer(wxSCI_LEX_NULL);
    m_stc->SetWrapMode(wxSCI_WRAP_NONE);
    m_stc->SetUseHorizontalScrollBar(true);
    m_stc->SetEOLMode(wxSCI_EOL_LF);
    wxFont font = wxSystemSettings::GetFont(wxSYS_ANSI_FIXED_FONT);
    m_stc->StyleSetFont(wxSCI_STYLE_DEFAULT, font);
    m_stc->StyleClearAll();

    auto* sizer = new wxBoxSizer(wxVERTICAL);
    sizer->Add(m_stc, 1, wxEXPAND);
    SetSizer(sizer);

    m_stc->Bind(wxEVT_CHAR, &ProgramConsolePanel::OnChar, this);

    g_panel = this;
#if defined(__WXMAC__)
    ProgramConsole::BindPanelHooks(&HookClear, &HookAppend, &HookShow, &HookSetGen);
#endif
}

ProgramConsolePanel::~ProgramConsolePanel()
{
    if (g_panel == this)
        g_panel = nullptr;
#if defined(__WXMAC__)
    ProgramConsole::BindPanelHooks(nullptr, nullptr, nullptr, nullptr);
#endif
}

void ProgramConsolePanel::ClearScrollback()
{
    m_pending.clear();
    m_stc->SetReadOnly(false);
    m_stc->ClearAll();
}

void ProgramConsolePanel::SetActiveGeneration(std::uint64_t generation)
{
    m_activeGeneration = generation;
}

void ProgramConsolePanel::AppendBytes(std::uint64_t generation, const wxString& text)
{
    if (m_activeGeneration != 0 && generation != m_activeGeneration)
        return;
    m_pending += text;
    wxWakeUpIdle();
}

void ProgramConsolePanel::FlushPending()
{
    if (m_pending.empty())
        return;
    wxString chunk;
    chunk.swap(m_pending);

    /* Basic CR handling: treat lone CR as return-to-line-start for progress. */
    m_stc->SetReadOnly(false);
    for (size_t i = 0; i < chunk.size(); ++i) {
        const wxChar ch = chunk[i];
        if (ch == '\r') {
            if (i + 1 < chunk.size() && chunk[i + 1] == '\n')
                continue; /* CRLF → LF handled next */
            const int line = m_stc->GetCurrentLine();
            const int start = m_stc->PositionFromLine(line);
            m_stc->GotoPos(start);
            continue;
        }
        if (ch == '\b') {
            const int pos = m_stc->GetCurrentPos();
            if (pos > 0) {
                m_stc->SetTargetRange(pos - 1, pos);
                m_stc->ReplaceTarget(wxEmptyString);
                m_stc->GotoPos(pos - 1);
            }
            continue;
        }
        m_stc->AppendText(wxString(ch));
        m_stc->GotoPos(m_stc->GetLength());
    }
    m_stc->EnsureCaretVisible();
}

void ProgramConsolePanel::OnIdle(wxIdleEvent& event)
{
    FlushPending();
    if (m_resizePending) {
        m_resizePending = false;
        ApplyResize();
    }
    event.Skip();
}

void ProgramConsolePanel::OnSize(wxSizeEvent& event)
{
    m_resizePending = true;
    event.Skip();
}

void ProgramConsolePanel::ApplyResize()
{
#if defined(__WXMAC__)
    if (!m_stc)
        return;
    const wxSize client = m_stc->GetClientSize();
    int w = m_stc->TextWidth(wxSCI_STYLE_DEFAULT, "M");
    int h = m_stc->TextHeight(0);
    if (w <= 0)
        w = 8;
    if (h <= 0)
        h = 16;
    unsigned cols = static_cast<unsigned>(std::max(1, client.GetWidth() / w));
    unsigned rows = static_cast<unsigned>(std::max(1, client.GetHeight() / h));
    ProgramConsole::ResizeSession(rows, cols);
#endif
}

void ProgramConsolePanel::OnChar(wxKeyEvent& event)
{
#if defined(__WXMAC__)
    const int key = event.GetKeyCode();
    if (event.GetModifiers() == wxMOD_CONTROL || event.GetModifiers() == wxMOD_RAW_CONTROL) {
        if (key == 'C' || key == 'c') {
            const unsigned char vintr = ProgramConsole::GetVIntrByte();
            ProgramConsole::SendPanelInput(&vintr, 1);
            return;
        }
        if (key == 'D' || key == 'd') {
            const unsigned char veof = ProgramConsole::GetVEofByte();
            ProgramConsole::SendPanelInput(&veof, 1);
            return;
        }
    }
    if (key == WXK_RETURN || key == WXK_NUMPAD_ENTER) {
        const char nl = '\n';
        ProgramConsole::SendPanelInput(&nl, 1);
        return;
    }
    if (key == WXK_BACK) {
        const char bs = '\b';
        ProgramConsole::SendPanelInput(&bs, 1);
        return;
    }
    const wxChar uc = event.GetUnicodeKey();
    if (uc >= 32) {
        const wxCharBuffer utf8 = wxString(uc).ToUTF8();
        if (utf8.length())
            ProgramConsole::SendPanelInput(utf8.data(), utf8.length());
        return;
    }
#else
    event.Skip();
#endif
}

namespace ProgramConsole
{
void RegisterPanel(wxWindow* infoPaneParent)
{
#if defined(__WXMAC__)
    if (!platform::macosx || !infoPaneParent)
        return;
    InfoPane* pane = dynamic_cast<InfoPane*>(infoPaneParent);
    if (!pane || g_panel)
        return;
    auto* panel = new ProgramConsolePanel(pane);
    pane->AddNonLogger(panel, _("Program Console"));
#else
    wxUnusedVar(infoPaneParent);
#endif
}
} // namespace ProgramConsole
