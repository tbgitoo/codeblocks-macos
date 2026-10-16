# Code::Blocks macOS — versioning plan

Roadmap for the unofficial Apple Silicon port in this repository. Versions are sequential: finish and test each before starting the next major step in the console/debugger line.

```text
  → v1.0 (native macOS port + packaging)
  → v1.1 (Terminal console polish + Darwin inferior-tty-claim)
  → v1.2 (bundled Darwin GDB in the .app)
  → v2.0 (integrated Program Console / PTY pane)
```

Upstream baseline: Code::Blocks trunk **r14050** / IDE **25.03**. Companion Darwin GDB lives in the sister repo **`binutils-gdb`** (sibling checkout, e.g. `../binutils-gdb`) — never hardcode a developer home path.

---

## Shared context (console / debugger)

On macOS, Run and Debug use **Terminal.app** through v1.x:

- **Run:** `osascript` + optional `cb_console_runner`.
- **Debug:** Terminal + holder (`cb_debug_console_hold` from v1.1; earlier `sleep`) → discover `/dev/ttys…` → GDB `tty …`.

Darwin refuses GDB’s `TIOCSCTTY` on a Terminal-owned pty (long warning). I/O still works; job control stays with Terminal. Windows `new-console` is not portable here.

GDB is not generally on PATH on macOS. Through v1.1 the user must install/configure it (typically the companion port). **v1.2** ships that GDB inside `CodeBlocks.app` as the macOS default before **v2.0** reworks the PTY pathway.

---

## v1.0 — Native macOS port and first shippable app

**Status: released** (git tag **`v1.0`** at commit **`e5bc580`**).

**Span:** initial import **`2408e77`** (*Import Code::Blocks trunk r14050*) through **`e5bc580`** (*gitignore dmg*).

### Goals

Deliver a usable unofficial **native arm64** Code::Blocks that builds against Homebrew wxWidgets, runs as a self-contained **`CodeBlocks.app`**, and can be distributed as a **`.dmg`**. Establish the Terminal.app-based Run/Debug/Tools path that later versions refine. GDB is **not** bundled; users configure an external debugger (companion `binutils-gdb` or other).

### Essential developments

1. **Upstream import** (`2408e77`)  
   Unmodified SVN export of Code::Blocks trunk r14050 as the git starting point.

2. **Build hygiene** (`7c6fbb3`, later `.gitignore` updates, `e5bc580`)  
   Ignore local configure/build artifacts and generated `.dmg` files so the tree stays clean for packaging work.

3. **macOS Terminal / osascript correctness** (`8edc3ee`, `75011cc`)  
   - Debugger console: ensure the `sleep` (holder) command is actually executed by Terminal’s `do script` (`$SCRIPT` substitution for osascript).  
   - Tools: same `$SCRIPT` embedding for tool commands, plus AppleScript-safe quote escaping.  
   Without this, Run/Debug/Tools payloads were not reliably started inside Terminal.app.

4. **App bundle packaging** (`ba5418c`)  
   Add `make_cb_app_bundle.sh`: stage from `_cb_install`, assemble `/Applications/CodeBlocks.app`, copy frameworks/plugins, rewrite install names / rpaths, ad-hoc codesign. Initial macOS README for build and install.

5. **Documentation and bootstrap** (`79637e5`, `78e0dbc`)  
   Expand macOS / compilers docs (`README.macos.md`, `COMPILERS`). Make `bootstrap` locate Homebrew `wxwin.m4` so Autotools configure works on a typical Apple Silicon Homebrew setup.

6. **DMG distribution** (`75a711b`, `e5bc580`)  
   Add `make_cb_dmg.sh` to wrap the finished `.app` in a drag-to-Applications disk image; ignore built DMGs in git.

### Non-goals (v1.0)

- Bundled GDB or default debugger path inside the app.  
- Integrated Program Console / PTY pane.  
- Silencing the Darwin `TIOCSCTTY` warning (`inferior-tty-claim`).  
- Replacing `~/.codeblocks_run.sh` / `sleep` with dedicated holders (later v1.1).

### Outcome

A documented build → `_cb_install` → `CodeBlocks.app` → `.dmg` pipeline, with Terminal-based console I/O usable for Run, Debug, and Tools once the user supplies a working GDB.

---

## v1.1 — Terminal console polish + Darwin inferior-tty-claim

**Status: released** (git tag **`v1.1`** at commit **`cf3e4f0`**).

**Span:** after **`e5bc580`** (v1.0) through **`cf3e4f0`** (*macos readme*). Companion GDB work for `inferior-tty-claim` lives in sister **`binutils-gdb`** (not this tag’s commit list).

### Goals

Keep Terminal.app-based Run/Debug, but stop the long Darwin “Failed to set controlling terminal” warning when GDB attaches to a busy Terminal pty, and make the macOS console helpers reliable without a home-directory wrapper. No integrated PTY pane (that is v2.0).

### Essential developments

1. **Planning** (`9b031d4`)  
   Capture the console/debugger roadmap (then `plans/console_pane.md`): v1.1 Terminal + claim-off vs later integrated pane.

2. **CB ↔ GDB no-claim handshake** (`728c36c`)  
   On macOS, after opening the debug console tty, probe `show inferior-tty-claim` and, if supported, queue `set inferior-tty-claim off` before `tty …`. Stock GDB without the setting is left alone. Depends on the companion Darwin GDB port exposing that boolean (skip `TIOCSCTTY` / Darwin `tcsetpgrp` when off).

3. **Console holders, single Terminal window, polish** (`155b2ee`)  
   - Add `cb_debug_console_hold` and wire `RunNixConsole` to the bundled holder (replace `sleep` / `~/.codeblocks_run.sh` dependency).  
   - Soft finish (SIGUSR1) vs force kill (SIGTERM) so Code::Blocks does not block on “press any key”.  
   - Default Terminal `osascript`: `clear; $SCRIPT; exit`, and cold-start reuse of window 1 so Run/Debug do not open a second empty Terminal.  
   - Bundle the new helper from `make_cb_app_bundle.sh`; adjust `cb_console_runner` as needed.

4. **v1.2 planning note** (`7d33e7e`)  
   Extend the plan for bundling Darwin GDB next (does not change v1.1 runtime behaviour).

5. **macOS README** (`cf3e4f0`)  
   Document Terminal helpers, Darwin claim warning, and the `inferior-tty-claim` handshake for users of the companion GDB.

### Non-goals (v1.1)

- Bundled GDB or default debugger path inside the app (v1.2).  
- Integrated Program Console / PTY ownership (v2.0).  
- Changing Linux/Windows debugger console paths.

### Outcome

Debug sessions on Terminal no longer print the long Darwin claim warning when using a matching companion GDB; Run/Debug use relocatable bundled console helpers; cold-start Terminal opens a single window.

---

## v1.2 — Bundle Darwin GDB in CodeBlocks.app (before PTY rework)

**Status: released** (ends at commit **`80d8e89`**, as tag **`v1.2`**).

**Span:** after **`cf3e4f0`** (v1.1) through **`80d8e89`** (*readme update*).

**Bundled debugger:** companion [binutils-gdb](https://github.com/tbgitoo/binutils-gdb) at commit **`5564fbe`** (staging `bin/gdb` + `share/gdb/` + runtime dylibs copied into `CodeBlocks.app` at package time).

### Goals

Ship the companion Darwin GDB inside `CodeBlocks.app` and make it the macOS default debugger path so users need not install GDB separately—before any integrated PTY work in v2.0. Linux/Windows debugger defaults unchanged. No automatic config migration.

### Essential developments

1. **Bundle GDB into the app** (`b096b85`)  
   Extend `make_cb_app_bundle.sh` to copy `../binutils-gdb/staging` into `Contents/Resources/debugger/` (`bin/gdb`, `share/gdb/`, vendored `lib/` dylibs + `licenses/`). Prefer bundled path when debugger `executable_path` is empty on macOS. Remove Terminal/`~/.codeblocks_run.sh` migration from `app.cpp`.

2. **Default UX and debugger entitlement** (`427cbea`)  
   Settings UI: theme-safe path colours, **Bundled** button, file picker opens inside `Resources/debugger/bin`. Sign nested `gdb` with `com.apple.security.cs.debugger` (`osx/gdb.entitlements`) so macOS allows the Mach task port (avoids “No stack” / taskgated failure after ad-hoc re-sign).

3. **Code-signing guide** (`edc8857`)  
   Add [`code_signing.md`](../code_signing.md) (symptom → certificate → `codesign` commands); rename top-level README to `README.md`; link from macOS docs.

4. **Top-level README** (`80d8e89`)  
   Point users at downloads, `README.macos.md`, and official upstream for non-macOS platforms.

### Non-goals (v1.2)

- Integrated Program Console / PTY pane (v2.0).  
- Bundling GDB for Linux/Windows.  
- Committing the GDB binary into this git history (copied from sister staging at bundle time).  
- Auto-migrating existing debugger or Terminal settings.

### Outcome

A fresh macOS install includes the matching GDB under `Resources/debugger/` and selects it by default. The bundled GDB carries the debugger entitlement; depending on macOS security settings, local trusted signing may still be required as documented in code_signing.md.

---

## v2.0 — Integrated Program Console (pending implementation)

**Status: Planned** (product decisions locked; repository hooks verified below; **not implemented**).

### Goals (unchanged)

- Code::Blocks owns the **PTY master**; the program / inferior uses the **slave**.  
- Run and Debug share one **Program Console** UI and one PTY/session service.  
- Debugger command/diagnostic output stays in the existing debugger log; inferior stdio in the Program Console.  
- Pane inactive (no PTY) until a console Run or Debug needs one; GUI targets unchanged.  
- **Default on macOS:** Program Console. **User option:** keep v1.x Terminal.app.  
- **Windows and Linux:** unchanged; no open-loop edits to those console paths.  
- GDB only (bundled Darwin GDB from v1.2); no LLDB.  
- Console contract: line-oriented I/O, prompts without trailing newline, PTY echo (no local echo of printables), Ctrl-C, Ctrl-D/EOF, CR handling, resize, scrollback, UTF-8, **limited ANSI colours/styles**. Not full VT100/curses.

### Product decisions (locked)

| Topic | Decision |
|-------|----------|
| UI | New **tab in Logs & others** (`InfoPane`); not a separate AUI pane. |
| Platform | New code / defaults **macOS-only** (`platform::macosx` / `__WXMAC__`). |
| Order | **1** standalone PTY proof → **2** GDB→PTY proof → **3** Run→pane → **4** Debug→pane → **5** preference / lifecycle / docs. |
| Terminal.app | Normal selectable mode; default = Program Console. |
| Scrollback | Keep previous output while a replacement session is created/launched. **Clear only when** the replacement Run process or Debug inferior/session has **successfully launched** and the new session is **active**. Do **not** clear on PTY-create alone. On any failure (PTY, exec, GDB setup, inferior launch), **retain** previous scrollback and report the error separately. |
| Debuggers | GDB only. |

### Repository findings (verified)

#### A. Run path (macOS today)

1. **Trace:** `CompilerGCC::OnRun` / `Run(target)` → builds a shell command → `m_CommandQueue.Add(CompilerCommand(..., isRun=true))` → `DoRunQueue` → `PipedProcess::Launch`.  
2. **Roles:**  
   - `CompilerGCC` owns the run queue, working directory (`m_CdRun`), and process slot (`CompilerProcess` / PID).  
   - Non-Windows console targets prepend `ConfigManager("app")->Read("/console_terminal", DEFAULT_CONSOLE_TERM)` (`globals.cpp`: osascript + `clear; $SCRIPT; exit`, cold-start window reuse).  
   - Payload (optional `cb_console_runner` + exe + args) is substituted for `$SCRIPT` (macOS also rewrites quotes for AppleScript).  
   - `PipedProcess` launches that command with `wxEXEC_ASYNC | wxEXEC_SHOW_CONSOLE` and **`pipe = false`** for runs — CB does **not** capture the program’s stdio; it tracks the launcher (osascript) PID.  
   - Completion / stop: existing `PipedProcess` / queue / `Kill` paths; Terminal session is outside CB’s direct control.  
3. **Narrowest Program Console branch:** macOS-only gate inside `CompilerGCC::Run` (when assembling the command) **and/or** `DoRunQueue` when `cmd->isRun`: if Program Console mode and console-like target, **do not** prepend Terminal `osascript`; hand off to a small **macOS launch adapter** that uses the PTY session. Windows/Linux keep the existing `DEFAULT_CONSOLE_TERM` / xterm path.  
4. **Launch capability:** Current run machinery cannot attach a child to CB-owned slave FDs via osascript. A **macOS-specific adapter** (fork/exec or equivalent with slave on 0/1/2, setsid/TIOCSCTTY/pgrp as proven in Milestone 1) is required.  
5. **Process ownership (principle):** `CompilerGCC` remains owner of Run **lifecycle semantics** (queue, cwd, env, Stop, PID/exit reporting, completion). The macOS Run launch adapter performs PTY-aware child launch and feeds PID/exit into that lifecycle. **Reuse `PipedProcess` only if** it can perform the required child-side PTY setup cleanly; do not force reuse of an unsuitable class — preserve behaviour over forcing an abstraction. `PtySession` remains terminal **transport**, not a process manager.

#### B. Debug path (verified)

1. **Trace:** `DebuggerGDB::DoDebug` → if console runner → `RunNixConsole(consoleTty)` (`cbplugin.cpp`) → on success queue GDB tty setup → `Start` / `Attach`.  
2. **Roles:**  
   - `RunNixConsole`: reads `/console_terminal`, substitutes `$SCRIPT` with `MakeDebugConsoleHoldCommand()` → bundled `cb_debug_console_hold <token>`; launches via Terminal; `ps`-discovers holder TTY path + PID.  
   - Holder exists so the slave **stays open** and Terminal owns the session; GDB did not allocate the pty.  
   - `m_nConsolePid` / `KillConsole` / `NotifyDebugConsoleFinished`: SIGTERM (force) vs SIGUSR1 (soft finish) to the **holder**, not a general process manager for the inferior.  
3. **Slave name to GDB:** path string from `RunNixConsole` (e.g. `/dev/ttys…`).  
4. **Command used (do not rename in code):** the driver queues **`tty <path>`** (literal GDB `tty` command). On macOS this is done inside `GdbCmd_SetupMacOSInferiorTty` after probing claim. Linux path queues the same `tty <path>` without the claim probe. The repository does **not** currently use `set inferior-tty` for this attach.  
5. **Claim-off:** `GdbCmd_SetupMacOSInferiorTty` queues `show inferior-tty-claim`, then on success `set inferior-tty-claim off`, then `tty <path>`, then `Start`/`Attach`. This stays on the **Terminal.app** path unless inspection later moves it.  
6. **Bundled GDB:** `DebuggerConfiguration::GetDebuggerExecutable` — empty config → `…/Resources/debugger/bin/gdb` via `GetDataFolder()/../../debugger/bin/gdb` (`debuggeroptionsdlg.cpp`).  
7. **Narrowest Program Console branch:** when macOS + Program Console mode + console debug, **skip** `RunNixConsole` / holder; `CreateSession` → pass `GetSlaveName()` into the same tty-queue path (reuse or thin variant of `GdbCmd_SetupMacOSInferiorTty`); leave GDB MI/CLI **pipes and debugger log** unchanged.  
8. **Hypothesis:** unclaimed CB-owned slave may allow omitting `set inferior-tty-claim off` — **test in Milestone 2 and re-check in Milestone 4**; do not assume.

#### C. UI integration (verified)

1. **Construction:** `main.cpp` creates `InfoPane` (caption “Logs & others”), registers loggers via `AddLogger`.  
2. **Pattern:** `Logger::CreateControl` → notebook page; clear/copy via InfoPane context menu; tab order persisted.  
3. **Least invasive:** register a dedicated **Program Console** page (custom `wxPanel`, not a classic `Logger` unless that helps lifecycle) beside existing logs; select/show when a session starts.  
4. **Eager vs lazy:** create the tab **eagerly** (empty, no PTY); allocate PTY only on demand. Matches “tab may exist before session”.  
5. **Widget:** do **not** use `TextCtrlLogger` as-is (`wxTextCtrl` **read-only**). Prefer a panel + **`cbStyledTextCtrl` / `wxStyledTextCtrl`** (or writable rich text) for append, styles/colours, monospace, selection/copy, and key handling. No new full terminal emulator widget.  
6. Capabilities to require: incremental append, colour spans (ANSI subset), scrollback trim, key capture, copy, font metrics for resize, CR-in-line update, GUI-thread-only view updates.

#### D. Preference and platform boundaries

1. **UI:** Environment settings (`environmentsettingsdlg.cpp` / `env_settings.xrc`) already hosts `cbConsoleTerm` / `/console_terminal` (disabled on Windows). Add a **macOS-only** choice near it.  
2. **Recommended key:** `app` config `/console_output_mode` = `program_console` | `terminal_app`. **Default:** `program_console` on macOS.  
3. **Wording (draft):** “Console programs:” → “Program Console (Logs & others)” / “Terminal.app”. When Terminal.app is selected, keep using `/console_terminal` as today.  
4. **Safeguard:** all new branches behind `platform::macosx` (and compile-time `__WXMAC__` where files are Darwin-only). No changes to Linux xterm or Windows `AllocConsole` / `new-console` paths. Smoke: Linux/Windows builds must not require new sources.

#### E. Bundle / GDB paths

- Preserve v1.2 resolution (`GetDataFolder()/../../debugger/bin/gdb`). No hard-coded home/staging paths.  
- If bundled GDB missing: existing empty→PATH behaviour / user config; Debug must fail clearly.  
- Entitlement / signing: document only (`code_signing.md`); v2.0 does not redesign signing.

---

### Architecture (implementation contract)

#### Components and ownership

| Component | Owns | Does not own |
|-----------|------|----------------|
| **Program Console session coordinator** (small; owned by Program Console integration) | **Current** monotonic generation; which Run/Debug session is active; rejecting stale events | Master FD; child/GDB processes |
| **PtySession / transport** | Master FD; reader thread/source; events **stamped** with the assigned generation; `SendInput` / `Resize` / `Close`; slave **name** for GDB | Deciding which generation is current; child PID; UI widgets |
| **macOS Run launch adapter** | PTY-aware child launch (setsid/claim/pgrp as proven); integrates PID/exit into `CompilerGCC` lifecycle | Master reads; Terminal.app; being a second process manager |
| **CompilerGCC** (Run) | Queue, cwd, env, Stop, PID/exit reporting, completion semantics | PTY master; forced use of `PipedProcess` if unsuitable |
| **DebuggerGDB** | GDB process, command queue, debugger log, breakpoints | Inferior stdio bytes (PtySession → pane) |
| **Program Console panel** | Display, key→bytes, scrollback buffer | Master FD; process kill; generation allocation |

Conceptual layering:

```text
Program Console UI
        |
session coordinator  — current generation; active session; drop stale events
        |
     PtySession      — PTY master transport; events carry assigned generation
```

**Destruction order (session end / shutdown):** coordinator marks generation inactive → stop reader → ignore further events for that generation → close master (hang-up) → join reader → clear session pointer on UI → (Run) wait/reap via CompilerGCC lifecycle / (Debug) existing GDB teardown. UI must not receive callbacks after destroy (unbind + generation check).

#### Session API (refined)

```text
CreateSession(options) -> SessionHandle   // allocates PTY; coordinator assigns generation
GetSlaveName(handle) -> path              // e.g. /dev/ttysN; for GDB tty command
SendInput(handle, bytes)
Resize(handle, rows, cols)                // Darwin TIOCSWINSZ; optional pixel fields if API allows
Close(handle)                             // lifecycle close; may HUP slave
```

- Do **not** expose raw master FD on the general API.  
- Do **not** grow this into a process manager or large framework — coordinator may be a small object beside the pane.  
- Run adapter and GDB attach are callers, not part of the transport.

#### Lifecycle state machine

`Idle → Starting → Running → Closing → Closed`, plus `Failed` from Starting/Running.

- **Starting:** PTY may exist; launch/GDB setup in progress; **previous scrollback still visible**.  
- **Running (active):** replacement Run process or Debug inferior/session has **successfully launched**; coordinator advances “current” generation for display; **only then** clear old scrollback and show the new session.  
- **Failed** (from Starting or early Running): retain previous scrollback; report failure separately; do not treat PTY-create success as license to clear.  

Every PtySession is stamped with a generation; the **coordinator** owns which generation is current. Queued output/exit/error/completion events carry that id. After the new session becomes **active**, drop events whose generation ≠ current (prevents A-finished → B-started → clear → stale A output).

#### Run-mode Darwin sequence (intended; Milestone 1 proves details)

1. Allocate PTY; parent keeps master, obtains slave path/FD.  
2. Fork; child: `setsid`, open slave, `TIOCSCTTY` if required, set foreground pgrp, dup slave→0/1/2, close extras, exec.  
3. Parent: close slave FD copies; start master reader; track child PID via **CompilerGCC** lifecycle (adapter integrates notification; `PipedProcess` only if suitable).  
4. Exit: detect child exit **and** the observed master-side final-slave-closure condition (EOF, EIO if observed, etc.); then Close session. 
5. Exec failure: report without leaving orphan master/reader; **retain** prior scrollback.

Debug-mode does **not** assume steps 2’s claim/pgrp for the inferior — GDB launches the inferior after `tty <slave>`. Milestone 2 records what Darwin GDB actually does with claim / `inferior-tty-claim`.

#### Input / signals (keep separate)

| Action | Mechanism |
|--------|-----------|
| Focus in Program Console, Ctrl-C | Send the slave’s configured **VINTR** character through the master (normally `0x03` / Ctrl-C); line discipline signals the foreground pgrp. Record termios control chars when the session is configured. **Verify VINTR delivery** in M1 (and re-check in M2). |
| Focus in Program Console, Ctrl-D | Send the slave’s configured **VEOF** character (normally `0x04` / Ctrl-D). **Not** implemented by closing the master. **Verify VEOF** in M1. |
| Printable / Enter / Backspace | Encode to bytes (UTF-8); write to master; **no local echo** of printables if slave ECHO is on — display only master output. |
| Run/Debug **Stop** | Existing CB lifecycle (kill run process / stop debugger); **not** “only write VINTR”. |
| Debugger interrupt | GDB control channel; **not** Program Console Ctrl-C / VINTR. |
| Session **Close** | Lifecycle; may cause hang-up; distinct from VEOF. |

#### Resize

- Derive rows/cols from client area ÷ monospace cell size; clamp to ≥1.  
- `ioctl(master, TIOCSWINSZ, …)`; set pixel sizes if Darwin `winsize` fields are used.  
- Coalesce resize events (timer or “pending” flag).  
- M1 proof: child reads winsize / receives SIGWINCH as applicable.

#### Threading

- **Never** block the GUI thread on master `read`.  
- Reader: dedicated thread or non-blocking FD + existing wake patterns (`PipedProcess` / idle timers / `wxQueueEvent` / `CallAfter`). Prefer one worker reading master, batching bytes, posting **generation-tagged** events to the GUI.  
- Distinguish: normal EOF, the observed Darwin master-side condition after final slave closure (including EIO if observed), genuine errors, hard errors, explicit `Close`.  
- Shutdown: `Close` → interrupt read → join → no UI callbacks into freed panel (`Destroy` unregisters first).  
- Rapid stop/restart: coordinator assigns new generation; clear display only when replacement becomes **active**; stale events ignored.

#### Buffering / UTF-8 / ANSI

- Bound transport queue and visible scrollback (constants in one header; initial suggestion: ~1–4 MiB scrollback, smaller pending queue).  
- Over limit: drop **oldest** scrollback; single marker `[Earlier console output discarded]` (no spam).  
- Incremental UTF-8 and stateful ANSI across reads — required for production (M3+); **not** the focus of Milestone 1.  
- **Supported ANSI (deliberately small):** SGR reset; standard + bright foreground; optional background; optional bold if STC supports it cleanly. Ignore unsupported CSI — **no** cursor addressing / screen buffers.  
- LF, CR, CRLF, repeated CR (progress), BS, TAB: prove CR progress and basic line behaviour in M1; full display policy in pane milestones.

---

### Milestones and acceptance

| # | Milestone | Form | Must prove / deliver |
|---|-----------|------|----------------------|
| **1** | Standalone Darwin PTY proof | Small **repo test utility** (e.g. `src/tools/` or `tests/`); Darwin-only; **not** production UI | Focus on **Darwin/Unix PTY semantics:** allocation; session / controlling-terminal setup; process groups; stdin/stdout/stderr on slave; output with newline; **prompt without newline**; terminal echo (no harness double-echo); **VINTR** (Ctrl-C) to expected pgrp; **VEOF** (Ctrl-D) EOF **without** closing master; record termios VINTR/VEOF; resize / SIGWINCH as applicable; CR progress; child exit / immediate exit; exec failure; close unused FDs; clean reader stop; rapid create/close/recreate without leaks. Defer rich UTF-8/ANSI split-display proofs to later pane work. |
| **2** | Bundled GDB → CB-owned PTY | Minimal harness using v1.2 bundled `gdb` + PtySession; optional tiny console dump | GDB accepts slave via **`tty <path>`** (as in tree); inferior stdio through PTY; break / continue; input when appropriate; Console VINTR vs debugger interrupt distinguishable; document **`inferior-tty-claim`** still needed or not; **no** Terminal/holder unless proof shows Darwin requires it |
| **3** | Run → Program Console tab | Production: InfoPane tab + coordinator + transport + macOS Run adapter | Console Run uses pane when mode=program_console; Terminal path when mode=terminal_app; GUI targets unchanged; **scrollback cleared only after successful Run launch / active session**; failures keep prior output; generation safety via coordinator |
| **4** | Debug → Program Console tab | Production: skip `RunNixConsole` in pane mode; same tab | Inferior I/O in pane; debugger log unchanged; Stop/shutdown safe; **scrollback cleared only after successful Debug inferior/session activation**; failures keep prior output; re-validate claim-off hypothesis; holder unused in pane mode |
| **5** | Preference / polish / docs | Settings UI + `README.macos.md` | Persist `/console_output_mode`; Linux/Windows untouched; UTF-8/ANSI display hardening; edge cases |

**Terminal.app mode** continues to use v1.x osascript + helpers + existing claim-off placement for Debug.

---

### Explicit non-goals (v2.0)

- LLDB; full VT100/curses; Tools-menu in-pane console; multiple concurrent Program Console sessions; redesign of Windows/Linux consoles; changing GDB signing model.

### Hooks (reference)

- Debug (Terminal mode): `RunNixConsole` → `tty <path>` (+ macOS claim probe).  
- Run (Terminal mode): `CompilerGCC` + `/console_terminal` + `$SCRIPT`.  
- GDB binary: `GetDebuggerExecutable` → bundled path when empty.

---

## Status

| Version | Status |
|---------|--------|
| **v1.0** | **Released** — tag `v1.0` @ `e5bc580`. Import r14050 → Terminal `$SCRIPT` fixes → app bundle + DMG + macOS docs/bootstrap. GDB external. |
| **v1.1** | **Released** — tag `v1.1` @ `cf3e4f0`. Claim-off handshake; `cb_debug_console_hold`; cold-start single Terminal window; README. GDB still external. |
| **v1.2** | **Released** — through `80d8e89`. Bundled GDB from [binutils-gdb](https://github.com/tbgitoo/binutils-gdb) **`5564fbe`**; macOS default; debugger entitlement; `code_signing.md`. |
| **v2.0** | **Planned (refined)** — InfoPane Program Console; macOS PTY; findings-backed Run/Debug branches; M1→M2→M3→M4→M5; Terminal option; Win/Linux unchanged. |
