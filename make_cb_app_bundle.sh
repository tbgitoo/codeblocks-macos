#!/usr/bin/env bash
set -euo pipefail

# Build a self-contained /Applications/CodeBlocks.app from the local prefix.
# Copies files into the bundle. Does not leave symlinks to the build tree.

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
PREFIX="$SCRIPT_DIR/_cb_install"
STAGE="/tmp/CodeBlocks-stage"
APP="$STAGE/CodeBlocks.app"
DEST="/Applications/CodeBlocks.app"
FW="$APP/Contents/Frameworks"
MAC="$APP/Contents/MacOS/codeblocks"
RES="$APP/Contents/Resources/share/codeblocks"
# Sister-repo staging prefix (override with CB_BUNDLE_GDB_PREFIX). Never hardcode a home path.
GDB_PREFIX="${CB_BUNDLE_GDB_PREFIX:-$SCRIPT_DIR/../binutils-gdb/staging}"
DBG="$APP/Contents/Resources/debugger"

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
cp -v "$PREFIX/bin/cb_debug_console_hold" "$APP/Contents/MacOS/"
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

# --- Bundled Darwin GDB (Resources/debugger) ---------------------------------
if [[ ! -d "$GDB_PREFIX" ]]; then
    echo "ERROR: GDB staging prefix not found: $GDB_PREFIX" >&2
    echo "Build the sister binutils-gdb staging tree, or set CB_BUNDLE_GDB_PREFIX." >&2
    exit 2
fi
GDB_PREFIX="$(cd "$GDB_PREFIX" && pwd)"
GDB_BIN_SRC="$GDB_PREFIX/bin/gdb"
GDB_SHARE_SRC="$GDB_PREFIX/share/gdb"
[[ -x "$GDB_BIN_SRC" ]] || {
    echo "ERROR: Missing GDB executable: $GDB_BIN_SRC" >&2
    exit 2
}
[[ -d "$GDB_SHARE_SRC" ]] || {
    echo "ERROR: Missing GDB data directory: $GDB_SHARE_SRC" >&2
    exit 2
}

echo "==> Bundling GDB from $GDB_PREFIX"
mkdir -p "$DBG/bin" "$DBG/share" "$DBG/lib" "$DBG/licenses"
cp -v "$GDB_BIN_SRC" "$DBG/bin/gdb"
chmod 755 "$DBG/bin/gdb"
rsync -a "$GDB_SHARE_SRC/" "$DBG/share/gdb/"

# License-permitted runtime dylibs currently linked from Homebrew (zstd BSD/MIT,
# mpfr/gmp LGPL-or-later). Vendor under debugger/lib and rewrite load paths.
copy_gdb_license_file() {
    local dest_name="$1"
    shift
    local candidate
    for candidate in "$@"; do
        if [[ -f "$candidate" ]]; then
            cp -v "$candidate" "$DBG/licenses/$dest_name"
            return 0
        fi
    done
    echo "WARNING: No license file found for $dest_name" >&2
}

GDB_REPO_ROOT="$(cd "$GDB_PREFIX/.." && pwd)"
copy_gdb_license_file "gdb-COPYING" \
    "$GDB_REPO_ROOT/COPYING" "$GDB_REPO_ROOT/gdb/COPYING"
copy_gdb_license_file "zstd-LICENSE" \
    "$(brew --prefix zstd 2>/dev/null)/LICENSE" \
    /opt/homebrew/opt/zstd/LICENSE
copy_gdb_license_file "mpfr-COPYING.LESSER" \
    "$(brew --prefix mpfr 2>/dev/null)/COPYING.LESSER" \
    /opt/homebrew/opt/mpfr/COPYING.LESSER
copy_gdb_license_file "gmp-README-license" \
    "$(brew --prefix gmp 2>/dev/null)/README" \
    /opt/homebrew/opt/gmp/README

cat >"$DBG/licenses/README.txt" <<'EOF'
Bundled debugger components inside CodeBlocks.app/Contents/Resources/debugger/

- bin/gdb and share/gdb come from the companion binutils-gdb Darwin port (GPL).
- lib/ may contain redistributable shared libraries required by that gdb build.
  Current permitted set: zstd (BSD/MIT dual), mpfr (LGPL-3.0-or-later),
  gmp (LGPL-3.0-or-later OR GPL-2.0-or-later). LGPL libraries remain replaceable
  shared objects under lib/.
EOF

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

# Vendor non-system dylibs required by bundled gdb into Resources/debugger/lib.
echo "==> Vendoring GDB runtime dylibs into Resources/debugger/lib"
gdb_queue=("$DBG/bin/gdb")
gdb_seen=()
while ((${#gdb_queue[@]})); do
    gdb_current="${gdb_queue[0]}"
    gdb_queue=("${gdb_queue[@]:1}")
    gdb_key="$(real_path "$gdb_current")"
    already=0
    for item in "${gdb_seen[@]:-}"; do
        [[ "$item" == "$gdb_key" ]] && { already=1; break; }
    done
    ((already)) && continue
    gdb_seen+=("$gdb_key")

    while read -r dep; do
        [[ -n "$dep" ]] || continue
        is_system_lib "$dep" && continue
        case "$dep" in
            @loader_path/*|@rpath/*|@executable_path/*)
                # Already rewritten or relative; still enqueue local lib if present.
                base="$(basename "$dep")"
                if [[ -f "$DBG/lib/$base" ]]; then
                    gdb_queue+=("$DBG/lib/$base")
                fi
                continue
                ;;
        esac
        if ! source_path="$(resolve_dep "$dep" "$gdb_current")"; then
            echo "ERROR: Unable to resolve GDB dependency: $dep" >&2
            exit 3
        fi
        base="$(basename "$dep")"
        dest="$DBG/lib/$base"
        if [[ ! -f "$dest" ]]; then
            canonical="$(real_path "$source_path")"
            echo "Copying GDB dependency: $canonical -> $dest"
            cp -v "$canonical" "$dest"
            chmod 755 "$dest"
            install_name_tool -id "@loader_path/$base" "$dest"
        fi
        # From gdb in bin/: libs live in ../lib. From a lib dylib: same directory.
        if [[ "$gdb_current" == "$DBG/bin/gdb" ]]; then
            new_ref="@loader_path/../lib/$base"
        else
            new_ref="@loader_path/$base"
        fi
        if [[ "$dep" != "$new_ref" ]]; then
            echo "Rewriting in $(basename "$gdb_current"): $dep -> $new_ref"
            install_name_tool -change "$dep" "$new_ref" "$gdb_current"
        fi
        gdb_queue+=("$dest")
    done < <(deps_of "$gdb_current")
done

# Fail if gdb still references Homebrew / user prefixes.
gdb_outside=0
while read -r dep; do
    case "$dep" in
        /Users/*|/opt/*)
            echo "OUTSIDE (gdb): $dep" >&2
            gdb_outside=1
            ;;
    esac
done < <(deps_of "$DBG/bin/gdb")
for library in "$DBG/lib/"*.dylib; do
    [[ -f "$library" ]] || continue
    while read -r dep; do
        case "$dep" in
            /Users/*|/opt/*)
                echo "OUTSIDE ($(basename "$library")): $dep" >&2
                gdb_outside=1
                ;;
        esac
    done < <(deps_of "$library")
done
if ((gdb_outside)); then
    echo "ERROR: Bundled GDB still references files outside the app." >&2
    exit 3
fi

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
done < <(find "$APP" -type f \( -name '*.dylib' -o -name 'codeblocks' -o -name 'cb_share_config' -o -name 'cb_console_runner' -o -name 'cb_debug_console_hold' -o -name 'gdb' \) -print0)

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
GDB_ENTITLEMENTS="$SCRIPT_DIR/osx/gdb.entitlements"
[[ -f "$GDB_ENTITLEMENTS" ]] || {
    echo "ERROR: Missing GDB entitlements file: $GDB_ENTITLEMENTS" >&2
    exit 2
}
# Sign nested debugger Mach-Os first. install_name_tool invalidates any prior
# signature, and codesign --deep alone is not reliable for Resources/debugger.
# GDB must keep com.apple.security.cs.debugger or taskgated refuses debugging.
if compgen -G "$DBG/lib/"*.dylib >/dev/null; then
    for library in "$DBG/lib/"*.dylib; do
        [[ -f "$library" && ! -L "$library" ]] || continue
        codesign --force --sign - "$library"
    done
fi
codesign --force --sign - --entitlements "$GDB_ENTITLEMENTS" "$DBG/bin/gdb"
codesign --force --deep --sign - "$APP"
# Re-apply debugger entitlement after --deep (which may resign nested code).
codesign --force --sign - --entitlements "$GDB_ENTITLEMENTS" "$DBG/bin/gdb"
codesign --verify --deep --strict --verbose=2 "$APP"
codesign --verify --verbose=2 "$DBG/bin/gdb"
if ! codesign -d --entitlements - "$DBG/bin/gdb" 2>&1 | grep -Fq "com.apple.security.cs.debugger"; then
    echo "ERROR: Bundled GDB is missing com.apple.security.cs.debugger entitlement." >&2
    exit 3
fi
"$DBG/bin/gdb" -nx -batch -ex 'show data-directory' >/dev/null
echo "Bundled GDB smoke check OK (debugger entitlement present)."

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
