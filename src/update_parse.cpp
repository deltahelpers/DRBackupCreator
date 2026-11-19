#include "update_parse.h"

#include <cctype>
#include <cstddef>
#include <functional>
#include <vector>

namespace upd {

namespace {

// ---------------------------------------------------------------------------
// Minimal JSON reader
//
// The GitHub release document is small and its shape is fixed, so a full JSON
// library would be overkill. This reader understands objects, arrays, strings
// (including \u escapes) and skips everything else.
// ---------------------------------------------------------------------------

void AppendUtf8(std::string& out, unsigned cp)
{
    if (cp < 0x80) {
        out.push_back(static_cast<char>(cp));
    } else if (cp < 0x800) {
        out.push_back(static_cast<char>(0xC0 | (cp >> 6)));
        out.push_back(static_cast<char>(0x80 | (cp & 0x3F)));
    } else if (cp < 0x10000) {
        out.push_back(static_cast<char>(0xE0 | (cp >> 12)));
        out.push_back(static_cast<char>(0x80 | ((cp >> 6) & 0x3F)));
        out.push_back(static_cast<char>(0x80 | (cp & 0x3F)));
    } else {
        out.push_back(static_cast<char>(0xF0 | (cp >> 18)));
        out.push_back(static_cast<char>(0x80 | ((cp >> 12) & 0x3F)));
        out.push_back(static_cast<char>(0x80 | ((cp >> 6) & 0x3F)));
        out.push_back(static_cast<char>(0x80 | (cp & 0x3F)));
    }
}

void SkipWs(const char*& p, const char* end)
{
    while (p < end) {
        const char c = *p;
        if (c == ' ' || c == '\t' || c == '\r' || c == '\n') ++p;
        else break;
    }
}

bool Hex4(const char* p, const char* end, unsigned& value)
{
    if (end - p < 4) return false;
    unsigned v = 0;
    for (int i = 0; i < 4; ++i) {
        const char c = p[i];
        v <<= 4;
        if (c >= '0' && c <= '9') v |= static_cast<unsigned>(c - '0');
        else if (c >= 'a' && c <= 'f') v |= static_cast<unsigned>(c - 'a' + 10);
        else if (c >= 'A' && c <= 'F') v |= static_cast<unsigned>(c - 'A' + 10);
        else return false;
    }
    value = v;
    return true;
}

bool ParseString(const char*& p, const char* end, std::string& out)
{
    if (p >= end || *p != '"') return false;
    ++p;

    out.clear();
    while (p < end) {
        const char c = *p++;
        if (c == '"') return true;
        if (c != '\\') {
            out.push_back(c);
            continue;
        }
        if (p >= end) return false;

        const char esc = *p++;
        switch (esc) {
        case '"':  out.push_back('"');  break;
        case '\\': out.push_back('\\'); break;
        case '/':  out.push_back('/');  break;
        case 'b':  out.push_back('\b'); break;
        case 'f':  out.push_back('\f'); break;
        case 'n':  out.push_back('\n'); break;
        case 'r':  out.push_back('\r'); break;
        case 't':  out.push_back('\t'); break;
        case 'u': {
            unsigned cp = 0;
            if (!Hex4(p, end, cp)) return false;
            p += 4;

            // Surrogate pair: the high half is followed by \uDC00-\uDFFF.
            if (cp >= 0xD800 && cp <= 0xDBFF && end - p >= 6 && p[0] == '\\' && p[1] == 'u') {
                unsigned low = 0;
                if (Hex4(p + 2, end, low) && low >= 0xDC00 && low <= 0xDFFF) {
                    p += 6;
                    cp = 0x10000 + ((cp - 0xD800) << 10) + (low - 0xDC00);
                }
            }
            AppendUtf8(out, cp);
            break;
        }
        default:
            return false;
        }
    }
    return false;    // unterminated
}

bool SkipValue(const char*& p, const char* end);

bool SkipNumber(const char*& p, const char* end)
{
    const char* start = p;
    while (p < end) {
        const char c = *p;
        if ((c >= '0' && c <= '9') || c == '-' || c == '+' || c == '.' ||
            c == 'e' || c == 'E')
            ++p;
        else
            break;
    }
    return p != start;
}

bool SkipArray(const char*& p, const char* end)
{
    if (p >= end || *p != '[') return false;
    ++p;
    SkipWs(p, end);
    if (p < end && *p == ']') { ++p; return true; }

    while (p < end) {
        if (!SkipValue(p, end)) return false;
        SkipWs(p, end);
        if (p >= end) return false;
        if (*p == ',') { ++p; SkipWs(p, end); continue; }
        if (*p == ']') { ++p; return true; }
        return false;
    }
    return false;
}

bool SkipObject(const char*& p, const char* end)
{
    if (p >= end || *p != '{') return false;
    ++p;
    SkipWs(p, end);
    if (p < end && *p == '}') { ++p; return true; }

    while (p < end) {
        std::string key;
        if (!ParseString(p, end, key)) return false;
        SkipWs(p, end);
        if (p >= end || *p != ':') return false;
        ++p;
        SkipWs(p, end);
        if (!SkipValue(p, end)) return false;
        SkipWs(p, end);
        if (p >= end) return false;
        if (*p == ',') { ++p; SkipWs(p, end); continue; }
        if (*p == '}') { ++p; return true; }
        return false;
    }
    return false;
}

bool SkipValue(const char*& p, const char* end)
{
    SkipWs(p, end);
    if (p >= end) return false;

    switch (*p) {
    case '"': {
        std::string ignored;
        return ParseString(p, end, ignored);
    }
    case '{': return SkipObject(p, end);
    case '[': return SkipArray(p, end);
    case 't': if (end - p >= 4) { p += 4; return true; } return false;
    case 'f': if (end - p >= 5) { p += 5; return true; } return false;
    case 'n': if (end - p >= 4) { p += 4; return true; } return false;
    default:  return SkipNumber(p, end);
    }
}

using MemberFn = std::function<bool(const std::string& key, const char*& value,
                                    const char* end)>;

// Calls `onMember` for every key of the object at `p`. Returning false from the
// callback means "I did not consume the value, skip it".
bool ParseObject(const char*& p, const char* end, const MemberFn& onMember)
{
    SkipWs(p, end);
    if (p >= end || *p != '{') return false;
    ++p;
    SkipWs(p, end);
    if (p < end && *p == '}') { ++p; return true; }

    while (p < end) {
        std::string key;
        if (!ParseString(p, end, key)) return false;
        SkipWs(p, end);
        if (p >= end || *p != ':') return false;
        ++p;
        SkipWs(p, end);

        const char* value = p;
        if (!onMember(key, value, end)) {
            if (!SkipValue(value, end)) return false;
        }
        p = value;

        SkipWs(p, end);
        if (p >= end) return false;
        if (*p == ',') { ++p; SkipWs(p, end); continue; }
        if (*p == '}') { ++p; return true; }
        return false;
    }
    return false;
}

std::string StripLeadingV(const std::string& tag)
{
    if (!tag.empty() && (tag[0] == 'v' || tag[0] == 'V')) return tag.substr(1);
    return tag;
}

} // namespace

// ---------------------------------------------------------------------------
// Versions
// ---------------------------------------------------------------------------
int CompareVersions(const std::string& a, const std::string& b)
{
    auto split = [](const std::string& text) {
        std::vector<long long> parts;
        size_t i = 0;
        if (!text.empty() && (text[0] == 'v' || text[0] == 'V')) i = 1;

        long long current = 0;
        bool inNumber = false;
        for (; i < text.size(); ++i) {
            const char c = text[i];
            if (c >= '0' && c <= '9') {
                if (!inNumber) { current = 0; inNumber = true; }
                // Clamp so a silly string cannot overflow.
                if (current < 100000000LL) current = current * 10 + (c - '0');
            } else if (c == '.') {
                parts.push_back(inNumber ? current : 0);
                current = 0;
                inNumber = false;
            } else {
                break;    // "-beta", "+build", " rc1"...
            }
        }
        if (inNumber) parts.push_back(current);
        return parts;
    };

    const std::vector<long long> left  = split(a);
    const std::vector<long long> right = split(b);
    const size_t count = left.size() > right.size() ? left.size() : right.size();

    for (size_t i = 0; i < count; ++i) {
        const long long l = i < left.size() ? left[i] : 0;
        const long long r = i < right.size() ? right[i] : 0;
        if (l < r) return -1;
        if (l > r) return 1;
    }
    return 0;
}

bool IsNewer(const std::string& latest, const std::string& current)
{
    return CompareVersions(latest, current) > 0;
}

// ---------------------------------------------------------------------------
// Release document
// ---------------------------------------------------------------------------
bool ParseReleaseJson(const std::string& json, const std::string& assetName,
                      ReleaseInfo& out, std::string& error)
{
    out = ReleaseInfo{};
    error.clear();

    const char* p = json.data();
    const char* end = p + json.size();

    // The API endpoint that returns a message instead of a release is the
    // "not found / rate limited" case; keep it for the error text.
    std::string message;

    bool sawObject = ParseObject(p, end, [&](const std::string& key, const char*& value,
                                             const char* limit) -> bool {
        if (key == "tag_name")  return ParseString(value, limit, out.version);
        if (key == "body")      return ParseString(value, limit, out.notes);
        if (key == "html_url")  return ParseString(value, limit, out.htmlUrl);
        if (key == "message")   return ParseString(value, limit, message);
        if (key != "assets")    return false;

        if (value >= limit || *value != '[') return false;
        ++value;
        SkipWs(value, limit);
        if (value < limit && *value == ']') { ++value; return true; }

        while (value < limit) {
            std::string name, url, digest;
            long long size = 0;

            ParseObject(value, limit, [&](const std::string& field, const char*& v,
                                          const char* l) -> bool {
                if (field == "name")                 return ParseString(v, l, name);
                if (field == "browser_download_url") return ParseString(v, l, url);
                if (field == "digest")               return ParseString(v, l, digest);
                if (field == "size") {
                    const char* s = v;
                    long long parsed = 0;
                    bool any = false;
                    while (s < l && *s >= '0' && *s <= '9') {
                        parsed = parsed * 10 + (*s - '0');
                        ++s;
                        any = true;
                    }
                    if (!any) return false;
                    size = parsed;
                    v = s;
                    return true;
                }
                return false;
            });

            if (name == assetName) {
                out.installerName   = name;
                out.installerUrl    = url;
                out.installerDigest = digest;
                out.installerSize   = size;
            }

            SkipWs(value, limit);
            if (value < limit && *value == ',') { ++value; SkipWs(value, limit); continue; }
            if (value < limit && *value == ']') { ++value; break; }
            break;
        }
        return true;
    });

    if (!sawObject) {
        error = "the server did not return a release (bad JSON)";
        return false;
    }

    // The trailing document may contain whitespace; ParseObject already ate it.

    out.version = StripLeadingV(out.version);
    if (out.version.empty()) {
        error = message.empty() ? "the release has no tag_name" : message;
        return false;
    }
    return true;
}

// ---------------------------------------------------------------------------
// Markdown -> plain text
// ---------------------------------------------------------------------------
std::string NotesToPlainText(const std::string& markdown)
{
    std::string out;
    out.reserve(markdown.size());

    size_t pos = 0;
    bool lastLineBlank = true;    // avoid leading/trailing blank lines
    bool firstLine = true;

    while (pos <= markdown.size()) {
        size_t eol = markdown.find('\n', pos);
        if (eol == std::string::npos) eol = markdown.size();

        std::string line = markdown.substr(pos, eol - pos);
        pos = eol + 1;

        // Drop the CR of CRLF files.
        if (!line.empty() && line.back() == '\r') line.pop_back();

        // Headings: "## Title" -> "Title"
        size_t start = line.find_first_not_of(" \t");
        if (start != std::string::npos && line[start] == '#') {
            size_t after = start;
            while (after < line.size() && line[after] == '#') ++after;
            if (after < line.size() && line[after] == ' ') {
                line = line.substr(after + 1);
            }
        }

        // List items: "- x", "* x", "+ x" -> "• x"
        start = line.find_first_not_of(" \t");
        if (start != std::string::npos && start + 1 < line.size() &&
            (line[start] == '-' || line[start] == '*' || line[start] == '+') &&
            line[start + 1] == ' ') {
            line = line.substr(0, start) + "\xE2\x80\xA2" + line.substr(start + 1);
        }

        // Inline marks.
        std::string clean;
        clean.reserve(line.size());
        for (size_t i = 0; i < line.size(); ++i) {
            if (line[i] == '*' && i + 1 < line.size() && line[i + 1] == '*') { ++i; continue; }
            if (line[i] == '`') continue;
            if (line[i] == '_' && i + 1 < line.size() && line[i + 1] == '_') { ++i; continue; }
            clean.push_back(line[i]);
        }

        // Links: [text](url) -> text
        std::string linked;
        linked.reserve(clean.size());
        for (size_t i = 0; i < clean.size();) {
            if (clean[i] == '[') {
                const size_t close = clean.find(']', i + 1);
                const size_t open  = close == std::string::npos ? close : clean.find('(', close);
                const size_t end   = open  == std::string::npos ? open  : clean.find(')', open);
                if (close != std::string::npos && open == close + 1 && end != std::string::npos) {
                    linked.append(clean, i + 1, close - i - 1);
                    i = end + 1;
                    continue;
                }
            }
            linked.push_back(clean[i]);
            ++i;
        }

        const bool blank = linked.find_first_not_of(" \t") == std::string::npos;
        if (blank) {
            if (!lastLineBlank && !firstLine) out += "\r\n";
            lastLineBlank = true;
        } else {
            if (!out.empty() && out.back() != '\n') out += "\r\n";
            out += linked;
            lastLineBlank = false;
        }
        firstLine = false;

        if (eol == markdown.size()) break;
    }

    return out;
}

} // namespace upd
