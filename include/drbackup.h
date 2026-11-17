#pragma once

// DRBackupCCore - backup engine for DRBackupCreator
// Original project: https://github.com/deltahelpers/DRBackupCreator (GPL-3.0)

#ifdef _WIN32
  #ifdef DRBACKUP_BUILD
    #define DRBC_API __declspec(dllexport)
  #else
    #define DRBC_API __declspec(dllimport)
  #endif
#else
  #define DRBC_API
#endif

#include <cstdint>
#include <filesystem>
#include <optional>
#include <string>
#include <vector>

namespace fs = std::filesystem;

// ---------------------------------------------------------------------------
// Public structs (ABI kept identical to the original project)
// ---------------------------------------------------------------------------
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

// ---------------------------------------------------------------------------
// Paths
// ---------------------------------------------------------------------------
DRBC_API std::optional<fs::path> getLocalAppData();
DRBC_API std::optional<fs::path> getDeltaruneDir(bool createIfMissing = false);

// %LOCALAPPDATA%\DRBackupCreator - where this port stores every backup.
DRBC_API std::optional<fs::path> getBackupDir(bool createIfMissing = false);

// ---------------------------------------------------------------------------
// Slots
// ---------------------------------------------------------------------------
DRBC_API std::optional<SlotFile> parseSlotFile(const fs::path& p);
DRBC_API std::vector<SlotFile> scanSlots(const fs::path& dir);
DRBC_API void groupBySave(std::vector<SlotFile>& slots);

// ---------------------------------------------------------------------------
// Process detection
// ---------------------------------------------------------------------------
DRBC_API bool isGameRunning();

// ---------------------------------------------------------------------------
// Backups
// ---------------------------------------------------------------------------
DRBC_API std::string createBackup(const fs::path& backupDir,
                                  const std::vector<SlotFile>& slots);

DRBC_API std::vector<BackupInfo> listBackups(const fs::path& parentDir);

// Primary folder + legacy folder (%LOCALAPPDATA%, used by the old CLI).
DRBC_API std::vector<BackupInfo> listAllBackups();

DRBC_API bool restoreBackup(const fs::path& backupPath,
                            const fs::path& targetDir,
                            int& restoredCount);

// ---------------------------------------------------------------------------
// Helpers
// ---------------------------------------------------------------------------
DRBC_API std::string formatFileSize(uintmax_t bytes);
DRBC_API std::string getCurrentTimestamp();
