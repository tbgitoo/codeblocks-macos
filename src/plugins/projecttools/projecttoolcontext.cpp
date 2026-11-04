#include <sdk.h>

#include "projecttoolcontext.h"

#include <cbproject.h>
#include <compileoptionsbase.h>
#include <macrosmanager.h>
#include <manager.h>
#include <projectbuildtarget.h>
#include <projectmanager.h>

#include <wx/arrstr.h>
#include <wx/filename.h>

namespace
{
    struct ScopedString
    {
        wxString value;
        ProjectToolSettingScope scope;
    };

    typedef wxVector<ScopedString> ScopedStrings;

    void AppendValues(ScopedStrings& destination,
                      const wxArrayString& source,
                      ProjectToolSettingScope scope)
    {
        for (size_t index = 0; index < source.GetCount(); ++index)
        {
            ScopedString item;
            item.value = source[index];
            item.scope = scope;

            destination.push_back(item);
        }
    }

    ScopedStrings GetOrderedValues(
        const wxArrayString& projectValues,
        const wxArrayString& targetValues,
        OptionsRelation relation)
    {
        ScopedStrings result;

        switch (relation)
        {
            case orUseParentOptionsOnly:
                AppendValues(result,
                             projectValues,
                             ProjectToolSettingScope::Project);
                break;

            case orUseTargetOptionsOnly:
                AppendValues(result,
                             targetValues,
                             ProjectToolSettingScope::Target);
                break;

            case orPrependToParentOptions:
                AppendValues(result,
                             targetValues,
                             ProjectToolSettingScope::Target);

                AppendValues(result,
                             projectValues,
                             ProjectToolSettingScope::Project);
                break;

            case orAppendToParentOptions:
                AppendValues(result,
                             projectValues,
                             ProjectToolSettingScope::Project);

                AppendValues(result,
                             targetValues,
                             ProjectToolSettingScope::Target);
                break;

            default:
                // Preserve all visible configuration if an unexpected
                // OptionsRelation value is encountered.
                AppendValues(result,
                             projectValues,
                             ProjectToolSettingScope::Project);

                AppendValues(result,
                             targetValues,
                             ProjectToolSettingScope::Target);
                break;
        }

        return result;
    }

    wxString ExpandValue(const wxString& rawValue,
                         ProjectBuildTarget* target)
    {
        wxString expandedValue = rawValue;

        MacrosManager* macrosManager =
            Manager::Get()->GetMacrosManager();

        if (macrosManager)
            macrosManager->ReplaceMacros(expandedValue, target);

        return expandedValue;
    }

    wxString NormalizeProjectPath(
        const wxString& expandedValue,
        const wxString& projectBasePath)
    {
        if (expandedValue.IsEmpty())
            return wxString();

        wxFileName path(expandedValue);

        if (path.IsRelative())
            path.MakeAbsolute(projectBasePath);

        path.Normalize(wxPATH_NORM_DOTS |
                       wxPATH_NORM_TILDE |
                       wxPATH_NORM_ABSOLUTE);

        return path.GetFullPath();
    }

    void CollectPaths(
        wxVector<ProjectToolPath>& destination,
        const ScopedStrings& source,
        ProjectBuildTarget* target,
        const wxString& projectBasePath)
    {
        for (size_t index = 0; index < source.size(); ++index)
        {
            const ScopedString& sourceItem = source[index];

            ProjectToolPath destinationItem;

            destinationItem.rawValue = sourceItem.value;
            destinationItem.expandedValue =
                ExpandValue(sourceItem.value, target);

            destinationItem.normalizedPath =
                NormalizeProjectPath(
                    destinationItem.expandedValue,
                    projectBasePath);

            destinationItem.scope = sourceItem.scope;

            destination.push_back(destinationItem);
        }
    }

void CollectValues(
    wxVector<ProjectToolValue>& destination,
    const ScopedStrings& source,
    ProjectBuildTarget* target)
{
    for (size_t index = 0; index < source.size(); ++index)
    {
        const ScopedString& sourceItem = source[index];

        ProjectToolValue destinationItem;

        destinationItem.rawValue = sourceItem.value;
        destinationItem.expandedValue =
            ExpandValue(sourceItem.value, target);

        destinationItem.scope = sourceItem.scope;

        destination.push_back(destinationItem);
    }
}

}

ProjectToolContext CollectCurrentProjectToolContext()
{
    ProjectToolContext context;

    Manager* manager = Manager::Get();
    if (!manager)
        return context;

    ProjectManager* projectManager =
        manager->GetProjectManager();

    if (!projectManager)
        return context;

    cbProject* project =
        projectManager->GetActiveProject();

    if (!project)
        return context;

    const wxString targetName =
        project->GetActiveBuildTarget();

    ProjectBuildTarget* target =
        project->GetBuildTarget(targetName);

    if (!target)
        return context;

    context.projectTitle = project->GetTitle();
    context.projectFilename = project->GetFilename();
    context.projectBasePath = project->GetBasePath();
    context.targetTitle = target->GetTitle();

    const ScopedStrings includeDirectories =
        GetOrderedValues(
            project->GetIncludeDirs(),
            target->GetIncludeDirs(),
            target->GetOptionRelation(ortIncludeDirs));

    const ScopedStrings libraryDirectories =
        GetOrderedValues(
            project->GetLibDirs(),
            target->GetLibDirs(),
            target->GetOptionRelation(ortLibDirs));
    
    const ScopedStrings linkedLibraries =
        GetOrderedValues(
            project->GetLinkLibs(),
            target->GetLinkLibs(),
            target->GetOptionRelation(ortLinkerOptions));

    CollectPaths(context.includeDirectories,
                 includeDirectories,
                 target,
                 context.projectBasePath);

    CollectPaths(context.libraryDirectories,
                 libraryDirectories,
                 target,
                 context.projectBasePath);
    
    CollectValues(context.linkedLibraries,
                  linkedLibraries,
                  target);

    return context;
}
