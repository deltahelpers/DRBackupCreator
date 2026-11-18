#include "config.h"

#include <algorithm>
#include <cctype>
#include <charconv>
#include <cstdio>
#include <fstream>
#include <sstream>
#include <system_error>

namespace cfg {

namespace {

bool isSpace(char c) { return c == ' ' || c == '\t'; }

std::string lower(std::string text)
{
    std::transform(text.begin(), text.end(), text.begin(),
                   [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    return text;
}

std::string stripComment(std::string text)
{
    bool inSingle = false;
    bool inDouble = false;

    for (size_t i = 0; i < text.size(); ++i) {
        const char c = text[i];
        if (c == '\\' && (inSingle || inDouble)) { ++i; continue; }
        if (c == '\'' && !inDouble) { inSingle = !inSingle; continue; }
        if (c == '"' && !inSingle) { inDouble = !inDouble; continue; }
        if (c == '#' && !inSingle && !inDouble) return text.substr(0, i);
    }
    return text;
}

bool unquote(std::string& text, std::string& error)
{
    if (text.size() < 2) return true;
    const char first = text.front();
    if (first != '"' && first != '\'') return true;
    if (text.back() != first) {
        error = "unterminated quoted value";
        return false;
    }

    std::string out;
    out.reserve(text.size() - 2);
    for (size_t i = 1; i + 1 < text.size(); ++i) {
        if (text[i] == '\\' && i + 2 < text.size()) {
            const char next = text[++i];
            switch (next) {
            case 'n': out.push_back('\n'); break;
            case 't': out.push_back('\t'); break;
            default:  out.push_back(next); break;
            }
            continue;
        }
        out.push_back(text[i]);
    }
    text = std::move(out);
    return true;
}

int toInt(const std::string& text, bool& ok)
{
    int value = 0;
    const char* first = text.data();
    const char* last  = text.data() + text.size();
    const auto result = std::from_chars(first, last, value);
    ok = result.ec == std::errc{} && result.ptr == last;
    return value;
}

int clampInt(int value, int low, int high)
{
    return value < low ? low : (value > high ? high : value);
}

std::string pathToUtf8(const fs::path& path)
{
    return narrow(path.wstring());
}

} // namespace

// ---------------------------------------------------------------------------
// Text helpers
// ---------------------------------------------------------------------------
std::string trim(const std::string& text)
{
    size_t first = 0;
    size_t last  = text.size();
    while (first < last && isSpace(text[first])) ++first;
    while (last > first && (isSpace(text[last - 1]) || text[last - 1] == '\r')) --last;
    return text.substr(first, last - first);
}

std::wstring widen(const std::string& text)
{
    if (text.empty()) return {};
    const int size = MultiByteToWideChar(CP_UTF8, 0, text.data(),
                                         static_cast<int>(text.size()), nullptr, 0);
    if (size <= 0) return {};
    std::wstring out(static_cast<size_t>(size), L'\0');
    MultiByteToWideChar(CP_UTF8, 0, text.data(), static_cast<int>(text.size()),
                        out.data(), size);
    return out;
}

std::string narrow(const std::wstring& text)
{
    if (text.empty()) return {};
    const int size = WideCharToMultiByte(CP_UTF8, 0, text.data(),
                                         static_cast<int>(text.size()),
                                         nullptr, 0, nullptr, nullptr);
    if (size <= 0) return {};
    std::string out(static_cast<size_t>(size), '\0');
    WideCharToMultiByte(CP_UTF8, 0, text.data(), static_cast<int>(text.size()),
                        out.data(), size, nullptr, nullptr);
    return out;
}

std::string readTextFile(const fs::path& path, bool& ok)
{
    std::ifstream file(path, std::ios::binary);
    if (!file) { ok = false; return {}; }

    std::ostringstream buffer;
    buffer << file.rdbuf();
    ok = true;

    std::string text = buffer.str();
    // Tolerate a UTF-8 BOM, which Notepad likes to add.
    if (text.size() >= 3 && static_cast<unsigned char>(text[0]) == 0xEF &&
        static_cast<unsigned char>(text[1]) == 0xBB &&
        static_cast<unsigned char>(text[2]) == 0xBF) {
        text.erase(0, 3);
    }
    return text;
}

// ---------------------------------------------------------------------------
// Document
// ---------------------------------------------------------------------------
const Entry* Document::find(const std::string& key) const
{
    const std::string wanted = lower(key);
    // Later entries win, so search backwards.
    for (auto it = entries.rbegin(); it != entries.rend(); ++it) {
        if (it->key == wanted) return &*it;
    }
    return nullptr;
}

bool Document::has(const std::string& key) const
{
    return find(key) != nullptr;
}

std::string Document::get(const std::string& key, const std::string& fallback) const
{
    const Entry* entry = find(key);
    return entry ? entry->value : fallback;
}

Document parse(const std::string& text)
{
    Document doc;

    struct Section {
        int         indent = 0;
        std::string name;
    };
    std::vector<Section> sections;

    std::istringstream stream(text);
    std::string raw;
    int number = 0;

    while (std::getline(stream, raw)) {
        ++number;

        const std::string line = stripComment(raw);
        if (trim(line).empty()) continue;

        int indent = 0;
        size_t pos = 0;
        while (pos < line.size() && isSpace(line[pos])) {
            indent += (line[pos] == '\t') ? 4 : 1;
            ++pos;
        }

        const std::string body = trim(line.substr(pos));
        if (body.empty()) continue;

        const size_t colon = body.find(':');
        if (colon == std::string::npos) {
            doc.error = {number, "expected \"key: value\", got \"" + body + "\""};
            return doc;
        }
        if (body.compare(0, 2, "- ") == 0) {
            doc.error = {number, "lists are not supported; write one key per line"};
            return doc;
        }

        const std::string key = lower(trim(body.substr(0, colon)));
        std::string value = trim(body.substr(colon + 1));
        if (key.empty()) {
            doc.error = {number, "missing key before \":\""};
            return doc;
        }

        std::string quoteError;
        if (!unquote(value, quoteError)) {
            doc.error = {number, quoteError};
            return doc;
        }

        // A bare "key:" opens a section; everything below it is prefixed.
        if (value.empty()) {
            while (!sections.empty() && sections.back().indent >= indent) sections.pop_back();
            sections.push_back({indent, key});
            continue;
        }

        while (!sections.empty() && sections.back().indent >= indent) sections.pop_back();
        if (!sections.empty() && sections.back().indent > indent) {
            doc.error = {number, "unexpected indentation for \"" + key + "\""};
            return doc;
        }

        std::string full;
        for (const auto& section : sections) full += section.name + ".";
        full += key;

        doc.entries.push_back({full, value, number});
    }

    return doc;
}

// ---------------------------------------------------------------------------
// Values
// ---------------------------------------------------------------------------
bool parseColor(const std::string& text, COLORREF& out)
{
    std::string value = trim(text);
    if (!value.empty() && value.front() == '#') value.erase(0, 1);
    if (value.size() != 3 && value.size() != 6) return false;

    for (char c : value) {
        if (!std::isxdigit(static_cast<unsigned char>(c))) return false;
    }

    auto hex = [](char c) {
        if (c >= '0' && c <= '9') return c - '0';
        if (c >= 'a' && c <= 'f') return c - 'a' + 10;
        return c - 'A' + 10;
    };

    if (value.size() == 3) {
        const int r = hex(value[0]) * 17;
        const int g = hex(value[1]) * 17;
        const int b = hex(value[2]) * 17;
        out = RGB(r, g, b);
        return true;
    }

    out = RGB(hex(value[0]) * 16 + hex(value[1]),
              hex(value[2]) * 16 + hex(value[3]),
              hex(value[4]) * 16 + hex(value[5]));
    return true;
}

std::string colorToText(COLORREF color)
{
    char buffer[8] = {};
    std::snprintf(buffer, sizeof(buffer), "#%02x%02x%02x",
                  static_cast<int>(GetRValue(color)),
                  static_cast<int>(GetGValue(color)),
                  static_cast<int>(GetBValue(color)));
    return buffer;
}

bool parseClock(const std::string& text, int& hour, int& minute)
{
    std::string value = trim(text);
    if (value.empty()) return false;

    std::string digits;
    for (char c : value) {
        if (c == ':' || c == '.' || c == 'h' || c == 'H') continue;
        if (!std::isdigit(static_cast<unsigned char>(c))) return false;
        digits.push_back(c);
    }
    if (digits.size() != 3 && digits.size() != 4) return false;

    if (digits.size() == 3) digits.insert(digits.begin(), '0');

    hour   = (digits[0] - '0') * 10 + (digits[1] - '0');
    minute = (digits[2] - '0') * 10 + (digits[3] - '0');
    return hour >= 0 && hour <= 23 && minute >= 0 && minute <= 59;
}

std::string clockToText(int hour, int minute)
{
    char buffer[8] = {};
    std::snprintf(buffer, sizeof(buffer), "%02d:%02d",
                  clampInt(hour, 0, 23), clampInt(minute, 0, 59));
    return buffer;
}

// ---------------------------------------------------------------------------
// Theme
// ---------------------------------------------------------------------------
Theme defaultTheme()
{
    Theme theme;
    theme.name       = "Default";
    theme.bg         = RGB(30, 30, 30);
    theme.bgDeep     = RGB(22, 22, 23);
    theme.panel      = RGB(37, 37, 38);
    theme.panelAlt   = RGB(43, 43, 45);
    theme.input      = RGB(28, 28, 30);
    theme.border     = RGB(58, 58, 62);
    theme.borderHot  = RGB(92, 92, 98);
    theme.text       = RGB(232, 232, 234);
    theme.muted      = RGB(150, 150, 156);
    theme.accent     = RGB(228, 59, 68);
    theme.accentHot  = RGB(241, 82, 91);
    theme.success    = RGB(152, 195, 121);
    theme.danger     = RGB(230, 112, 120);
    theme.sel        = RGB(58, 42, 45);
    theme.warnBg     = RGB(52, 30, 33);
    theme.warnLine   = RGB(116, 48, 54);
    theme.fontFamily = "Segoe UI";
    theme.fontScale  = 100;
    theme.windowWidth    = 960;
    theme.windowHeight   = 740;
    theme.windowMinWidth = 800;
    theme.windowMinHeight = 620;
    return theme;
}

namespace {

bool applyThemeEntry(const Entry& entry, Theme& theme, std::string& error)
{
    const std::string& key = entry.key;

    struct ColorSlot {
        const char* name;
        COLORREF Theme::*member;
    };
    static const ColorSlot colors[] = {
        {"colors.background",      &Theme::bg},
        {"colors.background_deep", &Theme::bgDeep},
        {"colors.panel",           &Theme::panel},
        {"colors.panel_alt",       &Theme::panelAlt},
        {"colors.input",           &Theme::input},
        {"colors.border",          &Theme::border},
        {"colors.border_hot",      &Theme::borderHot},
        {"colors.text",            &Theme::text},
        {"colors.muted",           &Theme::muted},
        {"colors.accent",          &Theme::accent},
        {"colors.accent_hot",      &Theme::accentHot},
        {"colors.success",         &Theme::success},
        {"colors.danger",          &Theme::danger},
        {"colors.selection",       &Theme::sel},
        {"colors.warning_bg",      &Theme::warnBg},
        {"colors.warning_line",    &Theme::warnLine},
    };

    for (const auto& slot : colors) {
        if (key != slot.name) continue;
        COLORREF color{};
        if (!parseColor(entry.value, color)) {
            error = "line " + std::to_string(entry.line) + ": \"" + entry.value +
                    "\" is not a colour (use #rrggbb)";
            return false;
        }
        theme.*(slot.member) = color;
        return true;
    }

    if (key == "name") {
        theme.name = entry.value.empty() ? "Untitled" : entry.value;
        return true;
    }

    if (key == "font.family") {
        theme.fontFamily = entry.value.empty() ? "Segoe UI" : entry.value;
        return true;
    }

    if (key == "font.scale") {
        bool ok = false;
        const int value = toInt(entry.value, ok);
        if (!ok || value < 50 || value > 200) {
            error = "line " + std::to_string(entry.line) +
                    ": font.scale must be a number from 50 to 200";
            return false;
        }
        theme.fontScale = value;
        return true;
    }

    struct SizeSlot {
        const char* name;
        int Theme::*member;
        int low;
        int high;
    };
    static const SizeSlot sizes[] = {
        {"window.width",      &Theme::windowWidth,      640, 4000},
        {"window.height",     &Theme::windowHeight,     520, 4000},
        {"window.min_width",  &Theme::windowMinWidth,   500, 4000},
        {"window.min_height", &Theme::windowMinHeight,  420, 4000},
    };

    for (const auto& slot : sizes) {
        if (key != slot.name) continue;
        bool ok = false;
        const int value = toInt(entry.value, ok);
        if (!ok || value < slot.low || value > slot.high) {
            error = "line " + std::to_string(entry.line) + ": " + slot.name +
                    " must be a number from " + std::to_string(slot.low) +
                    " to " + std::to_string(slot.high);
            return false;
        }
        theme.*(slot.member) = value;
        return true;
    }

    // version and anything unknown are ignored on purpose, so an old build can
    // still load a file written for a newer one.
    return true;
}

} // namespace

bool loadThemeFile(const fs::path& path, Theme& out, std::string& error)
{
    bool readOk = false;
    const std::string text = readTextFile(path, readOk);
    if (!readOk) {
        error = "cannot read " + pathToUtf8(path);
        return false;
    }

    const Document doc = parse(text);
    if (!doc.ok()) {
        error = "line " + std::to_string(doc.error.line) + ": " + doc.error.message;
        return false;
    }

    Theme theme = defaultTheme();
    for (const auto& entry : doc.entries) {
        if (!applyThemeEntry(entry, theme, error)) return false;
    }

    if (theme.windowMinWidth > theme.windowWidth) theme.windowMinWidth = theme.windowWidth;
    if (theme.windowMinHeight > theme.windowHeight) theme.windowMinHeight = theme.windowHeight;

    out = theme;
    return true;
}

std::string themeToText(const Theme& theme)
{
    std::ostringstream out;
    out << "# DRBackupCreator theme file (.bctheme)\n"
        << "# Reference: docs/FORMATS.md\n"
        << "\n"
        << "name: " << theme.name << "\n"
        << "version: 1\n"
        << "\n"
        << "colors:\n"
        << "  background:       \"" << colorToText(theme.bg)        << "\"\n"
        << "  background_deep:  \"" << colorToText(theme.bgDeep)    << "\"\n"
        << "  panel:            \"" << colorToText(theme.panel)     << "\"\n"
        << "  panel_alt:        \"" << colorToText(theme.panelAlt)  << "\"\n"
        << "  input:            \"" << colorToText(theme.input)     << "\"\n"
        << "  border:           \"" << colorToText(theme.border)    << "\"\n"
        << "  border_hot:       \"" << colorToText(theme.borderHot) << "\"\n"
        << "  text:             \"" << colorToText(theme.text)      << "\"\n"
        << "  muted:            \"" << colorToText(theme.muted)     << "\"\n"
        << "  accent:           \"" << colorToText(theme.accent)    << "\"\n"
        << "  accent_hot:       \"" << colorToText(theme.accentHot) << "\"\n"
        << "  success:          \"" << colorToText(theme.success)   << "\"\n"
        << "  danger:           \"" << colorToText(theme.danger)    << "\"\n"
        << "  selection:        \"" << colorToText(theme.sel)       << "\"\n"
        << "  warning_bg:       \"" << colorToText(theme.warnBg)    << "\"\n"
        << "  warning_line:     \"" << colorToText(theme.warnLine)  << "\"\n"
        << "\n"
        << "font:\n"
        << "  family: " << theme.fontFamily << "\n"
        << "  scale: " << theme.fontScale << "\n"
        << "\n"
        << "window:\n"
        << "  width: " << theme.windowWidth << "\n"
        << "  height: " << theme.windowHeight << "\n"
        << "  min_width: " << theme.windowMinWidth << "\n"
        << "  min_height: " << theme.windowMinHeight << "\n";
    return out.str();
}

bool saveThemeFile(const fs::path& path, const Theme& theme, std::string& error)
{
    std::error_code ec;
    if (!path.parent_path().empty()) fs::create_directories(path.parent_path(), ec);

    std::ofstream file(path, std::ios::binary | std::ios::trunc);
    if (!file) {
        error = "cannot write " + pathToUtf8(path);
        return false;
    }
    const std::string text = themeToText(theme);
    file.write(text.data(), static_cast<std::streamsize>(text.size()));
    if (!file) {
        error = "cannot write " + pathToUtf8(path);
        return false;
    }
    return true;
}

// ---------------------------------------------------------------------------
// App config
// ---------------------------------------------------------------------------
bool loadConfigFile(const fs::path& path, AppConfig& out, std::string& error)
{
    bool readOk = false;
    const std::string text = readTextFile(path, readOk);
    if (!readOk) {
        error = "cannot read " + pathToUtf8(path);
        return false;
    }

    const Document doc = parse(text);
    if (!doc.ok()) {
        error = "line " + std::to_string(doc.error.line) + ": " + doc.error.message;
        return false;
    }

    AppConfig config;

    if (doc.has("language")) {
        const std::string value = lower(doc.get("language"));
        if (value != "es" && value != "en") {
            error = "line " + std::to_string(doc.find("language")->line) +
                    ": language must be \"es\" or \"en\"";
            return false;
        }
        config.language = value;
    }

    config.themePath = doc.get("theme");
    config.backupDir = doc.get("backup_dir");

    if (doc.has("schedule.enabled")) {
        const std::string value = lower(doc.get("schedule.enabled"));
        if (value == "true" || value == "yes" || value == "on" || value == "1") {
            config.scheduleEnabled = true;
        } else if (value == "false" || value == "no" || value == "off" || value == "0") {
            config.scheduleEnabled = false;
        } else {
            error = "line " + std::to_string(doc.find("schedule.enabled")->line) +
                    ": schedule.enabled must be true or false";
            return false;
        }
    }

    if (doc.has("schedule.time")) {
        int hour = 0;
        int minute = 0;
        if (!parseClock(doc.get("schedule.time"), hour, minute)) {
            error = "line " + std::to_string(doc.find("schedule.time")->line) +
                    ": schedule.time must look like 21:30";
            return false;
        }
        config.scheduleTime = clockToText(hour, minute);
    }

    out = config;
    return true;
}

std::string configToText(const AppConfig& config)
{
    std::ostringstream out;
    out << "# DRBackupCreator configuration (.bcconfig)\n"
        << "# Reference: docs/FORMATS.md\n"
        << "\n"
        << "version: 1\n"
        << "\n"
        << "# \"es\" or \"en\". Remove the line to keep the current language.\n"
        << "language: " << (config.language.empty() ? "es" : config.language) << "\n"
        << "\n"
        << "# Colours, fonts and window size.\n"
        << "# Relative paths are resolved against this file.\n"
        << "theme: " << (config.themePath.empty() ? "default" : config.themePath) << "\n"
        << "\n"
        << "# Where backups are stored. Empty = %LOCALAPPDATA%\\DRBackupCreator\n"
        << "backup_dir: " << config.backupDir << "\n"
        << "\n"
        << "schedule:\n"
        << "  enabled: " << (config.scheduleEnabled ? "true" : "false") << "\n"
        << "  time: \"" << (config.scheduleTime.empty() ? "21:00" : config.scheduleTime)
        << "\"\n";
    return out.str();
}

bool saveConfigFile(const fs::path& path, const AppConfig& config, std::string& error)
{
    std::error_code ec;
    if (!path.parent_path().empty()) fs::create_directories(path.parent_path(), ec);

    std::ofstream file(path, std::ios::binary | std::ios::trunc);
    if (!file) {
        error = "cannot write " + pathToUtf8(path);
        return false;
    }
    const std::string text = configToText(config);
    file.write(text.data(), static_cast<std::streamsize>(text.size()));
    if (!file) {
        error = "cannot write " + pathToUtf8(path);
        return false;
    }
    return true;
}

} // namespace cfg
