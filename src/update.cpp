#include "update.h"

#include <windows.h>
#include <bcrypt.h>
#include <shellapi.h>
#include <winhttp.h>

#include <cstdio>
#include <cctype>
#include <string>
#include <vector>

#include "drbackup.h"
#include "version.h"

#ifndef WINHTTP_OPTION_SECURE_PROTOCOLS
#define WINHTTP_OPTION_SECURE_PROTOCOLS 84
#endif

// Value from winhttp.h; spelled out so older MinGW headers still compile.
#ifndef WINHTTP_ACCESS_TYPE_AUTOMATIC_PROXY
#define WINHTTP_ACCESS_TYPE_AUTOMATIC_PROXY 4
#endif

namespace upd {

namespace {

constexpr wchar_t kOwner[] = L"deltahelpers";
constexpr wchar_t kRepo[]  = L"DRBackupCreator";
constexpr size_t  kMaxText = 1024 * 1024;    // 1 MiB is plenty for a release


std::string Utf8(const std::wstring& text){
    if (text.empty()) return {};
    const int size = WideCharToMultiByte(CP_UTF8, 0, text.c_str(),
                                         static_cast<int>(text.size()),
                                         nullptr, 0, nullptr, nullptr);
    std::string out(static_cast<size_t>(size), '\0');
    WideCharToMultiByte(CP_UTF8, 0, text.c_str(), static_cast<int>(text.size()),
                        out.data(), size, nullptr, nullptr);
    return out;
}

std::wstring Wide(const std::string& text)
{
    if (text.empty()) return {};
    const int size = MultiByteToWideChar(CP_UTF8, 0, text.c_str(),
                                         static_cast<int>(text.size()), nullptr, 0);
    std::wstring out(static_cast<size_t>(size), L'\0');
    MultiByteToWideChar(CP_UTF8, 0, text.c_str(), static_cast<int>(text.size()),
                        out.data(), size);
    return out;
}

std::string StatusError(DWORD status)
{
    switch (status) {
    case 403: return "GitHub refused the request (rate limit or blocked)";
    case 404: return "the release does not exist";
    case 500: case 502: case 503: return "GitHub is having problems (HTTP 5xx)";
    default:  return "the server answered HTTP " + std::to_string(status);
    }
}

struct Session {
    HINTERNET handle = nullptr;
    ~Session() { if (handle) WinHttpCloseHandle(handle); }
};

struct Connection {
    HINTERNET handle = nullptr;
    ~Connection() { if (handle) WinHttpCloseHandle(handle); }
};

struct Request {
    HINTERNET handle = nullptr;
    ~Request() { if (handle) WinHttpCloseHandle(handle); }
};

Session OpenSession(std::string& error)
{
    Session session;
    const std::wstring agent = L"DRBackupCreator/" DRBC_VERSION_W L" (WinHTTP)";

    session.handle = WinHttpOpen(agent.c_str(), WINHTTP_ACCESS_TYPE_AUTOMATIC_PROXY,
                                 WINHTTP_NO_PROXY_NAME, WINHTTP_NO_PROXY_BYPASS, 0);
    if (!session.handle) {
        session.handle = WinHttpOpen(agent.c_str(), WINHTTP_ACCESS_TYPE_DEFAULT_PROXY,
                                     WINHTTP_NO_PROXY_NAME, WINHTTP_NO_PROXY_BYPASS, 0);
    }
    if (!session.handle) {
        error = "WinHTTP could not start";
        return session;
    }

    WinHttpSetTimeouts(session.handle, 5000, 5000, 10000, 20000);

    DWORD protocols = WINHTTP_FLAG_SECURE_PROTOCOL_TLS1_2 | WINHTTP_FLAG_SECURE_PROTOCOL_TLS1_3;
    WinHttpSetOption(session.handle, WINHTTP_OPTION_SECURE_PROTOCOLS,
                     &protocols, sizeof(protocols));
    return session;
}

struct UrlParts {
    std::wstring host;
    std::wstring path;
    bool secure = true;
};

bool CrackUrl(const std::wstring& url, UrlParts& parts)
{
    URL_COMPONENTS comps{};
    comps.dwStructSize = sizeof(comps);
    comps.dwHostNameLength   = static_cast<DWORD>(-1);
    comps.dwUrlPathLength    = static_cast<DWORD>(-1);
    comps.dwExtraInfoLength  = static_cast<DWORD>(-1);

    if (!WinHttpCrackUrl(url.c_str(), static_cast<DWORD>(url.size()), 0, &comps))
        return false;

    parts.host.assign(comps.lpszHostName, comps.dwHostNameLength);
    parts.secure = comps.nScheme == INTERNET_SCHEME_HTTPS;

    parts.path.assign(comps.lpszUrlPath, comps.dwUrlPathLength);
    if (comps.dwExtraInfoLength > 0)
        parts.path.append(comps.lpszExtraInfo, comps.dwExtraInfoLength);

    if (parts.path.empty()) parts.path = L"/";
    return true;
}

// Body consumer. Returns false to stop the transfer (cancelled or write error).
using Sink = std::function<bool(const char* data, size_t size)>;

// Performs one GET, following up to five redirects. Calls `sink` with every
// chunk of the final 200 response. `totalSize` receives the Content-Length
// when the server sends one (0 otherwise).
bool HttpGet(const std::wstring& url, const wchar_t* accept, const Sink& sink,
             long long& totalSize, std::string& error)
{
    totalSize = 0;
    Session session = OpenSession(error);
    if (!session.handle) return false;

    std::wstring current = url;

    for (int hop = 0; hop < 6; ++hop) {
        UrlParts parts;
        if (!CrackUrl(current, parts)) {
            error = "the download link is not a valid URL";
            return false;
        }

        Connection connection;
        connection.handle = WinHttpConnect(session.handle, parts.host.c_str(),
                                           parts.secure ? INTERNET_DEFAULT_HTTPS_PORT
                                                        : INTERNET_DEFAULT_HTTP_PORT, 0);
        if (!connection.handle) {
            error = "could not connect to " + Utf8(parts.host);
            return false;
        }

        // WinHTTP expects a NULL-terminated array of accept types, not a
        // pointer to a single string.
        const wchar_t* acceptTypes[] = { accept ? accept : L"*/*", nullptr };

        Request request;
        request.handle = WinHttpOpenRequest(
            connection.handle, L"GET", parts.path.c_str(), nullptr,
            WINHTTP_NO_REFERER, acceptTypes,
            parts.secure ? WINHTTP_FLAG_SECURE : 0);
        if (!request.handle) {
            error = "could not build the HTTP request";
            return false;
        }

        const wchar_t* headers = L"Cache-Control: no-cache\r\n";
        if (!WinHttpSendRequest(request.handle, headers, static_cast<DWORD>(-1),
                                WINHTTP_NO_REQUEST_DATA, 0, 0, 0)) {
            error = "the request could not be sent (error " +
                    std::to_string(GetLastError()) + ")";
            return false;
        }
        if (!WinHttpReceiveResponse(request.handle, nullptr)) {
            error = "the connection closed before an answer arrived (error " +
                    std::to_string(GetLastError()) + ")";
            return false;
        }

        DWORD status = 0;
        DWORD size = sizeof(status);
        WinHttpQueryHeaders(request.handle,
                            WINHTTP_QUERY_STATUS_CODE | WINHTTP_QUERY_FLAG_NUMBER,
                            WINHTTP_HEADER_NAME_BY_INDEX, &status, &size,
                            WINHTTP_NO_HEADER_INDEX);

        if (status == 301 || status == 302 || status == 303 || status == 307 || status == 308) {
            wchar_t location[2048] = {};
            DWORD locationSize = sizeof(location);
            if (!WinHttpQueryHeaders(request.handle, WINHTTP_QUERY_LOCATION,
                                     WINHTTP_HEADER_NAME_BY_INDEX, location,
                                     &locationSize, WINHTTP_NO_HEADER_INDEX)) {
                error = "the download was redirected but the target is missing";
                return false;
            }

            std::wstring next = location;
            if (next.rfind(L"http", 0) != 0) {
                next = (parts.secure ? L"https://" : L"http://") +
                       parts.host + (next.rfind(L'/', 0) == 0 ? L"" : L"/") + next;
            }
            current = next;
            continue;
        }

        if (status != 200) {
            error = StatusError(status);
            return false;
        }

        DWORD length = 0;
        DWORD lengthSize = sizeof(length);
        if (WinHttpQueryHeaders(request.handle,
                                WINHTTP_QUERY_CONTENT_LENGTH | WINHTTP_QUERY_FLAG_NUMBER,
                                WINHTTP_HEADER_NAME_BY_INDEX, &length, &lengthSize,
                                WINHTTP_NO_HEADER_INDEX))
            totalSize = length;

        char buffer[64 * 1024];
        for (;;) {
            DWORD read = 0;
            if (!WinHttpReadData(request.handle, buffer, sizeof(buffer), &read)) {
                error = "the transfer was interrupted";
                return false;
            }
            if (read == 0) break;
            if (!sink(buffer, read)) return true;    // caller stopped us
        }
        return true;
    }

    error = "too many redirects";
    return false;
}

std::wstring NarrowToWideUrl(const std::string& url)
{
    return Wide(url);
}

} // namespace

// ---------------------------------------------------------------------------
// Release lookup
// ---------------------------------------------------------------------------
bool FetchLatestRelease(ReleaseInfo& info, std::string& error)
{
    info = ReleaseInfo{};
    error.clear();

    const std::wstring url = std::wstring(L"https://api.github.com/repos/") +
                             kOwner + L"/" + kRepo + L"/releases/latest";

    std::string body;
    bool tooBig = false;
    long long total = 0;

    const bool ok = HttpGet(url, L"application/vnd.github+json",
                            [&](const char* data, size_t size) {
        if (body.size() + size > kMaxText) { tooBig = true; return false; }
        body.append(data, size);
        return true;
    }, total, error);

    if (!ok) return false;
    if (tooBig) {
        error = "the server response was unexpectedly large";
        return false;
    }

    return ParseReleaseJson(body, "drbcWindowsInstaller.exe", info, error);
}

// ---------------------------------------------------------------------------
// Download
// ---------------------------------------------------------------------------
bool DownloadFile(const std::string& url, const std::filesystem::path& dest,
                  const std::function<void(int)>& progress,
                  const std::function<bool()>& cancel,
                  std::string& error)
{
    error.clear();

    FILE* file = _wfopen(dest.c_str(), L"wb");
    if (!file) {
        error = "could not create the download file";
        return false;
    }

    bool writeFailed = false;
    bool cancelled = false;
    unsigned long long written = 0;
    long long total = 0;

    const bool ok = HttpGet(NarrowToWideUrl(url), L"application/octet-stream",
                            [&](const char* data, size_t size) {
        if (cancel && cancel()) { cancelled = true; return false; }
        if (fwrite(data, 1, size, file) != size) { writeFailed = true; return false; }
        written += size;
        if (progress) {
            const int percent = total > 0
                ? static_cast<int>((written * 100) / static_cast<unsigned long long>(total))
                : -1;
            progress(percent > 100 ? 100 : percent);
        }
        return true;
    }, total, error);

    fclose(file);

    if (cancelled) {
        error = "download cancelled";
        DeleteFileW(dest.c_str());
        return false;
    }
    if (writeFailed) {
        error = "could not write the download file (disk full?)";
        DeleteFileW(dest.c_str());
        return false;
    }
    if (!ok) {
        DeleteFileW(dest.c_str());
        return false;
    }
    if (progress) progress(100);
    return true;
}

// ---------------------------------------------------------------------------
// SHA-256
// ---------------------------------------------------------------------------
bool VerifyDigest(const std::filesystem::path& file, const std::string& digest,
                  std::string& error)
{
    error.clear();
    if (digest.empty()) return true;

    std::string expected = digest;
    if (expected.rfind("sha256:", 0) == 0) expected = expected.substr(7);
    if (expected.size() != 64) {
        error = "the published checksum has an unexpected format";
        return false;
    }

    BCRYPT_ALG_HANDLE algorithm = nullptr;
    BCRYPT_HASH_HANDLE hash = nullptr;
    std::vector<unsigned char> hashObject;
    std::vector<unsigned char> hashValue;

    bool result = false;
    FILE* input = _wfopen(file.c_str(), L"rb");

    do {
        if (!input) { error = "could not read the downloaded file"; break; }
        if (!BCRYPT_SUCCESS(BCryptOpenAlgorithmProvider(&algorithm, BCRYPT_SHA256_ALGORITHM,
                                                        nullptr, 0))) {
            error = "the checksum algorithm is unavailable";
            break;
        }

        DWORD objectSize = 0, resultSize = 0, produced = 0;
        if (!BCRYPT_SUCCESS(BCryptGetProperty(algorithm, BCRYPT_OBJECT_LENGTH,
                                              reinterpret_cast<PUCHAR>(&objectSize),
                                              sizeof(objectSize), &produced, 0)) ||
            !BCRYPT_SUCCESS(BCryptGetProperty(algorithm, BCRYPT_HASH_LENGTH,
                                              reinterpret_cast<PUCHAR>(&resultSize),
                                              sizeof(resultSize), &produced, 0))) {
            error = "the checksum could not be set up";
            break;
        }

        hashObject.resize(objectSize);
        hashValue.resize(resultSize);

        if (!BCRYPT_SUCCESS(BCryptCreateHash(algorithm, &hash, hashObject.data(), objectSize,
                                             nullptr, 0, 0))) {
            error = "the checksum could not be set up";
            break;
        }

        std::vector<unsigned char> buffer(64 * 1024);
        size_t read = 0;
        bool readFailed = false;
        while ((read = fread(buffer.data(), 1, buffer.size(), input)) > 0) {
            if (!BCRYPT_SUCCESS(BCryptHashData(hash, buffer.data(),
                                               static_cast<ULONG>(read), 0))) {
                readFailed = true;
                break;
            }
        }
        if (readFailed || ferror(input)) {
            error = "could not read the downloaded file";
            break;
        }

        if (!BCRYPT_SUCCESS(BCryptFinishHash(hash, hashValue.data(), resultSize, 0))) {
            error = "the checksum could not be completed";
            break;
        }

        static const char* kHex = "0123456789abcdef";
        std::string actual;
        actual.reserve(resultSize * 2);
        for (unsigned char byte : hashValue) {
            actual.push_back(kHex[byte >> 4]);
            actual.push_back(kHex[byte & 0x0F]);
        }

        auto lower = [](std::string text) {
            for (char& c : text) c = static_cast<char>(tolower(static_cast<unsigned char>(c)));
            return text;
        };

        if (lower(actual) != lower(expected)) {
            error = "the downloaded file does not match the published checksum";
            break;
        }
        result = true;
    } while (false);

    if (hash) BCryptDestroyHash(hash);
    if (algorithm) BCryptCloseAlgorithmProvider(algorithm, 0);
    if (input) fclose(input);

    return result;
}

// ---------------------------------------------------------------------------
// Installer
// ---------------------------------------------------------------------------
bool RunInstaller(const std::filesystem::path& installer, std::string& error)
{
    error.clear();

    // A per-user install (inside %LOCALAPPDATA%) can be upgraded in place
    // without asking anything. A per-machine install (Program Files) needs the
    // normal wizard, because Inno has to request elevation there.
    auto lower = [](std::wstring text) {
        for (wchar_t& c : text) c = static_cast<wchar_t>(towlower(c));
        return text;
    };

    wchar_t self[MAX_PATH * 2] = {};
    GetModuleFileNameW(nullptr, self, static_cast<DWORD>(std::size(self)));
    wchar_t localAppData[MAX_PATH] = {};
    GetEnvironmentVariableW(L"LOCALAPPDATA", localAppData,
                            static_cast<DWORD>(std::size(localAppData)));

    const std::wstring selfPath = lower(self);
    const std::wstring localPath = lower(localAppData);
    const bool perUser = !localPath.empty() &&
                         selfPath.rfind(localPath + L"\\", 0) == 0;

    // /SILENT shows only the progress window; /CURRENTUSER skips the install
    // mode page, which would otherwise wait for input.
    const wchar_t* parameters =
        perUser ? L"/SILENT /SUPPRESSMSGBOXES /CLOSEAPPLICATIONS /NORESTART /CURRENTUSER"
                : L"";

    const HINSTANCE result = ShellExecuteW(nullptr, L"open", installer.c_str(),
                                           parameters, nullptr, SW_SHOWNORMAL);
    if (reinterpret_cast<INT_PTR>(result) <= 32) {
        error = "Windows could not start the installer";
        return false;
    }
    return true;
}

std::filesystem::path UpdateDir(bool create)
{
    auto dir = getSettingsDir(create);
    if (!dir) return {};
    std::filesystem::path path = *dir / L"update";
    if (create) {
        std::error_code ec;
        std::filesystem::create_directories(path, ec);
    }
    return path;
}

} // namespace upd
