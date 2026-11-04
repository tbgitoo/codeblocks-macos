#include <sdk.h>

#include "toolkitdetection.h"

CurrentProjectToolkitDetection DetectCurrentProjectToolkits()
{
    CurrentProjectToolkitDetection result;

    // Always collect a fresh snapshot of the current active project
    // and target configuration.
    result.context = CollectCurrentProjectToolContext();

    if (!result.context.HasProject())
        return result;

    // Provider implementations are added later.
    // M3 will add the first one: OgreToolkitProvider.

    return result;
}
