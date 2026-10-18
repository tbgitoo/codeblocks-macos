# v2.2 — Standalone New → Save as C|C++ → Build / Run / Debug

Detail plan for this version. Overview and status table: [versioning_plan.md](versioning_plan.md).

---

**Status: DONE** (tree) — **M1**–**M6** in UI / tree. Tag/release when ready. See **Lessons learned** below.

**Depends on:** v2.1 standalone Debug (leave `DebuggeeDescription` / DebuggerGDB architecture intact; only adjust **routing gates** that formerly keyed off “any active project”).

**Goal:** Make the standalone-source workflow coherent:

```text
New → choose/save as C or C++ → edit → Build / Build and Run / Debug
```

while project-owned sources keep the existing project workflow. A project may be open without forcing every Build/Run/Debug onto that project.

Defects addressed by this version: (1) **M2/M3** — Build / Build and Run / Debug follow **active-source ownership** and post-save identity, not mere project existence or a stale Untitled path; (2) **M1** — Save As exposes separate **C files** / **C++ files** (not a combined filter whose first mask is `*.c`); (3) **M4** tab Rename; (4) **M5** unbuffered Debug stdout; (5) **M6** readable Apple-libc++ `std::string` Watches.

### Product decisions (locked)

| Topic | Decision |
|-------|----------|
| **Routing** | **Active-source ownership**, not mere project existence. See rule below. |
| Project-owned source | Existing project Build / Run / Debug **unchanged** (except empty-project / save gates below). |
| Standalone source (incl. with a project open) | Standalone Build / Build and Run / Debug on **that** file — never the unrelated project. |
| Unsaved new buffer | Save As first (PREPARE); cancel → **pre-COMMIT** (≡ never requested); on success classify **post-save** path and route. |
| C vs C++ Save As | Separate **C files** / **C++ files** / **C/C++ header files**; one shared `C/C++` lexer. |
| Legacy filter preference | Stored `"C/C++ files"` → migrate to **`"C files"`** (preserves legacy `.c`-first default). |
| Fresh / empty config default | **Independent** of migration: initial filter **`"C++ files"`** (`.cpp` first). Not a reason to force all new files to C. |
| v2.1 Debug | Preserve `DebuggeeDescription` / DebuggerGDB hand-off; update **candidate / branch gates** to the ownership rule. |
| **Operation lifecycle** | **PREPARE → COMMIT**; **Transaction** + **Launch Identity** (+ Retry) below. REQUIRED for M2/M3 acceptance (not symptom-level fixes alone). **M2** / **M3** are DONE in tree after the invariant tests below. |
| Scope | Minimal; reuse `cbEditor::Save` / Save As; no beginner mode, fake projects, parallel build system, or new transaction framework unless existing CB code cannot satisfy the invariants. |

Notes on operation lifecycle:

- PREPARE need not prove that an output executable already exists when a build is required; it must establish the intended output identity and the operation that will produce it. After asynchronous build success, the continuation may launch only that prepared output identity.

- Before COMMIT, know exactly what is to be built/launched and why. After a required build succeeds, launch only the resulting artefact belonging to that prepared operation.

#### Operation lifecycle (PREPARE / COMMIT) — REQUIRED

Locked after empty-project leftover-binary bugs and lone-file “cancel Save As still debugs the previous exe” observations. These are **architectural** acceptance criteria, not reproductions to special-case.

Applies to **Build and Run**, **Run**, **Debug**, and **Step Into** (and any path that arms build-then-launch / build-then-debug).

##### Two phases

```text
PREPARE  — establish the exact operation the user requested
COMMIT   — only after successful preparation may Build/Run/Debug execution state be mutated
```

**PREPARE** includes, as applicable:

- identify active editor / buffer
- Save / Save As (normal persistence to establish identity)
- honour cancellation
- reacquire the post-save filename
- validate that filename / path
- classify `FileTypeOf(path)`
- determine project membership from the **post-save** identity
- select **exactly one** route: Project or Standalone
- validate that the selected route has something valid to build / run / debug
- determine required build / rebuild
- determine launch identity / output executable / debuggee
- ensure that identity belongs to the **operation currently being requested**

Preparation may perform Save / Save As. Preparation must **not** yet mutate execution / session state associated with starting Build and Run, Run, Debug, or Step Into.

**COMMIT** occurs only when PREPARE has succeeded.

Immediately before COMMIT, the implementation must know enough to state:

> This exact requested operation will act on this exact current source/project identity through this exact Project or Standalone route.

Only after that statement is true may execution / session state be reserved.

Examples of state that belongs **after** COMMIT (not an exhaustive checklist of “do all at once”):

- `GetIsRunning` / debugger running reservation
- debugger log / session clearing performed specifically for the new launch
- automatic Step Into / bootstrap breakpoint creation
- `WaitingCompilerToFinish`
- pending build-complete-to-debug continuation
- pending Build-and-Run continuation
- GDB `start` / `run`
- Program Console process launch

None of these may be **caused** by an operation whose preparation later returns Cancelled. They need not all happen at the COMMIT instant; the invariant is ownership of causality.

Preferred architecture:

```text
PREPARE → success → COMMIT → mutate execution state
```

Not:

```text
mutate execution state → attempt PREPARE → roll back on cancel
```

Do **not** define correctness as “we set state and clean it up on cancel.” Rollback after COMMIT may still be needed for asynchronous build failures; ordinary Save / Save As cancellation must occur **before** COMMIT.

##### A. Operation Transaction Invariant (REQUIRED)

> Cancellation before COMMIT is observationally equivalent to the user never having requested the operation.

For Debug / Step Into / Run / Build and Run, a cancelled (or otherwise pre-COMMIT failed) preparation must **not** cause:

- compiler launch
- program launch
- debugger launch
- Continue of an existing / prior inferior
- creation of automatic / temporary breakpoints
- creation of orphan editor breakpoint markers
- `WaitingCompilerToFinish`
- pending rebuild-finish-to-launch continuation
- pending run-after-build continuation
- pending Debug / Step Into continuation
- selection of a fallback project executable
- launch of a leftover executable on disk
- changed behaviour of the next invocation

##### B. Launch Identity Invariant (REQUIRED)

> Every Build / Run / Debug operation acts only on an identity successfully established for the **current** request.

Never derive the operation target from:

- a previous Debug session
- a previous inferior
- a previous standalone executable
- a leftover project executable on disk
- the pre-SaveAs Untitled path
- “whatever GDB last had”
- project existence when the active source is unrelated
- any stale launch / debug description from a previous request

Conceptually:

```text
current request
  → successful PREPARE
  → current post-save identity
  → explicit route (Project | Standalone)
  → current build / launch identity
```

If that chain cannot be established, the operation does **not** start (no COMMIT).

##### Empty project is an identity test (not a special case)

An executable existing on disk is **not** sufficient proof that it is a valid target for the current request. Therefore:

```text
project has no valid sources / current build identity
  + leftover executable exists
```

must **not** imply Run / Debug of that leftover. Correctness is refusing an invalid current identity — **not** deleting old executables to make the test pass. **V1** / **V2** exercise the Launch Identity Invariant.

##### Pre-COMMIT cancel vs post-COMMIT failure

These are different lifecycle outcomes — do not blur them under one vague “failure.”

| Outcome | When | Result |
|---------|------|--------|
| **Pre-COMMIT** | Cancelled; invalid target; unable to establish routing / identity | No operation was started; no launch / debug continuation exists; no previous target is substituted |
| **Post-COMMIT** | Compiler accepted then compile fails; process / debugger launch fails after acceptance; inferior terminates / fails | Operation genuinely started; normal async completion / failure cleanup; continuation consumed / cancelled **exactly once**; return to a valid idle state when appropriate |

##### Retry Invariant (REQUIRED)

> A cancelled pre-COMMIT request is **indefinitely repeatable**.

For any \(N \ge 1\): request Step Into (or Run / Build and Run / Debug) → Save As appears → cancel — repeated \(N\) times — must produce the **same** observable result. Request \(N+1\) must behave like request 1.

Automatically a failed invariant:

- first cancel → apparently OK; second request → different route
- second request → phantom breakpoint
- second request → previous executable
- second request → no Save As
- second request → no-op
- second request → Continue prior inferior

No implementation may contain second-attempt-specific repair logic.

##### Asynchronous continuation ownership (REQUIRED)

If an operation COMMITs and then starts an asynchronous compiler operation: **exactly one** pending continuation owns the result.

| Compiler outcome | Required |
|------------------|----------|
| Success | Consume continuation **exactly once** → launch the **prepared** Run / Debug / Step Into |
| Failure (after accept) | Consume / cancel continuation **exactly once** → do not launch → valid idle state |
| Never accepted / never started | Do **not** leave `WaitingCompilerToFinish` or a launch continuation |

A continuation must retain / identify the **prepared** operation. It must never silently resolve later against whichever project / editor / executable happens to be current. Prefer existing Code::Blocks mechanisms; do not require a new framework or state machine if they can satisfy this.

##### Former violation (lone-file) — **resolved for M2 / M3 (observational)**

Historically the Debug menu reserved `GetIsRunning` / cleared the log **before** Save As (mutate-before-PREPARE), and cancel could appear to Continue a prior inferior or leave bootstrap state. **M2** (Run / Build and Run) and **M3** (Debug / Step Into / Run to Cursor) now satisfy the Transaction + Launch Identity + Retry invariants in practice: cancel Save As ≡ never requested (**F**, **P**, **U1–U4**, **E′**); no leftover run/debug continuation; no phantom bootstrap BP; Stop remains usable for a live standalone session when focus leaves the editor. Menu order may still set a running slot briefly before `Debug()` returns false on cancel — that is acceptable only while the **observable** outcome stays ≡ never requested (verified). Do not reopen this as an open bug without a failing U/E′ case.

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
| C | Project open + Untitled → Save As → `.cpp` not in project | Standalone on saved file; cancel = pre-COMMIT (≡ never requested) |
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
| Debug, project open | Ownership gate: standalone when active `ftSource` is not a project member (empty project does not suppress) | Same ownership rule as Build |

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

#### B. Shared save / classify / route (illustrative — PREPARE only)

```text
PrepareActiveSourceOperation(pathOut, routeOut) -> bool
  // PREPARE only: no GetIsRunning, no WaitingCompilerToFinish, no launch continuation
  ed = builtin active editor (fail if none)
  if !ed->Save() -> false                    // cancel / fail → pre-COMMIT
  pathOut = ed->GetFilename()
  if !IsOK or path empty -> false
  if FileTypeOf(pathOut) != ftSource -> false // clear message
  if BelongsToActiveProject(ed, pathOut)
       routeOut = Project
  else
       routeOut = Standalone
  // caller COMMITs only after this returns true and launch identity is known
  return true
```

#### C. Wire Build / Build and Run / Run

| Call site | Change |
|-----------|--------|
| `CompilerGCC::DoBuild` / `Build` entry | **Not** `if (CheckProject()) project else untitled`. PREPARE then COMMIT: **Project** → existing project `DoBuild`; **Standalone** → `CompileFileWithoutProject(pathOut)` (post-save path only). |
| `CompileFileWithoutProject` | Never compile a path captured before Save; honour Save failure (pre-COMMIT cancel). |
| `Run` / Build and Run | Same routing; Run after compile uses **active editor’s post-save** filename / standalone exe. Honour **Transaction** + **Launch Identity**: PREPARE → COMMIT → only then arm run-after-build / launch. Cancelled Save As → no Run, no leftover exe, no pending continuation. |
| Async Build-and-Run | Exactly one continuation owns the prepared identity; consume once on success or failure; never resolve against a later current editor/project. |
| Empty project / no sources | Refuse / disable Run (Launch Identity — leftover `bin/…` is not a valid current target). Do not delete old exes to pass the test. |
| Project-owned path | Existing project machinery only — no standalone ensure beyond normal project saves. |

#### D. Wire Debug (v2.1-compatible)

| Call site | Change |
|-----------|--------|
| `IsStandaloneDebugCandidate` | Replace `if (GetActiveProject()) return false` with: never-saved OK **or** (`ftSource` **and** not `BelongsToActiveProject`). |
| `DebuggerGDB::Debug` | Keep additive branches; ownership routing. PREPARE (save → route → rebuild identity) **before** COMMIT (`SetIsRunning` / log clear / `DoDebug` / `Continue`). Launch Identity: debuggee only from successful current preparation. |
| `DebuggerMenuHandler::OnStart` / `OnStep` | No running-slot reservation, log clear, or session mutate until PREPARE succeeds. Cold Start must not `Continue` a prior inferior when the user intended a new prepare. |
| Step Into / bootstrap BP | An automatically created Step Into / bootstrap breakpoint is **committed debugger-session state**. It must **not** exist before PREPARE succeeds. Untitled → Step Into → Save As → Cancel → **zero** automatic BP objects and **zero** automatic markers. Prevent premature creation — do not “create then delete on cancel.” |
| `SaveActiveSourceForStandaloneDebug` | Keep save → `GetFilename()` invariant; may share prepare helper (**optional** dedup). Cancel → false all the way up (pre-COMMIT). |
| Empty project / no sources | Gate the **project** route only (refuse leftover project exe). Valid active standalone source remains eligible (V2 / V3). Same for Run. |
| Build-before-debug continuation | Same async ownership as Build-and-Run: prepared operation retained; consume exactly once. |
| `DebuggeeDescription` | **Unchanged** type / MakeStandalone; identity only from successful current post-save path. |

Menu UpdateUI already uses `IsStandaloneDebugCandidate` — fixing that gate fixes enablement for project-open + standalone.

### Proposed change table

| # | File(s) | Class / function | Current | Proposed | Why narrowest | v2.1 | Risk |
|---|---------|------------------|---------|----------|---------------|------|------|
| 1 | `editorcolourset.cpp` (± filefilters) | `SetFileMasks` | One `"C/C++ files"` | **C** / **C++** / **header** filters; drop combined | Filter birthplace; one lexer | None | Med |
| 2 | `cbeditor.cpp` | `SaveAs` | Default / remember `"C/C++ files"` | Fresh default **`C++ files`**; migrate **`C/C++ files` → `C files`**; persist new name | Only preference I/O | None | Low–med |
| 3 | `compilergcc.cpp` (+ small helper) | `DoBuild`, `Run`, `CompileFileWithoutProject` | **M2 DONE:** PREPARE→COMMIT; ownership; `m_RunAfterCompile` identity | (complete) | Bug locus | Align Build with Debug | — |
| 4 | `debuggeedescription.cpp`, `debuggergdb.cpp`, `debuggermenu.cpp` | Candidate / `Debug` / OnStart / OnStep / UpdateUI | **M3 DONE:** ownership gates; cancel/retry (**U1–U4**); Stop with non-editor focus; same DebuggeeDescription | (complete) | Gate / ordering only | Soft touch | — |
| 5 | README / ChangeLog / this plan | docs | — | C\|C++ Save, migration, ownership routing | Release | Mentions Debug | Low |

**Abstain:** redesigning `DebuggeeDescription`; new debugger architecture; breakpoint subsystem redesign; fake projects; lexer split; global `.c`→`.cpp`; parallel build system; new generic transaction framework unless existing CB mechanisms cannot satisfy the invariants.

### Regression analysis

| Area | Risk | Mitigation |
|------|------|------------|
| Project-owned `.c`/`.cpp` | Med (routing change) | Tests **M**; `BelongsToActiveProject` true → untouched project path |
| Standalone with project open | Med (new REQUIRED) | Tests **N–Q** |
| Standalone no project | Low–med | Save invariant; post-save path |
| Cancel Save As (pre-COMMIT) | Must ≡ never requested | Transaction invariant; tests **F**, **P**, **U1–U4**, **V1–V2** |
| Lone-file after prior Debug | Med (stale session / exe / BP) | PREPARE→COMMIT; Launch Identity; retry; **U1–U3** |
| Empty project leftover exe | Med | Launch Identity (not “delete bin”); **V1–V2** |
| Repeated cancel | Med (state accumulates) | Retry invariant; **U2**, **U4** |
| Async build→launch continuation | Med (wrong identity later) | Exactly-once consume of prepared op; M2/M3 criteria |
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
| **U1** | No project: Debug saved file A OK → end/close A → New Untitled → **Step Into** → **Cancel** Save As | Nothing launches or continues; A’s exe not reused; prior inferior not Continued; **no** automatic BP object/marker; ≡ never pressed Step Into |
| **U2** | Immediately repeat **U1** cancellation **10** times | Save As every time; identical behaviour; no state / BP / exe accumulates; no request becomes a no-op because an earlier one was cancelled |
| **U3** | After **U2**, Step Into once more → successfully Save As standalone `.cpp` | Newly saved path authoritative; correct standalone exe built; **exactly this** file enters debugger; previous exe/session irrelevant |
| **U4** | Project open → New Untitled → Step Into → Cancel Save As **repeatedly** | Unrelated project never built/launched/debugged; no fallback project exe; no phantom BP |
| **V1** | Empty project + **no** valid active standalone source + stale project exe on disk → **Run** | Refuse/disable; stale binary **not** launched (Launch Identity — do not delete exe to pass) |
| **V2** | Same as **V1** → **Debug** / Step Into | Refuse/disable; stale binary not debugged; no prior-inferior Continue |
| **V3** | Empty project `new_test` (no sources; stale exe may remain) + active saved standalone `Untitled2.cpp` (not a project member) | **UI:** Debug / Step Into / Build / Build and Run enabled for standalone. **Build and Run:** prepares `Untitled2.cpp`, standalone build+run; `new_test` not built; stale project exe not run. **Debug / Step Into:** prepares `Untitled2.cpp`, v2.1 `DebuggeeDescription` path; standalone rebuild as required; `new_test` not built/debugged; stale project exe unused; prior inferior not Continued |

**U** / **V** above are architectural acceptance tests for Transaction + Launch Identity + retry — not cases to special-case in isolation. Retain **A–T** as before.

**V1 / V2 / V3 distinction (REQUIRED):** empty project + no valid standalone → refuse; empty project + valid active standalone → standalone operation. Empty-project protection gates the **project route only**.

### Milestones

| # | Status | Work |
|---|--------|------|
| **M1** | **DONE** (tree) | Split C / C++ / header filters; drop combined name; legacy → **C files**; fresh default **C++ files**; tests A/B/K/R/S/T |
| **M2** | **DONE** (tree) | Run-side PREPARE→COMMIT + ownership. See **M2 completion** below. |
| **M3** | **DONE** (tree) | Debug-side same invariants. See **M3 completion** below. Manual **U1–U4**, **E** / **E′**, **Q**, **V2**, **V3** passed; Run to Cursor cancel + Stop with focus off editor also OK. |
| **M4** | **DONE** (tree) | **Editor-tab Rename** (complement). See below. |
| **M5** | **DONE** (tree) | **Unbuffered inferior stdout** via companion GDB. See below. |
| **M6** | **DONE** (tree) | **Human-readable C++ Watches** (Apple-libc++ `std::string`). See below. Independent of M1–M5 lifecycle work. |

#### Lessons learned (v2.2 — carry forward)

Architectural and product lessons from shipping M1–M6. Prefer these over re-deriving from symptoms.

| Area | Lesson |
|------|--------|
| **Lifecycle (M2/M3)** | Symptom patches (clear a flag here, delete a BP there) do not fix cancel-still-runs / leftover-exe bugs. **PREPARE → COMMIT**, **Transaction**, **Launch Identity**, and **Retry** are the acceptance criteria. Pre-COMMIT cancel must be observationally ≡ never requested. |
| **Ownership** | Route by **active-source ↔ active-project membership**, not “is any project open?” An empty project must refuse leftover **project** binaries without blocking a valid **standalone** editor (**V1/V2** vs **V3**). |
| **Async continuations** | Run-after-compile / build-before-debug must retain the **prepared** identity and consume it exactly once. Never re-resolve against whichever editor/project is current later. |
| **Silent / bootstrap BP (M3)** | An automatic Step Into / start breakpoint is committed session state — do not create it before PREPARE succeeds. A `silent` start BP that auto-continues can leave Code::Blocks without a stack cursor (**“In ()”**); prefer a visible stop the IDE can bind. |
| **Filters (M1)** | Keep **one** `C/C++` lexer; split filter **names** only. Migration (`"C/C++ files"` → `"C files"`) and fresh default (`"C++ files"`) are **independent** decisions — do not conflate them. Drop the combined filter name so remembered prefs cannot point at a dead entry. |
| **Rename (M4)** | Save As is a **copy**; tab **Rename…** is the true same-dir rename. Never-saved tabs stay on Save As. Overwrite/Cancel when the target exists; do not invent inline editable captions. |
| **Unbuffer stdout (M5)** | Line buffering is the **inferior’s libc `FILE`** on a real PTY — not Code::Blocks, not termios, not `sync_with_stdio` in the IDE. Fix with companion GDB `setvbuf` (`inferior-stdio-unbuffered`) when the user opts in; **Run** stays line-buffered. On Darwin AArch64, prefer a memory poke of the `FILE` flags over `call_function_by_hand` when PC cannot be stored. Probe the private command before Start; stock GDB simply keeps today’s buffering. |
| **Watches (M6)** | For this distribution, **self-contained / Python-free GDB is a product requirement**. Do not reopen “enable Python pretty-printers” for STL display. Native CB formatting over the **already-parsed** `GDBWatch` tree is the architecture; raw GDB output is a fixed input. Wrong semantic value is worse than an ugly raw dump — strict recognition, length-based extraction (not C-string NUL), contained ABI knowledge, decline → raw. Apple libc++ SSO max length on the tested toolchain is **22**. |
| **Repo boundaries** | M5 may change sister **`binutils-gdb`** (negotiated debugger capability). M6 must **not** (presentation over existing data). Do not use the GDB fork as an escape hatch for IDE-side value display. |
| **Packaging** | After SDK/plugin installs, refresh the app with `./make_cb_app_bundle.sh` rather than hand-copy + crude `--deep` resign when the bundle must stay launchable. |

#### M1 completion (Save As filters — **DONE** in tree)

- Separate **C files** / **C++ files** / header filters; combined `"C/C++ files"` removed from registration.
- Legacy stored preference migrates to **`"C files"`**; fresh / empty configs default to **`"C++ files"`**.
- One shared `C/C++` lexer unchanged.

#### M2 completion (Run-side — **DONE** in tree)

Implementation (CompilerGCC) now satisfies the Run-side criteria:

- ownership routing + post-save identity (`PrepareBuildOrRunRouting`)
- **PREPARE → COMMIT**: `OnCompileAndRun` arms `m_RunAfterCompile` only after prepare succeeds; cancel Save As never arms a continuation
- continuation retains prepared route/path (`ArmRunAfterCompile` / `RunPrepared`); consumed **exactly once** on success or cleared on failure / kill / pre-COMMIT abort
- empty project cannot Run leftover binary (**V1**); UpdateUI allows standalone editor when project has no sources
- repeated cancel leaves no armed continuation (Retry Invariant for Run / Build and Run)

Debug / Step Into / bootstrap BP / Continue were **M3** (now DONE — see below).

#### M3 completion (Debug-side — **DONE** in tree)

M3 applied the same PREPARE / COMMIT invariants to Debug, Step Into, debugger menu start, bootstrap breakpoints, build-before-debug continuation, prior-inferior Continue, and session reservation — without redesigning `DebuggeeDescription`.

**Accepted after manual matrix (macOS):**

- **U1–U4** — cancel / retry Step Into (no project and with project open); no Continue of prior inferior; no phantom BP; post-cancel Save As still works (**U3**)
- **E** / **E′** — Untitled → Debug Save As OK / Cancel ≡ never requested
- **Q** — project open + standalone `.cpp` → Debug that file only
- **V2** / **V3** — empty project: refuse leftover exe without standalone; with standalone, Debug/Step use that file
- **Run to Cursor** cancel on unsaved buffer and **Stop** while focus is on another pane (former cursor) — OK

Docs: this plan + `README.macos.md` + ChangeLog **v2.2** note.

#### Milestone 4 — Editor-tab Rename (**DONE** in tree)

Standalone Save As leaves the old path on disk (copy, not rename). Code::Blocks has no inline tab-title edit. Project tree already has **Rename file…** (`ProjectManagerUI::OnRenameFile`); the editor-tab context menu now has **Rename…** as well.

**Goal (met):** Add **Rename…** to the editor-tab context menu so a wrongly saved extension (e.g. `.c` → `.cpp`) can be fixed without Finder + reopen and without leaving a duplicate from Save As.

| Topic | Implementation |
|-------|----------------|
| UI | Tab right-click → **Rename…** (near Open containing folder / Copy full path). |
| Standalone | Prompt for new name (same directory); `wxRenameFile` on disk; update open `cbEditor` path / tab title / language (`SetLanguage(HL_AUTO)`); **no** second copy. Never-saved (`!IsOK`) → menu disabled (use Save As). |
| Overwrite | If the target name already exists (and is not a case-only rename), **Overwrite** / **Cancel**; Cancel aborts with no disk change. Case-only renames allowed on case-insensitive volumes. |
| Project-owned | Same disk rename, then `ProjectFile::Rename` + project-tree rebuild; auto-generated files remain disabled. Editor stays open (unlike the tree Rename path, which closes first). |
| Prior art | `ProjectManagerUI::OnRenameFile`; tab menu in `EditorManager::OnPageContextMenu`. |
| Non-goals (unchanged) | Inline editable tab caption; mass rename; cross-directory moves. |

**Acceptance:** Rename standalone `.c` → `.cpp` via tab menu → one file on disk, tab and highlighting updated, Build/Run/Debug still see the new path; project-owned rename stays consistent with the project tree.

#### Milestone 5 — Unbuffered inferior stdout (companion GDB)

**Status: DONE** (tree). Implemented on Mac in sister `binutils-gdb` + this repo; bundled via staging. M5 does not block or depend on M1–M4.

**Problem.** On macOS, Program Console (CB-owned PTY) and Terminal.app are both real terminals. The debuggee's libc therefore line-buffers `stdout`. `std::cout << "cocou"` stays in that `FILE` buffer until a newline, `std::flush` / `std::endl`, a tied `cin` read, or exit. Nothing is written to the slave, so the console cannot show it. Windows is different for a platform reason, not a Code::Blocks one: the MSVC CRT implements console `_IOLBF` as `_IONBF`, so the same line appears as soon as it runs. This repo does not call `setvbuf`. `openpty` is called with a null termios (`src/sdk/programconsole.cpp`). Termios flags do not control stdio buffering.

**Constraint.** Do not edit the user's source and do not insert a dylib. The only sanctioned mutation is the companion GDB calling `setvbuf` inside the inferior at startup, and only when the user has asked for it. `std::ios_base::sync_with_stdio` in Code::Blocks does nothing to the inferior; the default in the inferior is already `true`, which is why `cout` uses the C `stdout` buffer. Do not call it.

##### Product decisions (locked)

| Topic | Decision |
|-------|----------|
| Who injects | Companion Darwin GDB in sister repo **`binutils-gdb`** (see top of this plan). Not Code::Blocks, not the PTY. |
| GDB command | `set inferior-stdio-unbuffered on\|off` and `show inferior-stdio-unbuffered`. Same shape as `inferior-tty-claim`. |
| GDB default | **off**. A shell `gdb` does not inject unless something sets the flag. |
| Code::Blocks default | **on** (macOS debugger config). Escape hatch for programs that must stay line-buffered. |
| When CB sends it | macOS **console Debug** only, and only after `show` proves this GDB has the command. Stock GDB: probe fails, Debug still starts, stdout stays line-buffered. |
| Which consoles | **Both** Program Console and Terminal.app. Both are ttys. Unlike claim: claim-off stays **Terminal-only**; this flag does not change claim. |
| Run | **Out of scope.** Run has no GDB. Program Console Run stays line-buffered. |
| Windows / Linux | Do not queue the command. Windows console debug is already unbuffered at the CRT. Hide or disable the checkbox off macOS. |
| Attach | Do not inject. The process has already started. |
| What is unbuffered | C `stdout` only. `stderr` is already unbuffered. Do not touch `stdin`. |
| `cout` | Covered only while `sync_with_stdio` stays at its default `true`. A program that turns it off gets a separate C++ buffer again. Document; do not fight it. |

##### Code::Blocks command order (REQUIRED)

`set` must be queued **before** `start` / `run`, and `Start()` must not be queued until that `set` is on the queue. `GdbCmd_SetupMacOSInferiorTty` already does this for claim by calling `Start()` from `ParseOutput`. Copy that. Do not queue the probe in `GDB_driver::Prepare` and then call `Start()` in `DebuggerGDB::DoDebug`: the probe's `ParseOutput` would append `set … on` **after** `run` is already queued.

Today (`src/plugins/debuggergdb/debuggergdb.cpp`):

- **Program Console:** queue `tty <slave>` (claim left **ON**), then `Start()` / `Attach()` immediately.
- **Terminal.app:** queue `show inferior-tty-claim`; `ParseOutput` queues `set inferior-tty-claim off` (if supported), then `tty <path>`, then `Start()` / `Attach()`. `deferStartForMacOSTty` skips the immediate start.

When the checkbox is **on**, both paths defer start through one new command, after the tty line and without disturbing claim:

```text
Program Console (claim stays ON):
  tty <slave>
  show inferior-stdio-unbuffered
      → if supported: set inferior-stdio-unbuffered on
      → then Start(breakOnEntry) or Attach   # from ParseOutput, not before

Terminal.app (claim-off unchanged):
  show inferior-tty-claim
      → if supported: set inferior-tty-claim off
  tty <path>
  show inferior-stdio-unbuffered
      → if supported: set inferior-stdio-unbuffered on
      → then Start(breakOnEntry) or Attach
```

When the checkbox is **off**, queue nothing about this setting and keep today's start placement. Do not send `set inferior-stdio-unbuffered off` (GDB's own default is off; a new GDB process is started per session). A hand-written debugger init command may still turn it on; that is the user's command.

Probe failure (`Undefined` / `No symbol` / `Ambiguous`, same test as `GdbCmd_SetupMacOSInferiorTty`) logs once and still starts. Debug must not abort because the GDB is stock.

Suggested shape: a small `DebuggerCmd` next to `GdbCmd_SetupMacOSInferiorTty` (anonymous namespace in `debuggergdb.cpp`), not a change to `ProgramConsole` and not a fold into the claim-off command. The PTY path must not start sending `set inferior-tty-claim off`.

##### Code::Blocks files

| Piece | Change |
|-------|--------|
| `debuggeroptionsdlg.h` | New `DebuggerConfiguration::Flags` value, e.g. `UnbufferStdio`. |
| `debuggeroptionsdlg.cpp` | Read/write like `CatchExceptions` (config bool, default **true**). |
| `resources/debugger_options_dlg.xrc` | Checkbox on the GDB page, macOS-only (hide or do not create on other platforms). Label along the lines of “Show program output immediately while debugging (macOS)”. Help text: companion GDB only; turns off line buffering of `stdout`; does not change the source; Run is unchanged. |
| `debuggergdb.cpp` | Queue the probe as above. Guard with `platform::macosx`, console debug, flag on, not attach-only if start is skipped — attach never gets the set. |
| `README.macos.md` | Short subsection beside “Darwin GDB terminal handling”: the new set/show, the probe, and that line buffering remains if the GDB is stock or the box is unchecked. |
| ChangeLog | One line when the milestone is done. |

Do not put this in Environment → Console programs (`/console_output_mode` chooses PTY vs Terminal, not buffering).

##### Companion GDB (sister `binutils-gdb`)

Grep that tree for `inferior-tty-claim` / `inferior_tty_claim` and add the boolean beside it. Then grep `start_command` / `run_command_1` in `infcmd.c` (names drift; use whatever that tree calls the `start`/`run` path).

**First task on the Mac, before writing the call.** `stdout` is a macro. Confirm the symbol and `_IONBF` against the SDK actually used to build this GDB:

```gdb
start
info address __stdoutp
ptype __stdoutp
p (int)setvbuf(__stdoutp, 0, 2, 0)
```

If `ptype` says `FILE **`, the expression is `setvbuf(*__stdoutp, 0, 2, 0)` instead. Keep the expression that returns `0`. Confirm `#define _IONBF 2` in that SDK's `stdio.h` (it is 2 on Darwin; do not assume if the probe says otherwise). Hardcode the working expression with a comment that cites this probe. Do not call `sync_with_stdio`.

**Injection (as shipped).** When the flag is on, `run` and `start` only (not `attach`, not a later `fork`/`exec` of a grandchild):

1. Plant / reuse a temporary breakpoint at `main` (`start`'s existing tbreak when break-on-entry; otherwise a silent run-only tbreak).
2. On that stop, apply Darwin `setvbuf(stdout, NULL, _IONBF, 0)` semantics to the `FILE` behind `__stdoutp` (probe: `_IONBF==2`, `__stdoutp` is `FILE*`). **Note:** `call_function_by_hand` is unavailable on AArch64 Darwin (PC store rejected); the shipped path writes the known Darwin libc `_IONBF` FILE state instead.
3. If the user asked for `run`: continue after unbuffering so the stop is not visible.
4. If the user asked for `start` (break on entry, Step Into): leave stopped at `main` once, stdout already unbuffered.
5. If `main` cannot be found: one warning, run as usual.

`stderr` and a raw `write()` already bypass this buffer.

##### Acceptance (manual, Mac)

Build a console C++ program. No `endl`, no `flush`, no `cin`:

```cpp
#include <iostream>
int main() {
    std::cout << "cocou";
    return 0;
}
```

Use the bundled companion GDB (`CodeBlocks.app` `…/debugger/bin/gdb`, or `CB_GDB`).

| ID | Steps | Expected |
|----|-------|----------|
| **W1** | Checkbox **on**, Program Console, Debug, step over the `cout` line | Pane shows `cocou` **before** the function returns. Claim stays **ON** (no `set inferior-tty-claim off` in the log). |
| **W2** | Same, checkbox **off** | `cocou` does **not** appear on that step. It appears at exit (libc flush) or if the line is changed to `endl` / `flush`. |
| **W3** | Checkbox **on**, `cout << "cocou\n"` and a later `endl` | Both still appear. `cin` still reads. |
| **W4** | Checkbox **on**, Program Console, Ctrl-C while the inferior is running | SIGINT still delivered (claim ON). Unbuffering must not turn claim off. |
| **W5** | Checkbox **on**, Terminal.app Debug | Same immediate `cocou`, **and** the existing claim-off handshake still runs (`set inferior-tty-claim off` before `tty` when that GDB supports it). |
| **W6** | Checkbox **on**, Step Into / break on entry | One stop at `main`, not two. After stepping the `cout`, `cocou` is visible. |
| **W7** | Checkbox **on**, point the debugger at a stock GDB with no such setting | `show` fails, Debug still starts, line-buffered behaviour, no fatal GDB error. |
| **W8** | Checkbox **on**, `printf("cocou")` without `\n` | Same as **W1** (stdio path). |
| **W9** | Run (not Debug), Program Console, checkbox on | **Unchanged** line buffering. M5 does not apply to Run. |
| **W10** | Attach to an already-running process | No `setvbuf` attempt. |

**W1** is the milestone. **W4** and **W5** exist so this flag cannot regress v1.1 claim-off or v2.0 claim-on.

##### Non-goals (M5)

- Editing the debuggee, `DYLD_INSERT_LIBRARIES`, or `stdbuf`.
- Unbuffering Run, or scraping the `FILE` buffer out of the inferior.
- Forcing `sync_with_stdio(true)` or `std::unitbuf`.
- Unbuffering grandchildren after a later `exec`.
- Changing Windows or Linux.
- Shipping a new GDB from this repo. The boolean lands in **`binutils-gdb`**; Code::Blocks only probes and sets it. Bump the bundled GDB when that port is ready (same packaging path as v1.2).

#### Milestone 6 — Human-readable C++ Watches (`std::string` first)

**Status: DONE** (tree). Architecture locked: **Code::Blocks owns M6**; companion **`binutils-gdb` completely out of scope**. Does not reopen M1–M5 PREPARE/COMMIT, Transaction, Launch Identity, Retry, breakpoint, or continuation work.

##### M6 completion

Implemented in the debuggergdb plugin:

- `nativewatchsummary.cpp` / `.h` — `TryNativeWatchSummary` after `ParseGDBWatchValue` (locals/args and user watches).
- Apple libc++ `std::__1::string` / `basic_string<char,…>` with strict type+layout recognition; fingerprint path for typeless `info locals`.
- SSO / long via `__is_long_`; length-based payload extraction (embedded NUL); parent summary + raw children preserved.
- Decline → raw; no GDB/Python changes. Unit fixtures in `debuggergdb_test_native_summary.cpp`.
- Verified ABI on this toolchain: SSO max length **22** (capacity 22; length 23 → heap).

##### Goal

Prefer a reliable **semantic** representation of a C++ value in Watches over exposing implementation internals as the primary display — without inventing values.

For a local `std::string a` whose value is `"12583934"`, the primary Watches row should be approximately:

```text
a    "12583934"
      > … raw implementation children …
```

The beginner gets the useful value first; the advanced user can still expand raw implementation children. Internals must not replace the semantic value as the primary representation. No “beginner mode.” This is a Code::Blocks **value-presentation** feature over data the IDE already receives.

##### Hard boundary — do not touch the GDB fork

The sister repository **`binutils-gdb` is completely out of scope for M6.** This is stronger than “no GDB changes are currently expected.”

For M6, do **not**:

- modify, inspect, search, or build `binutils-gdb`
- change its configure options; enable or propose enabling Python
- add Python or another scripting runtime; add GDB pretty-printer scripts
- add private GDB commands; add capability negotiation with the GDB fork
- change GDB output, packaging, or staging specifically for M6
- use the GDB fork as an implementation escape hatch

Treat the **currently bundled GDB** and the **raw type/value information it already supplies** as a **fixed external input**. Already-established observations about that output may remain as evidence below; they do **not** authorise further investigation of the GDB repository.

If existing output does not provide enough information to decode a value safely:

```text
native formatter → NOT HANDLED → display today’s raw representation
```

Do **not** respond by proposing a GDB change — even if changing GDB would make the formatter easier. Raw but truthful beats semantic but guessed.

##### Ownership boundary

```text
existing GDB  (FIXED INTERFACE FOR M6)
    |  raw type/value information already supplied today
    v
existing Code::Blocks GDB parsing
    v
parsed GDBWatch representation
    v
M6 native semantic formatter   ← Code::Blocks owns this
    v
Watches display
```

M6 answers: *Given the raw debugger information Code::Blocks already receives and parses, can Code::Blocks recognise a supported semantic C++ type and present its value more usefully?* The answer must **not** depend on changing the debugger.

**Native Code::Blocks formatting IS the architecture** for M6 — not a temporary fallback pending a future Python/GDB implementation. Do not keep a competing GDB/Python architecture in this plan.

##### Self-contained distribution (product requirement)

The macOS Code::Blocks distribution must remain **self-contained**. M6 must not introduce a new runtime dependency merely to improve Watches.

`std::string` presentation must **not** require the user to install, configure, locate, or maintain Python, printer scripts, debugger extensions, external packages, or another GDB build. Installing the existing Code::Blocks application bundle must be enough to obtain M6 behaviour from **Code::Blocks itself**.

##### Explicit ABI trade-off

We accept a **small amount of controlled Apple-libc++ ABI knowledge inside Code::Blocks** in exchange for keeping the distributed application self-contained and Python-free.

That knowledge must stay: isolated in the native formatter; conservative; testable; type-specific; absent from generic Watches UI / generic GDB parsing / debugger command generation; able to decline an unknown representation. Do not conceal the maintenance cost of owning that ABI surface.

##### Confirmed findings (preserved evidence only)

| Finding | Evidence (already established; no further GDB-repo work) |
|---------|----------|
| Default macOS `clang++` debuggee uses **Apple libc++** | Links `/usr/lib/libc++.1.dylib`; mangling `St3__1` / DWARF `std::__1::basic_string`; `whatis a` → `std::__1::string`. |
| Bundled GDB supplies **raw** aggregates (no semantic `std::string` summary) | Direct `print a` of libc++ string shows `__endian_factor` / `__rep_` / `__data_` / `npos` (sample below). |
| Bundled GDB has no Python pretty-printer path | Runtime: Python unsupported; `info pretty-printer` undefined. Treat as fixed input fact, not a knob M6 turns. |
| CB parses aggregates into a `GDBWatch` tree | `ParseGDBWatchValue`; brace aggregates clear parent value and build children. |
| Existing model supports **parent value + children** | Scalars use value; aggregates use children; parent `SetValue` is available. |

##### Direct-GDB sample for `std::string` (preserved fixed input)

Already observed with bundled GDB and `clang++ -g -O0` libc++ after `std::string a = "12583934"`:

```text
(gdb) print a
$1 = {static __endian_factor = 1, {__rep_ = {__s = {__data_ = "12583934\000...",
        __size_ = 8 '\b', __is_long_ = 0 '\000'}, __l = {...}}, ...},
     static npos = 18446744073709551615}
```

No primary `"12583934"` on the parent. Character data appears only under implementation fields. Code::Blocks already parses this shape into Watches. **M6 consumes that parsed tree** — it does not change what GDB emits.

##### Standard library / toolchain

- Compiler: Apple Clang / `clang++` (default).
- Library: **libc++** (`std::__1`), not libstdc++.
- M6 REQUIRED target: `std::__1::string` / `std::__1::basic_string<char, …>` only. Other STL types and libstdc++ are future/optional — not M6.

##### Code::Blocks insertion point

Determine the narrowest insertion point **using only this repository** (debuggergdb plugin):

```text
DebuggerGDB::DoWatches
  → GDB_driver::UpdateWatches
       locals / args:  "info locals" / "info args"
       user watches:   "whatis …" → "output <symbol>"
  → TokenizeGDBLocals / GdbCmd_Watch::ParseOutput
  → ParseGDBWatchValue  →  GDBWatch tree (type + children; parent often empty for `{…}`)
  → [M6] native semantic summariser (type + parsed tree)   ← own ABI knowledge here
        → handled: SetValue(semantic summary); keep children
        → not handled: leave raw tree as today
  → Watches dialog
```

Operate **after** enough information is parsed to recognise a supported representation, and **before** presentation loses the chance to assign a semantic parent value. Prefer a **distinct** formatter/helper module over embedding libc++ layout knowledge in generic parsing, Watches rendering, or command generation.

Why parent shows no useful string today: brace-wrapped dump → parser clears parent value → children hold `__endian_factor` / `__data_` / `npos` / etc.

##### Architecture — native semantic summariser

Illustrative contract (follow existing CB naming/conventions at implement time):

```text
TryNativeWatchSummary(type, parsedWatchTree)
  → handled + semantic value
  or
  → not handled
```

Properties:

1. **Distinct narrow transformation** over already-parsed debugger data — not ad-hoc field-name checks in the Watches UI.
2. **Strict recognition:** act only when (a) type matches a specifically supported family **and** (b) the parsed representation matches a known supported layout strongly enough to extract safely. Else **NOT HANDLED** → raw tree. Never “this somewhat looks like a string.”
3. **Contained ABI knowledge:** one formatter owns Apple libc++ short/long layout decoding. Do not spread `__rep_` / `__data_` / SSO flags through the plugin.
4. **Length semantics:** use the layout’s size/length, not C-string NUL truncation. `std::string("abc\0def", 7)` must not become `"abc"`.
5. **Value + children:** set semantic summary on the parent; **preserve** raw implementation children. Do not hide children because they are ugly. If brace-aggregate parsing always clears parent value, make only the **narrow** change needed so a formatter-generated parent summary can stick — do not globally rewrite unrelated aggregates unless evidence requires it.
6. **Fail-safe:** insufficient type/layout/size/data → NOT HANDLED → raw. No invented value. No GDB-side “fix.”
7. **No GDB protocol / probes** for M6.

##### Pre-implementation investigation (within Code::Blocks + toolchain only)

Before coding extraction, verify supported Apple libc++ layouts using **this repo**, the shipped app’s observed watch/parse behaviour, and public/toolchain headers as needed — **not** by inspecting or modifying `binutils-gdb`. Confirm at least:

| Case | What to confirm |
|------|-----------------|
| Empty | How size/flag/data appear in the **parsed** `GDBWatch` tree. |
| Short / SSO | Where characters and length live; SSO capacity for this ABI. |
| Long / heap | Pointer + size (+ capacity); long vs short discriminator. |
| SSO boundary | Transition length for this libc++; values immediately below/above. |
| Embedded NUL | Size is authoritative; payload may contain `'\0'` before “end”. |
| Endian / flag packing | Any packing that affects decoding of the **already-parsed** fields. |

Do **not** implement by “find a child named `__data_` and display it.” Document confirmed SSO capacity and field mapping in implementation notes when DONE (do not hardcode the boundary in this plan until verified).

##### Files / functions likely affected (implementation phase — not now)

| Area | Likely touch |
|------|----------------|
| New narrow module under debuggergdb (native watch summary / libc++ string formatter) | Type match + layout decode; owns all ABI-specific names. |
| Call site after `ParseGDBWatchValue` (locals/args and/or `GdbCmd_Watch::ParseOutput`) | Apply summary when handled; leave children. |
| `parsewatchvalue.cpp` / `GDBWatch` | Only if needed so parent `SetValue` coexists with children for formatter results. |
| Unit tests (`debuggergdb_test_parse_*.cpp` or new tests) | Empty / SSO / long / NUL / decline / unrelated aggregate. |
| README.macos.md / ChangeLog | User-facing note when DONE. |

**In scope:** Code::Blocks only. **Out of scope:** `binutils-gdb`, Python, printer scripts, GDB prepare/command changes for printers.

##### Must NOT change (M6)

- M1–M5 PREPARE→COMMIT, Transaction, Launch Identity, Retry, breakpoint lifecycle, Run-after-compile, claim-off / unbuffer-stdio order.
- Anything in **`binutils-gdb`** (including configure, Python, printers, private commands, output shaping, packaging).
- New Python / scripting runtime or printer scripts in the Code::Blocks bundle.
- `DebuggeeDescription` redesign; LLDB/DAP; Watches UI rewrite; beginner mode.
- Spreading libc++ member names outside the formatter module.
- Silently suppressing raw watch children when a summary exists.

##### Regression risks

| Risk | Mitigation |
|------|------------|
| Wrong summary on layout drift | Strict match; NOT HANDLED → raw; pin tests to observed ABI. |
| Truncation at embedded NUL | Length-based extraction; W14. |
| SSO↔long transition bugs | W12 / W13 / W15. |
| Parent value cleared by parser | Narrow preserve-summary change; W1 + W8. |
| Accidental effect on scalars / other aggregates | Type gate; W6; W7; unrelated aggregate untouched. |
| Temptation to “fix” insufficient data in GDB | Hard rule: NOT HANDLED only — never a GDB change. |

##### Acceptance matrix

| ID | Steps | Expected |
|----|-------|----------|
| **W1** | `std::string a = "12583934";` | Parent ≈ `"12583934"`; raw children available where supported. |
| **W2** | Empty `std::string a;` | Parent `""`. |
| **W3** | `"hello world"` | Full text including space. |
| **W4** | `"hello"` then `+= " world"` across step | Parent updates `"hello"` → `"hello world"`. |
| **W5** | Two strings | Independent correct values. |
| **W6** | `int` / `double` / `bool`; unrelated non-string aggregates | Unchanged / untouched by the formatter. |
| **W7** | Unrecognised / incomplete / malformed layout → decline | NOT HANDLED; raw remains; no crash; no guessed summary; **no** GDB-side proposal. |
| **W8** | Recognised string | Semantic parent + raw tree reachable (expand children). |
| **W9** | Standalone Debug W1 | Same semantic parent as project. |
| **W10** | Project Debug W1 | Same as W9. |
| **W11** | Very short / SSO (`"a"`) | `"a"`. |
| **W12** | Values around confirmed SSO boundary | Correct on both sides of the representation transition. |
| **W13** | Definitely long / heap-backed string | Full value from long representation. |
| **W14** | `std::string a("abc\0def", 7)` | Must not silently truncate to `"abc"`; escaping may follow existing UI conventions but length is seven characters. |
| **W15** | Step across short↔long (and long↔short) mutation | Semantic value remains correct across representation change. |

**W1** is the milestone gate. Presentation must not depend on project vs standalone routing (M2/M3 ownership unchanged). Embedded-NUL (W14) is mandatory: do not implement `std::string` as if it were a NUL-terminated C string.

##### REQUIRED / OPTIONAL / OUT OF SCOPE (M6)

**REQUIRED**

1. Code::Blocks owns M6; native semantic-summary mechanism with strict recognition.  
2. Apple-libc++ `std::string` / `basic_string<char>` for the shipped/tested toolchain.  
3. Empty; short/SSO; long/heap; spaces; step updates; SSO↔long transitions; embedded NUL via length semantics.  
4. Raw children remain accessible where practical.  
5. Unknown / incomplete layout → raw Watches (NOT HANDLED).  
6. Scalars and unrelated aggregates unchanged.  
7. Project and standalone Debug identical presentation.  
8. Self-contained bundle: no Python, no printer scripts, no external debugger dependency for M6.  
9. **`binutils-gdb` completely untouched** (no modify / inspect / build / configure for M6).  
10. M1–M5 lifecycle untouched.

**OPTIONAL (future — not M6)**

- Other native formatters (`vector`, `array`, `optional`, smart pointers, other string types) **only if** this mechanism proves clean — do not expand scope because the abstraction allows it.  
- libstdc++ `std::string` — not automatic unless this distribution needs it.

**OUT OF SCOPE**

- Entire **`binutils-gdb`** surface for M6 (source, configure, Python, printers, private commands, output changes, packaging/staging).  
- Python runtime; printer scripts; debugger extensions; external packages; alternate GDB builds.  
- Competing “GDB should provide the summary later” architecture.  
- Graphical / generic STL visualisers; beginner mode; Watches UI rewrite.  
- LLDB / DAP; M1–M5 lifecycle/routing; C++ language semantics changes.  
- Guessing unknown STL layouts; silently suppressing raw watch data.  
- Capability negotiation with GDB for M6.

##### Interaction with M1–M5

M6 is orthogonal: after the inferior is already stopped, it only adjusts **how** a parsed watch is summarised for display inside Code::Blocks. It must not alter prepare/save, ownership routing, claim-off, unbuffer-stdio probes, or breakpoint arming. W9/W10 must match once stopped. M1–M5 remain untouched.

### Scope summary

**REQUIRED**

1. C / C++ / header Save As filter separation (one `C/C++` lexer).  
2. Legacy stored `"C/C++ files"` → **`"C files"`** (rewrite so the name is not left dead); fresh default **`"C++ files"`** separately.  
3. Save / cancel / **post-save path** for standalone Build / Build and Run.  
4. **Active-source ↔ active-project membership** routing (even when a project is open).  
5. Preserve v2.1 standalone Debug (**gates** follow the same ownership rule; no architecture rewrite).  
6. **Operation Transaction Invariant** + **Launch Identity Invariant** (PREPARE → COMMIT; pre-COMMIT cancel ≡ never requested; no identity not established for the current request).  
7. Retry Invariant; pre-COMMIT vs post-COMMIT outcomes; async continuation exactly-once ownership.  
8. Manual matrix **A–T** plus invariant tests **U1–U4**, **V1–V3** (**M1**–**M6** delivered).

**OPTIONAL / complement** (future polish — not required to call v2.2 done)

- Deduplicate prepare/save helper with `SaveActiveSourceForStandaloneDebug`.  
- macOS-only post-OK extension nudge if native dialog omits ext despite filter.  
- Grep leftover UI strings expecting `"C/C++ files"` beyond Save As config.  
- Additional native Watches formatters (`vector`, etc.) if the M6 mechanism stays clean.

**EXPLICITLY OUT OF SCOPE**

- Redesign of `DebuggeeDescription` / DebuggerGDB core; new debugger architecture; breakpoint subsystem redesign.  
- New generic transaction framework unless existing CB mechanisms cannot satisfy the invariants.  
- Beginner mode; ephemeral `.cbp`; parallel build system; broad compiler redesign.  
- Splitting the `C/C++` lexer into two languages.  
- Globally forcing `.cpp` (or using migration to force C on fresh configs).  
- gdbmi standalone Debug; LLDB / DAP.  
- Changing project target language rules.  
- Standalone args/cwd/includes UI.  
- Deleting leftover executables merely to pass empty-project tests.  
- Inline editable tab titles (M4 is menu Rename, not caption editing).  
- Unbuffering Run, injecting a dylib, or editing the debuggee source to flush `cout` (M5 is GDB-side `setvbuf` on Debug only).  
- Unrelated cleanup.

### Outcome (**DONE** in tree)

New files Save As **C** or **C++** with the right extension. Build / Build and Run / Debug always operate on the **active** source after a successful PREPARE, routed by **membership in the active project** — so an open project never hijacks a standalone editor — while project-owned files and v2.1 debugging remain intact. **Pre-COMMIT cancellation is observationally equivalent to never requesting the operation; no Build/Run/Debug uses an identity not established for that request** (including empty-project leftover binaries and prior inferiors). Legacy Save As preference `"C/C++ files"` becomes `"C files"`; new configs start on `"C++ files"`. **M4** adds tab-menu Rename so a wrong extension can be fixed without a duplicate on disk. **M5** lets macOS Debug ask the companion GDB to unbuffer inferior `stdout` so a `cout` without `endl` shows up while stepping; Run stays line-buffered. **M6** makes Watches show a human-readable Apple-libc++ `std::string` parent summary via a native Code::Blocks formatter over the existing raw GDB parse tree (`binutils-gdb` out of scope; self-contained, no Python).

v2.2 is complete for product purposes once M1–M6 are in the UI; remaining OPTIONAL bullets above are polish, not blockers. Tag/release when packaging is ready.

