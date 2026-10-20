#include <windows.h>
#include <cstring>
#include <iostream>
#include <cstdio>

extern int runCli(int argc, char** argv);
extern int runGui();

static void attachConsole()
{
    if (!AttachConsole(ATTACH_PARENT_PROCESS)) {
        if (!AllocConsole()) return;
    }
    freopen("CONOUT$", "w", stdout);
    freopen("CONOUT$", "w", stderr);
    freopen("CONIN$",  "r", stdin);
    std::cout.clear();
    std::cin.clear();
    std::cerr.clear();
    std::cout << "\n";
}

static void printUsage()
{
    std::cout <<
        "DRBackupCreator\n\n"
        "Usage:\n"
        "  drbc.exe              Opens the GUI\n"
        "  drbc.exe --cli        Opens the CLI\n"
        "  drbc.exe --help       Prints this help\n\n"
        "CLI Subcommands Usage:\n"
        "  drbc.exe --cli backup\n"
        "  drbc.exe --cli list\n"
        "  drbc.exe --cli restore 0\n";
}

int main(int argc, char** argv)
{
    bool cliMode = false;
    bool helpMode = false;

    for (int i = 1; i < argc; ++i) {
        if (std::strcmp(argv[i], "--cli") == 0 || std::strcmp(argv[i], "-c") == 0)
            cliMode = true;
        else if (std::strcmp(argv[i], "--help") == 0 || std::strcmp(argv[i], "-h") == 0)
            helpMode = true;
    }

    if (helpMode) {
        attachConsole();
        printUsage();
        return 0;
    }

    if (cliMode) {
        attachConsole();
        return runCli(argc, argv);   
    }

    return runGui();
}