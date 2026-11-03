# Manual OGRE v14.6.3 Build Recipe on macOS

## Purpose

This is the reference manual procedure for obtaining, configuring, building, installing, and validating official OGRE v14.6.3 on macOS while staying as close as possible to the Code::Blocks wizard workflow.

The methodological rule is:

```text
clean shell
    |
    v
clone exact v14.6.3
    |
    v
configure the complete OGRE tree
    |
    v
build the default/all target
    |
    v
verify OgreAssimpConverter and Codec_Assimp
    |
    v
install the complete tree
    |
    v
validate installed runtime layout
    |
    v
only then test assets
```

Do not treat `OgreAssimpConverter` as an isolated build target. A targeted converter-only build can leave `Codec_Assimp.framework` as an incomplete framework skeleton without its versioned binary.

Official references:

- [OGRE guide to building from source](https://ogrecave.github.io/ogre/api/latest/building-ogre.html)
- [OGRE v14.6.3 release](https://github.com/OGRECave/ogre/releases/tag/v14.6.3)
- [OGRE project setup guide](https://ogrecave.github.io/ogre/api/latest/setup.html)

## 1. Clean the shell environment

Previous OGRE experiments may have exported loader variables pointing to another SDK. Remove them before starting.

```bash
unset DYLD_FRAMEWORK_PATH
unset DYLD_LIBRARY_PATH

env | grep '^DYLD_' || echo "No DYLD variables exported"
```

Ordinary unexported shell variables are harmless unless explicitly reused in a command. Exported `DYLD_*` variables are inherited by executables and can redirect macOS framework loading.

## 2. Establish the wizard-side Homebrew prerequisites

The current Code::Blocks wizard validates these Homebrew formulas:

```text
cmake
pkgconf
sdl2
assimp
```

For a machine intended to reproduce the wizard environment:

```bash
brew install cmake pkgconf sdl2 assimp
```

Confirm them:

```bash
cmake --version
pkgconf --version
brew --prefix sdl2
brew --prefix assimp
```

Important: do not add `-DOGRE_BUILD_DEPENDENCIES=OFF` to the CMake configure command. The Code::Blocks native CMake helper does not pass that option. Adding it changes OGRE dependency discovery and can disable the entire Tools subtree when dependencies such as pugixml are not externally available.

## 3. Define clean source, build, and install locations

Use separate source, build, and installed-SDK directories.

```bash
SOURCE="$HOME/git/ogre-manual-14.6.3"
BUILD="$HOME/git/ogre-manual-14.6.3-build"
SDK="$HOME/git/ogre-manual-14.6.3-sdk"

rm -rf "$SOURCE" "$BUILD" "$SDK"
```

## 4. Clone exact official OGRE v14.6.3

No Git submodules are required for this workflow.

```bash
git clone \
    --branch v14.6.3 \
    --depth 1 \
    https://github.com/OGRECave/ogre.git \
    "$SOURCE"
```

Verify the exact source state:

```bash
cd "$SOURCE"

git rev-parse HEAD
git describe --tags --exact-match HEAD
git status --short
```

Expected commit and tag:

```text
e3ee8850ecf5e0038b6d850ec4c865c2039342d5
v14.6.3
```

`git status --short` should produce no output.

## 5. Configure the complete OGRE build

The Code::Blocks helper normally supplies only source, build directory, build type, and final install prefix. For macOS, the vanilla OGRE v14.6.3 install was experimentally shown to give installed command-line tools an insufficient RPATH of `<prefix>/lib` even though Release frameworks are installed under `<prefix>/lib/macosx/Release`.

Therefore this reference recipe adds one explicit runtime-layout correction at configure time:

```bash
cmake \
    -S "$SOURCE" \
    -B "$BUILD" \
    -DCMAKE_BUILD_TYPE=Release \
    -DCMAKE_INSTALL_PREFIX="$SDK" \
    -DCMAKE_INSTALL_RPATH="$SDK/lib/macosx/Release" \
    -DOGRE_PLUGINS_PATH="lib/macosx/Release"
```

This preserves OGRE's normal dependency behaviour. It does not disable OGRE dependency handling and it does not patch OGRE source.

The final SDK destination is fixed at configure time, as required for an absolute install RPATH.

### 5.1 Why the explicit install RPATH is present

Exact pristine v14.6.3 was observed to generate these two policies:

```text
framework install destination:
    <prefix>/lib/macosx/Release

vanilla installed tool RPATH:
    <prefix>/lib
```

`OgreRTShaderSystem.framework` identifies itself through `@rpath`, so `<prefix>/lib` alone cannot locate the installed Release framework.

Adding:

```text
CMAKE_INSTALL_RPATH=<prefix>/lib/macosx/Release
```

produced an installed `OgreAssimpConverter` with both:

```text
<prefix>/lib/macosx/Release
<prefix>/lib
```

as `LC_RPATH` entries. This was sufficient to get the executable past its directly linked OGRE framework loading stage.

## 6. Record the effective CMake configuration

Before compiling, record the important cache state:

```bash
grep -E \
    '^(CMAKE_BUILD_TYPE|CMAKE_INSTALL_PREFIX|CMAKE_INSTALL_RPATH|OGRE_BUILD_DEPENDENCIES|OGRE_BUILD_PLUGIN_ASSIMP|OGRE_BUILD_TOOLS|OGRE_LIB_DIRECTORY):' \
    "$BUILD/CMakeCache.txt"
```

Also retain dependency-related entries:

```bash
grep -i 'assimp' "$BUILD/CMakeCache.txt"
grep -i 'pugixml' "$BUILD/CMakeCache.txt"
```

Check that the converter and codec targets were actually generated:

```bash
cmake --build "$BUILD" --target help \
    | grep -i -E 'assimp|converter'
```

The relevant generated targets should include:

```text
Codec_Assimp
OgreAssimpConverter
```

If `OgreAssimpConverter` is absent, do not attempt a targeted build. Diagnose configuration first.

## 7. Build the whole tree

This is the central rule of the recipe and mirrors the Code::Blocks native helper.

```bash
cmake \
    --build "$BUILD" \
    --parallel 8
```

Do not replace the complete build with only:

```bash
cmake --build "$BUILD" --target OgreAssimpConverter --parallel 8
```

A converter-only diagnostic build previously produced converter artefacts without guaranteeing that the dynamically loaded `Codec_Assimp.framework` contained its actual versioned binary.

## 8. Verify build-tree artefacts

After the complete build:

```bash
test -x "$BUILD/bin/OgreAssimpConverter" \
    && echo "OgreAssimpConverter: OK" \
    || echo "OgreAssimpConverter: MISSING"

test -e "$BUILD/lib/macosx/Codec_Assimp.framework/Codec_Assimp" \
    && echo "Codec_Assimp: OK" \
    || echo "Codec_Assimp: MISSING"

test -e "$BUILD/lib/macosx/Codec_Assimp.framework/Versions/14.6/Codec_Assimp" \
    && echo "Versioned Codec_Assimp binary: OK" \
    || echo "Versioned Codec_Assimp binary: MISSING"
```

Do not modify the build-tree `plugins.cfg` as part of the canonical workflow.

## 9. Install the complete build

```bash
cmake --install "$BUILD"
```

Do not supply a replacement prefix during installation. The final prefix and install RPATH were fixed during configuration.

## 10. Verify installed artefacts

```bash
find "$SDK" \
    \( -name 'OgreAssimpConverter' \
    -o -name 'Codec_Assimp.framework' \
    -o -name 'Ogre.framework' \
    -o -name 'OgreRTShaderSystem.framework' \
    -o -name 'plugins.cfg' \) \
    -print
```

Expected important locations include:

```text
$SDK/bin/macosx/OgreAssimpConverter
$SDK/lib/macosx/Release/Ogre.framework
$SDK/lib/macosx/Release/OgreRTShaderSystem.framework
$SDK/lib/macosx/Release/Codec_Assimp.framework
$SDK/bin/plugins.cfg
```

## 11. Validate direct framework loading

Set the installed converter path:

```bash
CONVERTER="$SDK/bin/macosx/OgreAssimpConverter"
```

Inspect direct dependencies:

```bash
otool -L "$CONVERTER"
```

Inspect all embedded runtime search paths:

```bash
otool -l "$CONVERTER" | awk '
    $1 == "cmd" && $2 == "LC_RPATH" { rpath=1; next }
    rpath && $1 == "path" { print $2; rpath=0 }
'
```

For this Release recipe, the result must include:

```text
$SDK/lib/macosx/Release
```

OGRE may also add:

```text
$SDK/lib
```

The additional `$SDK/lib` entry is not itself a problem. The essential requirement is that the actual Release framework directory is present.

## 12. Validate plugin discovery separately

Inspect the installed plugin configuration without editing it:

```bash
cat "$SDK/bin/plugins.cfg"
```

Then locate the actual installed Assimp plugin:

```bash
find "$SDK/lib" \
    -name 'Codec_Assimp.framework' \
    -print
```

Current observed v14.6.3 state:

```text
plugins.cfg PluginFolder:
    <prefix>/lib/OGRE

actual Codec_Assimp.framework:
    <prefix>/lib/macosx/Release/Codec_Assimp.framework
```

With the direct framework RPATH corrected, `OgreAssimpConverter` starts but subsequently fails while trying to load `Codec_Assimp` from the `PluginFolder` specified by `plugins.cfg`.

This is a separate plugin-discovery/configuration issue. Do not treat it as an OBJ/MTL conversion failure.

Do not permanently edit `plugins.cfg` yet. First determine why the installed configuration points to `<prefix>/lib/OGRE` while the macOS framework plugin is installed under `<prefix>/lib/macosx/Release`.

## 13. Runtime gate before asset testing

Do not start OBJ/MTL regression testing until all of the following are true without temporary environment variables or hand-edited configuration:

```text
OgreAssimpConverter launches
required directly linked OGRE frameworks resolve
installed plugins.cfg points to a usable plugin location
Codec_Assimp loads
Assimp reports its version and supported formats
```

If failure occurs before Assimp reports its version, classify the result as runtime/bootstrap failure, not asset-conversion failure.

## 14. Asset-conversion acceptance test

Only after the runtime gate passes should conversion fixtures be tested.

### 14.1 Flat fixture

```bash
MATRIX="/Users/thomasbraschler/git/ogre-charizard-test/test_matrix"
FIXTURE="$MATRIX/fixtures/flat"
TEST="$MATRIX/results/v14.6.3-manual-pristine/flat"

rm -rf "$TEST"
mkdir -p "$TEST"
cp -R "$FIXTURE/." "$TEST/"

cd "$SDK/bin"
rm -f OgreAssimp.log

"$CONVERTER" \
    "$TEST/pm0006_00_00.obj" \
    2>&1 | tee "$TEST/conversion.txt"

cp OgreAssimp.log "$TEST/OgreAssimp.log"
```

Inspect:

```bash
wc -c \
    "$TEST/pm0006_00_00.mesh" \
    "$TEST/pm0006_00_00.material"

grep -n -E '^(material|[[:space:]]*texture )' \
    "$TEST/pm0006_00_00.material"
```

### 14.2 Boundary-respecting child-material fixture

```bash
TEST="$MATRIX/results/v14.6.3-manual-pristine/child-material"

rm -rf "$TEST"
mkdir -p "$TEST"
cp -R "$MATRIX/fixtures/child-material/." "$TEST/"

cd "$SDK/bin"
rm -f OgreAssimp.log

"$CONVERTER" \
    "$TEST/pm0006_00_00.obj" \
    2>&1 | tee "$TEST/conversion.txt"

cp OgreAssimp.log "$TEST/OgreAssimp.log"
```

Inspect with the same `wc` and `grep` commands as the flat fixture.

For the existing private Charizard regression fixture, the historical fingerprints are:

```text
material-resolution failure:
    approximately 819 KB .mesh
    116-byte .material
    one fallback material
    no retained diffuse texture references

material-resolution success:
    approximately 819 KB .mesh
    640-byte .material
    four effective materials
    four retained diffuse texture references
```

These byte sizes are fingerprints for this private fixture only. They are not general OGRE acceptance thresholds.

## 15. Reference workflow for the Code::Blocks wizard

The current Code::Blocks native CMake helper performs the equivalent of:

```bash
cmake \
    -S "$SOURCE" \
    -B "$BUILD" \
    -DCMAKE_BUILD_TYPE=Release \
    -DCMAKE_INSTALL_PREFIX="$SDK"

cmake \
    --build "$BUILD" \
    --parallel 8

cmake --install "$BUILD"
```

The helper does not currently pass `OGRE_BUILD_DEPENDENCIES=OFF` and does not build only the converter target.

The experimentally supported candidate improvement for macOS is to add the final Release framework directory as `CMAKE_INSTALL_RPATH` during configuration:

```text
<prefix>/lib/macosx/Release
```

The plugin configuration problem must be understood separately before changing production wizard behaviour.

Target workflow:

```text
validate prerequisites
        |
        v
clone exact official tag
        |
        v
configure complete OGRE tree
with final prefix and valid macOS install RPATH
        |
        v
build default/all target
        |
        v
verify converter and codec binaries
        |
        v
install complete tree
        |
        v
validate direct framework loading
        |
        v
validate plugin discovery
        |
        v
only then run asset conversion
```

## 16. Failure classification

Use this classification to keep independent problems separate:

```text
converter fails before main/runtime startup
    -> direct framework/RPATH problem

converter starts but Codec_Assimp does not load
    -> plugins.cfg/plugin discovery problem

Assimp loads but MTL is not found
    -> Assimp/OGRE resource-resolution problem

mesh and material convert but rendering is wrong
    -> consumer resource setup, material, camera, or rendering problem
```

## 17. Reproducibility record

For every reference build retain:

```text
exact Git commit and exact tag
full configure command
CMakeCache.txt
complete build command
install command
OgreAssimpConverter otool -L output
OgreAssimpConverter LC_RPATH output
Codec_Assimp otool output
installed plugins.cfg
actual installed plugin/framework locations
conversion log
result manifest
```

This prevents later results from being attributed to a source change when the actual difference was build scope, CMake configuration, plugin discovery, runtime paths, install layout, or exported shell variables.

