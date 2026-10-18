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
    /* Closing the tab Toggle()s it hidden; ShowNonLogger re-inserts it.
     * Focus policy (on_launch / silence) is applied separately. */
    wxWindow* p = g_panel->GetParent();
    if (InfoPane* pane = dynamic_cast<InfoPane*>(p))
        pane->ShowNonLogger(g_panel);
    CodeBlocksLogEvent evtShow(cbEVT_SHOW_LOG_MANAGER);
    Manager::Get()->ProcessEvent(evtShow);
}

void HookSetGen(std::uint64_t gen)
{
    if (g_panel)
        g_panel->SetActiveGeneration(gen);
}

void HookArmInputFocus()
{
    if (g_panel)
        g_panel->ArmInputFocusTimer();
}

void HookCancelInputFocus()
{
    if (g_panel)
        g_panel->CancelInputFocusTimer();
}

void HookFocusNow()
{
    if (g_panel)
        g_panel->FocusConsoleNow();
}
} // namespace

BEGIN_EVENT_TABLE(ProgramConsolePanel, wxPanel)
    EVT_SIZE(ProgramConsolePanel::OnSize)
    EVT_IDLE(ProgramConsolePanel::OnIdle)
    EVT_TIMER(wxID_ANY, ProgramConsolePanel::OnInputFocusTimer)
END_EVENT_TABLE()

ProgramConsolePanel::ProgramConsolePanel(wxWindow* parent)
    : wxPanel(parent, wxID_ANY),
      m_inputFocusTimer(this)
{
    m_stc = new cbStyledTextCtrl(this, wxID_ANY);
    /*
     * Display-only: all keystrokes are forwarded to the PTY. If the STC is
     * writable, Scintilla consumes Return/typing on KEY_DOWN before CHAR, so
     * input appears locally and never reaches cin.
     */
    m_stc->SetReadOnly(true);
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

    m_stc->Bind(wxEVT_KEY_DOWN, &ProgramConsolePanel::OnKeyDown, this);
    m_stc->Bind(wxEVT_CHAR, &ProgramConsolePanel::OnChar, this);

    g_panel = this;
#if defined(__WXMAC__)
    ProgramConsole::BindPanelHooks(&HookClear, &HookAppend, &HookShow, &HookSetGen,
                                   &HookArmInputFocus, &HookCancelInputFocus, &HookFocusNow);
#endif
}

ProgramConsolePanel::~ProgramConsolePanel()
{
    CancelInputFocusTimer();
    if (g_panel == this)
        g_panel = nullptr;
#if defined(__WXMAC__)
    ProgramConsole::BindPanelHooks(nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr);
#endif
}

void ProgramConsolePanel::ClearScrollback()
{
    CancelInputFocusTimer();
    m_pending.clear();
    m_stc->SetReadOnly(false);
    m_stc->ClearAll();
    m_stc->SetReadOnly(true);
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

    m_stc->SetReadOnly(false);
    for (size_t i = 0; i < chunk.size(); ++i) {
        const wxChar ch = chunk[i];
        if (ch == '\r') {
            if (i + 1 < chunk.size() && chunk[i + 1] == '\n')
                continue;
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
    m_stc->SetReadOnly(true);
    m_stc->EnsureCaretVisible();
    /* After output quiets while the child is still alive, assume cin/input. */
    ArmInputFocusTimer();
}

void ProgramConsolePanel::FocusConsoleNow()
{
#if defined(__WXMAC__)
    wxWindow* p = GetParent();
    if (InfoPane* pane = dynamic_cast<InfoPane*>(p))
        pane->ShowNonLogger(this);
    CodeBlocksLogEvent evtShow(cbEVT_SHOW_LOG_MANAGER);
    Manager::Get()->ProcessEvent(evtShow);
    if (m_stc)
        m_stc->SetFocus();
#endif
}

void ProgramConsolePanel::ArmInputFocusTimer()
{
#if defined(__WXMAC__)
    /* Silence / step-silence policies only (see AllowInputFocusSteal). */
    if (!ProgramConsole::AllowInputFocusSteal())
        return;
    /* One-shot: restart on each flush so continuous cout does not steal focus. */
    m_inputFocusTimer.Start(250, wxTIMER_ONE_SHOT);
#else
    /* no-op off macOS */
#endif
}

void ProgramConsolePanel::CancelInputFocusTimer()
{
    if (m_inputFocusTimer.IsRunning())
        m_inputFocusTimer.Stop();
}

void ProgramConsolePanel::OnInputFocusTimer(wxTimerEvent& /*event*/)
{
#if defined(__WXMAC__)
    if (!ProgramConsole::AllowInputFocusSteal() || !m_stc)
        return;
    wxWindow* focused = wxWindow::FindFocus();
    if (focused == m_stc)
        return;
    if (InfoPane* pane = dynamic_cast<InfoPane*>(GetParent()))
        pane->ShowNonLogger(this);
    m_stc->SetFocus();
#endif
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

bool ProgramConsolePanel::SendKeyToPty(const wxKeyEvent& event)
{
#if defined(__WXMAC__)
    const int key = event.GetKeyCode();
    const int mods = event.GetModifiers();

    if (mods == wxMOD_CONTROL || mods == wxMOD_RAW_CONTROL) {
        if (key == 'C' || key == 'c') {
            const unsigned char vintr = ProgramConsole::GetVIntrByte();
            return ProgramConsole::SendPanelInput(&vintr, 1);
        }
        if (key == 'D' || key == 'd') {
            const unsigned char veof = ProgramConsole::GetVEofByte();
            return ProgramConsole::SendPanelInput(&veof, 1);
        }
    }

    /* Terminals send CR for Enter; ICRNL maps it to NL for the reader. */
    if (key == WXK_RETURN || key == WXK_NUMPAD_ENTER) {
        const char cr = '\r';
        return ProgramConsole::SendPanelInput(&cr, 1);
    }
    if (key == WXK_BACK || key == WXK_DELETE) {
        const char bs = '\b';
        return ProgramConsole::SendPanelInput(&bs, 1);
    }
    if (key == WXK_TAB) {
        const char tab = '\t';
        return ProgramConsole::SendPanelInput(&tab, 1);
    }

    const wxChar uc = event.GetUnicodeKey();
    if (uc >= 32) {
        const wxCharBuffer utf8 = wxString(uc).ToUTF8();
        if (utf8.length())
            return ProgramConsole::SendPanelInput(utf8.data(), utf8.length());
    }
#else
    wxUnusedVar(event);
#endif
    return false;
}

void ProgramConsolePanel::OnKeyDown(wxKeyEvent& event)
{
#if defined(__WXMAC__)
    const int key = event.GetKeyCode();
    /* Intercept keys Scintilla would otherwise handle before CHAR. */
    if (key == WXK_RETURN || key == WXK_NUMPAD_ENTER || key == WXK_BACK
        || key == WXK_DELETE || key == WXK_TAB
        || event.GetModifiers() == wxMOD_CONTROL
        || event.GetModifiers() == wxMOD_RAW_CONTROL) {
        if (SendKeyToPty(event))
            return; /* do not Skip — prevent STC editing */
    }
#endif
    event.Skip();
}

void ProgramConsolePanel::OnChar(wxKeyEvent& event)
{
#if defined(__WXMAC__)
    /* Printable characters: CHAR is the portable place for Unicode text. */
    const int key = event.GetKeyCode();
    if (key == WXK_RETURN || key == WXK_NUMPAD_ENTER || key == WXK_BACK
        || key == WXK_DELETE || key == WXK_TAB)
        return; /* already handled in KEY_DOWN */
    if (SendKeyToPty(event))
        return;
#endif
    event.Skip();
}

#if defined(__WXMAC__)
namespace ProgramConsole
{
void RegisterPanel(wxWindow* infoPaneParent)
{
    if (!platform::macosx || !infoPaneParent)
        return;
    InfoPane* pane = dynamic_cast<InfoPane*>(infoPaneParent);
    if (!pane || g_panel)
        return;
    auto* panel = new ProgramConsolePanel(pane);
    pane->AddNonLogger(panel, _("Program Console"));
}
} // namespace ProgramConsole
#endif
