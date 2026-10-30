# Code::Blocks v3.7 — modernise the MCS51 wizard and simulator-backed execution on macOS

**Status: DONE.** The Scripted Wizard MCS51 project entry now creates a working MCS51/8051 project on the tested Apple Silicon macOS configuration. SDCC is supplied through Homebrew, Code::Blocks refreshes its existing SDCC compiler profile, generated projects build successfully, and Intel HEX output can be run interactively through `s51` from either the integrated Program Console or Terminal.app.

**Question this version answers:** can the existing MCS51 wizard remain a genuinely useful modern project type rather than a historical template that leaves compiler installation, configuration, and execution to the user?

The answer in v3.7 is yes. The existing MCS51 wizard and SDCC compiler profile were already fundamentally sound. The required work was to modernise macOS toolchain discovery, provide the established Homebrew-assisted setup, refresh the live compiler profile, and make Run launch the MCS51 image through a simulator rather than trying to execute the `.ihx` file as a native macOS program.

The work also exposed and fixed a regression in standalone-file Terminal.app execution introduced around the Program Console path.

---

## 1. Scope and architecture

The MCS51 wizard remains an ordinary Code::Blocks embedded project using the existing SDCC compiler profile:

```text
Code::Blocks
    |
    +-- existing MCS51 scripted wizard
    +-- existing SDCC compiler profile
    +-- normal Debug / Release targets
    +-- external Homebrew SDCC toolchain
    |
    +-- MCS51 Intel HEX image
            |
            +-- s51 / uCsim simulator
```

SDCC and Homebrew remain external. No new MCS51 compiler backend, package manager, build system, or simulator subsystem was introduced.

The legacy MCS51 configuration for memory models, CODE/IDATA/XDATA sizing, Intel HEX/S19 output, SDCC, Keil and IAR is retained. On macOS, SDCC is the tested and supported wizard path.

---

## 2. Homebrew and SDCC integration

The wizard reuses the existing macOS Homebrew helpers to query or explicitly install the `sdcc` formula. Code::Blocks does not install Homebrew itself.

The tested Homebrew SDCC installation supplies the compiler and tools required by the workflow, including `sdcc`, `sdar`, `packihx` and `s51`.

The existing SDCC compiler definition previously relied on the stale non-Windows fallback:

```text
/usr/local/share/sdcc
```

The compiler XML is made Apple Silicon/Homebrew-aware by searching executable locations such as:

```text
/opt/homebrew/bin
/usr/local/bin
```

The compiler definition remains authoritative. After Homebrew confirms SDCC is installed, the wizard calls:

```text
Wizard.AutoDetectCompiler("sdcc")
```

so the existing `AutoDetectInstallationDir()` updates the live Code::Blocks compiler profile. The wizard does not hard-code `/opt/homebrew` as a project compiler path.

---

## 3. Generated project and simulator-backed Run

The existing SDCC MCS51 build behaviour is retained, including `-mmcs51`, the selected memory model and memory-size linker options. Fresh projects successfully produce the expected `.ihx` and `.hex` output together with the usual SDCC auxiliary files.

The generated `.ihx` was independently loaded and executed using Homebrew's `s51`. Interactive simulator commands were tested, including starting and stopping simulated MCS51 execution.

Run behaviour is therefore configured as:

```text
Build
    -> SDCC produces the MCS51 image

Run / Build and Run
    -> s51 receives the generated .ihx image
    -> interactive MCS51 simulator session
```

To support this cleanly, Code::Blocks' existing host-application semantics were generalised so an explicitly configured host application can also take precedence for a native target. The Scripted Wizard bindings were extended to expose the existing `SetRunHostApplicationInTerminal()` target property.

For the macOS SDCC/Intel HEX path the generated target uses the existing project model to configure `s51` as host application, pass the generated image as its argument, and request terminal-capable execution. The target remains an MCS51 image rather than being misrepresented as a native executable or commands-only target.

---

## 4. Program Console and standalone-file regression

Both execution modes now work for the MCS51 simulator:

```text
Program Console
    -> Code::Blocks-owned PTY
    -> persistent interactive simulator pane

Terminal.app
    -> cb_console_runner
    -> interactive simulator in external terminal
```

The Program Console intentionally does not display `Press any key to continue` because the pane persists after process completion. Terminal.app retains the traditional `cb_console_runner` completion behaviour.

Testing this path exposed a separate standalone-file regression. `RunSingleFile()` constructed the Terminal.app command before adding `cb_console_runner`, causing the standalone executable to be substituted directly into `$SCRIPT` while the runner was appended outside the `osascript` payload.

The standalone path was corrected to construct the complete payload first:

```text
optional cb_console_runner + executable + arguments
```

and only then substitute that payload into `$SCRIPT`.

Consequently:

```text
Standalone file + Program Console
    -> direct execution in persistent pane

Standalone file + Terminal.app
    -> cb_console_runner -> executable
    -> traditional completion banner and key wait
```

---

## 5. Files / areas changed

| File / area | v3.7 role |
|---|---|
| `src/plugins/scriptedwizard/resources/mcs51/wizard.script` | Adds the macOS Homebrew/SDCC workflow and configures simulator-backed Run behaviour while retaining the existing MCS51 project logic. |
| `src/plugins/compilergcc/resources/compilers/compiler_sdcc.xml` | Updates SDCC discovery for modern Homebrew/macOS installations. |
| `src/sdk/scripting/bindings/scriptbindings.cpp` | Exposes the existing terminal-host target property to Scripted Wizards. |
| `src/plugins/compilergcc/compilergcc.cpp` | Honours explicit host applications for the MCS51 execution case and restores correct standalone-file `$SCRIPT` payload composition for Terminal.app. |

---

## 6. Milestones

| | Objective | Check | Status |
|---|---|---|---|
| M1 | Modernise the existing MCS51 wizard without replacing its project model. | Existing MCS51/SDCC generation remains intact and macOS selects the supported SDCC path. | **DONE** |
| M2 | Provide reproducible SDCC setup on modern macOS. | Homebrew SDCC can be located/installed and the native compiler profile re-detects the installation. | **DONE** |
| M3 | Validate the complete MCS51 build path. | A fresh project builds and produces working `.ihx`/`.hex` output without manual compiler repair. | **DONE** |
| M4 | Give the cross-compiled project meaningful Run semantics. | Run/Build and Run launch `s51`; the generated image loads and simulated execution can be started/stopped interactively in Program Console and Terminal.app. | **DONE** |
| M5 | Preserve both console workflows. | Program Console remains persistent without a key wait, while standalone-file Terminal.app execution again uses `cb_console_runner` correctly. | **DONE** |

---

## 7. Acceptance

v3.7 is accepted because:

1. New Project -> MCS51 creates a working conventional Code::Blocks MCS51 project on the tested Apple Silicon macOS configuration.
2. The established Homebrew workflow can supply SDCC, while compiler XML and `AutoDetectCompiler("sdcc")` remain authoritative for Code::Blocks compiler configuration.
3. The generated project builds successfully and produces Intel HEX output without manual post-generation repair.
4. Run and Build & Run launch the generated image through `s51`, and actual simulated MCS51 execution was tested interactively.
5. Both Program Console and Terminal.app execution work, and the standalone-file Terminal regression discovered during this work is repaired without reintroducing an unnecessary pause in the persistent Program Console.

---

## 8. What stays rejected

- Removing MCS51 merely because the architecture is old.
- Treating `.ihx` as a native executable or changing the target to commands-only solely to obtain host-application behaviour.
- Hard-coding Homebrew compiler paths into generated projects.
- Bundling SDCC, uCsim, or Homebrew into Code::Blocks.
- Replacing the existing SDCC compiler profile or MCS51 project logic with a new subsystem.
- Launching the simulator as a post-build command; Build must remain distinct from Run.
- Automatically issuing the simulator `run` command instead of preserving the useful interactive prompt.
- Adding `Press any key` to the persistent Program Console.
- Folding ANSI colour rendering into the MCS51 wizard repair.

---

## 9. Known follow-up

The Program Console currently displays some ANSI/SGR terminal colour sequences from uCsim as visible escape text rather than rendering the intended colours. This is a Program Console presentation issue and does not affect compilation, simulator loading, interactive input, or execution.

It is deliberately left outside the v3.7 acceptance boundary.

---

## 10. Done when

**DONE.** A user can select the MCS51 wizard on the tested Apple Silicon macOS configuration, use the Homebrew-provided SDCC toolchain, build the generated project, and invoke Run or Build & Run to interact with the resulting MCS51 program through `s51` in either the integrated Program Console or Terminal.app.

The final implementation remains bounded:

**the wizard manages the user-facing setup; Homebrew supplies SDCC/uCsim; the existing compiler profile remains authoritative; the generated project remains an ordinary Code::Blocks project; and existing host-application and console mechanisms provide simulator-backed execution.**
