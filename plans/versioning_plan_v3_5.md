# Code::Blocks v3.5 — modernise the D application wizard and restore a working LDC toolchain path on macOS

**Status: DONE.** The Scripted Wizard D application entry now creates a working D project on the tested Apple Silicon macOS configuration. LDC is the preferred compiler, Homebrew can install it when missing, Code::Blocks re-runs its native compiler auto-detection after the Homebrew check/install, and the generated project has been built and run successfully.

**Question this version answers:** what is the smallest reusable change that turns an old compiler-dependent wizard from “template generation with manual toolchain setup” into a beginner-usable workflow that can detect/install the toolchain, hand control back to Code::Blocks’ existing compiler definitions, and leave the selected compiler in a usable global state?

The answer implemented in v3.5 is deliberately narrow: modernise the existing D application wizard around the existing `ldc`, `dmd`, and `gdc` compiler profiles; prefer LDC; add macOS Homebrew discovery/install; correct the stale LDC executable/path definitions; and expose one generic Scripted Wizard bridge that re-runs a compiler profile’s native `AutoDetectInstallationDir()`.

This is not a new D build system and does not bundle a D compiler into Code::Blocks.

---

## 1. Scope and responsibility boundary

A generated D application remains an ordinary Code::Blocks compiler project:

```text
Code::Blocks
    |
    +-- ordinary D project
    +-- selected Code::Blocks D compiler profile
    +-- normal Debug / Release targets
    +-- external D compiler toolchain
    |
    +-- executable application
```

On macOS, Homebrew is used only as an external toolchain provider. Code::Blocks does not install Homebrew itself.

The key boundary established by v3.5 is:

```text
Wizard
    |
    +-- choose compiler profile
    +-- verify/install external compiler if appropriate
    +-- ask Code::Blocks to auto-detect that compiler again
          |
          +-- compiler XML remains authoritative
          +-- native Compiler::AutoDetectInstallationDir() sets the live master path
```

The wizard does not hard-code `/opt/homebrew` into a project and does not invent a second compiler-configuration system.

---

## 2. D compiler selection

The legacy D wizard already allowed the three existing Code::Blocks D compiler IDs:

```text
dmd
gdc
ldc
```

The old compiler page used:

```cpp
Wizard.AddCompilerPage(_T(""), _T("dmd;gdc;ldc"), true, true);
```

The empty first argument means that the normal/default compiler is preselected. Merely reordering the list of allowed compiler IDs does not select the first item.

v3.5 therefore explicitly preselects LDC:

```cpp
DCompilerIDs <- _T("ldc;dmd;gdc");
Wizard.AddCompilerPage(_T("ldc"), DCompilerIDs, true, true);
```

LDC is the preferred starting point, while DMD and GDC remain selectable alternatives.

---

## 3. macOS Homebrew workflow

### 3.1 Formula mapping

The D wizard maps the selected Code::Blocks compiler ID to the corresponding Homebrew formula:

```text
ldc -> ldc
dmd -> dmd
gdc -> gcc
```

The existing macOS Homebrew Scripted Wizard helpers are reused rather than adding D-specific package-management code.

The workflow uses the existing calls:

```text
Wizard.MacBrewFormula(...)
Wizard.MacBrewTry(...)
Wizard.MacBrewChoice(...)
Wizard.MacBrewInstall(...)
```

### 3.2 Missing compiler

When the selected compiler formula is missing, the wizard offers the established Homebrew installation flow. Installation remains explicit: no Homebrew command is launched until the user accepts the installation action.

After a successful installation, the formula is queried again before project generation is allowed to continue.

### 3.3 Homebrew itself is not installed by Code::Blocks

If Homebrew cannot be used, the wizard retains the existing helper behaviour: the user can identify an existing `brew` executable or open the Homebrew installation site. Code::Blocks does not install Homebrew itself.

---

## 4. LDC compiler-definition corrections

Testing exposed two independent stale assumptions in the existing LDC compiler resources.

### 4.1 Correct non-Windows executable name

The former non-Windows LDC definition in:

```text
src/plugins/compilergcc/resources/compilers/options_ldc.xml
```

used:

```xml
<Program name="C"   value="ldc"/>
<Program name="CPP" value="ldc"/>
<Program name="LD"  value="ldc"/>
```

The tested current LDC installation provides `ldc2`. v3.5 therefore changes the non-Windows compiler/linker programs to:

```xml
<Program name="C"   value="ldc2"/>
<Program name="CPP" value="ldc2"/>
<Program name="LD"  value="ldc2"/>
```

The rest of the existing LDC compiler options and commands remain outside this change.

### 4.2 Apple Silicon Homebrew search path

The LDC detector in:

```text
src/plugins/compilergcc/resources/compilers/compiler_ldc.xml
```

already searched the inherited `PATH` and `/usr/local/bin`, then fell back to `/usr` on non-Windows systems. That does not explicitly cover the standard Apple Silicon Homebrew executable directory.

v3.5 adds:

```xml
<Search path="/opt/homebrew/bin"
        for="C"/>
```

before the existing `/usr/local/bin` search.

`CompilerXML::AddPath()` deliberately converts a detected master search path ending in `bin` to its parent. Therefore successful detection of:

```text
/opt/homebrew/bin/ldc2
```

produces the Code::Blocks compiler master path:

```text
/opt/homebrew
```

The normal Code::Blocks validity/build logic can then resolve:

```text
/opt/homebrew/bin/ldc2
```

without project-specific path manipulation.

---

## 5. Generic compiler auto-detection bridge

### 5.1 Problem discovered

Homebrew discovery/install and Code::Blocks compiler discovery are separate operations.

The D wizard could correctly establish that Homebrew had installed LDC, while the already-loaded Code::Blocks LLVM D Compiler profile still retained an invalid master path such as:

```text
/usr
```

That caused the project to try:

```text
/usr/bin/ldc2
```

and fail even though LDC was present under Homebrew.

Manually pressing **Auto-detect** in the global LLVM D Compiler settings correctly changed the master path to:

```text
/opt/homebrew
```

and the same generated D project then built and ran.

### 5.2 Reusable native bridge

v3.5 therefore adds a generic Scripted Wizard binding in:

```text
src/plugins/scriptedwizard/wiz.cpp
```

conceptually exposed to scripts as:

```text
Wizard.AutoDetectCompiler(compilerID)
```

The native implementation retrieves the existing compiler object using:

```cpp
Compiler* compiler = CompilerFactory::GetCompiler(compilerId);
```

and executes the existing native detector:

```cpp
compiler->AutoDetectInstallationDir();
```

The binding returns a small report containing the important result fields:

```text
found=1|0
detected=1|0
master=<detected master path>
```

For the tested LDC installation the successful report was:

```text
found=1
detected=1
master=/opt/homebrew
```

### 5.3 Why the bridge is generic

The bridge is intentionally not called `AutoDetectLDC` and is not part of the Homebrew helper itself.

It exposes an existing Code::Blocks compiler operation to Scripted Wizards. This keeps future wizard modernisation reusable:

```text
external dependency/toolchain handling
        +
selected Code::Blocks compiler ID
        |
        v
Wizard.AutoDetectCompiler(id)
        |
        v
existing compiler-specific AutoDetectInstallationDir()
```

A future AVR, Fortran, embedded, or other compiler-dependent wizard can reuse the same operation if its compiler definition already knows how to locate an installed toolchain.

### 5.4 Global compiler state

`AutoDetectInstallationDir()` mutates the live Code::Blocks compiler object. In the tested case this changes the LLVM D Compiler master path from `/usr` to `/opt/homebrew`.

This behaviour is intentional in v3.5. The wizard has selected a compiler, verified that the toolchain is installed, and asks Code::Blocks’ own detector to repair the compiler profile to the location its definition considers valid.

The wizard does not hard-code or directly write the master path and does not add an explicit separate persistence mechanism. It uses the same native detection operation as the compiler settings **Auto-detect** action.

---

## 6. D wizard project behaviour retained

The existing D project structure remains deliberately conventional.

`GetFilesDir()` continues to use:

```text
d/console
```

The generated project keeps ordinary Debug and Release targets.

The original compiler-specific warning behaviour remains:

```cpp
if (Wizard.GetCompilerID().Matches(_T("gdc")))
    WarningsOn(project, Wizard.GetCompilerID());
```

Debug targets continue to receive debugging symbols through the existing common wizard helper, and Release targets continue to receive the existing optimisation configuration.

No D-specific build abstraction was added.

---

## 7. Files changed

| File / area | v3.5 role |
|---|---|
| `src/plugins/scriptedwizard/resources/d/wizard.script` | Modernises the existing D application wizard: explicitly prefers LDC, retains LDC/DMD/GDC selection, adds macOS Homebrew detection/install, and invokes native compiler re-detection before project creation. |
| `src/plugins/scriptedwizard/wiz.cpp` | Adds the reusable `Wizard.AutoDetectCompiler(compilerID)` Scripted Wizard bridge and binds it to the wizard scripting object. |
| `src/plugins/compilergcc/resources/compilers/options_ldc.xml` | Corrects the non-Windows LDC executable/linker name from `ldc` to `ldc2`. |
| `src/plugins/compilergcc/resources/compilers/compiler_ldc.xml` | Adds `/opt/homebrew/bin` to non-Windows LDC discovery so Apple Silicon Homebrew installations can be found by the existing XML compiler detector. |

No D-specific changes were required in `CompilerFactory`, `CompilerXML`, the generic build-command generator, or the project build model.

---

## 8. Diagnostic path used during implementation

The failure was deliberately reduced to separate stages rather than hidden with project-local paths.

### Stage A — wrong default compiler

Putting `ldc` first in the allowed compiler list did not preselect it. The existing Fortran wizard documentation established that the first argument to `Wizard.AddCompilerPage()` is the explicit preselected compiler ID.

Fix:

```cpp
Wizard.AddCompilerPage(_T("ldc"), DCompilerIDs, true, true);
```

### Stage B — Homebrew installation succeeded but build still failed

The Homebrew flow successfully installed LDC, but Code::Blocks reported an invalid LLVM D Compiler setup and tried the stale compiler path.

### Stage C — executable-name mismatch

Inspection of `options_ldc.xml` showed that non-Windows Code::Blocks expected `ldc`, while the tested LDC installation exposed `ldc2`.

Fix: use `ldc2` for C/CPP/LD in the LDC profile.

### Stage D — Apple Silicon Homebrew path missing

Inspection of `compiler_ldc.xml` showed searches for `PATH` and `/usr/local/bin`, but no explicit `/opt/homebrew/bin` search.

Fix: add `/opt/homebrew/bin` to the existing XML detector.

### Stage E — installed compiler still not reflected in the live compiler object

Manual **Auto-detect** proved that the corrected XML definitions could locate LDC and change the master path to `/opt/homebrew`. Returning the master path to `/usr` reproduced the build failure.

Fix: expose and call the existing native `AutoDetectInstallationDir()` from the wizard.

This diagnostic separation is important for future wizard work: package installation, compiler definitions, and the live compiler object are three distinct layers.

---

## 9. Milestones

| | Objective | Check | Status |
|---|---|---|---|
| M1 | Preserve the existing D wizard and compiler choices while defining LDC as the preferred compiler. | Compiler page explicitly preselects compiler ID `ldc`; DMD and GDC remain selectable. | **DONE** |
| M2 | Add the established macOS Homebrew dependency workflow to the D wizard. | Wizard can query/install the selected D compiler formula and refuses to continue when dependency handling fails. | **DONE** |
| M3 | Correct the stale non-Windows LDC executable definition. | LDC profile uses `ldc2` for C, CPP, and LD. | **DONE** |
| M4 | Add Apple Silicon Homebrew LDC discovery. | Native LDC auto-detection finds `/opt/homebrew/bin/ldc2` and resolves the master path to `/opt/homebrew`. | **DONE** |
| M5 | Bridge compiler auto-detection into Scripted Wizards. | `Wizard.AutoDetectCompiler("ldc")` reports `found=1`, `detected=1`, and `master=/opt/homebrew` in the tested configuration. | **DONE** |
| M6 | Generate, build, and run a fresh D project without manual compiler repair. | Generated D application builds and runs successfully after the wizard completes. | **DONE** |
| M7 | Preserve reusable architecture. | Wizard does not set `/opt/homebrew` directly; existing compiler XML and `AutoDetectInstallationDir()` remain authoritative. | **DONE** |

---

## 10. Acceptance

v3.5 is accepted when all of the following hold:

1. New Project → D application presents the existing D compiler choice with LDC preselected.
2. DMD and GDC remain available as alternative compiler profiles.
3. On macOS, the wizard can query the Homebrew formula corresponding to the selected D compiler.
4. If the selected compiler formula is missing, the wizard can invoke the established explicit Homebrew installation flow.
5. The non-Windows LDC profile uses `ldc2`, not the obsolete `ldc` executable name.
6. The LDC XML detector can discover an Apple Silicon Homebrew installation through `/opt/homebrew/bin`.
7. After Homebrew reports LDC installed, the wizard invokes native Code::Blocks compiler auto-detection rather than hard-coding a compiler master path.
8. In the tested Apple Silicon configuration, auto-detection reports master path `/opt/homebrew`.
9. The generated project builds through `/opt/homebrew/bin/ldc2` without post-generation manual repair.
10. The generated D application runs successfully.
11. The change does not introduce a new build system, package manager, or D-specific compiler configuration subsystem.

---

## 11. Opportunistic wizard retirement completed during the v3.5 cleanup

While reviewing the old Scripted Wizard collection, two obsolete/beginner-hostile new-project entries were retired:

```text
MATLAB CSF wizard
Arduino wizard
```

This retirement is deliberately scoped to the **project wizards** and their registration/build-resource references.

It does **not** mean that MATLAB or Arduino-related editing/support material is globally removed from Code::Blocks.

In particular:

- MATLAB lexer/syntax-highlighting support is retained.
- Arduino-related lexers or other unrelated associated Code::Blocks functionality are retained where independent of the retired wizard.
- Historical ChangeLog entries are retained.
- Repository occurrences are reviewed by context rather than mechanically deleted because they contain the words `matlab` or `arduino`.

This follows the same compatibility principle used elsewhere in the fork: remove obsolete or misleading **new-project starting points** without gratuitously destroying independent editing capabilities or historical compatibility.

---

## 12. Reusable pattern for future compiler-dependent wizards

v3.5 establishes a useful pattern for later work, particularly the AVR and Fortran/embedded wizard backlog.

### Layer 1 — external toolchain availability

Use the platform-appropriate dependency mechanism to establish whether the requested toolchain exists. On macOS, where appropriate, this can use the existing Homebrew helper functions.

### Layer 2 — compiler definition correctness

The Code::Blocks compiler profile itself must know:

- the correct executable names;
- the locations worth searching;
- how a discovered executable path maps back to its master path.

Do not compensate for a stale compiler definition by adding project-specific absolute compiler paths in a wizard.

### Layer 3 — refresh the live compiler profile

After installing a previously missing compiler, call:

```text
Wizard.AutoDetectCompiler(compilerID)
```

and require a successful result before creating the project.

### Layer 4 — ordinary project generation

Once the selected compiler profile is valid, let the existing Code::Blocks project/wizard helpers configure Debug/Release targets. Do not duplicate compiler semantics in the wizard.

The resulting general workflow is:

```text
select compiler
      |
      v
check/install external toolchain
      |
      v
Code::Blocks compiler definition locates toolchain
      |
      v
AutoDetectCompiler refreshes live compiler object
      |
      v
generate ordinary project
      |
      v
build / run / debug normally
```

This pattern is a stronger basis for future AVR work than teaching each wizard to manipulate compiler paths independently.

---

## 13. What stays rejected

- Bundling LDC, DMD, GDC, LLVM, or Homebrew into Code::Blocks.
- Replacing the existing Code::Blocks D compiler profiles with wizard-local compiler commands.
- Hard-coding `/opt/homebrew` directly into generated D projects.
- Adding an LDC-specific `CompilerFactory` API when the existing compiler-ID lookup already works.
- Adding an `AutoDetectLDC` special case instead of the generic `Wizard.AutoDetectCompiler()` bridge.
- Expanding `CompilerXML` merely to repair an LDC resource-definition problem.
- Silently switching to a different D compiler from the one selected by the user.
- Removing DMD or GDC merely because LDC is now preferred.
- Treating the D project template directory `d/console` as a compiler configuration mechanism.
- Removing MATLAB or Arduino lexers merely because their project wizards were retired.
- Mechanically deleting historical ChangeLog entries or unrelated repository references.

---

## 14. Risks and maintenance boundary

**Homebrew remains external.** The wizard can use an existing Homebrew installation and can invoke an explicit formula install, but Code::Blocks does not take ownership of Homebrew itself.

**Compiler auto-detection mutates the live global compiler object.** This is intentional: a selected compiler profile that still points at `/usr` while the working compiler is under `/opt/homebrew` is not useful to the generated project. The wizard delegates the correction to Code::Blocks’ existing native detector instead of assigning the path itself.

**Compiler definitions remain the source of truth.** If LDC packaging changes again, the appropriate maintenance point is primarily `options_ldc.xml` / `compiler_ldc.xml`, not every wizard that may use LDC.

**Package installation does not imply compiler configuration.** Future wizard work should remember the central lesson from v3.5: successfully installing a compiler and successfully configuring the Code::Blocks compiler profile are separate steps.

**Platform-specific dependency acquisition should remain bounded.** The generic `AutoDetectCompiler()` bridge is reusable, but Homebrew-specific installation logic should not be generalised to platforms where it does not apply.

---

## 15. Done when

**DONE.** A user can choose the D application wizard, receive LDC as the preferred D compiler, allow Code::Blocks to locate or install the macOS Homebrew LDC toolchain when required, have the selected LLVM D Compiler profile automatically re-detected, and create a D project that builds and runs without manually repairing the global compiler path on the tested Apple Silicon macOS configuration.

The final implementation stays small in architectural terms:

**the wizard handles the user journey; Homebrew supplies the external compiler; the LDC XML resources describe the compiler correctly; `AutoDetectInstallationDir()` performs the authoritative Code::Blocks toolchain detection; and the generated D application remains an ordinary Code::Blocks project.**

The same division of responsibility is intentionally reusable for later compiler-dependent wizard modernisation.
