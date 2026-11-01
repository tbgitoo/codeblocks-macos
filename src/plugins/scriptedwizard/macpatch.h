/*
 * This file is part of the Code::Blocks IDE and licensed under the GNU General Public License, version 3
 * http://www.gnu.org/licenses/gpl-3.0.html
 *
 * v3.8.1 M5: probe and apply one known compatibility patch to an
 * explicitly supported source checkout using Git.
 *
 * This is deliberately not a general patch or Git command API.
 * The helper accepts only known patch identifiers and never accepts
 * arbitrary patch paths, patch contents, target files or Git commands
 * from wizard.script.
 */

#ifndef MACPATCH_H
#define MACPATCH_H

#include <wx/string.h>

#if defined(__WXMAC__)

// State of one known compatibility patch against one validated source tree.
enum MacPatchState
{
    MacPatchUnknown = 0,
    MacPatchApplicable = 1,
    MacPatchAlreadyApplied = 2,
    MacPatchUnexpectedSource = 3,
    MacPatchUnsupportedVersion = 4,
    MacPatchGitUnavailable = 5
};

struct MacPatchProbe
{
    int state;

    wxString patchId;
    wxString sourceRoot;
    wxString targetFile;
    wxString gitExecutable;
    wxString versionOutput;

    wxString applyCheckOutput;
    wxString reverseCheckOutput;
    wxString errorOutput;

    long applyCheckExit;
    long reverseCheckExit;

    MacPatchProbe()
        : state(MacPatchUnknown),
          applyCheckExit(-1),
          reverseCheckExit(-1)
    {}
};

// Probe one known compatibility patch.
//
// patchId must identify a patch compiled into the native helper.
// sourceRoot must be the expected absolute source-tree root.
//
// The implementation validates the Git executable, supported source
// version and known patch before performing the equivalents of:
//
//     git -C <sourceRoot> apply --check <knownPatch>
//     git -C <sourceRoot> apply --reverse --check <knownPatch>
//
// State classification:
//
//     apply --check succeeds
//         -> MacPatchApplicable
//
//     otherwise, reverse --check succeeds
//         -> MacPatchAlreadyApplied
//
//     neither succeeds
//         -> MacPatchUnexpectedSource
//
// Untracked build output such as build-codeblocks/ does not by itself
// make an otherwise supported tracked source tree unexpected.
//
// This function never modifies the source tree.
MacPatchProbe ProbeMacPatch(
    const wxString& patchId,
    const wxString& sourceRoot);

// Report string returned to wizard.script.
wxString FormatMacPatchProbe(const MacPatchProbe& probe);


// Result of one explicitly approved known compatibility patch.
struct MacPatchApplyResult
{
    bool launched;
    bool cancelled;
    long exitCode;
    bool success;

    int initialState;
    int finalState;

    wxString patchId;
    wxString sourceRoot;
    wxString targetFile;
    wxString gitExecutable;
    wxString output;
    wxString errorOutput;

    MacPatchApplyResult()
        : launched(false),
          cancelled(false),
          exitCode(-1),
          success(false),
          initialState(MacPatchUnknown),
          finalState(MacPatchUnknown)
    {}
};

// Consent first.
//
// If the known patch is applicable, performs the equivalent of:
//
//     git -C <sourceRoot> apply <knownPatch>
//
// The implementation must:
//   - recognise only known patch identifiers;
//   - resolve the patch resource internally;
//   - revalidate Git before use;
//   - validate the supported source checkout;
//   - probe patch state before modification;
//   - require explicit consent before application;
//   - construct argv internally;
//   - never invoke a shell command supplied by wizard.script;
//   - never accept an arbitrary patch path or patch contents;
//   - never run reset, checkout, clean, commit or other destructive recovery;
//   - verify successful application using reverse --check.
//
// MacPatchAlreadyApplied is a successful no-op.
// MacPatchUnexpectedSource and unsupported source states must fail before
// modification.
//
// Cancel before launch does not start Git.
// Success after application requires the post-apply probe to report
// MacPatchAlreadyApplied.
MacPatchApplyResult ApplyMacPatch(
    const wxString& patchId,
    const wxString& sourceRoot,
    const wxString& explanation,
    const wxString& confirmLabel);

// Report string returned to wizard.script.
wxString FormatMacPatchApply(const MacPatchApplyResult& result);

#endif // defined(__WXMAC__)

#endif // MACPATCH_H
