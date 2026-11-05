#include <sdk.h>

#include "ogretoolslocator.h"

#include <wx/filename.h>
#include <wx/utils.h>
#include <wx/filefn.h>

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

    wxString FindExecutableOnPath(
        const wxString& name)
    {
        wxString path;

        if (!wxGetEnv(wxT("PATH"), &path) ||
            path.IsEmpty())
        {
            return wxString();
        }

        wxString executable;

        if (!wxFindFileInPath(
                &executable,
                path,
                name))
        {
            return wxString();
        }

        if (!wxFileName::IsFileExecutable(executable))
            return wxString();

        return executable;
    }
}

OgreToolsAvailability LocateOgreTools()
{
    OgreToolsAvailability result;

#ifdef __WXOSX__
    const wxString home = wxGetHomeDir();

    if (!home.IsEmpty())
    {
        result.managedPrefix =
            home
            + wxFILE_SEP_PATH + wxT("Library")
            + wxFILE_SEP_PATH + wxT("CodeBlocks")
            + wxFILE_SEP_PATH + wxT("ogre-14.6.3");

        result.toolsDirectory =
            result.managedPrefix
            + wxFILE_SEP_PATH + wxT("bin")
            + wxFILE_SEP_PATH + wxT("macosx");

        if (wxFileName::DirExists(
                result.toolsDirectory))
        {
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
        }
        else
        {
            result.toolsDirectory.Clear();
        }
    }
#endif // __WXOSX__

    // PATH is deliberately lower priority than the managed installation.
    // Only command-line tools use PATH fallback.
    if (result.assimpConverter.IsEmpty())
    {
        result.assimpConverter =
            FindExecutableOnPath(
                wxT("OgreAssimpConverter"));
    }

    if (result.meshUpgrader.IsEmpty())
    {
        result.meshUpgrader =
            FindExecutableOnPath(
                wxT("OgreMeshUpgrader"));
    }

    if (result.xmlConverter.IsEmpty())
    {
        result.xmlConverter =
            FindExecutableOnPath(
                wxT("OgreXMLConverter"));
    }

    if (result.vrmlConverter.IsEmpty())
    {
        result.vrmlConverter =
            FindExecutableOnPath(
                wxT("VRMLConverter"));
    }

    return result;
}
