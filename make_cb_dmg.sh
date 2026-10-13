#!/usr/bin/env bash
set -euo pipefail

# Build a drag-to-Applications disk image from CodeBlocks.app.
# Expects a finished app (for example from ./make_cb_app_bundle.sh).

ROOT="$(cd "$(dirname "$0")" && pwd)"
SRC="${CODEBLOCKS_APP:-/Applications/CodeBlocks.app}"
STAGE="/tmp/CodeBlocks-dmg-stage"
VOLNAME="CodeBlocks"
ARCH="$(uname -m)"
OUT="${CODEBLOCKS_DMG:-$ROOT/CodeBlocks-macos-${ARCH}.dmg}"

require() {
    command -v "$1" >/dev/null 2>&1 || {
        echo "ERROR: required command not found: $1" >&2
        exit 2
    }
}

for cmd in ditto hdiutil ln rm mkdir; do
    require "$cmd"
done

[[ -d "$SRC" && -x "$SRC/Contents/MacOS/codeblocks" ]] || {
    echo "ERROR: Missing app bundle: $SRC" >&2
    echo "Build one first with ./make_cb_app_bundle.sh" >&2
    exit 2
}

echo "==> Staging DMG contents from $SRC"
rm -rf "$STAGE"
mkdir -p "$STAGE"
ditto "$SRC" "$STAGE/CodeBlocks.app"
ln -s /Applications "$STAGE/Applications"

echo "==> Creating $OUT"
rm -f "$OUT"
hdiutil create \
    -volname "$VOLNAME" \
    -srcfolder "$STAGE" \
    -ov \
    -format UDZO \
    "$OUT"

rm -rf "$STAGE"

echo
echo "Done: $OUT"
echo "Open: open $(printf %q "$OUT")"
echo "Install: open the disk image, then drag CodeBlocks.app onto Applications."
