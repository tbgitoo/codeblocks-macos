/*
 * This file is part of the Code::Blocks IDE and licensed under the GNU General Public License, version 3
 * http://www.gnu.org/licenses/gpl-3.0.html
 */

#ifndef ASTYLEPLUGIN_H
#define ASTYLEPLUGIN_H

// For compilers that support precompilation, includes <wx/wx.h>
#include <wx/wxprec.h>

#ifndef WX_PRECOMP
    #include <wx/wx.h>
#endif

#include "cbplugin.h" // the base class we 're inheriting
#include "settings.h" // needed to use the Code::Blocks SDK

class CodeBlocksEvent;

class AStylePlugin : public cbToolPlugin
{
  public:
    AStylePlugin();
    ~AStylePlugin();
    int GetConfigurationGroup() const { return cgEditor; }
    cbConfigurationPanel* GetConfigurationPanel(wxWindow* parent);
    void BuildModuleMenu( const ModuleType type, wxMenu* menu, const FileTreeData* data = 0 );
    int Execute();

    void OnAttach();
    void OnRelease(bool appShutDown);

    void OnFormatActiveFile( wxCommandEvent& event );
    void OnReindentActiveFile( wxCommandEvent& event );
    void OnReindentUpdateUI( wxUpdateUIEvent& event );
    void OnFormatProject( wxCommandEvent& event );
    void OnMenubarCreateEnd( CodeBlocksEvent& event );
    void FormatFile( const wxString &filename );
    // applyConfiguredOptions selects the user's stored AStyle options.
    // Re-indent passes false and leaves the formatter at AStyle's defaults.
    bool FormatEditor( cbEditor *ed, bool applyConfiguredOptions = true );

    DECLARE_EVENT_TABLE()

    void InsertReindentEditMenuItem();
    void RemoveReindentEditMenuItem();
};

#endif // ASTYLEPLUGIN_H
