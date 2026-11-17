#include <windows.h>

#include <iomanip>
#include <iostream>
#include <string>
#include <vector>

#include "drbackup.h"
#include "ui/i18n.h"
#include "ui/theme.h"

namespace {

using ui::GetLang;
using ui::Lang;
using ui::T;

std::string Narrow(const std::wstring& text)
{
    if (text.empty()) return {};

    const int needed = WideCharToMultiByte(CP_UTF8, 0, text.c_str(),
                                           static_cast<int>(text.size()),
                                           nullptr, 0, nullptr, nullptr);
    if (needed <= 0) return {};

    std::string out(static_cast<size_t>(needed), '\0');
    WideCharToMultiByte(CP_UTF8, 0, text.c_str(), static_cast<int>(text.size()),
                        out.data(), needed, nullptr, nullptr);
    return out;
}

std::string Narrow(const fs::path& path)
{
    return Narrow(path.wstring());
}

std::string Narrow(const wchar_t* text)
{
    return text ? Narrow(std::wstring(text)) : std::string();
}

std::string Bi(const wchar_t* spanish, const char* english)
{
    return GetLang() == Lang::EN ? std::string(english) : Narrow(spanish);
}

void PrintHeader()
{
    std::cout << "=== DELTARUNE Backup Creator ===\n"
              << "    " << Narrow(T().appSubtitle) << "\n\n";
}

void WarnIfGameRunning()
{
    if (isGameRunning())
        std::cout << "[!] " << Narrow(T().gameWarning) << "\n\n";
}

bool ResolveSaveDir(fs::path& dir)
{
    auto found = getDeltaruneDir(false);
    if (!found) {
        std::cout << "[!] " << Narrow(T().noSaveDir) << "\n\n";
        return false;
    }
    dir = *found;
    return true;
}

void ListSlots(const std::vector<SlotFile>& slots)
{
    if (slots.empty()) {
        std::cout << "[!] " << Narrow(T().emptySlots) << "\n\n";
        return;
    }

    std::cout << std::setw(4)  << "#"
              << std::setw(8)  << "Chap"
              << std::setw(10) << "Slot"
              << std::setw(14) << Narrow(T().colType)
              << std::setw(12) << Narrow(T().colSize)
              << "  " << Narrow(T().colFile) << "\n"
              << std::string(74, '-') << "\n";

    int i = 0;
    for (const auto& slot : slots) {
        std::cout << std::setw(4)  << ++i
                  << std::setw(8)  << slot.chapter
                  << std::setw(10) << slot.index
                  << std::setw(14) << Narrow(slot.isCompletion() ? T().typeCompletion
                                                                 : T().typeSave)
                  << std::setw(12) << formatFileSize(slot.size)
                  << "  " << Narrow(slot.path.filename()) << "\n";
    }
    std::cout << "\n";
}

int DoBackup(const fs::path& saveDir, const std::vector<SlotFile>& slots)
{
    if (slots.empty()) {
        std::cout << "[!] " << Narrow(T().noSlots) << "\n\n";
        return 1;
    }

    auto backupDir = getBackupDir(true);
    if (!backupDir) {
        std::cout << "[!] " << Narrow(T().backupErr) << "\n\n";
        return 1;
    }

    const std::string name = createBackup(*backupDir, slots);
    if (name.empty()) {
        std::cout << "[!] " << Narrow(T().backupErr) << "\n\n";
        return 1;
    }

    std::cout << "[+] "
              << Narrow(ui::FormatText(T().backupOk, ui::Widen(name).c_str()))
              << "\n\n";
    (void)saveDir;
    return 0;
}

int DoRestore(const fs::path& saveDir, int forcedIndex = -1)
{
    std::vector<BackupInfo> backups = listAllBackups();
    if (backups.empty()) {
        std::cout << "[!] " << Narrow(T().noBackups) << "\n\n";
        return 1;
    }

    std::cout << Bi(L"Backups disponibles:\n", "Available backups:\n");
    const size_t shown = backups.size() < 10 ? backups.size() : 10;
    for (size_t i = 0; i < shown; ++i) {
        std::cout << "  [" << i << "] " << backups[i].timestamp
                  << " (" << backups[i].slotCount << " slots, "
                  << formatFileSize(backups[i].totalSize) << ")\n";
    }

    int choice = forcedIndex;
    if (choice < 0) {
        std::cout << "\n" << Bi(L"Elige un backup: ", "Select a backup: ")
                  << "[0-" << (backups.size() - 1) << "]: ";
        if (!(std::cin >> choice)) {
            std::cin.clear();
            std::cin.ignore(10000, '\n');
            choice = -1;
        }
    }

    if (choice < 0 || static_cast<size_t>(choice) >= backups.size()) {
        std::cout << "[!] " << Bi(L"Ese backup no es válido.\n\n",
                                  "That backup is not valid.\n\n");
        return 1;
    }

    int restored = 0;
    if (restoreBackup(backups[static_cast<size_t>(choice)].path, saveDir, restored)) {
        std::cout << "[+] "
                  << Narrow(ui::FormatText(T().restoreOk, restored))
                  << "\n\n";
        return 0;
    }

    std::cout << "[!] " << Narrow(T().restoreErr) << "\n\n";
    return 1;
}

int RunInteractive(const fs::path& saveDir, const std::vector<SlotFile>& slots)
{
    std::cout << Narrow(ui::FormatText(T().slotsSection,
                                       static_cast<int>(slots.size())))
              << "\n";
    std::cout << Bi(L"Ruta: ", "Folder: ") << Narrow(saveDir) << "\n\n";

    std::cout << Bi(L"¿Qué quieres hacer?\n", "What do you want to do?\n")
              << Bi(L"  1 - Ver mis slots\n", "  1 - List my slots\n")
              << Bi(L"  2 - Crear un backup\n", "  2 - Create a backup\n")
              << Bi(L"  3 - Restaurar un backup\n", "  3 - Restore a backup\n")
              << Bi(L"  0 - Salir\n", "  0 - Exit\n")
              << "\n" << Bi(L"Tu elección: ", "Your choice: ");

    int choice = -1;
    if (!(std::cin >> choice)) {
        std::cin.clear();
        std::cin.ignore(10000, '\n');
        std::cout << "\n";
        return 1;
    }
    std::cout << "\n";

    switch (choice) {
    case 1: ListSlots(slots); return 0;
    case 2: return DoBackup(saveDir, slots);
    case 3: return DoRestore(saveDir);
    case 0:
        std::cout << Bi(L"Hasta la próxima.\n", "See you next time.\n");
        return 0;
    default:
        std::cout << "[!] " << Bi(L"Esa opción no es válida.\n",
                                  "That option is not valid.\n");
        return 1;
    }
}

} // namespace

int RunCli(const std::vector<std::wstring>& args)
{
    PrintHeader();
    WarnIfGameRunning();

    fs::path saveDir;
    if (!ResolveSaveDir(saveDir)) return 1;

    std::vector<SlotFile> slots = scanSlots(saveDir);
    groupBySave(slots);

    // Optional non-interactive subcommand: --cli <list|backup|restore N>
    std::wstring command;
    int restoreIndex = -1;

    for (size_t i = 1; i + 1 <= args.size(); ++i) {
        if (args[i] == L"--cli" || args[i] == L"-c") {
            if (i + 1 < args.size()) command = args[i + 1];
            if (command == L"restore" && i + 2 < args.size()) {
                try { restoreIndex = std::stoi(args[i + 2]); }
                catch (...) { restoreIndex = -1; }
            }
            break;
        }
    }

    if (!command.empty() && command[0] != L'-') {
        if (command == L"list") {
            std::cout << Bi(L"Ruta: ", "Folder: ") << Narrow(saveDir) << "\n\n";
            ListSlots(slots);
            return 0;
        }
        if (command == L"backup") {
            return DoBackup(saveDir, slots);
        }
        if (command == L"restore") {
            return DoRestore(saveDir, restoreIndex >= 0 ? restoreIndex : -1);
        }
        if (command == L"help") {
            std::cout << "drbc --cli <list|backup|restore N>\n";
            return 0;
        }
    }

    return RunInteractive(saveDir, slots);
}
