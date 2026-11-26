# Code::Blocks v4.4 — Arduino wizard, built by arduino-cli

**Status: Planning. The manual pipeline has been exercised. The wizard has not been implemented, and no Code::Blocks source file has been changed.**

The legacy Arduino project wizard was retired in v3.5 and stays retired. This version adds a new wizard only after the manual project below is the one the wizard will generate. The sketch is edited in Code::Blocks. Arduino CLI compiles, resolves libraries, detects a connected board, uploads, and provides the serial session. The existing AVR GCC wizard and the `avr-gcc` compiler profile stay as they are.

**Question this version answers:** what has to change in Code::Blocks when the build is an external project tool, `arduino-cli`, rather than GCC or any other compiler that Code::Blocks drives one source file at a time?

The investigation answer is narrow. Code::Blocks already has a project-level custom-build path, but that path is still gated by both a valid compiler profile and the presence of a project unit classified as source. v4.4 handles those constraints separately. A minimal `arduino-cli` compiler profile satisfies compiler validity and provides diagnostic parsing. A new generic `AllowBuildWithoutSources` project property allows an explicitly external-build project to reach its custom command without inventing a dummy source file. v4.4 does not support a project with no compiler profile. It does not teach `DirectCommands` to compile `.cpp` files with `arduino-cli`.

The split is:

```text
compiler profile
    executable detection and validity
    GCC diagnostic parsing

AllowBuildWithoutSources
    bypasses only the build-path source-presence gate
```

The earlier note in `plans/thoughts/codeblocks-arduino-architecture-discussion.md` still holds for debugging and for replacing Arduino IDE 2. v4.4 does not take that on. It delivers edit, build, clickable compiler diagnostics, project-local Arduino libraries, detection of a physically connected board, upload, serial monitoring, and Clean for one AVR sketch. Simulation is v4.5.

## Investigation results

Verified on Windows against Arduino CLI 0.31.0 (commit 940c9457, 2023-02-22) at `C:\Users\thoma\.vscode\extensions\vscode-arduino.vscode-arduino-community-0.7.2-win32-x64\assets\platform\win32-x64\arduino-cli\arduino-cli.exe`, and against the installed Code::Blocks binary (build date Oct 23 2026). Proof projects live under `%TEMP%\cb-v44-proof` and a throwaway `--user-data-dir`. The user's roaming Code::Blocks config and the real Arduino15 config were not modified.

- **Compile.** `arduino-cli compile --build-path build --libraries libraries` is the verified Build command. `--libraries` is a collection root, repeatable or comma-separated. A relative `libraries` path resolves from the working directory. One root holds many library directories. A project path containing a space still works when the space is only in the working directory. `sketch.yaml` `default_fqbn` supplies the board, so the command does not pass `-b`.
- **StepperDriver.** Version 1.4.1, no `depends=`. The sketch `#include <BasicStepperDriver.h>` / `BasicStepperDriver stepper(200, 8, 9); stepper.begin(120, 1); stepper.rotate(360);` compiled from `D:\Users\thoma\Documents\git\FillingInterfaceArduino\libraries\StepperDriver`. An isolated config showed the project copy (1.4.1) winning over a user-directory copy (9.9.9). A syntax error in a copy of `BasicStepperDriver.cpp` produced `...\BasicStepperDriver.cpp:22:1: error: expected unqualified-id before 'template'`, which matches the existing `Compiler error (2)` regex. The IDE double-click was shown for the `.ino`, not for that library file.
- **BMP280, later.** With the same `--libraries libraries` root, Adafruit BMP280 Library 2.6.8 pulled Adafruit Unified Sensor and Adafruit BusIO from sibling directories. Removing those siblings failed with `fatal error: Adafruit_Sensor.h`. This does not block the StepperDriver proof.
- **Board list.** With a board connected, `arduino-cli board list` printed one row: port `COM3`, protocol `serial`, type `Serial Port (USB)`, name `Arduino Mega or Mega 2560`, FQBN `arduino:avr:mega`, core `arduino:avr`. `board list --format json` and the same command with `--discovery-timeout 5s` returned the same one-element array. There is still no `board list --json` flag. An earlier run with nothing attached printed `No boards found.` and `[]`. The saved document is `%TEMP%\cb-v44-proof\board-list.json`.
- **Upload.** On that Mega, `arduino-cli compile --build-path build --libraries libraries` from `HardwareBlink` (FQBN and port only in `sketch.yaml`, no `-b`) exited 0: 1544 bytes, platform `arduino:avr` 1.8.6. `arduino-cli upload --input-dir build` then exited 0 and did not change the hex, elf, or `compile_commands.json` hashes. A following `upload -v` ran only `avrdude` (`-patmega2560 -cwiring -PCOM3`), wrote those same 1544 bytes, reported signature `0x1e9801`, and did not invoke `avr-g++`, `gcc`, or `g++`. A second upload of `HardwareHello` (1992 bytes) likewise left the hex hash unchanged. The LED timing itself was not visible from this session, so that part of acceptance is still open. The earlier COM255 failure remains the missing-port error, not a hardware flash.
- **Monitor.** `arduino-cli monitor -p COM3 -c baudrate=9600` in a real console printed `baudrate=9600`, `Connected to COM3! Press CTRL-C to exit.`, and then `Hello from Code::Blocks` repeatedly. The same command with stdout redirected printed `not running in a terminal` and exited at once, so it never held the port. One Ctrl+C ended a live session with status `-1073741510` in about 30 ms, and the port could be opened again immediately. A later Ctrl+C call did not end that process; it was then force-stopped. macOS was not exercised.
- **Interactive serial.** The same live console is bidirectional. A round-trip sketch printed `READY`, and typing `TEST123` followed by Enter produced `RX: TEST123` at both 9600 and 115200. Enter sent `0x0D` then `0x0A` (CRLF). `monitor --help` has no line-ending flag, and `monitor -p COM3 --describe` has no line-ending setting. The advertised `baudrate` values on this port are `300, 600, 750, 1200, 2400, 4800, 9600, 19200, 31250, 38400, 57600, 74880, 115200, 230400, 250000, 460800, 500000, 921600, 1000000, 2000000`, default `9600`. This is not source-level debugging.
- **Code::Blocks monitor path.** A build target is the wrong host: the build runner captures the process and holds the build until it exits. `IO::Execute` (`src/sdk/scripting/bindings/sc_io.cpp`) is synchronous and collects output. `ProgramConsole` is the Run/Debug console, and a commands-only target has no host application. The existing interactive launcher is Tools+, `cbTool::LAUNCH_NEW_CONSOLE_WINDOW` (`src/include/cbtool.h`), executed by `ToolsManager::Execute` (`src/sdk/toolsmanager.cpp`) through `cb_console_runner`. It allows one tool process, and nothing in the script bindings reads `sketch.yaml` into that tool. That runner cannot separate bytes read from the board from bytes the user typed, and it cannot be told to record after launch. The selected host, not implemented in this pass, is a Code::Blocks monitor pane. It opens the `port.address` from `arduino-cli board list --format json` and does not read `default_port` as the only address.
- **Source-presence gate.** `ProjectHasDebuggeeSources()` is a debuggee-identity check: an empty project must not Debug or Run a leftover binary. `CompilerGCC::DoBuild()` also uses it, so Build, Rebuild, and Clean never reach the custom command when the project has no `ftSource` unit. Batch mode with no editor fails one step earlier, in `PrepareBuildOrRunRouting`, and does not emit `cbEVT_COMPILER_FINISHED`. No existing project or target flag skips this. The general fix is an opt-in project property, `AllowBuildWithoutSources`, described in section 1. It is not implemented yet. `.ino` is not added to `FileTypeOf()`.
- **Ask-rebuild and Upload through Code::Blocks.** An empty target-level ask-rebuild command falls through to `$make -q` and was observed as `arduino-cli.exe -q -f Makefile Build`. One Code::Blocks `--target=Upload` run refreshed `compile_commands.json`. Direct CLI upload of the Mega sketches did not. That Code::Blocks repeat is still pending: it needs `AllowBuildWithoutSources`, and a dummy `.cpp` is not added to force it. The wizard is not started until the plain Upload target is shown not to compile.

---

## 1. What the build pipeline actually requires

A normal Code::Blocks build does not shell out to "the compiler" as a project. `CompilerGCC` asks `DirectCommands` for one command per source file, then a link command. Those commands come from the selected compiler's `CompileObject` / `LinkExe` templates. That is the shape of every listed compiler, including `avr-gcc`, `gcc`, `sdcc`, and `ldc`.

`arduino-cli compile` is a different shape. It takes a sketch directory, a fully-qualified board name, and a library search root. It chooses the core, the compiler inside that core, the include paths, the libraries, and the link line. It will not accept `-c file.cpp -o file.o`. Putting `arduino-cli` in the C-compiler slot of a normal profile, and leaving the GCC command templates in place, would invoke it once per file with flags it does not understand.

The pipeline already contains the other shape. `cbProject::IsMakefileCustom()` makes `CompilerGCC::UseMake()` true. In `bsTargetBuild`, Code::Blocks then runs the string from `GetMakeCommandFor(mcBuild)` instead of `DirectCommands::GetCompileCommands()`. The working directory is `cbProject::GetExecutionDir()`, which for a custom makefile is `GetMakefileExecutionDir()` and defaults to the project base path (`src/sdk/cbproject.cpp`, `GetMakefileExecutionDir` / `GetExecutionDir`). The Qt4 wizard is the existing user of this path.

That path is still gated by a compiler object, and by a source-file check that runs first.

```text
Build, Rebuild, or Clean
    |
    +-- PrepareBuildOrRunRouting
    |       no editor and no ftSource -> return false
    |       (batch window then stays open: no compiler-finished event)
    |
    +-- DoBuild
    |       ProjectHasDebuggeeSources()
    |       false and AllowBuildWithoutSources is off
    |           -> "Cannot build: the active project has no source files."
    |       false and AllowBuildWithoutSources is on
    |           -> this gate only is skipped
    |
    +-- PreprocessJob
    |       CompilerValid(target) -> Compiler::IsValid()
    |       invalid -> target is skipped, nothing is built
    |
    +-- valid and IsMakefileCustom()
            |
            +-- mcAskRebuildNeeded, unless that command is empty
            +-- run the stored build command in the project directory
            +-- parse each output line with that compiler's regexes
```

`Compiler::IsValid()` (`src/sdk/compiler.cpp`) does not ask whether the program is a C compiler. It asks whether the file named by Program C exists under the master path. An empty master path returns true, with the comment that the compiler is still initialising. A wizard must not treat that as success.

`FileTypeOf()` (`src/sdk/globals.cpp`) returns `ftSource` for `.c`, `.cpp`, `.cc`, `.cxx`, and the other extensions listed there. `.ino` is not among them, and v4.4 does not add it. Doing so would make the sketch look like a file `DirectCommands` may compile. The project instead sets `AllowBuildWithoutSources`. That property is general. It is not an Arduino flag. Run and Debug keep calling `ProjectHasDebuggeeSources()` unchanged, so an external-build project still cannot launch or debug a leftover binary.

`Compiler id null` and target type `ttCommandsOnly` do not open the gate. `ttCommandsOnly` suppresses generated flags and the output filename. It does not skip `CompilerValid()`, and it does not by itself run an external build. The proof targets are `ttCommandsOnly` and the custom command still ran, because the makefile-custom flag is what selects the command path.

`AddOutputLine` parses the process output with `compiler->CheckForWarningsAndErrors()`. The regexes belong to the selected compiler, including when the command was a custom build command.

### AllowBuildWithoutSources

Change 1 is a general project property. Change 2, the Arduino wizard, only calls it. They are separate reviews. The property name does not mention Arduino.

`ProjectHasDebuggeeSources()` (`src/sdk/debuggeedescription.cpp`, line 128) returns false when the project has no files, or when neither the active target's file list nor the project file list contains an `ftSource` unit. Its header comment says an empty project must not Debug a leftover binary. Callers that keep that meaning, and do not read the new flag:

- `CompilerGCC::RunActiveProjectAfterPrepare` (`compilergcc.cpp`, about line 2249), "Cannot run"
- `CompilerGCC::ExecutableExists` (about line 2190)
- `CompilerGCC::OnCompileAndRun` (about line 4483) and the Build-and-Run menu item (about line 4845)
- `DebuggerGDB` (about line 900) and `DebuggerMenuHandler` (`debuggermenu.cpp`, about line 483)

Build, Rebuild, and Clean all enter `CompilerGCC::DoBuild(const wxString&, bool clean, bool build)`. The source check there (about line 3238) runs before `PreprocessJob`, for a clean as well as for a build. With no editor open, `PrepareBuildOrRunRouting` (about line 1166) returns false first and `DoBuild` never logs the message. That is the batch-window hang. Workspace builds do not use this `DoBuild` overload, so they never had the check.

Nothing already expresses the intent. `IsMakefileCustom()`, `ttCommandsOnly`, compiler id `null`, and a virtual target do not skip it. A commands-only target is not even known yet at the project-level check, and virtual target `All` is not a `ProjectBuildTarget`. Inferring the bypass from either of those would change every existing custom-makefile project, including an empty one that would then run its command. That is why the flag is opt-in and project-level: Build and Upload belong to one external-build project, and ordinary projects stay as they are.

`AllowBuildWithoutSources` does not mean that the project must contain some other non-`ftSource` project unit. With the property set, Build, Rebuild, Clean, DistClean, a named build target, and a virtual build target are permitted even when the project has zero units.

The property bypasses only the source-presence checks in `PrepareBuildOrRunRouting` and `CompilerGCC::DoBuild`. The first of those lets a batch build with no editor take the project route. It does not bypass `CompilerValid()`, compiler detection, target selection, command selection, diagnostic parsing, Run checks, Build-and-Run checks, Debug checks, or process-result handling. `ProjectHasDebuggeeSources()` remains unchanged.

| Operation | Override off | Override on |
|---|---|---|
| Build, Rebuild, Clean, DistClean | Current refusal. Clean does not run its command. | The source gate is skipped. `CompilerValid()` still applies. |
| Named target, including Upload | Same refusal, before the target is expanded. | That target's own command runs, subject to `CompilerValid()`. |
| Virtual target `All` | Same, because `All` is not a real target and the check is on the project. | The expanded targets run. |
| `codeblocks.exe --build` and `--clean` | Batch returns before a compiler process. The batch window can stay open. | The custom command runs. |
| Zero project units | Refusal. | The custom command runs. No substitute unit is required. |
| Only `.ino`, headers, or resources | Refusal. | The custom command runs. `.ino` stays non-`ftSource`. |
| Ordinary GCC project | Refusal when it has no `ftSource`. Unchanged, because the flag is absent. | Not set by the GCC wizard. |
| Custom-build project that does not set the flag | Unchanged refusal. | — |
| Invalid compiler profile | Not reached. | Source gate skipped, then `PreprocessJob` skips the target. Nothing is built. |
| Run, Build and Run, Debug | Unchanged refusal. | Still unchanged. |

Persistence follows `makefile_is_custom`. `cbProject` defaults the bool to false, the copy constructor copies it, and `SetAllowBuildWithoutSources` calls `SetModified` only when the value changes. `projectloader.cpp` reads `allow_build_without_sources` while it walks project `Option` nodes, and writes the element only when the value is true:

```xml
<Option allow_build_without_sources="1" />
```

Absent means false. No project-file version bump. The value is on `cbProject` before `DoBuild` runs.

v4.4 has no checkbox. The wizard calls `project.SetAllowBuildWithoutSources(true)`. A project edited by hand adds the `Option` line.

Change 1 is complete only when all nine checks below pass. Checks 1–8 do not use Arduino. The zero-unit project uses a harmless marker as its Build command. On Windows that command is `cmd /c echo built> cb-build-marker.txt`. On Unix it is `echo built > cb-build-marker.txt`.

**General zero-unit test.** Custom-build project, `AllowBuildWithoutSources` true, zero project units, a valid compiler profile, and that stored Build command. Create or edit the project so it has zero units, set `allow_build_without_sources="1"`, save, close, and reopen. Then Build interactively, and again with `codeblocks.exe --build`.

1. Ordinary empty project, flag absent: "Cannot build: the active project has no source files."
2. Custom-build empty project, flag absent: the same refusal. `IsMakefileCustom()` does not bypass the gate.
3. Custom-build project, flag enabled, zero project units: the stored Build command runs. The marker file appears in the project execution directory. No `DirectCommands` command is generated. `CompilerValid()` is still reached.
4. Save, close, and reopen that zero-unit project: the property is still set, the project still has zero units, and Build still runs.
5. Batch-build the reopened zero-unit project: the stored Build command runs, the marker appears in the execution directory, and batch completion occurs normally.
6. The same zero-unit project with an invalid compiler profile: the source gate is bypassed, and `CompilerValid()` prevents the custom command.
7. Run and Debug still refuse the zero-unit project.
8. Existing ordinary GCC and AVR projects, flag absent, behave as they do today.

**Arduino non-`ftSource` test.** This is separate. It shows that the wizard's project consumes the generic property. It does not stand in for the zero-unit test.

9. Project units are only `Blink.ino`, and `AllowBuildWithoutSources` is set. `arduino-cli compile` runs. `.ino` remains non-`ftSource`. No dummy `.cpp` exists. `DirectCommands` is not used. The `.ino` diagnostic stays clickable. StepperDriver still comes from `libraries/StepperDriver`.

Clean of a zero-unit project with the flag set still runs the configured clean command, because Clean uses `DoBuild`. That path is traced and is not a tenth completion check.

---

## 2. Scope and responsibility boundary

```text
EDITOR                 Code::Blocks
BUILD                  arduino-cli compile          custom-build target
LIBRARY RESOLUTION     arduino-cli                  --libraries libraries
BOARD DISCOVERY        arduino-cli board list       wizard at creation; again before Upload and Monitor
UPLOAD                 arduino-cli upload           custom-build target
SERIAL MONITOR         Code::Blocks                 opens the resolved port.address
SIMULATION             not v4.4                     v4.5
DEBUGGING              not v4.4

compiler profile "arduino-cli"
    executable detection and validity
    GCC diagnostic parsing
    does not store FQBN, port, libraries, or the monitor

AllowBuildWithoutSources
    bypasses only the build-path source-presence gate
    does not remove the compiler-profile requirement
```

The FQBN is persistent build configuration. The port is runtime connection state. `default_fqbn` in `sketch.yaml` is what compilation uses, including when nothing is plugged in. `default_port` is the port remembered at creation, not a promise that the OS will keep that address. Build and Clean do not rediscover. Upload and Serial Monitor run `arduino-cli board list --format json` immediately before they open hardware. Nothing watches USB in the background.

Arduino CLI owns compilation, linking, library resolution, board discovery, and upload. Code::Blocks owns the live serial session: it opens the `port.address` Arduino CLI just returned. Code::Blocks does not add library `.cpp` files to `DirectCommands`, restore GCC `CompileObject` or `LinkExe` templates, select `avr-gcc`, copy library sources into the sketch root, install libraries globally, add a build driver, or edit core to make this path easier.

On macOS, Homebrew can install the `arduino-cli` formula through the existing Homebrew wizard helpers. Code::Blocks does not install Homebrew. On Windows, the wizard calls `AutoDetectCompiler("arduino-cli")` and stops when the tool is absent. This version does not download Arduino CLI on Windows.

The AVR core (`arduino:avr`) is also external. After the CLI is present, the wizard may run `arduino-cli core install arduino:avr`, and only after the user accepts that download. Code::Blocks does not vendor cores, compilers, or libraries. The wizard does not download arbitrary libraries. Library Manager and automatic dependency installation stay out of scope.

The generated project is a sketch directory:

```text
Blink/
    Blink.ino                 same base name as the directory
    Blink.cbp                 AllowBuildWithoutSources is set
    sketch.yaml               default_fqbn and, when given, default_port
    libraries/                empty until the user puts libraries here
        Adafruit_BMP280_Library/
            library.properties
            src/  or headers at the library root
            examples/
    build/                    created by arduino-cli, not by the wizard; removed by Clean
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

The profile does not gain board, port, library, monitor, baud, or simulator settings. `default_fqbn` and the remembered `default_port` stay in `sketch.yaml`. The baud rate is chosen when the monitor is launched, after the current port has been resolved. Libraries stay in the project `libraries\` directory.

During a build, `SwitchCompiler("arduino-cli")` calls `SetupEnvironment()`, which prepends `<master>/bin` to `PATH`. The Windows proof install has no `bin` directory there. The custom command can still be the bare name `arduino-cli` when the executable is already on `PATH`, which it was for the proof.

---

## 4. Commands the wizard writes

`sketch.yaml` holds the FQBN and a remembered port. On CLI 0.31.0, `default_fqbn` is enough for `arduino-cli compile`, so Build passes neither `-b` nor `-p`. A sketch that has no `sketch.yaml` fails with `Missing FQBN` until `-b` is passed. `default_port` is a preference. Upload and Monitor do not treat it as the only address that will ever work. `upload --help` names `-p` as the upload port address and says the command does not compile the sketch. The examples on that help line are `COM3` and `/dev/ttyACM2`, the same flag for either form. The value passed at runtime is whatever `port.address` the resolver returned.

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

The stored Upload command does not embed a port address:

```text
arduino-cli upload --input-dir build -p $(ARDUINO_PORT)
```

`GetMakeCommandFor` runs `ReplaceMacros`, which uppercases the macro name and then reads that environment variable (`src/sdk/macrosmanager.cpp`). The resolver sets `ARDUINO_PORT` to the current `port.address` and only then builds the Upload target. It does not call `SetMakeCommandFor` for the resolved address, because that marks the project modified and would save a concrete port into the `.cbp`. It does not rewrite `default_port` or `default_fqbn`. `-b` is omitted, so the FQBN stays the one in `sketch.yaml`. The direct proof that upload does not recompile used the command without `-p`, while `sketch.yaml` already named the live port. Adding `-p` does not add a compile; the help text says upload does not compile. Building the Upload target without the resolver is not the supported path.

`arduino-cli upload --help` on 0.31.0 says the command does not compile the sketch. The help text has `--input-dir` and no `--libraries`. From `HardwareBlink`, with `default_fqbn: arduino:avr:mega` and `default_port: COM3` and no `-b` or `-p`, `upload --input-dir build` exited 0. The hex, elf, and `compile_commands.json` hashes were the same before and after. `upload -v` showed only `avrdude` writing `build/HardwareBlink.ino.hex` (1544 bytes) to an ATmega2560 on COM3. It did not run `avr-g++`, `gcc`, or `g++`. A second upload of the already compiled `HardwareHello` hex behaved the same way. `--libraries` is not added to Upload. The quiet command captures no stdout or stderr on success; `-v` is what shows avrdude. An earlier run against COM255 failed with `ser_open()` and did not flash anything.

Both targets are `ttCommandsOnly`. Run is not upload. Upload is a second build target, chosen in the target dropdown, and started with Build.

| Target | Member of virtual target All | Build command | Clean |
|---|---|---|---|
| Build | yes | `arduino-cli compile --build-path build --libraries libraries` | remove the `build` directory |
| Upload | no | `arduino-cli upload --input-dir build -p $(ARDUINO_PORT)` | remove the `build` directory |

`includeInTargetAll` is not a usable persistence mechanism. `projectloader.cpp` still reads it for pre-1.5 projects, and the call that would write it is commented out. The wizard calls `DefineVirtualBuildTarget("All", ["Build"])` and does not call `SetIncludeInTargetAll`.

`mcAskRebuildNeeded` has to be empty on the project as well as on the target. `GetMakeCommandFor()` uses the target command only when it is non-empty; otherwise it uses the project command. A new project still has the `CompileTargetBase` default `$make -q -f $makefile $target`. With MAKE set to `arduino-cli.exe`, that default becomes `arduino-cli.exe -q -f Makefile Build`, which the proof build log actually ran before the real compile. An empty target element does not override it. The wizard writes a project-level `MakeCommands` block whose `AskRebuildNeeded` command is empty, and the same empty command on both targets. `bsTargetBuild` then logs that the ask command is empty and runs the build command. `arduino-cli` does its own incremental work inside `build/`.

Clean is not `arduino-cli compile --clean`, because that flag cleans and then compiles. Clean removes only this project's `build` directory. It does not remove `libraries\` or `serial-logs\`.

```text
macOS and other Unix    rm -rf build
Windows                 cmd /c if exist build rmdir /s /q build
```

The Unix build runner wraps the command in the configured shell. The Windows build runner does not, so the Windows clean command starts with `cmd /c`. That command, launched by Code::Blocks `--clean --target=Build`, removed `build\` and left `libraries\StepperDriver\library.properties` at its previous timestamp. `mcDistClean` is the same removal. It does not run `arduino-cli cache clean`.

`mcCompileFile` is set to the same compile line as `mcBuild`. `CompilerGCC::CompileFileDefault()` does not read it. Compile-current-file always calls `DirectCommands`. With no `CompileObject` template, Compile-current-file reports that it has no command. The supported actions are the Build target and the Upload target.

The project file carries the generic override, and nothing else is added to satisfy the source gate:

```xml
<Option allow_build_without_sources="1" />
```

The attribute is omitted when the value is false, which is the same rule as `makefile_is_custom`. An old project that lacks the attribute keeps today's behaviour. Library `.cpp` files under `libraries\` remain ordinary library sources. They may be added to the project tree so the user can open them. They are not required for Build, and they are not compiled by `DirectCommands`.

---

## 5. Wizard

Not implemented. Do not start it until a repeat of the Upload target through Code::Blocks, with the command in section 4 and no marker prefix, is shown not to refresh `compile_commands.json`. The Build, diagnostic, Clean, missing-CLI, and library-compile checks below already passed. One earlier `--build --target=Upload` invocation did refresh the hex and `compile_commands.json`. A later invocation whose command was prefixed with `cmd /c echo` left the hex unchanged, and the marker file contained the echo text rather than a confirmed `avrdude` run. That pair is the remaining pipeline question. It is not a reason to widen the `CompilerGCC` edit past the two `AllowBuildWithoutSources` checks.

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
5. Connected board. The page runs `arduino-cli board list --format json` through `IO.ExecuteAndGetOutput`. There is no `--json` flag on this CLI. The human-readable form is only a fallback when the JSON array cannot be read. The captured document, one physical Mega, is a one-element array. `--discovery-timeout 5s` returned the same document. Each element is one port:

```text
matching_boards[]          name, fqbn
port.address               COM3
port.label                 COM3
port.protocol              serial
port.protocol_label        Serial Port (USB)
port.properties.vid        0x2341
port.properties.pid        0x0042
port.properties.serialNumber
port.hardware_id
```

   This board produced one port object and one `matching_boards` entry. The parser reads `port.address` and `matching_boards[].fqbn` / `name`. It does not invent fields that were absent.
   - No board: the array is empty, as on the earlier run. The project is still created. FQBN stays `arduino:avr:uno`. `default_port` is omitted. Build works. A later Upload or Monitor resolves whatever is connected then. The user does not have to type a port into `sketch.yaml` for that.
   - One port and one `matching_boards` entry with an FQBN: write that FQBN and `port.address`. The connected board is `arduino:avr:mega`, not `arduino:avr:uno`.
   - One port and no usable FQBN: not observed on this board. The port would be written, and the FQBN would stay `arduino:avr:uno` for the user to edit.
   - Several port objects, or several `matching_boards` entries on one port: not observed. The page still uses `Wizard.AddGenericSingleChoiceListPage` (or `FillContainerWithChoices`) and lists `port — board name`. No new selector widget and no new C++.
6. The compiler page is `Wizard.AddCompilerPage(_T("arduino-cli"), _T("arduino-cli"), false, false)`. One profile, and the configuration checkboxes hidden. `GetWantDebug()` and `GetWantRelease()` are then false, so `RunProjectWizard` creates a single `default` target instead of Debug and Release. `Wizard.SetCompilerDefault()` ignores its argument and restores the IDE default, so it is not used to select this profile.

Before Upload or Serial Monitor, one script runs `arduino-cli board list --format json` with the project's Arduino CLI. It reads `default_fqbn` and the remembered `default_port`. It returns a `port.address` string and does not interpret it as a Windows COM name. A Windows move from `COM3` to `COM5`, or a macOS move from one `/dev/cu.usbmodem…` path to another, is the same case: the remembered address is gone and the list has a new one. Discovery is not repeated for Build or Clean, and nothing polls in the background.

`port.properties.serialNumber` and `port.hardware_id` are not used. On the one captured Mega they were the same string, `8513332303635110F1F1`. `vid` `0x2341` and `pid` `0x0042` name the USB product, not that board. One sample does not show that either field is always present, unique, or still there after the address changes. v4.4 does not store a hardware-identity table.

Resolution, in order:

- Remembered port is in the list and one of its `matching_boards` entries has the project's FQBN. Use that `port.address`.
- Remembered port is in the list but its FQBN is different. Stop. Name both FQBNs. Do not upload, and do not change `default_fqbn`. A Mega binary is not an Uno binary. Pointing the project at the other board is a separate edit, followed by a new Build.
- Remembered port is absent, and exactly one listed board has the project's FQBN. Use that `port.address` for this operation. Leave `sketch.yaml` unchanged. The log says the remembered address was absent and names the one that was used.
- Several listed boards have the project's FQBN. The user picks one `port.address — name` line. The binding does not choose.
- The list is empty. Build is unaffected. Upload stops with a clear message. A disconnected monitor does not connect. A monitor that was connected for this Upload stays disconnected after the restore attempt fails.
- A listed port has no FQBN. Do not invent one and do not select it automatically. Monitor needs only a port, and Upload still needs the project's saved FQBN because `-b` is omitted. If the user explicitly picks that `port.address`, this operation uses it. The saved FQBN is still the project's build identity, not a claim about that port.

The Arduino Serial Monitor is a Code::Blocks pane. It is not a build target, not Tools+, and not `arduino-cli monitor`. The user sees bytes read from the port and sends commands through a separate write. Baud rate and line ending are choices on the pane. The baud list still comes from `arduino-cli monitor -p <port.address> --describe`. On this Mega the default is `9600` and the list includes `115200`. Neither value is written into the compiler profile, the Build target, the Upload target, or `sketch.yaml`. The line ending is None, LF, CR, or CRLF, appended only to a Send. A typed character is firmware input and does not toggle recording.

Connect runs the same resolver as Upload, then opens that `port.address`. Disconnect closes the handle. The pane does not scan COM ports, WMI, IOKit, or `/dev`.

On CLI 0.31.0, Enter in `arduino-cli monitor` sent CRLF. That session is not the v4.4 host. The pane's line-ending choice replaces it. The earlier measurement still stands: while some other process held COM3, avrdude reported `ser_open(): can't open device "\\.\COM3": Access is denied.` The pane therefore closes its own handle before Upload. This Mega did not change COM number across an upload. v4.4 does not treat that as true for every board, and it rediscovers after upload because a reset can move the address. It does not watch USB in the background.

The pane has three states:

- Disconnected.
- Connected, recording off.
- Connected, recording on, file N.

Upload is the user's authorization to interrupt a connected monitor. There is no extra confirmation. If the monitor is disconnected, Upload resolves the port and uploads. If it is connected, Upload does this:

1. Remember the baud rate, the line ending, that the monitor was connected, and whether recording was on.
2. If recording is on, flush and close file N. Do not append to it later.
3. Close the serial handle. The pane is disconnected for the duration of the upload.
4. Rediscover with `arduino-cli board list --format json`. One matching FQBN selects that `port.address`, including when the address changed. Several matches ask the user which board. No match stops the upload. A different FQBN on the remembered port stops the upload. `default_fqbn` is not changed.
5. Upload the existing artifact with `-p` set to the address just resolved. Upload does not compile.
6. Rediscover again. The same rules apply. A port that moved is the one used for reconnect.
7. Reconnect at the remembered baud and line ending.
8. If recording was on and reconnect succeeded, exclusive-create a new file and record into that file only. File N stays closed.

A failed upload is reported on its own. Reconnect is still attempted. If reconnect fails, the pane stays disconnected. If recording had been on, it stays off and file N stays closed. If reconnect succeeds but the new file cannot be created, the pane stays connected, the error is shown, and recording stays off. The pane never shows recording on unless a file is open.

Ambiguous hardware uses the same rule before the upload and again before reconnect. The user picks one `port.address — name` line. Cancelling that pick does not guess and does not upload or reconnect. The project FQBN stays as saved.

### Recording an interval

Recording starts off. Each off-to-on transition exclusive-creates a new file and only then shows recording on. On-to-off flushes and closes that file and leaves the port open. Disconnect, and the close that Upload performs, do the same flush and close. An interval with no bytes still keeps its empty file. An existing file is never resumed, overwritten, or appended. Pre-upload bytes and post-upload bytes are different files. The post-upload file contains only bytes read after the monitor has reconnected.

The file is the bytes returned by the serial read, written unchanged. Bytes passed to the serial write are not recorded. Status text is not recorded. The display may substitute a glyph for a byte it cannot render. The file still contains that byte, including a NUL and a partial last line.

Files go in `<project>/serial-logs/`. Clean and DistClean do not remove that directory. The name is:

```text
serial-YYYYMMDD-HHMMSS-fff-<port>-<baud>.txt
```

`/` and `\` and `:` in the port address become `-`. If the name exists, a counter is added. No metadata is written into the text. A sidecar `<same-name>.json` holds the start time, the end time, the path, the port, the baud rate, and the FQBN.

The v4.4 target is that behavior at 9600 and 115200. The describe list goes up to 2000000, and that rate is not a promise. The two-interval check and the upload-boundary check are not run. They wait until the pane exists.

`SetupProject` then:

- renames `default` to `Build`, sets it to `ttCommandsOnly`;
- adds `Upload`, also `ttCommandsOnly`;
- defines the virtual target `All` as `Build` only;
- sets the compiler id `arduino-cli` on the project and on both targets;
- sets `SetMakefileCustom(true)` and the command strings from section 4, including an empty `AskRebuildNeeded` on the project and on both targets;
- writes `sketch.yaml`, the `.ino`, and an empty `libraries/` directory;
- adds the `.ino` as a project file;
- calls `project.SetAllowBuildWithoutSources(true)`;
- sets the active build target to `Build`.

The sketch template is the usual empty `setup` / `loop`. `*.ino` is added to the file mask of `src/sdk/resources/lexers/lexer_cpp.xml` so the sketch is highlighted as C/C++. `.ino` is not added to `FileTypeOf()`. The internal compiler is not invited to compile the sketch.

`SetAllowBuildWithoutSources` is the generic `cbProject` method from Change 1, bound beside `SetMakefileCustom`. There is no Arduino-specific binding. v4.4 does not add a Project-settings checkbox. A hand-written project sets the same attribute in the `.cbp`, or calls the method from the scripting console.

The rest of project creation uses bindings that already exist: `SetMakefileCustom`, `SetMakeCommandFor`, `SetCompilerID`, `SetTargetType`, `AddBuildTarget`, `RenameBuildTarget`, `DefineVirtualBuildTarget`, `AddGenericSingleChoiceListPage`, `FillContainerWithChoices`, `AutoDetectCompiler`, the Homebrew helpers, and `IO.ExecuteAndGetOutput`. The monitor pane is separate and is not added until this plan is reviewed.

---

## 6. Files

| File / area | v4.4 role |
|---|---|
| `src/plugins/compilergcc/resources/compilers/compiler_arduino-cli.xml` | New profile. Searches `PATH` and the Homebrew bin directories. Program C is `arduino-cli`. |
| `src/plugins/compilergcc/resources/compilers/options_arduino-cli.xml` | Program names, dependency/PCH switches, and the existing GCC diagnostic regexes. No compile or link templates. |
| `src/plugins/scriptedwizard/resources/arduino/` | New wizard script, board/port page, images, and sketch template. |
| `src/plugins/scriptedwizard/resources/config.script` | Registers "Arduino project" under Embedded Systems. |
| `src/plugins/scriptedwizard/resources/Makefile.am` | Adds the `arduino` wizard directory. |
| `src/sdk/resources/lexers/lexer_cpp.xml` | Adds `*.ino` to the C/C++ file mask. |
| `src/include/cbproject.h`, `src/sdk/cbproject.cpp`, `src/sdk/projectloader.cpp` | `SetAllowBuildWithoutSources` / `GetAllowBuildWithoutSources`, saved as `allow_build_without_sources` only when true. Change 1. |
| `src/plugins/compilergcc/compilergcc.cpp` | `PrepareBuildOrRunRouting` and `DoBuild` consult the flag. Run, Debug, and `ProjectHasDebuggeeSources()` do not. Change 1. |
| `src/sdk/scripting/bindings/scriptbindings.cpp` | `SetAllowBuildWithoutSources` and `GetAllowBuildWithoutSources` on `cbProject`. Change 1. The monitor binding stays a separate addition. |
| Serial monitor pane, Windows and POSIX port backends | Opens the resolved `port.address`. Incoming bytes, Send, baud, line ending, and one recording file per interval. Not started. |

`DirectCommands`, `Compiler::IsValid`, `CompileFileDefault`, `FileTypeOf`, `ProjectHasDebuggeeSources`, and `GetMakeCommandFor` remain unchanged.

`CompilerGCC` receives only the two narrow `AllowBuildWithoutSources` checks described in Change 1: `PrepareBuildOrRunRouting` and `DoBuild`. The compiler XML install rule is already `compilers/*.xml`.

---

## 7. Milestones

Order is fixed: prove the build, prove libraries, then the wizard, then a real board, then upload, then the monitor. The wizard is not started while the M1 Upload-through-Code::Blocks repeat in section 5 is still open.

| | Objective | Result on this machine |
|---|---|---|
| M1 | Compiler profile and the manual custom-build path. Build goes through `CompilerValid("arduino-cli")`, the custom command, `arduino-cli compile`, the existing GCC regex, and a click on the `.ino` line. No wizard. | Passed for Build, the `.ino` click, Clean, and a missing executable. The build log named no host `gcc`, `g++`, or `avr-gcc`. One `--target=Upload` run inside Code::Blocks refreshed `compile_commands.json`; the direct CLI upload did not. |
| M2 | Project-local library. Compile real StepperDriver from `./libraries`. | Passed from `FillingInterfaceArduino\libraries\StepperDriver` 1.4.1, with no non-core dependency. The library diagnostic matched `Compiler error (2)`. The IDE click was not repeated on that `.cpp`. BMP280 sibling resolution also passed and does not gate M1. |
| Change 1 | A project may set `AllowBuildWithoutSources`. Build, Rebuild, Clean, and a virtual target then pass the source-presence gate, including when the project has zero units. Run and Debug do not. | Designed, not implemented. Complete only when the nine checks in section 1 pass. Checks 1–8 do not use Arduino. |
| M3 | Wizard creates `<name>.ino`, `<name>.cbp`, `sketch.yaml`, empty `libraries\`, and the Build and Upload targets, and sets `AllowBuildWithoutSources`. | Not started. Depends on Change 1. |
| M4 | Detect or install Arduino CLI as already specified. Install `arduino:avr` only after confirmation. | Not started. This machine already had CLI 0.31.0 and core 1.8.6. |
| M5 | Detect a connected AVR board and obtain a usable port and, when the CLI gives one, an FQBN. | Passed at the CLI for one board: COM3, `Arduino Mega or Mega 2560`, `arduino:avr:mega`. The default and 5s JSON documents match. Zero boards was the earlier `[]`. Several boards were not observed. The wizard page is not written yet. |
| M6 | Build the sketch and the project-local library only through Arduino CLI, including with the board unplugged. | The unplugged compile passed (`sketch.yaml` FQBN, no `-b`). |
| M7 | Upload Blink to a physical board and watch it run. A blink period different from the previous sketch is the observation. | Flash passed: `upload --input-dir build` wrote the existing 1544-byte hex through avrdude to the Mega and did not recompile. The LED was not visible from this session, so the timing is not accepted yet. |
| M8 | Code::Blocks serial pane: bidirectional, chosen baud and line ending, interval files, and Upload that releases and reacquires the port. | Round trip passed on the Mega through `arduino-cli monitor`: `READY`, `TEST123`, `RX: TEST123`, at 9600 and 115200. Enter on that CLI sent CRLF. The pane, the incoming-only files, and the upload boundary are not implemented. |
| M9 | Clean removes `build\` only. `libraries\` and `serial-logs\` stay. The existing AVR GCC wizard and profile still behave as before. | Clean passed on the proof project for `build\` and `libraries\`. `serial-logs\` did not exist yet. The AVR wizard was not re-run. |

---

## 8. Acceptance

v4.4 is accepted when a real AVR board has been through this workflow. Items already shown on this machine are marked. Items that need a plugged-in board are not.

1. File -> New -> Project contains Arduino project under Embedded Systems.
2. The retired Arduino wizard is not restored.
3. The generated project's compiler profile is `arduino-cli`. GCC, `avr-gcc`, Debug, and Release are not selected.
4. A missing Arduino CLI is detected and, on macOS, can be installed through the existing Homebrew confirmation. A missing `arduino:avr` core is installed only after the user accepts. Windows does not download the CLI. This machine already had CLI 0.31.0 and core 1.8.6.
5. The generated directory contains `<name>.ino`, `<name>.cbp`, `sketch.yaml`, and `libraries\`. The `.cbp` sets `allow_build_without_sources="1"`. It contains no file whose only role is to satisfy `FileTypeOf()`.
6. StepperDriver compiles from that project's `libraries\StepperDriver`. Shown from `FillingInterfaceArduino\libraries\StepperDriver` 1.4.1, which has no `depends=`. The sketch is `#include <BasicStepperDriver.h>` and `BasicStepperDriver stepper(200, 8, 9); stepper.begin(120, 1); stepper.rotate(360);`.
7. Code::Blocks does not invoke host `gcc`, `g++`, or `avr-gcc`, and does not emit per-file `DirectCommands`, for the sketch or the library. Shown: the Build log contained one `arduino-cli compile` line, and the library objects were under `build\libraries\`.
8. A deliberate error in the `.ino` is clickable and opens that line. Shown: `InoClick.ino:3:1`, double-click, caret on line 3 column 1.
9. A deliberate error in a library `.cpp` is tested the same way. The CLI line `BasicStepperDriver.cpp:22:1: error: expected unqualified-id before 'template'` matches `Compiler error (2)`. The click inside the IDE is still to be done. BMP280, after StepperDriver, must also resolve Adafruit Unified Sensor and Adafruit BusIO from the same `libraries\` root, and must fail with `Adafruit_Sensor.h` when they are absent. Shown at the CLI. Neither test used a global copy.
10. With the board unplugged, Build still compiles for the FQBN in `sketch.yaml`. Shown: no `-b` on the command, platform `arduino:avr` 1.8.6.
11. A physically connected Arduino is detected with `arduino-cli board list --format json`. Shown: one object, COM3, `Arduino Mega or Mega 2560`, `arduino:avr:mega`. The earlier empty machine returned `[]`.
12. Port and FQBN stay separate. Zero boards leave the port empty and the FQBN at `arduino:avr:uno`. One recognised board fills both from the CLI. Shown: the Mega kept `arduino:avr:mega`. Several boards were not observed; they still use the existing single-choice wizard page.
13. A Blink sketch compiles and uploads to a physical board. Shown without `-p`, while `sketch.yaml` named the live port: compile 1544 bytes, avrdude wrote that hex, hashes unchanged. The runtime Upload command adds `-p` with the resolved `port.address` and still does not compile. The LED was not visible here.
14. The board's LED period is one that was not already running, so the new binary is the thing being observed. Not shown. The sketch is 200 ms on, 1800 ms off, on `LED_BUILTIN`.
15. Serial round trip, shown on the Mega at COM3 through `arduino-cli monitor`. A sketch prints `READY`, the user types `TEST123`, and the board returns `RX: TEST123`, at 9600 and at 115200. Enter on that CLI sent `54 45 53 54 31 32 33 0D 0A`. The v4.4 pane replaces that fixed ending with None, LF, CR, or CRLF. The pane itself is not shown yet.
16. The pane connects to the resolver's `port.address`, at a baud taken from `monitor --describe`. Incoming bytes stay visible. Send writes only the command plus the selected ending. Not run.
17. Upload while the pane is connected does not ask the user to disconnect. It closes the handle, uploads the existing artifact, rediscovers, and reconnects at the same baud and line ending. Shown only as the reason: avrdude got `Access is denied` while another process held COM3. The automatic release and reconnect are not run.
18. Clean and DistClean remove `build\` and do not delete `libraries\`, including `library.properties`. Shown on the StepperDriver proof.
19. Upload does not pass `--libraries` and does not compile sketch or library sources through `DirectCommands`.
20. The existing AVR GCC wizard and the `avr-gcc` profile still build an AVR project as they do today.
21. Remembered port still listed, and its FQBN matches the project: Upload and Monitor use that `port.address`.
22. Remembered port absent, exactly one listed board matches the saved FQBN: the operation uses that new `port.address` and does not rewrite `sketch.yaml`.
23. Several listed boards match the saved FQBN: the user selects one. The binding does not guess.
24. Remembered port is present with a different FQBN: no upload, and `default_fqbn` is not changed.
25. No board listed: Build still runs. Upload stops with a clear message. Monitor does not start.
26. The value passed to `-p` is the CLI's `port.address`. The resolver has no COM-port or `/dev` scan of its own.
27. That Upload still sends the existing artifact and does not compile.
28. The pane stays bidirectional with recording off and with recording on. Baud still comes from `monitor --describe`.
29. Recording starts off. Off to on exclusive-creates a new file under `serial-logs\`. On to off flushes and closes it and leaves the port open. A second interval is a different file. Existing files are not overwritten. Not run.
30. Each file contains only bytes read from the port during that interval. Typed commands are not in the file. The display may render a byte the file still stores unchanged. A sidecar holds the times, port, baud, and FQBN. Clean does not delete `serial-logs\`.
31. If the file cannot be created or a write fails, recording stays or returns off, the error is visible, and the connection stays up when the port is still open. Disconnect flushes an open file. No source-level debugging is claimed.
32. Upload while connected, recording off: the pane disconnects, upload runs, rediscovery runs again, and the pane reconnects with the previous baud and line ending. No confirmation dialog. Not run.
33. Upload while connected, recording on: file N is flushed and closed, upload runs, the pane reconnects, and recording resumes in a new exclusive-created file. File N is not reopened or appended. Not run.
34. Those two files are distinct. The second contains only bytes received after reconnection.
35. A failed upload is reported on its own, and the pane still attempts to reconnect. If reconnect fails, the pane stays disconnected and recording stays off. If reconnect works but the new file fails, the pane stays connected and recording stays off.
36. A port address that changes across the upload is the address used to reconnect, when exactly one listed board has the project's FQBN.
37. Several matching boards ask the user which one. The upload does not guess, and it does not change `default_fqbn`.

---

## 9. What stays rejected

- Repairing the pre-v3.5 Arduino wizard, or copying its `avr-gcc` recipes, programmer tables, or fuse tables into this wizard.
- Registering `arduino-cli` as a per-file compiler with GCC `CompileObject` / `LinkExe` templates.
- Adding Arduino library `.cpp` files to `DirectCommands`, or compiling them with the listed `avr-gcc` or host GCC profile.
- Copying library sources into the sketch root, or generating a build target per library.
- Installing the proof libraries into the global Arduino user directory in order to make a test pass.
- Treating compiler id `null`, a `ttCommandsOnly` target, or `IsMakefileCustom()` as a substitute for `AllowBuildWithoutSources`. Those settings do not skip the source-presence gate. Build still requires `IsValid()` on a real executable.
- Adding `.ino` to `FileTypeOf()`, or generating a source file that exists only so `ProjectHasDebuggeeSources()` returns true.
- Changing `ProjectHasDebuggeeSources()` itself. That function still answers "may Run or Debug treat this project as a debuggee?" The build override is a separate question.
- A new build driver, or a change to `DirectCommands`, `Compiler::IsValid`, `CompileFileDefault`, `FileTypeOf`, `ProjectHasDebuggeeSources`, or `GetMakeCommandFor`. The only `CompilerGCC` edit is the two `AllowBuildWithoutSources` checks. The serial pane is not a build driver.
- Teaching Compile-current-file to honour `mcCompileFile`.
- Changing the diagnostic parser so a library path matches. The existing regex already matched the Windows lines from this CLI. A future mismatch is another regex in `options_arduino-cli.xml`, not a parser edit made to force this test.
- Automatic Arduino include-path discovery for code completion. Headers under `libraries\` are a build concern for the CLI. Code completion is separate and is not part of v4.4 unless the existing project-file mechanism can show the files with no core change.
- Embedding the Arduino board database. The wizard may list only the boards `arduino-cli board list` just returned, using `AddGenericSingleChoiceListPage` or `FillContainerWithChoices`. It does not ship a board catalog.
- Win32, WMI, `/dev`, or IOKit port scans, a USB watcher, and a stored table of serial numbers. Discovery stays `arduino-cli board list --format json`.
- Rewriting `default_fqbn` because a different board is plugged in, or treating that as the same event as a new port address.
- Making Monitor a Build target, or running it through `IO::Execute`, `DirectCommands`, or Tools+.
- Hosting the live session in `arduino-cli monitor`, including a PTY or ConPTY wrapper around it. That stream mixes typed input with received bytes.
- Redirecting or teeing `arduino-cli monitor`. That fails the terminal check.
- A confirmation dialog whose only purpose is that the monitor is connected. Upload already authorizes the release and the reconnect.
- Appending bytes from after an upload onto the recording that was open before it.
- Downloading Arduino CLI on Windows, installing Homebrew, vendoring cores, or downloading arbitrary libraries. Library Manager and automatic dependency installation stay out of scope.
- Debug. `arduino-cli debug` and a GDB remote stub are outside this version. Run stays the existing commands-only behaviour and is not the upload action, and it is not the serial monitor.
- Simulation. v4.4 does not integrate Wokwi, simavr, any other AVR or Arduino simulator, a simulated serial port, a simulated peripheral, or simulation debugging. That work is v4.5 and stays a separate architecture. It does not extend the `arduino-cli` compiler profile.
- `arduino-cli cache clean`, and any board package other than `arduino:avr`.
- Making `arduino-cli` the IDE's default compiler. `RunProjectWizard` already saves and restores the previous default around project creation.

---

## 10. Risks and maintenance boundary

**The profile can drift into a real compiler.** The maintenance point for "what is Arduino CLI" is `options_arduino-cli.xml` and `compiler_arduino-cli.xml`. Adding GCC command templates there would make Compile-current-file, and any future non-custom build, call `arduino-cli` as though it were `gcc`.

**Detection and validity are different calls.** `AutoDetectInstallationDir()` returning a master path is what the wizard checks. `IsValid()` later checks that the file is still there. An empty master path makes `IsValid()` return true during initialisation; the wizard's `detected=1` requirement is what keeps a missing CLI from creating a project.

**An empty target ask-command is not empty.** It falls through to the project default and runs `arduino-cli.exe -q -f Makefile <target>`. The project-level command has to be cleared as well.

**The source gate and the debuggee check are the same function today, and they must not stay coupled.** `ProjectHasDebuggeeSources()` (`src/sdk/debuggeedescription.cpp`) is documented as "empty projects must not Debug a leftover binary." `DoBuild()` reuses it. The new flag is read only in `PrepareBuildOrRunRouting` and `DoBuild`. Run (`RunActiveProjectAfterPrepare`), Debug (`debuggergdb.cpp`), and the Debug menu keep the existing check. A project with the flag and no `ftSource` can Build and Clean. It still cannot Run or Debug.

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

**The current port is runtime state.** `default_port` can go stale after an unplug, a new USB socket, a reboot, or another machine. Upload and Monitor therefore call `arduino-cli board list --format json` immediately before they open hardware. Build does not. The remembered port is preferred when it is still listed with the project's FQBN. A single new address for that same FQBN is used for that operation and is not written back. Several matches, or a different FQBN on the remembered address, are shown to the user. A different FQBN is a different binary, not another name for the same port. Editing `sketch.yaml` by hand remains possible and is not the normal step. The compiler profile still has no board flag: the unplugged compile used `default_fqbn` and did not pass `-b`.

**The board-list document is one object per port.** The connected Mega produced one array element, one `matching_boards` entry (`name`, `fqbn`), and `port.address` / `protocol` / `properties.vid` / `properties.pid` / `properties.serialNumber` / `hardware_id`. `--json` is not a flag. The default timeout and `--discovery-timeout 5s` matched. An empty array is still the no-board result. A slow port that needs the longer timeout was not the case here.

**Upload without a prior Build fails in the CLI**, because `--input-dir build` is empty. The wizard does not chain compile into the Upload target. The user builds, then uploads. On the Mega, direct upload reused the hex and did not recompile. Exit status 0 from `codeblocks.exe --build` is still not proof of upload: `BatchJob()` returns 0 after calling the compiler plugin. The LED timing was not visible in this pass, so it remains the physical check.

**A recording file is the serial read for one interval.** Typed commands and pane status are not written into it. Upload closes the open file before it touches the port. Reconnecting with recording still on creates a different file. The two sides of an upload are not one acquisition. The v4.4 target is that behavior at 9600 and 115200. The describe list goes up to 2000000, and that rate is not a promise. `serial-logs\` is user data. The pane and the upload-boundary check are not implemented yet.

**Upload closes the handle it needs.** On this Mega, upload while `arduino-cli monitor` held COM3 failed with `Access is denied`. The pane therefore drops the port before avrdude runs, then rediscovers and opens it again. A failed upload is still followed by that reconnect attempt. The reconnect can fail on its own, and the pane then stays disconnected. This Mega kept COM3 across an upload. Another board can move. Several matches are a question, not a guess. This measurement is a USB-serial Mega, not a native-USB board. The baud rate is chosen from `monitor --describe` and is not fixed at 9600. The line ending is the pane's choice, not the CLI's CRLF.

**The Blink LED was not visible from this session.** avrdude did write the 200 ms / 1800 ms hex. The board was later overwritten by the Hello sketch and then by `SerialRoundTrip` at 115200.

**Windows does not gain an installer in this version.** A Windows machine without `arduino-cli` on `PATH`, or in a directory the profile searches, cannot finish the wizard.

---

## 11. Done when

A user can create an Arduino project, with Arduino CLI found or installed on macOS and already present on Windows, accept installation of the AVR core when it is missing, and produce a sketch that builds with the board unplugged. The generated project sets `AllowBuildWithoutSources` and contains no file added only to satisfy `FileTypeOf()`. Libraries under `libraries\<directory>\` are compiled by Arduino CLI. A syntax error in the `.ino` opens at the reported line. With a board connected, the wizard records its port and FQBN, Upload places a Blink sketch on that board, and the LED shows the new period. The project still builds with no board attached. When Upload or Serial Monitor is requested, Code::Blocks resolves the connected board through Arduino CLI instead of assuming the port saved at creation is still valid. A new port address for the expected FQBN is used without a hand edit. Ambiguous or mismatched hardware is shown to the user and is not guessed. The Arduino Serial Monitor is a Code::Blocks pane on the port Arduino CLI resolved. The user selects the baud rate and the line ending, sees incoming bytes, and sends commands. Recording writes only those incoming bytes, one new file per interval. Upload while the pane is connected closes the port, uploads, rediscovers, and reconnects. A recording that was on resumes in a new file after reconnect, not in the file that ended before the upload. Source-level debugging stays outside v4.4. Clean removes `build\` and leaves `libraries\` and `serial-logs\` in place. Code::Blocks does not invoke GCC to build the sketch or the libraries. An ordinary project that does not set the flag still refuses to build when it has no `ftSource` file.

Code::Blocks edits the project, starts the commands, and owns the serial session. Arduino CLI compiles, resolves Arduino libraries, detects hardware, and uploads. The `arduino-cli` compiler profile exists only so the current custom-build pipeline will run those commands and parse GCC diagnostics. Simulation and debugging are not part of v4.4. `DirectCommands` is not on this path.

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

arduino-cli board list
arduino-cli board list --format json
arduino-cli monitor -p COM255 --describe
```

An earlier `board list` printed `No boards found.` and the JSON form printed `[]`. `monitor --describe` printed `baudrate` default `9600` and exited 0 without a session.

With the Mega connected, from `%TEMP%\cb-v44-proof`:

```text
arduino-cli board list
arduino-cli board list --format json
arduino-cli board list --format json --discovery-timeout 5s

cd %TEMP%\cb-v44-proof\HardwareBlink
arduino-cli compile --build-path build --libraries libraries
arduino-cli upload --input-dir build
arduino-cli upload -v --input-dir build

cd %TEMP%\cb-v44-proof\HardwareHello
arduino-cli compile --build-path build --libraries libraries
arduino-cli upload --input-dir build
arduino-cli monitor -p COM3 -c baudrate=9600
arduino-cli upload -v --input-dir build

cd %TEMP%\cb-v44-proof\SerialRoundTrip
arduino-cli monitor -p COM3 --describe
arduino-cli compile --build-path build --libraries libraries
arduino-cli upload --input-dir build
arduino-cli monitor -p COM3 -c baudrate=9600
arduino-cli compile --build-path build --libraries libraries
arduino-cli upload --input-dir build
arduino-cli monitor -p COM3 -c baudrate=115200
```

The list named COM3, `Arduino Mega or Mega 2560`, `arduino:avr:mega`. Both JSON runs saved the same one-element document. HardwareBlink compiled to 1544 bytes. Both quiet uploads exited 0 and left the hex and `compile_commands.json` hashes unchanged. The verbose upload log is only avrdude. The monitor console showed `Hello from Code::Blocks` repeatedly. A later round-trip sketch, `SerialRoundTrip`, showed `READY` and then `RX: TEST123` after Enter, at 9600 and at 115200. Enter sent CRLF. Upload while a monitor console was open failed with `Access is denied`. The round-trip did not change that ownership result. The Blink LED was not visible from this session. The Code::Blocks `--target=Upload` repeat is still pending `AllowBuildWithoutSources`.

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

Build messages showed that error on line 3. Double-click moved the caret from line 1 to line 3, column 1. That IDE build used a throwaway `.cpp` outside the sketch root so `DoBuild` would start. That file is rejected. The same proof is repeated, after Change 1, on a project whose only units are the `.ino` and, when present, the real library sources.

`BatchJob()` returns 0 after it has invoked the compiler plugin (`src/src/app.cpp`). That process exit code is not the compiler status. The compiler status is the "Process terminated with status" line in the build log.
