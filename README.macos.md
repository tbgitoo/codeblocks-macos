# Code::Blocks for macOS

This repository provides an unofficial native macOS build of Code::Blocks for Apple Silicon. It is based on Code::Blocks trunk revision 14050 and IDE version 25.03.

This project is for macOS only. Windows and Linux users should use the official [Code::Blocks website](https://www.codeblocks.org) and the original Code::Blocks source repository:

```text
svn://svn.code.sf.net/p/codeblocks/code/trunk
```

The Code::Blocks IDE is licensed under GPL-3 (`COPYING`). The SDK is licensed under LGPL-3, as stated in the SDK source-file headers.

## Download and install

Prebuilt macOS releases are available from the **Releases** section of this GitHub repository. Download the `.dmg` file for the version you want, open it, and copy `CodeBlocks.app` to your Applications folder or another location of your choice.

The application is built natively for Apple Silicon. It does not require Rosetta.

The application is not currently notarised by Apple. Depending on your macOS security settings, the first launch may therefore need to be allowed manually in **System Settings > Privacy & Security**.

## Debugger support

Code::Blocks is an IDE. A separate debugger is required for debugging C and C++ programs.

The companion Apple Silicon GDB port is available in the [binutils-gdb repository](https://github.com/tbgitoo/binutils-gdb).

If debugging stops with **No stack** or “Unable to find Mach task port”, GDB is not accepted by macOS as a debugger. See **[code_signing.md](code_signing.md)** for the symptom, certificate setup, and `codesign` commands for a working local signature (including the `com.apple.security.cs.debugger` entitlement).

### Code::Blocks v1.0 and v1.1

The debugger is not included in Code::Blocks v1.0 or v1.1.

To debug programs with these releases, install either:

- the companion Apple Silicon GDB port; or
- another GDB build that works on your macOS system.

Then configure the debugger executable manually in Code::Blocks:

```text
Settings > Debugger
```

Select the GDB debugger configuration and set its executable path to the installed `gdb` binary.

The companion GDB port supports the macOS-specific `inferior-tty-claim` setting used by Code::Blocks v1.1 to avoid a harmless but noisy Darwin controlling-terminal warning.

### Code::Blocks v1.2 and later

Starting with v1.2, the matching Apple Silicon GDB is bundled inside `CodeBlocks.app` and used as the default debugger on macOS.

No separate GDB installation or manual debugger-path configuration should be required when using the bundled default. An explicitly configured custom debugger path still takes precedence.

If an older Code::Blocks configuration already contains a custom debugger path, clear or update it manually under:

```text
Settings > Debugger
```

## Build the application yourself

### Prerequisites

Install the required build tools and wxWidgets with Homebrew:

```bash
brew install wxwidgets autoconf automake libtool pkg-config
```

### Build Code::Blocks

From the root of this repository, run:

```bash
./bootstrap
./configure --prefix="$(pwd)/_cb_install" --disable-fortran --disable-contrib-plugins CC=clang CXX=clang++
make -j"$(sysctl -n hw.ncpu)"
make install
./make_cb_app_bundle.sh
```

The staged installation is written to `_cb_install` under the repository. `make_cb_app_bundle.sh` uses that staged installation to construct `CodeBlocks.app`.

Quit any running copy of Code::Blocks before replacing an existing application bundle.

If you want to repackage the .dmg file, run
```bash
./make_cb_dmg.sh
```

also from the repo root, after succesful run of ./make_cb_app_bundle.sh


### Building with bundled GDB for v1.2 and later

The companion GDB source tree is expected to be in a sibling directory:

```text
parent-directory/
  codeblocks-macos/
  binutils-gdb/
```

The Code::Blocks packaging process uses the staged GDB installation from:

```text
../binutils-gdb/staging/
```

The important inputs include:

```text
../binutils-gdb/staging/bin/gdb
../binutils-gdb/staging/share/gdb/
```

`make_cb_app_bundle.sh` copies those into the app and vendors the license-permitted runtime dylibs currently required by that GDB build (`libzstd`, `libmpfr`, `libgmp`, with install-name rewrite). Override the staging prefix with `CB_BUNDLE_GDB_PREFIX` if the sister tree is elsewhere. The bundle fails if GDB or `share/gdb` is missing.

Bundled `gdb` is ad-hoc signed with the `com.apple.security.cs.debugger` entitlement (`osx/gdb.entitlements`). Without that entitlement, macOS refuses the Mach task port and debugging stops with “Unable to find Mach task port” / “No stack”. Ad-hoc signing is not always enough on every Mac and is not reliable for downloaded `.dmg` installs; full local certificate steps are in [code_signing.md](code_signing.md).

Resulting layout:

```text
CodeBlocks.app/Contents/Resources/debugger/
  bin/gdb
  share/gdb/
  lib/           # vendored dylibs
  licenses/      # GDB + dylib license notices
```

No separate GDB installation is required for the macOS default. Existing debugger or Terminal settings in `default.conf` are **not** auto-migrated; clear or edit them manually to use the bundled defaults.

## If `bootstrap` cannot find the wxWidgets macros

`./bootstrap` requires the wxWidgets Autoconf macros, including `wxwin.m4`.

Homebrew installs these in a wxWidgets-specific `aclocal` directory, which `aclocal` does not always search automatically. The bootstrap script attempts to find the correct directory when `wx-config` is available on `PATH`.

If `bootstrap` still reports that it cannot find `AM_OPTIONS_WXCONFIG`, run:

```bash
ACLOCAL_FLAGS="-I $(wx-config --prefix)/share/wx/$(wx-config --release)/aclocal" ./bootstrap
```

Autoconf may also print warnings about obsolete macros. Those warnings are not normally fatal. A missing `wxwin.m4` or `AM_OPTIONS_WXCONFIG` macro is the relevant error in this case.

## macOS application contents

The macOS-specific runtime components are contained inside `CodeBlocks.app`. No helper script in the user's home directory is required.

For v1.1, the relevant layout is:

```text
CodeBlocks.app/
  Contents/
    MacOS/
      codeblocks
      cb_console_runner
      cb_debug_console_hold
```

For v1.2 and later, the bundled debugger is added under:

```text
CodeBlocks.app/
  Contents/
    Resources/
      debugger/
        bin/
          gdb
        share/
          gdb/
        lib/
        licenses/
```

## Technical notes

### Terminal-based console handling

The current macOS implementation uses Terminal.app for console programs.

The built-in terminal command uses `osascript` to run:

```text
clear; $SCRIPT; exit
```

`$SCRIPT` is generated by Code::Blocks.

For a normal Run, it contains the bundled `cb_console_runner` followed by the program. The runner displays `Press any key to continue.` when the program exits. Code::Blocks does not wait for that keypress and remains responsive.

For Debug, it contains the bundled `cb_debug_console_hold` helper with a unique token. The helper keeps the Terminal tty alive while GDB uses it for the inferior process.

The holder distinguishes two exit paths:

- On normal inferior completion, Terminal may display `Press any key to close...`. Code::Blocks does not wait for the keypress.
- On Stop Debugger or Code::Blocks shutdown, the holder exits immediately without prompting.

`clear` hides the generated helper command from the visible Terminal window. `exit` prevents an unused interactive shell from remaining after the helper finishes. If Terminal was not already running, the launcher reuses the initial Terminal window instead of creating an additional empty one.

The bundled console helpers are resolved relative to the running Code::Blocks executable through `ConfigManager::GetExecutableFolder()`. The application is therefore relocatable and is not hardcoded to `/Applications`.

### Darwin GDB terminal handling in v1.1

Terminal.app already owns the pseudo-terminal used for debugging. GDB normally attempts to make that terminal the inferior process's controlling terminal with `TIOCSCTTY`. Darwin refuses this operation because the terminal belongs to Terminal's existing session.

The failed claim does not prevent normal stdin, stdout and stderr from working, but it produces a long warning.

The companion Apple Silicon GDB port adds:

```gdb
set inferior-tty-claim on
set inferior-tty-claim off
show inferior-tty-claim
```

The default is `on`, preserving normal GDB behaviour.

When Code::Blocks v1.1 creates a Terminal-based debug console, it probes:

```gdb
show inferior-tty-claim
```

If the command is supported, Code::Blocks sends:

```gdb
set inferior-tty-claim off
```

before assigning the Terminal tty with GDB's `tty` command. GDB then uses the tty for inferior input and output without attempting the controlling-terminal claim that Darwin would reject.

A GDB build without this setting is left unchanged. Debugging may still work, but the Darwin controlling-terminal warning may remain visible.

This behaviour is macOS-specific. It does not change Code::Blocks debugger handling on Windows or Linux.

## Existing Code::Blocks settings

Explicit user settings override compiled defaults.

If an older configuration specifies a custom Terminal command or debugger executable, the application will continue to use that setting. To test the built-in defaults, clear the relevant setting manually or use a clean Code::Blocks configuration.
