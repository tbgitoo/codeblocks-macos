# Code::Blocks for Debian and Ubuntu

The Linux release is one `.tar.xz` archive of `.deb` packages, the same layout [codeblocks.org](https://www.codeblocks.org/downloads/binaries/) publishes. Unpack it and install every package in the directory with apt.

Run the blocks below from the root of the source tree, in order. Paste only the fenced blocks. The `.deb` files and the archive are written in the parent directory. Their version is the first entry in `debian/changelog`.

## 1. Packaging tools and wxWidgets

`./bootstrap` needs `wxwin.m4` before the later `apt build-dep` step. This block selects the wxWidgets package for the running Debian or Ubuntu release.

```bash
sudo apt update
sudo apt install build-essential fakeroot devscripts debhelper cdbs \
    automake autoconf libtool pkg-config xz-utils

. /etc/os-release
case "$VERSION_CODENAME" in
    stretch|buster) wx_pkg=libwxgtk3.0-dev ;;
    bullseye|focal|jammy) wx_pkg=libwxgtk3.0-gtk3-dev ;;
    *) wx_pkg=libwxgtk3.2-dev ;;
esac
sudo apt install "$wx_pkg"
```

stretch and buster use wxGTK 3.0 with GTK 2. bullseye, Ubuntu 20.04, and Ubuntu 22.04 use wxGTK 3.0 with GTK 3. Newer releases use wxGTK 3.2 with GTK 3.

## 2. Generate the build files

`./bootstrap` deletes `debian/control`. Generate it again immediately afterwards.

```bash
./bootstrap
./debian/setup_control.sh detect
```

## 3. Install the remaining build dependencies

This reads `Build-Depends` from the `debian/control` file just generated.

```bash
sudo apt build-dep .
```

## 4. Build the .deb packages

`-b` builds binary packages only. `-us -uc` skips signing. The compile includes every contrib plugin.

```bash
DEB_BUILD_OPTIONS=parallel=$(nproc) dpkg-buildpackage -us -uc -b
```

`dpkg-buildpackage` writes one `.deb` per binary package: the IDE, the contrib plugins, the shared libraries, the architecture-independent data, and the plugin-development packages. It also writes debug-symbol packages. Those are not part of the release archive.

## 5. Pack the release archive

```bash
./debian/make-linux-release.sh
```

The script reads the version from `debian/changelog`, the architecture from `dpkg`, and the codename from `/etc/os-release`. On Ubuntu 22.04 amd64, with changelog version `20.03svn12027`, it prints:

```text
../codeblocks_20.03svn12027_amd64_jammy.tar.xz
```

The archive contains one directory of the same name. Debug-symbol packages are omitted. Upload that `.tar.xz` as the Linux release asset.

## 6. Install from the archive

On the machine that should run Code::Blocks, use the archive name printed by `make-linux-release.sh`. This example is the Ubuntu 22.04 file from the previous section.

```bash
tar -xf codeblocks_20.03svn12027_amd64_jammy.tar.xz
cd codeblocks_20.03svn12027_amd64_jammy
sudo apt install ./*.deb
```

`apt` resolves the dependencies between the packages in that directory.

## Notes

`debian/setup_control.sh` can be given a codename instead of `detect`:

```bash
./debian/setup_control.sh jammy
```

Do that only when the generated `debian/control` should target a release other than the one running the build.

`debian/rules` sets debhelper compatibility level 10. Level 13 rejects installed files that no `debian/*.install` list claims. The `codeblocks-headers` package has no `.install` file, so that one package can be empty. The IDE does not need it.
