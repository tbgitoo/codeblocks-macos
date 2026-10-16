/*
 * Program Console (v2.0) — macOS PTY transport + Run session facade.
 * Linux/Windows: stubs only (PreferProgramConsole() is false).
 */
#ifndef PROGRAMCONSOLE_H
#define PROGRAMCONSOLE_H

#include "settings.h"
#include <wx/string.h>

class wxWindow;
class wxEvtHandler;

namespace ProgramConsole
{
    /** True on macOS when /console_output_mode is program_console (default). */
    DLLIMPORT bool PreferProgramConsole();

    /**
     * Program Console keyboard-focus policy (macOS + program_console only).
     * Config: /program_console_focus_run — on_launch (default) | never | on_silence
     * Config: /program_console_focus_debug — on_launch | never (default) | on_step_silence
     */
    DLLIMPORT wxString GetFocusPolicyRun();
    DLLIMPORT wxString GetFocusPolicyDebug();

    /** Create/register the InfoPane page (macOS only; no-op elsewhere). */
    DLLIMPORT void RegisterPanel(wxWindow* infoPaneParent);

    /** Show the Program Console tab if present (does not apply focus policy). */
    DLLIMPORT void ShowPanel();

    /** Show the tab and move keyboard focus into the console (on_launch policy). */
    DLLIMPORT void FocusPanel();

    /**
     * Begin a Run session: allocate PTY (master + held slave).
     * Does NOT clear scrollback. On failure, prior scrollback is retained.
     */
    DLLIMPORT bool BeginRunSession();

    /**
     * Mark the Run session active after successful child launch:
     * clear scrollback and switch to the Program Console tab.
     */
    DLLIMPORT void ActivateRunSession();

    /** Tear down a session that never became active (launch failed). */
    DLLIMPORT void AbortRunSession();

    /** Tear down after the Run process has exited (explicit Close). */
    DLLIMPORT void EndRunSession();

    /**
     * Append the cb_console_runner-style completion line to the Program Console
     * ("Process returned N (0xN)   execution time : X.XXX s"). No "press any key".
     */
    DLLIMPORT void AnnounceRunFinished(int exitCode);

    /**
     * Debug inferior finished: exit code only
     * ("Process returned N (0xN)"). No execution time (breakpoints inflate wall clock).
     */
    DLLIMPORT void AnnounceDebugFinished(int exitCode);

    /** True while a Run or Debug session is active on the Program Console PTY. */
    DLLIMPORT bool IsRunSessionActive();

    /**
     * True when the Program Console may steal keyboard focus for cin.
     * Run: whenever the session is active. Debug: only while the inferior is
     * running (not stopped at a breakpoint) — so focus waits until cin actually blocks.
     */
    DLLIMPORT bool AllowInputFocusSteal();

    /**
     * Debug only: inferior continued (true) or stopped (false).
     * Continue arms the input-focus timer; stop cancels it.
     */
    DLLIMPORT void NotifyDebugInferiorState(bool running);

    /**
     * Begin a Debug session: allocate PTY (master + held slave).
     * Does NOT clear scrollback. On success, @a outSlavePath receives the
     * slave pathname for GDB `tty <path>` (claim ON — do not claim-off).
     */
    DLLIMPORT bool BeginDebugSession(wxString& outSlavePath);

    /**
     * Mark the Debug session active after PTY allocation succeeded:
     * clear scrollback and switch to the Program Console tab.
     */
    DLLIMPORT void ActivateDebugSession();

    /** Tear down a Debug session that never became active. */
    DLLIMPORT void AbortDebugSession();

    /** Explicit Close when the debug session / inferior console ends. */
    DLLIMPORT void EndDebugSession();

    /**
     * Fork/exec command on the current session slave (M1 semantics).
     * Returns child pid > 0 on success.
     */
    DLLIMPORT long LaunchOnPty(const wxString& command, const wxString& workingDir);

    /**
     * Create a wxProcess-compatible tracker that waitpid-polls @a pid and
     * posts cbEVT_PIPEDPROCESS_TERMINATED to @a parent (same as PipedProcess).
     * Caller stores the pointer in CompilerProcess::pProcess.
     */
    DLLIMPORT class wxProcess* CreateRunProcessTracker(wxProcess** pvThis,
                                                       wxEvtHandler* parent,
                                                       int id,
                                                       int index,
                                                       long pid);
} // namespace ProgramConsole

#endif // PROGRAMCONSOLE_H
