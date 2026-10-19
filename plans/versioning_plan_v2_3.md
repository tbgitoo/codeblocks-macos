# v2.3 — Enable debugging without manual codesign (local helper)

Detail plan for this version. Overview and status table: [versioning_plan.md](versioning_plan.md).

---

**Status: Planned.** Do not implement until this plan is accepted. Does not reopen v2.2 M1–M6. Does not require a paid Apple Developer Program membership.

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
       └─ Consent → diagnose → Tier 1 / Tier 2 in background → retry Debug
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
| **Orchestration** | Code::Blocks owns diagnosis and user-facing flow. If privileged mutation is required, use the **smallest** practical helper — do not make the full IDE a generic privileged signing surface. Do not mandate a privileged helper until testing shows it is necessary. |
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

### Tier 2 identity lifecycle (implementation questions)

Resolve experimentally at implement time; mark as acceptance items, do not invent answers now:

- Deterministic product-specific identity name  
- Keychain / storage location  
- Certificate and private-key ownership  
- Detect and reuse an existing valid identity (no duplicate spam)  
- Trust settings and required macOS authorisation  
- Expiry / recovery  
- Upgrades / re-signing after app updates  
- Multi-user implications if relevant  
- Restriction to bundled GDB  
- Partial-failure handling (trust set but sign failed, etc.)

### Developer lab (this Mac only)

Injectable states (mechanism illustrative — env flag / packaging switch; default = normal release signing **on**):

| State | Intent |
|-------|--------|
| **normal** | Ad-hoc + debugger entitlement (healthy / no-op) |
| **unsigned** | Missing signature |
| **no-entitlement** | Signed but `cs.debugger` absent |
| **force Tier 2** | Dev-only injection to exercise Tier 2 even when this Mac accepts ad-hoc — **not** claimed as natural `taskgated` rejection |

**Lab may prove:** diagnosis; detect missing/invalid signing; entitlement detection; Tier 1 repair; Tier 2 create/trust/sign; verification; idempotence; UI / error handling; bundled-path-only restriction.

**Lab must not claim:** that unsigned or entitlement-stripped `gdb` faithfully reproduces  
`valid ad-hoc + valid entitlement + taskgated refusal on another Mac`.  
That remains an **explicit residual validation limitation**.

Never ship a release DMG with signing off or with force-Tier-2 injection enabled.

### Docs impact

| Doc | Direction |
|-----|-----------|
| `README.macos.md` | Keep Gatekeeper / Open Anyway. Point Debug signing failures to Enable debugging (not manual codesign). |
| `code_signing.md` | Advanced / recovery only; clarify ad-hoc vs cert; product path is the helper. |

### Milestones

| # | Status | Work |
|---|--------|------|
| **M1** | Planned | Dev injection of **normal / unsigned / no-entitlement** (+ optional force Tier 2); document what the lab proves and does not prove. |
| **M2** | Planned | Diagnosis: bundled path only; signature; entitlements; distinguish task-port/signing evidence from bare No stack; quarantine noted separately. |
| **M3** | Planned | Tier 1 restoration when defective; verify; skip resign when already valid; idempotent. |
| **M4** | Planned | Tier 2 path + force-injection; resolve identity lifecycle questions enough to ship safely; idempotent. |
| **M5** | Planned | Consent prompt on confirmed failure; silence + Debugger pref to re-enable; menu; README + `code_signing.md`; acceptance S1–S10. |

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
| Wrong binary | Bundled gdb path only. |
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

**OPTIONAL**

- Privileged helper only if testing shows in-process signing is insufficient.

**OUT OF SCOPE**

- Making the helper part of default install or a mandatory post-install step.  
- Replacing the shipped ad-hoc+entitlement `gdb` with “users must run Enable debugging first.”  
- Fully automatic repair with no consent dialog.  
- Developer ID / notarization / Program fee.  
- Distributing private keys.  
- Second Mac / VM as required validation.  
- Claiming Tier 1 fixes valid-ad-hoc-rejected-by-taskgated.  
- LLDB / DAP; sister-repo codesign requirement; replacing Open Anyway; v2.2 reopen.

### Interaction with prior versions

| Version | Relationship |
|---------|----------------|
| **v1.2** | Bundled gdb + entitlement; manual signing docs → v2.3 automates recovery. |
| **v2.0–v2.2** | Unchanged; Enable debugging is orthogonal to ownership / Watches / console. |
| **Gatekeeper README** | First-launch only; v2.3 adds Debug enablement. |

### Outcome (when done)

**Happy path:** users install the DMG with the same ad-hoc signed + entitled bundled `gdb` as today; after Open Anyway, Debug works without the helper when macOS accepts that signature. **Repair path:** on confirmed debugger authorization failure, Code::Blocks offers Enable debugging; repair runs only after consent, with tiers chosen automatically. Users can silence or re-enable those offers in Debugger preferences. They authorize macOS when asked; they do not run manual codesign as the normal path. This Mac’s lab covers injection, both tiers, consent/silence, and idempotence; residual cross-Mac `taskgated` behaviour stays an explicit limitation. No paid Apple Program.
