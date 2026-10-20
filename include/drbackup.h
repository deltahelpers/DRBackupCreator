#pragma once
#ifdef _WIN32
  #ifdef DRBACKUP_BUILD
    #define DRBC_API __declspec(dllexport)
  #else
    #define DRBC_API __declspec(dllimport)
  #endif
#else
  #define DRBC_API
#endif
#include <filesystem>
#include <optional>
#include <string>
#include <vector>
#include <cstdint>
namespace fs = std::filesystem;

// public structs
struct SlotFile {
    int       chapter;
    int       index;
    fs::path  path;
    uintmax_t size;

    [[nodiscard]] bool isCompletion() const { return index >= 5; }
    [[nodiscard]] int  pairedIndex()  const { return 9 - index; }

    SlotFile() : chapter(0), index(0), size(0) {}
};
struct BackupInfo {
    fs::path    path;
    std::string timestamp;
    int         slotCount;
    uintmax_t   totalSize;

    BackupInfo() : slotCount(0), totalSize(0) {}
};
// API for the other files
DRBC_API std::optional<fs::path> getLocalAppData();
DRBC_API std::optional<fs::path> getDeltaruneDir(bool crearSiFalta = false);
DRBC_API std::optional<SlotFile> parseSlotFile(const fs::path& p);
DRBC_API std::vector<SlotFile> scanSlots(const fs::path& dir);
DRBC_API void groupBySave(std::vector<SlotFile>& slots);
DRBC_API bool isGameRunning();
DRBC_API std::string createBackup(const fs::path& backupDir, const std::vector<SlotFile>& slots);
DRBC_API std::vector<BackupInfo> listBackups(const fs::path& parentDir);
DRBC_API bool restoreBackup(const fs::path& backupPath, const fs::path& targetDir, int& restoredCount);
DRBC_API std::string formatFileSize(uintmax_t bytes);
DRBC_API std::string getCurrentTimestamp();