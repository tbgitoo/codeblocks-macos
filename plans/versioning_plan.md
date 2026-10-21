# Code::Blocks macOS — versioning plan (overview)

Roadmap for the unofficial Apple Silicon port in this repository. Versions are sequential: finish and test each before starting the next major step in the console/debugger line.

**Detailed plans** live in sibling files (one per version). Keep this overview short; put investigation notes, milestone matrices, and architecture contracts in the detail file for that version.

```text
  → v1.0 (native macOS port + packaging)
  → v1.1 (Terminal console polish + Darwin inferior-tty-claim)
  → v1.2 (bundled Darwin GDB in the .app)
  → v2.0 (integrated Program Console / PTY pane)
  → v2.1 (Debug single open file, no project)
  → v2.2 (standalone New → Save as C|C++ → Build/Run/Debug; tab Rename; readable Watches)
  → v2.3 (Enable debugging — repair-only, in-process; no manual codesign)
  → v3.0 (existing SFML project wizard usable on Apple Silicon)
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

## Version index

| Version | Status (summary) | Detail plan |
|---------|------------------|-------------|
| **v1.0** | **Released** — tag `v1.0` @ `e5bc580`. Native arm64 app + DMG; Terminal `$SCRIPT` fixes; GDB external. | [versioning_plan_v1_0.md](versioning_plan_v1_0.md) |
| **v1.1** | **Released** — tag `v1.1` @ `cf3e4f0`. Claim-off handshake; `cb_debug_console_hold`; single Terminal window. | [versioning_plan_v1_1.md](versioning_plan_v1_1.md) |
| **v1.2** | **Released** — through `80d8e89`. Bundled companion GDB (`5564fbe`); debugger entitlement; `code_signing.md`. | [versioning_plan_v1_2.md](versioning_plan_v1_2.md) |
| **v2.0** | **In progress** — M1–M5 DONE in tree (Program Console default; Terminal.app optional; focus policies). | [versioning_plan_v2_0.md](versioning_plan_v2_0.md) |
| **v2.1** | **DONE** (tree) — standalone Debug / Step Into (DebuggerGDB). Tag/release when ready. | [versioning_plan_v2_1.md](versioning_plan_v2_1.md) |
| **v2.2** | **DONE** (tree) — M1–M6 in UI (ownership, Save As C/C++, Rename, unbuffered Debug stdout, readable `std::string` Watches). Tag/release when ready. | [versioning_plan_v2_2.md](versioning_plan_v2_2.md) |
| **v2.3** | **Complete** (tree) — M1–M6. Tag/release when ready. | [versioning_plan_v2_3.md](versioning_plan_v2_3.md) |
| **v3.0** | **M2 DONE** (tree). M3+ not started. Shared Homebrew locate / `--version` / `--prefix`. SFML is still the only client and is not wired yet. | [versioning_plan_v_3_0.md](versioning_plan_v_3_0.md) |

### One-line outcomes

| Version | Outcome |
|---------|---------|
| **v1.0** | Documented build → `_cb_install` → `CodeBlocks.app` → `.dmg`; Terminal Run/Debug/Tools once GDB is configured. |
| **v1.1** | No long Darwin claim warning with matching companion GDB; relocatable console helpers; one Terminal window on cold start. |
| **v1.2** | Fresh macOS install gets bundled GDB under `Resources/debugger/` by default (entitlement + signing docs). |
| **v2.0** | In-IDE Program Console (CB-owned PTY) as default console Run/Debug; Terminal.app remains optional. |
| **v2.1** | Debug / Step Into a single saved C/C++ file with no project (always rebuild for debug). |
| **v2.2** | Active-source ownership + PREPARE→COMMIT for Build/Run/Debug; Save As C/C++ split; tab Rename; unbuffered Debug stdout; human-readable Apple-libc++ `std::string` Watches. |
| **v2.3** | DMG still ships ad-hoc+entitled gdb. Enable debugging appears only after a confirmed signing or task-port failure; repair stays in-process and waits for consent. |
| **v3.0** | File → New → Project → SFML project → Build and Run yields a native window. A small shared Homebrew helper serves wizards; SFML is the only client. Install is explicit. Homebrew itself is not installed by the IDE. SFML stays out of the DMG. |

---

## How to maintain these plans

1. Update **status** and one-line outcome in **this** overview when a version ships or a milestone completes.
2. Put investigation notes, locked decisions, milestone matrices, and acceptance tests in the **version detail** file only.
3. Do not re-merge detail back into the overview; link instead.
