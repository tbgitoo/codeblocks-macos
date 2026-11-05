#ifndef OGRETOOLS_H
#define OGRETOOLS_H

#include <cbplugin.h>

class OgreTools : public cbPlugin
{
public:
    OgreTools();
    ~OgreTools() override;

    void OnAttach() override;
    void OnRelease(bool appShutDown) override;
    void BuildMenu(wxMenuBar* menuBar) override;
    
private:
    void OnOpenToolsFolder(wxCommandEvent& event);
    void OnOpenSampleBrowser(wxCommandEvent& event);

    DECLARE_EVENT_TABLE();
    
};

#endif // OGRETOOLS_H
