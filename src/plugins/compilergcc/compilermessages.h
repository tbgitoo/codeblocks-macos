/*
 * This file is part of the Code::Blocks IDE and licensed under the GNU General Public License, version 3
 * http://www.gnu.org/licenses/gpl-3.0.html
 */

#ifndef COMPILERMESSAGES_H
#define COMPILERMESSAGES_H

#include "loggers.h"

class CompilerErrors;
class wxArrayString;
class wxCommandEvent;
class wxContextMenuEvent;
class wxMouseEvent;

class CompilerMessages : public wxEvtHandler, public ListCtrlLogger
{
    public:
        CompilerMessages(const wxArrayString& titles, const wxArrayInt& widths);
        virtual ~CompilerMessages();
        virtual void SetCompilerErrors(CompilerErrors* errors){ m_pErrors = errors; }
        virtual void FocusError(int nr);

        void AutoFitColumns(int column) override;

        wxWindow* CreateControl(wxWindow* parent) override;
        void DestroyControls();

        bool HasFeature(Feature::Enum feature) const override;
        void AppendAdditionalMenuItems(wxMenu &menu) override;
    private:
        void OnClick(wxCommandEvent& event);
        void OnDoubleClick(wxCommandEvent& event);
        void OnFit(wxCommandEvent& event);
        void OnAutoFit(wxCommandEvent& event);
        void OnRightDown(wxMouseEvent& event);
        void OnListContextMenu(wxContextMenuEvent& event);
        void OnSearchStackOverflow(wxCommandEvent& event);

        void FitColumns();
        int MessageColumn() const;
        int FileColumn() const;
        wxString MessageText(long item) const;
        wxString FileText(long item) const;
        wxString SelectedMessageText() const;
        wxString LanguageForFile(const wxString& filename) const;
        long FirstWarningItem() const;
        void CaptureListContext(long item);
        wxString QueryForMenu() const;

        CompilerErrors* m_pErrors;
        bool m_autoFit;
        long m_rightClickItem;
        long m_clickItem;
        bool m_clickFromList;
        wxString m_clickSelection;
        wxString m_stackOverflowQuery;

        DECLARE_EVENT_TABLE()
};

#endif // COMPILERMESSAGES_H

