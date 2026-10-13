# Program console: v1.1 (silence Darwin tty warning) and v2.0 (integrated pane)

Two separate scopes. **Implement and test v1.1 first. Do not start v2.0 until v1.1 is complete.**

---

## Shared context

On macOS, Run and Debug both use **Terminal.app** today:

- **Run:** `osascript` + optional `cb_console_runner`.
- **Debug:** Terminal + `sleep`, discover `/dev/ttys…`, GDB `tty …`.

Debug then hits a long Darwin warning: GDB’s child cannot `TIOCSCTTY` because Terminal’s session already owns the device. **stdin/stdout/stderr still work.** Job control / Ctrl-C stay with the owning session.

Windows uses `set new-console on` (Win32 only). That path cannot be copied to Darwin.

---

## v1.1 — Silence the warning (Terminal architecture unchanged)

### Scope

1. Keep Terminal.app-based Run and Debug.
2. No PTY service, integrated pane, process manager, or other architecture changes.
3. Smallest clean change: skip the failing claim when the IDE intentionally uses a busy Terminal tty, via a **GDB-side setting** (not stderr filtering).
4. macOS/Darwin-specific in Code::Blocks unless evidence says otherwise.
5. Preserve debugger I/O, Windows, and Linux behaviour.
6. Update `README.macos.md` for the changed Debug handshake.
7. Size: v1.1 bugfix / polish.

### Inspection result (do not assume the command exists)

**Verified against `/Users/thomasbraschler/git/binutils-gdb` (local tip `ce3944268b7`, version `19.0.50.DATE-git`):**

| Finding | Detail |
|---------|--------|
| Claim / warning site | `gdb/inflow.c` → `new_tty()`: always attempts `ioctl(TIOCSCTTY)`; on Darwin `EPERM` + session leader prints the long “Darwin will not transfer it…” warning (commit `c18841665bf`). |
| Proposed setting | **`set inferior-tty-claim` does not exist yet.** Only discussed. Must be added in the GDB port before Code::Blocks can send it. |
| Existing tty API | `set inferior-tty` / alias `tty` in `gdb/infcmd.c`; path stored then applied in child via `new_tty()`. |
| CB send site today | After `RunNixConsole`, queues `tty <path>` in `DebuggerGDB::Debug` (`debuggergdb.cpp`, non-Windows block). |
| CB init path | `GDB_driver::Prepare` queues fixed `set …` commands + user init commands; `show version` is logged but there is **no** general GDB feature probe. |
| Platforms in CB | `platform::macosx` / `platform::darwin` in `src/include/prep.h`. |

### Exact command to add (GDB)

Proposed (default preserves today’s behaviour):

```text
set inferior-tty-claim on|off     # default: on
show inferior-tty-claim
```

Semantics:

- **`on` (default):** current behaviour — try `TIOCSCTTY` (and Darwin `tcsetpgrp` on success); keep verbose Darwin warning on expected `EPERM`.
- **`off`:** still open slave and dup onto 0/1/2; **skip** `TIOCSCTTY` and the Darwin `tcsetpgrp` follow-up; **emit no** controlling-terminal warning.

This disables the operation that causes the warning; it does not filter Terminal text in Code::Blocks.

### Minimal implementation plan (after this planning step)

**A. GDB (`binutils-gdb`) — required first**

1. Add a boolean (e.g. `inferior_tty_claim`, default `true`) and `add_setshow_boolean_cmd ("inferior-tty-claim", …)` next to related terminal / inferior-tty registration (likely `inflow.c` INIT or beside `inferior-tty` in `infcmd.c`, wired into `new_tty()`).
2. In `new_tty()`, gate the entire `#ifdef TIOCSCTTY` claim block (including Darwin `tcsetpgrp`) on that flag.
3. Extend `gdb/testsuite/gdb.darwin/aarch64-inferior-tty.exp` (and helper if needed): busy tty + `set inferior-tty-claim off` → I/O works, **no** “Failed to set controlling terminal” / “Darwin will not” text; default `on` + busy still documents/warns as today.
4. Build/install the Darwin GDB used by Code::Blocks before relying on the CB side.

**B. Code::Blocks (`codeblocks-macos`) — small, Darwin-only**

1. In `DebuggerGDB::Debug`, only when `platform::macosx` (or `darwin`) and `RunNixConsole` succeeded, **before** `tty <consoleTty>`:
   - Probe capability (prefer detection over blind `set`).
   - If supported: queue `set inferior-tty-claim off` (silent / log-to-debug as appropriate), then existing `tty …`.
2. Capability detection options (pick one small pattern):
   - Queue `show inferior-tty-claim` (or `help set inferior-tty-claim`) once per debug start; parse for GDB’s undefined-command error; only then queue `set … off`; **or**
   - One-shot session flag after first successful `set`.
   - Do **not** unconditionally send a command stock/other GDBs will reject as fatal to the session (unknown `set` errors are usually non-fatal but noisy — detection avoids that).
3. Do **not** change Linux `RunNixConsole` / `tty` behaviour; do **not** touch Windows `new-console` / `AllocConsole`.
4. Do **not** filter warning strings in `GDB_driver::ParseOutput` or the Terminal window.
5. Document in `README.macos.md`: paired Darwin GDB; CB sets `inferior-tty-claim off` for the Terminal+`sleep` console so I/O stays on that tty without attempting a claim Darwin will refuse.

**C. Test plan (v1.1)**

1. GDB dejagnu: busy vs claim-off cases above.
2. Manual in Code::Blocks + paired GDB: Step-Into / debug console target with `cin`/`cout` → Terminal opens, **no** long Darwin warning, input/output still work, breakpoints/continue unchanged.
3. Regression: Linux/Windows untouched (no code path change); stock GDB on macOS without the setting → CB skips `set` after failed probe, behaviour as today (warning may still appear).

### Files / functions likely touched

| Tree | Files / symbols |
|------|-----------------|
| `binutils-gdb` | `gdb/inflow.c` (`new_tty`, set/show registration); possibly `gdb/terminal.h` / `gdb/infcmd.c`; `gdb/testsuite/gdb.darwin/aarch64-inferior-tty.exp` (+ `.c` if needed) |
| `codeblocks-macos` | `src/plugins/debuggergdb/debuggergdb.cpp` (`DebuggerGDB::Debug` tty queue); small helper or one-off `DebuggerCmd` subclass for probe parse if needed; `README.macos.md` |

### Safety / compatibility

| Concern | Assessment |
|---------|------------|
| I/O regression | Low: claim-off keeps the same dup-to-0/1/2 path that already works when claim fails. |
| Ctrl-C / job control | Unchanged vs today’s failed-claim reality (stays with Terminal’s session). Explicit off makes that intentional. |
| DIY `tty` users | Default `on` keeps warning when claim is expected. |
| Stock GDB without setting | Unsafe only if CB sends blindly; **capability detection required.** |
| Wrong platform | Gate CB side on macOS/Darwin + console tty path only. |
| Incompatibility with this GDB port | None inherent — port already special-cases Darwin claim failure; v1.1 adds an explicit skip. Must ship GDB change before CB depends on it. |

### Sequencing

```text
[this planning step] → implement+test GDB setting → implement+test CB handshake + README → v1.1 done → only then v2.0
```

---

## v2.0 — Integrated Program Console (later)

**Do not implement until v1.1 is done.** Terminal.app remains the supported console until then.

### Goals (unchanged thrust)

- Code::Blocks owns the **PTY master**; user program / inferior uses the **slave**.
- Run and Debug share the same Program Console UI and PTY service.
- Debugger log stays separate from inferior I/O.
- Pane inactive until Run or Debug needs a console; GUI targets unchanged.
- External Terminal remains a **fallback**.

### Design constraints

1. **PTY design** as above; “unclaimed slave removes Darwin `TIOCSCTTY` warning” is a **hypothesis to test at Milestone 4**, not an assumed fact.
2. **Explicit ownership:** CB must not accidentally claim the slave; close unnecessary slave fds; Run child creates session and claims/uses slave; Debug leaves slave unclaimed and uses GDB `set inferior-tty` / `tty`; no Terminal/`sleep` merely to keep the slave alive.
3. **PTY ≠ process manager.** Prefer a small session API:

   ```text
   CreateSession()
   GetMasterFd()
   GetSlavePath()
   SendInput()
   Resize()
   Close()
   ```

   `CompilerGCC` still launches normal programs; `DebuggerGDB` still issues GDB commands; the console service owns PTY lifecycle, I/O transport, and the pane.

4. **Shared infrastructure, separate launch:**

   - **Run:** allocate PTY → launch target with slave on 0/1/2 + correct controlling tty → master in CB.
   - **Debug:** allocate PTY → leave slave unclaimed → give path to GDB → GDB launches inferior → master in CB.

5. **Limited first console contract** (not a full terminal emulator): ordinary text; UTF-8 where practical; line input; prompts without trailing newline; Enter; Backspace; Ctrl-C; Ctrl-D/EOF; scrolling; window-size propagation; clean exit. Out of scope: full ANSI/VT100, curses, shells, full-screen apps.
6. **Local echo:** avoid double echo if both pane and PTY line discipline echo.
7. **Non-blocking master I/O** off the wx GUI thread; marshal output to UI; handle EOF, EINTR, exit, high volume; bound buffer growth.
8. **Resize** → propagate via terminal window-size mechanism to the slave.
9. Resolve resources relative to the running app/bundle, not fixed absolute paths.
10. External-terminal **fallback** if integrated PTY fails.
11. macOS first; Unix-shaped API OK; **do not** change Linux xterm path for symmetry alone.
12. No unrelated refactors; each milestone independently reviewable.

### Milestones (v2.0)

| # | Milestone | Intent |
|---|-----------|--------|
| 1 | Minimal PTY proof of concept | Outside deep CB integration: create PTY; launch small test program; verify output, prompt without newline, stdin, EOF, Ctrl-C, clean exit, resize. Prove Darwin semantics first. |
| 2 | Program Console pane + non-blocking transport | Idle pane; master read/write; echo/input policy; buffer limits. |
| 3 | Normal Run integration | Console targets via session API; in-pane exit wait; fallback; GUI targets untouched. |
| 4 | GDB inferior I/O | Replace Terminal+`sleep` debug console with acquire + `tty`/`inferior-tty`. **Explicitly test:** GDB accepts slave; whether Darwin claim warning disappears; stdio; breakpoints/continue; Ctrl-C; session restart resets console. |
| 5 | Lifecycle / fallback / layout / polish | Persistence, docs (`README.macos.md`), edge cases. |

### Current hooks (v2.0 reference)

- Debug: `cbDebuggerPlugin::RunNixConsole` → `DebuggerGDB` `tty …` (`src/sdk/cbplugin.cpp`, `src/plugins/debuggergdb/debuggergdb.cpp`).
- Run: `CompilerGCC` + `DEFAULT_CONSOLE_TERM` / `$SCRIPT` (`compilergcc.cpp`, `globals.cpp`).
- After v1.1: Darwin may also queue `set inferior-tty-claim off` for the Terminal path; v2.0 Debug may drop that if Milestone 4 shows claim succeeds on an unclaimed slave.

---

## Status

| Version | Status |
|---------|--------|
| **v1.1** | Planned; **next to implement** after this document. GDB command must be added — not present in tree yet. |
| **v2.0** | Planned only; **blocked** until v1.1 implementation and tests are complete. |
