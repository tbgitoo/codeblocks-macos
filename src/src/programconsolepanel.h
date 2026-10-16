#ifndef PROGRAMCONSOLEPANEL_H
#define PROGRAMCONSOLEPANEL_H

#include <wx/panel.h>
#include <wx/string.h>
#include <wx/timer.h>
#include <cstdint>

class cbStyledTextCtrl;

/** InfoPane Program Console page (macOS). Display + key → PTY bytes. */
class ProgramConsolePanel : public wxPanel
{
public:
    explicit ProgramConsolePanel(wxWindow* parent);
    ~ProgramConsolePanel() override;

    void ClearScrollback();
    void AppendBytes(std::uint64_t generation, const wxString& text);
    void SetActiveGeneration(std::uint64_t generation);

private:
    void OnKeyDown(wxKeyEvent& event);
    void OnChar(wxKeyEvent& event);
    void OnSize(wxSizeEvent& event);
    void OnIdle(wxIdleEvent& event);
    void OnInputFocusTimer(wxTimerEvent& event);
    void FlushPending();
    void ApplyResize();
    void ArmInputFocusTimer();
    void CancelInputFocusTimer();
    bool SendKeyToPty(const wxKeyEvent& event);

    cbStyledTextCtrl* m_stc = nullptr;
    std::uint64_t m_activeGeneration = 0;
    wxString m_pending;
    bool m_resizePending = false;
    wxTimer m_inputFocusTimer;

    DECLARE_EVENT_TABLE()
};

#endif // PROGRAMCONSOLEPANEL_H
