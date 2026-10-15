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

**Status: Planned** (decisions locked below; not yet implemented).

### Goals

- Code::Blocks owns the **PTY master**; the program / inferior uses the **slave**.  
- Run and Debug share one **Program Console** UI and PTY service.  
- Debugger log stays separate from inferior I/O.  
- Pane inactive until Run/Debug needs a console; GUI targets unchanged.  
- **Default on macOS:** integrated pane. **User option** retains the existing Terminal.app path (v1.x behaviour).  
- **Windows and Linux:** no behaviour change — keep their current console modes; do not modify those paths in an untested open loop.

### Product decisions (locked)

| Topic | Decision |
|-------|----------|
| UI placement | New **tab inside Logs & others** (`InfoPane` / Messages notebook), **not** a separate dockable AUI pane. |
| Platform | **macOS only** for new PTY/pane code and defaults. No Linux/Windows console-path edits without a machine to test. |
| Milestone style | Small, realizable steps; prove PTY/GDB before full Run/Debug wiring. |
| Feature order after PoC | **GDB → PTY first**, then **Run → pane**, then **Debug → pane**. |
| Terminal.app | Remains a **user-selectable option**, not merely an emergency fallback. Default = integrated pane on macOS. |
| Console contract | Line-oriented I/O plus **colours / simple ANSI** (not full VT100/curses). Ctrl-C, Ctrl-D/EOF, resize, scrollback. |
| Scrollback | **Keep** until the next Run or Debug session starts (then clear / replace). |
| Debuggers | **GDB only** for this version (bundled Darwin GDB from v1.2). No LLDB work. |

### Design constraints (summary)

1. “Unclaimed slave removes Darwin claim warning” is a **hypothesis to test** (especially Milestone 2 / 4), not a given.  
2. Explicit ownership: CB must not claim the slave for Debug; Run child may claim as needed; no Terminal/holder merely to keep the slave alive when using the pane.  
3. Small session API (`CreateSession`, master/slave accessors, `SendInput`, `Resize`, `Close`) — not a full process manager.  
4. Non-blocking master I/O off the GUI thread; bound buffers; simple ANSI/colour parsing in the pane.  
5. Bundle-relative paths; use bundled macOS GDB from v1.2.  
6. Setting (name TBD) chooses **Program Console pane** vs **Terminal.app** on macOS only; Windows/Linux settings untouched.

### Milestones

| # | Milestone | Intent |
|---|-----------|--------|
| 1 | Standalone PTY proof of concept | Outside deep CB UI integration: create PTY; launch a small test program; verify output, prompt without newline, stdin, EOF, Ctrl-C, clean exit, resize. Prove Darwin semantics first. |
| 2 | GDB → PTY | Point bundled GDB at a CB-owned slave (`tty` / `inferior-tty`); prove inferior I/O, breakpoints/continue, and whether Darwin claim warning disappears / still needs `inferior-tty-claim off`. Minimal display is enough; full Run wiring not required yet. |
| 3 | Run → Program Console tab | Add the Logs & others tab + transport; console Run targets use the session API into the pane; honour scrollback-until-next-session; GUI targets untouched; Terminal.app remains available via option. |
| 4 | Debug → Program Console tab | Replace Terminal+holder debug console when pane mode is selected; same tab as Run; Debugger log unchanged; full Ctrl-C / session restart behaviour. |
| 5 | Option / lifecycle / polish | Persist pane-vs-Terminal preference; docs (`README.macos.md`); edge cases; ensure Linux/Windows paths never regressed. |

### Hooks (reference)

- Debug (today): `RunNixConsole` → `tty …` (`cbplugin.cpp`, `debuggergdb.cpp`).  
- Run (today): `CompilerGCC` + `DEFAULT_CONSOLE_TERM` / `$SCRIPT`.  
- After v1.1: Darwin may queue `set inferior-tty-claim off` on the Terminal path; pane mode may drop that if Milestone 2/4 shows claim succeeds on an unclaimed slave.

---

## Status

| Version | Status |
|---------|--------|
| **v1.0** | **Released** — tag `v1.0` @ `e5bc580`. Import r14050 → Terminal `$SCRIPT` fixes → app bundle + DMG + macOS docs/bootstrap. GDB external. |
| **v1.1** | **Released** — tag `v1.1` @ `cf3e4f0`. Claim-off handshake; `cb_debug_console_hold`; cold-start single Terminal window; README. GDB still external. |
| **v1.2** | **Released** — through `80d8e89`. Bundled GDB from [binutils-gdb](https://github.com/tbgitoo/binutils-gdb) **`5564fbe`**; macOS default; debugger entitlement; `code_signing.md`. |
| **v2.0** | **Planned** — Program Console tab in Logs & others; macOS-only PTY; GDB→PTY then Run→pane then Debug→pane; Terminal.app as user option; Windows/Linux unchanged. |
