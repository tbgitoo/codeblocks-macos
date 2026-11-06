#ifndef OGRETOOLS_H
#define OGRETOOLS_H

#include <cbplugin.h>

class wxMenu;
class wxMenuItem;
class wxMenuEvent;

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

    void OnOgreMenuOpen(wxMenuEvent& event);

    void OnOpenToolsFolder(wxCommandEvent& event);
    void OnOpenSampleBrowser(wxCommandEvent& event);
    void OnInstallOgre(wxCommandEvent& event);

    wxMenu* m_OgreMenu = nullptr;

    wxMenuItem* m_ImportAsset = nullptr;
    wxMenuItem* m_AssimpConverter = nullptr;
    wxMenuItem* m_MeshUpgrader = nullptr;
    wxMenuItem* m_XmlConverter = nullptr;
    wxMenuItem* m_VrmlConverter = nullptr;
    wxMenuItem* m_OpenToolsFolder = nullptr;
    wxMenuItem* m_SampleBrowser = nullptr;
    wxMenuItem* m_InstallOgre = nullptr;

    DECLARE_EVENT_TABLE();
};

#endif // OGRETOOLS_H
