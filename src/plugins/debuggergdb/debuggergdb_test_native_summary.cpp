#include "debuggergdb_test_common.h"
#include "parsewatchvalue.h"
#include "nativewatchsummary.h"

namespace
{

/** Apple libc++ short-string GDB dump (ALTERNATE_STRING_LAYOUT), synthetic but ABI-faithful. */
wxString MakeShortLibcxxDump(const wxString &gdbDataField, const wxString &sizeField,
                             const wxString &isLongField = wxT("0 '\\000'"))
{
    return wxT("{static __endian_factor = 1, {__rep_ = {__s = {__data_ = ")
         + gdbDataField
         + wxT(", __size_ = ") + sizeField
         + wxT(", __is_long_ = ") + isLongField
         + wxT("}, __l = {__data_ = 0x0, __size_ = 0, __cap_ = 0, __is_long_ = 0}}, __alloc_ = {}}, ")
           wxT("static npos = 18446744073709551615}");
}

wxString MakeLongLibcxxDump(const wxString &gdbDataField, const wxString &sizeField)
{
    return wxT("{static __endian_factor = 1, {__rep_ = {__s = {__data_ = \"\", __size_ = 0 '\\000', ")
           wxT("__is_long_ = 1 '\\001'}, __l = {__data_ = ")
         + gdbDataField
         + wxT(", __size_ = ") + sizeField
         + wxT(", __cap_ = 47, __is_long_ = 1}}, __alloc_ = {}}, ")
           wxT("static npos = 18446744073709551615}");
}

wxString ParentValue(const cb::shared_ptr<GDBWatch> &w)
{
    wxString v;
    w->GetValue(v);
    return v;
}

} // namespace

SUITE(NativeWatchSummaryLibcxxString)
{

TEST(OrdinaryShortString_W1)
{
    const wxString dump = MakeShortLibcxxDump(
        wxT("\"12583934\\000\\000\\000\\000\\000\\000\\000\\000\\000\\000\\000\\000\\000\\000\\000\""),
        wxT("8 '\\b'"));
    cb::shared_ptr<GDBWatch> w(new GDBWatch(wxT("a")));
    w->SetType(wxT("std::__1::string"));
    CHECK(ParseGDBWatchValue(w, dump));
    CHECK(TryNativeWatchSummary(w));
    CHECK_EQUAL(wxT("\"12583934\""), ParentValue(w));
    CHECK(w->GetChildCount() > 0); // raw children preserved
}

TEST(EmptyString_W2)
{
    const wxString dump = MakeShortLibcxxDump(
        wxT("\"\\000\\000\\000\\000\\000\\000\\000\\000\\000\\000\\000\\000\\000\\000\\000\\000\\000\\000\\000\\000\\000\\000\\000\""),
        wxT("0 '\\000'"));
    cb::shared_ptr<GDBWatch> w(new GDBWatch(wxT("a")));
    CHECK(ParseGDBWatchValue(w, dump));
    CHECK(TryNativeWatchSummary(w)); // fingerprint path (no type)
    CHECK_EQUAL(wxT("\"\""), ParentValue(w));
}

TEST(Spaces_W3)
{
    const wxString dump = MakeShortLibcxxDump(wxT("\"hello world\\000\\000\\000\\000\\000\\000\\000\\000\\000\\000\\000\""),
                                             wxT("11 '\\v'"));
    cb::shared_ptr<GDBWatch> w(new GDBWatch(wxT("a")));
    w->SetType(wxT("std::__1::string"));
    CHECK(ParseGDBWatchValue(w, dump));
    CHECK(TryNativeWatchSummary(w));
    CHECK_EQUAL(wxT("\"hello world\""), ParentValue(w));
}

TEST(VeryShortSSO_W11)
{
    const wxString dump = MakeShortLibcxxDump(wxT("\"a\\000\\000\\000\\000\\000\\000\\000\\000\\000\\000\\000\\000\\000\\000\\000\\000\\000\\000\\000\\000\\000\\000\""),
                                             wxT("1 '\\001'"));
    cb::shared_ptr<GDBWatch> w(new GDBWatch(wxT("a")));
    w->SetType(wxT("std::__1::string"));
    CHECK(ParseGDBWatchValue(w, dump));
    CHECK(TryNativeWatchSummary(w));
    CHECK_EQUAL(wxT("\"a\""), ParentValue(w));
}

TEST(SSOBoundary22_W12)
{
    wxString data = wxT("\"");
    for (int i = 0; i < 22; ++i)
        data += wxT('x');
    data += wxT("\\000\"");
    const wxString dump = MakeShortLibcxxDump(data, wxT("22 '\\026'"));
    cb::shared_ptr<GDBWatch> w(new GDBWatch(wxT("a")));
    w->SetType(wxT("std::__1::string"));
    CHECK(ParseGDBWatchValue(w, dump));
    CHECK(TryNativeWatchSummary(w));
    CHECK_EQUAL(wxT("\"xxxxxxxxxxxxxxxxxxxxxx\""), ParentValue(w));
}

TEST(LongHeap23_W13)
{
    wxString content;
    for (int i = 0; i < 23; ++i)
        content += wxT('y');
    const wxString dump = MakeLongLibcxxDump(wxT("0x1000 \"") + content + wxT("\""), wxT("23"));
    cb::shared_ptr<GDBWatch> w(new GDBWatch(wxT("a")));
    w->SetType(wxT("std::__1::string"));
    CHECK(ParseGDBWatchValue(w, dump));
    CHECK(TryNativeWatchSummary(w));
    CHECK_EQUAL(wxT("\"yyyyyyyyyyyyyyyyyyyyyyy\""), ParentValue(w));
}

TEST(EmbeddedNul_W14)
{
    // Seven characters: a b c NUL d e f — must not truncate to "abc"
    const wxString dump = MakeShortLibcxxDump(
        wxT("\"abc\\000def\\000\\000\\000\\000\\000\\000\\000\\000\\000\\000\\000\\000\\000\\000\\000\\000\""),
        wxT("7 '\\a'"));
    cb::shared_ptr<GDBWatch> w(new GDBWatch(wxT("a")));
    w->SetType(wxT("std::__1::string"));
    CHECK(ParseGDBWatchValue(w, dump));
    CHECK(TryNativeWatchSummary(w));
    CHECK_EQUAL(wxT("\"abc\\0def\""), ParentValue(w));
}

TEST(DeclineUnrecognisedLayout_W7)
{
    cb::shared_ptr<GDBWatch> w(new GDBWatch(wxT("a")));
    w->SetType(wxT("std::__1::string"));
    CHECK(ParseGDBWatchValue(w, wxT("{x = 1, y = 2}")));
    CHECK(!TryNativeWatchSummary(w));
    CHECK(ParentValue(w).empty());
    CHECK(w->GetChildCount() > 0);
}

TEST(DeclineWrongType)
{
    const wxString dump = MakeShortLibcxxDump(wxT("\"hello\\000\""), wxT("5"));
    cb::shared_ptr<GDBWatch> w(new GDBWatch(wxT("a")));
    w->SetType(wxT("int"));
    CHECK(ParseGDBWatchValue(w, dump));
    CHECK(!TryNativeWatchSummary(w));
}

TEST(ScalarsUntouched_W6)
{
    cb::shared_ptr<GDBWatch> w(new GDBWatch(wxT("i")));
    w->SetType(wxT("int"));
    CHECK(ParseGDBWatchValue(w, wxT("42")));
    CHECK(!TryNativeWatchSummary(w));
    CHECK_EQUAL(wxT("42"), ParentValue(w));
}

TEST(UnrelatedAggregateUntouched)
{
    cb::shared_ptr<GDBWatch> w(new GDBWatch(wxT("p")));
    CHECK(ParseGDBWatchValue(w, wxT("{a = 5, b = 7}")));
    CHECK(!TryNativeWatchSummary(w));
    CHECK(ParentValue(w).empty());
}

TEST(TruncatedLongDataDeclines)
{
    // size claims 23 but GDB only printed 5 characters
    const wxString dump = MakeLongLibcxxDump(wxT("0x1000 \"yyyyy\""), wxT("23"));
    cb::shared_ptr<GDBWatch> w(new GDBWatch(wxT("a")));
    w->SetType(wxT("std::__1::string"));
    CHECK(ParseGDBWatchValue(w, dump));
    CHECK(!TryNativeWatchSummary(w));
}

TEST(BasicStringCharTypeAccepted)
{
    const wxString dump = MakeShortLibcxxDump(wxT("\"hi\\000\""), wxT("2"));
    cb::shared_ptr<GDBWatch> w(new GDBWatch(wxT("a")));
    w->SetType(wxT("std::__1::basic_string<char, std::__1::char_traits<char>, std::__1::allocator<char> >"));
    CHECK(ParseGDBWatchValue(w, dump));
    CHECK(TryNativeWatchSummary(w));
    CHECK_EQUAL(wxT("\"hi\""), ParentValue(w));
}

} // SUITE

/** MinGW libstdc++ cxx11 dumps, captured from GDB 16.2 / GCC 14.2. */
SUITE(NativeWatchSummaryLibstdcxxString)
{

wxString LibstdcxxDump(const wxString &pointerField, const wxString &lengthField,
                       const wxString &localField)
{
    return wxT("{_M_dataplus = {<std::allocator<char>> = {<std::__new_allocator<char>> = {<No data fields>}, <No data fields>}, _M_p = ")
         + pointerField
         + wxT("}, _M_string_length = ") + lengthField
         + wxT(", {_M_local_buf = ") + localField
         + wxT(", _M_allocated_capacity = 16}}");
}

TEST(ShortStringShownOnParent)
{
    const wxString dump = LibstdcxxDump(wxT("0x5ffe40 \"19403\""), wxT("5"),
                                        wxT("\"19403\\000\\000\\000\\000\\000\\000\\000\\000\\000\\000\""));
    cb::shared_ptr<GDBWatch> w(new GDBWatch(wxT("a")));
    CHECK(ParseGDBWatchValue(w, dump));
    CHECK(TryNativeWatchSummary(w));
    CHECK_EQUAL(wxT("\"19403\""), ParentValue(w));
    CHECK(w->GetChildCount() > 0);
}

TEST(EmptyString)
{
    const wxString dump = LibstdcxxDump(wxT("0x5ffe20 \"\""), wxT("0"),
                                        wxT("\"\\000\""));
    cb::shared_ptr<GDBWatch> w(new GDBWatch(wxT("a")));
    w->SetType(wxT("std::string"));
    CHECK(ParseGDBWatchValue(w, dump));
    CHECK(TryNativeWatchSummary(w));
    CHECK_EQUAL(wxT("\"\""), ParentValue(w));
}

TEST(LocalRepeats)
{
    const wxString dump = LibstdcxxDump(wxT("0x5ffde0 'x' <repeats 15 times>"), wxT("15"),
                                        wxT("'x' <repeats 15 times>"));
    cb::shared_ptr<GDBWatch> w(new GDBWatch(wxT("a")));
    w->SetType(wxT("std::string"));
    CHECK(ParseGDBWatchValue(w, dump));
    CHECK(TryNativeWatchSummary(w));
    CHECK_EQUAL(wxT("\"xxxxxxxxxxxxxxx\""), ParentValue(w));
}

TEST(HeapRepeats)
{
    const wxString dump = LibstdcxxDump(wxT("0xd77eb0 'y' <repeats 16 times>"), wxT("16"),
                                        wxT("\"\\020\\000\""));
    cb::shared_ptr<GDBWatch> w(new GDBWatch(wxT("a")));
    w->SetType(wxT("std::__cxx11::basic_string<char, std::char_traits<char>, std::allocator<char> >"));
    CHECK(ParseGDBWatchValue(w, dump));
    CHECK(TryNativeWatchSummary(w));
    wxString expect = wxT("\"");
    expect.Append(wxT('y'), 16);
    expect += wxT("\"");
    CHECK_EQUAL(expect, ParentValue(w));
}

TEST(EmbeddedNulUsesLocalBuffer)
{
    const wxString dump = LibstdcxxDump(wxT("0x5ffda0 \"abc\""), wxT("7"),
                                        wxT("\"abc\\000def\\000\""));
    cb::shared_ptr<GDBWatch> w(new GDBWatch(wxT("a")));
    CHECK(ParseGDBWatchValue(w, dump));
    CHECK(TryNativeWatchSummary(w));
    CHECK_EQUAL(wxT("\"abc\\0def\""), ParentValue(w));
}

TEST(TruncatedHeapDeclines)
{
    const wxString dump = LibstdcxxDump(wxT("0xd77eb0 \"yyyyy\""), wxT("16"),
                                        wxT("\"\\020\\000\""));
    cb::shared_ptr<GDBWatch> w(new GDBWatch(wxT("a")));
    w->SetType(wxT("std::string"));
    CHECK(ParseGDBWatchValue(w, dump));
    CHECK(!TryNativeWatchSummary(w));
    CHECK(ParentValue(w).empty());
}

TEST(WrongTypeDeclines)
{
    const wxString dump = LibstdcxxDump(wxT("0x5ffe40 \"19403\""), wxT("5"),
                                        wxT("\"19403\\000\""));
    cb::shared_ptr<GDBWatch> w(new GDBWatch(wxT("a")));
    w->SetType(wxT("int"));
    CHECK(ParseGDBWatchValue(w, dump));
    CHECK(!TryNativeWatchSummary(w));
}

} // SUITE
