# v2.3 — Enable debugging without manual codesign (local helper)

Detail plan for this version. Overview and status table: [versioning_plan.md](versioning_plan.md).

---

**Status: M1–M5 DONE in tree. M6 planned (not started).** Does not reopen v2.2 M1–M6.

**Depends on:** v1.2+ bundled Darwin GDB; v2.2 Gatekeeper first-launch docs in `README.macos.md`.

**Single-Mac constraint (locked):** Only this development Mac is available. A second Mac, clean Mac, VM, or fresh OS install is **not** an acceptance prerequisite for v2.3. Validation is by local diagnosis + injected signing states + forced Tier 2. Residual field limitations must be documented, not deferred to unavailable hardware.

**Goal:** A user who downloads the `.dmg` may need to authorise macOS security UI (Gatekeeper **Open Anyway**, admin password, Privacy prompts), but must **not** need to understand or manually perform certificate creation or `codesign` as the normal product path.

### Default happy path (locked — unchanged from today)

**v2.3 does not change how release DMGs are signed.** The default install path remains:

```text
Ship DMG
  → nested gdb already ad-hoc signed + com.apple.security.cs.debugger
       (make_cb_app_bundle.sh, as today)
  → user: Open Anyway (Gatekeeper) if needed
  → Debug works on Macs that accept that ad-hoc+entitlement signature
```

**Enable debugging / Tier 1 / Tier 2 is a repair option only** — for when Debug fails with confirmed signing / task-port authorization problems. It is **not** part of default install, **not** a post-install wizard for every user, and **not** a replacement for shipping a pre-signed bundled `gdb`.

```text
Happy path (most installs):
  DMG (ad-hoc gdb) → Open Anyway → Debug  ✓   [no helper]

Repair path (only if needed):
  Debug hits confirmed task-port / signing failure
  → prompt: Enable debugging? (consent required — repair does not auto-run)
       ├─ Decline / “don’t ask again” → silence future prompts (re-enable in Debugger prefs)
       └─ Consent → diagnose → Tier 1 / Tier 2 inside the IDE → user retries Debug
  → Menu “Enable debugging…” also available for manual repair
```

### Product decisions (locked)

| Topic | Decision |
|-------|----------|
| **Default ship format** | **Same as now:** release DMG / app bundle ships bundled `gdb` **ad-hoc signed** with `com.apple.security.cs.debugger`. That is the happy path. |
| **Helper role** | **Repair only.** Not the default install; not required when Debug already works. |
| **User invariant** | Authorise macOS security changes; **no** Keychain Assistant / terminal `codesign` as the primary path. |
| **Gatekeeper vs debugger** | Separate. Open Anyway = run the app. Debugger attach = `taskgated` + nested `gdb` signature/entitlements. Do not conflate in UI or docs. |
| **What we ship** | Release bundles keep nested `gdb` **ad-hoc signed** with `com.apple.security.cs.debugger` (`make_cb_app_bundle.sh`, default **on**). Lab “sign off” exists only to inject repair scenarios. |
| **Repair scope** | Helper may modify **only** the bundled Code::Blocks `Resources/debugger/bin/gdb` (and its companion signed dylibs if required). **Never** resign arbitrary custom debugger paths. |
| **Ad-hoc vs certificate** | Alternatives, not a chain. Ad-hoc needs no certificate. Local trusted identity is **Tier 2 escalation**, not the default ship format. |
| **Prompt on confirmed failure** | **REQUIRED.** On first **confirmed** task-port / signing authorization failure, offer Enable debugging. **Not** when `gdb` is missing / not found. **Not** for bare “No stack” without signing evidence. |
| **Consent before repair** | Repair **never** proceeds automatically. The user must explicitly consent. After consent, Tier 1/Tier 2 selection runs without asking the user to choose a tier. |
| **Silence / re-offer** | At the prompt, the user may allow asking again on later failures, or silence future automatic offers. If silenced, **Debugger preferences** must expose a control to re-enable automatic offers. Manual **Enable debugging…** remains available regardless (unless a later UX finding says otherwise — default: keep menu). |
| **Orchestration** | The IDE owns diagnosis, consent, tier choice, and the repair. M6 keeps that repair **in-process**, in a macOS-only component. No separate executable. |
| **Manual docs** | `code_signing.md` remains **advanced / recovery** only. |
| **Sister `binutils-gdb`** | Signing policy unchanged; bundle script remains the product signing locus. |

### Confirmed findings (pre-plan)

| Finding | Evidence |
|---------|----------|
| Shipped nested `gdb` is ad-hoc + `cs.debugger` | `codesign -dv` → `Signature=adhoc`; entitlement present. |
| Staging GDB same; build `CODESIGN_CERT=""` | Sister tree does not require `gdb-cert`. |
| Bundle script uses `--sign -`, not a named cert | `make_cb_app_bundle.sh`. |
| Named cert trust does not affect an ad-hoc binary | Independent identities. |
| Injected unsigned / no-entitlement states stall Debug locally | Lab injection — **not** a replica of “valid ad-hoc rejected by taskgated elsewhere.” |

### Mental model

```text
                    ┌─ ad-hoc (-) ────────────────┐
  codesign ────────┤                             ├──► signature + entitlements
                    └─ named local identity ──────┘

  Gatekeeper / quarantine  →  app launch
  taskgated                →  debugger attach
```

### Diagnosis before any mutation

**“No stack” alone is not sufficient** to modify signing state. Prefer explicit evidence: Mach task-port / `taskgated` / codesign verify / missing or invalid signature / missing `com.apple.security.cs.debugger`. Distinguish unrelated debugger failures (wrong path, inferior crash, breakpoints, etc.) and **do not** propose repair for those.

**Quarantine / Gatekeeper** findings are separate from GDB signing / `taskgated`. Do **not** treat broad quarantine removal as part of generic debugger repair unless investigation shows it is required for the diagnosed condition.

### Architecture — evidence-driven tiers

```text
Diagnose bundled GDB only
  |
  +-- signature and/or debugger entitlement defective
  |     (missing, invalid, damaged, incomplete)
  |       -> Tier 1: restore ad-hoc signature + entitlements
  |       -> verify
  |       -> user retries Debug
  |
  +-- signature + entitlement already valid
        -> do NOT blindly re-sign
        -> confirm debugger / task-port authorization failure
           with evidence beyond bare “No stack”
        -> Tier 2 if appropriate (local trusted identity)
```

**Tier 1 — restoration only (no certificate)**  
Repair a **missing, invalid, damaged, or incomplete** ad-hoc signature and/or debugger entitlement on the **bundled** `gdb`.  
Do **not** claim that re-applying an ad-hoc signature fixes the field case where an otherwise **valid** ad-hoc+entitled `gdb` is refused by `taskgated`. If diagnosis already shows valid ad-hoc + entitlement, skip Tier 1 resign.

**Tier 2 — local trusted-identity recovery**  
Entered when diagnosis shows valid ad-hoc+entitlement **and** confirmed debugger/task-port authorization failure, **or** when developer-only injection forces Tier 2 for testing. Stronger local Code Signing identity + entitlements on the bundled `gdb`.

Product surface (illustrative):

- Dialog on confirmed task-port failure: explain briefly; **Enable** / **Not now**; option to **don’t ask again** (silence automatic offers).
- After **Enable**, show plain-language progress while tiers run; no cert names or shell recipes.
- Menu: **Enable debugging on this Mac…** (manual entry).
- **Settings → Debugger** (or equivalent): preference to re-enable automatic offers after silence.
- Idempotent repair after consent.

### Tier 2 identity lifecycle (M4 decisions)

| Question | Decision in tree |
|----------|------------------|
| Identity name | `codeblocks-gdb` (CN) |
| Keychain | **System** keychain (`/Library/Keychains/System.keychain`); login-only trust is not enough for `taskgated` |
| Create/reuse | `security find-identity`; create via openssl + `security import` only if missing |
| Trust | Admin-domain code-signing trust via `SecTrustSettingsSetTrustSettings` in the user process (second admin panel). `authorizationdb write` is not used (denied on this OS). |
| Expiry | 10-year self-signed; recreate if missing from keychain |
| Scope | Bundled `gdb` (+ companion dylibs) only |
| Force lab | `CB_FORCE_GDB_TIER2` / Debugger “Lab: force Tier 2…” |

### Developer lab (this Mac only)

Injectable states via **`--gdb-sign=`** on `make_cb_app_bundle.sh` (default **`normal`** = release):

| State | Mechanism | Intent |
|-------|-----------|--------|
| **normal** | `--gdb-sign=normal` (default / omit) | Ad-hoc + debugger entitlement (happy path / ship) |
| **unsigned** | `--gdb-sign=unsigned` | Missing signature |
| **no-entitlement** | `--gdb-sign=no-entitlement` | Signed but `cs.debugger` absent |
| **force Tier 2** | `CB_FORCE_GDB_TIER2=1` and/or Debugger option “Lab: force Tier 2…” | Exercise Tier 2 when this Mac accepts ad-hoc — **not** natural `taskgated` rejection |

Mode is recorded in `Resources/debugger/.cb_gdb_sign_lab`. Details: `code_signing.md` (Developer lab).

**Lab may prove (M1):** packaging of normal / unsigned / no-entitlement; smoke checks; loud warnings against shipping lab modes.

**Lab may prove (M2):** debugger log diagnosis for bundled path / signature / entitlement; bare No stack vs task-port evidence; no mutation.

**Lab may prove (M3):** Tier 1 restore via menu when defective; skip when already valid; bundled-only refuse; post-sign verify.

**Lab may prove (M4):** Tier 2 `codeblocks-gdb` identity create/reuse; force injection; Enable debugging orchestrates Tier 1 then Tier 2.

**Lab may prove (M5):** auto-offer consent/silence; Debugger pref re-enables offers; menu remains; S1–S10.

**Lab may prove (M6):** S1–S11 unchanged in behaviour after provisioning lives in the macOS-only component; a custom debugger path is still refused; Linux and Windows builds compile with that API omitted.

**Lab must not claim:** that unsigned or entitlement-stripped `gdb` faithfully reproduces  
`valid ad-hoc + valid entitlement + taskgated refusal on another Mac`.  
That remains an **explicit residual validation limitation**.

Never ship a release DMG with `unsigned` / `no-entitlement` or with force-Tier-2 injection enabled.

### Docs impact

| Doc | Direction |
|-----|-----------|
| `README.macos.md` | Keep Gatekeeper / Open Anyway. Point Debug signing failures to Enable debugging (not manual codesign). |
| `code_signing.md` | Advanced / recovery only; clarify ad-hoc vs cert; product path is Enable debugging. |

### Milestones

| # | Status | Work |
|---|--------|------|
| **M1** | **DONE** (tree) | `--gdb-sign=` = `normal` / `unsigned` / `no-entitlement`; lab marker + docs; force Tier 2 deferred to M4 runtime. |
| **M2** | **DONE** (tree) | `gdbmacossigningdiagnose`: bundled path only; `codesign` verify + `cs.debugger`; session classifiers (bare No stack vs task-port/taskgated); quarantine noted separately; logs evidence only (no prompt / no repair). |
| **M3** | **DONE** (tree) | Tier 1 restore (`TryMacGdbTier1Restore`); skip if valid; bundled-only; ships `Resources/debugger/gdb.entitlements`; **Debug → Enable debugging on this Mac…** enabled only after diagnosis suggests repair and debugger is stopped (auto-offer remains M5). |
| **M4** | **DONE** (tree) | Tier 2 (`codeblocks-gdb` in the **System** keychain; admin prompt; login copy removed); force via env/`ForceMacGdbTier2`; Enable debugging runs Tier 1 then Tier 2 when needed. |
| **M5** | **DONE** (tree) | Consent prompt on confirmed failure (**Enable debugging** / **Not now** / **Don't ask again**); Debugger pref re-enables offers; menu remains; README + `code_signing.md`. |
| **M6** | **PLANNED** | Isolate Tier 1 / Tier 2 provisioning in a macOS-only in-process component. Same behaviour. `#if` gate; no Windows/Linux stubs; no separate executable. |

### M6 — in-process macOS provisioning component (planned, not started)

M6 does **not** change what the user sees and does **not** redesign Tier 1, Tier 2, the certificate, the System keychain, or the admin prompts. It separates the macOS provisioning code from diagnosis and UI, and leaves that code inside the IDE process Gatekeeper has already allowed to run.

A separate executable is out of scope for v2.3. This distribution is ad-hoc only. On the development system used for v2.3 testing, when the .app retains com.apple.quarantine, launching a nested ad-hoc Mach-O has been observed to terminate with SIGKILL without presenting a second Open Anyway dialog. The repair therefore stays in the existing codeblocks process rather than introducing a separate executable with its own launch and code-signing considerations.

```text
diagnosis / UI (policy)
  session evidence, offer, menu, DecideMacGdbTier1 / DecideMacGdbTier2
        |
        |  after consent; macOS call site only
        v
provisioning component (same process)
  performs the mutation it was asked to perform
  post-provisioning verification of that mutation
  reports what happened
        |
        v
caller
  logs, existing dialogs, manual Debug retry
  does not ask the component what the product should do
```

**Stays with diagnosis and UI** (`gdbmacossigningdiagnose`, `debuggergdb`): session evidence; task-port / `taskgated` classification; bundled vs external; `WouldOfferEnableDebugging`; consent, silence, and the menu; `DecideMacGdbTier1` / `DecideMacGdbTier2`; whether the original Debug failure counts as repaired; whether the user should retry Debug. Debug retry stays manual.

**Moves into the macOS-only component:** certificate and key creation; System-keychain import; login-copy removal; trust setting; temp secrets; companion-dylib signing; bundled GDB signing. Both tier mechanisms live there. The caller decides whether Tier 2 is indicated (`taskPortOrTaskgated` or the lab force flag) and passes that in. The component does not receive `GdbMacSessionEvidence` and does not re-classify the debugger session.

**Platform gate.** Declarations, definitions, and call sites are all inside `#if defined(__WXMAC__)`. On any other build there is no provisioning API, no provisioning implementation, and no provisioning call site. The header exposes provisioning declarations only inside that gate. The `.cpp` may stay on the ordinary plugin source list; the preprocessor is the platform boundary. Do not add Windows or Linux stubs, fake non-macOS return values, a stub `.cpp`, or a `configure` source swap. The moved repair entry points must not remain on the ungated diagnosis header with a “not built for macOS” result. Diagnosis and policy declarations that already exist for every platform stay where they are; M6 does not wrap those in a new cross-platform abstraction.

**Inputs.** No path, entitlement, keychain, or identity parameters. The component uses the bundled `Resources/debugger/bin/gdb` and `gdb.entitlements` only, and the fixed name `codeblocks-gdb`. The caller passes only the mechanism switches it has already decided: whether to run Tier 2, and whether lab force must resign even when that identity is already in use. A configured debugger that is missing or outside the bundle never reaches it (`GdbMacPathKind` is checked first, as today).

**Result.** A small mechanism-level result so the caller can tell these outcomes apart:

- already healthy, nothing mutated;
- Tier 1 restored;
- Tier 2 provisioned;
- refused by a safety check (containment, missing `gdb` or entitlements, not the bundled payload);
- cancelled, only where the current code already recognises a cancel (trust panel `OSStatus` -60006 or -128);
- provisioning failed.

Exact type and function names are chosen at implementation time to match the tree. The result may include text for the existing debugger log, as `FormatGdbMacTier1ResultForLog` / `FormatGdbMacTier2ResultForLog` do now. It does **not** report whether Enable debugging should be offered, whether Tier 2 is indicated, whether offers should be silenced, whether a task-port failure was confirmed, or whether Debug should be retried. The caller keeps today’s dialogs: a recognised cancel is still “repair did not finish,” not a new dialog.

**Post-provisioning verification** (owned by the component) confirms only the mutation just performed:

- `codesign` verification succeeds for that tier;
- `com.apple.security.cs.debugger` is present on bundled `gdb`;
- Tier 1 ends as a valid ad-hoc signature, Tier 2 as the named `codeblocks-gdb` signature;
- every resolved or mutated file stays under `Resources/debugger/`.

That is the same post-sign bar as today’s “post-sign verify still defective” / “expected named + cs.debugger” checks. It is not debugger-session diagnosis. `WouldOfferEnableDebugging`, task-port classification, and `DecideMacGdbTier*` stay outside.

**Fail closed.** `realpath` of `gdb`, the entitlements file, and each companion `lib/*.dylib` must stay under `Resources/debugger/`. A symlink or `..` that escapes is a refusal, before `openssl`, `security`, or `codesign`. Missing `gdb` or entitlements is a refusal.

**Unchanged mechanism.** User-session OpenSSL; `sudo -A` with the `osascript` askpass and `script(1)`; `SecTrustSettingsSetTrustSettings` via `dlopen` of Security.framework (not linked); then `codesign`. Temp directory stays `mkdtemp` of `cb-gdb-cert-XXXXXX`, mode 0700, the same seven named files, then `rmdir`. No recursive delete. No `SMAppService`, `SMJobBless`, XPC, launchd job, or installer. Re-signing nested `gdb` may leave the outer ad-hoc bundle seal invalid, as it already can after the bundle script’s second `gdb` signature. M6 does not resign `CodeBlocks.app` and does not change `make_cb_app_bundle.sh`. S11 checks that the app still launches, not that `codesign --verify --deep` on the whole app passes.

**Build touch list:** new macOS-gated sources next to the debugger plugin (header plus one `.cpp`), and the existing repair call sites so they call the component from inside `#if defined(__WXMAC__)`. No `configure.ac` Darwin target, no `src/tools` program, no `make_cb_app_bundle.sh` change, no new bundle path. Unchanged: `osx/gdb.entitlements`, sister `binutils-gdb`, the M5 dialogs, certificate shape, and where `Resources/debugger/` sits.

**M6 acceptance**

- S1–S11 still hold, with the same prompts and the same tier outcomes.
- Repair still runs only after consent, only for the bundled GDB, and only inside the IDE process.
- On non-macOS builds the provisioning declarations, implementation, and call sites are gone. No stub returns a fake result.
- No `cb-gdb-setup` binary is built or packaged.

### Acceptance matrix

| ID | Steps | Expected |
|----|-------|----------|
| **S1** | **normal** bundle; Enable debugging (menu) | Healthy / no-op; no unnecessary resign; no new cert. |
| **S2** | **unsigned**; Debug | Confirmed signing failure → **prompt** (not silent repair). Consent → Tier 1 restores ad-hoc + entitlement; Debug works; no Tier 2 unless forced. |
| **S3** | **no-entitlement**; Debug → consent | Tier 1 restores entitlement (and valid signature as needed); Debug works. |
| **S4** | Valid ad-hoc+entitlement + unrelated failure / bare No stack without task-port evidence | No prompt for Enable debugging; no signing mutation. |
| **S5** | `gdb` not found / missing executable | **No** Enable debugging prompt; existing missing-debugger handling only. |
| **S6** | **force Tier 2** (dev) after consent | Tier 2 runs; identity reused on repeat; bundled gdb signed with trusted identity + entitlement; Debug works on this Mac. |
| **S7** | Prompt → **don’t ask again**; reproduce failure | No automatic offer; menu Enable debugging still works; Debugger pref can re-enable offers. |
| **S8** | Repeat Enable debugging after fix | Idempotent; no duplicate certs; no damage. |
| **S9** | Custom debugger path outside the bundle | Helper refuses to modify it; no false Enable debugging as if bundled. |
| **S10** | Gatekeeper-only launch | Open Anyway still sufficient to **launch**; Enable debugging not required merely to open the IDE. |
| **S11** | After a successful Tier 1, quit and relaunch the same bundle; repeat separately after a successful Tier 2 | The app still launches. Bundled `gdb` still has `com.apple.security.cs.debugger` and the signature just provisioned (ad-hoc after Tier 1; `codeblocks-gdb` after Tier 2). Debug still works, and provisioning does not run again just because the app was relaunched. |

**Residual limitation (documented, not a fail):** this one-Mac environment cannot validate every security decision macOS may make on an unrelated downloaded installation (including valid ad-hoc+entitlement refused by `taskgated` elsewhere). S11 does not change that. It checks persistence on this Mac only, and it does not require the outer app seal to verify.

### Regression / safety

| Risk | Mitigation |
|------|------------|
| Unsigned release | Injection default off; packaging must not ship lab modes. |
| Wrong binary | Bundled gdb path only. The macOS provisioning component takes no path argument; a symlink that leaves `Resources/debugger/` is a refusal. |
| Blind resign / No stack | Evidence-driven diagnosis; Tier 1 skipped when already valid; no prompt without evidence. |
| Silent repair without consent | Always prompt; never mutate until user consents. |
| Prompt when gdb missing | Guard: executable must exist / be found first. |
| Quarantine overreach | Separate from debugger repair unless diagnosed as required. |
| `--deep` drops entitlements | Entitlements after any deep resign. |

### REQUIRED / OPTIONAL / OUT OF SCOPE

**REQUIRED**

1. **Default happy path unchanged:** release DMG ships ad-hoc signed bundled `gdb` + `cs.debugger`; helper is repair-only.  
2. Enablement path satisfying the user invariant when repair is needed.  
3. On **confirmed** task-port / signing failure: **offer** Enable debugging (consent required before any repair).  
4. Do **not** offer when `gdb` is not found; do **not** repair on bare No stack without signing evidence.  
5. User may silence future automatic offers; Debugger preferences can re-enable them; menu entry remains.  
6. After consent: evidence-driven Tier 1 then Tier 2 (user does not pick the tier).  
7. One-Mac lab: normal / unsigned / no-entitlement + force Tier 2; documented residual limitation.  
8. Bundled GDB only; custom paths untouched.  
9. Docs: Gatekeeper ≠ debugger; manual codesign = recovery; happy path = pre-signed DMG.  
10. No second Mac as acceptance gate.
11. **M6:** provisioning stays in-process. Its declarations, definitions, and call sites exist only inside `#if defined(__WXMAC__)`. The result reports what the mutation did, not product policy. Tier 1 / Tier 2 semantics stay as implemented in M3–M5. S11 checks that a successful repair is still there after relaunch.

**OPTIONAL**

- None for M6.

**OUT OF SCOPE**

- Making the helper part of default install or a mandatory post-install step.  
- Replacing the shipped ad-hoc+entitlement `gdb` with “users must run Enable debugging first.”  
- Fully automatic repair with no consent dialog.  
- Distributing private keys.
- Second Mac / VM as required validation.  
- Claiming Tier 1 fixes valid-ad-hoc-rejected-by-taskgated.  
- LLDB / DAP; sister-repo codesign requirement; replacing Open Anyway; v2.2 reopen.
- A separate repair executable (`cb-gdb-setup` or otherwise).
- `SMAppService`, `SMJobBless`, XPC, launchd, or an installer package.
- Windows/Linux stub implementations of the macOS provisioning component.

### Interaction with prior versions

| Version | Relationship |
|---------|----------------|
| **v1.2** | Bundled gdb + entitlement; manual signing docs → v2.3 automates recovery. |
| **v2.0–v2.2** | Unchanged; Enable debugging is orthogonal to ownership / Watches / console. |
| **Gatekeeper README** | First-launch only; v2.3 adds Debug enablement. |

### Outcome (when done)

**Happy path:** users install the DMG with the same ad-hoc signed + entitled bundled `gdb` as today; after Open Anyway, Debug works without the helper when macOS accepts that signature. **Repair path:** on confirmed debugger authorization failure, Code::Blocks offers Enable debugging; repair runs only after consent, with tiers chosen automatically, inside the IDE process. **M6** only isolates that macOS code; it does not add a program to the bundle. Users can silence or re-enable those offers in Debugger preferences. They authorize macOS when asked; they do not run manual codesign as the normal path. This Mac’s lab covers injection, both tiers, consent/silence, and idempotence; residual cross-Mac `taskgated` behaviour stays an explicit limitation.
