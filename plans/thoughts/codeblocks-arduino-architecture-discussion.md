# Code::Blocks Arduino Support: Architectural Direction

**Date:** 27 October 2026  
**Status:** Architectural discussion and future investigation note, not an implementation plan

## 1. Why this came up

A newly created Code::Blocks Arduino project failed comprehensively on macOS. The visible build messages included:

- `fatal error: 'Arduino.h' file not found`
- repeated errors that `main.cpp` could not be found when included with angle brackets
- a warning that the `-s` argument was unused during compilation

The generated project also contained a `cores/libraries.cpp` aggregation file that conditionally included Arduino library implementation files directly, for example:

```cpp
#include <Arduino.h>

#if USE_EEPROM
#include "EEPROM.cpp"
#endif

#if USE_ETHERNET
#include "Ethernet.cpp"
#include "EthernetClient.cpp"
#include "EthernetServer.cpp"
#endif
```

This raised a more fundamental question than merely repairing include paths: **is the current Code::Blocks Arduino integration reproducing too much of the Arduino build system itself?**

## 2. Concern with the existing design

The current arrangement appears to treat an Arduino sketch approximately like an ordinary C++ project by copying, collecting, or directly including Arduino core and library implementation files.

That approach is potentially brittle because Arduino builds depend on more than a conventional compiler invocation. The selected board and installed platform determine, among other things:

- the Arduino core;
- the board variant;
- architecture-specific compiler and linker flags;
- library discovery and selection;
- conversion of sketch sources into compilable C++;
- upload tooling and board/port handling.

Repairing the immediate errors may therefore lead to a sequence of further compatibility problems. Even if support were repaired for one AVR board, the same implementation might not generalise cleanly to other Arduino platforms such as ESP32, SAMD, RP2040, or subsequently updated board packages.

The risk is that Code::Blocks unintentionally owns an ongoing Arduino build-system compatibility project.

## 3. What Arduino CLI is

`arduino-cli` is not a third-party C++ library comparable to SFML or GLFW. It is an official Arduino command-line tool.

Arduino describes Arduino CLI as an all-in-one solution providing board and library management, sketch building, board detection, uploading, and other services needed for Arduino-compatible platforms. Arduino also states that Arduino CLI is at the heart of its official development software.

Homebrew is only one installation route on macOS:

```bash
brew install arduino-cli
```

Arduino also distributes prebuilt binaries for macOS, Windows, and Linux, so a future Code::Blocks integration would not inherently have to depend on Homebrew on every platform.

Official references:

- [Arduino CLI documentation](https://docs.arduino.cc/arduino-cli/)
- [Arduino CLI command reference](https://docs.arduino.cc/arduino-cli/commands-reference/arduino-cli/)
- [Arduino CLI installation options](https://arduino.github.io/arduino-cli/dev/installation/)

## 4. Proposed architectural separation

A cleaner future design would separate responsibilities.

### Arduino CLI owns

- board-platform and core management;
- library discovery and dependency resolution;
- sketch preprocessing and compilation;
- architecture-specific build flags;
- board detection;
- uploading;
- serial-monitor and debugging operations where supported.

### Code::Blocks owns

- the project and file view;
- proper C and C++ editing;
- syntax highlighting;
- symbol browsing and code navigation;
- search and refactoring support;
- readable presentation of compiler diagnostics;
- build, upload, monitor, and possibly debug commands exposed through the IDE;
- board and port selection in the project user interface.

At the simplest conceptual level, Code::Blocks would invoke commands such as:

```bash
arduino-cli compile --fqbn arduino:avr:uno MySketch
arduino-cli upload --fqbn arduino:avr:uno --port /dev/cu.usbmodemXXXX MySketch
```

The exact command construction, paths, configuration storage, and platform handling would need to be verified during implementation. The important architectural point is that **Arduino's own tooling should determine how an Arduino program is actually built**.

## 5. The real added value over Arduino IDE

The purpose would not be to compete with Arduino IDE by reinventing Arduino's build system.

The credible added value is:

> **Use Arduino's official build machinery inside a capable, conventional C/C++ development environment.**

This matters when a sketch grows beyond a single `.ino` file and begins to contain real C++ components, for example:

```text
MyProject/
    MyProject.ino
    src/
        Controller.cpp
        Controller.h
    lib/
        MySensor/
            MySensor.cpp
            MySensor.h
```

Code::Blocks could provide a more natural environment for reading and editing classes, headers, implementation files, libraries, and larger source trees, while Arduino CLI remains responsible for producing valid firmware.

This is particularly relevant educationally. A beginner may start with `setup()` and `loop()`, then gradually move into ordinary C++ files, classes, reusable modules, and libraries without leaving the IDE or being trapped in a sketch-only mental model.

## 6. Why macOS and Homebrew change the apparent scale of the task

Code::Blocks Arduino support is a side feature inside an already existing IDE, so an architectural replacement initially sounds disproportionate.

However, the modern macOS environment changes the practical calculation:

- Homebrew makes specialised toolchains easy to install and update.
- Arduino CLI already encapsulates the difficult Arduino-specific build knowledge.
- Code::Blocks may need relatively little Arduino-specific code if it delegates rather than imitates.
- The redesign might therefore be smaller and more maintainable than repairing the accumulated assumptions in the legacy Arduino project generator.

In other words, reviewing the architecture of this small corner of Code::Blocks is not necessarily an attempt to redesign the entire universe. The surrounding tooling now provides an opportunity to **remove responsibilities from Code::Blocks**, rather than add another large subsystem.

## 7. Scope caution

This is not a simple fix. It should be treated as a deliberately scoped architectural investigation rather than as a sequence of isolated repairs to generated paths and compiler flags.

## 8. Suggested investigation sequence for later

1. **Locate the current Arduino wizard and generated project machinery.**  
   Identify where `libraries.cpp`, copied core files, include paths, compiler flags, and build targets are generated.

2. **Determine whether Arduino support is isolated.**  
   Establish whether it is only a wizard/template plus compiler configuration, or whether a dedicated plugin is involved.

3. **Create a manual proof of concept outside the wizard.**  
   Open an ordinary Arduino sketch directory in Code::Blocks and configure one custom build command that calls `arduino-cli compile`.

4. **Verify diagnostic usability.**  
   Check whether Arduino CLI output is parsed adequately by the Code::Blocks build log and whether file/line references are clickable.

5. **Test a narrow board baseline.**  
   Begin with one board family, probably an Arduino AVR board, without claiming cross-platform or cross-architecture completeness.

6. **Add upload only after compilation works.**  
   Keep build integration and device/port handling as separate milestones.

7. **Decide whether the legacy wizard should be replaced, deprecated, or retained.**  
   Make that decision from the proof of concept rather than attempting to repair and redesign simultaneously.

8. **Only then consider board/library management UI.**  
   Arduino CLI already provides those operations. Code::Blocks should expose only the parts that provide a clear usability benefit.

## 9. Important non-goals for the first implementation

A first bounded implementation should not attempt to:

- recreate Arduino Boards Manager;
- recreate Arduino Library Manager;
- parse and reproduce Arduino platform definitions;
- support every board architecture immediately;
- copy the Arduino core into each generated Code::Blocks project;
- compile Arduino library `.cpp` files through a hand-maintained aggregation file;
- redesign the general Code::Blocks build system;
- promise debugging support before the selected board/toolchain has been tested.

## 10. Provisional conclusion

The failures observed in the generated Arduino project are probably symptoms of an obsolete ownership boundary.

The promising direction is not:

> Make Code::Blocks better at pretending to be Arduino's build system.

It is:

> Let Arduino CLI build and upload Arduino software, while Code::Blocks provides the decent C/C++ editor, project navigation, diagnostics, and development workflow around it.

That approach preserves Code::Blocks' genuine strength, avoids duplicating Arduino's specialised build knowledge, and could turn a currently broken side feature into a relatively thin integration with a defensible maintenance boundary.

