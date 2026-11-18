#pragma once

// Everything the app persists in %LOCALAPPDATA%\DRBackupCreator\settings.ini.
// The language lives there too, but stays owned by ui/i18n.cpp.

#include <filesystem>
#include <string>

#include "config.h"

namespace app {

struct Settings {
    std::wstring themePath;       // empty = built-in palette
    std::wstring backupDir;       // empty = %LOCALAPPDATA%\DRBackupCreator
    std::wstring scheduleTime = L"21:00";
    bool         scheduleEnabled = false;
};

Settings Load();

void SaveThemePath(const std::wstring& path);
void SaveBackupDir(const std::wstring& path);
void SaveSchedule(bool enabled, const std::wstring& time);

// Reads settings.ini and returns the palette to use. Never fails: a missing or
// broken theme file falls back to the built-in one.
cfg::Theme LoadTheme(std::string* error = nullptr);

std::wstring SettingsFile();

// .bcconfig support. `message` receives the error text on failure, and a short
// summary on success.
bool ImportConfig(const std::filesystem::path& file, std::wstring& message);
bool ExportConfig(const std::filesystem::path& file, std::wstring& message);

} // namespace app
