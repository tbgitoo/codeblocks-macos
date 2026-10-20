/*
 * This file is part of the Code::Blocks IDE and licensed under the GNU General Public License, version 3
 * http://www.gnu.org/licenses/gpl-3.0.html
 *
 * v2.3 M6: macOS bundled-GDB provisioning. Declarations exist only on macOS.
 * The component reports what a mutation did. It does not decide product policy.
 */

#ifndef GDBMACPROVISION_H
#define GDBMACPROVISION_H

#if defined(__WXMAC__)

#include <wx/string.h>

/** Mechanism-level outcome of one provisioning call. Not a product-policy decision. */
enum class GdbMacProvisionOutcome
{
    AlreadyHealthy,   ///< nothing mutated
    Tier1Restored,    ///< ad-hoc signature + cs.debugger restored; Tier 2 not applied
    Tier2Provisioned, ///< bundled gdb signed with codeblocks-gdb
    Refused,          ///< containment or missing bundled payload
    Cancelled,        ///< trust panel dismissed (OSStatus -60006 or -128)
    Failed
};

struct GdbMacProvisionResult
{
    GdbMacProvisionOutcome outcome = GdbMacProvisionOutcome::Failed;
    bool tier2Attempted = false;
    wxString log; ///< debugger-log text; may be empty
};

/**
 * Provision the bundled debugger only.
 * @a runTier2 is the caller's decision that Tier 2 is indicated (task-port evidence).
 * @a forceTier2 resigns with the named identity even when that signature is already present.
 * No path, keychain, or identity arguments. Session evidence is not accepted.
 */
GdbMacProvisionResult ProvisionMacBundledGdb(bool runTier2, bool forceTier2);

#endif // __WXMAC__

#endif // GDBMACPROVISION_H
