#pragma once

#include <windows.h>
#include <filesystem>
#include <functional>
#include <string>

#include "drbackup.h"
#include "i18n.h"
#include "update_parse.h"

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

// Shows the release notes and returns true when the user chose to install.
bool ShowUpdateDialog(HWND owner, const upd::ReleaseInfo& info, bool allowSkip);

// Downloads and starts the installer. Returns true when the installer was
// launched, in which case the caller must close the main window.
bool RunDownloadAndInstall(HWND owner, const upd::ReleaseInfo& info);

} // namespace ui
