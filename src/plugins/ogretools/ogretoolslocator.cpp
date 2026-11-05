#include <sdk.h>

#include "ogretoolslocator.h"

#include <wx/filename.h>
#include <wx/utils.h>

namespace
{
    wxString ExistingExecutable(
        const wxString& directory,
        const wxString& name)
    {
        const wxString path =
            directory + wxFILE_SEP_PATH + name;

        if (!wxFileName::FileExists(path))
            return wxString();

        if (!wxFileName::IsFileExecutable(path))
            return wxString();

        return path;
    }

    wxString ExistingDirectory(
        const wxString& directory,
        const wxString& name)
    {
        const wxString path =
            directory + wxFILE_SEP_PATH + name;

        if (!wxFileName::DirExists(path))
            return wxString();

        return path;
    }
}

OgreToolsAvailability LocateOgreTools()
{
    OgreToolsAvailability result;

#ifdef __WXOSX__
    wxString home = wxGetHomeDir();

    if (home.IsEmpty())
        return result;

    result.managedPrefix =
        home
        + wxFILE_SEP_PATH + wxT("Library")
        + wxFILE_SEP_PATH + wxT("CodeBlocks")
        + wxFILE_SEP_PATH + wxT("ogre-14.6.3");

    result.toolsDirectory =
        result.managedPrefix
        + wxFILE_SEP_PATH + wxT("bin")
        + wxFILE_SEP_PATH + wxT("macosx");

    if (!wxFileName::DirExists(result.toolsDirectory))
    {
        result.toolsDirectory.Clear();
        return result;
    }

    result.assimpConverter =
        ExistingExecutable(
            result.toolsDirectory,
            wxT("OgreAssimpConverter"));

    result.meshUpgrader =
        ExistingExecutable(
            result.toolsDirectory,
            wxT("OgreMeshUpgrader"));

    result.xmlConverter =
        ExistingExecutable(
            result.toolsDirectory,
            wxT("OgreXMLConverter"));

    result.vrmlConverter =
        ExistingExecutable(
            result.toolsDirectory,
            wxT("VRMLConverter"));

    result.sampleBrowser =
        ExistingDirectory(
            result.toolsDirectory,
            wxT("SampleBrowser.app"));
#endif

    return result;
}
