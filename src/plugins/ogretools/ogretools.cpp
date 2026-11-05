#include <sdk.h>

#include "ogretools.h"

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

    const OgreToolsAvailability tools = LocateOgreTools();

    wxMenu* ogreMenu = new wxMenu();

    wxMenuItem* importAsset =
        ogreMenu->Append(idImportAsset, _("Import 3D asset..."));

    ogreMenu->AppendSeparator();

    wxMenuItem* assimpConverter =
        ogreMenu->Append(idAssimpConverter, _("OgreAssimpConverter..."));

    wxMenuItem* meshUpgrader =
        ogreMenu->Append(idMeshUpgrader, _("OgreMeshUpgrader..."));

    wxMenuItem* xmlConverter =
        ogreMenu->Append(idXmlConverter, _("OgreXMLConverter..."));

    wxMenuItem* vrmlConverter =
        ogreMenu->Append(idVrmlConverter, _("VRMLConverter..."));

    ogreMenu->AppendSeparator();

    wxMenuItem* openToolsFolder =
        ogreMenu->Append(idOpenToolsFolder, _("Open SDK tools folder"));

    wxMenuItem* sampleBrowser =
        ogreMenu->Append(idSampleBrowser, _("Open SampleBrowser"));

    ogreMenu->AppendSeparator();

    ogreMenu->Append(idInstallOgre, _("Install / Repair OGRE tools..."));

    importAsset->Enable(!tools.assimpConverter.IsEmpty());
    assimpConverter->Enable(!tools.assimpConverter.IsEmpty());
    meshUpgrader->Enable(!tools.meshUpgrader.IsEmpty());
    xmlConverter->Enable(!tools.xmlConverter.IsEmpty());
    vrmlConverter->Enable(!tools.vrmlConverter.IsEmpty());
    openToolsFolder->Enable(!tools.toolsDirectory.IsEmpty());
    sampleBrowser->Enable(!tools.sampleBrowser.IsEmpty());

    toolsMenu->AppendSubMenu(ogreMenu, _("OGRE"));
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
