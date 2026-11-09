#include <sdk.h>

#include "ogreimportdlg.h"
#include "ogretoolslocator.h"

#include <cbproject.h>
#include <configmanager.h>
#include <editorbase.h>
#include <editormanager.h>
#include <manager.h>
#include <projectfile.h>
#include <projectmanager.h>

#include <wx/arrstr.h>
#include <wx/busyinfo.h>
#include <wx/button.h>
#include <wx/datetime.h>
#include <wx/dialog.h>
#include <wx/dirdlg.h>
#include <wx/file.h>
#include <wx/filefn.h>
#include <wx/filedlg.h>
#include <wx/filename.h>
#include <wx/process.h>
#include <wx/sizer.h>
#include <wx/stattext.h>
#include <wx/textctrl.h>
#include <wx/textfile.h>
#include <wx/utils.h>

namespace
{
    bool HasControlCharacter(const wxString& value)
    {
        for (size_t i = 0; i < value.length(); ++i)
        {
            if (value[i].GetValue() < 32)
                return true;
        }

        return false;
    }

    wxString NormalisedDirectory(const wxString& path)
    {
        if (path.IsEmpty())
            return wxString();

        wxFileName name = wxFileName::DirName(path);

        name.Normalize(
            wxPATH_NORM_ENV_VARS
            | wxPATH_NORM_DOTS
            | wxPATH_NORM_ABSOLUTE
            | wxPATH_NORM_TILDE);

        wxString full = name.GetFullPath();

        if (full.IsEmpty())
            return wxString();

        if (!full.EndsWith(wxFILE_SEP_PATH))
            full += wxFILE_SEP_PATH;

        return full;
    }

    bool DirectoryIsInside(
        const wxString& directory,
        const wxString& root)
    {
        const wxString dir = NormalisedDirectory(directory);
        const wxString base = NormalisedDirectory(root);

        if (dir.IsEmpty() || base.IsEmpty())
            return false;

#ifdef __WXOSX__
        return dir.Lower().StartsWith(base.Lower());
#else
        return dir.StartsWith(base);
#endif
    }

    wxString UsableDestination(
        const wxString& directory,
        const wxString& managedPrefix)
    {
        if (directory.IsEmpty() || directory == wxT("."))
            return wxString();

        if (!wxFileName::DirExists(directory))
            return wxString();

        if (DirectoryIsInside(directory, managedPrefix))
            return wxString();

        return directory;
    }

    wxString StandaloneFileDestination(const wxString& managedPrefix)
    {
        if (!Manager::Get() ||
            !Manager::Get()->GetEditorManager() ||
            !Manager::Get()->GetProjectManager())
        {
            return wxString();
        }

        EditorBase* editor =
            Manager::Get()->GetEditorManager()->GetActiveEditor();

        if (!editor)
            return wxString();

        const wxString filename = editor->GetFilename();

        if (filename.IsEmpty())
            return wxString();

        wxFileName file(filename);

        file.Normalize(
            wxPATH_NORM_ENV_VARS
            | wxPATH_NORM_DOTS
            | wxPATH_NORM_ABSOLUTE
            | wxPATH_NORM_TILDE);

        if (!file.FileExists())
            return wxString();

        const wxString fullPath = file.GetFullPath();

        if (Manager::Get()->GetProjectManager()->FindProjectForFile(
                UnixFilename(fullPath),
                nullptr,
                false,
                true))
        {
            return wxString();
        }

        return UsableDestination(file.GetPath(), managedPrefix);
    }

    wxString ProjectDestination(const wxString& managedPrefix)
    {
        if (!Manager::Get() ||
            !Manager::Get()->GetProjectManager())
        {
            return wxString();
        }

        cbProject* project =
            Manager::Get()->GetProjectManager()->GetActiveProject();

        if (!project || project->GetFilename().IsEmpty())
            return wxString();

        return UsableDestination(project->GetBasePath(), managedPrefix);
    }

    enum DestinationOrigin
    {
        DestinationNone = 0,
        DestinationOpenFile,
        DestinationProject
    };

    struct InitialDestination
    {
        wxString path;
        DestinationOrigin origin;

        InitialDestination()
            : origin(DestinationNone)
        {
        }
    };

    InitialDestination SuggestedDestination(const wxString& managedPrefix)
    {
        InitialDestination result;

        result.path = StandaloneFileDestination(managedPrefix);

        if (!result.path.IsEmpty())
        {
            result.origin = DestinationOpenFile;
            return result;
        }

        result.path = ProjectDestination(managedPrefix);

        if (!result.path.IsEmpty())
            result.origin = DestinationProject;

        return result;
    }

    void Report(wxWindow* parent, const wxString& message)
    {
        cbMessageBox(
            message,
            _("Import 3D asset"),
            wxICON_WARNING | wxOK,
            parent);
    }

    wxString ReadAll(wxInputStream* in)
    {
        wxString out;

        if (!in)
            return out;

        char buf[4096];

        while (!in->Eof())
        {
            in->Read(buf, sizeof(buf));

            const size_t n = in->LastRead();

            if (n == 0)
                break;

            const wxString chunk = wxString::FromUTF8(buf, n);

            if (chunk.empty())
                out += wxString(buf, wxConvISO8859_1, n);
            else
                out += chunk;
        }

        return out;
    }

    wxString ReadFile(const wxString& path)
    {
        wxFile file;

        if (!file.Open(path))
            return wxString();

        const wxFileOffset length = file.Length();

        if (length <= 0 || length > 1024 * 1024)
            return wxString();

        wxString text;
        file.ReadAll(&text);
        return text;
    }

    wxString ConverterWorkingDirectory(const wxString& converter)
    {
        const wxString directory = wxFileName(converter).GetPath();

        if (wxFileName(directory, wxT("plugins.cfg")).FileExists())
            return directory;

        wxFileName parent = wxFileName::DirName(directory);
        parent.RemoveLastDir();

        const wxString parentDir = parent.GetPath();

        if (wxFileName(parentDir, wxT("plugins.cfg")).FileExists())
            return parentDir;

        return directory;
    }

    struct FileStamp
    {
        bool exists;
        wxDateTime modified;
        wxULongLong size;

        FileStamp()
            : exists(false),
              size(0)
        {
        }
    };

    FileStamp StampFile(const wxString& path)
    {
        FileStamp stamp;
        wxFileName file(path);

        stamp.exists = file.FileExists();

        if (!stamp.exists)
            return stamp;

        stamp.modified = file.GetModificationTime();
        stamp.size = file.GetSize();
        return stamp;
    }

    bool FileWasWritten(const FileStamp& before, const wxString& path)
    {
        const FileStamp after = StampFile(path);

        if (!after.exists)
            return false;

        if (!before.exists)
            return true;

        return after.modified != before.modified
            || after.size != before.size;
    }

    wxString OutputFile(
        const wxString& directory,
        const wxString& source,
        const wxString& extension)
    {
        wxFileName file;
        file.AssignDir(directory);
        file.SetFullName(wxFileName(source).GetName() + extension);
        return file.GetFullPath();
    }

    wxString CanonicalDirectory(const wxString& path)
    {
        wxFileName name = wxFileName::DirName(path);

        name.Normalize(
            wxPATH_NORM_ENV_VARS
            | wxPATH_NORM_DOTS
            | wxPATH_NORM_ABSOLUTE
            | wxPATH_NORM_TILDE);

        wxString full = name.GetFullPath();

        while (full.Contains(wxString(wxFILE_SEP_PATH) + wxFILE_SEP_PATH))
        {
            full.Replace(
                wxString(wxFILE_SEP_PATH) + wxFILE_SEP_PATH,
                wxString(wxFILE_SEP_PATH));
        }

        while (full.EndsWith(wxFILE_SEP_PATH) && full.length() > 1)
            full.RemoveLast();

#ifdef __WXOSX__
        return full.Lower();
#else
        return full;
#endif
    }

    wxString ResourcesConfigPath(const wxString& destination)
    {
        wxFileName file;
        file.AssignDir(destination);
        file.SetFullName(wxT("resources.cfg"));
        return file.GetFullPath();
    }

    wxString ResolveListedDirectory(
        const wxString& value,
        const wxString& configDirectory)
    {
        wxString arch = value;
        arch.Trim(true).Trim(false);

        if (arch.IsEmpty())
            return wxString();

        if (arch[0] == wxT('.'))
        {
            wxString combined = configDirectory;

            if (!combined.EndsWith(wxFILE_SEP_PATH))
                combined += wxFILE_SEP_PATH;

            combined += arch;
            return CanonicalDirectory(combined);
        }

        return CanonicalDirectory(arch);
    }

    bool DestinationAlreadyListed(
        const wxString& configPath,
        const wxString& destination)
    {
        wxTextFile file(configPath);

        if (!file.Open())
            return false;

        const wxString configDirectory = wxFileName(configPath).GetPath();
        const wxString wanted = CanonicalDirectory(destination);

        for (size_t i = 0; i < file.GetLineCount(); ++i)
        {
            const wxString line = file.GetLine(i);

            if (line.empty() ||
                line[0] == wxT('#') ||
                line[0] == wxT('@'))
            {
                continue;
            }

            wxString trimmed = line;
            trimmed.Trim(true).Trim(false);

            if (trimmed.StartsWith(wxT("[")))
                continue;

            const int eq = trimmed.Find(wxT('='));

            if (eq == wxNOT_FOUND)
                continue;

            wxString key = trimmed.Left(eq);
            wxString value = trimmed.Mid(eq + 1);
            key.Trim(true).Trim(false);
            value.Trim(true).Trim(false);

            if (key != wxT("FileSystem"))
                continue;

            if (ResolveListedDirectory(value, configDirectory) == wanted)
                return true;
        }

        return false;
    }

    bool AppendDestinationLocation(
        const wxString& configPath,
        const wxString& destination)
    {
        wxTextFile file(configPath);

        if (!file.Open())
            return false;

        file.AddLine(wxEmptyString);
        file.AddLine(
            wxT("# Destination directory for an imported 3D asset."));
        file.AddLine(wxT("[General]"));
        file.AddLine(wxT("FileSystem=") + destination);

        const bool wrote = file.Write();
        file.Close();
        return wrote;
    }

    bool CreateStarterResources(
        const wxString& configPath)
    {
        wxTextFile file(configPath);

        if (!file.Create())
            return false;

        file.AddLine(
            wxT("# Starter resource locations for imported OGRE assets."));
        file.AddLine(
            wxT("# OGRE loads this file automatically only when it is in a directory the program already searches."));
        file.AddLine(wxT("[General]"));
        file.AddLine(wxT("FileSystem=."));

        const bool wrote = file.Write();
        file.Close();
        return wrote;
    }

    bool ProjectAlreadyContains(
        cbProject* project,
        const wxString& path)
    {
        return project
            && project->GetFileByFilename(path, false, false);
    }

    const wxString kLastSourceKey = wxT("/import/last_source");

    wxString LastSourceAsset()
    {
        if (!Manager::Get())
            return wxString();

        ConfigManager* cfg =
            Manager::Get()->GetConfigManager(wxT("ogretools"));

        if (!cfg)
            return wxString();

        return cfg->Read(kLastSourceKey);
    }

    void RememberSourceAsset(const wxString& path)
    {
        if (!Manager::Get() || path.IsEmpty())
            return;

        ConfigManager* cfg =
            Manager::Get()->GetConfigManager(wxT("ogretools"));

        if (cfg)
            cfg->Write(kLastSourceKey, path);
    }

    void AddVisibleProjectFile(
        cbProject* project,
        const wxString& path)
    {
        const int existingTargets = project->GetBuildTargetsCount();
        const int limit = existingTargets > 0 ? existingTargets : 1;

        for (int i = 0; i < limit; ++i)
        {
            ProjectFile* file = project->AddFile(i, path, false, false);

            if (!file)
                continue;

            file->compile = false;
            file->link = false;
        }
    }

    wxArrayString SplitTokens(const wxString& text)
    {
        wxArrayString tokens;
        wxString current;
        bool quoted = false;

        for (size_t i = 0; i < text.length(); ++i)
        {
            const wxUniChar ch = text[i];

            if (ch == wxT('"'))
            {
                quoted = !quoted;
                continue;
            }

            if (!quoted && wxIsspace(ch))
            {
                if (!current.IsEmpty())
                {
                    tokens.Add(current);
                    current.clear();
                }
                continue;
            }

            current += ch;
        }

        if (!current.IsEmpty())
            tokens.Add(current);

        return tokens;
    }

    void AddResourceWarning(
        OgreImportResult& result,
        wxString& summary,
        const wxString& text)
    {
        summary += text;

        wxString warning = text;
        warning.Trim(true).Trim(false);

        if (!warning.IsEmpty() &&
            result.resourceWarnings.Index(warning) == wxNOT_FOUND)
        {
            result.resourceWarnings.Add(warning);
        }
    }

    bool IsReferencedResourceLine(const wxString& line)
    {
        const wxString lower = line.Lower();

        if (lower.Contains(wxT("shadegenerator")))
            return false;

        if (lower.Contains(wxT("falling back to")))
            return false;

        return lower.Contains(wxT("unable to locate"))
            || lower.Contains(wxT("fallback material file"))
            || lower.Contains(wxT("cannot locate resource"));
    }

    void CollectReferencedResourceLines(
        const wxString& text,
        wxArrayString& lines)
    {
        size_t start = 0;

        while (start < text.length())
        {
            const size_t end = text.find(wxT('\n'), start);
            wxString line = text.Mid(
                start,
                end == wxString::npos ? wxString::npos : end - start);

            line.Replace(wxT("\r"), wxT(""));
            line.Trim(true).Trim(false);

            if (IsReferencedResourceLine(line) &&
                lines.Index(line) == wxNOT_FOUND)
            {
                lines.Add(line);
            }

            if (end == wxString::npos)
                break;

            start = end + 1;
        }
    }

    bool IsDefaultMaterialName(const wxString& name)
    {
        const wxString lower = name.Lower();

        return lower.Contains(wxT("defaultmaterial"))
            || lower.Contains(wxT("dummymat"));
    }

    void ReadMaterialNames(
        const wxString& path,
        wxArrayString& names,
        bool& opened)
    {
        opened = false;
        names.Clear();

        wxTextFile file(path);

        if (!file.Open())
            return;

        opened = true;

        for (size_t line = 0; line < file.GetLineCount(); ++line)
        {
            const wxArrayString tokens = SplitTokens(file.GetLine(line));

            if (tokens.GetCount() >= 2 && tokens[0] == wxT("material"))
                names.Add(tokens[1]);
        }

        file.Close();
    }

    bool AssessConversion(OgreImportResult& result)
    {
        wxArrayString resourceLines;
        CollectReferencedResourceLines(result.output, resourceLines);
        CollectReferencedResourceLines(result.errorOutput, resourceLines);

        wxArrayString materialNames;
        bool materialOpened = false;

        if (result.materialWritten)
        {
            ReadMaterialNames(
                result.materialPath,
                materialNames,
                materialOpened);
        }

        const bool referencedFileMissing = !resourceLines.IsEmpty();
        bool everyMaterialIsDefault = !materialNames.IsEmpty();

        for (size_t i = 0; i < materialNames.GetCount(); ++i)
        {
            if (!IsDefaultMaterialName(materialNames[i]))
                everyMaterialIsDefault = false;
        }

        result.materialFallback = referencedFileMissing;
        result.defaultMaterial =
            everyMaterialIsDefault && !referencedFileMissing;

        if (result.materialWritten && materialOpened && materialNames.IsEmpty())
        {
            result.summary +=
                _("The material script contains no material definition.\n");
        }

        if (result.materialFallback)
        {
            result.summary +=
                _("The material is a fallback. A file referenced by the source was not loaded:\n");

            for (size_t i = 0; i < resourceLines.GetCount(); ++i)
                result.summary += resourceLines[i] + wxT("\n");
        }
        else if (result.defaultMaterial)
        {
            result.summary +=
                _("The material script is the converter default. The source did not define its own material.\n");
        }

        for (size_t i = 0; i < resourceLines.GetCount(); ++i)
        {
            if (result.resourceWarnings.Index(resourceLines[i]) == wxNOT_FOUND)
                result.resourceWarnings.Add(resourceLines[i]);
        }

        const bool materialScriptEmpty =
            result.materialWritten
            && materialOpened
            && materialNames.IsEmpty();

        return !result.materialFallback && !materialScriptEmpty;
    }

    bool IsTextureDirective(const wxString& key)
    {
        const wxString lower = key.Lower();

        return lower.StartsWith(wxT("map_"))
            || lower == wxT("bump")
            || lower == wxT("disp")
            || lower == wxT("decal")
            || lower == wxT("refl")
            || lower == wxT("norm");
    }

    wxString FindTextureFile(
        const wxString& reference,
        const wxString& materialDirectory,
        const wxString& objectDirectory)
    {
        if (reference.IsEmpty())
            return wxString();

        wxFileName absolute(reference);

        if (absolute.IsAbsolute() && absolute.FileExists())
            return absolute.GetFullPath();

        const wxString directories[2] = {
            materialDirectory,
            objectDirectory
        };

        for (int i = 0; i < 2; ++i)
        {
            if (directories[i].IsEmpty())
                continue;

            wxString combined = directories[i];

            if (!combined.EndsWith(wxFILE_SEP_PATH))
                combined += wxFILE_SEP_PATH;

            combined += reference;

            wxFileName file(combined);
            file.Normalize(
                wxPATH_NORM_ENV_VARS
                | wxPATH_NORM_DOTS
                | wxPATH_NORM_ABSOLUTE
                | wxPATH_NORM_TILDE);

            if (file.FileExists())
                return file.GetFullPath();
        }

        return wxString();
    }
}

class OgreImportDialog : public wxDialog
{
public:
    OgreImportDialog(
        wxWindow* parent,
        const wxString& converter,
        const wxString& managedPrefix)
        : wxDialog(
              parent,
              wxID_ANY,
              _("Import 3D asset"),
              wxDefaultPosition,
              wxDefaultSize,
              wxDEFAULT_DIALOG_STYLE | wxRESIZE_BORDER),
          m_managedPrefix(managedPrefix),
          m_converter(nullptr),
          m_source(nullptr),
          m_destination(nullptr)
    {
        const int idBrowseSource = wxNewId();
        const int idBrowseDestination = wxNewId();
        const int idImport = wxNewId();

        wxBoxSizer* outer = new wxBoxSizer(wxVERTICAL);

        wxStaticText* intro = new wxStaticText(
            this,
            wxID_ANY,
            _("Convert a 3D asset with OgreAssimpConverter. "
              "The source file is left unchanged. "
              "Mesh and material files are written only to the destination."));

        intro->Wrap(520);
        outer->Add(intro, 0, wxALL | wxEXPAND, 12);

        outer->Add(
            new wxStaticText(this, wxID_ANY, _("OgreAssimpConverter")),
            0,
            wxLEFT | wxRIGHT,
            12);

        m_converter = new wxTextCtrl(
            this,
            wxID_ANY,
            converter,
            wxDefaultPosition,
            wxDefaultSize,
            wxTE_READONLY);

        outer->Add(
            m_converter,
            0,
            wxLEFT | wxRIGHT | wxTOP | wxEXPAND,
            12);

        outer->Add(
            new wxStaticText(this, wxID_ANY, _("Source asset")),
            0,
            wxLEFT | wxRIGHT | wxTOP,
            12);

        wxBoxSizer* sourceRow = new wxBoxSizer(wxHORIZONTAL);

        const wxString lastSource = LastSourceAsset();
        wxString initialSource;

        if (!lastSource.IsEmpty() && wxFileName::FileExists(lastSource))
            initialSource = lastSource;

        m_source = new wxTextCtrl(this, wxID_ANY, initialSource);

        sourceRow->Add(m_source, 1, wxRIGHT | wxEXPAND, 8);
        sourceRow->Add(
            new wxButton(this, idBrowseSource, _("Browse...")),
            0,
            wxALIGN_CENTER_VERTICAL);

        outer->Add(
            sourceRow,
            0,
            wxLEFT | wxRIGHT | wxTOP | wxEXPAND,
            12);

        outer->Add(
            new wxStaticText(this, wxID_ANY, _("Destination directory")),
            0,
            wxLEFT | wxRIGHT | wxTOP,
            12);

        wxBoxSizer* destinationRow = new wxBoxSizer(wxHORIZONTAL);

        const InitialDestination initial =
            SuggestedDestination(managedPrefix);

        m_destination = new wxTextCtrl(
            this,
            wxID_ANY,
            initial.path);

        destinationRow->Add(m_destination, 1, wxRIGHT | wxEXPAND, 8);
        destinationRow->Add(
            new wxButton(this, idBrowseDestination, _("Browse...")),
            0,
            wxALIGN_CENTER_VERTICAL);

        outer->Add(
            destinationRow,
            0,
            wxLEFT | wxRIGHT | wxTOP | wxEXPAND,
            12);

        wxString destinationNoteText =
            _("Choose a directory outside the installed OGRE SDK.");

        if (initial.origin == DestinationOpenFile)
        {
            destinationNoteText =
                _("The destination starts in the open file's directory. "
                  "It must stay outside the installed OGRE SDK.");
        }
        else if (initial.origin == DestinationProject)
        {
            destinationNoteText =
                _("The destination starts in the active project. "
                  "It must stay outside the installed OGRE SDK.");
        }

        wxStaticText* destinationNote = new wxStaticText(
            this,
            wxID_ANY,
            destinationNoteText);

        destinationNote->Wrap(520);
        outer->Add(
            destinationNote,
            0,
            wxLEFT | wxRIGHT | wxTOP | wxEXPAND,
            12);

        wxBoxSizer* buttons = new wxBoxSizer(wxHORIZONTAL);
        buttons->AddStretchSpacer();

        wxButton* importButton = new wxButton(
            this,
            idImport,
            _("Import"));

        importButton->SetDefault();

        buttons->Add(importButton, 0, wxRIGHT, 8);
        buttons->Add(
            new wxButton(this, wxID_CANCEL, _("Cancel")),
            0);

        outer->Add(
            buttons,
            0,
            wxALL | wxEXPAND,
            12);

        SetSizer(outer);
        SetInitialSize(wxSize(560, -1));
        CentreOnParent();

        Bind(
            wxEVT_BUTTON,
            &OgreImportDialog::OnBrowseSource,
            this,
            idBrowseSource);

        Bind(
            wxEVT_BUTTON,
            &OgreImportDialog::OnBrowseDestination,
            this,
            idBrowseDestination);

        Bind(
            wxEVT_BUTTON,
            &OgreImportDialog::OnImport,
            this,
            idImport);
    }

private:
    void OnBrowseSource(wxCommandEvent& event)
    {
        (void)event;

        wxString initialDir;
        wxString initialFile;
        wxString remembered = m_source->GetValue();

        if (remembered.IsEmpty())
            remembered = LastSourceAsset();

        if (!remembered.IsEmpty())
        {
            wxFileName current(remembered);
            initialDir = current.GetPath();
            initialFile = current.GetFullName();
        }

        if (initialDir.IsEmpty() || !wxFileName::DirExists(initialDir))
            initialDir = m_destination->GetValue();

        wxFileDialog dlg(
            this,
            _("Select 3D asset"),
            initialDir,
            initialFile,
            _("3D assets|*.obj;*.dae;*.fbx;*.gltf;*.glb;*.3ds;*.blend;*.stl;*.ply;*.x;*.md5mesh;*.lwo|All files (*.*)|*.*"),
            wxFD_OPEN | wxFD_FILE_MUST_EXIST);

        PlaceWindow(&dlg);

        if (dlg.ShowModal() != wxID_OK)
            return;

        m_source->SetValue(dlg.GetPath());
        RememberSourceAsset(dlg.GetPath());

        if (!m_destination->GetValue().IsEmpty())
            return;

        const wxString sourceDir =
            wxFileName(dlg.GetPath()).GetPath();

        if (sourceDir.IsEmpty())
            return;

        if (DirectoryIsInside(sourceDir, m_managedPrefix))
            return;

        m_destination->SetValue(sourceDir);
    }

    void OnBrowseDestination(wxCommandEvent& event)
    {
        (void)event;

        wxDirDialog dlg(
            this,
            _("Select destination directory"),
            m_destination->GetValue(),
            wxDD_DEFAULT_STYLE | wxDD_DIR_MUST_EXIST | wxDD_NEW_DIR_BUTTON);

        PlaceWindow(&dlg);

        if (dlg.ShowModal() != wxID_OK)
            return;

        m_destination->SetValue(dlg.GetPath());
    }

    void OnImport(wxCommandEvent& event)
    {
        (void)event;

        const OgreToolsAvailability tools = LocateOgreTools();

        if (tools.assimpConverter.IsEmpty())
        {
            m_converter->Clear();
            Report(
                this,
                _("OgreAssimpConverter is no longer available."));
            return;
        }

        m_converter->SetValue(tools.assimpConverter);
        m_managedPrefix = tools.managedPrefix;

        const wxString source = m_source->GetValue();

        if (source.IsEmpty())
        {
            Report(this, _("Choose a source asset."));
            return;
        }

        if (HasControlCharacter(source))
        {
            Report(
                this,
                _("The source path contains a control character."));
            return;
        }

        wxFileName sourceName(source);
        sourceName.Normalize(
            wxPATH_NORM_ENV_VARS
            | wxPATH_NORM_DOTS
            | wxPATH_NORM_ABSOLUTE
            | wxPATH_NORM_TILDE);

        if (!sourceName.FileExists())
        {
            Report(this, _("The source asset does not exist."));
            return;
        }

        const wxString destination = m_destination->GetValue();

        if (destination.IsEmpty())
        {
            Report(this, _("Choose a destination directory."));
            return;
        }

        if (HasControlCharacter(destination))
        {
            Report(
                this,
                _("The destination path contains a control character."));
            return;
        }

        wxFileName destinationFile(destination);

        destinationFile.Normalize(
            wxPATH_NORM_ENV_VARS
            | wxPATH_NORM_DOTS
            | wxPATH_NORM_ABSOLUTE
            | wxPATH_NORM_TILDE);

        if (destinationFile.FileExists())
        {
            Report(
                this,
                _("The destination must be a directory."));
            return;
        }

        const wxString normalised = NormalisedDirectory(destination);

        if (normalised.IsEmpty() ||
            normalised == wxString(wxFILE_SEP_PATH))
        {
            Report(
                this,
                _("Choose a destination directory."));
            return;
        }

        if (!wxFileName::DirExists(normalised))
        {
            Report(
                this,
                _("The destination directory does not exist."));
            return;
        }

        if (DirectoryIsInside(normalised, m_managedPrefix))
        {
            Report(
                this,
                _("The destination is inside the installed OGRE SDK. "
                  "Choose a directory outside that installation."));
            return;
        }

        wxString acceptedDestination = normalised;

        if (acceptedDestination.EndsWith(wxFILE_SEP_PATH) &&
            acceptedDestination.length() > 1)
        {
            acceptedDestination.RemoveLast();
        }

        m_acceptedConverter = tools.assimpConverter;
        m_acceptedSource = sourceName.GetFullPath();
        m_acceptedDestination = acceptedDestination;

        RememberSourceAsset(m_acceptedSource);

        EndModal(wxID_OK);
    }

    wxString m_managedPrefix;
    wxTextCtrl* m_converter;
    wxTextCtrl* m_source;
    wxTextCtrl* m_destination;

    wxString m_acceptedConverter;
    wxString m_acceptedSource;
    wxString m_acceptedDestination;

public:
    wxString AcceptedConverter() const
    {
        return m_acceptedConverter;
    }

    wxString AcceptedSource() const
    {
        return m_acceptedSource;
    }

    wxString AcceptedDestination() const
    {
        return m_acceptedDestination;
    }
};

bool PromptOgreImport(wxWindow* parent, OgreImportRequest& request)
{
    request = OgreImportRequest();

    const OgreToolsAvailability tools = LocateOgreTools();

    if (tools.assimpConverter.IsEmpty())
    {
        Report(
            parent,
            _("OgreAssimpConverter is not available.\n\n"
              "Install or repair the OGRE tools, then try again."));
        return false;
    }

    OgreImportDialog dlg(
        parent,
        tools.assimpConverter,
        tools.managedPrefix);

    PlaceWindow(&dlg);

    if (dlg.ShowModal() != wxID_OK)
        return false;

    request.converter = dlg.AcceptedConverter();
    request.source = dlg.AcceptedSource();
    request.destination = dlg.AcceptedDestination();
    return true;
}

OgreImportResult RunOgreAssimpImport(
    wxWindow* parent,
    const OgreImportRequest& request)
{
    OgreImportResult result;

    const wxString cwd = ConverterWorkingDirectory(request.converter);
    const wxString logFile =
        wxFileName::CreateTempFileName(wxT("OgreAssimp"));

    result.meshPath = OutputFile(
        request.destination,
        request.source,
        wxT(".mesh"));
    result.materialPath = OutputFile(
        request.destination,
        request.source,
        wxT(".material"));

    result.commandSummary =
        request.converter
        + wxT("\n  -log\n  ")
        + logFile
        + wxT("\n  ")
        + request.source
        + wxT("\n  ")
        + request.destination
        + wxT("\nworking directory: ")
        + cwd;

    if (logFile.IsEmpty())
    {
        result.summary =
            _("A temporary converter log could not be created, "
              "so OgreAssimpConverter was not started.");
        return result;
    }

    wxExecuteEnv env;

    if (!wxGetEnvMap(&env.env) || env.env.empty())
    {
        wxRemoveFile(logFile);
        result.summary =
            _("Could not copy the environment, "
              "so OgreAssimpConverter was not started.");
        return result;
    }

    env.cwd = cwd;

    const wxString converter = request.converter;
    const wxString source = request.source;
    const wxString destination = request.destination;
    const wxString logArgument = logFile;

    const wxChar* argv[6];

    argv[0] = converter.wx_str();
    argv[1] = wxT("-log");
    argv[2] = logArgument.wx_str();
    argv[3] = source.wx_str();
    argv[4] = destination.wx_str();
    argv[5] = nullptr;

    const FileStamp meshBefore = StampFile(result.meshPath);
    const FileStamp materialBefore = StampFile(result.materialPath);
    const FileStamp sourceBefore = StampFile(request.source);

    {
        wxBusyInfo wait(_("Converting the 3D asset..."), parent);
        wxProcess process(wxPROCESS_REDIRECT);

        result.exitCode = wxExecute(
            argv,
            wxEXEC_SYNC,
            &process,
            &env);

        result.output = ReadAll(process.GetInputStream());
        result.errorOutput = ReadAll(process.GetErrorStream());
    }

    result.launched = result.exitCode != -1;

    const wxString converterLog = ReadFile(logFile);
    wxRemoveFile(logFile);

    if (!converterLog.IsEmpty())
    {
        if (!result.output.IsEmpty() &&
            !result.output.EndsWith(wxT("\n")))
        {
            result.output += wxT("\n");
        }

        result.output += converterLog;
    }

    result.meshWritten = FileWasWritten(meshBefore, result.meshPath);
    result.materialWritten =
        FileWasWritten(materialBefore, result.materialPath);
    result.sourceUnchanged =
        !FileWasWritten(sourceBefore, request.source);

    if (!result.launched)
    {
        result.summary = _("OgreAssimpConverter did not start.");
        return result;
    }

    result.summary = wxString::Format(
        _("OgreAssimpConverter exited with status %ld."),
        result.exitCode);
    result.summary += wxT("\n\n");

    if (result.meshWritten)
        result.summary += _("Mesh:\n") + result.meshPath + wxT("\n");
    else
        result.summary += _("No mesh file was written.\n");

    if (result.materialWritten)
    {
        result.summary +=
            _("Material:\n") + result.materialPath + wxT("\n");
    }
    else
    {
        result.summary += _("No material script was written.\n");
    }

    if (!result.sourceUnchanged)
    {
        result.summary +=
            _("\nThe source asset changed during conversion.\n");
    }

    const bool materialContentOk = AssessConversion(result);

    result.ok =
        result.exitCode == 0
        && result.meshWritten
        && result.materialWritten
        && result.sourceUnchanged
        && materialContentOk;

    if (!result.ok)
    {
        result.summary +=
            _("\nresources.cfg and the project file were left unchanged.");
    }

    return result;
}

wxString CopyObjTextures(
    const OgreImportRequest& request,
    OgreImportResult& result)
{
    result.copiedTextures.Clear();

    const wxString extension = wxFileName(request.source).GetExt().Lower();

    if (extension != wxT("obj"))
        return wxString();

    wxTextFile objectFile(request.source);

    if (!objectFile.Open())
    {
        return _("\nThe OBJ file could not be read, so its textures were not copied.\n");
    }

    const wxString objectDirectory = wxFileName(request.source).GetPath();
    wxArrayString materialLibraries;

    for (size_t i = 0; i < objectFile.GetLineCount(); ++i)
    {
        const wxArrayString tokens = SplitTokens(objectFile.GetLine(i));

        if (tokens.IsEmpty() || tokens[0].Lower() != wxT("mtllib"))
            continue;

        for (size_t token = 1; token < tokens.GetCount(); ++token)
            materialLibraries.Add(tokens[token]);
    }

    objectFile.Close();

    if (materialLibraries.IsEmpty())
        return _("\nThe OBJ file names no material library, so no textures were copied.\n");

    wxArrayString seenDestinations;
    wxString summary;
    bool copiedAny = false;

    for (size_t library = 0; library < materialLibraries.GetCount(); ++library)
    {
        const wxString foundLibrary = FindTextureFile(
            materialLibraries[library],
            objectDirectory,
            objectDirectory);

        if (foundLibrary.IsEmpty())
        {
            AddResourceWarning(
                result,
                summary,
                _("\nMaterial library not found:\n")
                + materialLibraries[library]
                + wxT("\n"));
            continue;
        }

        wxTextFile materialFile(foundLibrary);

        if (!materialFile.Open())
        {
            AddResourceWarning(
                result,
                summary,
                _("\nMaterial library could not be read:\n")
                + foundLibrary
                + wxT("\n"));
            continue;
        }

        const wxString materialDirectory = wxFileName(foundLibrary).GetPath();

        for (size_t line = 0; line < materialFile.GetLineCount(); ++line)
        {
            const wxArrayString tokens = SplitTokens(materialFile.GetLine(line));

            if (tokens.GetCount() < 2 || !IsTextureDirective(tokens[0]))
                continue;

            const wxString reference = tokens[tokens.GetCount() - 1];
            const wxString foundTexture = FindTextureFile(
                reference,
                materialDirectory,
                objectDirectory);

            if (foundTexture.IsEmpty())
            {
                AddResourceWarning(
                    result,
                    summary,
                    _("\nTexture referenced by the OBJ was not found:\n")
                    + reference
                    + wxT("\n"));
                continue;
            }

            wxFileName destinationFile;
            destinationFile.AssignDir(request.destination);
            destinationFile.SetFullName(wxFileName(foundTexture).GetFullName());

            const wxString destinationPath = destinationFile.GetFullPath();

            if (seenDestinations.Index(destinationPath) != wxNOT_FOUND)
                continue;

            seenDestinations.Add(destinationPath);

            if (CanonicalDirectory(wxFileName(foundTexture).GetPath())
                == CanonicalDirectory(request.destination)
                && wxFileName(foundTexture).GetFullName()
                    == destinationFile.GetFullName())
            {
                result.copiedTextures.Add(destinationPath);
                copiedAny = true;
                continue;
            }

            if (!wxCopyFile(foundTexture, destinationPath, true))
            {
                AddResourceWarning(
                    result,
                    summary,
                    _("\nTexture could not be copied:\n")
                    + foundTexture
                    + wxT("\n"));
                continue;
            }

            result.copiedTextures.Add(destinationPath);
            copiedAny = true;
        }

        materialFile.Close();
    }

    if (copiedAny)
    {
        summary += _("\nCopied OBJ textures next to the mesh:\n");

        for (size_t i = 0; i < result.copiedTextures.GetCount(); ++i)
            summary += result.copiedTextures[i] + wxT("\n");
    }

    return summary;
}

wxString NoteMaterialTextureFiles(
    const OgreImportRequest& request,
    OgreImportResult& result)
{
    if (!result.materialWritten ||
        !wxFileName::FileExists(result.materialPath))
    {
        return wxString();
    }

    wxTextFile materialFile(result.materialPath);

    if (!materialFile.Open())
        return wxString();

    wxString summary;
    wxArrayString seen;

    for (size_t line = 0; line < materialFile.GetLineCount(); ++line)
    {
        const wxArrayString tokens = SplitTokens(materialFile.GetLine(line));

        if (tokens.GetCount() < 2 || tokens[0] != wxT("texture"))
            continue;

        const wxString reference = tokens[tokens.GetCount() - 1];

        if (reference.IsEmpty() || seen.Index(reference) != wxNOT_FOUND)
            continue;

        seen.Add(reference);

        wxFileName named(reference);

        if (named.IsAbsolute() && named.FileExists())
            continue;

        wxFileName inDestination;
        inDestination.AssignDir(request.destination);
        inDestination.SetFullName(wxFileName(reference).GetFullName());

        if (inDestination.FileExists())
            continue;

        bool alreadyWarned = false;

        for (size_t i = 0; i < result.resourceWarnings.GetCount(); ++i)
        {
            if (result.resourceWarnings[i].Contains(reference)
                || result.resourceWarnings[i].Contains(
                    wxFileName(reference).GetFullName()))
            {
                alreadyWarned = true;
                break;
            }
        }

        if (alreadyWarned)
            continue;

        AddResourceWarning(
            result,
            summary,
            _("\nTexture named by the material is not in the destination:\n")
            + reference
            + wxT("\n"));
    }

    materialFile.Close();
    return summary;
}

wxString RegisterOgreImport(
    wxWindow* parent,
    const OgreImportRequest& request,
    const OgreImportResult& result)
{
    wxString summary;
    const wxString configPath = ResourcesConfigPath(request.destination);
    bool createdConfig = false;

    if (wxFileName::FileExists(configPath))
    {
        if (DestinationAlreadyListed(configPath, request.destination))
        {
            summary +=
                _("\nThe destination directory is already listed in:\n")
                + configPath
                + wxT("\n");
        }
        else
        {
            const int addLocation = cbMessageBox(
                _("Add this destination directory to the existing resources.cfg?\n\n")
                + configPath
                + _("\n\nDirectory:\n")
                + request.destination
                + _("\n\nThe entry is a FileSystem location. The mesh filename is not added."),
                _("Import 3D asset"),
                wxYES_NO | wxICON_QUESTION,
                parent);

            if (addLocation == wxID_YES)
            {
                if (AppendDestinationLocation(
                        configPath,
                        request.destination))
                {
                    summary +=
                        _("\nAdded the destination directory to:\n")
                        + configPath
                        + wxT("\n");
                }
                else
                {
                    summary +=
                        _("\nCould not update:\n")
                        + configPath
                        + wxT("\n");
                }
            }
            else
            {
                summary +=
                    _("\nLeft the existing resources.cfg unchanged:\n")
                    + configPath
                    + wxT("\n");
            }
        }
    }
    else if (CreateStarterResources(configPath))
    {
        createdConfig = true;
        summary +=
            _("\nCreated a starter resources.cfg beside the mesh:\n")
            + configPath
            + _("\n\nOGRE loads this file automatically only when it is in a directory the program already searches. A resources.cfg in another folder is ignored.\n");
    }
    else
    {
        summary +=
            _("\nCould not create a starter resources.cfg:\n")
            + configPath
            + wxT("\n");
    }

    cbProject* project = nullptr;

    if (Manager::Get() && Manager::Get()->GetProjectManager())
        project = Manager::Get()->GetProjectManager()->GetActiveProject();

    if (!project)
    {
        summary += _("\nNo project is open, so the project tree was left unchanged.\n");
        return summary;
    }

    wxArrayString files;

    if (result.meshWritten && wxFileName::FileExists(result.meshPath))
        files.Add(result.meshPath);

    if (result.materialWritten &&
        wxFileName::FileExists(result.materialPath))
    {
        files.Add(result.materialPath);
    }

    if (createdConfig && wxFileName::FileExists(configPath))
        files.Add(configPath);

    for (size_t i = 0; i < result.copiedTextures.GetCount(); ++i)
    {
        if (wxFileName::FileExists(result.copiedTextures[i]))
            files.Add(result.copiedTextures[i]);
    }

    wxArrayString newFiles;

    for (size_t i = 0; i < files.GetCount(); ++i)
    {
        if (!ProjectAlreadyContains(project, files[i]))
            newFiles.Add(files[i]);
    }

    if (newFiles.IsEmpty())
    {
        summary +=
            _("\nThe generated files are already in the project.\n");
        return summary;
    }

    wxString question =
        wxString::Format(
            _("Add the generated files to the project \"%s\"?\n\nThey will appear in the project tree and will not be compiled.\n"),
            project->GetTitle());

    for (size_t i = 0; i < newFiles.GetCount(); ++i)
        question += wxT("\n") + newFiles[i];

    const int addToProject = cbMessageBox(
        question,
        _("Import 3D asset"),
        wxYES_NO | wxICON_QUESTION,
        parent);

    if (addToProject != wxID_YES)
    {
        summary +=
            _("\nLeft the project file unchanged.\n");
        return summary;
    }

    project->BeginAddFiles();

    for (size_t i = 0; i < newFiles.GetCount(); ++i)
        AddVisibleProjectFile(project, newFiles[i]);

    project->EndAddFiles();

    if (!project->Save())
    {
        summary +=
            _("\nThe generated files were added in memory, but the project file could not be saved.\n");
    }
    else
    {
        summary +=
            _("\nAdded the generated files to the project. They are not compiled.\n");
    }

    if (Manager::Get() && Manager::Get()->GetProjectManager())
        Manager::Get()->GetProjectManager()->GetUI().RebuildTree();

    return summary;
}
