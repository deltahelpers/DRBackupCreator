#pragma once

#include <string>

namespace ui {

enum class Lang { ES = 0, EN = 1 };

struct Strings {
    const wchar_t* appTitle;
    const wchar_t* appSubtitle;

    const wchar_t* savesLabel;
    const wchar_t* backupsLabel;
    const wchar_t* open;
    const wchar_t* openFolder;

    const wchar_t* slotsSection;   // %d
    const wchar_t* emptySlots;
    const wchar_t* btnCreate;
    const wchar_t* btnBackups;
    const wchar_t* btnRefresh;
    const wchar_t* btnExit;

    const wchar_t* gameWarning;
    const wchar_t* gameOk;
    const wchar_t* statusReady;
    const wchar_t* refreshed;

    const wchar_t* backupOk;       // %s
    const wchar_t* backupErr;
    const wchar_t* noSlots;
    const wchar_t* noSaveDir;

    const wchar_t* colIndex;
    const wchar_t* colChapter;
    const wchar_t* colSlot;
    const wchar_t* colType;
    const wchar_t* colSize;
    const wchar_t* colFile;

    const wchar_t* typeSave;
    const wchar_t* typeCompletion;

    const wchar_t* backupsTitle;
    const wchar_t* backupsCount;   // %d
    const wchar_t* backupsCountOne; // %d
    const wchar_t* colDate;
    const wchar_t* colCount;
    const wchar_t* colFolder;
    const wchar_t* noBackups;

    const wchar_t* btnRestore;
    const wchar_t* btnDelete;
    const wchar_t* btnClose;

    const wchar_t* restoreTitle;
    const wchar_t* restoreText;    // %s
    const wchar_t* restoreOk;      // %d
    const wchar_t* restoreErr;

    const wchar_t* deleteTitle;
    const wchar_t* deleteText;     // %s
    const wchar_t* deleteOk;
    const wchar_t* deleteErr;

    const wchar_t* selectBackup;
    const wchar_t* openErr;

    const wchar_t* aboutTitle;
    const wchar_t* aboutBody;      // %s version
    const wchar_t* btnGithub;
    const wchar_t* btnWebsite;

    const wchar_t* langTitle;
    const wchar_t* langPrompt;
    const wchar_t* btnSpanish;
    const wchar_t* btnEnglish;

    const wchar_t* yes;
    const wchar_t* no;
    const wchar_t* cancel;
    const wchar_t* statusSlots;    // %d
};

Lang GetLang();
void SetLang(Lang lang);

const Strings& T();

// Returns false when the settings file has no language yet (first run).
bool LoadLang();
void SaveLang(Lang lang);

std::wstring SettingsPath();

} // namespace ui
