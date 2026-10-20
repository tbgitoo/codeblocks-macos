#include "debuggergdb_test_common.h"
#include "gdbmacossigningdiagnose.h"

SUITE(GdbMacSigningDiagnose)
{

TEST(ClassifyBareNoStack)
{
    GdbMacSessionEvidence e;
    CHECK(ClassifyGdbMacSessionLine(wxT("No stack."), e));
    CHECK(e.bareNoStack);
    CHECK(!e.taskPortOrTaskgated);
}

TEST(ClassifyTaskPortMessage)
{
    GdbMacSessionEvidence e;
    const wxString line =
        wxT("Unable to find Mach task port for process-id 1234: (os/kern) failure (0x5).\n"
            " (please check gdb is codesigned - see taskgated(8))");
    // Classifier is per line; feed the informative fragment.
    CHECK(ClassifyGdbMacSessionLine(
        wxT("Unable to find Mach task port for process-id 1234: (os/kern) failure (0x5)."), e));
    CHECK(e.taskPortOrTaskgated);
    CHECK(ClassifyGdbMacSessionLine(wxT(" (please check gdb is codesigned - see taskgated(8))"), e));
    CHECK(e.taskPortOrTaskgated);
    CHECK(!e.bareNoStack);
    (void)line;
}

TEST(BareNoStackAloneDoesNotOffer)
{
    GdbMacSigningDiagnosis d;
    d.pathKind = GdbMacPathKind::Bundled;
    d.signature = GdbMacSignatureState::ValidAdhoc;
    d.hasDebuggerEntitlement = true;
    d.session.bareNoStack = true;
    CHECK(!d.SigningDefective());
    CHECK(!d.WouldOfferEnableDebugging());
}

TEST(MissingEntitlementOffers)
{
    GdbMacSigningDiagnosis d;
    d.pathKind = GdbMacPathKind::Bundled;
    d.signature = GdbMacSignatureState::ValidAdhoc;
    d.hasDebuggerEntitlement = false;
    d.session.bareNoStack = true; // as seen in lab
    CHECK(d.SigningDefective());
    CHECK(d.WouldOfferEnableDebugging());
}

TEST(InvalidSignatureOffers)
{
    GdbMacSigningDiagnosis d;
    d.pathKind = GdbMacPathKind::Bundled;
    d.signature = GdbMacSignatureState::MissingOrInvalid;
    CHECK(d.SigningDefective());
    CHECK(d.WouldOfferEnableDebugging());
}

TEST(ValidPlusTaskPortOffers)
{
    GdbMacSigningDiagnosis d;
    d.pathKind = GdbMacPathKind::Bundled;
    d.signature = GdbMacSignatureState::ValidAdhoc;
    d.hasDebuggerEntitlement = true;
    d.session.taskPortOrTaskgated = true;
    CHECK(!d.SigningDefective());
    CHECK(d.WouldOfferEnableDebugging());
}

TEST(NotBundledNeverOffers)
{
    GdbMacSigningDiagnosis d;
    d.pathKind = GdbMacPathKind::NotBundled;
    d.signature = GdbMacSignatureState::MissingOrInvalid;
    d.session.bareNoStack = true;
    CHECK(!d.SigningDefective());
    CHECK(!d.WouldOfferEnableDebugging());
}

TEST(MissingPathNeverOffers)
{
    GdbMacSigningDiagnosis d;
    d.pathKind = GdbMacPathKind::Missing;
    d.session.launchFailed = true;
    CHECK(!d.WouldOfferEnableDebugging());
}

TEST(FormatSummaryMentionsBareNoStackNote)
{
    GdbMacSigningDiagnosis d;
    d.pathKind = GdbMacPathKind::Bundled;
    d.signature = GdbMacSignatureState::ValidAdhoc;
    d.hasDebuggerEntitlement = true;
    d.session.bareNoStack = true;
    const wxString s = FormatGdbMacSigningDiagnosisForLog(d);
    CHECK(s.Contains(wxT("would offer Enable debugging")));
    CHECK(s.Contains(wxT("bare \"No stack\" alone")));
}

TEST(Tier1DecideSkipWhenValid)
{
    GdbMacSigningDiagnosis d;
    d.pathKind = GdbMacPathKind::Bundled;
    d.signature = GdbMacSignatureState::ValidAdhoc;
    d.hasDebuggerEntitlement = true;
    CHECK_EQUAL(GdbMacTier1Decision::SkipAlreadyValid, DecideMacGdbTier1(d));
}

TEST(Tier1DecideRestoreWhenNoEntitlement)
{
    GdbMacSigningDiagnosis d;
    d.pathKind = GdbMacPathKind::Bundled;
    d.signature = GdbMacSignatureState::ValidAdhoc;
    d.hasDebuggerEntitlement = false;
    CHECK_EQUAL(GdbMacTier1Decision::ShouldRestore, DecideMacGdbTier1(d));
}

TEST(Tier1DecideRestoreWhenUnsigned)
{
    GdbMacSigningDiagnosis d;
    d.pathKind = GdbMacPathKind::Bundled;
    d.signature = GdbMacSignatureState::MissingOrInvalid;
    CHECK_EQUAL(GdbMacTier1Decision::ShouldRestore, DecideMacGdbTier1(d));
}

TEST(Tier1DecideRefuseNotBundled)
{
    GdbMacSigningDiagnosis d;
    d.pathKind = GdbMacPathKind::NotBundled;
    d.signature = GdbMacSignatureState::MissingOrInvalid;
    CHECK_EQUAL(GdbMacTier1Decision::RefuseNotBundled, DecideMacGdbTier1(d));
}

TEST(Tier1DecideRefuseMissing)
{
    GdbMacSigningDiagnosis d;
    d.pathKind = GdbMacPathKind::Missing;
    CHECK_EQUAL(GdbMacTier1Decision::RefuseMissing, DecideMacGdbTier1(d));
}

TEST(Tier2DecideSkipNotNeededWhenHealthy)
{
    GdbMacSigningDiagnosis d;
    d.pathKind = GdbMacPathKind::Bundled;
    d.signature = GdbMacSignatureState::ValidAdhoc;
    d.hasDebuggerEntitlement = true;
    CHECK_EQUAL(GdbMacTier2Decision::SkipNotNeeded, DecideMacGdbTier2(d, false));
}

TEST(Tier2DecideShouldRunWhenForce)
{
    GdbMacSigningDiagnosis d;
    d.pathKind = GdbMacPathKind::Bundled;
    d.signature = GdbMacSignatureState::ValidAdhoc;
    d.hasDebuggerEntitlement = true;
    CHECK_EQUAL(GdbMacTier2Decision::ShouldRun, DecideMacGdbTier2(d, true));
}

TEST(Tier2DecideShouldRunWhenTaskPort)
{
    GdbMacSigningDiagnosis d;
    d.pathKind = GdbMacPathKind::Bundled;
    d.signature = GdbMacSignatureState::ValidAdhoc;
    d.hasDebuggerEntitlement = true;
    d.session.taskPortOrTaskgated = true;
    CHECK_EQUAL(GdbMacTier2Decision::ShouldRun, DecideMacGdbTier2(d, false));
}

TEST(Tier2DecideRefuseIfStillDefective)
{
    GdbMacSigningDiagnosis d;
    d.pathKind = GdbMacPathKind::Bundled;
    d.signature = GdbMacSignatureState::MissingOrInvalid;
    CHECK_EQUAL(GdbMacTier2Decision::RefuseSigningStillDefective, DecideMacGdbTier2(d, true));
}

TEST(Tier2DecideSkipAlreadyNamed)
{
    GdbMacSigningDiagnosis d;
    d.pathKind = GdbMacPathKind::Bundled;
    d.signature = GdbMacSignatureState::ValidNamed;
    d.hasDebuggerEntitlement = true;
    d.authority = wxT("codeblocks-gdb");
    CHECK_EQUAL(GdbMacTier2Decision::SkipAlreadyNamed, DecideMacGdbTier2(d, false));
    CHECK_EQUAL(GdbMacTier2Decision::ShouldRun, DecideMacGdbTier2(d, true));
}

} // SUITE
