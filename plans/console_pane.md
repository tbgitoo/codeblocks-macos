# Program console: v1.1 (Darwin tty), v1.2 (bundled GDB), v2.0 (integrated pane)

Three scopes. **Finish and test each version before starting the next.** Do not start v2.0 until v1.2 is complete.

```text
v1.1 (Terminal + inferior-tty-claim) → v1.2 (ship macOS GDB in the .app) → v2.0 (integrated PTY pane)
```

---

## Shared context

On macOS, Run and Debug both use **Terminal.app** today:

- **Run:** `osascript` + optional `cb_console_runner`.
- **Debug:** Terminal + bundled holder (`cb_debug_console_hold`), discover `/dev/ttys…`, GDB `tty …`.

Debug then hits a long Darwin warning unless GDB supports `set inferior-tty-claim off` (v1.1). **stdin/stdout/stderr still work.** Job control / Ctrl-C stay with the owning session.

Windows uses `set new-console on` (Win32 only). That path cannot be copied to Darwin.

Unlike Linux/Windows, **GDB is not generally available on macOS** (not on PATH by default; Apple’s toolchain is LLDB). Code::Blocks currently falls back to `cbDetectDebuggerExecutable("gdb")`, which fails or points at an unrelated install unless the user configures a path by hand. The Darwin-specific tty setting also lives only in the companion `binutils-gdb` port. **v1.2 ships that GDB inside `CodeBlocks.app` and makes it the macOS default** before any PTY architecture work in v2.0.

Companion GDB lives in the sister repo **`binutils-gdb`** (sibling directory next to `codeblocks-macos`, e.g. `../binutils-gdb`). Use paths relative to that layout — **never hardcode a developer home directory**. Packaging input is the staging **prefix**, not only the executable (see v1.2 runtime table):

```text
../binutils-gdb/staging/bin/gdb
../binutils-gdb/staging/share/gdb/
# plus license-permitted non-system dylibs (today: Homebrew zstd, mpfr, gmp)
# → CodeBlocks.app/Contents/Resources/debugger/{bin,share,lib}/
```

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

**Verified against sister `binutils-gdb` (local tip `ce3944268b7`, version `19.0.50.DATE-git`):**

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
5. Document in `README.macos.md`: paired Darwin GDB; CB sets `inferior-tty-claim off` for the Terminal+holder console so I/O stays on that tty without attempting a claim Darwin will refuse.

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

### Sequencing (v1.1)

```text
[plan] → implement+test GDB setting → implement+test CB handshake + README → v1.1 done → v1.2
```

---

## v1.2 — Bundle Darwin GDB in CodeBlocks.app (before PTY rework)

**Do not implement until v1.1 is done.** **Do not start v2.0 until v1.2 is done.** Terminal.app remains the supported console; this version only makes the **correct GDB** available and the **default on macOS**.

### Why before v2.0

v2.0’s Debug milestone still depends on a Darwin GDB that understands inferior tty behaviour (and after v1.1, `inferior-tty-claim`). Shipping the sister-repo binary in the `.app` removes “install GDB yourself” as a prerequisite for macOS users and keeps Debug (Terminal today, PTY later) on a known-good debugger. Reworking the PTY pathway while users still have no default GDB would leave Debug broken on a fresh Mac.

### Scope

1. **macOS only:** default debugger executable → **bundled GDB inside `CodeBlocks.app`**.
2. **Linux / Windows:** no behaviour change. Keep existing PATH / config detection (`cbDetectDebuggerExecutable("gdb")` and user `executable_path`). Do not bundle GDB for those platforms in this version.
3. Source from the sister repo staging **prefix** (relative to `codeblocks-macos`): at least `../binutils-gdb/staging/bin/gdb` plus the runtime pieces below. Packaging must resolve that sibling tree (or an explicit override env var); **never embed a user home path**.
4. Copy those pieces into the app during `make_cb_app_bundle.sh` (extend the same pattern as `cb_console_runner` / `cb_debug_console_hold`).
5. Code::Blocks on macOS resolves the default to a **bundle-relative** path under `Contents/Resources/debugger/bin/gdb` (not an absolute staging path, and not beside `codeblocks` in `MacOS/`).
6. Users may still override Debugger → Executable path; empty / unset → bundled default on macOS only.
7. Update `README.macos.md`: where GDB comes from, what else is shipped, license notices for any vendored dylibs, how to rebuild/replace it, codesign notes. Document that existing debugger/Terminal settings are **not** auto-migrated — clear or edit them by hand if switching to the bundle.
8. Size: packaging + macOS default-path wiring; no PTY/console architecture changes.
9. **No config migration in any release.** Do not write one-way rewrites of `executable_path`, `console_terminal`, or similar. During v1.2 implementation, **remove** the existing macOS Terminal migration block in `src/src/app.cpp` (the `~/.codeblocks_run.sh` / bare / clear-exit-no-reuse rewrite). Operators who still have obsolete settings update them manually (or delete `default.conf` keys).

### Non-goals (v1.2)

- No integrated Program Console / PTY service (that is v2.0).
- No change to Run/Debug Terminal handshake beyond using the bundled debugger.
- No Linux/Windows packaging of GDB.
- No requirement that `binutils-gdb` be a git submodule of `codeblocks-macos` (sister checkout is enough); document the expected layout.
- Do not commit large binary blobs into `codeblocks-macos` git history if the bundle script can copy from the sibling staging tree at package time (preferred).
- Do not ship `gdb-add-index` / `gstack` unless a later need appears (shell helpers; not used by Code::Blocks Debug).
- Do not ship staging `lib/*.a`, `include/`, `share/man`, `share/info` (build/docs only).
- **No automatic migration** of debugger executable path, Terminal command, or other user settings (including removal of any leftover v1.1-era Terminal migration code).

### Runtime requirements (verified against current `../binutils-gdb/staging`)

Probed the present Darwin staging build (`gdb` ~13 MB, `--without-python`, data-directory marked **relocatable**). Findings for v1.2 packaging:

| Need | Verdict | Detail |
|------|---------|--------|
| `staging/bin/gdb` | **Required** | The debugger itself. |
| `staging/share/gdb/` | **Required for a relocatable install** | ~432 KB (`syscalls/*.xml`, `system-gdbinit/*.py`). Relocatable rule: from `$prefix/bin/gdb`, GDB resolves `$prefix/share/gdb`. If only the binary is copied, `show data-directory` falls back to the **compile-time absolute** staging path (a developer machine path) — wrong on end-user Macs. Core `start`/`print` can still work with a missing data-dir, but shipping `share/gdb` next to `bin/` (correct relative layout) is required for a clean default. This build has **no** Python support, so the `.py` files are unused; still copy the whole `share/gdb` tree. |
| Non-system dylibs | **Required at runtime; ship only if license allows** | `otool -L` links absolute Homebrew paths: `libzstd.1`, `libmpfr.6`, `libgmp.10` (mpfr → gmp). System libs stay on the OS. Without these three, a Mac that lacks Homebrew cannot load `gdb`. **License check (Homebrew formulae / upstream):** zstd `(BSD-3-Clause OR GPL-2.0-only) AND BSD-2-Clause AND MIT` — redistributable with notices; mpfr `LGPL-3.0-or-later` — redistributable as a replaceable shared library with LGPL notices/source offer; gmp `LGPL-3.0-or-later OR GPL-2.0-or-later` — same, dual-licensed. **Conclusion: permitted to vendor** into the app. Ship them under `Resources/debugger/lib/` (not `MacOS/`), rewrite install names to `@loader_path` / `@rpath`, and include license text (e.g. `Resources/debugger/licenses/`). If a future dep fails that bar, do **not** copy it — fix linkage in `binutils-gdb` (static or otherwise) instead. |
| `staging/lib/*.a` | **Not required at runtime** | Static archives already linked into `gdb`; no `lib/gdb`, `lib/debug`, or `bfd-plugins` present in staging. |
| Python / pretty-printers | **N/A for this build** | Configured `--without-python`. Revisit if a future GDB build enables Python. |
| `gdb-add-index`, `gstack` | **Not required** for CB Debug | Optional scripts in `staging/bin/`. |

**In-app layout** (keeps helpers in `MacOS/`, debugger tree self-contained under `Resources/`; preserves GDB’s `bin` → `../share/gdb` relocation):

```text
CodeBlocks.app/
  Contents/
    MacOS/
      codeblocks
      cb_console_runner
      cb_debug_console_hold
    Resources/
      debugger/
        bin/
          gdb
        share/
          gdb/          # from staging/share/gdb
        lib/            # only license-permitted dylibs (zstd, mpfr, gmp today)
        licenses/       # COPYING / LICENSE excerpts for vendored libs + GDB notice
```

| Item | Build-machine input | In `CodeBlocks.app` |
|------|---------------------|---------------------|
| Debugger | `../binutils-gdb/staging/bin/gdb` | `Contents/Resources/debugger/bin/gdb` |
| Data dir | `../binutils-gdb/staging/share/gdb/` | `Contents/Resources/debugger/share/gdb/` |
| Dep dylibs | Homebrew cellar `libzstd.1`, `libmpfr.6`, `libgmp.10` (until GDB links differently) | `Contents/Resources/debugger/lib/` + install-name fixups on `gdb` and those dylibs |
| Optional override | e.g. `CB_BUNDLE_GDB_PREFIX=../binutils-gdb/staging` | — |

From `Resources/debugger/bin/gdb`, relocatable data-directory resolves to `Resources/debugger/share/gdb` automatically — no `--data-directory` flag required.

Default executable path for Code::Blocks: bundle-relative  
`…/Contents/Resources/debugger/bin/gdb`  
(resolve via the app’s Resources/data folder API, **not** `GetExecutableFolder() + "/gdb"`).

Bundle script checks: `gdb` exists and is executable; `share/gdb` exists; after copy, `otool -L` on bundled `gdb` must not retain `/opt/homebrew/...` or `/Users/...` dependency paths; license files for every vendored dylib are present. Codesign `gdb` and vendored dylibs. Fail the bundle with a clear error if the sister staging tree is missing.

### Code::Blocks default (macOS only)

Today (`DebuggerConfiguration::GetDebuggerExecutable`):

- Read `executable_path` from debugger config.
- If empty → `cbDetectDebuggerExecutable("gdb")` (PATH search).

v1.2 change:

| Platform | Empty `executable_path` means |
|----------|-------------------------------|
| macOS | Bundled `Resources/debugger/bin/gdb` if that file exists; else fall back to PATH detection. |
| Linux / Windows | Unchanged: PATH / existing detection only. |

If `executable_path` is already set (e.g. an old absolute staging path), **leave it unchanged**. No rewrite, no “looks like our old path” heuristics. Clear the field locally to pick up the bundled default.

### Implementation plan (when implementing)

**A. `binutils-gdb` (prerequisite, not a CB code change)**

1. Ensure staging install includes a runnable `staging/bin/gdb` with v1.1 `inferior-tty-claim`, plus `staging/share/gdb/`.
2. Ideally: link/install so `gdb` does not depend on absolute `/opt/homebrew/...` dylibs (static or `@rpath` into staging). Until then, the CB bundle script vendors+rewrites only **license-permitted** dylibs (zstd/mpfr/gmp today).
3. Confirm a **relocated** copy under `Resources/debugger/` (no access to the original staging absolute path) still starts: `show data-directory` is `…/Resources/debugger/share/gdb`; `otool -L` has no Homebrew/user prefixes.

**B. `codeblocks-macos` packaging**

1. `make_cb_app_bundle.sh`: resolve sibling staging prefix (and honor optional override); copy into `Contents/Resources/debugger/{bin,share,lib,licenses}` as in the layout above; install-name fixups so `gdb` loads dylibs from `debugger/lib/`; codesign Mach-O pieces. Leave `MacOS/` for `codeblocks` + console helpers only.
2. Fail fast if sister GDB or `share/gdb` is missing, or if dependency rewrite left Homebrew paths behind.
3. README: sibling layout, tree diagram, license notices, rebuild GDB, re-run bundle script; note that settings are not auto-migrated.

**C. `codeblocks-macos` debugger default + remove migrations**

1. In `DebuggerConfiguration::GetDebuggerExecutable` (or a thin macOS helper): when `platform::macosx` and config path empty, prefer `Resources/debugger/bin/gdb` if present.
2. Environment / Settings UI: show the resolved path; combo/default list may include the bundled path on macOS.
3. No `#ifdef` changes to Linux/Windows branches beyond staying on the existing empty→PATH path.
4. **Delete** the macOS `console_terminal` migration block in `src/src/app.cpp` (`.codeblocks_run.sh` / obsolete osascript rewrites). Do not add a replacement migration for GDB or Terminal.

**D. Test plan (v1.2)**

1. Fresh Mac-like config (empty debugger path): Debug starts `Resources/debugger/bin/gdb`; `show inferior-tty-claim` works; v1.1 handshake still silences Darwin warning.
2. On a tree **without** the original staging path and **without** Homebrew mpfr/gmp/zstd on the default load path: bundled `gdb -nx -batch -ex 'show data-directory' -ex 'show configuration'` succeeds; data-directory is `…/Resources/debugger/share/gdb`; no dyld “library not loaded” for vendored libs.
3. Custom `executable_path`: still honored.
4. Bundle without sibling GDB / share: script errors clearly — do not silently ship a half-broken default.
5. Linux/Windows build: no new default, no bundle dependency on `binutils-gdb`.
6. Codesigned app launches Debug without “debugger killed” / library-load failures from the bundled binary.

### Files / functions likely touched

| Area | Files |
|------|--------|
| Packaging | `make_cb_app_bundle.sh`; possibly `bundle.sh` / `README.macos.md` |
| Default path | `src/plugins/debuggergdb/debuggeroptionsdlg.cpp` (`GetDebuggerExecutable`); maybe small macOS helper near `cbDetectDebuggerExecutable` |
| Remove migrations | `src/src/app.cpp` (delete macOS Terminal `console_terminal` migration) |
| Docs | `README.macos.md`; this plan’s status |

### Safety / compatibility

| Concern | Assessment |
|---------|------------|
| Linux/Windows | Out of scope; no default or packaging change. |
| User override | Configured `executable_path` wins; never auto-rewritten. |
| Missing sister repo at bundle time | Hard fail (or explicit skip flag) — better than a default that points nowhere. |
| Stale absolute paths in config | Out of scope for code — user clears/edits debugger or Terminal settings manually. |
| Binary size / notarization | `gdb` ~13 MB + ~1.5 MB dylibs + ~0.4 MB `share/gdb`; expect longer bundle and codesign; document. |
| Homebrew-only linkage | Vendor+rewrite license-permitted dylibs under `Resources/debugger/lib/`, or end users without mpfr/gmp/zstd cannot start GDB. |
| License | Ship GDB under its GPL terms. Vendor zstd (BSD/MIT dual) and mpfr/gmp (LGPL-or-later) only with required notices / LGPL compliance (licenses dir; ability to replace dylibs). Do not vendor any future dep whose license forbids redistribution. |

### Sequencing (v1.2)

```text
v1.1 done → build sibling staging gdb → bundle into .app → macOS empty-path default → README/tests → v1.2 done → only then v2.0
```

---

## v2.0 — Integrated Program Console (later)

**Do not implement until v1.2 is done.** Terminal.app remains the supported console until then. Assume macOS Debug already uses the **bundled sister-repo GDB** from v1.2.

### Goals (unchanged thrust)

- Code::Blocks owns the **PTY master**; user program / inferior uses the **slave**.
- Run and Debug share the same Program Console UI and PTY service.
- Debugger log stays separate from inferior I/O.
- Pane inactive until Run or Debug needs a console; GUI targets unchanged.
- External Terminal remains a **fallback**.

### Design constraints

1. **PTY design** as above; “unclaimed slave removes Darwin `TIOCSCTTY` warning” is a **hypothesis to test at Milestone 4**, not an assumed fact.
2. **Explicit ownership:** CB must not accidentally claim the slave; close unnecessary slave fds; Run child creates session and claims/uses slave; Debug leaves slave unclaimed and uses GDB `set inferior-tty` / `tty`; no Terminal/holder merely to keep the slave alive.
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
9. Resolve resources relative to the running app/bundle, not fixed absolute paths (same rule as v1.2’s bundled gdb).
10. External-terminal **fallback** if integrated PTY fails.
11. macOS first; Unix-shaped API OK; **do not** change Linux xterm path for symmetry alone.
12. No unrelated refactors; each milestone independently reviewable.
13. Continue using the **bundled macOS GDB** from v1.2; do not reintroduce “user must install GDB” as a v2.0 requirement.

### Milestones (v2.0)

| # | Milestone | Intent |
|---|-----------|--------|
| 1 | Minimal PTY proof of concept | Outside deep CB integration: create PTY; launch small test program; verify output, prompt without newline, stdin, EOF, Ctrl-C, clean exit, resize. Prove Darwin semantics first. |
| 2 | Program Console pane + non-blocking transport | Idle pane; master read/write; echo/input policy; buffer limits. |
| 3 | Normal Run integration | Console targets via session API; in-pane exit wait; fallback; GUI targets untouched. |
| 4 | GDB inferior I/O | Replace Terminal+holder debug console with acquire + `tty`/`inferior-tty`. **Explicitly test:** bundled GDB accepts slave; whether Darwin claim warning disappears; stdio; breakpoints/continue; Ctrl-C; session restart resets console. |
| 5 | Lifecycle / fallback / layout / polish | Persistence, docs (`README.macos.md`), edge cases. |

### Current hooks (v2.0 reference)

- Debug: `cbDebuggerPlugin::RunNixConsole` → `DebuggerGDB` `tty …` (`src/sdk/cbplugin.cpp`, `src/plugins/debuggergdb/debuggergdb.cpp`).
- Run: `CompilerGCC` + `DEFAULT_CONSOLE_TERM` / `$SCRIPT` (`compilergcc.cpp`, `globals.cpp`).
- After v1.1: Darwin may also queue `set inferior-tty-claim off` for the Terminal path; v2.0 Debug may drop that if Milestone 4 shows claim succeeds on an unclaimed slave.
- After v1.2: macOS default debugger is `Contents/Resources/debugger/bin/gdb` from sister `binutils-gdb` staging.

---

## Status

| Version | Status |
|---------|--------|
| **v1.1** | **Implemented (uncommitted).** GDB `inferior-tty-claim`; CB probe; bundled `cb_console_runner` + `cb_debug_console_hold` replace `~/.codeblocks_run.sh`/`sleep`; SIGUSR1 soft finish vs SIGTERM force (CB never waits on keypress); cold-start single-window default Terminal command. (Temporary Terminal migration code may still be present — **remove in v1.2**.) |
| **v1.2** | **Planned only.** Bundle sister-repo staging into `Contents/Resources/debugger/{bin,share,lib}`; macOS-only default `Resources/debugger/bin/gdb`; license-permitted dylibs only (`zstd`/`mpfr`/`gmp`); Linux/Windows unchanged; **no config migration** (remove Terminal migration from `app.cpp`; GDB path left to the user). **Blocked** until v1.1 is considered complete. **Blocks v2.0.** |
| **v2.0** | Planned only; **blocked** until v1.2 is complete. |
