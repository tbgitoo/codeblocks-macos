# Code signing GDB on macOS

This guide explains how to code-sign the Darwin GDB used by Code::Blocks so debugging works on your Mac. It covers the usual failure symptom, why signing is required, how to create a suitable local certificate, and the exact `codesign` commands to apply.

For building and bundling Code::Blocks itself, see [README.macos.md](README.macos.md).

---

## Symptom

Debugging appears to start, then stalls with no execution arrow in the editor. The Debugger log looks similar to:

```text
Starting debugger: …/gdb -nx -fullname -quiet -args …/your_program
Setting SHELL to ‘/bin/zsh’
done
Setting breakpoints
Debugger name and version: GNU gdb (GDB) …
No stack.
```

If you run the same GDB from a terminal, the underlying error is usually:

```text
Unable to find Mach task port for process-id …: (os/kern) failure (0x5).
 (please check gdb is codesigned - see taskgated(8))
```

**“No stack” here means macOS refused to let GDB control the inferior (the program being debugged)**, not that your program has no call stack. Breakpoints and the editor arrow never appear because the session never reaches a stop in user code.

---

## Why signing is required

On modern macOS, obtaining another process’s Mach task port is restricted. A debugger must be signed with the Hardened Runtime entitlement:

```text
com.apple.security.cs.debugger
```

That entitlement is defined in this repository as [`osx/gdb.entitlements`](osx/gdb.entitlements):

```xml
<?xml version="1.0" encoding="UTF-8"?>
<!DOCTYPE plist PUBLIC "-//Apple//DTD PLIST 1.0//EN" "http://www.apple.com/DTDs/PropertyList-1.0.dtd">
<plist version="1.0">
<dict>
	<key>com.apple.security.cs.debugger</key>
	<true/>
</dict>
</plist>
```

`make_cb_app_bundle.sh` signs the bundled `Contents/Resources/debugger/bin/gdb` with that entitlement. The **identity** used for the signature still matters:

| Identity | Typical use | Debugging on the development Mac | Debugging after a downloaded `.dmg` |
|----------|-------------|-------------------------|-------------------------------------|
| Ad-hoc (`codesign --sign -`) | Local bundle script default | Often works on the machine that signed the binary | **Not reliable** — Gatekeeper / `taskgated` may still refuse the task port |
| Self-signed cert (e.g. `gdb-cert`) trusted for Code Signing | Local development | Works once the cert is trusted in Keychain | Only on Macs that trust *your* cert (not for public releases) |
| Apple Developer ID Application (+ notarization) | Public distribution | Works | Intended path for release DMGs |

This document focuses on a **successful local setup** with a self-signed code-signing certificate (the usual fix when ad-hoc signing is not enough on your machine, or when you want a stable identity for the sister-repo `staging/bin/gdb` and the app bundle).

---

## Create a local code-signing certificate

Use **Keychain Access** (or equivalent) to create a certificate that macOS will accept for code signing.

### Recommended properties

Match these properties (recommended name `gdb-cert`; this matches the entitlement file, see below):

| Property | Recommended value |
|----------|-------------------|
| Common Name | `gdb-cert` (self-signed: issuer = subject) |
| Key type / size | RSA, **2048-bit** |
| Signature algorithm | SHA-256 with RSA Encryption |
| Key Usage (critical) | **Digital Signature** |
| Extended Key Usage (critical) | **Code Signing** (`1.3.6.1.5.5.7.3.3`) |
| Keychain | Prefer the **System** keychain so all users/tools on the Mac see it |
| Trust → Code Signing | **Always Trust** (custom trust settings) |

Validity is typically one year (or longer); renew before expiry.

### Steps in Keychain Access

1. Open **Keychain Access**.
2. Create a certificate for code signing (Certificate Assistant → Create a Certificate…, or import one you already use for GDB). Choose **Code Signing** as the type / purpose so Extended Key Usage includes Code Signing.
3. Store the certificate **and** its private key in the **System** keychain (you will be prompted for an administrator password).
4. Double-click the certificate → expand **Trust**:
   - **When using this certificate:** Use Custom Settings
   - **Code Signing:** Always Trust  
   Leave other trust rows unchanged unless you know you need them.
5. Close the window and authenticate when asked so the trust change is saved.
6. Confirm you see both:
   - `gdb-cert` (certificate)
   - `gdb-cert` (private key, RSA 2048-bit)

If `codesign` later cannot find the identity, unlock the System keychain, confirm the private key is present, and that Code Signing trust is Always Trust.

List identities from a terminal:

```bash
security find-identity -v -p codesigning
```

You should see a line containing `gdb-cert` (or whatever Common Name you chose).

---

## Sign GDB (commands)

Replace paths if your layout differs. The entitlement file lives in this repo:

```bash
ENTITLEMENTS="/path/to/codeblocks-macos/osx/gdb.entitlements"
IDENTITY="gdb-cert"   # must match the certificate Common Name
```

### 1. Sister-repo staging GDB (optional but useful)

If you also run GDB from the companion `binutils-gdb` staging tree:

```bash
GDB="../binutils-gdb/staging/bin/gdb"   # relative to codeblocks-macos, or use an absolute path

codesign --force --sign "$IDENTITY" \
  --entitlements "$ENTITLEMENTS" \
  "$GDB"

codesign --verify --verbose=2 "$GDB"
codesign -d --entitlements - "$GDB"
```

The entitlements dump must show `com.apple.security.cs.debugger` = true.

### 2. GDB inside `CodeBlocks.app`

After `./make_cb_app_bundle.sh` (or whenever the bundle was rebuilt), re-sign the nested debugger with your certificate. Ad-hoc signing from the bundle script is not enough if your Mac requires a trusted identity:

```bash
APP="/Applications/CodeBlocks.app"
GDB="$APP/Contents/Resources/debugger/bin/gdb"

# Runtime dylibs next to gdb (no debugger entitlement required)
for lib in "$APP/Contents/Resources/debugger/lib/"*.dylib; do
  [[ -f "$lib" ]] || continue
  codesign --force --sign "$IDENTITY" "$lib"
done

codesign --force --sign "$IDENTITY" \
  --entitlements "$ENTITLEMENTS" \
  "$GDB"

# Optional: re-sign the app bundle itself with the same identity
codesign --force --deep --sign "$IDENTITY" "$APP"

# Deep signing can resign nested code without entitlements — apply gdb again
codesign --force --sign "$IDENTITY" \
  --entitlements "$ENTITLEMENTS" \
  "$GDB"

codesign --verify --verbose=2 "$GDB"
codesign -d --entitlements - "$GDB"
```

Again, confirm `com.apple.security.cs.debugger` is present.

### 3. Quick functional check

```bash
GDB="/Applications/CodeBlocks.app/Contents/Resources/debugger/bin/gdb"
# Use any small Debug build of your own:
PROG="/path/to/your/Debug/program"

"$GDB" -nx -batch \
  -ex 'set startup-with-shell off' \
  -ex "file $PROG" \
  -ex 'break main' \
  -ex 'run' \
  -ex 'bt' \
  -ex 'quit'
```

Success looks like a hit on `main` and a backtrace. Failure still shows the Mach task port / `taskgated` message.

Then debug the same program from Code::Blocks (Debugger executable pointing at the bundled or staging `gdb` you signed).

---

## What `make_cb_app_bundle.sh` already does

The bundle script:

1. Copies `../binutils-gdb/staging/bin/gdb` and `share/gdb/` into `Contents/Resources/debugger/`.
2. Vendors license-permitted dylibs into `debugger/lib/` and rewrites install names.
3. Ad-hoc signs those Mach-Os and attaches `osx/gdb.entitlements` to `debugger/bin/gdb`.
4. Re-applies the entitlement after `codesign --deep` on the app.

That is enough on many developer machines. If you still see **No stack** / Mach task port errors after a fresh bundle, run the **Sign GDB** commands above with your `gdb-cert` (or other trusted code-signing identity).

---

## Common pitfalls

- **Signing without the entitlement** — a valid signature alone is not enough; `com.apple.security.cs.debugger` must be embedded.
- **`--deep` after signing `gdb`** — can replace the nested signature and drop entitlements. Always sign `gdb` with `--entitlements` **after** any `--deep` on the app.
- **`install_name_tool`** — invalidates signatures; sign again after rewriting dylib paths.
- **Wrong identity name** — `codesign -s gdb-cert` must match an identity from `security find-identity -v -p codesigning`.
- **Trust not set** — without Code Signing = Always Trust, macOS may reject the signature for debugging even though `codesign --verify` succeeds.
- **Certificate only in login keychain** — some tools run in contexts that do not see it; System keychain is more reliable for this use case.
- **Downloaded `.dmg` on another Mac** — a self-signed `gdb-cert` from your machine is not trusted there. Public releases need Developer ID signing and notarization; until then, document that debugging may require a locally signed GDB (this guide) or the companion port signed on that Mac.

---

## Related files

| Path | Role |
|------|------|
| [`osx/gdb.entitlements`](osx/gdb.entitlements) | Entitlement plist for GDB |
| [`make_cb_app_bundle.sh`](make_cb_app_bundle.sh) | Bundles GDB and ad-hoc signs with the entitlement |
| [`README.macos.md`](README.macos.md) | Build, install, and debugger overview |
