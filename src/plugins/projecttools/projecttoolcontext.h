#ifndef PROJECTTOOLCONTEXT_H
#define PROJECTTOOLCONTEXT_H

#include <wx/string.h>
#include <wx/vector.h>

enum class ProjectToolSettingScope
{
    Project,
    Target
};

struct ProjectToolPath
{
    // Value exactly as stored in the project/target configuration.
    wxString rawValue;

    // Value after Code::Blocks target-aware macro expansion.
    wxString expandedValue;

    // Absolute, lexically normalized path used for discovery.
    wxString normalizedPath;

    ProjectToolSettingScope scope;
};

struct ProjectToolValue
{
    // For configuration values that are not necessarily paths,
    // for example linked library names.
    wxString rawValue;
    wxString expandedValue;
    ProjectToolSettingScope scope;
};

struct ProjectToolContext
{
    wxString projectTitle;
    wxString projectFilename;
    wxString projectBasePath;
    wxString targetTitle;

    wxVector<ProjectToolPath> includeDirectories;
    wxVector<ProjectToolPath> libraryDirectories;
    wxVector<ProjectToolValue> linkedLibraries;

    bool HasProject() const
    {
        return !projectFilename.IsEmpty();
    }
};

ProjectToolContext CollectCurrentProjectToolContext();

#endif // PROJECTTOOLCONTEXT_H
