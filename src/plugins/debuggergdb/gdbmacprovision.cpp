/*
 * This file is part of the Code::Blocks IDE and licensed under the GNU General Public License, version 3
 * http://www.gnu.org/licenses/gpl-3.0.html
 *
 * v2.3 M6: in-process macOS provisioning for the bundled GDB.
 * Compiled on every platform; the body exists only when __WXMAC__ is set.
 */

#include "sdk.h"
#ifndef CB_PRECOMP
    #include <wx/dir.h>
    #include <wx/filename.h>
    #include <wx/ffile.h>
    #include <wx/utils.h>
#endif

#include "gdbmacprovision.h"

#if defined(__WXMAC__)

#include "gdbmacossigningdiagnose.h"

#include <Security/SecCertificate.h>
#include <Security/SecPolicy.h>
#include <Security/SecTrustSettings.h>
#include <dlfcn.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>
#include <limits.h>

namespace
{

wxString QuoteForShell(const wxString &path)
{
    wxString escaped = path;
    escaped.Replace("'", "'\\''");
    return "'" + escaped + "'";
}

void AppendCapturedOutput(wxString &message, const wxString &output)
{
    if (!output.empty())
        message += ": " + output;
}

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
wxString CreateTier2IdentityInSystemKeychain(const wxString &cn, bool *created, bool *cancelled)
{
    if (cancelled)
        *cancelled = false;
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
    {
        if (cancelled && trustErr == "code signing trust was cancelled")
            *cancelled = true;
        return trustErr;
    }

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
wxString RealPathOf(const wxString &path)
{
    if (path.empty())
        return wxString();
    const wxScopedCharBuffer utf8 = path.utf8_str();
    if (!utf8 || utf8.length() == 0)
        return wxString();
    char resolved[PATH_MAX];
    if (!realpath(utf8.data(), resolved))
        return wxString();
    return wxString::FromUTF8(resolved);
}

bool IsUnderRoot(const wxString &realFile, const wxString &realRoot)
{
    if (realFile.empty() || realRoot.empty())
        return false;
    if (realFile == realRoot)
        return true;
    return realFile.StartsWith(realRoot + wxFileName::GetPathSeparator());
}

struct BundledPayload
{
    wxString gdbPath;
    wxString entitlements;
    wxArrayString dylibs;
    wxString refusal;
};

/** Lexical bundled paths whose realpath stays under Resources/debugger/. */
BundledPayload ResolveBundledPayload()
{
    BundledPayload payload;
    const wxString gdbPath = GetMacBundledGdbPath();
    if (gdbPath.empty() || !wxFileName::FileExists(gdbPath))
    {
        payload.refusal = "bundled gdb is missing";
        return payload;
    }

    wxFileName rootFn;
    rootFn.AssignDir(wxFileName(gdbPath).GetPath()); // .../debugger/bin
    rootFn.RemoveLastDir();                          // .../debugger
    const wxString realRoot = RealPathOf(rootFn.GetFullPath());
    if (!IsUnderRoot(RealPathOf(gdbPath), realRoot))
    {
        payload.refusal = "bundled gdb path is not contained in Resources/debugger/";
        return payload;
    }
    payload.gdbPath = gdbPath;

    const wxString ents = GetMacBundledGdbEntitlementsPath();
    if (!ents.empty())
    {
        if (!IsUnderRoot(RealPathOf(ents), realRoot))
        {
            payload.refusal = "gdb.entitlements path is not contained in Resources/debugger/";
            return payload;
        }
        payload.entitlements = ents;
    }

    const wxString libDir = rootFn.GetFullPath() + wxFileName::GetPathSeparator() + "lib";
    if (!wxDir::Exists(libDir))
        return payload;
    if (!IsUnderRoot(RealPathOf(libDir), realRoot))
    {
        payload.refusal = "debugger lib path is not contained in Resources/debugger/";
        return payload;
    }

    wxDir dir(libDir);
    wxString name;
    bool cont = dir.GetFirst(&name, "*.dylib", wxDIR_FILES);
    while (cont)
    {
        const wxString libPath = libDir + wxFileName::GetPathSeparator() + name;
        if (!IsUnderRoot(RealPathOf(libPath), realRoot))
        {
            payload.refusal = "dylib " + name + " is not contained in Resources/debugger/";
            return payload;
        }
        payload.dylibs.Add(libPath);
        cont = dir.GetNext(&name);
    }
    return payload;
}

bool PayloadStillContained(const BundledPayload &payload)
{
    BundledPayload again = ResolveBundledPayload();
    if (!again.refusal.empty() || again.gdbPath != payload.gdbPath)
        return false;
    if (again.entitlements != payload.entitlements)
        return false;
    if (again.dylibs.GetCount() != payload.dylibs.GetCount())
        return false;
    for (size_t i = 0; i < payload.dylibs.GetCount(); ++i)
    {
        if (again.dylibs[i] != payload.dylibs[i])
            return false;
    }
    return true;
}

void AppendTier1Log(wxString &log, const wxString &resultWord, const wxString &message)
{
    if (!log.empty())
        log << '\n';
    log << "macOS GDB Tier 1 restore (v2.3 M3):\n";
    log << "  result: " << resultWord << "\n";
    log << "  " << message;
}

void AppendTier2Log(wxString &log, const wxString &resultWord, const wxString &identity,
                    const wxString &message)
{
    if (!log.empty())
        log << '\n';
    log << "macOS GDB Tier 2 restore (v2.3 M4):\n";
    log << "  result: " << resultWord << "\n";
    log << "  identity: " << identity << "\n";
    log << "  " << message;
}

bool Tier1Verified(const wxString &gdbPath)
{
    const GdbMacSigningDiagnosis after = DiagnoseMacGdbSigning(gdbPath, GdbMacSessionEvidence());
    return after.signature == GdbMacSignatureState::ValidAdhoc
        && after.hasDebuggerEntitlement
        && !after.SigningDefective();
}

bool Tier2Verified(const wxString &gdbPath, const wxString &identity)
{
    const GdbMacSigningDiagnosis after = DiagnoseMacGdbSigning(gdbPath, GdbMacSessionEvidence());
    return after.signature == GdbMacSignatureState::ValidNamed
        && after.hasDebuggerEntitlement
        && after.authority.Contains(identity)
        && !after.SigningDefective();
}

bool AlreadyNamedIdentity(const GdbMacSigningDiagnosis &d, const wxString &identity)
{
    return d.signature == GdbMacSignatureState::ValidNamed
        && d.hasDebuggerEntitlement
        && d.authority.Contains(identity);
}

GdbMacProvisionResult FailTier1(const wxString &message)
{
    GdbMacProvisionResult result;
    result.outcome = GdbMacProvisionOutcome::Failed;
    AppendTier1Log(result.log, "failed", message);
    return result;
}

} // namespace

GdbMacProvisionResult ProvisionMacBundledGdb(bool runTier2, bool forceTier2)
{
    if (forceTier2)
        runTier2 = true;

    GdbMacProvisionResult result;
    const BundledPayload payload = ResolveBundledPayload();
    if (!payload.refusal.empty())
    {
        result.outcome = GdbMacProvisionOutcome::Refused;
        result.log = "macOS GDB provisioning refused: " + payload.refusal;
        return result;
    }

    const wxString identity = GetMacGdbTier2IdentityName();
    const GdbMacSigningDiagnosis before = DiagnoseMacGdbSigning(payload.gdbPath, GdbMacSessionEvidence());
    const bool needTier1 = before.SigningDefective();

    if (needTier1)
    {
        if (payload.entitlements.empty())
            return FailTier1("Tier 1 failed: bundled gdb.entitlements not found under Resources/debugger/");

        for (size_t i = 0; i < payload.dylibs.GetCount(); ++i)
        {
            const wxString libPath = payload.dylibs[i];
            int libRc = -1;
            const wxString libOut = RunCapture("codesign --force --sign - " + QuoteForShell(libPath), &libRc);
            if (libRc != 0)
            {
                wxString message = wxString::Format("Tier 1 failed: codesign dylib %s (exit %d)",
                                                    wxFileName(libPath).GetFullName(), libRc);
                AppendCapturedOutput(message, libOut);
                return FailTier1(message);
            }
        }

        int signRc = -1;
        const wxString signOut = RunCapture("codesign --force --sign - --entitlements "
                                                + QuoteForShell(payload.entitlements) + " "
                                                + QuoteForShell(payload.gdbPath),
                                            &signRc);
        if (signRc != 0)
        {
            wxString message = wxString::Format("Tier 1 failed: codesign gdb (exit %d)", signRc);
            AppendCapturedOutput(message, signOut);
            return FailTier1(message);
        }
        if (!PayloadStillContained(payload) || !Tier1Verified(payload.gdbPath))
            return FailTier1("Tier 1 failed: post-sign verify still defective");

        AppendTier1Log(result.log, "restored",
                       "Tier 1 restored: ad-hoc signature + com.apple.security.cs.debugger");
        result.outcome = GdbMacProvisionOutcome::Tier1Restored;
    }

    if (!runTier2)
    {
        if (!needTier1)
            result.outcome = GdbMacProvisionOutcome::AlreadyHealthy;
        return result;
    }

    const GdbMacSigningDiagnosis ready = DiagnoseMacGdbSigning(payload.gdbPath, GdbMacSessionEvidence());
    if (ready.SigningDefective())
    {
        result.outcome = GdbMacProvisionOutcome::Failed;
        result.tier2Attempted = true;
        AppendTier2Log(result.log, "failed", identity,
                       "Tier 2 failed: signing still defective after Tier 1");
        return result;
    }
    if (!forceTier2 && AlreadyNamedIdentity(ready, identity))
    {
        if (!needTier1)
            result.outcome = GdbMacProvisionOutcome::AlreadyHealthy;
        return result;
    }

    result.tier2Attempted = true;
    if (payload.entitlements.empty())
    {
        result.outcome = GdbMacProvisionOutcome::Failed;
        AppendTier2Log(result.log, "failed", identity, "Tier 2 failed: bundled gdb.entitlements not found");
        return result;
    }

    bool created = false;
    bool cancelled = false;
    const wxString createErr = CreateTier2IdentityInSystemKeychain(identity, &created, &cancelled);
    if (!createErr.empty())
    {
        result.outcome = cancelled ? GdbMacProvisionOutcome::Cancelled : GdbMacProvisionOutcome::Failed;
        AppendTier2Log(result.log, cancelled ? "cancelled" : "failed", identity,
                       "Tier 2 failed: " + createErr);
        return result;
    }

    wxString err;
    for (size_t i = 0; i < payload.dylibs.GetCount(); ++i)
    {
        if (!SignPathWithIdentity(payload.dylibs[i], identity, wxString(), &err))
        {
            result.outcome = GdbMacProvisionOutcome::Failed;
            AppendTier2Log(result.log, "failed", identity, "Tier 2 failed: " + err);
            return result;
        }
    }
    if (!SignPathWithIdentity(payload.gdbPath, identity, payload.entitlements, &err))
    {
        result.outcome = GdbMacProvisionOutcome::Failed;
        AppendTier2Log(result.log, "failed", identity, "Tier 2 failed: " + err);
        return result;
    }
    if (!PayloadStillContained(payload) || !Tier2Verified(payload.gdbPath, identity))
    {
        result.outcome = GdbMacProvisionOutcome::Failed;
        AppendTier2Log(result.log, "failed", identity,
                       "Tier 2 failed: post-sign verify (expected named + cs.debugger)");
        return result;
    }

    result.outcome = GdbMacProvisionOutcome::Tier2Provisioned;
    AppendTier2Log(result.log, "restored", identity,
                   wxString::Format("Tier 2 restored: identity \"%s\"%s + com.apple.security.cs.debugger",
                                    identity, created ? " (created)" : " (reused)"));
    return result;
}

#endif // __WXMAC__
