#include <sdk.h>

#include "projecttools.h"

#include <cbplugin.h>

namespace
{
    PluginRegistrant<ProjectTools> reg(_T("ProjectTools"));
}

ProjectTools::ProjectTools()
{
}

ProjectTools::~ProjectTools()
{
}

void ProjectTools::OnAttach()
{
}

void ProjectTools::OnRelease(bool appShutDown)
{
    (void)appShutDown;
}
