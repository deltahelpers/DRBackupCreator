#pragma once

// Pure helpers for the update checker: no Windows headers, so the tests can
// link them without WinHTTP.

#include <string>

namespace upd {

struct ReleaseInfo {
    std::string version;          // "2.2.0" (the leading v is stripped)
    std::string notes;            // release body, markdown
    std::string htmlUrl;          // human page, used as a fallback link
    std::string installerUrl;     // browser_download_url of the .exe asset
    std::string installerDigest;  // "sha256:..." as reported by GitHub
    std::string installerName;    // asset file name
    long long   installerSize = 0;
};

// Compares dotted versions ("2.1.0" vs "2.10.0"), ignoring a leading v and any
// suffix after the numbers ("-beta", "+build"). Returns <0, 0 or >0.
int CompareVersions(const std::string& a, const std::string& b);

// True when `latest` is strictly newer than `current`.
bool IsNewer(const std::string& latest, const std::string& current);

// Parses the JSON returned by /repos/{owner}/{repo}/releases/latest.
// `assetName` picks which binary to download. Returns false and fills `error`
// when the document is not a release object (or has no tag_name).
bool ParseReleaseJson(const std::string& json, const std::string& assetName,
                      ReleaseInfo& out, std::string& error);

// Turns the release markdown into something that reads well in a plain edit
// control: headings lose their hashes, **bold** and `code` lose their marks,
// "- item" becomes "• item".
std::string NotesToPlainText(const std::string& markdown);

} // namespace upd
