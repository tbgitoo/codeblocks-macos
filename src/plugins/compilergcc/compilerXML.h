#ifndef COMPILER_XML_H
#define COMPILER_XML_H

#include "compiler.h"

#include <wx/string.h>

class CompilerXML : public Compiler
{
    public:
        CompilerXML(const wxString& name, const wxString& ID, const wxString& file);
        ~CompilerXML() override;

        AutoDetectResult AutoDetectInstallationDir() override;

    protected:
        Compiler* CreateCopy() override;

    private:

        enum SearchMode
        {
            master,
            extra,
            include,
            resource,
            lib,
            none
        };

        bool AddPath(const wxString& pth, SearchMode sm, int rmDirs = 0);
#ifdef __WXMSW__
        // The official Windows LDC archive has no make.exe. Use mingw32-make
        // from the GNU GCC profile when the LDC tree does not provide one.
        bool EnsureWindowsLdcMake();
        // The official Windows SDCC installer also ships no make.exe.
        // Same rule, kept separate so the LDC detector is unchanged.
        bool EnsureWindowsSdccMake();
#endif

        wxString m_fileName;
};

#endif // COMPILER_XML_H
