#include "settings.h"

#include <windows.h>

#include "drbackup.h"
#include "schedule.h"
#include "ui/i18n.h"

namespace app {

namespace {

std::wstring iniPath()
{
    auto dir = getSettingsDir(true);
    if (!dir) return {};
    return dir->wstring() + L"\\settings.ini";
}

std::wstring readValue(const wchar_t* section, const wchar_t* key)
{
    const std::wstring path = iniPath();
    if (path.empty()) return {};

    wchar_t buffer[1024] = {};
    GetPrivateProfileStringW(section, key, L"", buffer, 1024, path.c_str());
    return buffer;
}

void writeValue(const wchar_t* section, const wchar_t* key, const std::wstring& value)
{
    const std::wstring path = iniPath();
    if (path.empty()) return;
    // An empty string removes the key, which is exactly what "use the default"
    // should do.
    WritePrivateProfileStringW(section, key, value.c_str(), path.c_str());
}

} // namespace

std::wstring SettingsFile()
{
    return iniPath();
}

Settings Load()
{
    Settings settings;

    settings.themePath = readValue(L"theme", L"path");
    settings.backupDir = readValue(L"paths", L"backup_dir");

    const std::wstring time = readValue(L"schedule", L"time");
    if (!time.empty()) settings.scheduleTime = time;
    settings.scheduleEnabled = readValue(L"schedule", L"enabled") == L"1";

    return settings;
}

void SaveThemePath(const std::wstring& path)
{
    writeValue(L"theme", L"path", path);
}

void SaveBackupDir(const std::wstring& path)
{
    writeValue(L"paths", L"backup_dir", path);
}

void SaveSchedule(bool enabled, const std::wstring& time)
{
    writeValue(L"schedule", L"enabled", enabled ? L"1" : L"");
    writeValue(L"schedule", L"time", time);
}

cfg::Theme LoadTheme(std::string* error)
{
    const Settings settings = Load();
    if (settings.themePath.empty()) return cfg::defaultTheme();

    cfg::Theme theme;
    std::string message;
    if (!cfg::loadThemeFile(settings.themePath, theme, message)) {
        if (error) *error = message;
        return cfg::defaultTheme();
    }
    return theme;
}

// ---------------------------------------------------------------------------
// .bcconfig
// ---------------------------------------------------------------------------
bool ImportConfig(const std::filesystem::path& file, std::wstring& message)
{
    cfg::AppConfig config;
    std::string error;
    if (!cfg::loadConfigFile(file, config, error)) {
        message = cfg::widen(error);
        return false;
    }

    // Relative paths inside a .bcconfig are relative to the file itself, so a
    // folder with a theme + config can be moved around (or shipped together).
    const std::filesystem::path base = file.parent_path();
    auto resolve = [&base](const std::string& value) -> std::filesystem::path {
        if (value.empty()) return {};
        std::filesystem::path path = cfg::widen(value);
        if (path.is_relative() && !base.empty()) path = base / path;
        return path;
    };

    std::wstring themePath;
    {
        const std::filesystem::path path = resolve(config.themePath);
        const std::wstring raw = path.wstring();
        if (!raw.empty() && _wcsicmp(raw.c_str(), L"default") != 0) {
            // Refuse a broken theme instead of starting with an unusable UI.
            cfg::Theme probe;
            std::string themeError;
            if (!cfg::loadThemeFile(path, probe, themeError)) {
                message = cfg::widen(themeError);
                return false;
            }
            themePath = raw;
        }
    }

    if (!config.language.empty()) {
        const ui::Lang lang = config.language == "en" ? ui::Lang::EN : ui::Lang::ES;
        ui::SetLang(lang);
        ui::SaveLang(lang);
    }

    const std::filesystem::path backupDir = resolve(config.backupDir);
    SaveBackupDir(backupDir.wstring());
    SaveThemePath(themePath);

    const std::wstring time = cfg::widen(
        config.scheduleTime.empty() ? std::string("21:00") : config.scheduleTime);
    SaveSchedule(config.scheduleEnabled, time);

    if (config.scheduleEnabled) {
        std::string scheduleError;
        if (!sched::enable(cfg::narrow(time), scheduleError)) {
            message = cfg::widen(scheduleError);
            return false;
        }
    } else {
        std::string scheduleError;
        if (!sched::disable(scheduleError)) {
            message = cfg::widen(scheduleError);
            return false;
        }
    }

    message.clear();
    return true;
}

bool ExportConfig(const std::filesystem::path& file, std::wstring& message)
{
    const Settings settings = Load();

    cfg::AppConfig config;
    config.language     = ui::GetLang() == ui::Lang::EN ? "en" : "es";
    config.themePath    = cfg::narrow(settings.themePath);
    config.backupDir    = cfg::narrow(settings.backupDir);
    config.scheduleEnabled = settings.scheduleEnabled;
    config.scheduleTime = cfg::narrow(settings.scheduleTime);

    std::string error;
    if (!cfg::saveConfigFile(file, config, error)) {
        message = cfg::widen(error);
        return false;
    }
    message.clear();
    return true;
}

} // namespace app
