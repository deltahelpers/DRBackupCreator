#include "drbackup.h"

#include <windows.h>

#include <iterator>

namespace {

std::optional<fs::path> getEnvPath(const wchar_t* name)
{
    DWORD needed = GetEnvironmentVariableW(name, nullptr, 0);
    if (needed == 0) return std::nullopt;

    std::wstring buffer(needed, L'\0');
    DWORD written = GetEnvironmentVariableW(name, buffer.data(), needed);
    if (written == 0 || written >= needed) return std::nullopt;

    buffer.resize(written);
    if (buffer.empty()) return std::nullopt;
    return fs::path{ buffer };
}

} // namespace

std::optional<fs::path> getLocalAppData()
{
    if (auto lad = getEnvPath(L"LOCALAPPDATA")) return lad;

    if (auto up = getEnvPath(L"USERPROFILE"))
        return *up / L"AppData" / L"Local";

    return std::nullopt;
}

std::optional<fs::path> getDeltaruneDir(bool createIfMissing)
{
    auto base = getLocalAppData();
    if (!base) return std::nullopt;

    fs::path dir = *base / L"DELTARUNE";

    std::error_code ec;
    if (!fs::exists(dir, ec)) {
        if (!createIfMissing) return std::nullopt;
        fs::create_directories(dir, ec);
        if (ec) return std::nullopt;
    }
    return dir;
}

std::optional<fs::path> getSettingsDir(bool createIfMissing)
{
    auto base = getLocalAppData();
    if (!base) return std::nullopt;

    fs::path dir = *base / L"DRBackupCreator";

    std::error_code ec;
    if (!fs::exists(dir, ec)) {
        if (!createIfMissing) return std::nullopt;
        fs::create_directories(dir, ec);
        if (ec) return std::nullopt;
    }
    return dir;
}

namespace {

// [paths] backup_dir in settings.ini, with %VARS% expanded.
std::wstring configuredBackupDir()
{
    auto settings = getSettingsDir(false);
    if (!settings) return {};

    wchar_t buffer[1024] = {};
    const std::wstring ini = settings->wstring() + L"\\settings.ini";
    const DWORD length =
        GetPrivateProfileStringW(L"paths", L"backup_dir", L"", buffer, 1024, ini.c_str());
    if (length == 0) return {};

    wchar_t expanded[1024] = {};
    const DWORD written =
        ExpandEnvironmentStringsW(buffer, expanded, static_cast<DWORD>(std::size(expanded)));
    if (written == 0 || written > std::size(expanded)) return buffer;
    return expanded;
}

} // namespace

std::optional<fs::path> getBackupDir(bool createIfMissing)
{
    fs::path dir;

    const std::wstring configured = configuredBackupDir();
    if (!configured.empty()) {
        dir = fs::path(configured);
    } else {
        auto base = getSettingsDir(createIfMissing);
        if (!base) return std::nullopt;
        dir = *base;
    }

    std::error_code ec;
    if (!fs::exists(dir, ec)) {
        if (!createIfMissing) return std::nullopt;
        fs::create_directories(dir, ec);
        if (ec) return std::nullopt;
    }
    return dir;
}
