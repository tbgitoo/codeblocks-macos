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

## v2.0 — Integrated Program Console

**Status: In progress** — M1–M4 done; **M5 DONE** (focus policies + docs; not committed unless requested).

```text
  M1 Darwin PTY semantics              PASS
  M2 GDB → CB-owned PTY semantics      PASS  (17/17)
  M3 Run → Program Console             DONE (committed)
  M4 Debug → Program Console           DONE (committed)
  M5 Preference / polish / docs        DONE
```

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
4. **Command used (do not rename in code):** the driver queues **`tty <path>`** (literal GDB `tty` command). On macOS Terminal mode this is done inside `GdbCmd_SetupMacOSInferiorTty` after probing claim. Linux path queues the same `tty <path>` without the claim probe. The repository does **not** currently use `set inferior-tty` for this attach.  
5. **Claim-off (Terminal.app path only):** `GdbCmd_SetupMacOSInferiorTty` queues `show inferior-tty-claim`, then on success `set inferior-tty-claim off`, then `tty <path>`, then `Start`/`Attach`. **Preserve this handshake unchanged for Terminal.app Debug.** Do **not** generalise it to Program Console mode.  
6. **Bundled GDB:** `DebuggerConfiguration::GetDebuggerExecutable` — empty config → `…/Resources/debugger/bin/gdb` via `GetDataFolder()/../../debugger/bin/gdb` (`debuggeroptionsdlg.cpp`).  
7. **Narrowest Program Console branch:** when macOS + Program Console mode + console debug, **skip** `RunNixConsole` / Terminal / holder; `CreateSession` → retain master + one slave FD → pass `GetSlaveName()` into a **Program Console** tty-queue path that issues **`tty <path>` only** (no `set inferior-tty-claim off`); leave GDB MI/CLI **pipes and debugger log** unchanged.  
8. **`inferior-tty-claim` (M2 RESOLVED — production contract):**  
   - **Program Console Debug:** leave claim **ON**; do **not** queue `set inferior-tty-claim off`; queue existing `tty <slave-path>`; allow GDB’s normal tty claim. Required so Program Console **VINTR** delivers SIGINT to the inferior.  
   - **Terminal.app Debug:** keep existing v1.x/v1.2 claim-off handshake (holder already owns ctty).  
   - Claim-off is **not** required for inferior stdio on a CB-owned PTY; claim-on is **required** for correct PTY VINTR → inferior SIGINT.

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
5. **Program Console keyboard-focus policy (M5 — configurable):** see **Focus policy (Run / Debug)** below. macOS Program Console only; not applicable to Linux/Windows external-terminal paths.

#### E. Bundle / GDB paths

- Preserve v1.2 resolution (`GetDataFolder()/../../debugger/bin/gdb`). No hard-coded home/staging paths.  
- If bundled GDB missing: existing empty→PATH behaviour / user config; Debug must fail clearly.  
- Entitlement / signing: document only (`code_signing.md`); v2.0 does not redesign signing.

---

### Architecture (implementation contract)

#### Components and ownership

| Component | Owns | Does not own |
|-----------|------|----------------|
| **Program Console session coordinator** (small; owned by Program Console integration) | **Current** monotonic generation; which Run/Debug session is active; rejecting stale events | Master FD; slave FD; child/GDB processes |
| **PtySession / transport** | Master FD; **one retained slave FD** (PTY lifetime); slave pathname; reader thread/source; events **stamped** with the assigned generation; `SendInput` / `Resize` / `Close` | Run child lifecycle; GDB process; debugger interrupt; UI widgets; which generation is current |
| **macOS Run launch adapter** | PTY-aware child launch (setsid / TIOCSCTTY / pgrp as proven in M1); integrates PID/exit into `CompilerGCC` lifecycle | Master reads; Terminal.app; GDB tty-claim; being a second process manager |
| **CompilerGCC** (Run) | Queue, cwd, env, Stop, PID/exit reporting, completion semantics | PTY master/slave; forced use of `PipedProcess` if unsuitable |
| **DebuggerGDB** | GDB process, command queue, debugger log, breakpoints, **debugger interrupt** (existing driver/control path) | Inferior stdio bytes (PtySession → pane); Program Console Ctrl-C / VINTR |
| **Program Console panel** | Display, key→bytes (incl. VINTR/VEOF), scrollback buffer | Master/slave FDs; process kill; generation allocation |

Conceptual layering (slave hold visible):

```text
                 Program Console
                       |
             session coordinator
                       |
                   PtySession
            master + held slave FD
                  /          \
         macOS Run adapter    GDB tty setup
                |                  |
             program            inferior
```

Shared vs not shared:

- Run and Debug **share** Program Console, coordinator, and PtySession transport.  
- Run and Debug **do not share** process launching.  
- Run adapter launches the ordinary child on the slave (M1 semantics).  
- GDB receives the slave pathname via the existing **`tty <path>`** mechanism.  
- **Program Console Debug** uses **normal GDB tty claim** (claim left **ON**).  
- **Terminal.app Debug** retains the existing **claim-off** path + holder.

#### PtySession slave lifetime (M2 RESOLVED — production requirement)

`PtySession` **must** retain:

- the PTY master FD;  
- **one open slave FD**;  
- the slave pathname.

**Why (M2):** If the final slave FD is closed before GDB attaches, Darwin reports terminal completion on the master and a naive master reader can terminate before GDB opens the slave by pathname.

```text
CreateSession()
    |
    +-- retain master FD
    |
    +-- retain one slave FD
    |
    +-- expose slave pathname to GDB
    |
    +-- GDB: tty <slave-path>
    |
    +-- inferior runs on slave
    |
    +-- Close() performs deterministic teardown
```

No Terminal.app or `cb_debug_console_hold` is required in Program Console mode. The retained slave FD replaces the **PTY-lifetime** function previously supplied by the Terminal/holder arrangement, **not** its process-management behaviour.

**Consequence:** Because PtySession deliberately retains a slave FD, **inferior exit alone may not produce final-slave EOF on the master**. Production must use existing Run/Debug lifecycle notifications **plus explicit `PtySession::Close()`**. Do **not** rely on master EOF alone to detect inferior exit.

#### Session API (refined)

```text
CreateSession(options) -> SessionHandle   // allocates PTY; retains master + one slave FD; coordinator assigns generation
GetSlaveName(handle) -> path              // e.g. /dev/ttysN; for GDB tty command
SendInput(handle, bytes)
Resize(handle, rows, cols)                // Darwin TIOCSWINSZ; optional pixel fields if API allows
Close(handle)                             // deterministic teardown (see below); not “wait for master EOF”
```

- Do **not** expose raw master FD on the general API.  
- Do **not** grow this into a process manager or large framework — coordinator may be a small object beside the pane.  
- Run adapter and GDB attach are callers, not part of the transport.

#### Deterministic Close() (accounts for retained slave FD)

Exact ordering may be adjusted for thread safety; the retained slave FD must have **explicit ownership and deterministic closure**. Do **not** use final-slave EOF as the only process-completion signal.

1. Session coordinator marks the generation inactive / closing.  
2. Existing Run or Debug lifecycle establishes that the process/inferior is finished or being terminated.  
3. Stop / disable further input for the session.  
4. Stop or prepare the master reader for intentional teardown.  
5. Close the retained slave FD.  
6. Close the master FD.  
7. Join / finish the reader.  
8. Reject any queued events from the obsolete generation.  
9. Release the session.

UI must not receive callbacks after destroy (unbind + generation check).

#### Lifecycle state machine

`Idle → Starting → Running → Closing → Closed`, plus `Failed` from Starting/Running.

- **Starting:** PTY may exist; launch/GDB setup in progress; **previous scrollback still visible**.  
- **Running (active):** replacement Run process or Debug inferior/session has **successfully launched**; coordinator advances “current” generation for display; **only then** clear old scrollback and show the new session.  
- **Failed** (from Starting or early Running): retain previous scrollback; report failure separately; do not treat PTY-create success as license to clear.  

Every PtySession is stamped with a generation; the **coordinator** owns which generation is current. Queued output/exit/error/completion events carry that id. After the new session becomes **active**, drop events whose generation ≠ current (prevents A-finished → B-started → clear → stale A output).

#### Run-mode Darwin sequence (M1 proven; M3 implements)

1. Allocate PTY; **retain master and one slave FD**; obtain slave pathname.  
2. Fork; child: `setsid`, open/use slave, `TIOCSCTTY` (required after setsid per M1), set foreground pgrp, dup slave→0/1/2, close extras, exec.  
3. Parent: start master reader; **keep PtySession’s held slave FD open** until `Close()`; track child PID via **CompilerGCC** lifecycle (adapter integrates notification; `PipedProcess` only if suitable).  
4. Exit: detect child exit via **CompilerGCC / adapter lifecycle**; then **explicit `Close()`** (do not wait for master EOF alone — held slave FD may prevent final-slave EOF).  
5. Exec failure: report without leaving orphan master/reader/slave; **retain** prior scrollback.  

Do **not** add GDB-specific tty-claim behaviour to Run.

#### Debug-mode Darwin sequence (M2 proven; M4 implements)

1. Create PtySession (master + held slave FD + pathname).  
2. Queue **`tty <GetSlaveName()>`** only — **do not** queue `set inferior-tty-claim off`.  
3. GDB launches the inferior after `tty`; normal claim remains **ON**.  
4. Inferior stdio flows through PtySession → Program Console.  
5. Inferior exit / debug stop → existing debugger lifecycle → **explicit PtySession teardown**.

#### Input / signals (M1+M2 established)

| Action | Mechanism |
|--------|-----------|
| Focus in Program Console, Ctrl-C | Send configured **VINTR** through the master (normally `0x03`). Line discipline → SIGINT to foreground inferior pgrp. **Established (M2):** works when GDB `inferior-tty-claim` remains **ON**. Remains **PTY input** — **not** a GDB debugger interrupt. |
| Focus in Program Console, Ctrl-D | Send configured **VEOF** (normally `0x04`). **Not** by closing the master. **Verified in M1.** |
| Printable / Enter / Backspace | Encode to bytes (UTF-8); write to master; **no local echo** of printables if slave ECHO is on — display only master output. |
| Run/Debug **Stop** | Existing CB lifecycle (kill run process / stop debugger); **not** “only write VINTR”. |
| Debugger interrupt | **Existing GDB driver/control path** → debugger-controlled stop. **Not** PTY VINTR; **not** a naive sync CLI `interrupt` while `continue` blocks (M2). Keep independent from Program Console input. |
| Session **Close** | Deterministic lifecycle teardown (above); distinct from VEOF. |

Program Console Ctrl-C path (established):

```text
keyboard Ctrl-C
    → PtySession sends configured VINTR
    → PTY master
    → terminal line discipline
    → foreground inferior process group receives SIGINT
```

#### Resize

- Derive rows/cols from client area ÷ monospace cell size; clamp to ≥1.  
- `ioctl(master, TIOCSWINSZ, …)`; set pixel sizes if Darwin `winsize` fields are used.  
- Coalesce resize events (timer or “pending” flag).  
- M1 proof: child reads winsize / receives SIGWINCH as applicable.

#### Threading

- **Never** block the GUI thread on master `read`.  
- Reader: dedicated thread or non-blocking FD + existing wake patterns (`PipedProcess` / idle timers / `wxQueueEvent` / `CallAfter`). Prefer one worker reading master, batching bytes, posting **generation-tagged** events to the GUI.  
- Distinguish master EOF, genuine read errors, and explicit `Close`. Because PtySession retains a slave FD, process completion is determined by the Run/Debug lifecycle rather than master EOF alone.  
- Shutdown: follow deterministic `Close()` (above).  
- Rapid stop/restart: coordinator assigns new generation; clear display only when replacement becomes **active**; stale events ignored.

#### Buffering / UTF-8 / ANSI

- Bound transport queue and visible scrollback (constants in one header; initial suggestion: ~1–4 MiB scrollback, smaller pending queue).  
- Over limit: drop **oldest** scrollback; single marker `[Earlier console output discarded]` (no spam).  
- Incremental UTF-8 and stateful ANSI across reads — required for production (M3+); **not** the focus of Milestone 1.  
- **Supported ANSI (deliberately small):** SGR reset; standard + bright foreground; optional background; optional bold if STC supports it cleanly. Ignore unsupported CSI — **no** cursor addressing / screen buffers.  
- LF, CR, CRLF, repeated CR (progress), BS, TAB: prove CR progress and basic line behaviour in M1; full display policy in pane milestones.

---

### Milestones and acceptance

| # | Status | Milestone | Form / how to run | Must prove / deliver |
|---|--------|-----------|-------------------|----------------------|
| **1** | **PASS** | Standalone Darwin PTY proof | `src/tools/PtyProof/` — `make -C src/tools/PtyProof run` | Darwin/Unix PTY semantics: allocation; setsid+TIOCSCTTY required; process groups; stdio on slave; newline + prompt without newline; terminal echo; **VINTR**; **VEOF** without closing master; termios VINTR/VEOF; resize/SIGWINCH; CR progress; exit paths; exec failure; FD hygiene; reader stop; rapid recreate. Master after final slave close: `read`→0 (EOF). |
| **2** | **PASS (17/17)** | Bundled GDB → CB-owned PTY | `make -C src/tools/PtyProof run-m2` (default GDB `/Applications/CodeBlocks.app/Contents/Resources/debugger/bin/gdb`; override `CB_GDB` / argv). No Terminal / holder. | See **M2 results** below. All prior M2 claim/lifetime questions **RESOLVED**. |
| **3** | **DONE (committed)** | Run → Program Console tab | Production: `ProgramConsole` SDK + InfoPane panel + `CompilerGCC` macOS branch. Config `/console_output_mode` (`program_console` default / `terminal_app`). Settings: Environment → Console programs. | See **M3 results / lessons** below. Console Run uses pane when mode=`program_console`; Terminal when `terminal_app`; GUI targets unchanged; scrollback cleared only after successful Run launch / active session; failures keep prior output; generation safety. M1 Darwin PTY semantics for Run adapter. **No** GDB tty-claim on Run. |
| **4** | **DONE** | Debug → Program Console tab | Production: `BeginDebugSession` / `ActivateDebugSession` / `EndDebugSession` + `DebuggerGDB` branch. | See **M4 requirements** (all met) and **M4 implementation notes** below. Terminal.app mode unchanged (claim-off + holder). |
| **5** | **DONE** | Preference / polish / docs | Settings UI + `README.macos.md` | `/console_output_mode` + **Run/Debug focus policies** (defaults match Win/Linux); Linux/Windows untouched; README documents Program Console; UTF-8/CR/BS noted (full ANSI still out of scope). |

**Terminal.app mode** continues to use v1.x osascript + helpers + existing claim-off placement for Debug.

#### Milestone 3 results / lessons (DONE — committed)

Production Run→Program Console path is in-tree and smoke-tested in the IDE (cin, focus, exit banner). Carry these lessons into M4 and any further Run polish:

| Lesson | Fact / fix |
|--------|------------|
| **`OnIdle` / `PipedProcess` crash (critical)** | `CompilerGCC::OnIdle` casts `CompilerProcess::pProcess` to `PipedProcess*` and calls `HasInput()`. Tracking the Run child with a bare `wxProcess` **crashes on first idle** after launch. **Fix:** `PtyRunProcessTracker` **must derive from `PipedProcess`**; override `HasInput()` to return `false` (stdio is on the PTY, not wx pipes). Do not “simplify” to `wxProcess` alone. |
| Bundle refresh | Hand-copying binaries into `CodeBlocks.app` and/or crude `codesign --deep` can leave an **unlaunchable** app. Refresh via `./make_cb_app_bundle.sh` after SDK/app installs. |
| STC eats Enter | A writable `cbStyledTextCtrl` consumes Return on `KEY_DOWN` before `CHAR`, so digits may PTY-echo while **cin never completes**. **Fix:** keep STC **read-only**; handle Return / Backspace / Ctrl-C / Ctrl-D on `KEY_DOWN`; send **`\\r`** for Enter (ICRNL → NL for the reader). |
| Focus policy (interim M3/M4) | Hard-coded heuristics until M5: Run ≈ silence-after-output; Debug ≈ silence only while inferior running. **Superseded by configurable policies below** — silence cannot distinguish `cin` from a long quiet calculation. |
| Exit banner | Match `cb_console_runner`: `Process returned N (0xN)   execution time : X.XXX s`. Skip “Press any key to continue” — the pane stays open. |
| Closed tab | InfoPane close **Toggle**s the page hidden; restore via tab context **Toggle…** or `ShowNonLogger` on activate/show (not `SetSelection` alone). |
| Child `tcsetpgrp` | Ignore `SIGTTOU` / `SIGTTIN` before becoming foreground pgrp on the slave. |
| Target scope | Program Console Run gate is **target type**, not language: `ttConsoleOnly` or host-app-in-terminal. GUI templates stay unchanged; Fortran/D console apps use the same path as C. |

#### Milestone 2 results (RESOLVED — implementation requirements)

Proof GDB: `/Applications/CodeBlocks.app/Contents/Resources/debugger/bin/gdb`. Inferior I/O over CB-owned PTY **without** Terminal.app or `cb_debug_console_hold`. **17/17 PASS.**

| Result | Fact |
|--------|------|
| Bundled GDB | Found and executed |
| `tty <path>` | CB-owned slave accepted |
| Inferior stdout/stderr | Works through PtySession |
| Inferior stdin | Works through PtySession |
| Break / continue | Works |
| Terminal.app | **Not required** in Program Console mode |
| `cb_debug_console_hold` | **Not required** in Program Console mode |
| `inferior-tty-claim` support | Bundled GDB supports it |
| Claim-off for I/O | **Not required** for inferior stdio on CB-owned PTY |
| Claim-on warning | Claim-on works **without** the Terminal claim warning |
| Claim-on for VINTR | **Required** for PTY VINTR → inferior SIGINT |
| Claim-off + VINTR | Echoes `^C`; does **not** deliver SIGINT |
| Debugger interrupt | Separate from PTY VINTR; no new `^C` on PTY stream |
| Slave FD hold | **One slave FD must remain open** until `Close()` so the PTY stays alive before GDB attaches |
| Sync CLI `interrupt` | Not a substitute for the production GDB driver interrupt path |

#### Milestone 4 production requirements (from M2) — **MET**

1. Program Console mode creates a PtySession.  
2. PtySession retains master FD and one slave FD.  
3. Pass `GetSlaveName()` into the existing GDB **`tty <path>`** setup path.  
4. Do **not** call `RunNixConsole` in Program Console mode.  
5. Do **not** launch Terminal.app in Program Console mode.  
6. Do **not** launch `cb_debug_console_hold` in Program Console mode.  
7. Do **not** issue `set inferior-tty-claim off` in Program Console mode.  
8. Leave normal GDB tty claiming enabled (claim **ON**).  
9. Inferior stdio flows through PtySession and Program Console.  
10. Program Console Ctrl-C sends VINTR through the PTY.  
11. Debugger Interrupt continues using the existing GDB driver/control path.  
12. Inferior exit triggers **explicit** session teardown rather than relying solely on master EOF.  
13. Terminal.app mode remains completely unchanged, including its existing claim-off behaviour.

#### Milestone 4 implementation notes

| Item | Implementation |
|------|----------------|
| API | `BeginDebugSession(outSlavePath)` / `ActivateDebugSession` / `AbortDebugSession` / `EndDebugSession` (share Run `PtySession` + coordinator). |
| `DebuggerGDB::DoDebug` | If macOS + `PreferProgramConsole()` + console runner: allocate PTY → activate pane → queue **`tty <path>` only** → `Start`/`Attach` (no `GdbCmd_SetupMacOSInferiorTty`). |
| Terminal path | Unchanged: `RunNixConsole` + claim-off probe when mode is `terminal_app` (or non-macOS). |
| Teardown | `NotifyDebugConsoleFinished` and `KillConsole` call `EndDebugSession()` when `m_programConsoleDebug`; no holder SIGUSR1/SIGTERM. |
| Interrupt | Pane Ctrl-C → PTY VINTR (claim ON). Debugger Interrupt → existing GDB driver (unchanged). |

---

### Open questions (M5 / polish)

Resolved by M1/M2 — **do not reopen** unless production evidence contradicts:

- whether GDB can use a CB-owned slave;  
- whether Terminal.app / `cb_debug_console_hold` is required in Program Console mode;  
- whether `inferior-tty-claim off` is required for Program Console;  
- whether PTY VINTR reaches the inferior with claim-on;  
- whether debugger interrupt should be implemented as PTY VINTR;  
- setsid / TIOCSCTTY / VEOF / resize / master-after-slave-close Darwin basics.

Resolved by M3 (Run production) — **do not reopen** for Run:

- Run adapter + `CompilerGCC` lifecycle (`Begin` / `LaunchOnPty` / `Activate` / tracker / `End` + `AnnounceRunFinished`);  
- held slave FD in production `PtySession`;  
- InfoPane panel + key→PTY + generation-safe scrollback;  
- **must** use a `PipedProcess`-derived Run tracker (`HasInput` false) — see M3 lessons.

Resolved by M4 (Debug production) — **do not reopen** for Debug:

- claim-**ON** `tty <path>` without Terminal / holder;  
- teardown via `NotifyDebugConsoleFinished` / `KillConsole` → `EndDebugSession()`;  
- shared Run/Debug `PtySession` coordinator (one active generation at a time).

Still open (post-M5 polish, optional):

- Richer ANSI/SGR colour spans in Program Console (contract remains limited; not full VT100).  
- Further README edge-case notes as users report them.

---

### Focus policy (Run / Debug) — M5 product decision

**Problem:** There is no OS signal for “blocked on `cin`”. Auto-focus after “quiet output” or “quiet while running” can help teaching demos and can steal focus during long silent calculations. Preference must be **user-visible and configurable**.

**Platform compatibility (verified — not a conflict):**

| Platform | Run / Debug console today | Focus behaviour in Code::Blocks |
|----------|---------------------------|----------------------------------|
| **Linux** | External terminal (`DEFAULT_CONSOLE_TERM` = `xterm -T $TITLE -e`, or user `/console_terminal`) + optional `cb_console_runner` / `cb_debug_console_hold` | CB does **not** move IDE focus into an in-pane console (there is none). The WM typically activates the new xterm. On Debug **stop**, `BringCBToFront()` **raises Code::Blocks** so the editor/debugger UI comes forward — the opposite of focusing the console. |
| **Windows** | Separate console window (`AllocConsole` / `wxEXEC_SHOW_CONSOLE` paths) | Same idea: OS owns the console window; CB has no Program Console focus policy. |
| **macOS Terminal.app mode** | External Terminal via osascript / holder | Same external-window model as Linux; Program Console focus settings **do not apply**. |
| **macOS Program Console** | In-IDE InfoPane tab + CB-owned PTY | Focus *can* be moved between editor and pane — **only here** do the options below apply. |

**Conclusion:** Configurable Program Console focus policies are **macOS + `program_console` only**. They do not change Linux/Windows pathways and are not incompatible with them. Do not invent parallel focus knobs for xterm/Windows consoles in v2.0.

**A) Run mode** — config key (draft): `app` `/program_console_focus_run`  

| Value | Behaviour |
|-------|-----------|
| `on_launch` | Transfer keyboard focus to Program Console when the Run session becomes **active** (successful launch / clear scrollback). |
| `never` | Never auto-transfer focus; user clicks the pane (or uses existing focus shortcuts). |
| `on_silence` | After PTY output settles (~250 ms quiet) while the Run child is still alive, transfer focus (current M3-style heuristic). |

**B) Debug mode** — config key (draft): `app` `/program_console_focus_debug`  

| Value | Behaviour |
|-------|-----------|
| `on_launch` | Transfer focus when the Debug Program Console session becomes **active** (PTY ready / pane shown). |
| `never` | Never auto-transfer focus during Debug. |
| `on_step_silence` | Transfer focus only after quiet output **within a running step** (inferior not stopped — debugger triangle absent): i.e. after Continue/Step, while running, when output goes quiet ~250 ms. Do **not** steal focus while stopped on a line (including a `cin` line with prompt already printed). Cancel pending transfer on stop. |

**Defaults (locked for M5 — match Windows/Linux external-console behaviour):**

| Mode | Default | Rationale (Win/Linux today) |
|------|---------|------------------------------|
| **Run** | `on_launch` | External `xterm` / Windows console / Terminal.app is activated by the OS/WM when the console process starts — focus leaves the editor for the console at launch. |
| **Debug** | `never` | Debug stop path calls `BringCBToFront()` so the **IDE** stays (or returns) frontmost; users click the external console when they need to type. No CB heuristic focuses the console on silence/`cin`. |

`on_silence` / `on_step_silence` remain **opt-in** for teaching or cin-heavy workflows. Document defaults + alternatives in settings + `README.macos.md`.

**UI (draft):** macOS-only controls under Environment near “Console programs:” — e.g. “Program Console focus (Run):” and “Program Console focus (Debug):” combo boxes. Hidden or disabled when Console programs = Terminal.app.

**Honesty constraint:** Even `on_silence` / `on_step_silence` cannot distinguish `cin` from a silent long calculation; settings text should say so briefly (“heuristic; may focus during quiet computation”).

**Non-goals for this preference:** controlling WM focus of external xterm/Terminal.app; changing `BringCBToFront` on Linux/Windows Debug stop.

---

### Explicit non-goals (v2.0)

- LLDB; full VT100/curses; Tools-menu in-pane console; multiple concurrent Program Console sessions; redesign of Windows/Linux consoles; changing GDB signing model.

### Hooks (reference)

- Debug (**Terminal.app** mode): `RunNixConsole` → `tty <path>` (+ macOS **claim-off** probe).  
- Debug (**Program Console** mode): PtySession → `tty <path>` only (**claim ON**); no holder.  
- Run (Terminal mode): `CompilerGCC` + `/console_terminal` + `$SCRIPT`.  
- Run (Program Console mode): macOS Run adapter + PtySession (M3).  
- GDB binary: `GetDebuggerExecutable` → bundled path when empty.

---

## Status

| Version | Status |
|---------|--------|
| **v1.0** | **Released** — tag `v1.0` @ `e5bc580`. Import r14050 → Terminal `$SCRIPT` fixes → app bundle + DMG + macOS docs/bootstrap. GDB external. |
| **v1.1** | **Released** — tag `v1.1` @ `cf3e4f0`. Claim-off handshake; `cb_debug_console_hold`; cold-start single Terminal window; README. GDB still external. |
| **v1.2** | **Released** — through `80d8e89`. Bundled GDB from [binutils-gdb](https://github.com/tbgitoo/binutils-gdb) **`5564fbe`**; macOS default; debugger entitlement; `code_signing.md`. |
| **v2.0** | **In progress** — M1–M5 **DONE** (M3/M4 committed; M5 focus policies + docs in tree); Program Console default; Terminal.app optional; Win/Linux unchanged. |
