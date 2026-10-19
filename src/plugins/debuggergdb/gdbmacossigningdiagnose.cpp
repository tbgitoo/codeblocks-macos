/*
 * This file is part of the Code::Blocks IDE and licensed under the GNU General Public License, version 3
 * http://www.gnu.org/licenses/gpl-3.0.html
 *
 * v2.3 M2: macOS bundled-GDB signing / task-port diagnosis (evidence only; no repair).
 */

#include "sdk.h"
#ifndef CB_PRECOMP
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
