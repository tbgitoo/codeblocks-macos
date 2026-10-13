# Code::Blocks for macOS

Unofficial native macOS build of Code::Blocks for Apple Silicon, on top of trunk r14050.

The IDE is GPL-3 (`COPYING`). The SDK is LGPL-3, as stated in the SDK file headers.

On macOS the default terminal command is an `osascript` that runs whatever is substituted for `$SCRIPT`. These changes fill that in:

- The debugger console runs its `sleep` command inside Terminal, so the debugger can find the window and attach the program's input and output to it.
- A tool launched in a new console is placed in that same `$SCRIPT`, with quotes escaped so an `osascript` tool does not close the Terminal string early.

Build and Run already uses `$SCRIPT`. No custom tool is required.

## Build

Install wxWidgets, then from this directory:

```bash
brew install wxwidgets autoconf automake libtool pkg-config
./bootstrap
./configure --prefix="$(pwd)/_cb_install" --disable-fortran --disable-contrib-plugins CC=clang CXX=clang++
make -j"$(sysctl -n hw.ncpu)"
make install
./make_cb_app_bundle.sh
```

`make_cb_app_bundle.sh` reads `_cb_install` next to `configure` and installs a self-contained `/Applications/CodeBlocks.app`. Quit any running Code::Blocks before replacing that app.
