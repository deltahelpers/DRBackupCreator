#define DRBACKUP_BUILD

#include "drbackup.h"
#include <algorithm>
#include <chrono>
#include <ctime>
#include <iomanip>
#include <sstream>

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

    char buf[32];
    std::strftime(buf, sizeof(buf), "%Y-%m-%d_%H-%M-%S", std::localtime(&time_t_now));
    return std::string(buf);
}

std::string createBackup(const fs::path& backupDir, const std::vector<SlotFile>& slots)
{
    if (slots.empty()) return "";

    const std::string backupName = "backup_" + getCurrentTimestamp();
    const fs::path fullBackupPath = backupDir / backupName;

    std::error_code ec;
    fs::create_directories(fullBackupPath, ec);
    if (ec) return "";

    for (const auto& s : slots) {
        fs::path dest = fullBackupPath / s.path.filename();
        fs::copy_file(s.path, dest, fs::copy_options::overwrite_existing, ec);
    }

    return backupName;
}

std::vector<BackupInfo> listBackups(const fs::path& parentDir)
{
    std::vector<BackupInfo> out;
    std::error_code ec;

    for (const auto& entry : fs::directory_iterator(parentDir, ec)) {
        if (!entry.is_directory(ec)) continue;

        const std::string name = entry.path().filename().string();
        if (name.size() < 8 || name.compare(0, 8, "backup_") != 0) continue;

        BackupInfo info;
        info.path      = entry.path();
        info.timestamp = name.substr(8);

        for (const auto& f : fs::directory_iterator(entry.path(), ec)) {
            if (f.is_regular_file(ec)) {
                info.slotCount++;
                info.totalSize += f.file_size(ec);
            }
        }
        out.push_back(info);
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

    for (const auto& entry : fs::directory_iterator(backupPath, ec)) {
        if (!entry.is_regular_file(ec)) continue;
        fs::path dest = targetDir / entry.path().filename();
        fs::copy_file(entry.path(), dest, fs::copy_options::overwrite_existing, ec);
        if (!ec) restoredCount++;
    }

    return restoredCount > 0;
}