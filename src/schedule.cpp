#include "schedule.h"

#include "config.h"

#include <windows.h>

#include <cstdio>
#include <string>

namespace sched {

namespace {

constexpr const wchar_t* kTaskName = L"DRBackupCreator Auto Backup";

std::wstring exePath()
{
    std::wstring buffer(MAX_PATH, L'\0');
    for (;;) {
        const DWORD written =
            GetModuleFileNameW(nullptr, buffer.data(), static_cast<DWORD>(buffer.size()));
        if (written == 0) return {};
        if (written < buffer.size()) {
            buffer.resize(written);
            return buffer;
        }
        buffer.resize(buffer.size() * 2);
    }
}

std::wstring tempFile()
{
    wchar_t dir[MAX_PATH + 1] = {};
    if (GetTempPathW(MAX_PATH, dir) == 0) return {};

    wchar_t name[96] = {};
    std::swprintf(name, 96, L"drbc_schtasks_%lu_%llu.txt",
                  GetCurrentProcessId(),
                  static_cast<unsigned long long>(GetTickCount64()));
    return std::wstring(dir) + name;
}

std::string firstLine(const std::string& text)
{
    size_t start = 0;
    while (start < text.size()) {
        const size_t end = text.find_first_of("\r\n", start);
        const std::string line =
            cfg::trim(text.substr(start, end == std::string::npos ? std::string::npos
                                                                  : end - start));
        if (!line.empty()) return line;
        if (end == std::string::npos) break;
        start = end + 1;
    }
    return {};
}

// schtasks writes UTF-16 when its output is redirected; normalise it to UTF-8.
std::string decode(const std::string& raw)
{
    if (raw.size() < 2) return raw;

    const auto b0 = static_cast<unsigned char>(raw[0]);
    const auto b1 = static_cast<unsigned char>(raw[1]);
    if (!((b0 == 0xFF && b1 == 0xFE) || (b0 == 0xFE && b1 == 0xFF))) return raw;

    const bool little = b0 == 0xFF;
    std::wstring wide;
    wide.reserve(raw.size() / 2);
    for (size_t i = 2; i + 1 < raw.size(); i += 2) {
        const auto lo = static_cast<unsigned char>(raw[i]);
        const auto hi = static_cast<unsigned char>(raw[i + 1]);
        wide.push_back(static_cast<wchar_t>(little ? (lo | (hi << 8)) : ((lo << 8) | hi)));
    }
    return cfg::narrow(wide);
}

// Runs a command with stdout/stderr redirected to a temp file.
bool run(const std::wstring& command, std::string& output, DWORD& exitCode)
{
    const std::wstring file = tempFile();
    if (file.empty()) return false;

    const HANDLE handle =
        CreateFileW(file.c_str(), GENERIC_WRITE, FILE_SHARE_READ | FILE_SHARE_WRITE,
                    nullptr, CREATE_ALWAYS, FILE_ATTRIBUTE_TEMPORARY, nullptr);
    if (handle == INVALID_HANDLE_VALUE) return false;

    STARTUPINFOW si{};
    si.cb          = sizeof(si);
    si.dwFlags     = STARTF_USESTDHANDLES | STARTF_USESHOWWINDOW;
    si.hStdOutput  = handle;
    si.hStdError   = handle;
    si.hStdInput   = GetStdHandle(STD_INPUT_HANDLE);
    si.wShowWindow = SW_HIDE;

    PROCESS_INFORMATION pi{};
    std::wstring line = command;

    const BOOL started =
        CreateProcessW(nullptr, line.data(), nullptr, nullptr, TRUE,
                       CREATE_NO_WINDOW, nullptr, nullptr, &si, &pi);
    CloseHandle(handle);

    exitCode = static_cast<DWORD>(-1);
    if (started) {
        WaitForSingleObject(pi.hProcess, 20000);
        GetExitCodeProcess(pi.hProcess, &exitCode);
        CloseHandle(pi.hProcess);
        CloseHandle(pi.hThread);
    }

    bool readOk = false;
    output = decode(cfg::readTextFile(file, readOk));
    DeleteFileW(file.c_str());
    if (!readOk) output.clear();

    return started != FALSE;
}

std::string failureText(const std::string& output, const char* fallback)
{
    const std::string line = firstLine(output);
    return line.empty() ? std::string(fallback) : line;
}

} // namespace

const wchar_t* TaskName()
{
    return kTaskName;
}

Status query()
{
    Status status;

    std::string output;
    DWORD exitCode = 0;
    if (!run(std::wstring(L"schtasks.exe /Query /TN \"") + kTaskName + L"\" /XML",
             output, exitCode)) {
        status.state = State::Error;
        status.error = "cannot run schtasks.exe";
        return status;
    }

    if (exitCode != 0) {
        // "cannot find the task" is the normal first-run answer.
        status.state = State::NotScheduled;
        return status;
    }

    status.state = State::Scheduled;

    const size_t start = output.find("<StartBoundary>");
    if (start != std::string::npos) {
        const size_t value = start + 15;             // strlen("<StartBoundary>")
        const size_t timePos = output.find('T', value);
        const size_t end = output.find('<', value);
        if (timePos != std::string::npos && end != std::string::npos &&
            timePos + 5 <= end) {
            status.time = output.substr(timePos + 1, 5);
        }
    }

    if (output.find("<Enabled>false</Enabled>") != std::string::npos) {
        status.state = State::Error;
        status.error = "the task is disabled in Windows Task Scheduler";
    }

    return status;
}

bool enable(const std::string& hhmm, std::string& error)
{
    int hour = 0;
    int minute = 0;
    if (!cfg::parseClock(hhmm, hour, minute)) {
        error = "the time must look like 21:30";
        return false;
    }
    const std::string time = cfg::clockToText(hour, minute);

    const std::wstring exe = exePath();
    if (exe.empty()) {
        error = "cannot determine the program path";
        return false;
    }

    // /TR needs the whole command line as one quoted argument, and the exe path
    // itself needs escaped quotes so paths with spaces keep working.
    const std::wstring escaped = L"\\\"";
    std::wstring action = L"\"" + escaped + exe + escaped + L" --auto-backup\"";

    std::wstring command = L"schtasks.exe /Create /F /TN \"";
    command += kTaskName;
    command += L"\" /TR ";
    command += action;
    command += L" /SC DAILY /ST ";
    command += cfg::widen(time);
    command += L" /RL LIMITED";

    std::string output;
    DWORD exitCode = 0;
    if (!run(command, output, exitCode)) {
        error = "cannot run schtasks.exe";
        return false;
    }
    if (exitCode != 0) {
        error = failureText(output, "could not create the scheduled task");
        return false;
    }
    return true;
}

bool disable(std::string& error)
{
    if (query().state == State::NotScheduled) return true;   // already off

    std::string output;
    DWORD exitCode = 0;
    if (!run(std::wstring(L"schtasks.exe /Delete /F /TN \"") + kTaskName + L"\"",
             output, exitCode)) {
        error = "cannot run schtasks.exe";
        return false;
    }
    if (exitCode != 0) {
        error = failureText(output, "could not delete the scheduled task");
        return false;
    }
    return true;
}

} // namespace sched
