#ifndef OGREIMPORTDLG_H
#define OGREIMPORTDLG_H

#include <wx/arrstr.h>
#include <wx/string.h>

class wxWindow;

struct OgreImportRequest
{
    wxString converter;
    wxString source;
    wxString destination;
};

struct OgreImportResult
{
    bool launched = false;
    bool ok = false;
    long exitCode = -1;
    bool meshWritten = false;
    bool materialWritten = false;
    bool sourceUnchanged = true;
    // A referenced material file was not loaded, so the script is a fallback.
    bool materialFallback = false;
    // The source defined no material. The written script is the converter default.
    bool defaultMaterial = false;

    wxString commandSummary;
    wxString output;
    wxString errorOutput;
    wxString meshPath;
    wxString materialPath;
    wxArrayString copiedTextures;
    wxArrayString resourceWarnings;
    wxString summary;
};

// Returns false when the converter is missing or the user cancels.
// A true return is a verified converter, an existing source file, and a
// destination outside the managed OGRE installation.
bool PromptOgreImport(wxWindow* parent, OgreImportRequest& request);

// Runs OgreAssimpConverter with a separate argument vector.
// Writes mesh and material files only in the destination directory.
OgreImportResult RunOgreAssimpImport(
    wxWindow* parent,
    const OgreImportRequest& request);

// For an OBJ source, copies textures named by its material library into
// the destination and records them on the result. The source files stay put.
wxString CopyObjTextures(
    const OgreImportRequest& request,
    OgreImportResult& result);

// After textures have been copied, reports material texture names that are
// still not in the destination. This does not change result.ok.
wxString NoteMaterialTextureFiles(
    const OgreImportRequest& request,
    OgreImportResult& result);

// After a successful conversion, asks before editing an existing
// resources.cfg and before adding generated files to the active project.
// Creates a starter resources.cfg when the destination has none.
// Returns text describing what the user confirmed.
wxString RegisterOgreImport(
    wxWindow* parent,
    const OgreImportRequest& request,
    const OgreImportResult& result);

#endif // OGREIMPORTDLG_H
