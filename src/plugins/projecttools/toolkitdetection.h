#ifndef TOOLKITDETECTION_H
#define TOOLKITDETECTION_H

#include "projecttoolcontext.h"
#include "toolkitprovider.h"

#include <wx/vector.h>

struct CurrentProjectToolkitDetection
{
    ProjectToolContext context;
    wxVector<ToolkitDetection> providers;
};

CurrentProjectToolkitDetection DetectCurrentProjectToolkits();

#endif // TOOLKITDETECTION_H
