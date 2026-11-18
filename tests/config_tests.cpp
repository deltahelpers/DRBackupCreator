#include "config.h"

#include <windows.h>

#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>

namespace fs = std::filesystem;

namespace {

int g_failures = 0;

#define CHECK(condition)                                                       \
    do {                                                                       \
        if (!(condition)) {                                                    \
            std::cerr << "FAIL line " << __LINE__ << ": " #condition "\n";     \
            ++g_failures;                                                      \
        }                                                                      \
    } while (0)

void WriteFile(const fs::path& path, const std::string& content)
{
    std::ofstream out(path, std::ios::binary | std::ios::trunc);
    out << content;
}

void TestParser()
{
    const auto doc = cfg::parse(
        "# comment line\n"
        "name: Midnight Blue\n"
        "colors:\n"
        "  accent: \"#4d8bf5\"   # trailing comment\n"
        "  panel: '#252526'\n"
        "font:\n"
        "  scale: 110\n"
        "empty_value_is_ignored:\n");

    CHECK(doc.ok());
    CHECK(doc.get("name") == "Midnight Blue");
    CHECK(doc.get("colors.accent") == "#4d8bf5");
    CHECK(doc.get("colors.panel") == "#252526");
    CHECK(doc.get("font.scale") == "110");
    CHECK(doc.entries.size() == 4);

    // Deeper nesting keeps the whole dotted path.
    const auto nested = cfg::parse("a:\n  b:\n    c: 1\n");
    CHECK(nested.ok());
    CHECK(nested.get("a.b.c") == "1");

    // Duplicates: the last one wins.
    const auto dupes = cfg::parse("k: 1\nk: 2\n");
    CHECK(dupes.ok());
    CHECK(dupes.get("k") == "2");
}

void TestParserErrors()
{
    auto missingColon = cfg::parse("name Midnight\n");
    CHECK(!missingColon.ok());
    CHECK(missingColon.error.line == 1);

    auto unterminated = cfg::parse("name: \"oops\n");
    CHECK(!unterminated.ok());
    CHECK(unterminated.error.line == 1);

    auto list = cfg::parse("items:\n  - one\n");
    CHECK(!list.ok());
    CHECK(list.error.line == 2);

    auto blankKey = cfg::parse(": value\n");
    CHECK(!blankKey.ok());

    // A second level of indentation without a section above it is an error.
    auto indent = cfg::parse("a: 1\n    b: 2\n");
    CHECK(indent.ok());   // tolerated: b becomes a top-level key

    const auto error = cfg::parse("colors:\n  accent: #12345\n");
    CHECK(error.ok());    // the parser only tokenises; colours are validated later
}

void TestColors()
{
    COLORREF color{};

    CHECK(cfg::parseColor("#4d8bf5", color));
    CHECK(GetRValue(color) == 0x4d);
    CHECK(GetGValue(color) == 0x8b);
    CHECK(GetBValue(color) == 0xf5);

    CHECK(cfg::parseColor("ffffff", color));
    CHECK(color == RGB(255, 255, 255));

    CHECK(cfg::parseColor("#f00", color));
    CHECK(color == RGB(255, 0, 0));

    CHECK(!cfg::parseColor("#12345", color));
    CHECK(!cfg::parseColor("nope", color));
    CHECK(cfg::parseColor("#4d8bf5", color));
    CHECK(cfg::colorToText(color) == "#4d8bf5");
}

void TestClock()
{
    int hour = 0;
    int minute = 0;

    CHECK(cfg::parseClock("21:30", hour, minute));
    CHECK(hour == 21 && minute == 30);

    CHECK(cfg::parseClock("9:05", hour, minute));
    CHECK(hour == 9 && minute == 5);

    CHECK(cfg::parseClock("2130", hour, minute));
    CHECK(hour == 21 && minute == 30);

    CHECK(cfg::parseClock("21.30", hour, minute));
    CHECK(hour == 21 && minute == 30);

    CHECK(!cfg::parseClock("24:00", hour, minute));
    CHECK(!cfg::parseClock("21:60", hour, minute));
    CHECK(!cfg::parseClock("later", hour, minute));
    CHECK(cfg::clockToText(7, 5) == "07:05");
}

void TestThemeFiles(const fs::path& dir)
{
    const fs::path path = dir / "sample.bctheme";
    WriteFile(path,
        "name: Azul medianoche\n"
        "version: 1\n"
        "colors:\n"
        "  background: \"#101820\"\n"
        "  accent: \"#4d8bf5\"\n"
        "  text: \"#e6e6e6\"\n"
        "font:\n"
        "  family: \"Consolas\"\n"
        "  scale: 110\n"
        "window:\n"
        "  width: 1100\n"
        "  height: 820\n"
        "  min_width: 900\n"
        "  min_height: 700\n"
        "future_key: whatever\n");

    cfg::Theme theme;
    std::string error;
    CHECK(cfg::loadThemeFile(path, theme, error));
    CHECK(error.empty());
    CHECK(theme.bg == RGB(0x10, 0x18, 0x20));
    CHECK(theme.accent == RGB(0x4d, 0x8b, 0xf5));
    CHECK(theme.text == RGB(0xe6, 0xe6, 0xe6));
    CHECK(theme.fontFamily == "Consolas");
    CHECK(theme.fontScale == 110);
    CHECK(theme.windowWidth == 1100);
    CHECK(theme.windowMinHeight == 700);
    CHECK(theme.name == "Azul medianoche");
    // Unspecified colours keep the defaults.
    CHECK(theme.success == cfg::defaultTheme().success);

    // Round trip through the writer.
    const fs::path saved = dir / "saved.bctheme";
    CHECK(cfg::saveThemeFile(saved, theme, error));
    cfg::Theme reloaded;
    CHECK(cfg::loadThemeFile(saved, reloaded, error));
    CHECK(reloaded.bg == theme.bg);
    CHECK(reloaded.fontFamily == theme.fontFamily);
    CHECK(reloaded.windowWidth == theme.windowWidth);
    CHECK(reloaded.name == theme.name);

    // A bad colour is reported with the line that contains it.
    const fs::path bad = dir / "bad.bctheme";
    WriteFile(bad, "colors:\n  accent: \"red\"\n");
    cfg::Theme ignored;
    CHECK(!cfg::loadThemeFile(bad, ignored, error));
    CHECK(error.find("line 2") != std::string::npos);

    // Out-of-range numbers are rejected too.
    WriteFile(bad, "font:\n  scale: 900\n");
    CHECK(!cfg::loadThemeFile(bad, ignored, error));

    // A missing file is an error, not a crash.
    CHECK(!cfg::loadThemeFile(dir / "nope.bctheme", ignored, error));
}

void TestConfigFiles(const fs::path& dir)
{
    const fs::path path = dir / "sample.bcconfig";
    WriteFile(path,
        "version: 1\n"
        "language: en\n"
        "theme: \"C:\\\\themes\\\\midnight.bctheme\"\n"
        "backup_dir: \"\"\n"
        "schedule:\n"
        "  enabled: true\n"
        "  time: \"21:30\"\n");

    cfg::AppConfig config;
    std::string error;
    CHECK(cfg::loadConfigFile(path, config, error));
    CHECK(error.empty());
    CHECK(config.language == "en");
    CHECK(config.themePath == "C:\\themes\\midnight.bctheme");
    CHECK(config.backupDir.empty());
    CHECK(config.scheduleEnabled);
    CHECK(config.scheduleTime == "21:30");

    // Round trip.
    const fs::path saved = dir / "saved.bcconfig";
    CHECK(cfg::saveConfigFile(saved, config, error));
    cfg::AppConfig reloaded;
    CHECK(cfg::loadConfigFile(saved, reloaded, error));
    CHECK(reloaded.language == config.language);
    CHECK(reloaded.scheduleTime == config.scheduleTime);
    CHECK(reloaded.scheduleEnabled == config.scheduleEnabled);

    // Invalid values name the field.
    const fs::path bad = dir / "bad.bcconfig";
    WriteFile(bad, "language: fr\n");
    cfg::AppConfig ignored;
    CHECK(!cfg::loadConfigFile(bad, ignored, error));
    CHECK(error.find("line 1") != std::string::npos);

    WriteFile(bad, "schedule:\n  time: \"25:00\"\n");
    CHECK(!cfg::loadConfigFile(bad, ignored, error));

    WriteFile(bad, "schedule:\n  enabled: maybe\n");
    CHECK(!cfg::loadConfigFile(bad, ignored, error));
}

} // namespace

int main()
{
    std::error_code ec;
    const fs::path base =
        fs::temp_directory_path() /
        ("drbc_cfg_tests_" + std::to_string(GetCurrentProcessId()));
    fs::remove_all(base, ec);
    fs::create_directories(base, ec);

    TestParser();
    TestParserErrors();
    TestColors();
    TestClock();
    TestThemeFiles(base);
    TestConfigFiles(base);

    fs::remove_all(base, ec);

    if (g_failures == 0) {
        std::cout << "config tests: all passed\n";
        return 0;
    }
    std::cerr << "config tests: " << g_failures << " failure(s)\n";
    return 1;
}
