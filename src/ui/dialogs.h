#pragma once

#include <windows.h>
#include <filesystem>
#include <functional>
#include <string>

#include "drbackup.h"
#include "i18n.h"

namespace ui {

bool ConfirmDialog(HWND owner,
                   const wchar_t* title,
                   const std::wstring& message,
                   const wchar_t* action,
                   bool danger);

Lang PickLanguage(HWND owner);

void ShowAboutDialog(HWND owner);

void ShowSettingsDialog(HWND owner, std::function<void()> onApplied);

void ShowBackupsDialog(HWND owner,
                       const fs::path& saveDir,
                       std::function<void()> onRestored);

} // namespace ui
