#include <windows.h>
#include <shellapi.h>

#include <cstdio>
#include <fstream>
#include <iostream>
#include <string>
#include <vector>

#include <cwctype>

#include "config.h"
#include "drbackup.h"
#include "schedule.h"
#include "settings.h"
#include "ui/gui.h"
#include "ui/i18n.h"
#include "version.h"

int RunCli(const std::vector<std::wstring>& args);

namespace {

bool IsStandardHandleRedirected(DWORD stdHandle)
{
    HANDLE handle = GetStdHandle(stdHandle);
    if (!handle || handle == INVALID_HANDLE_VALUE) return false;

    const DWORD type = GetFileType(handle);
    return type == FILE_TYPE_PIPE || type == FILE_TYPE_DISK;
}

void AttachParentConsole()
{
    if (!AttachConsole(ATTACH_PARENT_PROCESS)) {
        if (!AllocConsole()) return;
    }

    // Keep redirected streams (pipes/files) so the CLI stays scriptable;
    // otherwise bind the C runtime streams to the attached console.
    if (!IsStandardHandleRedirected(STD_OUTPUT_HANDLE))
        freopen("CONOUT$", "w", stdout);
    if (!IsStandardHandleRedirected(STD_ERROR_HANDLE))
        freopen("CONOUT$", "w", stderr);
    if (!IsStandardHandleRedirected(STD_INPUT_HANDLE))
        freopen("CONIN$", "r", stdin);

    SetConsoleOutputCP(CP_UTF8);
    SetConsoleCP(CP_UTF8);

    std::cout.clear();
    std::cerr.clear();
    std::cin.clear();
    std::cout << "\n";
}

void PrintUsage()
{
    const bool english = ui::GetLang() == ui::Lang::EN;

    if (english) {
        std::cout <<
            "DRBackupCreator " DRBC_VERSION_A " (Win32 port)\n\n"
            "Usage:\n"
            "  drbc.exe                          Opens the GUI\n"
            "  drbc.exe <file.bctheme|.bcconfig> Opens the GUI with that file applied\n"
            "  drbc.exe --cli                    Opens the interactive CLI\n"
            "  drbc.exe --cli list               Lists the detected slots\n"
            "  drbc.exe --cli backup             Creates a backup\n"
            "  drbc.exe --cli restore <index>    Restores a backup\n"
            "  drbc.exe --schedule 21:30         Backs up every day at that time\n"
            "  drbc.exe --schedule off           Turns the daily backup off\n"
            "  drbc.exe --theme <file.bctheme>   Applies a theme file\n"
            "  drbc.exe --config <file.bcconfig> Imports a configuration file\n"
            "  drbc.exe --export-config <file>   Saves the current settings\n"
            "  drbc.exe --auto-backup            Silent backup (used by the schedule)\n"
            "  drbc.exe --help                   Prints this help\n"
            "  drbc.exe --version                Prints the version\n";
    } else {
        std::cout <<
            "DRBackupCreator " DRBC_VERSION_A " (port Win32)\n\n"
            "Uso:\n"
            "  drbc.exe                          Abre la interfaz gráfica\n"
            "  drbc.exe <archivo.bctheme|.bcconfig> Abre la app aplicando ese archivo\n"
            "  drbc.exe --cli                    Abre la CLI interactiva\n"
            "  drbc.exe --cli list               Lista los slots detectados\n"
            "  drbc.exe --cli backup             Crea un backup\n"
            "  drbc.exe --cli restore <indice>   Restaura un backup\n"
            "  drbc.exe --schedule 21:30         Hace un backup diario a esa hora\n"
            "  drbc.exe --schedule off           Desactiva el backup diario\n"
            "  drbc.exe --theme <archivo.bctheme> Aplica un tema\n"
            "  drbc.exe --config <archivo.bcconfig> Importa una configuración\n"
            "  drbc.exe --export-config <archivo> Guarda la configuración actual\n"
            "  drbc.exe --auto-backup            Backup silencioso (lo usa la programación)\n"
            "  drbc.exe --help                   Muestra esta ayuda\n"
            "  drbc.exe --version                Muestra la versión\n";
    }
}

std::string Bi(const wchar_t* spanish, const char* english)
{
    const std::wstring text(spanish);
    if (ui::GetLang() == ui::Lang::EN) return english;

    const int needed = WideCharToMultiByte(CP_UTF8, 0, text.c_str(),
                                           static_cast<int>(text.size()),
                                           nullptr, 0, nullptr, nullptr);
    if (needed <= 0) return {};
    std::string out(static_cast<size_t>(needed), '\0');
    WideCharToMultiByte(CP_UTF8, 0, text.c_str(), static_cast<int>(text.size()),
                        out.data(), needed, nullptr, nullptr);
    return out;
}

// ---------------------------------------------------------------------------
// Scheduled backup
// ---------------------------------------------------------------------------
void AppendScheduledLog(const std::wstring& line)
{
    auto dir = getSettingsDir(true);
    if (!dir) return;

    std::ofstream out(fs::path(dir->wstring() + L"\\scheduled.log"),
                      std::ios::binary | std::ios::app);
    if (!out) return;

    SYSTEMTIME now{};
    GetLocalTime(&now);
    char stamp[40] = {};
    std::snprintf(stamp, sizeof(stamp), "%04d-%02d-%02d %02d:%02d:%02d ",
                  now.wYear, now.wMonth, now.wDay, now.wHour, now.wMinute, now.wSecond);

    const std::string text = std::string(stamp) + cfg::narrow(line) + "\r\n";
    out.write(text.data(), static_cast<std::streamsize>(text.size()));
}

// Runs with no window and no console: the schedule calls this.
int RunAutoBackup()
{
    if (isGameRunning()) {
        AppendScheduledLog(L"skipped - DELTARUNE is running");
        return 0;
    }

    auto saveDir = getDeltaruneDir(false);
    if (!saveDir) {
        AppendScheduledLog(L"skipped - no DELTARUNE folder");
        return 1;
    }

    std::vector<SlotFile> slots = scanSlots(*saveDir);
    groupBySave(slots);
    if (slots.empty()) {
        AppendScheduledLog(L"skipped - no save slots found");
        return 1;
    }

    auto backupDir = getBackupDir(true);
    if (!backupDir) {
        AppendScheduledLog(L"failed - cannot create the backup folder");
        return 1;
    }

    const std::string name = createBackup(*backupDir, slots);
    if (name.empty()) {
        AppendScheduledLog(L"failed - the backup could not be created");
        return 1;
    }

    AppendScheduledLog(L"ok - " + cfg::widen(name) + L" (" +
                       std::to_wstring(slots.size()) + L" slots)");
    return 0;
}

// ---------------------------------------------------------------------------
// Settings commands (schedule / theme / config)
// ---------------------------------------------------------------------------
int RunSettingsCommands(const std::wstring& schedule,
                        const std::wstring& themeFile,
                        const std::wstring& configFile,
                        const std::wstring& exportFile)
{
    int result = 0;

    if (!schedule.empty()) {
        const bool off = _wcsicmp(schedule.c_str(), L"off") == 0 ||
                         _wcsicmp(schedule.c_str(), L"disable") == 0;

        std::string error;
        if (off) {
            if (sched::disable(error)) {
                app::SaveSchedule(false, app::Load().scheduleTime);
                std::cout << "[+] " << Bi(L"Backup diario desactivado.\n",
                                          "Daily backup disabled.\n");
            } else {
                std::cout << "[!] " << error << "\n";
                result = 1;
            }
        } else {
            int hour = 0;
            int minute = 0;
            if (!cfg::parseClock(cfg::narrow(schedule), hour, minute)) {
                std::cout << "[!] " << Bi(L"La hora debe ser algo como 21:30.\n",
                                          "The time must look like 21:30.\n");
                return 1;
            }
            const std::string time = cfg::clockToText(hour, minute);
            if (sched::enable(time, error)) {
                app::SaveSchedule(true, cfg::widen(time));
                std::cout << "[+] " << Bi(L"Backup diario programado a las ",
                                          "Daily backup scheduled at ")
                          << time << "\n";
            } else {
                std::cout << "[!] " << error << "\n";
                result = 1;
            }
        }
    }

    if (!themeFile.empty()) {
        cfg::Theme theme;
        std::string error;
        if (cfg::loadThemeFile(themeFile, theme, error)) {
            app::SaveThemePath(themeFile);
            std::cout << "[+] " << Bi(L"Tema aplicado: ", "Theme applied: ")
                      << theme.name << "\n";
        } else {
            std::cout << "[!] " << error << "\n";
            result = 1;
        }
    }

    if (!configFile.empty()) {
        std::wstring message;
        if (app::ImportConfig(configFile, message)) {
            std::cout << "[+] " << Bi(L"Configuración importada.\n",
                                      "Configuration imported.\n");
        } else {
            std::cout << "[!] " << cfg::narrow(message) << "\n";
            result = 1;
        }
    }

    if (!exportFile.empty()) {
        std::wstring message;
        if (app::ExportConfig(exportFile, message)) {
            std::cout << "[+] " << Bi(L"Configuración guardada en ",
                                      "Configuration saved to ")
                      << cfg::narrow(exportFile) << "\n";
        } else {
            std::cout << "[!] " << cfg::narrow(message) << "\n";
            result = 1;
        }
    }

    return result;
}

} // namespace

int WINAPI wWinMain(HINSTANCE instance, HINSTANCE, PWSTR, int)
{
    int argc = 0;
    LPWSTR* argv = CommandLineToArgvW(GetCommandLineW(), &argc);

    std::vector<std::wstring> args;
    args.reserve(static_cast<size_t>(argc > 0 ? argc : 0));
    for (int i = 0; i < argc; ++i) args.emplace_back(argv[i]);
    if (argv) LocalFree(argv);

    bool cli = false;
    bool help = false;
    bool version = false;
    bool autoBackup = false;

    std::wstring scheduleArg;
    std::wstring themeArg;
    std::wstring configArg;
    std::wstring exportArg;
    std::wstring openFile;

    for (size_t i = 1; i < args.size(); ++i) {
        const std::wstring& arg = args[i];

        auto takeValue = [&](std::wstring& target) {
            if (i + 1 >= args.size()) return false;
            target = args[++i];
            return true;
        };

        if (arg == L"--cli" || arg == L"-c") cli = true;
        else if (arg == L"--help" || arg == L"-h") help = true;
        else if (arg == L"--version" || arg == L"-v") version = true;
        else if (arg == L"--auto-backup") autoBackup = true;
        else if (arg == L"--schedule") takeValue(scheduleArg);
        else if (arg == L"--theme") takeValue(themeArg);
        else if (arg == L"--config") takeValue(configArg);
        else if (arg == L"--export-config") takeValue(exportArg);
        else if (!arg.empty() && arg[0] != L'-') openFile = arg;
    }

    ui::LoadLang();

    // The schedule runs this: no window, no console, just a log line.
    if (autoBackup) return RunAutoBackup();

    if (help || version) {
        AttachParentConsole();
        if (version) std::cout << "DRBackupCreator " DRBC_VERSION_A "\n";
        if (help) PrintUsage();
        return 0;
    }

    if (!scheduleArg.empty() || !themeArg.empty() || !configArg.empty() ||
        !exportArg.empty()) {
        AttachParentConsole();
        return RunSettingsCommands(scheduleArg, themeArg, configArg, exportArg);
    }

    // Double-clicking a .bctheme / .bcconfig (file association) lands here.
    if (!openFile.empty()) {
        const std::wstring lower = [&] {
            std::wstring text = openFile;
            for (wchar_t& c : text) c = static_cast<wchar_t>(towlower(c));
            return text;
        }();

        if (lower.size() > 8 && lower.compare(lower.size() - 8, 8, L".bctheme") == 0) {
            cfg::Theme theme;
            std::string error;
            if (cfg::loadThemeFile(openFile, theme, error)) app::SaveThemePath(openFile);
        } else if (lower.size() > 9 &&
                   lower.compare(lower.size() - 9, 9, L".bcconfig") == 0) {
            std::wstring message;
            app::ImportConfig(openFile, message);
        }
    }

    if (cli) {
        AttachParentConsole();
        return RunCli(args);
    }

    return ui::RunMainWindow(instance);
}
