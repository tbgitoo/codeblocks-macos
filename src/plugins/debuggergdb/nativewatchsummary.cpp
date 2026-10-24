/*
 * This file is part of the Code::Blocks IDE and licensed under the GNU General Public License, version 3
 * http://www.gnu.org/licenses/gpl-3.0.html
 *
 * Contained std::string ABI knowledge for watch summaries.
 * Apple libc++ (__rep_ / __data_) and MinGW libstdc++ (_M_dataplus / _M_p).
 * Keep those layout names in this translation unit only.
 */

#include "sdk.h"
#ifndef CB_PRECOMP
    #include <wx/string.h>
#endif

#include "nativewatchsummary.h"

namespace
{

wxString ChildSymbol(const cb::shared_ptr<cbWatch> &child)
{
    wxString symbol;
    if (child)
        child->GetSymbol(symbol);
    return symbol;
}

wxString ChildValue(const cb::shared_ptr<cbWatch> &child)
{
    wxString value;
    if (child)
        child->GetValue(value);
    return value;
}

cb::shared_ptr<cbWatch> FindDirectChild(const cb::shared_ptr<cbWatch> &parent, const wxString &name)
{
    if (!parent)
        return cb::shared_ptr<cbWatch>();
    return parent->FindChild(name);
}

/** Search direct children, then one level into anonymous `[n]` / unnamed nodes. */
cb::shared_ptr<cbWatch> FindNamedDescendant(const cb::shared_ptr<cbWatch> &root, const wxString &name)
{
    if (!root)
        return cb::shared_ptr<cbWatch>();

    cb::shared_ptr<cbWatch> direct = FindDirectChild(root, name);
    if (direct)
        return direct;

    for (int i = 0; i < root->GetChildCount(); ++i)
    {
        cb::shared_ptr<cbWatch> child = root->GetChild(i);
        const wxString symbol = ChildSymbol(child);
        if (symbol.StartsWith(wxT("[")) || symbol.empty())
        {
            cb::shared_ptr<cbWatch> nested = FindDirectChild(child, name);
            if (nested)
                return nested;
        }
    }
    return cb::shared_ptr<cbWatch>();
}

bool IsAppleLibcxxStringType(const wxString &type)
{
    wxString t = type;
    t.Trim(true);
    t.Trim(false);
    if (t.empty())
        return false;

    // Strip common reference / const decorations used by GDB.
    while (t.EndsWith(wxT("&")) || t.EndsWith(wxT("*")))
    {
        t.RemoveLast();
        t.Trim(true);
    }
    if (t.StartsWith(wxT("const ")))
        t = t.Mid(6);
    t.Trim(true);
    t.Trim(false);

    if (t == wxT("std::__1::string") || t == wxT("std::string"))
        return true;

    if (t.StartsWith(wxT("std::__1::basic_string<char")))
        return true;
    if (t.StartsWith(wxT("std::basic_string<char")))
        return true;

    return false;
}

bool HasAppleLibcxxStringFingerprint(const cb::shared_ptr<GDBWatch> &watch)
{
    // Unique enough for Apple libc++ basic_string dumps observed with bundled GDB.
    const bool hasEndian =
        FindNamedDescendant(watch, wxT("static __endian_factor"))
        || FindNamedDescendant(watch, wxT("__endian_factor"));
    const bool hasNpos =
        FindNamedDescendant(watch, wxT("static npos"))
        || FindNamedDescendant(watch, wxT("npos"));
    const bool hasRep = static_cast<bool>(FindNamedDescendant(watch, wxT("__rep_")));
    return hasEndian && hasNpos && hasRep;
}

bool TypeAllowsAppleLibcxxString(const cb::shared_ptr<GDBWatch> &watch)
{
    wxString type;
    watch->GetType(type);
    type.Trim(true);
    type.Trim(false);

    if (!type.empty())
        return IsAppleLibcxxStringType(type);

    // Locals from "info locals" often have no type string; require the full ABI fingerprint.
    return HasAppleLibcxxStringFingerprint(watch);
}

bool ParseLeadingInteger(const wxString &raw, unsigned long long &out)
{
    wxString s = raw;
    s.Trim(true);
    s.Trim(false);
    if (s.empty())
        return false;

    // Forms: "8", "8 '\b'", "0 '\000'", "23 '\027'"
    size_t i = 0;
    while (i < s.length() && (s[i] == wxT(' ') || s[i] == wxT('\t')))
        ++i;
    if (i >= s.length() || !wxIsdigit(s[i]))
        return false;

    unsigned long long value = 0;
    while (i < s.length() && wxIsdigit(s[i]))
    {
        value = value * 10ull + static_cast<unsigned long long>(s[i] - wxT('0'));
        ++i;
    }
    out = value;
    return true;
}

bool ParseIsLongFlag(const wxString &raw, bool &isLong)
{
    wxString s = raw;
    s.Trim(true);
    s.Trim(false);
    if (s.empty())
        return false;

    if (s.CmpNoCase(wxT("true")) == 0 || s == wxT("1") || s.StartsWith(wxT("1 ")))
    {
        isLong = true;
        return true;
    }
    if (s.CmpNoCase(wxT("false")) == 0 || s == wxT("0") || s.StartsWith(wxT("0 ")))
    {
        isLong = false;
        return true;
    }

    unsigned long long n = 0;
    if (!ParseLeadingInteger(s, n))
        return false;
    isLong = (n != 0);
    return true;
}

/** Decode one GDB/C string escape starting at *pos (which points at '\\'). Advances *pos past it. */
bool DecodeEscape(const wxString &src, size_t &pos, wxChar &out)
{
    if (pos >= src.length() || src[pos] != wxT('\\'))
        return false;
    ++pos;
    if (pos >= src.length())
        return false;

    const wxChar c = src[pos++];
    switch (c)
    {
        case wxT('a'): out = wxT('\a'); return true;
        case wxT('b'): out = wxT('\b'); return true;
        case wxT('f'): out = wxT('\f'); return true;
        case wxT('n'): out = wxT('\n'); return true;
        case wxT('r'): out = wxT('\r'); return true;
        case wxT('t'): out = wxT('\t'); return true;
        case wxT('v'): out = wxT('\v'); return true;
        case wxT('\\'): out = wxT('\\'); return true;
        case wxT('\''): out = wxT('\''); return true;
        case wxT('"'): out = wxT('"'); return true;
        case wxT('0'):
        case wxT('1'):
        case wxT('2'):
        case wxT('3'):
        case wxT('4'):
        case wxT('5'):
        case wxT('6'):
        case wxT('7'):
        {
            --pos; // re-read first digit
            unsigned value = 0;
            int digits = 0;
            while (digits < 3 && pos < src.length()
                   && src[pos] >= wxT('0') && src[pos] <= wxT('7'))
            {
                value = (value << 3) + static_cast<unsigned>(src[pos] - wxT('0'));
                ++pos;
                ++digits;
            }
            out = static_cast<wxChar>(value & 0xff);
            return true;
        }
        case wxT('x'):
        {
            if (pos >= src.length())
                return false;
            unsigned value = 0;
            int digits = 0;
            while (digits < 2 && pos < src.length() && wxIsxdigit(src[pos]))
            {
                const wxChar h = src[pos++];
                value <<= 4;
                if (h >= wxT('0') && h <= wxT('9'))
                    value += static_cast<unsigned>(h - wxT('0'));
                else if (h >= wxT('a') && h <= wxT('f'))
                    value += 10u + static_cast<unsigned>(h - wxT('a'));
                else
                    value += 10u + static_cast<unsigned>(h - wxT('A'));
                ++digits;
            }
            if (digits == 0)
                return false;
            out = static_cast<wxChar>(value & 0xff);
            return true;
        }
        default:
            out = c;
            return true;
    }
}

bool ParseOneGdbCharLiteral(const wxString &src, size_t &pos, wxChar &out)
{
    while (pos < src.length() && (src[pos] == wxT(' ') || src[pos] == wxT('\t')))
        ++pos;
    if (pos >= src.length() || src[pos] != wxT('\''))
        return false;
    ++pos;
    if (pos >= src.length())
        return false;

    if (src[pos] == wxT('\\'))
    {
        if (!DecodeEscape(src, pos, out))
            return false;
    }
    else
    {
        out = src[pos++];
    }

    if (pos >= src.length() || src[pos] != wxT('\''))
        return false;
    ++pos;
    return true;
}

/** GDB prints a run of the same character as `'x' <repeats 15 times>`. Count is the total. */
bool ParseGdbRepeatCount(const wxString &src, size_t &pos, unsigned long long &count)
{
    const size_t save = pos;
    while (pos < src.length() && (src[pos] == wxT(' ') || src[pos] == wxT('\t')))
        ++pos;

    const wxString marker = wxT("<repeats ");
    if (src.Mid(pos, marker.length()) != marker)
    {
        pos = save;
        count = 1;
        return true;
    }

    pos += marker.length();
    if (pos >= src.length() || !wxIsdigit(src[pos]))
    {
        pos = save;
        return false;
    }

    unsigned long long n = 0;
    while (pos < src.length() && wxIsdigit(src[pos]))
    {
        n = n * 10ull + static_cast<unsigned long long>(src[pos] - wxT('0'));
        ++pos;
    }

    const wxString tail = wxT(" times>");
    if (n == 0 || src.Mid(pos, tail.length()) != tail)
    {
        pos = save;
        return false;
    }
    pos += tail.length();
    count = n;
    return true;
}

/** `'x' <repeats N times>` sequences. A repeat longer than the string is accepted only as NUL padding. */
bool ExtractPayloadFromGdbRepeats(const wxString &src, size_t pos, size_t expectedLen, wxString &payload)
{
    payload.clear();
    payload.reserve(expectedLen);

    while (payload.length() < expectedLen)
    {
        wxChar c = 0;
        if (!ParseOneGdbCharLiteral(src, pos, c))
            return false;

        unsigned long long count = 1;
        if (!ParseGdbRepeatCount(src, pos, count))
            return false;

        const unsigned long long need = static_cast<unsigned long long>(expectedLen - payload.length());
        const unsigned long long take = count < need ? count : need;
        for (unsigned long long i = 0; i < take; ++i)
            payload.Append(c);

        if (take < count)
        {
            if (c != wxT('\0'))
                return false;
            break;
        }

        while (pos < src.length() && (src[pos] == wxT(' ') || src[pos] == wxT('\t')))
            ++pos;
        if (pos < src.length() && src[pos] == wxT(','))
        {
            ++pos;
            continue;
        }
        break;
    }

    return payload.length() == expectedLen;
}

/** Extract character payload from a GDB data field; require exactly expectedLen decoded chars. */
bool ExtractPayloadFromGdbDataField(const wxString &rawField, size_t expectedLen, wxString &payload)
{
    wxString s = rawField;
    s.Trim(true);
    s.Trim(false);
    if (s.empty())
        return false;

    size_t pos = 0;
    if (s.length() > 2 && s[0] == wxT('0') && (s[1] == wxT('x') || s[1] == wxT('X')))
    {
        pos = 2;
        while (pos < s.length() && wxIsxdigit(s[pos]))
            ++pos;
        while (pos < s.length() && (s[pos] == wxT(' ') || s[pos] == wxT('\t')))
            ++pos;
    }

    if (pos < s.length() && s[pos] == wxT('\''))
        return ExtractPayloadFromGdbRepeats(s, pos, expectedLen, payload);

    if (pos >= s.length() || s[pos] != wxT('"'))
        return false;
    ++pos;

    payload.clear();
    payload.reserve(expectedLen);

    while (pos < s.length() && payload.length() < expectedLen)
    {
        const wxChar c = s[pos];
        if (c == wxT('"'))
            break;
        if (c == wxT('\\'))
        {
            wxChar decoded = 0;
            if (!DecodeEscape(s, pos, decoded))
                return false;
            payload.Append(decoded);
            continue;
        }
        payload.Append(c);
        ++pos;
    }

    if (payload.length() != expectedLen)
        return false;

    return true;
}

wxString FormatWatchStringValue(const wxString &payload)
{
    wxString out;
    out.Alloc(payload.length() + 2);
    out.Append(wxT('"'));
    for (size_t i = 0; i < payload.length(); ++i)
    {
        const wxChar c = payload[i];
        switch (c)
        {
            case wxT('\\'): out += wxT("\\\\"); break;
            case wxT('"'):  out += wxT("\\\""); break;
            case wxT('\0'): out += wxT("\\0"); break;
            case wxT('\n'): out += wxT("\\n"); break;
            case wxT('\r'): out += wxT("\\r"); break;
            case wxT('\t'): out += wxT("\\t"); break;
            default:
                if (c < 32 || c == 127)
                    out += wxString::Format(wxT("\\%03o"), static_cast<unsigned>(c) & 0xffu);
                else
                    out.Append(c);
                break;
        }
    }
    out.Append(wxT('"'));
    return out;
}

bool TryAppleLibcxxStringSummary(const cb::shared_ptr<GDBWatch> &watch, wxString &summary)
{
    if (!TypeAllowsAppleLibcxxString(watch))
        return false;

    cb::shared_ptr<cbWatch> rep = FindNamedDescendant(watch, wxT("__rep_"));
    if (!rep)
        return false;

    cb::shared_ptr<cbWatch> shortRep = FindDirectChild(rep, wxT("__s"));
    cb::shared_ptr<cbWatch> longRep = FindDirectChild(rep, wxT("__l"));
    if (!shortRep || !longRep)
        return false;

    cb::shared_ptr<cbWatch> shortIsLongNode = FindDirectChild(shortRep, wxT("__is_long_"));
    if (!shortIsLongNode)
        return false;

    bool isLong = false;
    if (!ParseIsLongFlag(ChildValue(shortIsLongNode), isLong))
        return false;

    cb::shared_ptr<cbWatch> active = isLong ? longRep : shortRep;
    cb::shared_ptr<cbWatch> sizeNode = FindDirectChild(active, wxT("__size_"));
    cb::shared_ptr<cbWatch> dataNode = FindDirectChild(active, wxT("__data_"));
    if (!sizeNode || !dataNode)
        return false;

    unsigned long long size = 0;
    if (!ParseLeadingInteger(ChildValue(sizeNode), size))
        return false;

    if (size > 64ull * 1024ull * 1024ull)
        return false;

    wxString payload;
    if (!ExtractPayloadFromGdbDataField(ChildValue(dataNode), static_cast<size_t>(size), payload))
        return false;

    summary = FormatWatchStringValue(payload);
    return true;
}

wxString StripTypeDecorations(wxString type)
{
    type.Trim(true);
    type.Trim(false);
    while (type.EndsWith(wxT("&")) || type.EndsWith(wxT("*")))
    {
        type.RemoveLast();
        type.Trim(true);
    }
    if (type.StartsWith(wxT("const ")))
        type = type.Mid(6);
    type.Trim(true);
    type.Trim(false);
    return type;
}

bool IsLibstdcxxCharStringType(const wxString &type)
{
    const wxString t = StripTypeDecorations(type);
    if (t.empty())
        return false;
    if (t == wxT("std::string") || t == wxT("std::__cxx11::string"))
        return true;
    if (t.StartsWith(wxT("std::basic_string<char")))
        return true;
    if (t.StartsWith(wxT("std::__cxx11::basic_string<char")))
        return true;
    return false;
}

cb::shared_ptr<cbWatch> FindLibstdcxxPointer(const cb::shared_ptr<GDBWatch> &watch)
{
    cb::shared_ptr<cbWatch> dataplus = FindDirectChild(watch, wxT("_M_dataplus"));
    if (!dataplus)
        dataplus = FindNamedDescendant(watch, wxT("_M_dataplus"));
    if (!dataplus)
        return cb::shared_ptr<cbWatch>();

    cb::shared_ptr<cbWatch> pointer = FindDirectChild(dataplus, wxT("_M_p"));
    if (!pointer)
        pointer = FindNamedDescendant(dataplus, wxT("_M_p"));
    return pointer;
}

bool HasLibstdcxxStringFingerprint(const cb::shared_ptr<GDBWatch> &watch)
{
    if (!FindLibstdcxxPointer(watch))
        return false;
    if (!FindNamedDescendant(watch, wxT("_M_string_length")))
        return false;
    return static_cast<bool>(FindNamedDescendant(watch, wxT("_M_local_buf")))
        || static_cast<bool>(FindNamedDescendant(watch, wxT("_M_allocated_capacity")));
}

bool TypeAllowsLibstdcxxString(const cb::shared_ptr<GDBWatch> &watch)
{
    wxString type;
    watch->GetType(type);
    type.Trim(true);
    type.Trim(false);
    if (!type.empty())
        return IsLibstdcxxCharStringType(type);
    return HasLibstdcxxStringFingerprint(watch);
}

/** MinGW / libstdc++ cxx11 basic_string<char>. Local buffer holds at most 15 characters. */
bool TryLibstdcxxStringSummary(const cb::shared_ptr<GDBWatch> &watch, wxString &summary)
{
    if (!TypeAllowsLibstdcxxString(watch))
        return false;

    cb::shared_ptr<cbWatch> pointer = FindLibstdcxxPointer(watch);
    cb::shared_ptr<cbWatch> lengthNode = FindNamedDescendant(watch, wxT("_M_string_length"));
    if (!pointer || !lengthNode)
        return false;

    unsigned long long size = 0;
    if (!ParseLeadingInteger(ChildValue(lengthNode), size))
        return false;
    if (size > 64ull * 1024ull * 1024ull)
        return false;

    if (size == 0)
    {
        summary = wxT("\"\"");
        return true;
    }

    // _S_local_capacity for char. Longer strings live on the heap, at _M_p.
    const unsigned long long localCapacity = 15;
    cb::shared_ptr<cbWatch> localBuf = FindNamedDescendant(watch, wxT("_M_local_buf"));

    wxString payload;
    bool got = false;
    if (size <= localCapacity && localBuf)
        got = ExtractPayloadFromGdbDataField(ChildValue(localBuf), static_cast<size_t>(size), payload);
    if (!got)
        got = ExtractPayloadFromGdbDataField(ChildValue(pointer), static_cast<size_t>(size), payload);
    if (!got)
        return false;

    summary = FormatWatchStringValue(payload);
    return true;
}

} // namespace

bool TryNativeWatchSummary(cb::shared_ptr<GDBWatch> watch)
{
    if (!watch)
        return false;

    // Already has a non-empty parent value (e.g. pretty-printer prefix) — leave alone.
    wxString existing;
    watch->GetValue(existing);
    if (!existing.empty())
        return false;

    wxString summary;
    if (!TryAppleLibcxxStringSummary(watch, summary)
        && !TryLibstdcxxStringSummary(watch, summary))
        return false;

    watch->SetValue(summary);
    return true;
}
