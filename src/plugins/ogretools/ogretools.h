#ifndef OGRETOOLS_H
#define OGRETOOLS_H

#include <cbplugin.h>

class wxMenu;
class wxMenuItem;
class wxMenuEvent;
class TextCtrlLogger;

class OgreTools : public cbPlugin
{
public:
    OgreTools();
    ~OgreTools() override;

    void OnAttach() override;
    void OnRelease(bool appShutDown) override;
    void BuildMenu(wxMenuBar* menuBar) override;

private:
    void RefreshCapabilities();
    void EnsureLog();
    void WriteLog(const wxString& text);

    void OnOgreMenuOpen(wxMenuEvent& event);

    void OnImportAsset(wxCommandEvent& event);
    void OnOpenToolsFolder(wxCommandEvent& event);
    void OnOpenSampleBrowser(wxCommandEvent& event);
    void OnInstallOgre(wxCommandEvent& event);

    wxMenu* m_OgreMenu = nullptr;

    wxMenuItem* m_ImportAsset = nullptr;
    wxMenuItem* m_OpenToolsFolder = nullptr;
    wxMenuItem* m_SampleBrowser = nullptr;
    wxMenuItem* m_InstallOgre = nullptr;

    TextCtrlLogger* m_Log = nullptr;
    int m_LogPageIndex = -1;

    DECLARE_EVENT_TABLE();
};

#endif // OGRETOOLS_H
