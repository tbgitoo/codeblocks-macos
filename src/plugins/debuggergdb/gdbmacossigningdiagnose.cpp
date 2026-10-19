/*
 * This file is part of the Code::Blocks IDE and licensed under the GNU General Public License, version 3
 * http://www.gnu.org/licenses/gpl-3.0.html
 *
 * v2.3 M2: macOS bundled-GDB signing / task-port diagnosis.
 * v2.3 M3: Tier 1 ad-hoc + cs.debugger restoration (bundled only).
 */

#include "sdk.h"
#ifndef CB_PRECOMP
    #include <wx/dir.h>
    #include <wx/filename.h>
    #include <wx/utils.h>
    #include "configmanager.h"
    #include "prep.h"
#endif

#include "gdbmacossigningdiagnose.h"

namespace
{

wxString NormalizePath(const wxString &path)
{
    if (path.empty())
        return wxString();
    wxFileName fn(path);
    fn.Normalize(wxPATH_NORM_DOTS | wxPATH_NORM_ABSOLUTE | wxPATH_NORM_TILDE);
    return fn.GetFullPath();
}

wxString QuoteForShell(const wxString &path)
{
    // Single-quote for /bin/sh; escape embedded single quotes.
    wxString escaped = path;
    escaped.Replace("'", "'\\''");
    return "'" + escaped + "'";
}

bool LineLooksLikeTaskPortEvidence(const wxString &line)
{
    const wxString lower = line.Lower();
    return lower.Contains("mach task port")
        || lower.Contains("taskgated")
        || (lower.Contains("codesigned") && lower.Contains("taskgated"));
}

#ifdef __WXMAC__

wxString RunCapture(const wxString &command, int *exitCode)
{
    wxArrayString output;
    wxArrayString errors;
    const int rc = wxExecute(command, output, errors, wxEXEC_SYNC | wxEXEC_NODISABLE);
    if (exitCode)
        *exitCode = rc;
    wxString all;
    for (size_t i = 0; i < output.GetCount(); ++i)
    {
        if (!all.empty())
            all << '\n';
        all << output[i];
    }
    for (size_t i = 0; i < errors.GetCount(); ++i)
    {
        if (!all.empty())
            all << '\n';
        all << errors[i];
    }
    return all;
}

void FillCodesignFields(const wxString &gdbPath, GdbMacSigningDiagnosis &d)
{
    const wxString q = QuoteForShell(gdbPath);
    int verifyRc = -1;
    RunCapture("codesign --verify --verbose=2 " + q, &verifyRc);
    if (verifyRc != 0)
    {
        d.signature = GdbMacSignatureState::MissingOrInvalid;
        d.hasDebuggerEntitlement = false;
        return;
    }

    int dvRc = 0;
    const wxString dv = RunCapture("codesign -dv --verbose=4 " + q, &dvRc);
    if (dv.Contains("Signature=adhoc") || dv.Lower().Contains("signature=adhoc"))
        d.signature = GdbMacSignatureState::ValidAdhoc;
    else
        d.signature = GdbMacSignatureState::ValidNamed;

    int entRc = 0;
    const wxString ents = RunCapture("codesign -d --entitlements - " + q, &entRc);
    d.hasDebuggerEntitlement = ents.Contains("com.apple.security.cs.debugger");

    int xaRc = 0;
    const wxString xa = RunCapture("xattr -p com.apple.quarantine " + q, &xaRc);
    wxString xaTrimmed = xa;
    xaTrimmed.Trim(true).Trim(false);
    d.quarantinePresent = (xaRc == 0 && !xaTrimmed.empty());
}

#endif // __WXMAC__

} // namespace

bool GdbMacSigningDiagnosis::SigningDefective() const
{
    if (pathKind != GdbMacPathKind::Bundled)
        return false;
    if (signature == GdbMacSignatureState::MissingOrInvalid)
        return true;
    if (signature == GdbMacSignatureState::ValidAdhoc || signature == GdbMacSignatureState::ValidNamed)
        return !hasDebuggerEntitlement;
    return false;
}

bool GdbMacSigningDiagnosis::WouldOfferEnableDebugging() const
{
    if (pathKind != GdbMacPathKind::Bundled)
        return false;
    if (SigningDefective())
        return true;
    // Valid ad-hoc/named + entitlement: only with confirmed task-port evidence (Tier 2 later).
    if ((signature == GdbMacSignatureState::ValidAdhoc || signature == GdbMacSignatureState::ValidNamed)
        && hasDebuggerEntitlement
        && session.taskPortOrTaskgated)
    {
        return true;
    }
    return false;
}

wxString GetMacBundledGdbPath()
{
#ifdef __WXMAC__
    // Contents/Resources/share/codeblocks → ../../debugger/bin/gdb
    wxFileName bundled(ConfigManager::GetDataFolder() + "/../../debugger/bin/gdb");
    bundled.Normalize(wxPATH_NORM_DOTS | wxPATH_NORM_ABSOLUTE);
    return bundled.GetFullPath();
#else
    return wxString();
#endif
}

bool IsMacBundledGdbPath(const wxString &path)
{
    const wxString bundled = GetMacBundledGdbPath();
    if (bundled.empty() || path.empty())
        return false;
    return NormalizePath(path) == NormalizePath(bundled);
}

bool ClassifyGdbMacSessionLine(const wxString &line, GdbMacSessionEvidence &evidence)
{
    bool changed = false;
    const wxString trimmed = line.Strip(wxString::both);
    if (trimmed.StartsWith("No stack"))
    {
        if (!evidence.bareNoStack)
        {
            evidence.bareNoStack = true;
            changed = true;
        }
    }
    if (LineLooksLikeTaskPortEvidence(trimmed))
    {
        if (!evidence.taskPortOrTaskgated)
        {
            evidence.taskPortOrTaskgated = true;
            changed = true;
        }
    }
    return changed;
}

GdbMacSigningDiagnosis DiagnoseMacGdbSigning(const wxString &configuredDebuggerPath,
                                             const GdbMacSessionEvidence &session)
{
    GdbMacSigningDiagnosis d;
    d.session = session;
    d.bundledPath = GetMacBundledGdbPath();
    d.configuredPath = NormalizePath(configuredDebuggerPath);

#if !defined(__WXMAC__)
    d.pathKind = GdbMacPathKind::NotOnMac;
    return d;
#else
    if (!platform::macosx)
    {
        d.pathKind = GdbMacPathKind::NotOnMac;
        return d;
    }

    if (d.configuredPath.empty() || !wxFileName::FileExists(d.configuredPath))
    {
        d.pathKind = GdbMacPathKind::Missing;
        return d;
    }

    if (!IsMacBundledGdbPath(d.configuredPath))
    {
        d.pathKind = GdbMacPathKind::NotBundled;
        return d;
    }

    d.pathKind = GdbMacPathKind::Bundled;
    FillCodesignFields(d.configuredPath, d);
    return d;
#endif
}

wxString FormatGdbMacSigningDiagnosisForLog(const GdbMacSigningDiagnosis &d)
{
    wxString s;
    s << "macOS GDB signing diagnosis (v2.3 M2; evidence only, no repair):\n";
    s << "  configured: " << (d.configuredPath.empty() ? wxString("<empty>") : d.configuredPath) << '\n';
    s << "  bundled:    " << (d.bundledPath.empty() ? wxString("<n/a>") : d.bundledPath) << '\n';

    switch (d.pathKind)
    {
        case GdbMacPathKind::NotOnMac:
            s << "  path: not macOS\n";
            break;
        case GdbMacPathKind::Missing:
            s << "  path: missing / not found (no Enable debugging offer)\n";
            break;
        case GdbMacPathKind::NotBundled:
            s << "  path: not the bundled Resources/debugger/bin/gdb (helper will refuse)\n";
            break;
        case GdbMacPathKind::Bundled:
            s << "  path: bundled\n";
            break;
    }

    switch (d.signature)
    {
        case GdbMacSignatureState::NotChecked:
            s << "  signature: (not checked)\n";
            break;
        case GdbMacSignatureState::MissingOrInvalid:
            s << "  signature: missing or invalid\n";
            break;
        case GdbMacSignatureState::ValidAdhoc:
            s << "  signature: valid ad-hoc\n";
            break;
        case GdbMacSignatureState::ValidNamed:
            s << "  signature: valid named identity\n";
            break;
    }

    if (d.pathKind == GdbMacPathKind::Bundled && d.signature != GdbMacSignatureState::NotChecked
        && d.signature != GdbMacSignatureState::MissingOrInvalid)
    {
        s << "  entitlement com.apple.security.cs.debugger: "
          << (d.hasDebuggerEntitlement ? "present" : "ABSENT") << '\n';
    }

    s << "  quarantine xattr: " << (d.quarantinePresent ? "present (noted separately; not debugger repair)"
                                                        : "absent or unset") << '\n';
    s << "  session: bare No stack=" << (d.session.bareNoStack ? "yes" : "no")
      << "; task-port/taskgated=" << (d.session.taskPortOrTaskgated ? "yes" : "no")
      << "; launch failed=" << (d.session.launchFailed ? "yes" : "no") << '\n';
    s << "  signing defective (Tier 1 candidate): " << (d.SigningDefective() ? "yes" : "no") << '\n';
    s << "  would offer Enable debugging (M5 later): "
      << (d.WouldOfferEnableDebugging() ? "yes" : "no");
    if (d.session.bareNoStack && !d.WouldOfferEnableDebugging() && !d.SigningDefective())
        s << "\n  note: bare \"No stack\" alone is not enough without signing or task-port evidence";
    return s;
}

GdbMacTier1Decision DecideMacGdbTier1(const GdbMacSigningDiagnosis &d)
{
    switch (d.pathKind)
    {
        case GdbMacPathKind::NotOnMac:
            return GdbMacTier1Decision::RefuseNotOnMac;
        case GdbMacPathKind::Missing:
            return GdbMacTier1Decision::RefuseMissing;
        case GdbMacPathKind::NotBundled:
            return GdbMacTier1Decision::RefuseNotBundled;
        case GdbMacPathKind::Bundled:
            break;
    }
    if (!d.SigningDefective())
        return GdbMacTier1Decision::SkipAlreadyValid;
    return GdbMacTier1Decision::ShouldRestore;
}

wxString GetMacBundledGdbEntitlementsPath()
{
#ifdef __WXMAC__
    // Contents/Resources/share/codeblocks → ../../debugger/gdb.entitlements
    wxFileName ents(ConfigManager::GetDataFolder() + "/../../debugger/gdb.entitlements");
    ents.Normalize(wxPATH_NORM_DOTS | wxPATH_NORM_ABSOLUTE);
    if (ents.FileExists())
        return ents.GetFullPath();
    return wxString();
#else
    return wxString();
#endif
}

GdbMacTier1Result TryMacGdbTier1Restore(const wxString &configuredDebuggerPath)
{
    GdbMacTier1Result r;
    r.before = DiagnoseMacGdbSigning(configuredDebuggerPath, GdbMacSessionEvidence());
    const GdbMacTier1Decision decision = DecideMacGdbTier1(r.before);

    switch (decision)
    {
        case GdbMacTier1Decision::RefuseNotOnMac:
            r.kind = GdbMacTier1ResultKind::Refused;
            r.message = "Tier 1 refused: not macOS";
            return r;
        case GdbMacTier1Decision::RefuseMissing:
            r.kind = GdbMacTier1ResultKind::Refused;
            r.message = "Tier 1 refused: debugger executable missing";
            return r;
        case GdbMacTier1Decision::RefuseNotBundled:
            r.kind = GdbMacTier1ResultKind::Refused;
            r.message = "Tier 1 refused: not the bundled Resources/debugger/bin/gdb";
            return r;
        case GdbMacTier1Decision::SkipAlreadyValid:
            r.kind = GdbMacTier1ResultKind::SkippedAlreadyValid;
            r.after = r.before;
            r.message = "Tier 1 skipped: signature and cs.debugger already valid (no resign)";
            return r;
        case GdbMacTier1Decision::ShouldRestore:
            break;
    }

#if !defined(__WXMAC__)
    r.kind = GdbMacTier1ResultKind::Refused;
    r.message = "Tier 1 refused: not built for macOS";
    return r;
#else
    const wxString ents = GetMacBundledGdbEntitlementsPath();
    if (ents.empty())
    {
        r.kind = GdbMacTier1ResultKind::Failed;
        r.message = "Tier 1 failed: bundled gdb.entitlements not found under Resources/debugger/";
        return r;
    }

    const wxString gdbPath = r.before.configuredPath;
    const wxString qGdb = QuoteForShell(gdbPath);
    const wxString qEnts = QuoteForShell(ents);

    // Re-sign companion dylibs ad-hoc (no debugger entitlement required).
    wxFileName libDirFn(gdbPath);
    const wxString libDir = libDirFn.GetPath() + wxFileName::GetPathSeparator() + ".."
                          + wxFileName::GetPathSeparator() + "lib";
    wxFileName libNorm(libDir);
    libNorm.Normalize(wxPATH_NORM_DOTS | wxPATH_NORM_ABSOLUTE);
    if (wxDir::Exists(libNorm.GetFullPath()))
    {
        wxDir dir(libNorm.GetFullPath());
        wxString name;
        bool cont = dir.GetFirst(&name, "*.dylib", wxDIR_FILES);
        while (cont)
        {
            const wxString libPath = libNorm.GetFullPath() + wxFileName::GetPathSeparator() + name;
            int libRc = -1;
            RunCapture("codesign --force --sign - " + QuoteForShell(libPath), &libRc);
            if (libRc != 0)
            {
                r.kind = GdbMacTier1ResultKind::Failed;
                r.message = wxString::Format("Tier 1 failed: codesign dylib %s (exit %d)", name, libRc);
                return r;
            }
            cont = dir.GetNext(&name);
        }
    }

    int signRc = -1;
    RunCapture("codesign --force --sign - --entitlements " + qEnts + " " + qGdb, &signRc);
    if (signRc != 0)
    {
        r.kind = GdbMacTier1ResultKind::Failed;
        r.message = wxString::Format("Tier 1 failed: codesign gdb (exit %d)", signRc);
        r.after = DiagnoseMacGdbSigning(gdbPath, GdbMacSessionEvidence());
        return r;
    }

    r.after = DiagnoseMacGdbSigning(gdbPath, GdbMacSessionEvidence());
    if (r.after.SigningDefective() || !r.after.hasDebuggerEntitlement
        || r.after.signature == GdbMacSignatureState::MissingOrInvalid)
    {
        r.kind = GdbMacTier1ResultKind::Failed;
        r.message = "Tier 1 failed: post-sign verify still defective";
        return r;
    }

    r.kind = GdbMacTier1ResultKind::Restored;
    r.message = "Tier 1 restored: ad-hoc signature + com.apple.security.cs.debugger";
    return r;
#endif
}

wxString FormatGdbMacTier1ResultForLog(const GdbMacTier1Result &r)
{
    wxString s;
    s << "macOS GDB Tier 1 restore (v2.3 M3):\n";
    s << "  result: ";
    switch (r.kind)
    {
        case GdbMacTier1ResultKind::Refused:
            s << "refused";
            break;
        case GdbMacTier1ResultKind::SkippedAlreadyValid:
            s << "skipped (already valid)";
            break;
        case GdbMacTier1ResultKind::Restored:
            s << "restored";
            break;
        case GdbMacTier1ResultKind::Failed:
            s << "failed";
            break;
    }
    s << '\n';
    s << "  " << r.message;
    return s;
}
