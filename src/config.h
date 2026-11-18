#pragma once

// Reader/writer for the two text formats this app understands:
//
//   .bctheme   - colours, fonts and window geometry
//   .bcconfig  - language, theme file, backup folder and the daily schedule
//
// Both use the same small YAML-like syntax (see docs/FORMATS.md):
//
//   # comment
//   name: Midnight Blue
//   colors:
//     accent: "#4d8bf5"
//
// Nothing here touches the UI, so it is unit tested on its own.

#include <windows.h>

#include <filesystem>
#include <optional>
#include <string>
#include <vector>

namespace fs = std::filesystem;

namespace cfg {

// ---------------------------------------------------------------------------
// Document
// ---------------------------------------------------------------------------
struct Entry {
    std::string key;    // dotted, lower case: "colors.accent"
    std::string value;  // unquoted, trimmed
    int         line;   // 1-based, for error messages
};

struct ParseError {
    int         line = 0;
    std::string message;

    [[nodiscard]] bool ok() const { return line == 0 && message.empty(); }
};

struct Document {
    std::vector<Entry> entries;
    ParseError         error;

    [[nodiscard]] bool ok() const { return error.ok(); }
    [[nodiscard]] const Entry* find(const std::string& key) const;
    [[nodiscard]] bool        has(const std::string& key) const;
    [[nodiscard]] std::string get(const std::string& key,
                                  const std::string& fallback = {}) const;
};

// Parses text; on failure doc.error points at the offending line.
Document parse(const std::string& text);

// ---------------------------------------------------------------------------
// Values
// ---------------------------------------------------------------------------
// Accepts "#RRGGBB", "#RGB" and the same without '#'.
bool parseColor(const std::string& text, COLORREF& out);
std::string colorToText(COLORREF color);   // "#rrggbb"

// Accepts "21:30", "9:05", "21.30" and "2130".
bool parseClock(const std::string& text, int& hour, int& minute);
std::string clockToText(int hour, int minute);

// ---------------------------------------------------------------------------
// Theme
// ---------------------------------------------------------------------------
struct Theme {
    std::string name = "Default";

    COLORREF bg{};
    COLORREF bgDeep{};
    COLORREF panel{};
    COLORREF panelAlt{};
    COLORREF input{};
    COLORREF border{};
    COLORREF borderHot{};
    COLORREF text{};
    COLORREF muted{};
    COLORREF accent{};
    COLORREF accentHot{};
    COLORREF success{};
    COLORREF danger{};
    COLORREF sel{};
    COLORREF warnBg{};
    COLORREF warnLine{};

    std::string fontFamily = "Segoe UI";
    int         fontScale = 100;      // percent, 50..200

    int windowWidth = 960;
    int windowHeight = 740;
    int windowMinWidth = 800;
    int windowMinHeight = 620;
};

Theme defaultTheme();

// Both return false and fill `error` (with a line number when available).
bool loadThemeFile(const fs::path& path, Theme& out, std::string& error);
std::string themeToText(const Theme& theme);
bool saveThemeFile(const fs::path& path, const Theme& theme, std::string& error);

// ---------------------------------------------------------------------------
// App config
// ---------------------------------------------------------------------------
struct AppConfig {
    std::string language;                 // "es" / "en" / empty = leave as is
    std::string themePath;                // empty = built-in palette
    std::string backupDir;                // empty = default location
    bool        scheduleEnabled = false;
    std::string scheduleTime;             // "HH:MM", empty = 21:00
};

bool loadConfigFile(const fs::path& path, AppConfig& out, std::string& error);
std::string configToText(const AppConfig& config);
bool saveConfigFile(const fs::path& path, const AppConfig& config, std::string& error);

// ---------------------------------------------------------------------------
// Small helpers shared by the UI
// ---------------------------------------------------------------------------
std::string readTextFile(const fs::path& path, bool& ok);
std::string trim(const std::string& text);
std::wstring widen(const std::string& text);   // UTF-8 -> UTF-16
std::string  narrow(const std::wstring& text); // UTF-16 -> UTF-8

} // namespace cfg
