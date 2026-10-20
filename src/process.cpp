#include "drbackup.h"
#include <windows.h>
#include <tlhelp32.h>
#include <cwctype>

inline bool drbc_iequalsW(const wchar_t* a, const wchar_t* b)
{
    while (*a && *b) {
        if (towlower(*a) != towlower(*b)) return false;
        ++a; ++b;
    }
    return *a == *b;
}

bool isGameRunning()
{
    static constexpr const wchar_t* kNames[] = {
        L"DELTARUNE.exe",
        L"deltarune.exe"
    };

    HANDLE snap = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    if (snap == INVALID_HANDLE_VALUE) return false;

    PROCESSENTRY32W pe{};
    pe.dwSize = sizeof(pe);

    bool found = false;
    if (Process32FirstW(snap, &pe)) {
        do {
            for (auto* n : kNames) {
                if (drbc_iequalsW(pe.szExeFile, n)) { found = true; break; }
            }
            if (found) break;
        } while (Process32NextW(snap, &pe));
    }
    CloseHandle(snap);
    return found;
}