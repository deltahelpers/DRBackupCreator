#include "drbackup.h"
#include <algorithm>
#include <string>

std::optional<SlotFile> parseSlotFile(const fs::path& p)
{
    const std::wstring stem   = p.stem().wstring();
    const std::wstring prefix = L"filech";

    if (stem.size() <= prefix.size() || stem.compare(0, prefix.size(), prefix) != 0)
        return std::nullopt;

    const std::wstring rest = stem.substr(prefix.size());   // "1_9"
    const auto sep = rest.find(L'_');
    if (sep == std::wstring::npos) return std::nullopt;

    try {
        SlotFile s;
        s.chapter = std::stoi(rest.substr(0, sep));
        s.index   = std::stoi(rest.substr(sep + 1));
        s.path    = p;

        std::error_code ec;
        s.size = fs::file_size(p, ec);
        if (ec) s.size = 0;
        return s;
    } catch (const std::exception&) {
        return std::nullopt;
    }
}

std::vector<SlotFile> scanSlots(const fs::path& dir)
{
    std::vector<SlotFile> out;
    std::error_code ec;

    for (const auto& e : fs::directory_iterator(dir, ec)) {
        if (!e.is_regular_file(ec)) continue;
        if (auto s = parseSlotFile(e.path())) out.push_back(*s);
    }

    std::sort(out.begin(), out.end(), [](const SlotFile& a, const SlotFile& b) {
        return a.chapter != b.chapter ? a.chapter < b.chapter : a.index < b.index;
    });
    return out;
}

void groupBySave(std::vector<SlotFile>& slots)
{
    std::sort(slots.begin(), slots.end(), [](const SlotFile& a, const SlotFile& b) {
        int aGroup = a.chapter * 10 + std::min(a.index, a.pairedIndex());
        int bGroup = b.chapter * 10 + std::min(b.index, b.pairedIndex());
        if (aGroup != bGroup) return aGroup < bGroup;
        return !a.isCompletion() && b.isCompletion();
    });
}