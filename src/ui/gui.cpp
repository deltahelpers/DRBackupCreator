#include "gui.h"

#include "dialogs.h"
#include "theme.h"
#include "i18n.h"
#include "resource.h"
#include "settings.h"
#include "version.h"
#include "drbackup.h"

#include <commctrl.h>
#include <shellapi.h>

#include <string>
#include <vector>

namespace ui {

namespace {

enum ControlId : int {
    IDC_SLOTS = 1001,
    IDC_SAVES_EDIT,
    IDC_BACKUPS_EDIT,
    IDC_OPEN_SAVES,
    IDC_OPEN_BACKUPS,
    IDC_CREATE,
    IDC_SHOW_BACKUPS,
    IDC_REFRESH,
    IDC_EXIT,
    IDC_LANG,
    IDC_SETTINGS,
    IDC_ABOUT
};

const wchar_t* kMainClass = L"DRBC_MainWindow";
constexpr UINT_PTR kTimerId = 1;

struct MainState {
    HWND hwnd = nullptr;

    HWND list = nullptr;
    HWND editSaves = nullptr;
    HWND editBackups = nullptr;
    HWND openSaves = nullptr;
    HWND openBackups = nullptr;
    HWND create = nullptr;
    HWND showBackups = nullptr;
    HWND refresh = nullptr;
    HWND exitButton = nullptr;
    HWND langButton = nullptr;
    HWND settingsButton = nullptr;
    HWND aboutButton = nullptr;

    HFONT font = nullptr;
    HFONT fontBold = nullptr;
    HFONT fontTitle = nullptr;
    HFONT fontSmall = nullptr;

    std::vector<SlotFile> slots;
    fs::path saveDir;
    fs::path backupDir;

    bool gameRunning = false;
    std::wstring status;
    std::wstring emptyText;
    COLORREF statusColor = kMuted;

    int dpi = 96;

    RECT card1{};
    RECT card2{};
    RECT field1{};
    RECT field2{};
    RECT listRect{};
    RECT banner{};
};

// ---------------------------------------------------------------------------
// Helpers
// ---------------------------------------------------------------------------
void EnableDpiAwareness()
{
    using SetContextFn = BOOL(WINAPI*)(HANDLE);

    if (HMODULE user32 = GetModuleHandleW(L"user32.dll")) {
        auto setContext = reinterpret_cast<SetContextFn>(
            GetProcAddress(user32, "SetProcessDpiAwarenessContext"));
        if (setContext) {
            // DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2
            if (setContext(reinterpret_cast<HANDLE>(static_cast<INT_PTR>(-4))))
                return;
        }
    }
    SetProcessDPIAware();
}

void SetStatus(MainState* st, const std::wstring& text, COLORREF color)
{
    st->status = text;
    st->statusColor = color;
    InvalidateRect(st->hwnd, nullptr, FALSE);
}

void SetColumnText(HWND list, int index, const wchar_t* text)
{
    LVCOLUMNW column{};
    column.mask = LVCF_TEXT;
    column.pszText = const_cast<wchar_t*>(text);
    SendMessageW(list, LVM_SETCOLUMNW, static_cast<WPARAM>(index),
                 reinterpret_cast<LPARAM>(&column));
}

void RecreateFonts(MainState* st)
{
    auto release = [](HFONT& font) {
        if (font) { DeleteObject(font); font = nullptr; }
    };

    release(st->font);
    release(st->fontBold);
    release(st->fontTitle);
    release(st->fontSmall);

    st->dpi = DpiOf(st->hwnd);
    st->font      = MakeFont(9, FW_NORMAL, st->dpi);
    st->fontBold  = MakeFont(9, FW_SEMIBOLD, st->dpi);
    st->fontTitle = MakeFont(18, FW_SEMIBOLD, st->dpi);
    st->fontSmall = MakeFont(8, FW_NORMAL, st->dpi);

    HWND controls[] = {
        st->list, st->editSaves, st->editBackups, st->openSaves, st->openBackups,
        st->create, st->showBackups, st->refresh, st->exitButton, st->langButton,
        st->settingsButton, st->aboutButton
    };

    for (HWND control : controls) {
        if (control)
            SendMessageW(control, WM_SETFONT,
                         reinterpret_cast<WPARAM>(st->font), TRUE);
    }
}

std::wstring SlotsBadgeText(const MainState* st)
{
    return FormatText(L"%d slots · v" DRBC_VERSION_W,
                      static_cast<int>(st->slots.size()));
}

// ---------------------------------------------------------------------------
// Slots
// ---------------------------------------------------------------------------
void PopulateSlots(MainState* st)
{
    if (!st->list) return;

    SendMessageW(st->list, WM_SETREDRAW, FALSE, 0);
    ListView_DeleteAllItems(st->list);

    for (size_t i = 0; i < st->slots.size(); ++i) {
        const SlotFile& slot = st->slots[i];

        std::wstring index   = std::to_wstring(i + 1);
        std::wstring chapter = std::to_wstring(slot.chapter);
        std::wstring number  = std::to_wstring(slot.index);
        std::wstring type    = slot.isCompletion() ? T().typeCompletion : T().typeSave;
        std::wstring size    = SizeText(slot.size);
        std::wstring file    = slot.path.filename().wstring();

        LVITEMW item{};
        item.mask    = LVIF_TEXT;
        item.iItem   = static_cast<int>(i);
        item.pszText = index.data();
        int row = ListView_InsertItem(st->list, &item);

        ListView_SetItemText(st->list, row, 1, chapter.data());
        ListView_SetItemText(st->list, row, 2, number.data());
        ListView_SetItemText(st->list, row, 3, type.data());
        ListView_SetItemText(st->list, row, 4, size.data());
        ListView_SetItemText(st->list, row, 5, file.data());
    }

    SendMessageW(st->list, WM_SETREDRAW, TRUE, 0);
    InvalidateRect(st->list, nullptr, TRUE);
}

void RefreshSlots(MainState* st, bool announce)
{
    st->saveDir.clear();
    if (auto dir = getDeltaruneDir(false)) st->saveDir = *dir;

    st->slots.clear();
    if (!st->saveDir.empty()) {
        st->slots = scanSlots(st->saveDir);
        groupBySave(st->slots);
    }

    if (!st->backupDir.empty())
        SetWindowTextW(st->editBackups, st->backupDir.c_str());
    SetWindowTextW(st->editSaves,
                   st->saveDir.empty() ? L"" : st->saveDir.c_str());

    st->emptyText = st->saveDir.empty() ? T().noSaveDir : T().emptySlots;

    PopulateSlots(st);

    ShowWindow(st->list, st->slots.empty() ? SW_HIDE : SW_SHOW);

    if (announce) SetStatus(st, T().refreshed, kSuccess);
    InvalidateRect(st->hwnd, nullptr, FALSE);
}

// ---------------------------------------------------------------------------
// Actions
// ---------------------------------------------------------------------------
void CreateBackupNow(MainState* st)
{
    if (st->gameRunning) {
        SetStatus(st, T().gameWarning, kDanger);
        return;
    }
    if (st->slots.empty()) {
        SetStatus(st, T().noSlots, kDanger);
        return;
    }

    fs::path target = st->backupDir;
    if (target.empty()) {
        if (auto dir = getBackupDir(true)) target = *dir;
    }
    if (target.empty()) {
        SetStatus(st, T().backupErr, kDanger);
        return;
    }

    const std::string name = createBackup(target, st->slots);
    if (name.empty()) {
        SetStatus(st, T().backupErr, kDanger);
    } else {
        SetStatus(st, FormatText(T().backupOk, Widen(name).c_str()), kSuccess);
    }
}

void OpenPath(MainState* st, const fs::path& path)
{
    if (path.empty()) {
        SetStatus(st, T().openErr, kDanger);
        return;
    }

    HINSTANCE result = ShellExecuteW(st->hwnd, L"open", path.c_str(),
                                     nullptr, nullptr, SW_SHOWNORMAL);
    if (reinterpret_cast<INT_PTR>(result) <= 32)
        SetStatus(st, T().openErr, kDanger);
}

void OpenSavesFolder(MainState* st)
{
    if (st->saveDir.empty()) {
        SetStatus(st, T().noSaveDir, kDanger);
        return;
    }
    OpenPath(st, st->saveDir);
}

void OpenBackupsFolder(MainState* st)
{
    fs::path dir = st->backupDir;
    if (dir.empty()) {
        if (auto created = getBackupDir(true)) dir = *created;
    }
    OpenPath(st, dir);
}

void ShowBackups(MainState* st)
{
    fs::path saves;
    if (auto dir = getDeltaruneDir(false)) saves = *dir;

    ShowBackupsDialog(st->hwnd, saves, [st]() { RefreshSlots(st, false); });
    RefreshSlots(st, false);
}

void ApplyThemeChange(MainState* st);
void LayoutMain(MainState* st);

// Re-applies every visible string. Used after a language change and after the
// Settings dialog imports a .bcconfig.
void RefreshTexts(MainState* st)
{
    SetWindowTextW(st->hwnd, T().appTitle);
    SetWindowTextW(st->langButton, GetLang() == Lang::EN ? L"EN" : L"ES");
    SetWindowTextW(st->create, T().btnCreate);
    SetWindowTextW(st->showBackups, T().btnBackups);
    SetWindowTextW(st->refresh, T().btnRefresh);
    SetWindowTextW(st->exitButton, T().btnExit);
    SetWindowTextW(st->openSaves, T().open);
    SetWindowTextW(st->openBackups, T().open);

    SetColumnText(st->list, 0, T().colIndex);
    SetColumnText(st->list, 1, T().colChapter);
    SetColumnText(st->list, 2, T().colSlot);
    SetColumnText(st->list, 3, T().colType);
    SetColumnText(st->list, 4, T().colSize);
    SetColumnText(st->list, 5, T().colFile);
}

void ToggleLanguage(MainState* st)
{
    const Lang next = GetLang() == Lang::ES ? Lang::EN : Lang::ES;
    SetLang(next);
    SaveLang(next);

    RefreshTexts(st);

    st->emptyText = st->saveDir.empty() ? T().noSaveDir : T().emptySlots;
    st->status = T().statusReady;
    st->statusColor = kMuted;

    PopulateSlots(st);
    InvalidateRect(st->hwnd, nullptr, TRUE);
}

// Called by the Settings dialog after a theme/config change: rebuild the
// palette-derived fonts and repaint everything.
void ApplyThemeChange(MainState* st)
{
    RecreateFonts(st);
    RefreshTexts(st);
    st->emptyText = st->saveDir.empty() ? T().noSaveDir : T().emptySlots;
    LayoutMain(st);
    InvalidateRect(st->hwnd, nullptr, TRUE);
}

// ---------------------------------------------------------------------------
// Layout / paint
// ---------------------------------------------------------------------------
void LayoutMain(MainState* st)
{
    if (!st->hwnd) return;

    RECT rc{};
    GetClientRect(st->hwnd, &rc);
    const int width  = rc.right;
    const int height = rc.bottom;

    const int m = S(st->hwnd, 20);
    const int gap = S(st->hwnd, 10);
    const int cardHeight = S(st->hwnd, 84);
    const int fieldHeight = S(st->hwnd, 30);
    const int openWidth = S(st->hwnd, 96);
    const int headerButtonHeight = S(st->hwnd, 30);

    const int aboutWidth = headerButtonHeight;
    const int langWidth = S(st->hwnd, 52);

    if (st->aboutButton)
        MoveWindow(st->aboutButton, width - m - aboutWidth, S(st->hwnd, 16),
                   aboutWidth, headerButtonHeight, TRUE);
    if (st->settingsButton)
        MoveWindow(st->settingsButton,
                   width - m - aboutWidth - aboutWidth - S(st->hwnd, 8),
                   S(st->hwnd, 16), aboutWidth, headerButtonHeight, TRUE);
    if (st->langButton)
        MoveWindow(st->langButton,
                   width - m - aboutWidth * 2 - langWidth - S(st->hwnd, 16),
                   S(st->hwnd, 16), langWidth, headerButtonHeight, TRUE);

    const int card1Y = S(st->hwnd, 78);
    const int card2Y = card1Y + cardHeight + gap;
    st->card1 = RECT{m, card1Y, width - m, card1Y + cardHeight};
    st->card2 = RECT{m, card2Y, width - m, card2Y + cardHeight};

    const int editX = m + S(st->hwnd, 14);
    const int openX = width - m - S(st->hwnd, 14) - openWidth;
    const int editWidth = openX - S(st->hwnd, 8) - editX;

    const int field1Y = card1Y + S(st->hwnd, 38);
    st->field1 = RECT{editX, field1Y, openX - S(st->hwnd, 8), field1Y + fieldHeight};
    if (st->editSaves)
        MoveWindow(st->editSaves, editX + S(st->hwnd, 2), field1Y + S(st->hwnd, 2),
                   editWidth - S(st->hwnd, 4), fieldHeight - S(st->hwnd, 4), TRUE);
    if (st->openSaves)
        MoveWindow(st->openSaves, openX, field1Y, openWidth, fieldHeight, TRUE);

    const int field2Y = card2Y + S(st->hwnd, 38);
    st->field2 = RECT{editX, field2Y, openX - S(st->hwnd, 8), field2Y + fieldHeight};
    if (st->editBackups)
        MoveWindow(st->editBackups, editX + S(st->hwnd, 2), field2Y + S(st->hwnd, 2),
                   editWidth - S(st->hwnd, 4), fieldHeight - S(st->hwnd, 4), TRUE);
    if (st->openBackups)
        MoveWindow(st->openBackups, openX, field2Y, openWidth, fieldHeight, TRUE);

    const int sectionY = card2Y + cardHeight + S(st->hwnd, 18);
    const int listTop = sectionY + S(st->hwnd, 24);
    const int statusHeight = S(st->hwnd, 28);
    const int actionHeight = S(st->hwnd, 38);
    const int actionY = height - statusHeight - S(st->hwnd, 14) - actionHeight;

    const int bannerHeight = st->gameRunning ? S(st->hwnd, 40) : 0;
    const int bannerY = actionY - (bannerHeight ? bannerHeight + S(st->hwnd, 10) : 0);
    st->banner = RECT{m, bannerY, width - m, bannerY + bannerHeight};

    const int listBottom = bannerHeight ? bannerY - S(st->hwnd, 10)
                                        : actionY - S(st->hwnd, 12);
    st->listRect = RECT{m, listTop, width - m, listBottom > listTop ? listBottom : listTop + 40};

    if (st->list) {
        MoveWindow(st->list, st->listRect.left, st->listRect.top,
                   st->listRect.right - st->listRect.left,
                   st->listRect.bottom - st->listRect.top, TRUE);

        const int listWidth = st->listRect.right - st->listRect.left;
        const int c0 = S(st->hwnd, 40);
        const int c1 = S(st->hwnd, 80);
        const int c2 = S(st->hwnd, 62);
        const int c3 = S(st->hwnd, 122);
        const int c4 = S(st->hwnd, 96);
        int c5 = listWidth - c0 - c1 - c2 - c3 - c4 - S(st->hwnd, 28);
        if (c5 < S(st->hwnd, 140)) c5 = S(st->hwnd, 140);

        const int widths[6] = {c0, c1, c2, c3, c4, c5};
        for (int i = 0; i < 6; ++i)
            ListView_SetColumnWidth(st->list, i, widths[i]);
    }

    const int createWidth  = S(st->hwnd, 170);
    const int backupsWidth = S(st->hwnd, 150);
    const int refreshWidth = S(st->hwnd, 122);
    const int exitWidth    = S(st->hwnd, 100);

    int x = m;
    if (st->create)
        MoveWindow(st->create, x, actionY, createWidth, actionHeight, TRUE);
    x += createWidth + gap;
    if (st->showBackups)
        MoveWindow(st->showBackups, x, actionY, backupsWidth, actionHeight, TRUE);
    x += backupsWidth + gap;
    if (st->refresh)
        MoveWindow(st->refresh, x, actionY, refreshWidth, actionHeight, TRUE);
    if (st->exitButton)
        MoveWindow(st->exitButton, width - m - exitWidth, actionY, exitWidth,
                   actionHeight, TRUE);
}

void PaintMain(MainState* st)
{
    BufferedPainter paint(st->hwnd);
    HDC dc = paint.dc();
    const RECT rc = paint.client();
    const int width = rc.right;
    RECT client{};
    GetClientRect(st->hwnd, &client);

    const int m = S(st->hwnd, 20);

    // Header
    RECT accent{m, S(st->hwnd, 18), m + S(st->hwnd, 4), S(st->hwnd, 48)};
    FillRectC(dc, accent, kAccent);

    RECT titleRc{m + S(st->hwnd, 14), S(st->hwnd, 10),
                 width - m - S(st->hwnd, 180), S(st->hwnd, 40)};
    DrawWrapped(dc, T().appTitle, titleRc, st->fontTitle, kText,
                DT_LEFT | DT_SINGLELINE | DT_VCENTER | DT_NOPREFIX);

    RECT subtitleRc{m + S(st->hwnd, 14), S(st->hwnd, 40),
                    width - m - S(st->hwnd, 180), S(st->hwnd, 62)};
    DrawWrapped(dc, T().appSubtitle, subtitleRc, st->fontSmall, kMuted,
                DT_LEFT | DT_SINGLELINE | DT_VCENTER | DT_NOPREFIX);

    // Cards
    FillRoundRectC(dc, st->card1, S(st->hwnd, 10), kPanel, kBorder);
    FillRoundRectC(dc, st->card2, S(st->hwnd, 10), kPanel, kBorder);

    RECT label1{st->card1.left + S(st->hwnd, 14), st->card1.top + S(st->hwnd, 10),
                st->card1.right - S(st->hwnd, 14), st->card1.top + S(st->hwnd, 32)};
    DrawWrapped(dc, T().savesLabel, label1, st->fontBold, kMuted,
                DT_LEFT | DT_SINGLELINE | DT_VCENTER | DT_NOPREFIX);

    RECT label2{st->card2.left + S(st->hwnd, 14), st->card2.top + S(st->hwnd, 10),
                st->card2.right - S(st->hwnd, 14), st->card2.top + S(st->hwnd, 32)};
    DrawWrapped(dc, T().backupsLabel, label2, st->fontBold, kMuted,
                DT_LEFT | DT_SINGLELINE | DT_VCENTER | DT_NOPREFIX);

    FillRoundRectC(dc, st->field1, S(st->hwnd, 5), kInput, kBorder);
    FillRoundRectC(dc, st->field2, S(st->hwnd, 5), kInput, kBorder);

    // Slots section
    RECT sectionRc{m, st->listRect.top - S(st->hwnd, 24), width - m,
                   st->listRect.top - S(st->hwnd, 2)};
    DrawWrapped(dc, FormatText(T().slotsSection, static_cast<int>(st->slots.size())),
                sectionRc, st->fontBold, kText,
                DT_LEFT | DT_SINGLELINE | DT_VCENTER | DT_NOPREFIX);

    if (st->slots.empty()) {
        RECT empty = st->listRect;
        InflateRect(&empty, -S(st->hwnd, 24), -S(st->hwnd, 12));
        DrawTextCentered(dc, st->emptyText, empty, st->font, kMuted);
    }

    // Game warning
    if (st->gameRunning) {
        FillRoundRectC(dc, st->banner, S(st->hwnd, 8), kWarnBg, kWarnLine);
        RECT dot{st->banner.left + S(st->hwnd, 14),
                 st->banner.top + (st->banner.bottom - st->banner.top) / 2 - S(st->hwnd, 4),
                 0, 0};
        dot.right = dot.left + S(st->hwnd, 8);
        dot.bottom = dot.top + S(st->hwnd, 8);
        HBRUSH brush = CreateSolidBrush(kAccent);
        HGDIOBJ old = SelectObject(dc, brush);
        HGDIOBJ oldPen = SelectObject(dc, GetStockObject(NULL_PEN));
        Ellipse(dc, dot.left, dot.top, dot.right, dot.bottom);
        SelectObject(dc, oldPen);
        SelectObject(dc, old);
        DeleteObject(brush);

        RECT warn{st->banner.left + S(st->hwnd, 30), st->banner.top,
                  st->banner.right - S(st->hwnd, 12), st->banner.bottom};
        DrawWrapped(dc, T().gameWarning, warn, st->font, kDanger,
                    DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS | DT_NOPREFIX);
    }

    // Status bar
    const int statusHeight = S(st->hwnd, 28);
    RECT statusRc{0, client.bottom - statusHeight, client.right, client.bottom};
    FillRectC(dc, statusRc, kBgDeep);
    RECT line{0, statusRc.top, client.right, statusRc.top + 1};
    FillRectC(dc, line, kBorder);

    RECT statusText{m, statusRc.top, client.right - S(st->hwnd, 180), statusRc.bottom};
    DrawWrapped(dc, st->status, statusText, st->fontSmall, st->statusColor,
                DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS | DT_NOPREFIX);

    RECT badge{client.right - S(st->hwnd, 180), statusRc.top, client.right - m,
               statusRc.bottom};
    DrawWrapped(dc, SlotsBadgeText(st), badge, st->fontSmall, kMuted,
                DT_RIGHT | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX);
}

// ---------------------------------------------------------------------------
// Creation
// ---------------------------------------------------------------------------
HWND CreateControl(MainState* st, const wchar_t* cls, const wchar_t* text,
                   DWORD style, DWORD exStyle, int id)
{
    return CreateWindowExW(exStyle, cls, text, WS_CHILD | WS_VISIBLE | style,
                           0, 0, 0, 0, st->hwnd,
                           reinterpret_cast<HMENU>(static_cast<INT_PTR>(id)),
                           GetModuleHandleW(nullptr), nullptr);
}

void CreateControls(MainState* st)
{
    st->list = CreateControl(st, WC_LISTVIEWW, L"",
                             LVS_REPORT | LVS_SINGLESEL | LVS_SHOWSELALWAYS | WS_TABSTOP,
                             0, IDC_SLOTS);
    ListView_SetExtendedListViewStyle(
        st->list, LVS_EX_FULLROWSELECT | LVS_EX_DOUBLEBUFFER | LVS_EX_LABELTIP);
    ApplyDarkListView(st->list);

    st->editSaves = CreateControl(st, L"EDIT", L"",
                                  ES_READONLY | ES_AUTOHSCROLL | WS_TABSTOP,
                                  0, IDC_SAVES_EDIT);
    st->editBackups = CreateControl(st, L"EDIT", L"",
                                    ES_READONLY | ES_AUTOHSCROLL | WS_TABSTOP,
                                    0, IDC_BACKUPS_EDIT);

    ApplyDarkEdit(st->editSaves);
    ApplyDarkEdit(st->editBackups);

    for (HWND edit : {st->editSaves, st->editBackups}) {
        SendMessageW(edit, EM_SETMARGINS, EC_LEFTMARGIN | EC_RIGHTMARGIN,
                     MAKELONG(S(st->hwnd, 8), S(st->hwnd, 8)));
    }

    st->openSaves = CreateControl(st, L"BUTTON", L"", WS_TABSTOP, 0, IDC_OPEN_SAVES);
    st->openBackups = CreateControl(st, L"BUTTON", L"", WS_TABSTOP, 0, IDC_OPEN_BACKUPS);
    st->create = CreateControl(st, L"BUTTON", L"", WS_TABSTOP | BS_DEFPUSHBUTTON,
                               0, IDC_CREATE);
    st->showBackups = CreateControl(st, L"BUTTON", L"", WS_TABSTOP, 0, IDC_SHOW_BACKUPS);
    st->refresh = CreateControl(st, L"BUTTON", L"", WS_TABSTOP, 0, IDC_REFRESH);
    st->exitButton = CreateControl(st, L"BUTTON", L"", WS_TABSTOP, 0, IDC_EXIT);
    st->langButton = CreateControl(st, L"BUTTON", L"", WS_TABSTOP, 0, IDC_LANG);
    st->settingsButton = CreateControl(st, L"BUTTON", L"", WS_TABSTOP, 0, IDC_SETTINGS);
    st->aboutButton = CreateControl(st, L"BUTTON", L"?", WS_TABSTOP, 0, IDC_ABOUT);

    StyleButton(st->openSaves, false, false, kPanel);
    StyleButton(st->openBackups, false, false, kPanel);
    StyleButton(st->create, true, false);
    StyleButton(st->showBackups, false, false);
    StyleButton(st->refresh, false, false);
    StyleButton(st->exitButton, false, false);
    StyleButton(st->langButton, false, false);
    StyleButton(st->settingsButton, false, false, kBg, true);
    StyleButton(st->aboutButton, false, false);

    LVCOLUMNW column{};
    column.mask = LVCF_TEXT | LVCF_WIDTH;
    column.cx = S(st->hwnd, 40);
    column.pszText = const_cast<wchar_t*>(T().colIndex);
    SendMessageW(st->list, LVM_INSERTCOLUMNW, 0, reinterpret_cast<LPARAM>(&column));

    column.cx = S(st->hwnd, 80);
    column.pszText = const_cast<wchar_t*>(T().colChapter);
    SendMessageW(st->list, LVM_INSERTCOLUMNW, 1, reinterpret_cast<LPARAM>(&column));

    column.cx = S(st->hwnd, 62);
    column.pszText = const_cast<wchar_t*>(T().colSlot);
    SendMessageW(st->list, LVM_INSERTCOLUMNW, 2, reinterpret_cast<LPARAM>(&column));

    column.cx = S(st->hwnd, 122);
    column.pszText = const_cast<wchar_t*>(T().colType);
    SendMessageW(st->list, LVM_INSERTCOLUMNW, 3, reinterpret_cast<LPARAM>(&column));

    column.cx = S(st->hwnd, 96);
    column.pszText = const_cast<wchar_t*>(T().colSize);
    SendMessageW(st->list, LVM_INSERTCOLUMNW, 4, reinterpret_cast<LPARAM>(&column));

    column.cx = S(st->hwnd, 300);
    column.pszText = const_cast<wchar_t*>(T().colFile);
    SendMessageW(st->list, LVM_INSERTCOLUMNW, 5, reinterpret_cast<LPARAM>(&column));
}

// ---------------------------------------------------------------------------
// Window procedure
// ---------------------------------------------------------------------------
LRESULT CALLBACK MainProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
    auto* st = reinterpret_cast<MainState*>(GetWindowLongPtrW(hwnd, GWLP_USERDATA));

    switch (msg) {
    case WM_NCCREATE: {
        auto* cs = reinterpret_cast<CREATESTRUCTW*>(lParam);
        auto* state = reinterpret_cast<MainState*>(cs->lpCreateParams);
        state->hwnd = hwnd;
        SetWindowLongPtrW(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(state));
        return TRUE;
    }
    case WM_CREATE: {
        if (!st) return -1;

        EnableDarkTitleBar(hwnd);
        RecreateFonts(st);

        st->backupDir.clear();
        if (auto dir = getBackupDir(true)) st->backupDir = *dir;

        st->gameRunning = isGameRunning();
        CreateControls(st);

        SetWindowTextW(st->hwnd, T().appTitle);
        SetWindowTextW(st->langButton, GetLang() == Lang::EN ? L"EN" : L"ES");
        SetWindowTextW(st->create, T().btnCreate);
        SetWindowTextW(st->showBackups, T().btnBackups);
        SetWindowTextW(st->refresh, T().btnRefresh);
        SetWindowTextW(st->exitButton, T().btnExit);
        SetWindowTextW(st->openSaves, T().open);
        SetWindowTextW(st->openBackups, T().open);

        RecreateFonts(st);

        LayoutMain(st);
        RefreshSlots(st, false);

        if (st->gameRunning) {
            SetStatus(st, T().gameWarning, kDanger);
            EnableWindow(st->create, FALSE);
        } else {
            SetStatus(st, T().statusReady, kMuted);
        }

        SetTimer(hwnd, kTimerId, 2000, nullptr);
        SetFocus(st->list);
        return 0;
    }
    case WM_SIZE:
        if (st) {
            LayoutMain(st);
            InvalidateRect(hwnd, nullptr, FALSE);
        }
        return 0;
    case WM_DPICHANGED:
        if (st) {
            RecreateFonts(st);
            auto* suggested = reinterpret_cast<RECT*>(lParam);
            SetWindowPos(hwnd, nullptr, suggested->left, suggested->top,
                         suggested->right - suggested->left,
                         suggested->bottom - suggested->top,
                         SWP_NOZORDER | SWP_NOACTIVATE);
            LayoutMain(st);
            InvalidateRect(hwnd, nullptr, TRUE);
        }
        return 0;    case WM_GETMINMAXINFO: {
        // Let the system fill the defaults first: clearing ptMaxTrackSize
        // makes Windows clamp the window to the minimum size at creation.
        DefWindowProcW(hwnd, msg, wParam, lParam);

        auto* info = reinterpret_cast<MINMAXINFO*>(lParam);
        const cfg::Theme& theme = CurrentTheme();
        const DWORD style = static_cast<DWORD>(GetWindowLongPtrW(hwnd, GWL_STYLE));
        const DWORD exStyle = static_cast<DWORD>(GetWindowLongPtrW(hwnd, GWL_EXSTYLE));

        RECT rc{0, 0, S(hwnd, theme.windowMinWidth), S(hwnd, theme.windowMinHeight)};
        AdjustWindowRectDpi(rc, style, FALSE, exStyle, DpiOf(hwnd));

        int minWidth  = rc.right - rc.left;
        int minHeight = rc.bottom - rc.top;

        // A theme must never make the window impossible to resize back.
        MONITORINFO mi{};
        mi.cbSize = sizeof(mi);
        if (GetMonitorInfoW(MonitorFromWindow(hwnd, MONITOR_DEFAULTTONEAREST), &mi)) {
            const int areaWidth  = mi.rcWork.right - mi.rcWork.left;
            const int areaHeight = mi.rcWork.bottom - mi.rcWork.top;
            if (minWidth > areaWidth)   minWidth = areaWidth;
            if (minHeight > areaHeight) minHeight = areaHeight;
        }

        info->ptMinTrackSize.x = minWidth;
        info->ptMinTrackSize.y = minHeight;

        // A zero/invalid max would clamp the window to the minimum size.
        if (info->ptMaxTrackSize.x < info->ptMinTrackSize.x ||
            info->ptMaxTrackSize.x <= 0)
            info->ptMaxTrackSize.x = GetSystemMetrics(SM_CXMAXTRACK);
        if (info->ptMaxTrackSize.y < info->ptMinTrackSize.y ||
            info->ptMaxTrackSize.y <= 0)
            info->ptMaxTrackSize.y = GetSystemMetrics(SM_CYMAXTRACK);
        return 0;
    }
    case WM_NOTIFY: {
        LRESULT handled = 0;
        if (HandleHeaderCustomDraw(st ? st->list : nullptr, lParam, &handled))
            return handled;
        break;
    }
    case WM_ERASEBKGND:
        return 1;
    case WM_PAINT:
        if (st) PaintMain(st);
        return 0;
    case WM_CTLCOLORSTATIC:
    case WM_CTLCOLOREDIT: {
        HDC dc = reinterpret_cast<HDC>(wParam);
        SetTextColor(dc, kText);
        SetBkColor(dc, kInput);
        return reinterpret_cast<LRESULT>(InputBrush());
    }
    case WM_DRAWITEM:
        DrawOwnerButton(reinterpret_cast<const DRAWITEMSTRUCT*>(lParam));
        return TRUE;
    case WM_COMMAND:
        if (!st) break;
        switch (LOWORD(wParam)) {
        case IDC_CREATE:
            CreateBackupNow(st);
            return 0;
        case IDC_SHOW_BACKUPS:
            ShowBackups(st);
            return 0;
        case IDC_REFRESH:
            RefreshSlots(st, true);
            return 0;
        case IDC_EXIT:
            DestroyWindow(hwnd);
            return 0;
        case IDC_OPEN_SAVES:
            OpenSavesFolder(st);
            return 0;
        case IDC_OPEN_BACKUPS:
            OpenBackupsFolder(st);
            return 0;
        case IDC_LANG:
            ToggleLanguage(st);
            return 0;
        case IDC_SETTINGS:
            ShowSettingsDialog(hwnd, [st]() { ApplyThemeChange(st); });
            return 0;
        case IDC_ABOUT:
            ShowAboutDialog(hwnd);
            return 0;
        default:
            break;
        }
        return 0;
    case WM_TIMER:
        if (st && wParam == kTimerId) {
            const bool running = isGameRunning();
            if (running != st->gameRunning) {
                st->gameRunning = running;
                EnableWindow(st->create, !running);
                LayoutMain(st);
                InvalidateRect(hwnd, nullptr, TRUE);
                if (!running) RefreshSlots(st, false);
            }
        }
        return 0;
    case WM_ACTIVATE:
        if (st && LOWORD(wParam) != WA_INACTIVE && IsWindowEnabled(hwnd))
            RefreshSlots(st, false);
        return 0;
    case WM_CLOSE:
        DestroyWindow(hwnd);
        return 0;
    case WM_DESTROY:
        KillTimer(hwnd, kTimerId);
        PostQuitMessage(0);
        return 0;
    case WM_NCDESTROY:
        if (st) {
            if (st->font) DeleteObject(st->font);
            if (st->fontBold) DeleteObject(st->fontBold);
            if (st->fontTitle) DeleteObject(st->fontTitle);
            if (st->fontSmall) DeleteObject(st->fontSmall);
            SetWindowLongPtrW(hwnd, GWLP_USERDATA, 0);
            delete st;
        }
        return 0;
    default:
        break;
    }

    return DefWindowProcW(hwnd, msg, wParam, lParam);
}

bool HandleShortcut(MainState* st, const MSG& msg)
{
    if (msg.message != WM_KEYDOWN) return false;

    const bool ctrl  = (GetKeyState(VK_CONTROL) & 0x8000) != 0;
    const bool shift = (GetKeyState(VK_SHIFT) & 0x8000) != 0;

    if (msg.wParam == VK_F5) {
        RefreshSlots(st, true);
        return true;
    }
    if (ctrl && (msg.wParam == 'B' || msg.wParam == 'b')) {
        CreateBackupNow(st);
        return true;
    }
    if (ctrl && (msg.wParam == 'L' || msg.wParam == 'l')) {
        ShowBackups(st);
        return true;
    }
    if (ctrl && (msg.wParam == 'D' || msg.wParam == 'd')) {
        if (shift) OpenBackupsFolder(st);
        else OpenSavesFolder(st);
        return true;
    }
    if (msg.wParam == VK_ESCAPE) {
        return true;
    }
    return false;
}

} // namespace

int RunMainWindow(HINSTANCE instance)
{
    EnableDpiAwareness();

    INITCOMMONCONTROLSEX icc{};
    icc.dwSize = sizeof(icc);
    icc.dwICC = ICC_WIN95_CLASSES | ICC_LISTVIEW_CLASSES | ICC_STANDARD_CLASSES;
    InitCommonControlsEx(&icc);

    if (!LoadLang())
        PickLanguage(nullptr);

    // Before the first window exists: the palette and the fonts are global.
    ApplyTheme(app::LoadTheme());

    const cfg::Theme& theme = CurrentTheme();

    WNDCLASSEXW wc{};
    wc.cbSize        = sizeof(wc);
    wc.style         = CS_HREDRAW | CS_VREDRAW;
    wc.lpfnWndProc   = MainProc;
    wc.hInstance     = instance;
    wc.hCursor       = LoadCursorW(nullptr, IDC_ARROW);
    wc.hIcon         = LoadIconW(instance, MAKEINTRESOURCEW(IDI_APP));
    wc.hIconSm       = wc.hIcon;
    wc.lpszClassName = kMainClass;
    RegisterClassExW(&wc);

    auto* state = new MainState();

    HWND hwnd = CreateWindowExW(
        0, kMainClass, T().appTitle,
        WS_OVERLAPPEDWINDOW | WS_CLIPCHILDREN,
        CW_USEDEFAULT, CW_USEDEFAULT, theme.windowWidth, theme.windowHeight,
        nullptr, nullptr, instance, state);

    if (!hwnd) {
        delete state;
        return 1;
    }

    // Scale the requested theme size to the monitor DPI and centre it on the
    // work area. A freshly created window still reports 96 DPI, so the monitor
    // is asked directly; without this a 125% display clips the layout.
    {
        const int dpi = MonitorDpi(hwnd);
        RECT desired{0, 0, MulDiv(theme.windowWidth, dpi, 96),
                     MulDiv(theme.windowHeight, dpi, 96)};
        AdjustWindowRectDpi(desired, WS_OVERLAPPEDWINDOW | WS_CLIPCHILDREN,
                            FALSE, 0, dpi);

        int width  = desired.right - desired.left;
        int height = desired.bottom - desired.top;

        MONITORINFO mi{};
        mi.cbSize = sizeof(mi);
        if (GetMonitorInfoW(MonitorFromWindow(hwnd, MONITOR_DEFAULTTONEAREST), &mi)) {
            const int areaWidth  = mi.rcWork.right - mi.rcWork.left;
            const int areaHeight = mi.rcWork.bottom - mi.rcWork.top;
            if (width > areaWidth)   width = areaWidth;
            if (height > areaHeight) height = areaHeight;

            SetWindowPos(hwnd, nullptr,
                         mi.rcWork.left + (areaWidth - width) / 2,
                         mi.rcWork.top + (areaHeight - height) / 2,
                         width, height, SWP_NOZORDER | SWP_NOACTIVATE);
        }
    }

    ShowWindow(hwnd, SW_SHOW);
    UpdateWindow(hwnd);

    MSG msg{};
    while (GetMessageW(&msg, nullptr, 0, 0) > 0) {
        if (HandleShortcut(state, msg)) continue;
        TranslateMessage(&msg);
        DispatchMessageW(&msg);
    }

    return 0;
}

} // namespace ui
