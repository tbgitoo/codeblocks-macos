#!/usr/bin/env bash
set -euo pipefail

# Build a self-contained /Applications/CodeBlocks.app from the local prefix.
# Copies files into the bundle. Does not leave symlinks to the build tree.

PREFIX="$(cd "$(dirname "$0")" && pwd)/_cb_install"
STAGE="/tmp/CodeBlocks-stage"
APP="$STAGE/CodeBlocks.app"
DEST="/Applications/CodeBlocks.app"
FW="$APP/Contents/Frameworks"
MAC="$APP/Contents/MacOS/codeblocks"
RES="$APP/Contents/Resources/share/codeblocks"

require() {
    command -v "$1" >/dev/null 2>&1 || {
        echo "ERROR: required command not found: $1" >&2
        exit 2
    }
}

for cmd in awk codesign find install_name_tool otool python3 rsync; do
    require "$cmd"
done

[[ -x "$PREFIX/bin/codeblocks" ]] || {
    echo "ERROR: Missing executable: $PREFIX/bin/codeblocks" >&2
    exit 2
}
[[ -d "$PREFIX/lib/codeblocks/plugins" ]] || {
    echo "ERROR: Missing plugins: $PREFIX/lib/codeblocks/plugins" >&2
    exit 2
}

echo "==> Creating bundle at $APP"
rm -rf "$STAGE"
mkdir -p "$APP/Contents/MacOS" "$APP/Contents/Resources" "$FW"

cp -v "$PREFIX/bin/codeblocks" "$MAC"
cp -v "$PREFIX/bin/cb_console_runner" "$APP/Contents/MacOS/"
cp -v "$PREFIX/bin/cb_share_config" "$APP/Contents/MacOS/"
chmod 755 "$APP/Contents/MacOS/"*

# Resources live in share/codeblocks. Plugin binaries live in
# lib/codeblocks/plugins. Copy the resources without a plugins entry, then
# copy those binaries into the bundle.
rsync -a --exclude '/plugins' "$PREFIX/share/codeblocks/" "$RES/"
mkdir -p "$RES/plugins"
for plugin in "$PREFIX/lib/codeblocks/plugins/"*.dylib; do
    [[ -e "$plugin" ]] || continue
    cp -v "$plugin" "$RES/plugins/"
done

cp -v "$PREFIX/share/codeblocks/osx_bundle/codeblocks.plist" "$APP/Contents/Info.plist"
cp -v "$PREFIX/share/codeblocks/osx_bundle/icons/"*.icns "$APP/Contents/Resources/"

deps_of() {
    otool -L "$1" | tail -n +2 | awk '{print $1}'
}

is_system_lib() {
    case "$1" in
        /System/*|/usr/lib/*) return 0 ;;
        *) return 1 ;;
    esac
}

real_path() {
    python3 - "$1" <<'PY'
import os, sys
print(os.path.realpath(sys.argv[1]))
PY
}

add_rpath_if_missing() {
    local binary="$1"
    local rpath="$2"
    if ! otool -l "$binary" | grep -A3 LC_RPATH | grep -Fq "path $rpath"; then
        echo "Adding rpath $rpath to $(basename "$binary")"
        install_name_tool -add_rpath "$rpath" "$binary"
    fi
}

resolve_dep() {
    local dep="$1"
    local owner="$2"
    local base candidate
    base="$(basename "$dep")"

    case "$dep" in
        @rpath/*)
            [[ -e "$FW/$base" ]] && { echo "$FW/$base"; return 0; }
            for candidate in \
                "/opt/homebrew/lib/$base" \
                /opt/homebrew/opt/*/lib/"$base" \
                "/usr/local/lib/$base"; do
                [[ -e "$candidate" ]] && { echo "$candidate"; return 0; }
            done
            candidate="$(find /opt/homebrew/Cellar -path "*/lib/$base" -print 2>/dev/null | head -1 || true)"
            [[ -n "$candidate" ]] && { echo "$candidate"; return 0; }
            return 1
            ;;
        @loader_path/*)
            candidate="$(dirname "$owner")/${dep#@loader_path/}"
            [[ -e "$candidate" ]] && { echo "$candidate"; return 0; }
            [[ -e "$FW/$base" ]] && { echo "$FW/$base"; return 0; }
            return 1
            ;;
        @executable_path/*)
            candidate="$APP/Contents/MacOS/${dep#@executable_path/}"
            [[ -e "$candidate" ]] && { echo "$candidate"; return 0; }
            [[ -e "$FW/$base" ]] && { echo "$FW/$base"; return 0; }
            return 1
            ;;
        /*)
            [[ -e "$dep" ]] && { echo "$dep"; return 0; }
            return 1
            ;;
        *)
            [[ -e "$dep" ]] && { echo "$dep"; return 0; }
            [[ -e "$FW/$base" ]] && { echo "$FW/$base"; return 0; }
            return 1
            ;;
    esac
}

bundle_library() {
    local requested="$1"
    local requested_base canonical canonical_base canonical_dst alias_dst

    requested_base="$(basename "$requested")"
    canonical="$(real_path "$requested")"
    canonical_base="$(basename "$canonical")"
    canonical_dst="$FW/$canonical_base"
    alias_dst="$FW/$requested_base"

    if [[ ! -f "$canonical_dst" ]]; then
        echo "Copying canonical dependency: $canonical" >&2
        cp -v "$canonical" "$canonical_dst" >&2
        chmod 755 "$canonical_dst"
    fi

    if [[ "$requested_base" != "$canonical_base" ]]; then
        rm -f "$alias_dst"
        echo "Creating dylib alias: $requested_base -> $canonical_base" >&2
        ln -s "$canonical_base" "$alias_dst"
    fi

    echo "$canonical_dst"
}

rewrite_dep_to_rpath() {
    local binary="$1"
    local old="$2"
    local base
    base="$(basename "$old")"
    [[ "$old" == "@rpath/$base" ]] && return 0
    echo "Rewriting in $(basename "$binary"): $old -> @rpath/$base"
    install_name_tool -change "$old" "@rpath/$base" "$binary"
}

already_seen() {
    local wanted="$1"
    local item
    for item in "${seen[@]:-}"; do
        [[ "$item" == "$wanted" ]] && return 0
    done
    return 1
}

add_rpath_if_missing "$MAC" "@executable_path/../Frameworks"
add_rpath_if_missing "$APP/Contents/MacOS/cb_share_config" "@executable_path/../Frameworks" || true

targets=("$MAC" "$APP/Contents/MacOS/cb_share_config")
if compgen -G "$RES/plugins/*.dylib" >/dev/null; then
    for module in "$RES/plugins"/*.dylib; do
        add_rpath_if_missing "$module" "@loader_path/../../../../Frameworks"
        install_name_tool -id "@loader_path/$(basename "$module")" "$module"
        targets+=("$module")
    done
fi

echo "==> Recursively collecting dylib dependencies"
queue=("${targets[@]}")
seen=()

while ((${#queue[@]})); do
    current="${queue[0]}"
    queue=("${queue[@]:1}")

    current_key="$(real_path "$current")"
    already_seen "$current_key" && continue
    seen+=("$current_key")

    while read -r dep; do
        [[ -n "$dep" ]] || continue
        is_system_lib "$dep" && continue

        if source_path="$(resolve_dep "$dep" "$current")"; then
            if [[ "$source_path" == "$FW/"* ]]; then
                canonical_bundled="$(real_path "$source_path")"
            else
                canonical_bundled="$(bundle_library "$source_path")"
            fi
            rewrite_dep_to_rpath "$current" "$dep"
            queue+=("$canonical_bundled")
        else
            echo "WARNING: Unable to resolve dependency from $(basename "$current"): $dep" >&2
        fi
    done < <(deps_of "$current")
done

echo "==> Normalizing Framework dylib IDs"
for library in "$FW/"*.dylib; do
    [[ -f "$library" && ! -L "$library" ]] || continue
    library_base="$(basename "$library")"
    install_name_tool -id "@rpath/$library_base" "$library"
    add_rpath_if_missing "$library" "@loader_path"
done

echo "==> Checking for paths that still leave the bundle"
outside=0
while IFS= read -r -d '' binary; do
    [[ -f "$binary" && ! -L "$binary" ]] || continue
    while read -r dep; do
        case "$dep" in
            /Users/*|/opt/*|"$PREFIX"/*)
                echo "OUTSIDE: $(basename "$binary") -> $dep" >&2
                outside=1
                ;;
        esac
    done < <(deps_of "$binary")
done < <(find "$APP" -type f \( -name '*.dylib' -o -name 'codeblocks' -o -name 'cb_share_config' -o -name 'cb_console_runner' \) -print0)

if ((outside)); then
    echo "ERROR: Bundle still references files outside the app." >&2
    exit 3
fi

while IFS= read -r link; do
    target="$(readlink "$link")"
    case "$target" in
        /*)
            echo "ERROR: Absolute symlink inside bundle: $link -> $target" >&2
            exit 3
            ;;
    esac
done < <(find "$APP" -type l)

echo "==> Ad-hoc signing"
xattr -cr "$APP" || true
codesign --force --deep --sign - "$APP"
codesign --verify --deep --strict --verbose=2 "$APP"

echo "==> Installing to $DEST"
if [[ -e "$DEST" ]]; then
    rm -rf "$DEST"
fi
if mv "$APP" "$DEST" 2>/dev/null; then
    echo "Installed with user permissions."
else
    osascript -e "do shell script \"rm -rf '$DEST' && mv '$APP' '$DEST' && chown -R root:admin '$DEST'\" with administrator privileges"
fi

echo
echo "Done: $DEST"
echo "Finder launch: open '$DEST'"
