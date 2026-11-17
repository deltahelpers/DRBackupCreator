#include "drbackup.h"

#include <algorithm>
#include <chrono>
#include <ctime>
#include <iomanip>
#include <set>
#include <sstream>

#include <windows.h>

std::string formatFileSize(uintmax_t bytes)
{
    const char* units[] = { "B", "KB", "MB", "GB" };
    int unit = 0;
    double size = static_cast<double>(bytes);

    while (size >= 1024.0 && unit < 3) { size /= 1024.0; unit++; }

    std::ostringstream oss;
    oss << std::fixed << std::setprecision(1) << size << " " << units[unit];
    return oss.str();
}

std::string getCurrentTimestamp()
{
    auto now = std::chrono::system_clock::now();
    auto time_t_now = std::chrono::system_clock::to_time_t(now);

    std::tm tm_buf{};
#if defined(_WIN32)
    localtime_s(&tm_buf, &time_t_now);
#else
    localtime_r(&time_t_now, &tm_buf);
#endif

    char buf[32];
    std::strftime(buf, sizeof(buf), "%Y-%m-%d_%H-%M-%S", &tm_buf);
    return std::string(buf);
}

std::string createBackup(const fs::path& backupDir, const std::vector<SlotFile>& slots)
{
    if (slots.empty()) return "";

    std::error_code ec;
    fs::create_directories(backupDir, ec);
    if (ec) return "";

    const std::string timestamp = getCurrentTimestamp();
    std::string backupName = "backup_" + timestamp;
    fs::path fullBackupPath = backupDir / backupName;

    // Avoid silently merging into an existing backup created in the same second.
    for (int suffix = 2; fs::exists(fullBackupPath, ec); ++suffix) {
        backupName = "backup_" + timestamp + "_" + std::to_string(suffix);
        fullBackupPath = backupDir / backupName;
    }

    ec.clear();
    fs::create_directories(fullBackupPath, ec);
    if (ec) return "";

    bool ok = true;
    for (const auto& s : slots) {
        fs::path dest = fullBackupPath / s.path.filename();
        // CopyFileW is used instead of fs::copy_file because some MinGW
        // libstdc++ builds ignore copy_options::overwrite_existing.
        if (!CopyFileW(s.path.c_str(), dest.c_str(), FALSE)) { ok = false; break; }
    }

    if (!ok) {
        std::error_code removeEc;
        fs::remove_all(fullBackupPath, removeEc);
        return "";
    }

    return backupName;
}

std::vector<BackupInfo> listBackups(const fs::path& parentDir)
{
    std::vector<BackupInfo> out;
    std::error_code ec;

    if (!fs::is_directory(parentDir, ec)) return out;

    for (const auto& entry : fs::directory_iterator(parentDir, ec)) {
        std::error_code entryEc;
        if (!entry.is_directory(entryEc)) continue;

        const std::string name = entry.path().filename().string();
        if (name.rfind("backup_", 0) != 0) continue;

        BackupInfo info;
        info.path      = entry.path();
        info.timestamp = name.substr(7);

        std::error_code dirEc;
        for (const auto& f : fs::directory_iterator(entry.path(), dirEc)) {
            std::error_code fileEc;
            if (f.is_regular_file(fileEc)) {
                info.slotCount++;
                info.totalSize += f.file_size(fileEc);
            }
        }
        out.push_back(info);
    }

    std::sort(out.begin(), out.end(), [](const BackupInfo& a, const BackupInfo& b) {
        return a.timestamp > b.timestamp;
    });

    return out;
}

std::vector<BackupInfo> listAllBackups()
{
    std::vector<BackupInfo> out;

    fs::path primary;
    if (auto dir = getBackupDir(false)) {
        primary = *dir;
        out = listBackups(primary);
    }

    // Backups written by the old CLI lived directly in %LOCALAPPDATA%.
    if (auto local = getLocalAppData()) {
        std::set<std::wstring> seen;
        for (const auto& b : out) seen.insert(b.path.filename().wstring());

        for (auto& legacy : listBackups(*local)) {
            const std::wstring name = legacy.path.filename().wstring();
            if (seen.count(name)) continue;
            seen.insert(name);
            out.push_back(std::move(legacy));
        }
    }

    std::sort(out.begin(), out.end(), [](const BackupInfo& a, const BackupInfo& b) {
        return a.timestamp > b.timestamp;
    });

    return out;
}

bool restoreBackup(const fs::path& backupPath, const fs::path& targetDir, int& restoredCount)
{
    restoredCount = 0;
    std::error_code ec;

    if (!fs::exists(backupPath, ec) || !fs::is_directory(backupPath, ec))
        return false;

    if (!fs::is_directory(targetDir, ec))
        return false;

    for (const auto& entry : fs::directory_iterator(backupPath, ec)) {
        std::error_code fileEc;
        if (!entry.is_regular_file(fileEc)) continue;

        fs::path dest = targetDir / entry.path().filename();
        if (CopyFileW(entry.path().c_str(), dest.c_str(), FALSE))
            restoredCount++;
    }

    return restoredCount > 0;
}
