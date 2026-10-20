# v2.3 — Enable debugging without manual codesign (local helper)

Detail plan for this version. Overview and status table: [versioning_plan.md](versioning_plan.md).

---

**Status: M1–M5 DONE in tree. M6 planned (not started).** Does not reopen v2.2 M1–M6. Does not require a paid Apple Developer Program membership.

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
       └─ Consent → IDE chooses the tier → cb-gdb-setup runs Tier 1 / Tier 2
            → IDE re-diagnoses (helper report is not success) → user retries Debug
  → Menu “Enable debugging…” also available for manual repair
```

### Product decisions (locked)

| Topic | Decision |
|-------|----------|
| **Default ship format** | **Same as now:** release DMG / app bundle ships bundled `gdb` **ad-hoc signed** with `com.apple.security.cs.debugger`. That is the happy path. |
| **Helper role** | **Repair only.** Not the default install; not required when Debug already works. |
| **User invariant** | Authorise macOS security changes; **no** Keychain Assistant / terminal `codesign` as the primary path. |
| **Apple Developer Program** | **Out of scope** — no Developer ID, no notarization requirement. |
| **Gatekeeper vs debugger** | Separate. Open Anyway = run the app. Debugger attach = `taskgated` + nested `gdb` signature/entitlements. Do not conflate in UI or docs. |
| **What we ship** | Release bundles keep nested `gdb` **ad-hoc signed** with `com.apple.security.cs.debugger` (`make_cb_app_bundle.sh`, default **on**). Lab “sign off” exists only to inject repair scenarios. |
| **Repair scope** | Helper may modify **only** the bundled Code::Blocks `Resources/debugger/bin/gdb` (and its companion signed dylibs if required). **Never** resign arbitrary custom debugger paths. |
| **Ad-hoc vs certificate** | Alternatives, not a chain. Ad-hoc needs no certificate. Local trusted identity is **Tier 2 escalation**, not the default ship format. |
| **Prompt on confirmed failure** | **REQUIRED.** On first **confirmed** task-port / signing authorization failure, offer Enable debugging. **Not** when `gdb` is missing / not found. **Not** for bare “No stack” without signing evidence. |
| **Consent before repair** | Repair **never** proceeds automatically. The user must explicitly consent. After consent, Tier 1/Tier 2 selection runs without asking the user to choose a tier. |
| **Silence / re-offer** | At the prompt, the user may allow asking again on later failures, or silence future automatic offers. If silenced, **Debugger preferences** must expose a control to re-enable automatic offers. Manual **Enable debugging…** remains available regardless (unless a later UX finding says otherwise — default: keep menu). |
| **Orchestration** | The IDE owns diagnosis, consent, and tier choice. After consent, certificate, keychain, trust, and `codesign` run in the short-lived **`cb-gdb-setup`** helper (M6), not in the IDE process. Not a persistent privileged service. The IDE declares success only after its own diagnosis. |
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

**Lab may prove (M6):** the same S1–S10 outcomes after the IDE no longer signs; helper refuses paths and out-of-bundle copies; IDE success follows its own diagnosis.

**Lab must not claim:** that unsigned or entitlement-stripped `gdb` faithfully reproduces  
`valid ad-hoc + valid entitlement + taskgated refusal on another Mac`.  
That remains an **explicit residual validation limitation**.

Never ship a release DMG with `unsigned` / `no-entitlement` or with force-Tier-2 injection enabled.

### Docs impact

| Doc | Direction |
|-----|-----------|
| `README.macos.md` | Keep Gatekeeper / Open Anyway. Point Debug signing failures to Enable debugging (not manual codesign). |
| `code_signing.md` | Advanced / recovery only; clarify ad-hoc vs cert; product path is Enable debugging. M6 notes that provisioning runs in `cb-gdb-setup`. |

### Milestones

| # | Status | Work |
|---|--------|------|
| **M1** | **DONE** (tree) | `--gdb-sign=` = `normal` / `unsigned` / `no-entitlement`; lab marker + docs; force Tier 2 deferred to M4 runtime. |
| **M2** | **DONE** (tree) | `gdbmacossigningdiagnose`: bundled path only; `codesign` verify + `cs.debugger`; session classifiers (bare No stack vs task-port/taskgated); quarantine noted separately; logs evidence only (no prompt / no repair). |
| **M3** | **DONE** (tree) | Tier 1 restore (`TryMacGdbTier1Restore`); skip if valid; bundled-only; ships `Resources/debugger/gdb.entitlements`; **Debug → Enable debugging on this Mac…** enabled only after diagnosis suggests repair and debugger is stopped (auto-offer remains M5). |
| **M4** | **DONE** (tree) | Tier 2 (`codeblocks-gdb` in the **System** keychain; admin prompt; login copy removed); force via env/`ForceMacGdbTier2`; Enable debugging runs Tier 1 then Tier 2 when needed. |
| **M5** | **DONE** (tree) | Consent prompt on confirmed failure (**Enable debugging** / **Not now** / **Don't ask again**); Debugger pref re-enables offers; menu remains; README + `code_signing.md`. |
| **M6** | **PLANNED** | Extract the working provisioning machinery into `cb-gdb-setup`. Same Tier 1 / Tier 2 behaviour. IDE keeps diagnosis, consent, and the post-run check. |

### M6 — `cb-gdb-setup` (planned, not started)

M6 does **not** redesign Tier 1, Tier 2, the certificate, the System keychain, or the admin prompts. It moves the working mutation out of the IDE process into a short-lived macOS-only helper that ships inside this repository and inside `CodeBlocks.app`.

```text
Code::Blocks IDE
  detects a bundled-GDB setup problem
  decides whether setup is appropriate
  asks the user
  invokes cb-gdb-setup
        |
        v
cb-gdb-setup
  validates its own bundle layout
  Tier 1 and/or Tier 2, bundled GDB only
        |
        v
IDE runs DiagnoseMacGdbSigning again
  helper stdout is not enough to declare success
```

**Stays in the IDE:** session evidence; task-port classification; bundled vs external; offer / silence / menu; tier choice; locating and launching the helper; interpreting its result; independent diagnosis. Debug retry stays manual.

**Moves to the helper:** bundle-layout check; certificate and key creation; System-keychain import; login-copy removal; trust setting; temp secrets; companion-dylib signing; bundled GDB signing; the helper's own post-sign check. Both tiers live entirely in the helper. The IDE still decides whether Tier 2 is indicated.

Do **not** put the provisioning code in a library the plugin links. That would load it back into the IDE.

**Privilege model:** extract what works today. User-session OpenSSL, `sudo -A` with the `osascript` askpass and `script(1)`, then `SecTrustSettingsSetTrustSettings` via `dlopen` of Security.framework (not linked), then `codesign`. No `SMAppService`, `SMJobBless`, XPC, launchd job, or installer. First Tier 2 run must confirm the trust panel still appears when the caller is the helper; if it does not, stop and report that. Do not invent a new privilege design in that case.

**Location and lifetime**

| Item | Choice |
|------|--------|
| Source | `src/tools/MacGdbSetup/` |
| Name | `cb-gdb-setup` |
| `make install` | `libexec_PROGRAMS` → `$(libexecdir)/cb-gdb-setup` (not `bindir`, not `PATH`) |
| Bundle | `CodeBlocks.app/Contents/MacOS/cb-gdb-setup`, next to `codeblocks` |
| Lookup | `ConfigManager::GetExecutableFolder()`, never `PATH` |
| Process | One synchronous run per consented repair |
| Link | C++17. No wx. No Security.framework. UI stays `osascript` plus the Security trust panel. |

A copy run from `libexec` or from anywhere outside `Contents/MacOS/` fails the layout check and signs nothing.

**Command line** (nothing else is accepted):

```text
cb-gdb-setup [--tier2] [--force-tier2]
```

No `install` subcommand: there is only one operation. No path, entitlement, keychain, or identity arguments. The helper ignores `CB_FORCE_GDB_TIER2`; the IDE reads that env var and the lab checkbox and passes `--force-tier2`.

| Invocation | Meaning (same rules as M3–M5) |
|------------|--------------------------------|
| `cb-gdb-setup` | Tier 1 only if signature or entitlement is defective. Already valid → no change. |
| `--tier2` | After Tier 1 if needed, Tier 2 unless already signed by `codeblocks-gdb` with the entitlement. This is the task-port case. The helper cannot see GDB output, so the IDE must pass this. |
| `--force-tier2` | Tier 2 even when that identity is already in use. Implies `--tier2`. |

The IDE launches the helper only after consent, and only when diagnosis says the configured executable **is** the bundled GDB. A custom path never reaches the helper.

**Stdout** is one JSON object and nothing else. No PEM, keys, or passwords in `message`.

```json
{"protocol":1,"result":"success","stage":"complete","message":"Bundled GDB debugging enabled"}
```

- `protocol` is `1`.
- `result`: `success`, `skipped`, `failed`, `refused`, `cancelled`.
- `stage`: `arguments`, `layout`, `tier1`, `identity`, `keychain`, `trust`, `sign-dylib`, `sign-gdb`, `verify`, `complete`.

Stderr may carry the existing `codesign` / `openssl` text and must not contain secret file contents. Exit `0` = success or skipped, `2` = cancelled (askpass dismissed, or trust status -60006 or -128), `1` = failed or refused. Any other exit, a signal, extra stdout, or an unknown protocol is abnormal: the IDE does not show success and still diagnoses. Exit 0 alone is not success.

**Layout check (fail closed).** From `_NSGetExecutablePath` and `realpath`:

1. Resolved path ends with `/Contents/MacOS/cb-gdb-setup`, and `../MacOS/codeblocks` exists in that bundle.
2. Mutable targets are only `Contents/Resources/debugger/bin/gdb`, reading `gdb.entitlements`, and regular `Contents/Resources/debugger/lib/*.dylib`.
3. `realpath` of each target stays under `realpath(Contents/Resources/debugger/)`. Symlinks, `..`, and aliases that escape are refusals. Missing `gdb` or entitlements is a refusal.
4. Any other argv token is a refusal, before `openssl`, `security`, or `codesign`.

Temp files stay the current `mkdtemp` `cb-gdb-cert-XXXXXX` directory, mode 0700, seven named files, then `rmdir`. No recursive delete.

**Bundle signature.** Re-signing nested `gdb` already breaks the outer ad-hoc seal (the bundle script signs the app `--deep`, then signs `gdb` again). M6 does not move GDB out of the app. Before acceptance, record before and after a Tier 1 repair and a Tier 2 repair:

```text
codesign --verify --deep --strict --verbose=4 CodeBlocks.app
codesign -dv --verbose=4 CodeBlocks.app/Contents/MacOS/cb-gdb-setup
codesign -d --entitlements :- CodeBlocks.app/Contents/Resources/debugger/bin/gdb
spctl --assess --type execute --verbose=4 CodeBlocks.app
```

Acceptable: outer verify fails after `gdb` or a dylib is re-signed, and the IDE still launches. Not acceptable: helper missing, helper carrying `cs.debugger`, or Tier 1 dropping that entitlement from `gdb`.

**Build touch list:** `configure.ac` (`CODEBLOCKS_DARWIN`, `AC_CONFIG_FILES`), `src/tools/Makefile.am`, new `src/tools/MacGdbSetup/`, `gdbmacossigningdiagnose.*`, `debuggergdb.cpp`, `make_cb_app_bundle.sh`, `README.macos.md`, `code_signing.md`, `ChangeLog`, this plan. Unchanged: `osx/gdb.entitlements`, sister `binutils-gdb`, the M5 dialogs, certificate shape, and where `Resources/debugger/` sits.

**M6 acceptance**

- After consent, the IDE process does not run `openssl`, `security import`, `SecTrustSettingsSetTrustSettings`, or `codesign` on GDB. The helper does, with the same prompts and the same tier outcomes.
- S1–S10 still hold.
- Helper copied outside the bundle, or given a path argument, refuses and signs nothing.
- Missing helper, bad JSON, unknown protocol, crash, or cancel: no success dialog; diagnosis still runs.
- Linux and Windows builds do not produce `cb-gdb-setup`.

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

**Residual limitation (documented, not a fail):** this one-Mac environment cannot validate every security decision macOS may make on an unrelated downloaded installation (including valid ad-hoc+entitlement refused by `taskgated` elsewhere).

### Regression / safety

| Risk | Mitigation |
|------|------------|
| Unsigned release | Injection default off; packaging must not ship lab modes. |
| Wrong binary | Bundled gdb path only. The helper derives that path from its own location and accepts no path arguments. |
| Helper as a general signer | Unknown argv and any layout escape fail closed, before `codesign`. |
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
10. No paid Apple membership; no second Mac as acceptance gate.
11. **M6:** provisioning runs in `cb-gdb-setup` with the contract in the M6 section. Tier 1 / Tier 2 semantics stay as implemented in M3–M5.

**OPTIONAL**

- None for M6. A persistent privileged helper is not an optional upgrade of this milestone.

**OUT OF SCOPE**

- Making the helper part of default install or a mandatory post-install step.  
- Replacing the shipped ad-hoc+entitlement `gdb` with “users must run Enable debugging first.”  
- Fully automatic repair with no consent dialog.  
- Developer ID / notarization / Program fee.  
- Distributing private keys.  
- Second Mac / VM as required validation.  
- Claiming Tier 1 fixes valid-ad-hoc-rejected-by-taskgated.  
- LLDB / DAP; sister-repo codesign requirement; replacing Open Anyway; v2.2 reopen.
- `SMAppService`, `SMJobBless`, XPC, launchd, or an installer package.
- Moving `Resources/debugger` outside `CodeBlocks.app` so repair does not affect the outer signature.
- `remove` / `uninstall`, certificate listing, identity management, or signing a caller-supplied path.
- Linking Security.framework, or a shared signing library loaded by the IDE.

### Interaction with prior versions

| Version | Relationship |
|---------|----------------|
| **v1.2** | Bundled gdb + entitlement; manual signing docs → v2.3 automates recovery. |
| **v2.0–v2.2** | Unchanged; Enable debugging is orthogonal to ownership / Watches / console. |
| **Gatekeeper README** | First-launch only; v2.3 adds Debug enablement. |

### Outcome (when done)

**Happy path:** users install the DMG with the same ad-hoc signed + entitled bundled `gdb` as today; after Open Anyway, Debug works without the helper when macOS accepts that signature. **Repair path:** on confirmed debugger authorization failure, Code::Blocks offers Enable debugging; repair runs only after consent, with tiers chosen automatically. **M6 (when implemented):** that repair runs in `cb-gdb-setup`, and the IDE still checks the result itself. Users can silence or re-enable those offers in Debugger preferences. They authorize macOS when asked; they do not run manual codesign as the normal path. This Mac’s lab covers injection, both tiers, consent/silence, and idempotence; residual cross-Mac `taskgated` behaviour stays an explicit limitation. No paid Apple Program.
