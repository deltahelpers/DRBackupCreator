#include "update_parse.h"

#include <iostream>
#include <string>

namespace {

int g_failures = 0;

#define CHECK(condition)                                                       \
    do {                                                                       \
        if (!(condition)) {                                                    \
            std::cerr << "FAIL line " << __LINE__ << ": " #condition "\n";     \
            ++g_failures;                                                      \
        }                                                                      \
    } while (0)

#define CHECK_EQ(a, b)                                                         \
    do {                                                                       \
        const auto lhs_ = (a);                                                 \
        const auto rhs_ = (b);                                                 \
        if (!(lhs_ == rhs_)) {                                                 \
            std::cerr << "FAIL line " << __LINE__ << ": " #a " != " #b         \
                      << "  (" << lhs_ << " vs " << rhs_ << ")\n";             \
            ++g_failures;                                                      \
        }                                                                      \
    } while (0)

void TestVersions()
{
    using upd::CompareVersions;
    using upd::IsNewer;

    CHECK_EQ(CompareVersions("2.1.0", "2.1.0"), 0);
    CHECK_EQ(CompareVersions("v2.1.0", "2.1.0"), 0);
    CHECK_EQ(CompareVersions("2.2.0", "2.1.9"), 1);
    CHECK_EQ(CompareVersions("2.1.9", "2.2.0"), -1);
    CHECK_EQ(CompareVersions("2.10.0", "2.9.9"), 1);
    CHECK_EQ(CompareVersions("10.0.0", "9.99.99"), 1);

    // Missing components count as zero.
    CHECK_EQ(CompareVersions("2.1", "2.1.0"), 0);
    CHECK_EQ(CompareVersions("2.1.1", "2.1"), 1);

    // Suffixes stop the numeric comparison.
    CHECK_EQ(CompareVersions("2.2.0-beta", "2.2.0"), 0);
    CHECK_EQ(CompareVersions("2.2.0+build7", "2.2.0"), 0);
    CHECK_EQ(CompareVersions("2.2.0-rc1", "2.1.9"), 1);

    CHECK(IsNewer("2.2.0", "2.1.0"));
    CHECK(!IsNewer("2.1.0", "2.1.0"));
    CHECK(!IsNewer("2.0.0", "2.1.0"));
    CHECK(!IsNewer("", "2.1.0"));
}

const char* kReleaseJson = R"({
  "url": "https://api.github.com/repos/deltahelpers/DRBackupCreator/releases/1",
  "html_url": "https://github.com/deltahelpers/DRBackupCreator/releases/tag/v2.2.0",
  "tag_name": "v2.2.0",
  "name": "DRBackupCreator v2.2.0",
  "draft": false,
  "prerelease": false,
  "created_at": "2026-11-20T10:00:00Z",
  "body": "## What's new\r\n\r\n- Automatic updater\r\n- **Checks** the release `notes`\r\n\r\n### Notes\r\nQuotes: \"like this\" and \u00e9 accents.\r\n",
  "uploader": { "login": "github-actions[bot]", "id": 41898282 },
  "assets": [
    {
      "name": "DRBackupCreator-2.2.0-win32.zip",
      "size": 1600898,
      "browser_download_url": "https://github.com/deltahelpers/DRBackupCreator/releases/download/v2.2.0/DRBackupCreator-2.2.0-win32.zip",
      "digest": "sha256:aaaa"
    },
    {
      "name": "drbcWindowsInstaller.exe",
      "size": 2930366,
      "browser_download_url": "https://github.com/deltahelpers/DRBackupCreator/releases/download/v2.2.0/drbcWindowsInstaller.exe",
      "digest": "sha256:bbbb",
      "uploader": { "login": "bot", "nested": [1, 2, {"x": true}] }
    },
    {
      "name": "drbcWindowsInstaller.msi",
      "size": 1347584,
      "browser_download_url": "https://github.com/deltahelpers/DRBackupCreator/releases/download/v2.2.0/drbcWindowsInstaller.msi"
    }
  ]
})";

void TestReleaseJson()
{
    upd::ReleaseInfo info;
    std::string error;

    CHECK(upd::ParseReleaseJson(kReleaseJson, "drbcWindowsInstaller.exe", info, error));
    CHECK(error.empty());
    CHECK_EQ(info.version, std::string("2.2.0"));
    CHECK_EQ(info.installerName, std::string("drbcWindowsInstaller.exe"));
    CHECK_EQ(info.installerDigest, std::string("sha256:bbbb"));
    CHECK(info.installerSize == 2930366);
    CHECK_EQ(info.installerUrl,
             std::string("https://github.com/deltahelpers/DRBackupCreator/releases/"
                         "download/v2.2.0/drbcWindowsInstaller.exe"));
    CHECK_EQ(info.htmlUrl,
             std::string("https://github.com/deltahelpers/DRBackupCreator/releases/tag/v2.2.0"));

    // Escapes in the body are decoded.
    CHECK(info.notes.find("What's new") != std::string::npos);
    CHECK(info.notes.find("\"like this\"") != std::string::npos);
    CHECK(info.notes.find("\xC3\xA9") != std::string::npos);   // é

    // A missing asset still parses: the UI then offers the release page.
    upd::ReleaseInfo partial;
    CHECK(upd::ParseReleaseJson(kReleaseJson, "does-not-exist.exe", partial, error));
    CHECK(partial.installerUrl.empty());

    // API error documents.
    upd::ReleaseInfo none;
    CHECK(!upd::ParseReleaseJson(R"({"message":"Not Found"})", "x", none, error));
    CHECK_EQ(error, std::string("Not Found"));

    // Garbage.
    CHECK(!upd::ParseReleaseJson("not json", "x", none, error));
    CHECK(!error.empty());

    CHECK(!upd::ParseReleaseJson("{}", "x", none, error));
    CHECK(!error.empty());
}

void TestNotesToPlainText()
{
    const std::string markdown =
        "## Highlights\r\n"
        "\r\n"
        "- Native **Win32** GUI\r\n"
        "- See [the docs](https://example.com/docs)\r\n"
        "\r\n"
        "Run `drbc.exe` now.\r\n";

    const std::string text = upd::NotesToPlainText(markdown);
    CHECK(text.find("##") == std::string::npos);
    CHECK(text.find("**") == std::string::npos);
    CHECK(text.find('`') == std::string::npos);
    CHECK(text.find("Highlights") != std::string::npos);
    CHECK(text.find("Native Win32 GUI") != std::string::npos);
    CHECK(text.find("the docs") != std::string::npos);
    CHECK(text.find("https://example.com/docs") == std::string::npos);
    CHECK(text.find("Run drbc.exe now.") != std::string::npos);
    CHECK(text.find("\xE2\x80\xA2") != std::string::npos);   // bullet
}

} // namespace

int main()
{
    TestVersions();
    TestReleaseJson();
    TestNotesToPlainText();

    if (g_failures) {
        std::cerr << g_failures << " check(s) failed\n";
        return 1;
    }
    std::cout << "update tests passed\n";
    return 0;
}
