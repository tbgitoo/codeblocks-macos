#include <sdk.h>

#include <cbplugin.h>
#include <pluginmanager.h>

#include <loggers.h>
#include <logmanager.h>
#include <manager.h>
#include <sdk_events.h>

#include "ogretools.h"
#include "ogreimportdlg.h"
#include "ogretoolslocator.h"

#include <wx/menu.h>
#include <wx/utils.h>




namespace
{
    PluginRegistrant<OgreTools> reg(_T("OgreTools"));

    int idImportAsset       = wxNewId();
    int idOpenToolsFolder   = wxNewId();
    int idSampleBrowser     = wxNewId();
    int idInstallOgre       = wxNewId();





}

BEGIN_EVENT_TABLE(OgreTools, cbPlugin)
    EVT_MENU_OPEN(OgreTools::OnOgreMenuOpen)
    EVT_MENU(idImportAsset, OgreTools::OnImportAsset)
    EVT_MENU(idOpenToolsFolder, OgreTools::OnOpenToolsFolder)
    EVT_MENU(idSampleBrowser, OgreTools::OnOpenSampleBrowser)
    EVT_MENU(idInstallOgre, OgreTools::OnInstallOgre)
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
    if (!appShutDown && m_Log && Manager::Get())
    {
        CodeBlocksLogEvent evt(cbEVT_REMOVE_LOG_WINDOW, m_Log);
        Manager::Get()->ProcessEvent(evt);
    }

    m_Log = nullptr;
    m_LogPageIndex = -1;
}

void OgreTools::EnsureLog()
{
    if (m_Log || !Manager::Get())
        return;

    LogManager* manager = Manager::Get()->GetLogManager();

    if (!manager)
        return;

    m_Log = new TextCtrlLogger();

    const size_t index = manager->SetLog(m_Log);

    if (index == static_cast<size_t>(LogManager::invalid_log))
    {
        m_Log = nullptr;
        return;
    }

    m_LogPageIndex = static_cast<int>(index);
    manager->Slot(m_LogPageIndex).title = _("OGRE Tools");

    CodeBlocksLogEvent add(
        cbEVT_ADD_LOG_WINDOW,
        m_Log,
        manager->Slot(m_LogPageIndex).title);

    Manager::Get()->ProcessEvent(add);
}

void OgreTools::WriteLog(const wxString& text)
{
    EnsureLog();

    if (!m_Log || !Manager::Get())
        return;

    CodeBlocksLogEvent show(cbEVT_SHOW_LOG_MANAGER);
    Manager::Get()->ProcessEvent(show);

    CodeBlocksLogEvent switchTo(cbEVT_SWITCH_TO_LOG_WINDOW, m_Log);
    Manager::Get()->ProcessEvent(switchTo);

    Manager::Get()->GetLogManager()->Log(text, m_LogPageIndex);
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

void OgreTools::OnImportAsset(wxCommandEvent& event)
{
    (void)event;

    wxWindow* parent = Manager::Get()->GetAppWindow();
    OgreImportRequest request;

    if (!PromptOgreImport(parent, request))
        return;

    OgreImportResult result =
        RunOgreAssimpImport(parent, request);

    if (result.ok)
    {
        result.summary += CopyObjTextures(request, result);
        result.summary += NoteMaterialTextureFiles(request, result);
        result.summary += RegisterOgreImport(parent, request, result);
    }

    wxString log = result.commandSummary;

    if (!result.output.IsEmpty())
        log += wxT("\n\n") + result.output;

    if (!result.errorOutput.IsEmpty())
        log += wxT("\n") + result.errorOutput;

    log += wxT("\n") + result.summary;
    WriteLog(log);

    const bool clean =
        result.ok && result.resourceWarnings.IsEmpty();

    cbMessageBox(
        result.summary,
        _("Import 3D asset"),
        clean ? wxICON_INFORMATION : wxICON_WARNING,
        parent);
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

void OgreTools::OnInstallOgre(wxCommandEvent& event)
{
    (void)event;

    const PluginsArray wizards =
        Manager::Get()
            ->GetPluginManager()
            ->GetOffersFor(ptWizard);

    for (unsigned int p = 0; p < wizards.GetCount(); ++p)
    {
        cbWizardPlugin* wizard =
            static_cast<cbWizardPlugin*>(wizards[p]);

        if (!wizard)
            continue;

        for (int i = 0; i < wizard->GetCount(); ++i)
        {
            if (wizard->GetOutputType(i) != totCustom)
                continue;

            if (!wizard->GetScriptFilename(i).Matches(
                    wxT("ogre/wizard.script")))
            {
                continue;
            }

            wizard->Launch(i);
            return;
        }
    }

    cbMessageBox(
        _("The OGRE SDK setup wizard could not be found."),
        _("OGRE Tools"),
        wxICON_ERROR);
}


