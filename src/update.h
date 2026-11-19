#pragma once

// Network side of the update checker. Everything here talks to
// api.github.com over WinHTTP, which ships with Windows, and verifies the
// downloaded installer with the SHA-256 GitHub publishes on each asset.

#include <filesystem>
#include <functional>
#include <string>

#include "update_parse.h"

namespace upd {

// Synchronous GET of /repos/deltahelpers/DRBackupCreator/releases/latest.
// Never throws; `error` receives a readable reason on failure.
bool FetchLatestRelease(ReleaseInfo& info, std::string& error);

// Downloads `url` into `dest` (overwriting). `progress` receives 0..100, or -1
// when the server does not report a size. `cancel` is polled between chunks;
// returning true aborts the transfer. Redirects are followed.
bool DownloadFile(const std::string& url, const std::filesystem::path& dest,
                  const std::function<void(int)>& progress,
                  const std::function<bool()>& cancel,
                  std::string& error);

// Compares the file against "sha256:<hex>". An empty digest counts as success
// (older GitHub responses do not include one).
bool VerifyDigest(const std::filesystem::path& file, const std::string& digest,
                  std::string& error);

// Starts the downloaded Inno installer silently. It closes the running app
// itself (/CLOSEAPPLICATIONS), so callers should quit right after this.
bool RunInstaller(const std::filesystem::path& installer, std::string& error);

// %LOCALAPPDATA%\DRBackupCreator\update, created on demand.
std::filesystem::path UpdateDir(bool create);

} // namespace upd
