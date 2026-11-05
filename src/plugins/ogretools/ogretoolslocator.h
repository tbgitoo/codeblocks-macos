#ifndef OGRETOOLSLOCATOR_H
#define OGRETOOLSLOCATOR_H

#include <wx/string.h>

struct OgreToolsAvailability
{
    wxString managedPrefix;
    wxString toolsDirectory;

    wxString assimpConverter;
    wxString meshUpgrader;
    wxString xmlConverter;
    wxString vrmlConverter;
    wxString sampleBrowser;
};

OgreToolsAvailability LocateOgreTools();

#endif // OGRETOOLSLOCATOR_H
