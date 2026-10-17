# Code::Blocks macOS — versioning plan

Roadmap for the unofficial Apple Silicon port in this repository. Versions are sequential: finish and test each before starting the next major step in the console/debugger line.

```text
  → v1.0 (native macOS port + packaging)
  → v1.1 (Terminal console polish + Darwin inferior-tty-claim)
  → v1.2 (bundled Darwin GDB in the .app)
  → v2.0 (integrated Program Console / PTY pane)
  → v2.1 (Debug single open file, no project)
  → v2.2 (standalone New → Save as C|C++ → Build/Run/Debug; tab Rename)
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

## v2.1 — Debug a single open file (no project)

**Status: DONE** — M1–M4 complete (standalone Debug + compatibility + docs).
**Goal:** Allow **Debug** and cold-start **Step Into** when there is **no active project**, for the same class of files single-file **Run** already supports: one open source file, built as a console executable next to the source. Reuse existing single-file compile/run machinery. **No** fake or temporary `cbProject` / `.cbp`.

### Product decisions (locked)

| Topic | Decision |
|-------|----------|
| Platform | **Cross-platform** (shared menu + compiler + debugger entry paths). |
| Scope | One open `ftSource` file → console executable (same as single-file compile). Scripts stay Run-only. |
| Rebuild | **Always** recompile before no-project Debug / cold-start Step Into. No timestamp / stale-exe / “use existing executable” shortcut. Failed compile → **never** fall back to an older exe; **never** start the debugger. |
| Debug information | Enable the selected compiler’s **debug-information** option via existing compiler abstractions (not a hardcoded `"-g"` in generic code). **Do not strip** the resulting executable, using that compiler’s strip semantics. Other **default compiler** options remain as configured. |
| Args / cwd | Same as `RunSingleFile`: cwd = source file directory; **no** program arguments. |
| Console | **Inherit** existing console preference only. **No** new PTY/tty/focus/signalling. v2.0 console architecture stays orthogonal. |
| Debuggers | **DebuggerGDB** = primary implement + runtime-test path. **gdbmi** = compatibility constraint (must not knowingly break; compile when contrib on if practical). Unavailable Linux/contrib runtime must **not** block primary v2.1 release. |
| Project / attach | **Additive** branch only. Existing project Debug and attach-to-process stay **as unchanged as reasonably possible**. |
| `DebuggeeDescription` | A **means** to remove the project requirement for the **standalone** path — **not** a mandate to rewrite or normalise the existing debugger architecture. |

**Governing rule (locked):**

> Remove the unnecessary project requirement for single-file Debug;  
> do **not** redesign the debugger for aesthetic symmetry.

### Conceptual control flow (locked)

```text
attaching to process?
    → existing attach path  (leave as-is)

active project?
    → existing project debug path  (leave as-is unless a tiny local cleanup is forced)

no project + valid active ftSource editor (saved / saveable)?
    → save if needed (existing CB save-before-compile)
    → always rebuild single file (debug info on, strip off)
    → compile OK?
         → construct standalone DebuggeeDescription
         → enter existing debugger startup at the narrowest hand-off point
              (Debug or Step Into semantics as requested)
    → compile failed?
         → normal compiler diagnostics
         → do NOT launch debugger
         → do NOT use a previous executable
         → do NOT clear Program Console / debugger state merely because compile started

otherwise
    → Debug unavailable / existing error behaviour
```

### Inspection notes (pre-implementation — current code)

These are findings from the tree; they refine the plan and must not be ignored at implementation time.

#### Compiler debug symbols / stripping (≠ literal `"-g"`)

- There is **no** single API like `Compiler::GetDebugInfoFlag()`.  
- Each compiler’s options XML defines a **named** option **`"Produce debugging symbols"`** whose `option=` string is compiler-specific (`-g` in `options_gcc.xml` / Clang; `/Zi` in `options_msvc*.xml`; etc.).  
- Strip is typically a separate option **`"Strip all symbols from binary (minimizes size)"`** (often `additionalLibs="-s"` for GCC-family, not always a compile `-s`). GCC’s debug option lists `supersedes="-s"` in the catalogue.  
- Lookup exists: `CompilerOptions::GetOptionByName` / `GetOptionByOption`.  
- **Plan requirement:** for the no-project Debug compile path, resolve and apply a debug-information CompOption from the selected/default compiler’s catalogue, and ensure the **strip** CompOption is **not** applied — without hardcoding `"-g"` / `"-s"` in shared single-file debug code. Preferred name order: **Produce debugging symbols** (GCC/MSVC) → **Tune Debugging for GDB** (Clang) → **Produce minimal debugging symbols** (Clang fallback) → match common option strings (`-g`, `-ggdb`, `/Zi`, …) in the catalogue.  
- If an exotic compiler lacks any such option, fail clearly rather than inventing flags.

#### Save-before-compile

- `CompilerGCC::PrepareCompileFile` calls `cbEditor::Save()` then uses `ed->GetFilename()`.  
- `CompileFileWithoutProject` calls `EditorManager::Save(file)`.  
- `cbEditor::Save()`: if `!m_IsOK` (never successfully associated with a path), returns **`SaveAs()`**. Unmodified save is a no-op; modified buffers are written.  
- **Reuse this.** Do not invent a second save workflow. Debugger must correspond to what the user sees; never compile an older on-disk copy while leaving newer unsaved buffer contents out of the build. Never-saved source must obtain a real filesystem path (Save As) before compile/debug.

#### Step Into cold-start (project reference behaviour)

- Menu **Step** (`OnStep`): if debugger **not** running → `Debug(true)` (`breakOnEntry = true`); if running → `Step()`.  
- Menu **Debug/Continue** (`OnStart`): if not running → `Debug(false)`.  
- `DebuggerGDB::Debug(breakOnEntry)` → `EnsureBuildUpToDate(breakOnEntry ? StartTypeStepInto : StartTypeRun)` → driver `Start(breakOnEntry)` → GDB **`start`** vs **`run`**.  
- **No-project Step Into:** same `Debug(true)` / `StartTypeStepInto` / `start` semantics **after** the always-rebuild succeeds. Do not invent separate single-file Step Into meaning.

#### Minimum data DebuggerGDB actually uses today

From `DoDebug` / `GetDebuggee` / console setup (not speculative):

| Need | Project path today | Standalone v2.1 |
|------|--------------------|-----------------|
| Executable path | `target->GetOutputFilename()` (+ host app for libs) | Same-named exe beside source (`EXECUTABLE_EXT`), as `RunSingleFile` |
| Working directory | `target->GetWorkingDir()` (abs. to project) | Source file directory |
| Program arguments | `target->GetExecutionParameters()` | Empty |
| Console vs GUI | `ttConsoleOnly` / `GetUseConsoleRunner()` | Console = true (both “console target” and “use console runner” semantics as required by existing Debug console branch) |
| Compiler → debugger exe | Target/project compiler, else default | Default compiler |
| Optional project | Search dirs, remote-debug map, lib path generator | null (skip project-only features) |
| Optional target | Above + `StartDriver(target)` | null |
| Attach PID | Separate branch | Unchanged |

GDB **user** arguments remain `GetActiveConfigEx().GetUserArguments()` (debugger config), not program args.

**Note:** `ParseSearchDirs(*m_pProject)` today runs outside a null check when not attaching — the **standalone** path **must** be null-safe (guard, early branch, or only call when project non-null). Fixing that hazard on the new path does **not** require converting the whole project-debug flow to `DebuggeeDescription`.

#### Always rebuild vs Run

- Single-file **Run** may prompt to build only if the exe is missing — it does **not** always rebuild.  
- v2.1 **Debug** deliberately always rebuilds. That difference is intentional, not a bug.

#### Scope of DebuggeeDescription (inspection)

- The type is justified for the **standalone** hand-off: carry exe / cwd / args / console / optional project/target into existing startup without a fake `cbProject`.  
- Prefer an **additive** overload, adapter, or **early standalone branch** with the **smallest regression surface**.  
- **Do not** require converting every existing project-debug call site to `DebuggeeDescription` for symmetry.  
- Shared downstream code may consume the description **only** where that is locally clean and removes a **genuine** project assumption on the new path. Do **not** propagate the type through unrelated debugger layers without a concrete need.  
- If, at implementation time, routing **project** Debug through the same description proves naturally small and low-risk, that may be done and documented as an opportunistic cleanup — it is **optional**, not a milestone requirement. Default assumption: **preserve the mature project path**; use the description **primarily for standalone**.

### Why today it fails (repository finding)

1. **UI:** `DebuggerMenuHandler::OnUpdateUI` enables Debug only when `GetActiveProject()` is non-null (unless attached).  
2. **DebuggerGDB:** `Debug` returns false if no project and not attaching (*“can only debug projects or attach to processes”*).  
3. **gdbmi:** Same project hard-fail / `StartDebugger(project, …)`.  
4. **Shared helpers:** `GetDebuggee` requires `ProjectBuildTarget*`; `EnsureBuildUpToDate` calls project `Build()`.  
5. **Contrast:** `RunSingleFile` / `CompileFileWithoutProject` already cover no-project compile+run (without forced debug info).

### Architecture (implementation contract)

#### DebuggeeDescription — narrow means, not a rewrite

`DebuggeeDescription` exists to **remove the project requirement** on the single-file Debug path. It is **not** a mandate to rewrite, normalise, or “modernise” the existing debugger architecture.

Exact fields follow inspection (above); do **not** add speculative fields. Conceptually for the **standalone** hand-off:

- executable path  
- working directory  
- arguments (empty for standalone)  
- console classification (standalone: console)  
- optional `cbProject*` (null for standalone)  
- optional `ProjectBuildTarget*` (null for standalone)  

**How it is used:**

1. Construct it for the **standalone** path after a successful always-rebuild.  
2. Introduce it only at the **narrowest** point needed to feed that information into **existing** debugger startup (additive overload, adapter, or early branch).  
3. Keep **project Debug** and **attach** on their current paths unless a tiny, forced local change is required for shared null-safety or an optional low-risk cleanup (see inspection).  
4. Do **not** propagate the type through unrelated layers “because we have it.”  
5. Do **not** turn this into a large framework.

#### Other pieces

| Piece | Responsibility |
|-------|----------------|
| **Branching** | Attach → existing; project → existing; standalone → new; else unavailable. Additive only. |
| **Menu / UpdateUI** | Enable Debug / Step when project (as today) **or** no project + valid `ftSource` builtin editor (not “Start here”). |
| **Save** | Reuse `PrepareCompileFile` / `cbEditor::Save` / Save As for never-saved. Abort path if user cancels save. |
| **Standalone compile** | Always rebuild via single-file machinery + default compiler options + **debug-info CompOption on** + **strip CompOption off**. On failure: diagnostics only; hard stop before GDB; no stale exe; no gratuitous console/debugger state wipe. |
| **DebuggerGDB** | After successful standalone compile, hand off via `DebuggeeDescription` into existing startup (`breakOnEntry` false/true). Console via **existing** v2.0 Program Console or Terminal.app / Linux–Windows paths. Project/attach code paths stay mature references. |
| **gdbmi** | Compatibility only: do not knowingly break; avoid architecture expansion; document leftover work if standalone support needs substantial separate effort. Compiles with contrib if practical; runtime optional. |
| **v2.0 console** | Orthogonal. Standalone Debug only **supplies a debuggee**; inferior I/O stays the already-selected console mode. |

### Deliberate minimality (no UI)

Retain for v2.1:

- one source file; default compiler configuration; debug information enabled; stripping disabled; exe via existing single-file rules; cwd = source dir; no program args; console program; existing debugger selection; existing console preference.

Do **not** add UI for: program arguments, custom cwd, environment variables, include paths, libraries, linker options, per-file compiler options, multiple sources, custom build targets.

Dividing line: default single-file semantics suffice → no project; custom build/run configuration → create a project. Do **not** recreate the project system inside single-file mode. Do **not** auto-create an ephemeral `.cbp`.

### Milestones

| # | Status | Work |
|---|--------|------|
| **M1** | **DONE** | **Standalone compile + narrow hand-off.** CompOption debug-info/strip handling; define small `DebuggeeDescription` for **standalone**; save + always-rebuild; menu/UpdateUI; compile failure aborts debugger launch. Project Debug path not converted to the description. |
| **M2** | **DONE** | **DebuggerGDB standalone.** No-project Debug / Step Into (existing `Debug(true)` / `start` semantics after always-rebuild); source dir for breakpoints; exe existence check; console via existing Program Console / Terminal path; clean exit clears standalone state for restart. Project Debug and attach left on existing paths. |
| **M3** | **DONE** | **Compatibility.** See **M3 results** below. |
| **M4** | **DONE** | **Documentation / release.** User note + limitations in `README.macos.md`; ChangeLog v2.1 entry; this plan marked complete. |

Order: **M1 → M2 → M3 → M4**. M3 must not block M2 on this fork (`--disable-contrib-plugins`).

#### Milestone 3 results (DONE)

| Check | Result |
|-------|--------|
| **Project Debug** | Still on the mature path: `EnsureBuildUpToDate` → `Build()` → optional “Debug anyway”. **Not** routed through `DebuggeeDescription` (expected default). |
| **Attach** | Unchanged early branch (`m_PidToAttach != 0`); standalone only when no project and not attaching. |
| **Single-file Run** | `RunSingleFile` / `CompileFileWithoutProject` untouched; always-rebuild + CompOption guard only on `CompileStandaloneFileForDebug`. |
| **Shared helpers** | `GetDebuggee(target*)` signature unchanged. `EnsureBuildUpToDate` clears `m_StandaloneDebugBuild`. Project build-fail still offers “Debug anyway”; standalone hard-fails only when that flag was set. |
| **Linux / Windows** | Standalone entry is shared C++ (menu + CompilerGCC + DebuggerGDB); no `#ifdef` narrowing the feature to macOS. **Not runtime-tested** on Linux/Windows on this fork. |
| **gdbmi** | No references to `DebuggeeDescription` / `CompileStandaloneFileForDebug`. Still requires an active project (`Debug` logs “no active project”). **Compile OK** against post-M1/M2 SDK: `make -C src/plugins/contrib/debugger_gdbmi` produced `libdebugger_gdbmi.dylib` (tree configured with `--disable-contrib-plugins`; plugin not installed into the app bundle). **Runtime not tested.** |
| **Menu vs gdbmi** | `DebuggerMenuHandler::OnUpdateUI` enables no-project Debug for any active debugger when an `ftSource` candidate is open. If **gdbmi** is the active debugger, the menu can enable while `Debugger_GDB_MI::Debug` still refuses — leftover UX gap, not an API break. Standalone support remains DebuggerGDB-only by design. |
| **Runtime smoke (this fork)** | Case **A** (standalone `.cpp` Debug) confirmed working after DoRunQueue `-2` / CompOption fixes. Cases **F–H** (project / attach / Run) verified by code review only in M3. |

### Acceptance cases (minimum)

| ID | Case | Expected |
|----|------|----------|
| **A** | Saved `.cpp`, no project → Debug | Recompiles with debug info; breakpoint in `main` works; Step/Continue; Program Console I/O (macOS default); clean exit. |
| **B** | Saved `.c`, no project | Same path; default compiler C semantics. |
| **C** | Modified unsaved existing source | Existing save-before-build; debugger runs the newly compiled source. |
| **D** | Never-saved source | Must Save As (real path) before compile/debug; cancel → no debug. |
| **E** | Compile error | Diagnostics visible; GDB does not start; stale previous exe unused; Program Console/debugger state not wiped merely for the failed attempt. |
| **F** | Existing project → Debug | Unchanged. |
| **G** | Attach to process | Unchanged. |
| **H** | Single-file Run | Unchanged (still not forced always-rebuild). |
| **I** | macOS console preference | Program Console Debug path still works; Terminal.app Debug path still works. |
| **J** | Explicit custom debugger selection | Existing debugger-selection semantics remain in force. |

### Verification environments

| Environment | Required for primary v2.1 release? | Notes |
|-------------|--------------------------------------|--------|
| macOS + DebuggerGDB + Program Console / Terminal.app | **Yes** | Cases A–J as applicable. |
| Linux / Windows + DebuggerGDB | Designed for; smoke if available | No Linux laptop assumed. |
| gdbmi (contrib) | Compile if practical; runtime optional | **Compiled OK** in M3 (`make -C src/plugins/contrib/debugger_gdbmi`); not in default `--disable-contrib-plugins` app; runtime not tested; still project-only. |

### Explicit non-goals (v2.1)

- Ephemeral / auto-created `.cbp` or fake `cbProject`.  
- Timestamp-based skip of rebuild; falling back to older executables after failed compile.  
- Hardcoded `"-g"` / `"-s"` in generic cross-compiler code.  
- New console/PTY/focus behaviour for single-file mode.  
- UI for args, cwd, env, includes, libs, per-file options, multi-file.  
- Making gdbmi the centre of the design or blocking release on untested contrib runtime.  
- LLDB; changing single-file Run semantics (except shared helpers that preserve Run).  
- **Rewriting or normalising existing project Debug / attach for architectural symmetry.**  
- Propagating `DebuggeeDescription` through unrelated debugger layers without a concrete need.  
- Making “everything goes through `DebuggeeDescription`” a success criterion.

### Outcome (DONE)

With no project open, an active console-language source file can be Debugged / cold Step-Into’d after an always-rebuild with that compiler’s debug information: GDB runs the freshly linked exe with existing console handling, cwd next to the source, no program args. Project Debug and attach-to-process remain the mature, largely untouched paths; `DebuggeeDescription` is only the narrow bridge that made the standalone case possible.

User-facing note and limitations: `README.macos.md` → **Debugging a single source file (no project)**. ChangeLog: **v2.1** entry.

---

## v2.2 — Standalone New → Save as C|C++ → Build / Run / Debug

**Status: Planned.** Do not implement until this section is accepted.  
**Depends on:** v2.1 standalone Debug (leave `DebuggeeDescription` / DebuggerGDB architecture intact; only adjust **routing gates** that today key off “any active project”).

**Goal:** Make the standalone-source workflow coherent:

```text
New → choose/save as C or C++ → edit → Build / Build and Run / Debug
```

while project-owned sources keep the existing project workflow. A project may be open without forcing every Build/Run/Debug onto that project.

Two defects to fix: (1) Build / Build and Run (and today also Debug gates) can follow **project existence** or a **stale Untitled path** instead of the active source; (2) Save As exposes a combined **C/C++ files** filter whose first mask is `*.c`, without a first-class C++ choice.

### Product decisions (locked)

| Topic | Decision |
|-------|----------|
| **Routing** | **Active-source ownership**, not mere project existence. See rule below. |
| Project-owned source | Existing project Build / Run / Debug **unchanged**. |
| Standalone source (incl. with a project open) | Standalone Build / Build and Run / Debug on **that** file — never the unrelated project. |
| Unsaved new buffer | Save As first; cancel → **abort**; then classify **post-save** path and route. |
| C vs C++ Save As | Separate **C files** / **C++ files** / **C/C++ header files**; one shared `C/C++` lexer. |
| Legacy filter preference | Stored `"C/C++ files"` → migrate to **`"C files"`** (preserves legacy `.c`-first default). |
| Fresh / empty config default | **Independent** of migration: initial filter **`"C++ files"`** (`.cpp` first). Not a reason to force all new files to C. |
| v2.1 Debug | Preserve `DebuggeeDescription` / DebuggerGDB hand-off; update **candidate / branch gates** to the ownership rule. |
| Scope | Minimal; reuse `cbEditor::Save` / Save As; no beginner mode, fake projects, or parallel build system. |

#### Routing rule (REQUIRED)

```text
active source editor
      |
      +-- source belongs to the active project
      |       -> existing project Build / Run / Debug
      |
      +-- source does not belong to the active project
              -> standalone Build / Build and Run / Debug
```

Examples:

| | Situation | Route |
|---|-----------|-------|
| A | Project open + active project-owned `foo.cpp` | Project workflow |
| B | Project open + active standalone `foo.cpp` | Standalone on `foo.cpp` |
| C | Project open + Untitled → Save As → `.cpp` not in project | Standalone on saved file; cancel aborts |
| D | No project + standalone source | Standalone |

### Repository findings (grounded)

#### 1. New / Untitled editor (`File → New → Empty file`)

| Fact | Evidence |
|------|----------|
| Entry | `MainFrame::OnFileNewWhat` (`src/src/main.cpp`) → `EditorManager::New()` → `cbEditor` empty filename. |
| Identity | `cbEditor::DoInitializations`: `_("Untitled")` + counter; optional project base path prefix. |
| `m_IsOK` | Never-saved ⇒ `!IsOK()`. |
| Language | No ext ⇒ not `ftSource`; no C/C++ identity until Save As + `SetLanguage(HL_AUTO)`. |

#### 2. Save As filters and `Untitled1.c`

| Fact | Evidence |
|------|----------|
| Combined name | `EditorColourSet::SetFileMasks` → `FileFilters::Add("%s files", lang)`; lexer `C/C++` → **`C/C++ files`**. |
| Masks | `lexer_cpp.xml`: `*.c,*.cpp,*.cc,*.cxx,…` — **first source mask `*.c`**. |
| Remembered filter | `cbEditor::SaveAs` reads `/file_dialogs/save_file_as/filter`, default `"C/C++ files"`. |
| Why `.c` | First mask of that filter steers default extension where the platform appends one; CB comments document wxGTK first-filter behaviour. |
| Platform | wx save-extension append is **not** uniform (MSW vs OSX/GTK). |

#### 3. Project membership API (targeted finding)

**Do not invent a parallel ownership model.** Existing mechanisms:

| Mechanism | Location | Role |
|-----------|----------|------|
| **`cbEditor::GetProjectFile()`** | `cbeditor.h` / `cbeditor.cpp` | Non-null ⇒ editor is bound to a `ProjectFile`. Header: editors belonging to a project file have this set; others return `nullptr`. |
| **`ProjectFile::GetParentProject()`** | `projectfile.h` | Owning `cbProject*`. |
| **`cbProject::GetFileByFilename(filename, isRelative, isUnixFilename)`** | `cbproject.h` | Path membership lookup. |
| **Prior art** | `CompilerGCC::PrepareCompileFile` | Uses `ed->GetProjectFile()` then `pf->GetParentProject()` (and may `SetProject`). `CompileFile` uses `m_pProject->GetFileByFilename(...)`. |
| **Save As clears binding** | `cbEditor::SaveAs` | Calls `SetProjectFile(nullptr)` — after Save As, **path** membership must be re-checked; do not trust a stale `GetProjectFile()`. |

**v2.2 ownership predicate (active project):**

```text
BelongsToActiveProject(ed, path):
  active = ProjectManager::GetActiveProject()
  if !active -> false
  pf = ed->GetProjectFile()
  if pf && pf->GetParentProject() == active -> true
  if active->GetFileByFilename(path, /*isRelative=*/false, /*isUnixFilename=*/false) -> true
  else false
```

(Exact `GetFileByFilename` flags: use absolute path as `PrepareCompileFile` / open-file call sites do with `false` for relative when the editor path is absolute — **confirm at implement time** against the overload used for absolute editor paths.)

**Multi-project note:** If `GetProjectFile()` names a **non-active** open project, that is “does not belong to the **active** project” under the locked rule → **standalone** route. Do not silently build the inactive project. (Verify in tests if this edge appears in normal UI.)

#### 4. Separating C and C++ (Save As / filters only)

Keep **one** lexer `C/C++`. Split filter registration (e.g. special-case in `SetFileMasks`):

| Filter name | Mask |
|-------------|------|
| **C files** | `*.c` |
| **C++ files** | `*.cpp;*.cc;*.cxx;*.c++` (**.cpp` first**) |
| **C/C++ header files** | `*.h;*.hpp;*.hh;*.hxx;*.inl;*.ipp;*.tcc;*.tpp` |

**Do not** keep a live `"C/C++ files"` filter entry after the split (avoids a dead remembered name that no longer resolves). Migration handles old configs (below).

Standalone compile already selects C vs C++ by extension (`CompilerCommandGenerator`: ext `"c"` → C program; else CPP).

#### 5. Extensions

| Role | Extensions |
|------|------------|
| C | `.c` → C driver |
| C++ | `.cpp`, `.cc`, `.cxx`, `.c++` → C++ driver; Save As default **`.cpp`** |
| Headers / templates | as today (`ftHeader` / `ftTemplateSource`) |

#### 6. Save → reacquire path → operate (invariant)

**Confirmed correct pattern** (reuse, do not reinvent):

```text
ed->Save()                    // SaveAs if !IsOK
if false / cancel -> abort
path = ed->GetFilename()      // post-save only
classify FileTypeOf(path)
BelongsToActiveProject(ed, path) -> route
Build / Run / Debug that path
```

Demonstrated by `PrepareCompileFile` and v2.1 `SaveActiveSourceForStandaloneDebug`.

**Never:** capture `Untitled…` → Save As → continue with the captured Untitled path.

#### 7. Build / Run / Debug today vs required

| Case | Today (confirmed) | v2.2 required |
|------|-------------------|---------------|
| Untitled, no project | `CompileFileWithoutProject`: Save return ignored; compile **pre-SaveAs** path → often empty cmds | Save → post-save path → standalone |
| Untitled / standalone, **project open** | `CheckProject()` true → **project** build (unrelated) | Save if needed → ownership → **standalone** if not in project |
| Project-owned active file | Project build | Unchanged project build |
| Modified saved standalone | Save-by-path (often OK) | Same invariant; honour failures |
| Non-`ftSource` after save | Empty / odd | Abort clearly |
| Debug, project open | `IsStandaloneDebugCandidate` **false** if any active project | Same ownership rule as Build (gate change only) |

Primary bug surface: **`CompilerGCC`** (`DoBuild` / `Run` / `CompileFileWithoutProject`) and **v2.1 gates** in `DebuggeeDescriptionUtil` / `DebuggerGDB::Debug` / menu UpdateUI — not a DebuggeeDescription redesign.

#### 8. Filter preference: legacy migration vs fresh default

Config key: `/file_dialogs/save_file_as/filter` (app).

| Situation | Behaviour |
|-----------|-----------|
| **Legacy migration** | Stored value **`"C/C++ files"`** → resolve as **`"C files"`**. Rationale: old combined mask was `*.c`-first; preserve that user’s historical default. On successful Save As (or when resolving the filter for display), **rewrite** the stored value to `"C files"` so the dead name does not linger. |
| **Fresh / empty config** | If the key is missing / empty, initial selection is **`"C++ files"`** (`.cpp` default). Independent of migration — does **not** force all new files to C. |
| **Valid modern values** | `"C files"`, `"C++ files"`, header filter, other languages, All files — existing last-used behaviour. |

Implementation locus: `cbEditor::SaveAs` when reading/resolving filter name + index (and when writing back after OK).

### Architecture (implementation contract)

#### A. Save As filters + migration

| Piece | Responsibility |
|-------|----------------|
| `EditorColourSet::SetFileMasks` (C/C++ special-case) | Register C / C++ / header filters; **omit** combined `"C/C++ files"`. |
| `cbEditor::SaveAs` | Resolve remembered filter: migrate `"C/C++ files"` → `"C files"`; fresh default `"C++ files"`; after OK persist the **new** valid name. |

#### B. Shared save / classify / route (illustrative)

```text
PrepareActiveSourceOperation(pathOut, routeOut) -> bool
  ed = builtin active editor (fail if none)
  if !ed->Save() -> false                    // cancel / fail
  pathOut = ed->GetFilename()
  if !IsOK or path empty -> false
  if FileTypeOf(pathOut) != ftSource -> false // clear message
  if BelongsToActiveProject(ed, pathOut)
       routeOut = Project
  else
       routeOut = Standalone
  return true
```

#### C. Wire Build / Build and Run / Run

| Call site | Change |
|-----------|--------|
| `CompilerGCC::DoBuild` / `Build` entry | **Not** `if (CheckProject()) project else untitled`. After prepare: **Project** → existing project `DoBuild`; **Standalone** → `CompileFileWithoutProject(pathOut)` (post-save path only). |
| `CompileFileWithoutProject` | Never compile a path captured before Save; honour Save failure. |
| `Run` / Build and Run | Same routing; Run after compile uses **active editor’s post-save** filename / standalone exe. |
| Project-owned path | Existing project machinery only — no standalone ensure beyond normal project saves. |

#### D. Wire Debug (v2.1-compatible)

| Call site | Change |
|-----------|--------|
| `IsStandaloneDebugCandidate` | Replace `if (GetActiveProject()) return false` with: never-saved OK **or** (`ftSource` **and** not `BelongsToActiveProject`). |
| `DebuggerGDB::Debug` | Keep additive branches; standalone when candidate under **ownership** rule; project path when active source belongs to active project. |
| `SaveActiveSourceForStandaloneDebug` | Keep save → `GetFilename()` invariant; may share prepare helper (**optional** dedup). |
| `DebuggeeDescription` | **Unchanged** type / MakeStandalone. |

Menu UpdateUI already uses `IsStandaloneDebugCandidate` — fixing that gate fixes enablement for project-open + standalone.

### Proposed change table

| # | File(s) | Class / function | Current | Proposed | Why narrowest | v2.1 | Risk |
|---|---------|------------------|---------|----------|---------------|------|------|
| 1 | `editorcolourset.cpp` (± filefilters) | `SetFileMasks` | One `"C/C++ files"` | **C** / **C++** / **header** filters; drop combined | Filter birthplace; one lexer | None | Med |
| 2 | `cbeditor.cpp` | `SaveAs` | Default / remember `"C/C++ files"` | Fresh default **`C++ files`**; migrate **`C/C++ files` → `C files`**; persist new name | Only preference I/O | None | Low–med |
| 3 | `compilergcc.cpp` (+ small helper) | `DoBuild`, `Run`, `CompileFileWithoutProject` | Project-if-exists; stale Untitled path | Prepare → ownership route; post-save path; abort on cancel | Bug locus | Align Build with Debug | Med |
| 4 | `debuggeedescription.cpp`, `debuggergdb.cpp`, `debuggermenu.cpp` | Candidate / `Debug` branch / UpdateUI | No-project-only standalone | Ownership rule; same DebuggeeDescription | Gate only | Soft touch | Med |
| 5 | README / ChangeLog / this plan | docs | — | C\|C++ Save, migration, ownership routing | Release | Mentions Debug | Low |

**Abstain:** redesigning `DebuggeeDescription`; fake projects; lexer split; global `.c`→`.cpp`; parallel build system.

### Regression analysis

| Area | Risk | Mitigation |
|------|------|------------|
| Project-owned `.c`/`.cpp` | Med (routing change) | Tests **M**; `BelongsToActiveProject` true → untouched project path |
| Standalone with project open | Med (new REQUIRED) | Tests **N–Q** |
| Standalone no project | Low–med | Save invariant; post-save path |
| Cancel Save As | Must abort | Tests **F**, **P** |
| Legacy `"C/C++ files"` | Med | Migrate → **C files**; rewrite store; test **R** |
| Fresh config | Low | Default **C++ files**; test **T** — not driven by migration |
| Explicit C++ then remember | Low | Test **S** |
| Headers | Med | Dedicated header filter |
| Multi-project + wrong active | Low–med | Per locked rule → standalone; document |
| v2.1 Debug architecture | Low | Gates only |
| macOS no auto-ext | Med | Non-`ftSource` abort; filter still steers where platform appends |

### Manual test matrix (REQUIRED)

| ID | Steps | Expected |
|----|-------|----------|
| **A** | New → Save As → **C files** | `.c`; C driver; Run OK |
| **B** | New → Save As → **C++ files** | `.cpp`; C++ driver; Run OK |
| **C** | New unsaved → **Build** | Save As; compile that buffer; cancel → nothing |
| **D** | New unsaved → **Build and Run** | Save As; compile+run that file |
| **E** | New unsaved → **Debug** | Save As; v2.1 standalone debug |
| **F** | Build and Run → cancel Save As | Nothing built/run |
| **G** | Modified saved `.cpp` → Build and Run | In-place save; updated program |
| **H** | Modified saved `.c` → Build and Run | C driver |
| **I** | C project, project-owned active → Build | Project build unchanged |
| **J** | C++ project, project-owned active → Build | Unchanged |
| **K** | Save As → All files → no ext | Not standalone source; abort / no wrong compile |
| **L** | Plain text / Start here | No pretend compile |
| **M** | Project open + **project-owned** `.cpp` → Build and Run | **Project** path unchanged |
| **N** | Project open + **standalone** `.cpp` → Build and Run | **Standalone** only; project **not** built |
| **O** | Project open + Untitled → Build and Run → Save As **C++ files** | `.cpp`; standalone compile/run; project **not** built |
| **P** | As **O**, cancel Save As | Nothing built/run |
| **Q** | Project open + standalone `.cpp` → **Debug** | v2.1 standalone debug; project **not** debugged |
| **R** | Config remembers `"C/C++ files"` → Save As | Resolves to **C files**; `.c` default behaviour; store becomes valid `"C files"` |
| **S** | User selects **C++ files** → later Save As | Remembers **C++ files** normally |
| **T** | Fresh/empty filter config → Save As | Initial selection **C++ files** (independent of **R**) |

### Milestones

| # | Status | Work |
|---|--------|------|
| **M1** | In progress (tree) | Split C / C++ / header filters; drop combined name; legacy → **C files**; fresh default **C++ files**; tests A/B/K/R/S/T |
| **M2** | Planned | Save invariant + **ownership routing** for Build / Build and Run / Run; tests C–H, F, L, **M–P** |
| **M3** | Planned | Debug gates / menu under ownership rule; preserve DebuggeeDescription; tests E, **Q**; docs + ChangeLog |
| **M4** | Planned | **Editor-tab Rename** (complement). See below. Order: after M1–M3 core workflow; does not block M1–M3. |

#### Milestone 4 — Editor-tab Rename (planned complement)

Standalone Save As leaves the old path on disk (copy, not rename). Code::Blocks has no inline tab-title edit. Project tree already has **Rename file…** (`ProjectManagerUI::OnRenameFile`); the editor-tab context menu (`EditorManager` tab popup: Close / Save / Open containing folder / …) has no Rename.

**Goal:** Add **Rename…** to the editor-tab context menu so a wrongly saved extension (e.g. `.c` → `.cpp`) can be fixed without Finder + reopen and without leaving a duplicate from Save As.

| Topic | Plan |
|-------|------|
| UI | Tab right-click → **Rename…** (near Open containing folder / Copy full path). |
| Standalone | Prompt for new name (same directory); `wxRenameFile` on disk; update open `cbEditor` path / tab title / language (`SetLanguage(HL_AUTO)`); **no** second copy. Never-saved (`!IsOK`) → disable or fall through to Save As. |
| Project-owned | Prefer reusing / aligning with existing project-tree rename (`ProjectFile::Rename` + tree rebuild), or delegate to that path — do not invent a second ownership model. |
| Prior art | `ProjectManagerUI::OnRenameFile`; tab menu built in `EditorManager` (~`OnNotebookContextMenu` / tab popup). |
| Non-goals for M4 | Inline editable tab caption; mass rename; moving across directories (unless trivial same-dir rename only). |

**Acceptance (sketch):** Rename standalone `.c` → `.cpp` via tab menu → one file on disk, tab and highlighting updated, Build/Run/Debug still see the new path; project-owned rename still consistent with project tree.

### Scope summary

**REQUIRED**

1. C / C++ / header Save As filter separation (one `C/C++` lexer).  
2. Legacy stored `"C/C++ files"` → **`"C files"`** (rewrite so the name is not left dead); fresh default **`"C++ files"`** separately.  
3. Save / cancel / **post-save path** for standalone Build / Build and Run.  
4. **Active-source ↔ active-project membership** routing (even when a project is open).  
5. Preserve v2.1 standalone Debug (**gates** follow the same ownership rule; no architecture rewrite).  
6. Manual matrix **A–T** (project + standalone + migration + fresh default).

**OPTIONAL / complement**

- **M4:** Editor-tab **Rename…** (standalone true rename on disk + editor rebind; project-owned aligned with existing rename).  
- Deduplicate prepare/save helper with `SaveActiveSourceForStandaloneDebug`.  
- macOS-only post-OK extension nudge if native dialog omits ext despite filter.  
- Grep leftover UI strings expecting `"C/C++ files"` beyond Save As config.

**EXPLICITLY OUT OF SCOPE**

- Redesign of `DebuggeeDescription` / DebuggerGDB core.  
- Beginner mode; ephemeral `.cbp`; parallel build system.  
- Splitting the `C/C++` lexer into two languages.  
- Globally forcing `.cpp` (or using migration to force C on fresh configs).  
- gdbmi standalone Debug; LLDB.  
- Changing project target language rules.  
- Standalone args/cwd/includes UI.  
- Inline editable tab titles (M4 is menu Rename, not caption editing).

### Outcome (when done)

New files Save As **C** or **C++** with the right extension. Build / Build and Run / Debug always operate on the **active** source after a successful save, routed by **membership in the active project** — so an open project never hijacks a standalone editor — while project-owned files and v2.1 debugging remain intact. Legacy Save As preference `"C/C++ files"` becomes `"C files"`; new configs start on `"C++ files"`. Optional **M4** adds tab-menu Rename so a wrong extension can be fixed without a duplicate on disk.
---

## Status

| Version | Status |
|---------|--------|
| **v1.0** | **Released** — tag `v1.0` @ `e5bc580`. Import r14050 → Terminal `$SCRIPT` fixes → app bundle + DMG + macOS docs/bootstrap. GDB external. |
| **v1.1** | **Released** — tag `v1.1` @ `cf3e4f0`. Claim-off handshake; `cb_debug_console_hold`; cold-start single Terminal window; README. GDB still external. |
| **v1.2** | **Released** — through `80d8e89`. Bundled GDB from [binutils-gdb](https://github.com/tbgitoo/binutils-gdb) **`5564fbe`**; macOS default; debugger entitlement; `code_signing.md`. |
| **v2.0** | **In progress** — M1–M5 **DONE** (M3/M4 committed; M5 focus policies + docs in tree); Program Console default; Terminal.app optional; Win/Linux unchanged. |
| **v2.1** | **DONE** (tree) — M1–M4 complete: standalone Debug / Step Into (DebuggerGDB); docs in README.macos.md + ChangeLog. Tag/release when ready. |
| **v2.2** | **Planned** — M1–M3: Save As C\|C++, ownership routing, Debug gates; **M4** (complement): editor-tab Rename. |
