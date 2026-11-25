#!/bin/sh
# Pack the .deb files written by dpkg-buildpackage into one .tar.xz archive.
#
# This is the Linux release layout published by codeblocks.org: one compressed
# archive whose top-level directory contains the .deb packages. After unpacking,
# install them with: sudo apt install ./*.deb
#
# Run this from anywhere. dpkg-buildpackage must already have written the .deb
# files in the parent of the source tree. Debug-symbol packages are left out;
# they are large and are not part of the installable release archive.
#
# The archive is written next to the .deb files:
#   codeblocks_<changelog-version>_<arch>_<codename>.tar.xz

set -e

root=$(CDPATH= cd -- "$(dirname "$0")/.." && pwd)
parent=$(dirname "$root")

if ! command -v xz >/dev/null 2>&1; then
    echo "xz is required. Install it with: sudo apt install xz-utils" >&2
    exit 1
fi

version=$(sed -n '1s/^codeblocks (\([^)]*\)).*/\1/p' "$root/debian/changelog")
if [ -z "$version" ]; then
    echo "Could not read the package version from debian/changelog." >&2
    exit 1
fi

arch=$(dpkg --print-architecture)
os_release=/etc/os-release
if [ ! -e "$os_release" ]; then
    os_release=/usr/lib/os-release
fi
# shellcheck disable=SC1090
. "$os_release"
distro=${VERSION_CODENAME:-$ID}
if [ -z "$distro" ]; then
    echo "Could not read VERSION_CODENAME from $os_release." >&2
    exit 1
fi

name="codeblocks_${version}_${arch}_${distro}"
stage="$parent/$name"
archive="$parent/${name}.tar.xz"

# Collect every binary package for this version except debug symbols.
# dpkg-buildpackage names them <package>_<version>_<arch>.deb.
set -- "$parent"/*_"${version}"_*.deb
if [ ! -e "$1" ]; then
    echo "No .deb files for version $version in $parent" >&2
    echo "Run dpkg-buildpackage from the source tree first." >&2
    exit 1
fi

debs=""
for deb in "$@"; do
    base=$(basename "$deb")
    case "$base" in
        *-dbg_*.deb|*-dbgsym_*.deb)
            continue
            ;;
    esac
    debs="$debs $deb"
done

if [ -z "$debs" ]; then
    echo "Only debug-symbol packages were found for version $version." >&2
    exit 1
fi

missing=""
for pkg in codeblocks codeblocks-common libcodeblocks0 \
           codeblocks-contrib codeblocks-contrib-common \
           codeblocks-libwxcontrib0 libwxsmithlib0; do
    found=""
    for deb in $debs; do
        case "$(basename "$deb")" in
            "${pkg}_${version}"_*.deb)
                found=1
                break
                ;;
        esac
    done
    if [ -z "$found" ]; then
        missing="$missing $pkg"
    fi
done
if [ -n "$missing" ]; then
    echo "Missing packages required for the release archive:$missing" >&2
    exit 1
fi

rm -rf "$stage"
mkdir "$stage"
# shellcheck disable=SC2086
cp -a $debs "$stage/"

tar -C "$parent" -cJf "$archive" "$name"
rm -rf "$stage"

echo "Release archive:"
echo "  $archive"
echo "Install from the unpacked directory with:"
echo "  sudo apt install ./*.deb"
