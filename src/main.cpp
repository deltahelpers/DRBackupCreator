#include <windows.h>
#include <shellapi.h>

#include <cstdio>
#include <iostream>
#include <string>
#include <vector>

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
            "  drbc.exe                         Opens the GUI\n"
            "  drbc.exe --cli                   Opens the interactive CLI\n"
            "  drbc.exe --cli list              Lists the detected slots\n"
            "  drbc.exe --cli backup            Creates a backup\n"
            "  drbc.exe --cli restore <index>   Restores a backup\n"
            "  drbc.exe --help                  Prints this help\n"
            "  drbc.exe --version               Prints the version\n";
    } else {
        std::cout <<
            "DRBackupCreator " DRBC_VERSION_A " (port Win32)\n\n"
            "Uso:\n"
            "  drbc.exe                         Abre la interfaz gráfica\n"
            "  drbc.exe --cli                   Abre la CLI interactiva\n"
            "  drbc.exe --cli list              Lista los slots detectados\n"
            "  drbc.exe --cli backup            Crea un backup\n"
            "  drbc.exe --cli restore <indice>  Restaura un backup\n"
            "  drbc.exe --help                  Muestra esta ayuda\n"
            "  drbc.exe --version               Muestra la versión\n";
    }
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

    for (size_t i = 1; i < args.size(); ++i) {
        if (args[i] == L"--cli" || args[i] == L"-c") cli = true;
        else if (args[i] == L"--help" || args[i] == L"-h") help = true;
        else if (args[i] == L"--version" || args[i] == L"-v") version = true;
    }

    ui::LoadLang();

    if (help || version) {
        AttachParentConsole();
        if (version) std::cout << "DRBackupCreator " DRBC_VERSION_A "\n";
        if (help) PrintUsage();
        return 0;
    }

    if (cli) {
        AttachParentConsole();
        return RunCli(args);
    }

    return ui::RunMainWindow(instance);
}
