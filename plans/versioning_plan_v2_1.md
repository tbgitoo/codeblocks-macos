# v2.1 — Debug a single open file (no project)

Detail plan for this version. Overview and status table: [versioning_plan.md](versioning_plan.md).

---

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

