#ifndef PROJECTTOOLS_H
#define PROJECTTOOLS_H

#include <cbplugin.h>

class ProjectTools : public cbPlugin
{
public:
    ProjectTools();
    ~ProjectTools() override;

    void OnAttach() override;
    void OnRelease(bool appShutDown) override;
};

#endif // PROJECTTOOLS_H
