# Code::Blocks v4.4 — Arduino wizard, built by arduino-cli

**Status: Planning. The manual pipeline has been exercised. The wizard has not been implemented, and no Code::Blocks source file has been changed.**

The legacy Arduino project wizard was retired in v3.5 and stays retired. This version adds a new wizard only after the manual project below is the one the wizard will generate. The sketch is edited in Code::Blocks. Arduino CLI compiles, links, resolves libraries, and uploads. The existing AVR GCC wizard and the `avr-gcc` compiler profile stay as they are.

**Question this version answers:** what has to change in Code::Blocks when the build is an external project tool, `arduino-cli`, rather than GCC or any other compiler that Code::Blocks drives one source file at a time?

The investigation answer is narrow. Code::Blocks already has a project-level command path, and the Build button already runs it. What it does not have is a way to press Build with no compiler selected, or with a project that contains no file `FileTypeOf()` classifies as source. v4.4 therefore adds a compiler profile whose only jobs are to be found on disk and to recognise GCC diagnostics, and a wizard that stores `arduino-cli` in the existing custom-build command slots. It does not teach `DirectCommands` to compile `.cpp` files with `arduino-cli`.

The earlier note in `plans/thoughts/codeblocks-arduino-architecture-discussion.md` still holds for debugging and for replacing Arduino IDE 2. v4.4 does not take that on. It delivers edit, build, clickable compiler diagnostics, project-local Arduino libraries, Clean, and Upload for one AVR sketch.

Verified on Windows against Arduino CLI 0.31.0 (commit 940c9457, 2023-02-22) at `C:\Users\thoma\.vscode\extensions\vscode-arduino.vscode-arduino-community-0.7.2-win32-x64\assets\platform\win32-x64\arduino-cli\arduino-cli.exe`, and against the installed Code::Blocks binary (build date Oct 23 2026). The proof projects live under `%TEMP%\cb-v44-proof` and a throwaway `--user-data-dir`. The user's roaming Code::Blocks config and the real Arduino15 config were not modified.

---

## 1. What the build pipeline actually requires

A normal Code::Blocks build does not shell out to "the compiler" as a project. `CompilerGCC` asks `DirectCommands` for one command per source file, then a link command. Those commands come from the selected compiler's `CompileObject` / `LinkExe` templates. That is the shape of every listed compiler, including `avr-gcc`, `gcc`, `sdcc`, and `ldc`.

`arduino-cli compile` is a different shape. It takes a sketch directory, a fully-qualified board name, and a library search root. It chooses the core, the compiler inside that core, the include paths, the libraries, and the link line. It will not accept `-c file.cpp -o file.o`. Putting `arduino-cli` in the C-compiler slot of a normal profile, and leaving the GCC command templates in place, would invoke it once per file with flags it does not understand.

The pipeline already contains the other shape. `cbProject::IsMakefileCustom()` makes `CompilerGCC::UseMake()` true. In `bsTargetBuild`, Code::Blocks then runs the string from `GetMakeCommandFor(mcBuild)` instead of `DirectCommands::GetCompileCommands()`. The working directory is `cbProject::GetExecutionDir()`, which for a custom makefile is `GetMakefileExecutionDir()` and defaults to the project base path (`src/sdk/cbproject.cpp`, `GetMakefileExecutionDir` / `GetExecutionDir`). The Qt4 wizard is the existing user of this path.

That path is still gated by a compiler object, and by a source-file check that runs first.

```text
Build
    |
    +-- DoBuild
    |       ProjectHasDebuggeeSources()
    |       FileTypeOf(unit) == ftSource
    |       .ino, .h, .cbp, and .yaml do not count
    |       false -> "Cannot build: the active project has no source files."
    |                no compiler process is started
    |
    +-- PreprocessJob
    |       CompilerValid(target) -> Compiler::IsValid()
    |       looks for <master>/bin/<Program C>, then <master>/<Program C>
    |       invalid -> target is skipped, nothing is built
    |
    +-- valid and IsMakefileCustom()
            |
            +-- mcAskRebuildNeeded, unless that command is empty
            +-- run the stored build command in the project directory
            +-- parse each output line with that compiler's regexes
```

`Compiler::IsValid()` (`src/sdk/compiler.cpp`) does not ask whether the program is a C compiler. It asks whether the file named by Program C exists under the master path. An empty master path returns true, with the comment that the compiler is still initialising. A wizard must not treat that as success.

`FileTypeOf()` (`src/sdk/globals.cpp`) returns `ftSource` for `.c`, `.cpp`, `.cc`, `.cxx`, and the other extensions listed there. `.ino` is not among them. `DebuggeeDescriptionUtil::ProjectHasDebuggeeSources()` (`src/sdk/debuggeedescription.cpp`) requires at least one such file. `CompilerGCC::DoBuild()` (`src/plugins/compilergcc/compilergcc.cpp`) returns before `PreprocessJob` when that check fails. A project whose only unit is the `.ino` never reaches `arduino-cli`. In batch mode that early return also skips `cbEVT_COMPILER_FINISHED`, so the batch window stays open. The correction that does not change core C++ is in section 4: the project carries one `.cpp` that Arduino CLI does not compile.

`Compiler id null` and target type `ttCommandsOnly` do not open the gate. `ttCommandsOnly` suppresses generated flags and the output filename. It does not skip `CompilerValid()`, and it does not by itself run an external build. The proof targets are `ttCommandsOnly` and the custom command still ran, because the makefile-custom flag is what selects the command path.

`AddOutputLine` parses the process output with `compiler->CheckForWarningsAndErrors()`. The regexes belong to the selected compiler, including when the command was a custom build command.

---

## 2. Scope and responsibility boundary

```text
Code::Blocks
    |
    +-- Arduino project wizard          not implemented in this pass
    |       writes sketch.yaml
    |       writes the custom build commands
    |       creates an empty libraries/
    |
    +-- compiler profile id "arduino-cli"
    |       found by the existing compiler_*.xml scan
    |       IsValid() because arduino-cli.exe is on disk
    |       GCC diagnostic regexes, and no compile/link templates
    |
    +-- Build target "Build"
    |       arduino-cli compile --build-path build --libraries libraries
    |
    +-- Build target "Upload"
            arduino-cli upload --input-dir build
```

Arduino CLI owns compilation, linking, library resolution, and upload. Code::Blocks does not add library `.cpp` files to `DirectCommands`, restore GCC `CompileObject` or `LinkExe` templates, select `avr-gcc`, copy library sources into the sketch root, install libraries globally, add a build driver, or edit core to make this path easier.

On macOS, Homebrew can install the `arduino-cli` formula through the existing Homebrew wizard helpers. Code::Blocks does not install Homebrew. On Windows, the wizard calls `AutoDetectCompiler("arduino-cli")` and stops when the tool is absent. This version does not download Arduino CLI on Windows.

The AVR core (`arduino:avr`) is also external. After the CLI is present, the wizard may run `arduino-cli core install arduino:avr`, and only after the user accepts that download. Code::Blocks does not vendor cores, compilers, or libraries. The wizard does not download arbitrary libraries. Library Manager and automatic dependency installation stay out of scope.

The generated project is a sketch directory:

```text
Blink/
    Blink.ino                 same base name as the directory
    Blink.cbp
    sketch.yaml               default_fqbn and, when given, default_port
    extra/
        anchor.cpp            one ftSource file; Arduino CLI does not compile this directory
    libraries/                empty until the user puts libraries here
        Adafruit_BMP280_Library/
            library.properties
            src/  or headers at the library root
            examples/
    build/                    created by arduino-cli, removed by Clean
```

`libraries` is one search root. Each child directory is one ordinary Arduino library. The directory name may differ from the `name` in `library.properties` and from the header the sketch includes. `Adafruit_BMP280_Library` is included as `<Adafruit_BMP280.h>`. Code::Blocks does not copy those sources into the sketch root, generate a build target per library, or compile their `.cpp` files through `DirectCommands`.

---

## 3. The compiler profile

Two new files, loaded by the existing scan of `compiler_*.xml` in `CompilerGCC` and installed by the existing `compilers/*.xml` rule in `src/plugins/compilergcc/resources/Makefile.am`. No `CompilerFactory` registration and no C++ compiler subclass.

```text
src/plugins/compilergcc/resources/compilers/compiler_arduino-cli.xml
src/plugins/compilergcc/resources/compilers/options_arduino-cli.xml
```

Identity:

```text
name   Arduino CLI
id     arduino-cli
```

`MakeValidID` strips hyphens only in the config key (`arduino_cli`). The compiler id stored on the project stays `arduino-cli`.

Programs, on every platform, are the CLI executable rather than a compiler inside a core:

```text
Windows     arduino-cli.exe
elsewhere   arduino-cli
```

C, CPP, and LD all name that executable. `IsValid()` checks C only. CPP and LD are set so the toolchain page does not display a blank GCC. MAKE names the same executable so that a forgotten `$make` is visible as a bad `arduino-cli` invocation rather than a silent call to `make`. The build commands do not use `$make`.

Master-path search, in `compiler_arduino-cli.xml`:

```text
PATH
macOS: /opt/homebrew/bin, then /usr/local/bin
```

`CompilerXML::AddPath()` turns a hit in a directory named `bin` into the parent directory. A hit on `/opt/homebrew/bin/arduino-cli` becomes master path `/opt/homebrew`, and `IsValid()` looks for `/opt/homebrew/bin/arduino-cli`. The Windows install used for the proof is not inside a directory named `bin`. Auto-detect saved the master path as the directory that contains `arduino-cli.exe`, and `IsValid()` succeeded on `<master>/arduino-cli.exe`. There is no fallback master path. A missing CLI stays undetected, and the wizard refuses to create the project.

A throwaway profile whose saved master path was an empty directory, and whose XML had no `PATH` search, did not update the sketch's hex. The batch process exited without a compile. That is the "missing executable prevents the build" check.

`options_arduino-cli.xml` includes `<Common name="re"/>` and nothing else from the GCC option set. The regex that classified the proof diagnostic is `Compiler error (2)` in `src/plugins/compilergcc/resources/compilers/options_common_re.xml`:

```text
([][{}()[:blank:]#%$~[:alnum:]!&_:+/\\\.-]+):([0-9]+):[0-9]+:[[:blank:]](.*)
```

The path class includes colon, backslash, and blank, so a Windows drive letter, backslashes, and spaces can match. The proof line was classified, and double-clicking it in Build messages opened the file at the reported line and column. The profile does not include `<Common name="warnings"/>`, optimisation, architecture, or `<Common name="cmds"/>`.

Opening a project that uses this profile logs `Current compiler 'Arduino CLI' doesn't have correctly defined debugger!` Debug stays out of scope. The message is not a build failure.

Switches that matter:

```text
needDependencies   false
supportsPCH        false
logging            default, which is full
```

Full logging selects `mcBuild` rather than `mcSilentBuild`. The silent command is still set, to the same compile line, so a later logging change does not append `> $(CMD_NULL)` and hide the diagnostics.

During a build, `SwitchCompiler("arduino-cli")` calls `SetupEnvironment()`, which prepends `<master>/bin` to `PATH`. The Windows proof install has no `bin` directory there. The custom command can still be the bare name `arduino-cli` when the executable is already on `PATH`, which it was for the proof.

---

## 4. Commands the wizard writes

`sketch.yaml` is the board and port. On CLI 0.31.0, `default_fqbn` is enough for `arduino-cli compile` and `default_port` is enough for `arduino-cli upload`. Neither command in the project passes `-b` or `-p`. A sketch that has no `sketch.yaml` fails with `Missing FQBN` until `-b` is passed; that is why the file is required, not why the file is insufficient.

```yaml
default_fqbn: arduino:avr:uno
default_port: <port, omitted when the user left it empty>
```

`arduino-cli compile --help` on 0.31.0 describes `--libraries` as "Path to a collection of libraries. Can be used multiple times or entries can be comma separated." `--library` is the single-library form and is not used. The verified Build command is:

```text
arduino-cli compile --build-path build --libraries libraries
```

Checked on this CLI, from the sketch directory:

- `--libraries libraries` resolves the relative path from the process working directory. The custom-build working directory is the project directory, and the Code::Blocks build of the proof sketch found `libraries\StepperDriver` and wrote `build\` beside the `.cbp`.
- The option accepts a directory that contains many libraries. One flag is the whole `libraries` root, not one flag per library.
- Passing the option twice is accepted.
- With an isolated `--config-file` whose user directory held StepperDriver 9.9.9, omitting `--libraries` selected 9.9.9 and passing `--libraries libraries` selected the project copy 1.4.1. The real Arduino config (`directories.user` = `C:\Users\thoma\Documents\Arduino`) was not modified. `arduino-cli lib list` on that config reported no libraries.
- `library.properties` `depends=` is resolved from the same root. Directory names do not have to equal the `name` field.
- A project directory whose path contains a space still works with the relative token, because the space is in the working directory and not in the command token. An absolute path with a space works only when the shell quotes it. Code::Blocks on Windows does not wrap the build command in a shell, so the relative token is the form that stays portable on Windows, macOS, and Unix.
- `$(PROJECT_DIR)` already exists (`MacrosManager`; `GetBasePath()` with a trailing separator, stored with forward slashes). `GetMakeCommandFor()` runs `ReplaceMacros`. An absolute form would be `--libraries $(PROJECT_DIR)libraries`. It was not required. No new substitution and no change to `GetMakeCommandFor()` is proposed.
- Without `--libraries`, a `libraries` directory sitting in the sketch is not discovered on 0.31.0. The flag is required.
- A second compile with `--build-path build` inside the sketch did not copy `build\` into `build\sketch\build`. The active build path is excluded. Other directories that happen to sit in the sketch, including an older build directory with a different name, are copied into `build\sketch\`. Clean removes `build\` so those copies do not accumulate. `libraries\` is user content and is not a build directory.

The Upload command is:

```text
arduino-cli upload --input-dir build
```

`arduino-cli upload --help` on 0.31.0 says the command does not compile the sketch. The help text has `--input-dir` and no `--libraries`. A direct invocation from the sketch directory, with `default_fqbn` and `default_port: COM255` in `sketch.yaml`, ran `avrdude` on `build\StepperSketch.ino.hex`, did not run `avr-g++`, and did not change the hex timestamp. It failed with `ser_open(): can't open device "\\.\COM255"`. No board was flashed. `--libraries` is not added to Upload.

Both targets are `ttCommandsOnly`. Run is not upload. Upload is a second build target, chosen in the target dropdown, and started with Build.

| Target | Member of virtual target All | Build command | Clean |
|---|---|---|---|
| Build | yes | `arduino-cli compile --build-path build --libraries libraries` | remove the `build` directory |
| Upload | no | `arduino-cli upload --input-dir build` | remove the `build` directory |

`includeInTargetAll` is not a usable persistence mechanism. `projectloader.cpp` still reads it for pre-1.5 projects, and the call that would write it is commented out. The wizard calls `DefineVirtualBuildTarget("All", ["Build"])` and does not call `SetIncludeInTargetAll`.

`mcAskRebuildNeeded` has to be empty on the project as well as on the target. `GetMakeCommandFor()` uses the target command only when it is non-empty; otherwise it uses the project command. A new project still has the `CompileTargetBase` default `$make -q -f $makefile $target`. With MAKE set to `arduino-cli.exe`, that default becomes `arduino-cli.exe -q -f Makefile Build`, which the proof build log actually ran before the real compile. An empty target element does not override it. The wizard writes a project-level `MakeCommands` block whose `AskRebuildNeeded` command is empty, and the same empty command on both targets. `bsTargetBuild` then logs that the ask command is empty and runs the build command. `arduino-cli` does its own incremental work inside `build/`.

Clean is not `arduino-cli compile --clean`, because that flag cleans and then compiles. Clean removes only this project's `build` directory. It does not remove `libraries\`.

```text
macOS and other Unix    rm -rf build
Windows                 cmd /c if exist build rmdir /s /q build
```

The Unix build runner wraps the command in the configured shell. The Windows build runner does not, so the Windows clean command starts with `cmd /c`. That command, launched by Code::Blocks `--clean --target=Build`, removed `build\` and left `libraries\StepperDriver\library.properties` at its previous timestamp. `mcDistClean` is the same removal. It does not run `arduino-cli cache clean`.

`mcCompileFile` is set to the same compile line as `mcBuild`. `CompilerGCC::CompileFileDefault()` does not read it. Compile-current-file always calls `DirectCommands`. With no `CompileObject` template, Compile-current-file reports that it has no command. The supported actions are the Build target and the Upload target.

The `.ino`-only gate is closed by one file:

```text
extra/anchor.cpp
```

A syntax error in `extra\bad.cpp` did not fail `arduino-cli compile` on 0.31.0. Arduino CLI compiles sources in the sketch root and under `src/`. It does not compile `extra/`. The file exists so `FileTypeOf()` returns `ftSource` and `DoBuild` reaches the custom command. It is not a generated GCC command, and it is not part of the sketch. Library `.cpp` files under `libraries\` remain ordinary library sources. They may also be added to the project tree later so the user can open them; they are still not compiled by `DirectCommands`.

---

## 5. Wizard

Not implemented. Do not start it until a repeat of the Upload target through Code::Blocks, with the command in section 4 and no marker prefix, is shown not to refresh `compile_commands.json`. The Build, diagnostic, Clean, missing-CLI, and library-compile checks below already passed. One earlier `--build --target=Upload` invocation did refresh the hex and `compile_commands.json`. A later invocation whose command was prefixed with `cmd /c echo` left the hex unchanged, and the marker file contained the echo text rather than a confirmed `avrdude` run. That pair is the remaining pipeline question. It is not a reason to edit `CompilerGCC`.

When the wizard is implemented, it is a Scripted Wizard registered beside AVR:

```text
src/plugins/scriptedwizard/resources/config.script
    RegisterWizard(wizProject, _T("arduino"), _("Arduino project"), _("Embedded Systems"));
```

`src/plugins/scriptedwizard/resources/Makefile.am` gains an `arduino` subdirectory. That subdirectory installs `wizard.script`, `wizard.xrc`, `logo.png`, `wizard.png`, and the sketch template, following `resources/d/Makefile.am` and `resources/avr/Makefile.am`. The images may be copies of the AVR wizard images.

Page order:

1. Introduction. The build tool is Arduino CLI. The default board is an Arduino Uno (`arduino:avr:uno`). Debug and Release are not offered.
2. Project path. The directory name and the `.ino` name are the project name.
3. Toolchain. macOS uses `Wizard.MacBrewFormula("arduino-cli")`, and the existing explicit install when the formula is missing. Windows only detects. Both then call `Wizard.AutoDetectCompiler("arduino-cli")` and stop unless `found=1`, `detected=1`, and the master path is non-empty.
4. AVR core. `arduino-cli core list` is run with `IO.ExecuteAndGetOutput`. When `arduino:avr` is absent, the wizard offers `arduino-cli core update-index` and `arduino-cli core install arduino:avr`. Nothing is downloaded until the user accepts. The page does not advance when the core is still absent.
5. Board and port. FQBN defaults to `arduino:avr:uno` and stays an editable field. The wizard does not embed a board list. Port is an editable field. When `arduino-cli board list` returns a single serial port, that port is the initial text. An empty port is allowed; Upload then fails with the CLI's own missing-port message until `sketch.yaml` has `default_port`.
6. The compiler page is `Wizard.AddCompilerPage(_T("arduino-cli"), _T("arduino-cli"), false, false)`. One profile, and the configuration checkboxes hidden. `GetWantDebug()` and `GetWantRelease()` are then false, so `RunProjectWizard` creates a single `default` target instead of Debug and Release. `Wizard.SetCompilerDefault()` ignores its argument and restores the IDE default, so it is not used to select this profile.

`SetupProject` then:

- renames `default` to `Build`, sets it to `ttCommandsOnly`;
- adds `Upload`, also `ttCommandsOnly`;
- defines the virtual target `All` as `Build` only;
- sets the compiler id `arduino-cli` on the project and on both targets;
- sets `SetMakefileCustom(true)` and the command strings from section 4, including an empty `AskRebuildNeeded` on the project and on both targets;
- writes `sketch.yaml`, the `.ino`, `extra/anchor.cpp`, and an empty `libraries/` directory;
- adds `extra/anchor.cpp` and the `.ino` as project files;
- sets the active build target to `Build`.

The sketch template is the usual empty `setup` / `loop`. `*.ino` is added to the file mask of `src/sdk/resources/lexers/lexer_cpp.xml` so the sketch is highlighted as C/C++. `.ino` is not added to `FileTypeOf()`. Doing that would be a core change, and the anchor file already lets `DoBuild` proceed. The internal compiler is not invited to compile the sketch.

No new Scripted Wizard C++ binding is required. `SetMakefileCustom`, `SetMakeCommandFor`, `SetCompilerID`, `SetTargetType`, `AddBuildTarget`, `RenameBuildTarget`, `DefineVirtualBuildTarget`, `AutoDetectCompiler`, the Homebrew helpers, and `IO.ExecuteAndGetOutput` are already exposed.

---

## 6. Files

| File / area | v4.4 role |
|---|---|
| `src/plugins/compilergcc/resources/compilers/compiler_arduino-cli.xml` | New profile. Searches `PATH` and the Homebrew bin directories. Program C is `arduino-cli`. |
| `src/plugins/compilergcc/resources/compilers/options_arduino-cli.xml` | Program names, dependency/PCH switches, and the existing GCC diagnostic regexes. No compile or link templates. |
| `src/plugins/scriptedwizard/resources/arduino/` | New wizard script, board/port page, images, sketch template, and `extra/anchor.cpp`. |
| `src/plugins/scriptedwizard/resources/config.script` | Registers "Arduino project" under Embedded Systems. |
| `src/plugins/scriptedwizard/resources/Makefile.am` | Adds the `arduino` wizard directory. |
| `src/sdk/resources/lexers/lexer_cpp.xml` | Adds `*.ino` to the C/C++ file mask. |

`CompilerGCC`, `DirectCommands`, `Compiler::IsValid`, `CompileFileDefault`, `FileTypeOf`, and `GetMakeCommandFor` stay unchanged. The compiler XML install rule is already `compilers/*.xml`.

---

## 7. Milestones

The first milestone is the manual proof. It was run before this plan was revised. The wizard is not milestone 1.

| | Objective | Result on this machine |
|---|---|---|
| M1 | Prove the pipeline with a hand-written `.cbp`, the `arduino-cli` profile in a throwaway user-data directory, `sketch.yaml`, and `libraries\`. No wizard. | Build of a sketch that contains one `ftSource` file ran `arduino-cli compile --build-path build --libraries libraries`. The build log named no `gcc`, `g++`, or `avr-gcc` launched by Code::Blocks. A `.ino` syntax error was classified and a double-click opened line 3, column 1. Clean removed `build\` and did not touch the library. A master path with no `arduino-cli.exe` did not update the hex. Upload through the IDE still needs the repeat in section 5. |
| M2 | Add the same profile under `src/plugins/compilergcc/resources/compilers/`. | Not started. The proof profile was only in the throwaway user-data directory. |
| M3 | Register the wizard and create the sketch layout in section 2, including empty `libraries/` and `extra/anchor.cpp`. | Not started. Blocked on the Upload repeat in section 5. |
| M4 | Install or detect the CLI, then the AVR core, before the project exists. | Not started. The proof machine already had CLI 0.31.0 and `arduino:avr` 1.8.6. |
| M5 | Project-local libraries. | CLI compile of StepperDriver and of Adafruit BMP280 from `FillingInterfaceArduino\libraries` passed. See section 8. The IDE click was shown for the `.ino`, not yet for a library `.cpp`. |
| M6 | Upload is a target, and Clean stays limited to `build\`. | Clean through Code::Blocks passed. Direct `arduino-cli upload --input-dir build` passed and did not recompile. The IDE Upload repeat is the open item. |

---

## 8. Acceptance

v4.4 is accepted when all of the following hold.

Build and diagnostics:

1. File -> New -> Project contains Arduino project under Embedded Systems, and the retired wizard is not restored.
2. The wizard's only compiler profile is `arduino-cli`. GCC, `avr-gcc`, Debug, and Release are not selected for the generated project.
3. On macOS, a missing `arduino-cli` formula can be installed through the existing explicit Homebrew flow. Code::Blocks does not install Homebrew.
4. On Windows, a missing `arduino-cli` stops the wizard. This version does not download it.
5. The wizard calls `Wizard.AutoDetectCompiler("arduino-cli")` and creates a project only when detection reports a real master path.
6. A missing `arduino:avr` core is installed only after the user accepts the download.
7. The generated directory matches section 2. The Build command is `arduino-cli compile --build-path build --libraries libraries`. The project-level ask-rebuild command is empty.
8. Build of a fresh Uno sketch finishes with the CLI's size summary. The Code::Blocks build log contains no host `gcc`, `g++`, or `avr-gcc` invocation of the sketch or of a library.
9. A syntax error in the `.ino` is clickable and lands on that source line. This was shown: `InoClick.ino:3:1: error: expected unqualified-id before '}' token`, double-click, caret on line 3 column 1.
10. A profile whose master path does not contain `arduino-cli.exe` does not build the target.
11. The AVR project wizard and the `avr-gcc` profile still build an AVR project the way they do today.

Libraries:

12. The wizard creates an empty `libraries\` directory.
13. A library placed at `libraries\<library-directory>\` is found by Arduino CLI when the Build command passes `--libraries libraries`.
14. StepperDriver 1.4.1, which has no `depends=` entry, compiles from `libraries\StepperDriver`. The proof sketch is `#include <BasicStepperDriver.h>` and `BasicStepperDriver stepper(200, 8, 9); stepper.begin(120, 1); stepper.rotate(360);`, taken from `examples\BasicStepperDriver\BasicStepperDriver.ino`.
15. `Adafruit_BMP280_Library` compiles from the project-local root. Its non-core dependencies, Adafruit Unified Sensor and Adafruit BusIO, resolve from that same root. Wire and SPI come from the AVR core.
16. The successful BMP280 compile does not depend on a global copy. `arduino-cli lib list` reported none, and the CLI's "Used library" lines named `D:\Users\thoma\Documents\git\FillingInterfaceArduino\libraries\Adafruit_BMP280_Library`.
17. A libraries root that contains only `Adafruit_BMP280_Library` fails in the build log with `fatal error: Adafruit_Sensor.h: No such file or directory`, pointing at `Adafruit_BMP280.h`.
18. A library `.cpp` diagnostic is parsed by the same `Compiler error (2)` regex. The line produced from a copy of `BasicStepperDriver.cpp`, not from the user's tree, was `...\libraries\StepperDriver\src\BasicStepperDriver.cpp:22:1: error: expected unqualified-id before 'template'`. The IDE double-click was not repeated on that file. The regex and the click path are the ones that opened the `.ino`.
19. Clean removes `build\` and leaves `libraries\` untouched, including `library.properties` timestamps.
20. Upload does not pass `--libraries`, does not rebuild the sketch, and does not modify `libraries\`.
21. Code::Blocks never invokes GCC directly for the sketch or for library sources. Library objects appear under `build\libraries\<name>\` because Arduino CLI wrote them.

---

## 9. What stays rejected

- Repairing the pre-v3.5 Arduino wizard, or copying its `avr-gcc` recipes, programmer tables, or fuse tables into this wizard.
- Registering `arduino-cli` as a per-file compiler with GCC `CompileObject` / `LinkExe` templates.
- Adding Arduino library `.cpp` files to `DirectCommands`, or compiling them with the listed `avr-gcc` or host GCC profile.
- Copying library sources into the sketch root, or generating a build target per library.
- Installing the proof libraries into the global Arduino user directory in order to make a test pass.
- Treating compiler id `null` or a `ttCommandsOnly` target as sufficient. Build still requires `IsValid()` on a real executable, and `ProjectHasDebuggeeSources()`.
- A new build driver, a new plugin, or a change to `DirectCommands`, `CompileFileDefault`, `FileTypeOf`, or `GetMakeCommandFor`.
- Teaching Compile-current-file to honour `mcCompileFile`.
- Changing the diagnostic parser so a library path matches. The existing regex already matched the Windows lines from this CLI. A future mismatch is another regex in `options_arduino-cli.xml`, not a parser edit made to force this test.
- Automatic Arduino include-path discovery for code completion. Headers under `libraries\` are a build concern for the CLI. Code completion is separate and is not part of v4.4 unless the existing project-file mechanism can show the files with no core change.
- Embedding the Arduino board database, or a board picker larger than one editable FQBN field defaulting to `arduino:avr:uno`.
- Downloading Arduino CLI on Windows, installing Homebrew, vendoring cores, or downloading arbitrary libraries. Library Manager and automatic dependency installation stay out of scope.
- Debug. `arduino-cli debug` and a GDB remote stub are outside this version. Run stays the existing commands-only behaviour and is not the upload action.
- `arduino-cli cache clean`, and any board package other than `arduino:avr`.
- Making `arduino-cli` the IDE's default compiler. `RunProjectWizard` already saves and restores the previous default around project creation.

---

## 10. Risks and maintenance boundary

**The profile can drift into a real compiler.** The maintenance point for "what is Arduino CLI" is `options_arduino-cli.xml` and `compiler_arduino-cli.xml`. Adding GCC command templates there would make Compile-current-file, and any future non-custom build, call `arduino-cli` as though it were `gcc`.

**Detection and validity are different calls.** `AutoDetectInstallationDir()` returning a master path is what the wizard checks. `IsValid()` later checks that the file is still there. An empty master path makes `IsValid()` return true during initialisation; the wizard's `detected=1` requirement is what keeps a missing CLI from creating a project.

**An empty target ask-command is not empty.** It falls through to the project default and runs `arduino-cli.exe -q -f Makefile <target>`. The project-level command has to be cleared as well.

**A `.ino`-only project never starts the compiler.** `ProjectHasDebuggeeSources()` requires `ftSource`. The anchor file in `extra\` is the non-core fix. Deleting that file makes Build return immediately.

**Custom-build output is parsed by the selected profile.** Clickable errors depend on `<Common name="re"/>` and on `arduino-cli` still emitting ordinary GCC lines. The Windows lines from 0.31.0 matched, including an absolute path with backslashes. A library diagnostic that the regex stops matching is recorded with the exact line and is not patched in `compilermessages.cpp` to force the test.

**A project-local library can depend on another library.** BMP280 depends on Adafruit Unified Sensor and Adafruit BusIO. Those have to be present under the same `libraries` root or the build fails with the missing-header diagnostic. The wizard does not install them.

**The checkout directory name can differ from `library.properties`.** `Adafruit_BMP280_Library` has `name=Adafruit BMP280 Library` and is included as `<Adafruit_BMP280.h>`. The CLI resolves that from the collection root. A test that only works when the directory name equals the include name is not sufficient.

**Duplicate local and global libraries make a green build ambiguous.** Precedence was checked with a temporary config file, not by installing into the user's Arduino directory. A later test has to keep that discipline. `arduino-cli lib list` on the real config was empty at the time of the proof.

**Relative paths depend on the working directory.** `--libraries libraries` and `--build-path build` are relative to the project directory. `GetExecutionDir()` for a custom makefile is that directory. A command that does not run there will miss the library root; the first library-error compile in this proof did exactly that until the working directory was the sketch.

**Library diagnostics may contain absolute Windows paths, spaces, or backslashes.** The path class in `Compiler error (2)` includes those characters. The click uses the captured filename and line.

**Some libraries are valid Arduino libraries and still do not build for `arduino:avr:uno`.** A structural library is not a promise that this FQBN can compile it. The failure stays the CLI's diagnostic.

**`libraries\` is user content.** Clean and DistClean remove `build\` only. They must not delete `libraries\`, including when the directory is empty.

**Code completion is not build correctness.** v4.4 does not grow an include-path probe for headers inside `libraries\`.

**`build\` inside the sketch is safe on 0.31.0 only for the directory named by `--build-path`.** A second build directory with another name is copied into the sketch snapshot. The commands and Clean all use the one directory `build`.

**Core install blocks the wizard.** `IO.Execute` is synchronous, as the existing Homebrew install is. The core download can take several minutes. The page tells the user that before starting it.

**Board and port live in `sketch.yaml`, not in the compiler profile.** Editing the FQBN in Compiler settings has no effect, because that profile has no board flag. This was confirmed: the compile command had no `-b`, and the CLI reported platform `arduino:avr` 1.8.6 from `sketch.yaml`.

**Upload without a prior Build fails in the CLI**, because `--input-dir build` is empty. The wizard does not chain compile into the Upload target. The user builds, then uploads.

**Windows does not gain an installer in this version.** A Windows machine without `arduino-cli` on `PATH`, or in a directory the profile searches, cannot finish the wizard.

---

## 11. Done when

A user can create an Arduino project, with Arduino CLI found or installed on macOS and already present on Windows, accept installation of the AVR core when it is missing, and produce a sketch whose Build target compiles with `arduino-cli compile --build-path build --libraries libraries` and whose Upload target uploads with `arduino-cli upload --input-dir build`. Libraries live under `libraries\<directory>\` and are compiled by Arduino CLI. Compiler errors in the `.ino` open at the reported line. Clean removes `build\` and leaves `libraries\` in place. Code::Blocks does not invoke GCC to build the sketch or the libraries.

The division of responsibility:

**the wizard writes the sketch, the empty library root, the anchor file, and the two commands; Arduino CLI compiles, resolves libraries, links, and uploads; the `arduino-cli` compiler profile exists so the current build pipeline will agree to run those commands and will recognise GCC diagnostics; `DirectCommands` is not on this path.**

---

## 12. Verification record

Arduino CLI 0.31.0. `compile --help`: `--libraries` is a collection path, repeatable, or comma-separated. `upload --help`: does not compile; has `--input-dir`; has no `--libraries`.

Commands that passed, with the working directory noted:

```text
cd %TEMP%\cb-v44-proof\StepperSketch
arduino-cli compile --no-color --build-path build --libraries libraries .
arduino-cli compile --no-color --build-path build --libraries libraries --libraries libraries .

cd D:\Users\thoma\Documents\git\FillingInterfaceArduino
arduino-cli compile --no-color --fqbn arduino:avr:uno --build-path %TEMP%\cb-v44-proof\repo-build --libraries libraries %TEMP%\cb-v44-proof\RepoSketch
arduino-cli compile --no-color --fqbn arduino:avr:uno --build-path %TEMP%\cb-v44-proof\bmp-build --libraries libraries %TEMP%\cb-v44-proof\BmpSketch

cd %TEMP%\cb-v44-proof\BmpMissing
arduino-cli compile --no-color --fqbn arduino:avr:uno --build-path %TEMP%\cb-v44-proof\bmp-missing-build --libraries libraries .

cd %TEMP%\cb-v44-proof\StepperSketch
arduino-cli upload --no-color -v --input-dir build -p COM255 .
arduino-cli upload --no-color --input-dir build .
```

The BMP280 "Used library" lines named the FillingInterfaceArduino copies of Adafruit BMP280 Library 2.6.8, Adafruit Unified Sensor 1.1.15, and Adafruit BusIO 1.17.2, plus Wire and SPI from `arduino:avr` 1.8.6. The missing-dependency build named `Adafruit_Sensor.h` and the local `Adafruit_BMP280.h`. StepperDriver has no `depends=` line; its includes are `Arduino.h` and its own headers.

Code::Blocks, throwaway `--user-data-dir %TEMP%\cb-v44-userdata`:

```text
codeblocks.exe --no-dde --no-splash-screen --no-check-associations --user-data-dir %TEMP%\cb-v44-userdata --build --target=Build %TEMP%\cb-v44-proof\StepperSketch\StepperSketch.cbp
codeblocks.exe --no-dde --no-splash-screen --no-check-associations --user-data-dir %TEMP%\cb-v44-userdata --clean --target=Build %TEMP%\cb-v44-proof\StepperSketch\StepperSketch.cbp
```

The IDE build of `InoClick.cbp` (Ctrl+F9, target Build) logged:

```text
Checking if target is up-to-date: arduino-cli.exe -q -f Makefile Build
Running command: arduino-cli compile --build-path build --libraries libraries
C:\Users\thoma\AppData\Local\Temp\cb-v44-proof\InoClick\InoClick.ino:3:1: error: expected unqualified-id before '}' token
```

Build messages showed that error on line 3. Double-click moved the caret from line 1 to line 3, column 1. `InoClick` contains `extra\anchor.cpp` because the `.ino`-only `InoErr` project never started `arduino-cli`.

`BatchJob()` returns 0 after it has invoked the compiler plugin (`src/src/app.cpp`). That process exit code is not the compiler status. The compiler status is the "Process terminated with status" line in the build log.
