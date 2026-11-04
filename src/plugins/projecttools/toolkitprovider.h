#ifndef TOOLKITPROVIDER_H
#define TOOLKITPROVIDER_H

#include "projecttoolcontext.h"

#include <wx/arrstr.h>
#include <wx/string.h>
#include <wx/vector.h>

struct ToolkitDetection
{
    wxString providerId;
    wxString displayName;

    bool evidenceFound = false;
    bool installationVerified = false;
    bool ambiguous = false;

    wxString selectedRoot;
    wxArrayString candidateRoots;
    wxArrayString diagnostics;
};

class ToolkitProvider
{
public:
    virtual ~ToolkitProvider() = default;

    virtual wxString GetId() const = 0;
    virtual wxString GetDisplayName() const = 0;

    virtual ToolkitDetection Detect(
        const ProjectToolContext& context) const = 0;
};

#endif // TOOLKITPROVIDER_H
