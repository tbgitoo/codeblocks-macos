# Third-party notices

## Optional MinGW-w64/GCC Toolchain

The Windows installer can optionally download and install a prebuilt
MinGW-w64/GCC compiler toolchain. This download is enabled by default
and can be disabled during installation.

The toolchain is provided by WinLibs and is downloaded directly from
the WinLibs GitHub release. It is not included in or distributed as
part of this project.

### Downloaded toolchain

- **Distribution:** WinLibs standalone MinGW-w64/GCC toolchain
- **GCC:** 14.2.0
- **MinGW-w64:** 12.0.0, UCRT
- **Architecture:** x86-64
- **Thread model:** POSIX
- **Release:** `14.2.0posix-12.0.0-ucrt-r3`
- **Archive:** `winlibs-x86_64-posix-seh-gcc-14.2.0-mingw-w64ucrt-12.0.0-r3.7z`
- **Upstream release:** https://github.com/brechtsanders/winlibs_mingw/releases/tag/14.2.0posix-12.0.0-ucrt-r3
- **SHA-256:** `0475a96b4d687a3b85dd979a982883ed9172c55b2516e953664232d46471096f`

The installer verifies the SHA-256 checksum before using the downloaded
archive.

### Licensing

WinLibs is a collection of independently licensed open-source
components rather than a single software package covered by one
license. The applicable licenses are those of the individual
components.

Major components include:

- **GCC** — GNU General Public License (GPL), with the GCC Runtime
  Library Exception applying to relevant runtime libraries
- **MinGW-w64** — primarily the Zope Public License 2.1, with portions
  under other open-source licenses
- **GDB, GNU Binutils and GNU Make** — GNU General Public License
- Additional utilities are distributed under their respective
  open-source licenses

For details, see:

- WinLibs: https://winlibs.com/
- GCC GPLv3: https://www.gnu.org/licenses/gpl-3.0.html
- GCC Runtime Library Exception:
  https://github.com/gcc-mirror/gcc/blob/releases/gcc-14.2.0/COPYING.RUNTIME
- MinGW-w64 licensing:
  https://github.com/mingw-w64/mingw-w64/blob/v12.0.0/COPYING

The toolchain and its components remain subject to their respective
upstream licenses. These licenses do not apply to this project merely
because the installer offers the toolchain as an optional download.