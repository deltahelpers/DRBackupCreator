#include "drbackup.h"

#include <iostream>
#include <iomanip>
#include <functional>

using namespace std;

void printHeader()
{
    cout << "=== DELTARUNE Backup Creator ===\n\n";
}

void listSlots(const vector<SlotFile>& slots)
{
    if (slots.empty()) {
        cout << "[!] Actually, you didn't saved a single time on DELTARUNE. Run the game, save, then come back.\n\n";
        return;
    }

    cout << setw(4)  << "#"
         << setw(8)  << "Chap"
         << setw(10) << "Slot"
         << setw(14) << "Type"
         << setw(12) << "Size"
         << "  Ruta\n" << string(70, '-') << '\n';

    int i = 0;
    for (const auto& s : slots) {
        cout << setw(4)  << ++i
             << setw(8)  << s.chapter
             << setw(10) << s.index
             << setw(14) << (s.isCompletion() ? "COMPLETION" : "SAVE")
             << setw(12) << formatFileSize(s.size)
             << "  " << s.path.filename().string() << '\n';
    }
    cout << '\n';
}

void doBackup()
{
    auto dirOpt = getDeltaruneDir(false);
    if (!dirOpt) { cout << "[!] I didn't found the DELTARUNE save folder where it should be...\n\n"; return; }

    vector<SlotFile> slots = scanSlots(*dirOpt);
    groupBySave(slots);

    if (slots.empty()) { cout << "[!] Theres nothing to backup.\n\n"; return; }

    string name = createBackup(dirOpt->parent_path(), slots);
    if (name.empty()) { cout << "[!] There was an error creating the backup.\n\n"; return; }

    cout << "[+] You succesfully created an backup: " << name << "\n\n";
}

void doRestore()
{
    auto dirOpt = getDeltaruneDir(false);
    if (!dirOpt) { cout << "[!] I didn't found the DELTARUNE save folder where it should be...\n\n"; return; }

    vector<BackupInfo> backups = listBackups(dirOpt->parent_path());
    if (backups.empty()) { cout << "[!] Actually, theres no backups. Do an backup, then come back.\n\n"; return; }

    cout << "Available backups:\n";
    for (size_t i = 0; i < backups.size() && i < 10; i++) {
        cout << "  [" << i << "] " << backups[i].timestamp
             << " (" << backups[i].slotCount << " slots, "
             << formatFileSize(backups[i].totalSize) << ")\n";
    }

    int choice = -1;
    cout << "\nSelect an backup: [0-" << backups.size() - 1 << "]: ";
    cin >> choice;

    if (choice < 0 || choice >= static_cast<int>(backups.size())) {
        cout << "[!] That's invalid.\n\n"; return;
    }

    int restored = 0;
    if (restoreBackup(backups[choice].path, *dirOpt, restored))
        cout << "[+] I got restored " << restored << " archivos.\n\n";
    else
        cout << "[!] I had an error restoring the save files.\n\n";
}

int runCli(int argc, char** argv)
{
    printHeader();

    if (isGameRunning())
        cout << "[!] Uh, DELTARUNE is running on this PC. Is recommended closing it to avoid corrupting your save file.\n\n";

    auto dirOpt = getDeltaruneDir(false);
    if (!dirOpt) {
        cout << "[!] I didn't found the DELTARUNE save folder where it should be...\n\n";
        return 1;
    }

    cout << "Folder: " << dirOpt->string() << "\n\n";

    vector<SlotFile> slots = scanSlots(*dirOpt);
    groupBySave(slots);

    cout << "Welcome, what do you want?\n"
         << "  1 - An list of my slots\n"
         << "  2 - Creating an backup\n"
         << "  3 - Restoring an backup\n"
         << "  0 - Get me out of here\n\n"
         << "Your choice: ";

    int choice = -1;
    cin >> choice;

    switch (choice) {
        case 1: listSlots(slots); break;
        case 2: doBackup();       break;
        case 3: doRestore();      break;
        case 0: cout << "See you next time.\n"; break;
        default: cout << "[!] That option isn't valid.\n"; break;
    }

    return 0;
}