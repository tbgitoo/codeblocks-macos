# Code::Blocks for macOS

Unofficial native macOS build of Code::Blocks for Apple Silicon, on top of trunk r14050.

The IDE is GPL-3 (`COPYING`). The SDK is LGPL-3, as stated in the SDK file headers.

On macOS the default terminal command is an `osascript` that runs whatever is substituted for `$SCRIPT`. These changes fill that in:

- The debugger console runs its `sleep` command inside Terminal, so the debugger can find the window and attach the program's input and output to it.
- A tool launched in a new console is placed in that same `$SCRIPT`, with quotes escaped so an `osascript` tool does not close the Terminal string early.

Build and Run already uses `$SCRIPT`. No custom tool is required.

### Debugger console and `inferior-tty-claim` (v1.1)

Terminal’s session already owns that pty, so Darwin refuses GDB’s attempt to make it the inferior’s controlling terminal (`TIOCSCTTY`). Program input/output still work; only the claim fails. With a matching Darwin GDB build that supports it, Code::Blocks probes `show inferior-tty-claim` and, if available, runs `set inferior-tty-claim off` before `tty …`. That keeps I/O on the Terminal window without attempting the claim, so the long “Failed to set controlling terminal” / “Darwin will not transfer it” warning is not printed.

Use the Apple Silicon GDB from the companion `binutils-gdb` port (or any build that implements `inferior-tty-claim`). Stock GDB without that setting is detected and left unchanged (the warning may still appear). Linux and Windows debugger paths are not affected.

## Build

Install the build tools and wxWidgets, then from this directory:

```bash
brew install wxwidgets autoconf automake libtool pkg-config
./bootstrap
./configure --prefix="$(pwd)/_cb_install" --disable-fortran --disable-contrib-plugins CC=clang CXX=clang++
make -j"$(sysctl -n hw.ncpu)"
make install
./make_cb_app_bundle.sh
```

`./bootstrap` needs the wxWidgets autoconf macros (`wxwin.m4`). Homebrew keeps them under `share/wx/<version>/aclocal`, which `aclocal` does not search by default. The script adds that path automatically when `wx-config` is on `PATH`. If bootstrap still cannot find `AM_OPTIONS_WXCONFIG`, run:

```bash
ACLOCAL_FLAGS="-I $(wx-config --prefix)/share/wx/$(wx-config --release)/aclocal" ./bootstrap
```

The obsolete-macro warnings from autoconf are harmless. The fatal error is only a missing `wxwin.m4`.

`make_cb_app_bundle.sh` reads `_cb_install` next to `configure` and installs a self-contained `/Applications/CodeBlocks.app`. Quit any running Code::Blocks before replacing that app.
