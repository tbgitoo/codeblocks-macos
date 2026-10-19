/*
 * This file is part of the Code::Blocks IDE and licensed under the GNU General Public License, version 3
 * http://www.gnu.org/licenses/gpl-3.0.html
 *
 * v2.3 M2: macOS bundled-GDB signing / task-port diagnosis (evidence only; no repair).
 */

#ifndef GDBMACOSSIGNINGDIAGNOSE_H
#define GDBMACOSSIGNINGDIAGNOSE_H

#include <wx/string.h>

/** Session-side evidence from GDB output / launch (unit-testable classifiers). */
struct GdbMacSessionEvidence
{
    bool bareNoStack = false;          ///< line starts with "No stack"
    bool taskPortOrTaskgated = false;  ///< Mach task port / taskgated (confirmed auth evidence)
    bool launchFailed = false;         ///< GDB process failed to start / early death
};

/** How the configured debugger path relates to the Code::Blocks bundle. */
enum class GdbMacPathKind
{
    NotOnMac,      ///< compile-time / runtime N/A
    Missing,       ///< path empty or file does not exist
    NotBundled,    ///< exists but is not Resources/debugger/bin/gdb
    Bundled        ///< matches nested bundled gdb
};

enum class GdbMacSignatureState
{
    NotChecked,
    MissingOrInvalid, ///< codesign --verify failed
    ValidAdhoc,       ///< verify OK, Signature=adhoc
    ValidNamed        ///< verify OK, named identity
};

struct GdbMacSigningDiagnosis
{
    GdbMacPathKind pathKind = GdbMacPathKind::NotOnMac;
    GdbMacSignatureState signature = GdbMacSignatureState::NotChecked;
    wxString configuredPath;
    wxString bundledPath;
    bool hasDebuggerEntitlement = false; ///< com.apple.security.cs.debugger (when signed)
    bool quarantinePresent = false;      ///< com.apple.quarantine — noted separately, not repair evidence
    GdbMacSessionEvidence session;

    /** Signature missing/invalid and/or debugger entitlement absent (Tier 1 candidate). */
    bool SigningDefective() const;

    /**
     * Evidence that would justify offering Enable debugging later (M5).
     * Requires bundled path + (signing defect OR task-port evidence).
     * Bare "No stack" alone is never enough. Missing / non-bundled → false.
     */
    bool WouldOfferEnableDebugging() const;
};

/** Contents/Resources/debugger/bin/gdb from the running app (empty if unavailable). */
wxString GetMacBundledGdbPath();

/** True if @a path normalizes to the bundled gdb path. */
bool IsMacBundledGdbPath(const wxString &path);

/**
 * Classify one GDB log / output line into session evidence flags (OR into @a evidence).
 * Returns true if any flag was set from this line.
 */
bool ClassifyGdbMacSessionLine(const wxString &line, GdbMacSessionEvidence &evidence);

/**
 * Diagnose configured debugger path + session evidence.
 * On macOS may shell out to codesign / xattr. Never mutates signing state.
 */
GdbMacSigningDiagnosis DiagnoseMacGdbSigning(const wxString &configuredDebuggerPath,
                                             const GdbMacSessionEvidence &session);

/** One multi-line human-readable summary for the debugger log. */
wxString FormatGdbMacSigningDiagnosisForLog(const GdbMacSigningDiagnosis &d);

#endif // GDBMACOSSIGNINGDIAGNOSE_H
