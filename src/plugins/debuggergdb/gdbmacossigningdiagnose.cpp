/*
 * This file is part of the Code::Blocks IDE and licensed under the GNU General Public License, version 3
 * http://www.gnu.org/licenses/gpl-3.0.html
 *
 * v2.3 M2: macOS bundled-GDB signing / task-port diagnosis.
 * v2.3 M3: Tier 1 ad-hoc + cs.debugger restoration (bundled only).
 * v2.3 M4: Tier 2 local trusted identity + force injection.
 */

#include "sdk.h"
#ifndef CB_PRECOMP
    #include <wx/dir.h>
    #include <wx/filename.h>
    #include <wx/ffile.h>
    #include <wx/utils.h>
    #include "configmanager.h"
    #include "prep.h"
#endif

#include "gdbmacossigningdiagnose.h"

#ifdef __WXMAC__
#include <Security/SecCertificate.h>
#include <Security/SecPolicy.h>
#include <Security/SecTrustSettings.h>
#include <dlfcn.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <unistd.h>
#endif

namespace
{

const char *kTier2IdentityCn = "codeblocks-gdb";

wxString NormalizePath(const wxString &path)
{
    if (path.empty())
        return wxString();
    wxFileName fn(path);
    fn.Normalize(wxPATH_NORM_DOTS | wxPATH_NORM_ABSOLUTE | wxPATH_NORM_TILDE);
    return fn.GetFullPath();
}

wxString QuoteForShell(const wxString &path)
{
    // Single-quote for /bin/sh; escape embedded single quotes.
    wxString escaped = path;
    escaped.Replace("'", "'\\''");
    return "'" + escaped + "'";
}

#ifdef __WXMAC__
void AppendCapturedOutput(wxString &message, const wxString &output)
{
    if (!output.empty())
        message += ": " + output;
}
#endif

bool LineLooksLikeTaskPortEvidence(const wxString &line)
{
    const wxString lower = line.Lower();
    return lower.Contains("mach task port")
        || lower.Contains("taskgated")
        || (lower.Contains("codesigned") && lower.Contains("taskgated"));
}

#ifdef __WXMAC__

wxString RunCapture(const wxString &command, int *exitCode)
{
    wxArrayString output;
    wxArrayString errors;
    const int rc = wxExecute(command, output, errors, wxEXEC_SYNC | wxEXEC_NODISABLE);
    if (exitCode)
        *exitCode = rc;
    wxString all;
    for (size_t i = 0; i < output.GetCount(); ++i)
    {
        if (!all.empty())
            all << '\n';
        all << output[i];
    }
    for (size_t i = 0; i < errors.GetCount(); ++i)
    {
        if (!all.empty())
            all << '\n';
        all << errors[i];
    }
    return all;
}

void FillCodesignFields(const wxString &gdbPath, GdbMacSigningDiagnosis &d)
{
    const wxString q = QuoteForShell(gdbPath);
    int verifyRc = -1;
    RunCapture("codesign --verify --verbose=2 " + q, &verifyRc);
    if (verifyRc != 0)
    {
        d.signature = GdbMacSignatureState::MissingOrInvalid;
        d.hasDebuggerEntitlement = false;
        return;
    }

    int dvRc = 0;
    const wxString dv = RunCapture("codesign -dv --verbose=4 " + q, &dvRc);
    if (dv.Contains("Signature=adhoc") || dv.Lower().Contains("signature=adhoc"))
        d.signature = GdbMacSignatureState::ValidAdhoc;
    else
        d.signature = GdbMacSignatureState::ValidNamed;
    // First Authority= line (adhoc often has none).
    {
        const wxString key = "Authority=";
        const int pos = dv.Find(key);
        if (pos != wxNOT_FOUND)
        {
            wxString rest = dv.Mid(pos + key.length());
            d.authority = rest.BeforeFirst('\n').Trim(true).Trim(false);
        }
    }

    int entRc = 0;
    const wxString ents = RunCapture("codesign -d --entitlements - " + q, &entRc);
    d.hasDebuggerEntitlement = ents.Contains("com.apple.security.cs.debugger");

    int xaRc = 0;
    const wxString xa = RunCapture("xattr -p com.apple.quarantine " + q, &xaRc);
    wxString xaTrimmed = xa;
    xaTrimmed.Trim(true).Trim(false);
    d.quarantinePresent = (xaRc == 0 && !xaTrimmed.empty());
}

wxString SystemKeychainPath()
{
    return "/Library/Keychains/System.keychain";
}

wxString LoginKeychainPath()
{
    const wxString home = wxFileName::GetHomeDir();
    wxFileName db(home + "/Library/Keychains/login.keychain-db");
    if (db.FileExists())
        return db.GetFullPath();
    wxFileName legacy(home + "/Library/Keychains/login.keychain");
    return legacy.GetFullPath();
}

bool IdentityExistsInSystemKeychain(const wxString &cn)
{
    int rc = 0;
    const wxString out = RunCapture(
        "security find-identity -v -p codesigning " + QuoteForShell(SystemKeychainPath()), &rc);
    return out.Contains(cn);
}

/** Drop a login-keychain copy so codesign and taskgated use the System identity. */
void RemoveLoginTier2Identity(const wxString &cn)
{
    const wxString login = QuoteForShell(LoginKeychainPath());
    const wxString name = QuoteForShell(cn);
    int rc = 0;
    RunCapture("security delete-identity -c " + name + " " + login, &rc);
    RunCapture("security delete-certificate -c " + name + " " + login, &rc);
}

struct CfTypeGuard
{
    CFTypeRef ref = nullptr;
    CfTypeGuard() = default;
    ~CfTypeGuard()
    {
        if (ref)
            CFRelease(ref);
    }
    CfTypeGuard(const CfTypeGuard &) = delete;
    CfTypeGuard &operator=(const CfTypeGuard &) = delete;
    void reset(CFTypeRef next)
    {
        if (ref)
            CFRelease(ref);
        ref = next;
    }
};

struct DlopenGuard
{
    void *handle = nullptr;
    DlopenGuard() = default;
    ~DlopenGuard()
    {
        if (handle)
            dlclose(handle);
    }
    DlopenGuard(const DlopenGuard &) = delete;
    DlopenGuard &operator=(const DlopenGuard &) = delete;
};

/**
 * Trust a DER certificate for code signing in the admin domain.
 * Called from this GUI process so macOS can show the administrator panel.
 * Root cannot: authorizationdb write is denied (-60005) on current macOS,
 * and SecTrustSettingsSetTrustSettings refuses interaction when already root.
 */
wxString TrustCertificateFileForCodeSigning(const wxString &derPath)
{
    wxFFile in(derPath, "rb");
    if (!in.IsOpened())
        return "cannot read certificate for trust settings";
    const wxFileOffset len = in.Length();
    if (len <= 0 || len > 1024 * 1024)
        return "certificate file is empty or too large";
    wxCharBuffer bytes(static_cast<size_t>(len));
    if (in.Read(bytes.data(), static_cast<size_t>(len)) != static_cast<size_t>(len))
        return "cannot read certificate for trust settings";
    in.Close();

    DlopenGuard securityLib;
    securityLib.handle = dlopen("/System/Library/Frameworks/Security.framework/Security", RTLD_LAZY);
    if (!securityLib.handle)
        return wxString("cannot load Security framework: ") + dlerror();

    using CreateCertFn = SecCertificateRef (*)(CFAllocatorRef, CFDataRef);
    using CreatePolicyFn = SecPolicyRef (*)(CFTypeRef, CFDictionaryRef);
    using SetTrustFn = OSStatus (*)(SecCertificateRef, SecTrustSettingsDomain, CFTypeRef);
    auto createCert = reinterpret_cast<CreateCertFn>(dlsym(securityLib.handle, "SecCertificateCreateWithData"));
    auto createPolicy = reinterpret_cast<CreatePolicyFn>(dlsym(securityLib.handle, "SecPolicyCreateWithProperties"));
    auto setTrust = reinterpret_cast<SetTrustFn>(dlsym(securityLib.handle, "SecTrustSettingsSetTrustSettings"));
    auto policyId = reinterpret_cast<CFStringRef *>(dlsym(securityLib.handle, "kSecPolicyAppleCodeSigning"));
    if (!createCert || !createPolicy || !setTrust || !policyId || !*policyId)
        return "Security framework is missing the trust-settings API";

    CfTypeGuard der;
    der.reset(CFDataCreate(kCFAllocatorDefault,
                           reinterpret_cast<const UInt8 *>(bytes.data()),
                           static_cast<CFIndex>(len)));
    if (!der.ref)
        return "certificate could not be parsed";

    CfTypeGuard cert;
    cert.reset(createCert(kCFAllocatorDefault, static_cast<CFDataRef>(der.ref)));
    if (!cert.ref)
        return "certificate could not be parsed";

    CfTypeGuard policy;
    policy.reset(createPolicy(*policyId, nullptr));
    if (!policy.ref)
        return "cannot create code signing policy";

    SInt32 trustResult = kSecTrustSettingsResultTrustRoot;
    CfTypeGuard resultNum;
    resultNum.reset(CFNumberCreate(kCFAllocatorDefault, kCFNumberSInt32Type, &trustResult));
    if (!resultNum.ref)
        return "cannot create trust settings";

    const void *keys[] = { kSecTrustSettingsPolicy, kSecTrustSettingsResult };
    const void *vals[] = { policy.ref, resultNum.ref };
    CfTypeGuard constraint;
    constraint.reset(CFDictionaryCreate(kCFAllocatorDefault, keys, vals, 2,
                                        &kCFTypeDictionaryKeyCallBacks,
                                        &kCFTypeDictionaryValueCallBacks));
    if (!constraint.ref)
        return "cannot create trust settings";

    const void *items[] = { constraint.ref };
    CfTypeGuard settings;
    settings.reset(CFArrayCreate(kCFAllocatorDefault, items, 1, &kCFTypeArrayCallBacks));
    if (!settings.ref)
        return "cannot create trust settings";

    const OSStatus st = setTrust(reinterpret_cast<SecCertificateRef>(const_cast<void *>(cert.ref)),
                                  kSecTrustSettingsDomainAdmin,
                                  settings.ref);

    if (st == errSecSuccess)
        return wxString();
    if (st == -60006 || st == -128) // errAuthorizationCanceled, userCanceledErr
        return "code signing trust was cancelled";
    return wxString::Format("code signing trust was not set (error %d)", static_cast<int>(st));
}

/** mkdtemp directory, mode 0700. Empty string on failure. */
wxString CreatePrivateCertTempDir()
{
    const wxString pattern = wxFileName::GetTempDir()
                           + wxFileName::GetPathSeparator()
                           + "cb-gdb-cert-XXXXXX";
    const wxScopedCharBuffer utf8 = pattern.utf8_str();
    if (!utf8 || utf8.length() == 0)
        return wxString();
    char *storage = static_cast<char *>(malloc(utf8.length() + 1));
    if (!storage)
        return wxString();
    memcpy(storage, utf8.data(), utf8.length() + 1);
    const char *made = mkdtemp(storage);
    const wxString dir = made ? wxString::FromUTF8(made) : wxString();
    free(storage);
    return dir;
}

/** Unlink one temp file. unlink does not follow a final-path symlink. */
void UnlinkOwnedTempFile(const wxString &path)
{
    const wxScopedCharBuffer utf8 = path.utf8_str();
    if (!utf8)
        return;
    struct stat st;
    if (lstat(utf8.data(), &st) != 0)
        return;
    if (S_ISLNK(st.st_mode) || S_ISREG(st.st_mode))
        unlink(utf8.data());
}

/** Removes only the files this operation creates, then the directory itself. */
struct Tier2TempCleanup
{
    wxString dir;
    Tier2TempCleanup() = default;
    ~Tier2TempCleanup()
    {
        if (dir.empty())
            return;
        static const char *const kFiles[] = {
            "openssl.cnf", "key.pem", "cert.pem", "cert.der",
            "import.sh", "askpass.sh", "launch.sh"
        };
        const wxString sep = wxFileName::GetPathSeparator();
        for (const char *name : kFiles)
            UnlinkOwnedTempFile(dir + sep + name);
        const wxScopedCharBuffer dirUtf8 = dir.utf8_str();
        if (dirUtf8)
            rmdir(dirUtf8.data());
    }
    Tier2TempCleanup(const Tier2TempCleanup &) = delete;
    Tier2TempCleanup &operator=(const Tier2TempCleanup &) = delete;
};

/** Create self-signed code-signing cert in the System keychain. Empty string means success. */
wxString CreateTier2IdentityInSystemKeychain(const wxString &cn, bool *created)
{
    if (created)
        *created = false;
    if (IdentityExistsInSystemKeychain(cn))
    {
        RemoveLoginTier2Identity(cn);
        return wxString();
    }

    const wxString tmpDir = CreatePrivateCertTempDir();
    if (tmpDir.empty())
        return "cannot create temp dir for certificate";
    Tier2TempCleanup cleanup;
    cleanup.dir = tmpDir;

    // Install the cert and private key the way LLDB's macos-setup-codesign.sh
    // does (System keychain, key imported with -A, taskgated restarted).
    // That script then flips com.apple.trust-settings.admin to "allow". This
    // macOS denies that write (-60005) even under sudo, so trust is applied
    // afterwards from this GUI process, which is allowed to show the admin panel.
    const wxString cnf = tmpDir + "/openssl.cnf";
    const wxString key = tmpDir + "/key.pem";
    const wxString cert = tmpDir + "/cert.pem";
    const wxString der = tmpDir + "/cert.der";
    const wxString keychain = SystemKeychainPath();
    const wxString script = tmpDir + "/import.sh";
    const wxString askpass = tmpDir + "/askpass.sh";
    const wxString launcher = tmpDir + "/launch.sh";

    {
        wxFFile f(cnf, "w");
        if (!f.IsOpened())
            return "cannot write openssl.cnf";
        f.Write(wxString::Format(
            "[ req ]\n"
            "default_bits       = 2048\n"
            "encrypt_key        = no\n"
            "default_md         = sha512\n"
            "prompt             = no\n"
            "distinguished_name = codesign_dn\n"
            "[ codesign_dn ]\n"
            "commonName         = \"%s\"\n"
            "[ codesign_reqext ]\n"
            "basicConstraints   = critical,CA:TRUE\n"
            "keyUsage           = critical,digitalSignature,keyCertSign\n"
            "extendedKeyUsage   = critical,codeSigning\n",
            cn));
        f.Close();
    }

    int rc = 0;
    const wxString reqOut = RunCapture(wxString::Format(
                   "openssl req -new -newkey rsa:2048 -x509 -days 3650 -nodes "
                   "-config %s -extensions codesign_reqext -batch -out %s -keyout %s",
                   QuoteForShell(cnf), QuoteForShell(cert), QuoteForShell(key)),
               &rc);
    if (rc != 0)
        return "openssl req failed creating code-signing certificate: " + reqOut;

    const wxString derOut = RunCapture(wxString::Format(
                   "openssl x509 -in %s -outform DER -out %s",
                   QuoteForShell(cert), QuoteForShell(der)),
               &rc);
    if (rc != 0)
        return "openssl x509 DER conversion failed: " + derOut;

    {
        wxFFile sh(script, "w");
        if (!sh.IsOpened())
            return "cannot write System-keychain import script";
        sh.Write(
            "#!/bin/sh\n"
            "kc=" + QuoteForShell(keychain) + "\n"
            "security delete-identity -c " + QuoteForShell(cn) + " \"$kc\" || true\n"
            "security delete-certificate -c " + QuoteForShell(cn) + " \"$kc\" || true\n"
            "security add-certificates -k \"$kc\" " + QuoteForShell(der) + " || exit 1\n"
            "security import " + QuoteForShell(key) + " -A -k \"$kc\" || exit 1\n"
            "pkill -f /usr/libexec/taskgated || true\n"
            "exit 0\n");
        sh.Close();
    }
    {
        wxFFile ap(askpass, "w");
        if (!ap.IsOpened())
            return "cannot write administrator password prompt";
        ap.Write(
            "#!/bin/sh\n"
            "osascript -e 'display dialog \"Code::Blocks needs an administrator password "
            "to install the debugger certificate in the System keychain.\" "
            "default answer \"\" with hidden answer with title \"Enable debugging\"' "
            "-e 'text returned of result'\n");
        ap.Close();
    }
    {
        // wxExecute execs argv[0] directly. A SUDO_ASKPASS=... prefix is not
        // a program, so the export lives in this launcher.
        wxFFile launch(launcher, "w");
        if (!launch.IsOpened())
            return "cannot write System-keychain launch script";
        launch.Write(
            "#!/bin/sh\n"
            "export SUDO_ASKPASS=" + QuoteForShell(askpass) + "\n"
            "exec script -q /dev/null sudo -A /bin/sh " + QuoteForShell(script) + "\n");
        launch.Close();
    }

    RunCapture("chmod 700 " + QuoteForShell(script) + " " + QuoteForShell(askpass)
                   + " " + QuoteForShell(launcher),
               &rc);

    const wxString importOut = RunCapture("/bin/sh " + QuoteForShell(launcher), &rc);
    wxString trustErr;
    if (rc == 0)
        trustErr = TrustCertificateFileForCodeSigning(der);

    if (rc != 0)
        return "System keychain import failed: " + importOut;
    if (!trustErr.empty())
        return trustErr;

    if (!IdentityExistsInSystemKeychain(cn))
        return "identity not visible in the System keychain after import: " + importOut;

    RemoveLoginTier2Identity(cn);

    if (created)
        *created = true;
    return wxString();
}

bool SignPathWithIdentity(const wxString &path, const wxString &identity,
                          const wxString &entitlementsPath, wxString *err)
{
    const wxString qPath = QuoteForShell(path);
    const wxString qId = QuoteForShell(identity);
    int rc = 0;
    wxString captured;
    if (entitlementsPath.empty())
        captured = RunCapture("codesign --force --sign " + qId + " " + qPath, &rc);
    else
        captured = RunCapture("codesign --force --sign " + qId + " --entitlements "
                                  + QuoteForShell(entitlementsPath) + " " + qPath,
                              &rc);
    if (rc != 0)
    {
        if (err)
        {
            *err = wxString::Format("codesign failed for %s (exit %d)", path, rc);
            AppendCapturedOutput(*err, captured);
        }
        return false;
    }
    return true;
}

#endif // __WXMAC__

} // namespace

bool GdbMacSigningDiagnosis::SigningDefective() const
{
    if (pathKind != GdbMacPathKind::Bundled)
        return false;
    if (signature == GdbMacSignatureState::MissingOrInvalid)
        return true;
    if (signature == GdbMacSignatureState::ValidAdhoc || signature == GdbMacSignatureState::ValidNamed)
        return !hasDebuggerEntitlement;
    return false;
}

bool GdbMacSigningDiagnosis::WouldOfferEnableDebugging() const
{
    if (pathKind != GdbMacPathKind::Bundled)
        return false;
    if (SigningDefective())
        return true;
    // Valid ad-hoc/named + entitlement: only with confirmed task-port evidence (Tier 2 later).
    if ((signature == GdbMacSignatureState::ValidAdhoc || signature == GdbMacSignatureState::ValidNamed)
        && hasDebuggerEntitlement
        && session.taskPortOrTaskgated)
    {
        return true;
    }
    return false;
}

wxString GetMacBundledGdbPath()
{
#ifdef __WXMAC__
    // Contents/Resources/share/codeblocks → ../../debugger/bin/gdb
    wxFileName bundled(ConfigManager::GetDataFolder() + "/../../debugger/bin/gdb");
    bundled.Normalize(wxPATH_NORM_DOTS | wxPATH_NORM_ABSOLUTE);
    return bundled.GetFullPath();
#else
    return wxString();
#endif
}

bool IsMacBundledGdbPath(const wxString &path)
{
    const wxString bundled = GetMacBundledGdbPath();
    if (bundled.empty() || path.empty())
        return false;
    return NormalizePath(path) == NormalizePath(bundled);
}

bool ClassifyGdbMacSessionLine(const wxString &line, GdbMacSessionEvidence &evidence)
{
    bool changed = false;
    const wxString trimmed = line.Strip(wxString::both);
    if (trimmed.StartsWith("No stack"))
    {
        if (!evidence.bareNoStack)
        {
            evidence.bareNoStack = true;
            changed = true;
        }
    }
    if (LineLooksLikeTaskPortEvidence(trimmed))
    {
        if (!evidence.taskPortOrTaskgated)
        {
            evidence.taskPortOrTaskgated = true;
            changed = true;
        }
    }
    return changed;
}

GdbMacSigningDiagnosis DiagnoseMacGdbSigning(const wxString &configuredDebuggerPath,
                                             const GdbMacSessionEvidence &session)
{
    GdbMacSigningDiagnosis d;
    d.session = session;
    d.bundledPath = GetMacBundledGdbPath();
    d.configuredPath = NormalizePath(configuredDebuggerPath);

#if !defined(__WXMAC__)
    d.pathKind = GdbMacPathKind::NotOnMac;
    return d;
#else
    if (!platform::macosx)
    {
        d.pathKind = GdbMacPathKind::NotOnMac;
        return d;
    }

    if (d.configuredPath.empty() || !wxFileName::FileExists(d.configuredPath))
    {
        d.pathKind = GdbMacPathKind::Missing;
        return d;
    }

    if (!IsMacBundledGdbPath(d.configuredPath))
    {
        d.pathKind = GdbMacPathKind::NotBundled;
        return d;
    }

    d.pathKind = GdbMacPathKind::Bundled;
    FillCodesignFields(d.configuredPath, d);
    return d;
#endif
}

wxString FormatGdbMacSigningDiagnosisForLog(const GdbMacSigningDiagnosis &d)
{
    wxString s;
    s << "macOS GDB signing diagnosis (v2.3 M2; evidence only, no repair):\n";
    s << "  configured: " << (d.configuredPath.empty() ? wxString("<empty>") : d.configuredPath) << '\n';
    s << "  bundled:    " << (d.bundledPath.empty() ? wxString("<n/a>") : d.bundledPath) << '\n';

    switch (d.pathKind)
    {
        case GdbMacPathKind::NotOnMac:
            s << "  path: not macOS\n";
            break;
        case GdbMacPathKind::Missing:
            s << "  path: missing / not found (no Enable debugging offer)\n";
            break;
        case GdbMacPathKind::NotBundled:
            s << "  path: not the bundled Resources/debugger/bin/gdb (helper will refuse)\n";
            break;
        case GdbMacPathKind::Bundled:
            s << "  path: bundled\n";
            break;
    }

    switch (d.signature)
    {
        case GdbMacSignatureState::NotChecked:
            s << "  signature: (not checked)\n";
            break;
        case GdbMacSignatureState::MissingOrInvalid:
            s << "  signature: missing or invalid\n";
            break;
        case GdbMacSignatureState::ValidAdhoc:
            s << "  signature: valid ad-hoc\n";
            break;
        case GdbMacSignatureState::ValidNamed:
            s << "  signature: valid named identity\n";
            break;
    }

    if (d.pathKind == GdbMacPathKind::Bundled && d.signature != GdbMacSignatureState::NotChecked
        && d.signature != GdbMacSignatureState::MissingOrInvalid)
    {
        s << "  entitlement com.apple.security.cs.debugger: "
          << (d.hasDebuggerEntitlement ? "present" : "ABSENT") << '\n';
    }

    s << "  quarantine xattr: " << (d.quarantinePresent ? "present (noted separately; not debugger repair)"
                                                        : "absent or unset") << '\n';
    s << "  session: bare No stack=" << (d.session.bareNoStack ? "yes" : "no")
      << "; task-port/taskgated=" << (d.session.taskPortOrTaskgated ? "yes" : "no")
      << "; launch failed=" << (d.session.launchFailed ? "yes" : "no") << '\n';
    s << "  signing defective (Tier 1 candidate): " << (d.SigningDefective() ? "yes" : "no") << '\n';
    s << "  would offer Enable debugging (M5 later): "
      << (d.WouldOfferEnableDebugging() ? "yes" : "no");
    if (d.session.bareNoStack && !d.WouldOfferEnableDebugging() && !d.SigningDefective())
        s << "\n  note: bare \"No stack\" alone is not enough without signing or task-port evidence";
    return s;
}

GdbMacTier1Decision DecideMacGdbTier1(const GdbMacSigningDiagnosis &d)
{
    switch (d.pathKind)
    {
        case GdbMacPathKind::NotOnMac:
            return GdbMacTier1Decision::RefuseNotOnMac;
        case GdbMacPathKind::Missing:
            return GdbMacTier1Decision::RefuseMissing;
        case GdbMacPathKind::NotBundled:
            return GdbMacTier1Decision::RefuseNotBundled;
        case GdbMacPathKind::Bundled:
            break;
    }
    if (!d.SigningDefective())
        return GdbMacTier1Decision::SkipAlreadyValid;
    return GdbMacTier1Decision::ShouldRestore;
}

wxString GetMacBundledGdbEntitlementsPath()
{
#ifdef __WXMAC__
    // Contents/Resources/share/codeblocks → ../../debugger/gdb.entitlements
    wxFileName ents(ConfigManager::GetDataFolder() + "/../../debugger/gdb.entitlements");
    ents.Normalize(wxPATH_NORM_DOTS | wxPATH_NORM_ABSOLUTE);
    if (ents.FileExists())
        return ents.GetFullPath();
    return wxString();
#else
    return wxString();
#endif
}

GdbMacTier1Result TryMacGdbTier1Restore(const wxString &configuredDebuggerPath)
{
    GdbMacTier1Result r;
    r.before = DiagnoseMacGdbSigning(configuredDebuggerPath, GdbMacSessionEvidence());
    const GdbMacTier1Decision decision = DecideMacGdbTier1(r.before);

    switch (decision)
    {
        case GdbMacTier1Decision::RefuseNotOnMac:
            r.kind = GdbMacTier1ResultKind::Refused;
            r.message = "Tier 1 refused: not macOS";
            return r;
        case GdbMacTier1Decision::RefuseMissing:
            r.kind = GdbMacTier1ResultKind::Refused;
            r.message = "Tier 1 refused: debugger executable missing";
            return r;
        case GdbMacTier1Decision::RefuseNotBundled:
            r.kind = GdbMacTier1ResultKind::Refused;
            r.message = "Tier 1 refused: not the bundled Resources/debugger/bin/gdb";
            return r;
        case GdbMacTier1Decision::SkipAlreadyValid:
            r.kind = GdbMacTier1ResultKind::SkippedAlreadyValid;
            r.after = r.before;
            r.message = "Tier 1 skipped: signature and cs.debugger already valid (no resign)";
            return r;
        case GdbMacTier1Decision::ShouldRestore:
            break;
    }

#if !defined(__WXMAC__)
    r.kind = GdbMacTier1ResultKind::Refused;
    r.message = "Tier 1 refused: not built for macOS";
    return r;
#else
    const wxString ents = GetMacBundledGdbEntitlementsPath();
    if (ents.empty())
    {
        r.kind = GdbMacTier1ResultKind::Failed;
        r.message = "Tier 1 failed: bundled gdb.entitlements not found under Resources/debugger/";
        return r;
    }

    const wxString gdbPath = r.before.configuredPath;
    const wxString qGdb = QuoteForShell(gdbPath);
    const wxString qEnts = QuoteForShell(ents);

    // Re-sign companion dylibs ad-hoc (no debugger entitlement required).
    wxFileName libDirFn(gdbPath);
    const wxString libDir = libDirFn.GetPath() + wxFileName::GetPathSeparator() + ".."
                          + wxFileName::GetPathSeparator() + "lib";
    wxFileName libNorm(libDir);
    libNorm.Normalize(wxPATH_NORM_DOTS | wxPATH_NORM_ABSOLUTE);
    if (wxDir::Exists(libNorm.GetFullPath()))
    {
        wxDir dir(libNorm.GetFullPath());
        wxString name;
        bool cont = dir.GetFirst(&name, "*.dylib", wxDIR_FILES);
        while (cont)
        {
            const wxString libPath = libNorm.GetFullPath() + wxFileName::GetPathSeparator() + name;
            int libRc = -1;
            const wxString libOut = RunCapture("codesign --force --sign - " + QuoteForShell(libPath), &libRc);
            if (libRc != 0)
            {
                r.kind = GdbMacTier1ResultKind::Failed;
                r.message = wxString::Format("Tier 1 failed: codesign dylib %s (exit %d)", name, libRc);
                AppendCapturedOutput(r.message, libOut);
                return r;
            }
            cont = dir.GetNext(&name);
        }
    }

    int signRc = -1;
    const wxString signOut = RunCapture("codesign --force --sign - --entitlements " + qEnts + " " + qGdb, &signRc);
    if (signRc != 0)
    {
        r.kind = GdbMacTier1ResultKind::Failed;
        r.message = wxString::Format("Tier 1 failed: codesign gdb (exit %d)", signRc);
        AppendCapturedOutput(r.message, signOut);
        r.after = DiagnoseMacGdbSigning(gdbPath, GdbMacSessionEvidence());
        return r;
    }

    r.after = DiagnoseMacGdbSigning(gdbPath, GdbMacSessionEvidence());
    if (r.after.SigningDefective() || !r.after.hasDebuggerEntitlement
        || r.after.signature == GdbMacSignatureState::MissingOrInvalid)
    {
        r.kind = GdbMacTier1ResultKind::Failed;
        r.message = "Tier 1 failed: post-sign verify still defective";
        return r;
    }

    r.kind = GdbMacTier1ResultKind::Restored;
    r.message = "Tier 1 restored: ad-hoc signature + com.apple.security.cs.debugger";
    return r;
#endif
}

wxString FormatGdbMacTier1ResultForLog(const GdbMacTier1Result &r)
{
    wxString s;
    s << "macOS GDB Tier 1 restore (v2.3 M3):\n";
    s << "  result: ";
    switch (r.kind)
    {
        case GdbMacTier1ResultKind::Refused:
            s << "refused";
            break;
        case GdbMacTier1ResultKind::SkippedAlreadyValid:
            s << "skipped (already valid)";
            break;
        case GdbMacTier1ResultKind::Restored:
            s << "restored";
            break;
        case GdbMacTier1ResultKind::Failed:
            s << "failed";
            break;
    }
    s << '\n';
    s << "  " << r.message;
    return s;
}

wxString GetMacGdbTier2IdentityName()
{
    return wxString::FromUTF8(kTier2IdentityCn);
}

bool IsMacGdbForceTier2FromEnv()
{
    wxString v;
    if (!wxGetEnv("CB_FORCE_GDB_TIER2", &v))
        return false;
    v = v.Lower().Trim(true).Trim(false);
    return v == "1" || v == "true" || v == "yes" || v == "on";
}

GdbMacTier2Decision DecideMacGdbTier2(const GdbMacSigningDiagnosis &d, bool forceTier2)
{
    switch (d.pathKind)
    {
        case GdbMacPathKind::NotOnMac:
            return GdbMacTier2Decision::RefuseNotOnMac;
        case GdbMacPathKind::Missing:
            return GdbMacTier2Decision::RefuseMissing;
        case GdbMacPathKind::NotBundled:
            return GdbMacTier2Decision::RefuseNotBundled;
        case GdbMacPathKind::Bundled:
            break;
    }
    if (d.SigningDefective())
        return GdbMacTier2Decision::RefuseSigningStillDefective;

    const wxString ourCn = GetMacGdbTier2IdentityName();
    const bool alreadyOurs = (d.signature == GdbMacSignatureState::ValidNamed)
                             && d.hasDebuggerEntitlement
                             && d.authority.Contains(ourCn);

    if (forceTier2)
        return GdbMacTier2Decision::ShouldRun;

    if (alreadyOurs)
        return GdbMacTier2Decision::SkipAlreadyNamed;

    if (d.hasDebuggerEntitlement
        && (d.signature == GdbMacSignatureState::ValidAdhoc
            || d.signature == GdbMacSignatureState::ValidNamed)
        && d.session.taskPortOrTaskgated)
    {
        return GdbMacTier2Decision::ShouldRun;
    }

    return GdbMacTier2Decision::SkipNotNeeded;
}

GdbMacTier2Result TryMacGdbTier2Restore(const wxString &configuredDebuggerPath,
                                         const GdbMacSessionEvidence &session,
                                         bool forceTier2)
{
    GdbMacTier2Result r;
    r.identityName = GetMacGdbTier2IdentityName();
    r.before = DiagnoseMacGdbSigning(configuredDebuggerPath, session);
    const GdbMacTier2Decision decision = DecideMacGdbTier2(r.before, forceTier2);

    switch (decision)
    {
        case GdbMacTier2Decision::RefuseNotOnMac:
            r.kind = GdbMacTier2ResultKind::Refused;
            r.message = "Tier 2 refused: not macOS";
            return r;
        case GdbMacTier2Decision::RefuseMissing:
            r.kind = GdbMacTier2ResultKind::Refused;
            r.message = "Tier 2 refused: debugger executable missing";
            return r;
        case GdbMacTier2Decision::RefuseNotBundled:
            r.kind = GdbMacTier2ResultKind::Refused;
            r.message = "Tier 2 refused: not the bundled Resources/debugger/bin/gdb";
            return r;
        case GdbMacTier2Decision::RefuseSigningStillDefective:
            r.kind = GdbMacTier2ResultKind::Refused;
            r.message = "Tier 2 refused: signing still defective (run Tier 1 first)";
            return r;
        case GdbMacTier2Decision::SkipNotNeeded:
            r.kind = GdbMacTier2ResultKind::SkippedNotNeeded;
            r.after = r.before;
            r.message = "Tier 2 skipped: not required (no force; no task-port evidence)";
            return r;
        case GdbMacTier2Decision::SkipAlreadyNamed:
            r.kind = GdbMacTier2ResultKind::SkippedAlreadyNamed;
            r.after = r.before;
            r.message = "Tier 2 skipped: already signed with " + r.identityName;
            return r;
        case GdbMacTier2Decision::ShouldRun:
            break;
    }

#if !defined(__WXMAC__)
    r.kind = GdbMacTier2ResultKind::Refused;
    r.message = "Tier 2 refused: not built for macOS";
    return r;
#else
    // Re-diagnose is path-only; Tier2 ShouldRun from force doesn't need session on before.
    // Merge: if force, before.session may be empty — OK.

    bool created = false;
    const wxString createErr = CreateTier2IdentityInSystemKeychain(r.identityName, &created);
    r.createdNewIdentity = created;
    if (!createErr.empty())
    {
        r.kind = GdbMacTier2ResultKind::Failed;
        r.message = "Tier 2 failed: " + createErr;
        return r;
    }

    const wxString ents = GetMacBundledGdbEntitlementsPath();
    if (ents.empty())
    {
        r.kind = GdbMacTier2ResultKind::Failed;
        r.message = "Tier 2 failed: bundled gdb.entitlements not found";
        return r;
    }

    const wxString gdbPath = r.before.configuredPath;
    wxString err;

    // Companion dylibs: same identity, no debugger entitlement.
    wxFileName libDirFn(gdbPath);
    const wxString libDir = libDirFn.GetPath() + wxFileName::GetPathSeparator() + ".."
                          + wxFileName::GetPathSeparator() + "lib";
    wxFileName libNorm(libDir);
    libNorm.Normalize(wxPATH_NORM_DOTS | wxPATH_NORM_ABSOLUTE);
    if (wxDir::Exists(libNorm.GetFullPath()))
    {
        wxDir dir(libNorm.GetFullPath());
        wxString name;
        bool cont = dir.GetFirst(&name, "*.dylib", wxDIR_FILES);
        while (cont)
        {
            const wxString libPath = libNorm.GetFullPath() + wxFileName::GetPathSeparator() + name;
            if (!SignPathWithIdentity(libPath, r.identityName, wxString(), &err))
            {
                r.kind = GdbMacTier2ResultKind::Failed;
                r.message = "Tier 2 failed: " + err;
                return r;
            }
            cont = dir.GetNext(&name);
        }
    }

    if (!SignPathWithIdentity(gdbPath, r.identityName, ents, &err))
    {
        r.kind = GdbMacTier2ResultKind::Failed;
        r.message = "Tier 2 failed: " + err;
        r.after = DiagnoseMacGdbSigning(gdbPath, GdbMacSessionEvidence());
        return r;
    }

    r.after = DiagnoseMacGdbSigning(gdbPath, GdbMacSessionEvidence());
    if (r.after.SigningDefective() || !r.after.hasDebuggerEntitlement
        || r.after.signature != GdbMacSignatureState::ValidNamed)
    {
        r.kind = GdbMacTier2ResultKind::Failed;
        r.message = "Tier 2 failed: post-sign verify (expected named + cs.debugger)";
        return r;
    }

    r.kind = GdbMacTier2ResultKind::Restored;
    r.message = wxString::Format(
        "Tier 2 restored: identity \"%s\"%s + com.apple.security.cs.debugger",
        r.identityName,
        r.createdNewIdentity ? " (created)" : " (reused)");
    return r;
#endif
}

wxString FormatGdbMacTier2ResultForLog(const GdbMacTier2Result &r)
{
    wxString s;
    s << "macOS GDB Tier 2 restore (v2.3 M4):\n";
    s << "  result: ";
    switch (r.kind)
    {
        case GdbMacTier2ResultKind::Refused:
            s << "refused";
            break;
        case GdbMacTier2ResultKind::SkippedNotNeeded:
            s << "skipped (not needed)";
            break;
        case GdbMacTier2ResultKind::SkippedAlreadyNamed:
            s << "skipped (already named)";
            break;
        case GdbMacTier2ResultKind::Restored:
            s << "restored";
            break;
        case GdbMacTier2ResultKind::Failed:
            s << "failed";
            break;
    }
    s << "\n  identity: " << r.identityName;
    s << "\n  " << r.message;
    return s;
}

GdbMacEnableRepairResult TryMacGdbEnableDebuggingRepair(const wxString &configuredDebuggerPath,
                                                       const GdbMacSessionEvidence &session,
                                                       bool forceTier2)
{
    GdbMacEnableRepairResult out;
    GdbMacSigningDiagnosis diag = DiagnoseMacGdbSigning(configuredDebuggerPath, session);

    if (DecideMacGdbTier1(diag) == GdbMacTier1Decision::ShouldRestore)
    {
        out.attemptedTier1 = true;
        out.tier1 = TryMacGdbTier1Restore(configuredDebuggerPath);
        if (out.tier1.kind == GdbMacTier1ResultKind::Failed
            || out.tier1.kind == GdbMacTier1ResultKind::Refused)
        {
            return out;
        }
        diag = DiagnoseMacGdbSigning(configuredDebuggerPath, session);
    }
    else
    {
        out.tier1.kind = GdbMacTier1ResultKind::SkippedAlreadyValid;
        out.tier1.message = "Tier 1 not required";
        out.tier1.before = diag;
        out.tier1.after = diag;
    }

    // Attach session to diagnosis for Tier 2 decision (task-port).
    diag.session = session;
    const GdbMacTier2Decision t2 = DecideMacGdbTier2(diag, forceTier2);
    if (t2 == GdbMacTier2Decision::ShouldRun
        || (forceTier2 && t2 != GdbMacTier2Decision::RefuseNotBundled
            && t2 != GdbMacTier2Decision::RefuseMissing
            && t2 != GdbMacTier2Decision::RefuseNotOnMac
            && t2 != GdbMacTier2Decision::RefuseSigningStillDefective))
    {
        out.attemptedTier2 = true;
        out.tier2 = TryMacGdbTier2Restore(configuredDebuggerPath, session, forceTier2);
    }
    else
    {
        out.tier2.kind = (t2 == GdbMacTier2Decision::SkipAlreadyNamed)
                             ? GdbMacTier2ResultKind::SkippedAlreadyNamed
                             : GdbMacTier2ResultKind::SkippedNotNeeded;
        out.tier2.message = (t2 == GdbMacTier2Decision::SkipAlreadyNamed)
                                ? "Tier 2 not required (already named)"
                                : "Tier 2 not required";
        out.tier2.identityName = GetMacGdbTier2IdentityName();
        out.tier2.before = diag;
        out.tier2.after = diag;
    }
    return out;
}
