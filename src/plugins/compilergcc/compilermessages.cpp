/*
 * This file is part of the Code::Blocks IDE and licensed under the GNU General Public License, version 3
 * http://www.gnu.org/licenses/gpl-3.0.html
 *
 * $Revision: 14024 $
 * $Id: compilermessages.cpp 14024 2026-09-15 16:10:18Z wh11204 $
 * $HeadURL: https://svn.code.sf.net/p/codeblocks/code/trunk/src/plugins/compilergcc/compilermessages.cpp $
 */

#include "sdk.h"
#ifndef CB_PRECOMP
    #include <wx/intl.h>
    #include <wx/listctrl.h>
    #include <wx/menu.h>
    #include <wx/string.h>
    #include <wx/utils.h>
    #include "cbeditor.h"
    #include "configmanager.h"
    #include "editorcolourset.h"
    #include "editormanager.h"
    #include "globals.h"
    #include "manager.h"
#endif
#include <wx/clipbrd.h>
#include <wx/dataobj.h>
#include "compilererrors.h"
#include "compilermessages.h"
#include "prep.h"

namespace
{
    int idList = wxNewId();
    int idMenuFit = wxNewId();
    int idMenuAutoFit = wxNewId();
    int idMenuStackOverflow = wxNewId();

    bool IsEdgePunct(wxChar c)
    {
        switch (c)
        {
            case '\'': case '"': case '`':
            case '(': case ')': case '[': case ']': case '{': case '}':
            case ',': case ';': case ':': case '.':
                return true;
            default:
                return false;
        }
    }

    wxString TrimSearchWord(const wxString& word)
    {
        size_t begin = 0;
        size_t end = word.length();
        while (begin < end && IsEdgePunct(word[begin]))
            ++begin;
        while (end > begin && IsEdgePunct(word[end - 1]))
            --end;
        return word.Mid(begin, end - begin);
    }

    bool AtTokenStart(const wxString& text, size_t i)
    {
        if (i == 0)
            return true;
        const wxChar prev = text[i - 1];
        return wxIsspace(prev) || prev == '\'' || prev == '"' || prev == '(' || prev == '<';
    }

    bool StartsAbsolutePath(const wxString& text, size_t i)
    {
        if (text[i] == '/' || text[i] == '\\')
            return true;
        if (text[i] == '~' && (i + 1 >= text.length() || text[i + 1] == '/' || text[i + 1] == '\\'))
            return true;
        if (i + 2 < text.length() && wxIsalpha(text[i]) && text[i + 1] == ':'
                && (text[i + 2] == '\\' || text[i + 2] == '/'))
            return true;
        return false;
    }

    // file.cpp:12 or file.cpp:12:3, not a bare header name such as string.h.
    bool IsFileLocation(const wxString& token)
    {
        const wxString lower = token.Lower();
        const wxChar* exts[] = {
            wxT(".cpp"), wxT(".cxx"), wxT(".cc"), wxT(".c"), wxT(".hpp"), wxT(".hxx"),
            wxT(".hh"), wxT(".h"), wxT(".ipp"), wxT(".inl"), wxT(".mm"), wxT(".m"),
            wxT(".tcc"), nullptr
        };
        for (int e = 0; exts[e]; ++e)
        {
            const wxString ext(exts[e]);
            int pos = 0;
            while (pos < (int)lower.length())
            {
                const int found = lower.Mid(pos).Find(ext);
                if (found == wxNOT_FOUND)
                    break;
                const size_t after = (size_t)pos + (size_t)found + ext.length();
                pos = (int)after;
                if (after >= lower.length())
                    break;
                if ((lower[after] == ':' || lower[after] == '(') && after + 1 < lower.length()
                        && wxIsdigit(lower[after + 1]))
                    return true;
            }
        }
        return false;
    }

    bool SegmentContinuesPath(const wxString& segment)
    {
        if (segment.empty())
            return false;
        if (segment.Find('/') != wxNOT_FOUND || segment.Find('\\') != wxNOT_FOUND)
            return true;
        return IsFileLocation(segment);
    }

    // Drop absolute paths, including ones with spaces such as "/Users/me/My Project/file.cpp:12:3:".
    wxString StripAbsolutePaths(const wxString& input)
    {
        wxString out;
        size_t i = 0;
        while (i < input.length())
        {
            if (AtTokenStart(input, i) && StartsAbsolutePath(input, i))
            {
                size_t end = i;
                while (end < input.length() && !wxIsspace(input[end])
                        && input[end] != '\'' && input[end] != '"')
                    ++end;
                for (;;)
                {
                    size_t space = end;
                    while (space < input.length() && wxIsspace(input[space]))
                        ++space;
                    if (space == end || space >= input.length())
                        break;
                    size_t segEnd = space;
                    while (segEnd < input.length() && !wxIsspace(input[segEnd])
                            && input[segEnd] != '\'' && input[segEnd] != '"')
                        ++segEnd;
                    if (!SegmentContinuesPath(input.Mid(space, segEnd - space)))
                        break;
                    end = segEnd;
                }
                if (!out.empty() && !wxIsspace(out.Last()))
                    out << wxT(' ');
                i = end;
                continue;
            }
            out << input[i];
            ++i;
        }
        return out;
    }

    bool IsPathToken(const wxString& raw)
    {
        const wxString core = TrimSearchWord(raw);
        if (core.empty())
            return false;
        if (core[0] == '/' || core[0] == '\\' || core.StartsWith(wxT("~/")))
            return true;
        if (core.length() >= 3 && wxIsalpha(core[0]) && core[1] == ':'
                && (core[2] == '/' || core[2] == '\\'))
            return true;
        if (IsFileLocation(core))
            return true;
        const int slash = core.Find('/');
        const int back = core.Find('\\');
        const int sep = slash == wxNOT_FOUND ? back : (back == wxNOT_FOUND ? slash : wxMin(slash, back));
        return sep > 0 && sep < (int)core.length() - 1;
    }

    bool IsLocationNoise(const wxString& raw)
    {
        bool digit = false;
        bool punct = false;
        if (raw.empty())
            return false;
        for (size_t i = 0; i < raw.length(); ++i)
        {
            const wxChar c = raw[i];
            if (wxIsdigit(c))
                digit = true;
            else if (c == ':' || c == '(' || c == ')')
                punct = true;
            else if (c == ',' || c == ';' || c == '.')
                continue;
            else
                return false;
        }
        return digit && punct;
    }

    wxString StripPathTokens(const wxString& text)
    {
        wxString out;
        wxString token;
        const auto flush = [&]()
        {
            if (!token.empty() && !IsPathToken(token) && !IsLocationNoise(token))
            {
                if (!out.empty())
                    out << wxT(' ');
                out << token;
            }
            token.clear();
        };
        for (size_t i = 0; i < text.length(); ++i)
        {
            if (wxIsspace(text[i]))
                flush();
            else
                token << text[i];
        }
        flush();
        return out;
    }

    bool KeywordBoundary(const wxString& text, size_t pos)
    {
        return pos == 0 || !wxIsalnum(text[pos - 1]);
    }

    // Earliest "error:", "warning:", "note:", "fatal error:", "error C2065:" and the same kind of introduction.
    bool FindIntro(const wxString& text, size_t& keywordAt, size_t& bodyAt)
    {
        const wxString lower = text.Lower();
        const wxChar* keys[] = {
            wxT("fatal error"), wxT("error"), wxT("warning"), wxT("notice"),
            wxT("note"), wxT("remark"), wxT("info"), wxT("message"), nullptr
        };
        bool found = false;
        keywordAt = wxString::npos;
        bodyAt = wxString::npos;
        for (int k = 0; keys[k]; ++k)
        {
            const wxString key(keys[k]);
            size_t from = 0;
            while (from < lower.length())
            {
                const int rel = lower.Mid(from).Find(key);
                if (rel == wxNOT_FOUND)
                    break;
                const size_t pos = from + (size_t)rel;
                from = pos + 1;
                if (!KeywordBoundary(lower, pos))
                    continue;
                const size_t afterKey = pos + key.length();
                if (afterKey < lower.length() && wxIsalnum(lower[afterKey]))
                    continue;

                size_t cursor = afterKey;
                while (cursor < lower.length() && wxIsspace(lower[cursor]))
                    ++cursor;
                if (cursor < lower.length() && lower[cursor] != ':')
                {
                    size_t code = cursor;
                    if (lower[code] == '[')
                    {
                        while (code < lower.length() && lower[code] != ']')
                            ++code;
                        if (code < lower.length() && lower[code] == ']')
                            ++code;
                    }
                    else if (wxIsalnum(lower[code]) || lower[code] == '_' || lower[code] == '-' || lower[code] == '+')
                    {
                        while (code < lower.length()
                                && (wxIsalnum(lower[code]) || lower[code] == '_' || lower[code] == '-' || lower[code] == '+'))
                            ++code;
                    }
                    else
                        continue;
                    while (code < lower.length() && wxIsspace(lower[code]))
                        ++code;
                    if (code >= lower.length() || lower[code] != ':')
                        continue;
                    cursor = code;
                }
                if (cursor < lower.length() && lower[cursor] == ':')
                {
                    if (!found || pos < keywordAt)
                    {
                        found = true;
                        keywordAt = pos;
                        bodyAt = cursor + 1;
                    }
                    break;
                }
            }
        }
        return found;
    }

    wxString CollapseWhitespace(const wxString& text)
    {
        wxString out;
        bool pendingSpace = false;
        for (size_t i = 0; i < text.length(); ++i)
        {
            if (wxIsspace(text[i]))
            {
                pendingSpace = !out.empty();
                continue;
            }
            if (pendingSpace)
                out << wxT(' ');
            pendingSpace = false;
            out << text[i];
        }
        return out;
    }

    wxString ExtensionOf(const wxString& filename)
    {
        const wxString base = filename.AfterLast(wxT('/')).AfterLast(wxT('\\'));
        const int dot = base.Find(wxT('.'), true);
        if (dot == wxNOT_FOUND || dot <= 0 || dot >= (int)base.length() - 1)
            return wxEmptyString;
        return base.Mid(dot + 1).Lower();
    }

    // Extension fallback used when the IDE has no more specific language.
    wxString LanguageFromExtension(const wxString& ext)
    {
        if (ext == wxT("cpp") || ext == wxT("cc") || ext == wxT("cxx")
                || ext == wxT("hpp") || ext == wxT("hh") || ext == wxT("hxx")
                || ext == wxT("ipp") || ext == wxT("inl") || ext == wxT("tcc") || ext == wxT("tpp"))
            return wxT("C++");
        if (ext == wxT("c"))
            return wxT("C");
        if (ext == wxT("m"))
            return wxT("Objective-C");
        if (ext == wxT("mm"))
            return wxT("Objective-C++");
        if (ext == wxT("java"))
            return wxT("Java");
        if (ext == wxT("py") || ext == wxT("pyw") || ext == wxT("pyx"))
            return wxT("Python");
        return wxEmptyString;
    }

    // ideLanguage is the lexer name from EditorColourSet, such as "C/C++" or "Python".
    wxString SearchLanguageName(const wxString& ideLanguage, const wxString& filename)
    {
        const wxString ext = ExtensionOf(filename);
        wxString ide = ideLanguage;
        ide.Trim(true).Trim(false);
        if (ide == wxT("Plain text"))
            ide.clear();

        if (ide == wxT("C/C++"))
        {
            if (ext == wxT("c"))
                return wxT("C");
            const wxString fromExt = LanguageFromExtension(ext);
            return fromExt.empty() ? wxString(wxT("C++")) : fromExt;
        }
        if (ide == wxT("Objective C") || ide == wxT("Objective-C") || ide == wxT("Objective-C++"))
            return ext == wxT("mm") ? wxString(wxT("Objective-C++")) : wxString(wxT("Objective-C"));
        if (!ide.empty())
            return ide;
        return LanguageFromExtension(ext);
    }

    bool IsCompilerDriver(const wxString& name)
    {
        const wxString n = name.Lower();
        return n == wxT("gcc") || n == wxT("g++") || n == wxT("clang") || n == wxT("clang++")
            || n == wxT("cc") || n == wxT("c++") || n == wxT("ld") || n == wxT("ld.lld")
            || n == wxT("lld") || n == wxT("collect2") || n == wxT("cc1") || n == wxT("cc1plus")
            || n == wxT("as") || n == wxT("ar") || n == wxT("ranlib") || n == wxT("make")
            || n == wxT("clang-cl") || n == wxT("link");
    }

    wxString StripCompilerDrivers(wxString text)
    {
        for (;;)
        {
            text.Trim(false);
            const int colon = text.Find(wxT(':'));
            if (colon <= 0 || colon > 40)
                break;
            const wxString head = text.Left(colon);
            if (head.Find(wxT(' ')) != wxNOT_FOUND || head.Find(wxT('\t')) != wxNOT_FOUND)
                break;
            wxString base = head;
            if (base.Find(wxT('/')) != wxNOT_FOUND || base.Find(wxT('\\')) != wxNOT_FOUND)
                base = base.AfterLast(wxT('/')).AfterLast(wxT('\\'));
            if (!IsCompilerDriver(base))
                break;
            text = text.Mid(colon + 1);
        }
        return text;
    }

    wxString StripBracketNoise(const wxString& text)
    {
        wxString out;
        for (size_t i = 0; i < text.length(); )
        {
            if (text[i] == wxT('['))
            {
                size_t j = i + 1;
                while (j < text.length() && text[j] != wxT(']'))
                    ++j;
                if (j < text.length())
                {
                    wxString inner = text.Mid(i + 1, j - i - 1);
                    inner.Trim(true).Trim(false);
                    const wxString lower = inner.Lower();
                    if (lower.StartsWith(wxT("-w")) || lower.StartsWith(wxT("with ")))
                    {
                        if (!out.empty() && wxIsspace(out.Last()))
                            out.RemoveLast();
                        i = j + 1;
                        continue;
                    }
                }
            }
            out << text[i];
            ++i;
        }
        return out;
    }

    // Drop template-instantiation and include context that is not the diagnostic itself.
    wxString StripContextNoise(const wxString& text)
    {
        const wxChar* markers[] = {
            wxT("in instantiation of"),
            wxT("during instantiation of"),
            wxT("required from"),
            wxT("instantiated from"),
            wxT("in file included from"),
            wxT("in substitution of"),
            wxT("while substituting"),
            nullptr
        };
        const wxString lower = text.Lower();
        size_t cut = wxString::npos;
        for (int m = 0; markers[m]; ++m)
        {
            const wxString marker(markers[m]);
            size_t from = 0;
            while (from < lower.length())
            {
                const int rel = lower.Mid(from).Find(marker);
                if (rel == wxNOT_FOUND)
                    break;
                const size_t pos = from + (size_t)rel;
                from = pos + marker.length();
                if (!KeywordBoundary(lower, pos))
                    continue;
                if (cut == wxString::npos || pos < cut)
                    cut = pos;
                break;
            }
        }
        if (cut == wxString::npos)
            return text;
        wxString head = text.Left(cut);
        head.Trim(true).Trim(false);
        return head;
    }

    wxString StripBoilerplateParens(wxString text)
    {
        text.Trim(true).Trim(false);
        for (;;)
        {
            if (text.empty() || text.Last() != wxT(')'))
                break;
            const int open = text.Find(wxT('('), true);
            if (open == wxNOT_FOUND)
                break;
            const wxString inner = text.Mid(open + 1, text.length() - (size_t)open - 2).Lower().Trim(true).Trim(false);
            if (!(inner.StartsWith(wxT("first use"))
                    || inner.StartsWith(wxT("each undeclared"))
                    || inner.StartsWith(wxT("maybe you meant"))))
                break;
            text = text.Left(open);
            text.Trim(true).Trim(false);
        }
        return text;
    }

    void StripTrailingConnector(wxString& text)
    {
        text.Trim(true);
        const wxString lower = text.Lower();
        const wxChar* rels[] = { wxT(" of"), wxT(" to"), nullptr };
        for (int i = 0; rels[i]; ++i)
        {
            const wxString rel(rels[i]);
            if (lower.EndsWith(rel) && text.length() >= rel.length())
            {
                text = text.Left(text.length() - rel.length());
                text.Trim(true);
                return;
            }
        }
    }

    bool IsIdentChar(wxChar c)
    {
        return wxIsalnum(c) || c == wxT('_') || c == wxT('~') || c == wxT('$');
    }

    const int kDiagnosticWordLimit = 8;

    bool IsListedWord(const wxString& word, const wxChar* const* words)
    {
        const wxString lower = word.Lower();
        for (int i = 0; words[i]; ++i)
        {
            if (lower == words[i])
                return true;
        }
        return false;
    }

    // Built-in type words. A quoted span of only these is not a project symbol.
    bool IsBuiltinTypeWord(const wxString& word)
    {
        static const wxChar* words[] = {
            wxT("int"), wxT("char"), wxT("void"), wxT("bool"), wxT("float"), wxT("double"),
            wxT("long"), wxT("short"), wxT("signed"), wxT("unsigned"), wxT("const"),
            wxT("volatile"), wxT("auto"), wxT("wchar_t"), wxT("char8_t"), wxT("char16_t"),
            wxT("char32_t"), wxT("size_t"), wxT("nullptr"), wxT("true"), wxT("false"),
            nullptr
        };
        return IsListedWord(word, words);
    }

    bool IsGlueWord(const wxString& word)
    {
        static const wxChar* words[] = {
            wxT("a"), wxT("an"), wxT("the"), wxT("its"), wxT("it"), wxT("their"),
            wxT("this"), wxT("that"), wxT("these"), wxT("those"),
            wxT("will"), wxT("be"), wxT("is"), wxT("are"), wxT("was"), wxT("were"),
            wxT("in"), wxT("of"), wxT("to"), wxT("for"), wxT("and"), wxT("or"),
            wxT("every"), wxT("each"), wxT("such"), wxT("with"), wxT("by"), wxT("as"),
            wxT("on"), wxT("at"), wxT("from"), nullptr
        };
        return IsListedWord(word, words);
    }

    // True when a quoted span names a project symbol rather than a built-in type.
    bool QuotedSpanIsProjectSymbol(const wxString& inner)
    {
        bool anyIdent = false;
        bool foreign = false;
        wxString token;
        const auto consider = [&]()
        {
            if (token.empty())
                return;
            anyIdent = true;
            if (!IsBuiltinTypeWord(token))
                foreign = true;
            token.clear();
        };
        for (size_t i = 0; i < inner.length(); ++i)
        {
            if (IsIdentChar(inner[i]))
                token << inner[i];
            else
                consider();
        }
        consider();
        return anyIdent && foreign;
    }

    bool ContainsWord(const wxString& text, const wxString& word)
    {
        if (word.empty())
            return false;
        const wxString hay = text.Lower();
        const wxString needle = word.Lower();
        size_t from = 0;
        while (from < hay.length())
        {
            const int rel = hay.Mid(from).Find(needle);
            if (rel == wxNOT_FOUND)
                return false;
            const size_t pos = from + (size_t)rel;
            const size_t end = pos + needle.length();
            const bool left = pos == 0 || !IsIdentChar(hay[pos - 1]);
            const bool right = end >= hay.length() || !IsIdentChar(hay[end]);
            if (left && right)
                return true;
            from = pos + 1;
        }
        return false;
    }

    // Drop quoted project symbols. Built-in types such as 'int' stay, without the quotes.
    wxString DropQuotedProjectSymbols(const wxString& text)
    {
        wxString out;
        size_t i = 0;
        while (i < text.length())
        {
            const wxChar q = text[i];
            if (q != wxT('\'') && q != wxT('`') && q != wxT('"'))
            {
                out << text[i];
                ++i;
                continue;
            }
            const wxChar close = (q == wxT('`')) ? wxT('\'') : q;
            size_t j = i + 1;
            while (j < text.length() && text[j] != close && text[j] != q)
                ++j;
            if (j >= text.length())
            {
                out << text[i];
                ++i;
                continue;
            }
            const wxString inner = text.Mid(i + 1, j - i - 1);
            if (!QuotedSpanIsProjectSymbol(inner))
                out << inner;
            else if (!out.empty() && !wxIsspace(out.Last()))
                out << wxT(' ');
            i = j + 1;
        }
        return out;
    }

    bool IsSearchEdgePunct(wxChar c)
    {
        switch (c)
        {
            case '\'': case '"': case '`':
            case ',': case ';': case ':': case '.':
            case '(': case ')': case '[': case ']': case '{': case '}':
                return true;
            default:
                return false;
        }
    }

    wxString JoinWords(const wxArrayString& words, int maxWords)
    {
        wxString out;
        const int limit = wxMin(maxWords, (int)words.GetCount());
        for (int i = 0; i < limit; ++i)
        {
            if (i)
                out << wxT(' ');
            out << words[i];
        }
        return out;
    }

    // First clause, plus one distinctive word from the clause after ';' when it fits.
    wxString ReduceDiagnosticText(const wxString& diagnostic)
    {
        const wxString stripped = CollapseWhitespace(DropQuotedProjectSymbols(diagnostic));
        wxArrayString core;
        wxArrayString after;
        bool afterBreak = false;
        wxString token;
        const auto flush = [&]()
        {
            if (token.empty())
                return;
            const bool clauseBreak = token.Find(wxT(';')) != wxNOT_FOUND;
            wxString word = token;
            token.clear();
            while (!word.empty() && IsSearchEdgePunct(word[0]))
                word = word.Mid(1);
            while (!word.empty() && IsSearchEdgePunct(word.Last()))
                word.RemoveLast();
            if (!word.empty())
            {
                if (afterBreak)
                    after.Add(word);
                else
                    core.Add(word);
            }
            if (clauseBreak)
                afterBreak = true;
        };
        for (size_t i = 0; i < stripped.length(); ++i)
        {
            if (wxIsspace(stripped[i]))
                flush();
            else
                token << stripped[i];
        }
        flush();

        wxArrayString chosen;
        for (size_t i = 0; i < core.GetCount() && (int)chosen.GetCount() < kDiagnosticWordLimit; ++i)
            chosen.Add(core[i]);
        if (afterBreak && (int)chosen.GetCount() < kDiagnosticWordLimit)
        {
            for (size_t i = 0; i < after.GetCount(); ++i)
            {
                if (IsGlueWord(after[i]))
                    continue;
                chosen.Add(after[i]);
                break;
            }
        }

        wxString reduced = JoinWords(chosen, kDiagnosticWordLimit);
        StripTrailingConnector(reduced);
        reduced.Trim(true).Trim(false);
        if (!reduced.empty())
            return reduced;

        // Quote removal left nothing useful. Keep a short normalised prefix instead.
        wxString fallback = diagnostic;
        fallback.Replace(wxT("'"), wxT(" "));
        fallback.Replace(wxT("\""), wxT(" "));
        fallback.Replace(wxT("`"), wxT(" "));
        fallback = CollapseWhitespace(fallback);
        wxArrayString plain;
        wxString plainToken;
        const auto flushPlain = [&]()
        {
            while (!plainToken.empty() && IsSearchEdgePunct(plainToken[0]))
                plainToken = plainToken.Mid(1);
            while (!plainToken.empty() && IsSearchEdgePunct(plainToken.Last()))
                plainToken.RemoveLast();
            if (!plainToken.empty())
                plain.Add(plainToken);
            plainToken.clear();
        };
        for (size_t i = 0; i < fallback.length(); ++i)
        {
            if (wxIsspace(fallback[i]))
                flushPlain();
            else
                plainToken << fallback[i];
        }
        flushPlain();
        return JoinWords(plain, kDiagnosticWordLimit);
    }

    wxString TrimEdgePunctuation(wxString text)
    {
        text.Trim(true).Trim(false);
        while (!text.empty())
        {
            const wxChar c = text.Last();
            if (c == wxT(',') || c == wxT(';') || c == wxT(':'))
                text.RemoveLast();
            else if (c == wxT('.') && text.length() >= 2 && wxIsalnum(text[text.length() - 2]))
                text.RemoveLast();
            else
                break;
            text.Trim(true);
        }
        return text;
    }

    // Keep the start of the diagnostic. Extra detail is what gets dropped.
    wxString KeepDiagnosticHead(const wxString& phrase, int maxChars)
    {
        if (maxChars < 1 || (int)phrase.length() <= maxChars)
            return phrase;
        wxString cut = phrase.Left(maxChars);
        int boundary = -1;
        for (int i = (int)cut.length() - 1; i >= 24; --i)
        {
            if (cut[i] == wxT(';') || cut[i] == wxT(','))
            {
                boundary = i;
                break;
            }
        }
        if (boundary < 0)
        {
            const int space = cut.Find(wxT(' '), true);
            if (space >= 24)
                boundary = space;
        }
        if (boundary > 0)
            cut = cut.Left(boundary);
        cut.Trim(true).Trim(false);
        return cut;
    }

    wxString ExtractDiagnostic(const wxString& message)
    {
        wxString text = StripPathTokens(StripAbsolutePaths(message));
        text = StripCompilerDrivers(text);
        text.Trim(true).Trim(false);
        if (text.empty())
            return wxEmptyString;

        size_t keywordAt = 0;
        size_t bodyAt = 0;
        if (FindIntro(text, keywordAt, bodyAt))
        {
            size_t content = 0;
            while (content < text.length() && wxIsspace(text[content]))
                ++content;
            if (keywordAt == content)
                text = text.Mid(bodyAt);
        }

        text = StripBracketNoise(text);
        text = StripContextNoise(text);
        text = StripBoilerplateParens(text);
        text = CollapseWhitespace(text);
        text = TrimEdgePunctuation(text);
        return text;
    }

    wxString AssembleSearchQuery(const wxString& language, const wxString& phraseIn, const wxString& symbol)
    {
        wxString phrase = CollapseWhitespace(phraseIn);
        phrase.Trim(true).Trim(false);
        wxString name = symbol;
        name.Trim(true).Trim(false);
        if (!name.empty() && ContainsWord(phrase, name))
            name.clear();

        const int kMaxQuery = 160;
        const int languageChars = language.empty() ? 0 : (int)language.length() + 1;
        int phraseBudget = kMaxQuery - languageChars;
        if (!name.empty())
            phraseBudget -= (int)name.length() + 1;
        if (phraseBudget < 24)
        {
            name.clear();
            phraseBudget = kMaxQuery - languageChars;
            if (phraseBudget < 24)
                phraseBudget = 24;
        }
        phrase = KeepDiagnosticHead(phrase, phraseBudget);
        if (phrase.empty() && name.empty())
            return wxEmptyString;

        wxString query;
        if (!language.empty())
            query << language;
        if (!phrase.empty())
        {
            if (!query.empty())
                query << wxT(' ');
            query << phrase;
        }
        if (!name.empty() && (int)query.length() + 1 + (int)name.length() <= kMaxQuery)
        {
            if (!query.empty())
                query << wxT(' ');
            query << name;
        }
        return query;
    }

    wxString MakeStackOverflowQuery(const wxString& message, const wxString& language)
    {
        const wxString diagnostic = ExtractDiagnostic(message);
        if (diagnostic.empty())
            return wxEmptyString;
        return AssembleSearchQuery(language, ReduceDiagnosticText(diagnostic), wxEmptyString);
    }
}

BEGIN_EVENT_TABLE(CompilerMessages, wxEvtHandler)
END_EVENT_TABLE()

CompilerMessages::CompilerMessages(const wxArrayString& titles_in, const wxArrayInt& widths_in)
    : ListCtrlLogger(titles_in, widths_in, true),
      m_rightClickItem(-1),
      m_clickItem(-1),
      m_clickFromList(false)
{
    m_autoFit = Manager::Get()->GetConfigManager("compiler")->ReadBool("/autofit_during_build", false);
    //ctor
}

CompilerMessages::~CompilerMessages()
{
	//dtor
	Disconnect(idList, -1, wxEVT_COMMAND_LIST_ITEM_SELECTED,
            (wxObjectEventFunction) (wxEventFunction) (wxCommandEventFunction)
            &CompilerMessages::OnClick);
    Disconnect(idList, -1, wxEVT_COMMAND_LIST_ITEM_ACTIVATED,
            (wxObjectEventFunction) (wxEventFunction) (wxCommandEventFunction)
            &CompilerMessages::OnDoubleClick);
    Disconnect(idMenuFit, -1, wxEVT_COMMAND_MENU_SELECTED,
            (wxObjectEventFunction) (wxEventFunction) (wxCommandEventFunction)
            &CompilerMessages::OnFit);
    Disconnect(idMenuAutoFit, -1, wxEVT_COMMAND_MENU_SELECTED,
            (wxObjectEventFunction) (wxEventFunction) (wxCommandEventFunction)
            &CompilerMessages::OnAutoFit);
    Disconnect(idMenuStackOverflow, -1, wxEVT_COMMAND_MENU_SELECTED,
            (wxObjectEventFunction) (wxEventFunction) (wxCommandEventFunction)
            &CompilerMessages::OnSearchStackOverflow);
}

wxWindow* CompilerMessages::CreateControl(wxWindow* parent)
{
    ListCtrlLogger::CreateControl(parent);
    control->SetId(idList);
    Connect(idList, -1, wxEVT_COMMAND_LIST_ITEM_SELECTED,
            (wxObjectEventFunction) (wxEventFunction) (wxCommandEventFunction)
            &CompilerMessages::OnClick);
    Connect(idList, -1, wxEVT_COMMAND_LIST_ITEM_ACTIVATED,
            (wxObjectEventFunction) (wxEventFunction) (wxCommandEventFunction)
            &CompilerMessages::OnDoubleClick);
    Connect(idMenuFit, -1, wxEVT_COMMAND_MENU_SELECTED,
            (wxObjectEventFunction) (wxEventFunction) (wxCommandEventFunction)
            &CompilerMessages::OnFit);
    Connect(idMenuAutoFit, -1, wxEVT_COMMAND_MENU_SELECTED,
            (wxObjectEventFunction) (wxEventFunction) (wxCommandEventFunction)
            &CompilerMessages::OnAutoFit);
    Connect(idMenuStackOverflow, -1, wxEVT_COMMAND_MENU_SELECTED,
            (wxObjectEventFunction) (wxEventFunction) (wxCommandEventFunction)
            &CompilerMessages::OnSearchStackOverflow);
    control->Connect(wxEVT_RIGHT_DOWN, wxMouseEventHandler(CompilerMessages::OnRightDown), nullptr, this);
    control->Connect(wxEVT_CONTEXT_MENU, wxContextMenuEventHandler(CompilerMessages::OnListContextMenu), nullptr, this);
    Manager::Get()->GetAppWindow()->PushEventHandler(this);
    return control;
}

void CompilerMessages::DestroyControls()
{
    if (Manager::Get()->IsAppShuttingDown())
    {
        // The info pane, and this list with it, is destroyed before the logger.
        control = nullptr;
        return;
    }

    if (control)
    {
        control->Disconnect(wxEVT_RIGHT_DOWN, wxMouseEventHandler(CompilerMessages::OnRightDown), nullptr, this);
        control->Disconnect(wxEVT_CONTEXT_MENU, wxContextMenuEventHandler(CompilerMessages::OnListContextMenu), nullptr, this);
    }
    Manager::Get()->GetAppWindow()->RemoveEventHandler(this);
}

bool CompilerMessages::HasFeature(Feature::Enum feature) const
{
    if (feature == Feature::Additional)
        return true;
    else
        return ListCtrlLogger::HasFeature(feature);
}

void CompilerMessages::AppendAdditionalMenuItems(wxMenu &menu)
{
    m_stackOverflowQuery = QueryForMenu();
    m_clickFromList = false;

    const wxString copySelection = _("Copy selection to clipboard");
    size_t insertAt = menu.GetMenuItemCount();
    size_t index = 0;
    wxMenuItemList& items = menu.GetMenuItems();
    for (wxMenuItemList::iterator it = items.begin(); it != items.end(); ++it, ++index)
    {
        wxMenuItem* item = *it;
        if (item && item->GetItemLabelText() == copySelection)
        {
            insertAt = index + 1;
            break;
        }
    }
    wxMenuItem* searchItem = menu.Insert(insertAt, idMenuStackOverflow, _("Search Stackoverflow"),
                                         _("Search this build message on Stack Overflow"));
    if (searchItem)
        searchItem->Enable(!m_stackOverflowQuery.empty());

    menu.Append(idMenuFit, _("Fit text"), _("Makes the whole text visible"));
    menu.AppendCheckItem(idMenuAutoFit, _("Fit automatically"),
                         _("Automatically makes the whole text visible during compilation"));
    menu.Check(idMenuAutoFit, m_autoFit);
}

void CompilerMessages::FocusError(int nr)
{
    if (nr < 0 or nr >= control->GetItemCount())
        return;
    control->SetItemState(nr, wxLIST_STATE_SELECTED | wxLIST_STATE_FOCUSED, wxLIST_STATE_SELECTED | wxLIST_STATE_FOCUSED);
    control->EnsureVisible(nr);
}

void CompilerMessages::OnClick(cb_unused wxCommandEvent& event)
{
    // single and double-click, behave the same

    // a compiler message has been clicked
    // go to the relevant file/line
    if (control->GetSelectedItemCount() == 0 || !m_pErrors)
        return;

    // find selected item index
    int index = control->GetNextItem(-1,
                                     wxLIST_NEXT_ALL,
                                     wxLIST_STATE_SELECTED);

    // call the CompilerErrors* ptr; it 'll do all the hard work ;)
    m_pErrors->GotoError(index);
}

void CompilerMessages::OnDoubleClick(wxCommandEvent& event)
{
    // single and double-click, behave the same
    OnClick(event);
    return;
}

void CompilerMessages::OnFit(wxCommandEvent& WXUNUSED(event))
{
    FitColumns();
}

void CompilerMessages::OnAutoFit(wxCommandEvent& event)
{
    m_autoFit = event.IsChecked();
    Manager::Get()->GetConfigManager("compiler")->Write("/autofit_during_build", m_autoFit);
}

void CompilerMessages::AutoFitColumns(int cb_unused column)
{
    if (m_autoFit)
        FitColumns();
}

void CompilerMessages::FitColumns()
{
    if (!control)
        return;
    int count = control->GetColumnCount();
    for (int ii = 0; ii < count; ++ii)
        control->SetColumnWidth(ii, wxLIST_AUTOSIZE);
    int ctrlWidth = control->GetClientSize().x;
    for (int ii = 1; ii < count; ++ii)
        ctrlWidth -= control->GetColumnWidth(ii);
    int width = control->GetColumnWidth(0);
    if (width > ctrlWidth)
        control->SetColumnWidth(0, ctrlWidth);
}

int CompilerMessages::MessageColumn() const
{
    for (size_t i = 0; i < titles.GetCount(); ++i)
    {
        if (titles[i] == _("Message"))
            return (int)i;
    }
    if (!titles.IsEmpty())
        return (int)titles.GetCount() - 1;
    return 2;
}

int CompilerMessages::FileColumn() const
{
    for (size_t i = 0; i < titles.GetCount(); ++i)
    {
        if (titles[i] == _("File"))
            return (int)i;
    }
    return 0;
}

wxString CompilerMessages::MessageText(long item) const
{
    if (!control || item < 0 || item >= control->GetItemCount())
        return wxEmptyString;
    return control->GetItemText(item, MessageColumn());
}

wxString CompilerMessages::FileText(long item) const
{
    if (!control || item < 0 || item >= control->GetItemCount())
        return wxEmptyString;
    return control->GetItemText(item, FileColumn());
}

wxString CompilerMessages::LanguageForFile(const wxString& filename) const
{
    wxString ide;
    if (!filename.empty() && Manager::Get() && !Manager::IsAppShuttingDown())
    {
        if (EditorManager* editorManager = Manager::Get()->GetEditorManager())
        {
            EditorColourSet* colours = editorManager->GetColourSet();
            if (cbEditor* editor = editorManager->GetBuiltinEditor(filename))
            {
                if (colours)
                    ide = colours->GetLanguageName(editor->GetLanguage());
            }
            if (colours && (ide.empty() || ide == _("Plain text") || ide == wxT("Plain text")))
                ide = colours->GetLanguageName(colours->GetLanguageForFilename(filename));
        }
    }
    return SearchLanguageName(ide, filename);
}

wxString CompilerMessages::SelectedMessageText() const
{
    if (!control)
        return wxEmptyString;

    if (wxTextCtrl* editor = control->GetEditControl())
    {
        long from = 0;
        long to = 0;
        editor->GetSelection(&from, &to);
        if (to > from)
            return editor->GetStringSelection();
    }

    for (wxWindowList::compatibility_iterator it = control->GetChildren().GetFirst(); it; it = it->GetNext())
    {
        wxTextCtrl* text = wxDynamicCast(it->GetData(), wxTextCtrl);
        if (!text)
            continue;
        long from = 0;
        long to = 0;
        text->GetSelection(&from, &to);
        if (to > from)
            return text->GetStringSelection();
    }
    return wxEmptyString;
}

long CompilerMessages::FirstWarningItem() const
{
    if (!control || control->GetItemCount() < 1)
        return -1;

    const long count = control->GetItemCount();
    long first = -1;
    for (long item = 0; item < count; ++item)
    {
        const wxString text = MessageText(item);
        if (text.empty())
            continue;
        if (first < 0)
            first = item;
        if (text.Lower().Contains(wxT("warning")))
            return item;
    }
    return first;
}

void CompilerMessages::CaptureListContext(long item)
{
    m_clickFromList = true;
    m_clickItem = item;
    m_clickSelection = SelectedMessageText();
}

wxString CompilerMessages::QueryForMenu() const
{
    long item = -1;
    if (m_clickFromList && m_clickItem >= 0)
        item = m_clickItem;
    else
        item = FirstWarningItem();

    const wxString language = LanguageForFile(item >= 0 ? FileText(item) : wxString());
    if (m_clickFromList && !m_clickSelection.empty())
    {
        const wxString selected = MakeStackOverflowQuery(m_clickSelection, language);
        if (!selected.empty())
            return selected;
    }
    if (item < 0)
        return wxEmptyString;
    return MakeStackOverflowQuery(MessageText(item), language);
}

void CompilerMessages::OnRightDown(wxMouseEvent& event)
{
    m_rightClickItem = -1;
    if (control)
    {
        int flags = 0;
        const long hit = control->HitTest(event.GetPosition(), flags);
        if (hit >= 0)
            m_rightClickItem = hit;
    }
    event.Skip();
}

void CompilerMessages::OnListContextMenu(wxContextMenuEvent& event)
{
    long item = -1;
    const bool keyboard = !control || event.GetPosition() == wxDefaultPosition;
    if (!keyboard)
    {
        int flags = 0;
        const long hit = control->HitTest(control->ScreenToClient(event.GetPosition()), flags);
        if (hit >= 0)
            item = hit;
        else if (m_rightClickItem >= 0)
            item = m_rightClickItem;
    }
    else if (control)
    {
        const long focused = control->GetNextItem(-1, wxLIST_NEXT_ALL, wxLIST_STATE_FOCUSED);
        if (focused >= 0)
            item = focused;
    }
    m_rightClickItem = -1;
    CaptureListContext(item);
    event.Skip();
}

void CompilerMessages::OnSearchStackOverflow(wxCommandEvent& WXUNUSED(event))
{
    if (m_stackOverflowQuery.empty())
        return;
    wxLaunchDefaultBrowser(wxT("https://stackoverflow.com/search?q=") + URLEncode(m_stackOverflowQuery));
}
