#include <sdk.h>

#include <cbplugin.h>

#include <logmanager.h>
#include <manager.h>

#include "ogretools.h"
#include "ogretoolslocator.h"

#include <wx/menu.h>
#include <wx/utils.h>




namespace
{
    PluginRegistrant<OgreTools> reg(_T("OgreTools"));

    int idImportAsset       = wxNewId();
    int idAssimpConverter   = wxNewId();
    int idMeshUpgrader      = wxNewId();
    int idXmlConverter      = wxNewId();
    int idVrmlConverter     = wxNewId();
    int idOpenToolsFolder   = wxNewId();
    int idSampleBrowser     = wxNewId();
    int idInstallOgre       = wxNewId();





}

BEGIN_EVENT_TABLE(OgreTools, cbPlugin)
    EVT_MENU_OPEN(OgreTools::OnOgreMenuOpen)
    EVT_MENU(idOpenToolsFolder, OgreTools::OnOpenToolsFolder)
    EVT_MENU(idSampleBrowser, OgreTools::OnOpenSampleBrowser)
END_EVENT_TABLE()

OgreTools::OgreTools()
{
}

OgreTools::~OgreTools()
{
}

void OgreTools::OnAttach()
{
}

void OgreTools::OnRelease(bool appShutDown)
{
    (void)appShutDown;
}



void OgreTools::BuildMenu(wxMenuBar* menuBar)
{
    if (!menuBar)
        return;

    const int pos = menuBar->FindMenu(_("&Tools"));
    if (pos == wxNOT_FOUND)
        return;

    wxMenu* toolsMenu = menuBar->GetMenu(pos);
    if (!toolsMenu)
        return;

    m_OgreMenu = new wxMenu();

    m_ImportAsset =
        m_OgreMenu->Append(
            idImportAsset,
            _("Import 3D asset..."));

    m_OgreMenu->AppendSeparator();

    m_AssimpConverter =
        m_OgreMenu->Append(
            idAssimpConverter,
            _("OgreAssimpConverter..."));

    m_MeshUpgrader =
        m_OgreMenu->Append(
            idMeshUpgrader,
            _("OgreMeshUpgrader..."));

    m_XmlConverter =
        m_OgreMenu->Append(
            idXmlConverter,
            _("OgreXMLConverter..."));

    m_VrmlConverter =
        m_OgreMenu->Append(
            idVrmlConverter,
            _("VRMLConverter..."));

    m_OgreMenu->AppendSeparator();

    m_OpenToolsFolder =
        m_OgreMenu->Append(
            idOpenToolsFolder,
            _("Open SDK tools folder"));

    m_SampleBrowser =
        m_OgreMenu->Append(
            idSampleBrowser,
            _("Open SampleBrowser"));

    m_OgreMenu->AppendSeparator();

    m_InstallOgre =
        m_OgreMenu->Append(
            idInstallOgre,
            _("Install / Repair OGRE tools..."));

    RefreshCapabilities();

    toolsMenu->AppendSubMenu(
        m_OgreMenu,
        _("OGRE"));
}

void OgreTools::OnOpenToolsFolder(wxCommandEvent& event)
{
    (void)event;

    const OgreToolsAvailability tools = LocateOgreTools();

    if (tools.toolsDirectory.IsEmpty())
        return;

    wxLaunchDefaultApplication(tools.toolsDirectory);
}

void OgreTools::OnOpenSampleBrowser(wxCommandEvent& event)
{
    (void)event;

    const OgreToolsAvailability tools = LocateOgreTools();

    if (tools.sampleBrowser.IsEmpty())
        return;

    wxLaunchDefaultApplication(tools.sampleBrowser);
}

void OgreTools::RefreshCapabilities()
{
    if (!m_OgreMenu)
        return;

    const OgreToolsAvailability tools =
        LocateOgreTools();

    if (m_ImportAsset)
        m_ImportAsset->Enable(
            !tools.assimpConverter.IsEmpty());

    if (m_AssimpConverter)
        m_AssimpConverter->Enable(
            !tools.assimpConverter.IsEmpty());

    if (m_MeshUpgrader)
        m_MeshUpgrader->Enable(
            !tools.meshUpgrader.IsEmpty());

    if (m_XmlConverter)
        m_XmlConverter->Enable(
            !tools.xmlConverter.IsEmpty());

    if (m_VrmlConverter)
        m_VrmlConverter->Enable(
            !tools.vrmlConverter.IsEmpty());

    if (m_OpenToolsFolder)
        m_OpenToolsFolder->Enable(
            !tools.toolsDirectory.IsEmpty());

    if (m_SampleBrowser)
        m_SampleBrowser->Enable(
            !tools.sampleBrowser.IsEmpty());
    
    
}

void OgreTools::OnOgreMenuOpen(wxMenuEvent& event)
{
    if (event.GetMenu() == m_OgreMenu)
        RefreshCapabilities();

    event.Skip();
}


