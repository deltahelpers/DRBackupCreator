#include "drbackup.h"

#include <windows.h>

#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>
#include <vector>

namespace fs = std::filesystem;

namespace {

int g_failures = 0;

#define CHECK(condition)                                                       \
    do {                                                                       \
        if (!(condition)) {                                                    \
            std::cerr << "FAIL line " << __LINE__ << ": " #condition "\n";     \
            ++g_failures;                                                      \
        }                                                                      \
    } while (0)

void WriteFile(const fs::path& path, const std::string& content)
{
    std::ofstream out(path, std::ios::binary | std::ios::trunc);
    out << content;
}

std::string ReadFile(const fs::path& path)
{
    std::ifstream in(path, std::ios::binary);
    return std::string(std::istreambuf_iterator<char>(in),
                       std::istreambuf_iterator<char>());
}

} // namespace

int main()
{
    std::error_code ec;
    const fs::path base =
        fs::temp_directory_path() /
        ("drbc_tests_" + std::to_string(GetCurrentProcessId()) + "_" +
         std::to_string(GetTickCount64()));

    fs::remove_all(base, ec);
    fs::create_directories(base);

    // --- fixtures -----------------------------------------------------------
    const fs::path saves = base / "DELTARUNE";
    fs::create_directories(saves);

    WriteFile(saves / "filech1_0.dat", "abc");
    WriteFile(saves / "filech1_5.dat", "abc");
    WriteFile(saves / "filech1_9.dat", "abc");
    WriteFile(saves / "filech2_1.dat", "abc");
    WriteFile(saves / "notes.txt", "not a save");
    WriteFile(saves / "filechX_1.dat", "bad");
    WriteFile(saves / "filech_1.dat", "bad");

    // --- scanning -----------------------------------------------------------
    std::vector<SlotFile> slots = scanSlots(saves);
    CHECK(slots.size() == 4);

    auto parsed = parseSlotFile(saves / "filech2_1.dat");
    CHECK(parsed.has_value());
    if (parsed) {
        CHECK(parsed->chapter == 2);
        CHECK(parsed->index == 1);
        CHECK(parsed->size == 3);
        CHECK(!parsed->isCompletion());
        CHECK(parsed->pairedIndex() == 8);
    }

    CHECK(!parseSlotFile(saves / "notes.txt").has_value());
    CHECK(!parseSlotFile(saves / "filech_1.dat").has_value());
    CHECK(!parseSlotFile(saves / "filechX_1.dat").has_value());

    auto completion = parseSlotFile(saves / "filech1_9.dat");
    CHECK(completion.has_value());
    if (completion) {
        CHECK(completion->isCompletion());
        CHECK(completion->pairedIndex() == 0);
    }

    // --- grouping order -----------------------------------------------------
    groupBySave(slots);
    CHECK(slots.size() == 4);
    CHECK(slots[0].chapter == 1 && slots[0].index == 0);
    CHECK(slots[1].chapter == 1 && slots[1].index == 9);
    CHECK(slots[2].chapter == 1 && slots[2].index == 5);
    CHECK(slots[3].chapter == 2 && slots[3].index == 1);

    // --- backup creation ----------------------------------------------------
    const fs::path backups = base / "backups";
    const std::string name = createBackup(backups, slots);
    CHECK(!name.empty());
    CHECK(name.rfind("backup_", 0) == 0);

    const fs::path backupPath = backups / name;
    CHECK(fs::is_directory(backupPath));
    CHECK(fs::exists(backupPath / "filech1_0.dat"));
    CHECK(fs::exists(backupPath / "filech2_1.dat"));

    CHECK(createBackup(backups, {}).empty());

    // --- listing ------------------------------------------------------------
    std::vector<BackupInfo> listed = listBackups(backups);
    CHECK(listed.size() == 1);    if (!listed.empty()) {
        CHECK(listed[0].slotCount == 4);
        CHECK(listed[0].totalSize == 12);
        CHECK(listed[0].timestamp == name.substr(7));
    }

    // --- restore ------------------------------------------------------------
    const fs::path target = base / "restore_target";
    fs::create_directories(target);
    WriteFile(target / "filech1_0.dat", "zz");
    WriteFile(target / "filech1_5.dat", "zz");

    int restored = 0;
    CHECK(restoreBackup(backupPath, target, restored));
    CHECK(restored == 4);
    CHECK(ReadFile(target / "filech1_0.dat") == "abc");
    CHECK(ReadFile(target / "filech1_5.dat") == "abc");

    CHECK(!restoreBackup(base / "does_not_exist", target, restored));
    CHECK(restored == 0);

    // --- helpers ------------------------------------------------------------
    CHECK(formatFileSize(0) == "0.0 B");
    CHECK(formatFileSize(1024) == "1.0 KB");
    CHECK(formatFileSize(1536) == "1.5 KB");
    CHECK(formatFileSize(3ull * 1024 * 1024) == "3.0 MB");

    const std::string timestamp = getCurrentTimestamp();
    CHECK(timestamp.size() == 19);
    CHECK(timestamp[4] == '-');
    CHECK(timestamp[10] == '_');

    // --- cleanup ------------------------------------------------------------
    fs::remove_all(base, ec);

    if (g_failures > 0) {
        std::cerr << g_failures << " test(s) failed\n";
        return 1;
    }

    std::cout << "All core tests passed.\n";
    return 0;
}
