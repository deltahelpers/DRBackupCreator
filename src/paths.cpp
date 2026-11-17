#include "drbackup.h"

#include <windows.h>

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

std::optional<fs::path> getBackupDir(bool createIfMissing)
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
