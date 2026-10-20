#include "drbackup.h"
#include <cstdlib>
#include <cwctype>

std::optional<fs::path> getLocalAppData()
{
    if (const wchar_t* lad = _wgetenv(L"LOCALAPPDATA"); lad && *lad)
        return fs::path{ lad };

    if (const wchar_t* up = _wgetenv(L"USERPROFILE"); up && *up)
        return fs::path{ up } / L"AppData" / L"Local";

    return std::nullopt;
}

std::optional<fs::path> getDeltaruneDir(bool crearSiFalta)
{
    auto base = getLocalAppData();
    if (!base) return std::nullopt;

    fs::path dir = *base / L"DELTARUNE";

    std::error_code ec;
    if (!fs::exists(dir, ec)) {
        if (!crearSiFalta) return std::nullopt;
        fs::create_directories(dir, ec);
        if (ec) return std::nullopt;
    }
    return dir;
}