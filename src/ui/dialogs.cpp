#include "dialogs.h"

#include "config.h"
#include "schedule.h"
#include "settings.h"
#include "theme.h"
#include "resource.h"
#include "update.h"
#include "update_ui.h"
#include "version.h"
#include "drbackup.h"

#include <commctrl.h>
#include <commdlg.h>
#include <shellapi.h>

#include <atomic>
#include <cwchar>
#include <memory>
#include <thread>
#include <utility>
#include <vector>

namespace ui {

namespace {

constexpr int kIdYes        = 3001;
constexpr int kIdNo         = 3002;

constexpr int kIdLangEs     = 3101;
constexpr int kIdLangEn     = 3102;

constexpr int kIdGithub     = 3201;
constexpr int kIdWebsite    = 3202;
constexpr int kIdAboutClose = 3203;

constexpr int kIdBackupList    = 3301;
constexpr int kIdBackupRestore = 3302;
constexpr int kIdBackupOpen    = 3303;
constexpr int kIdBackupDelete  = 3304;
constexpr int kIdBackupClose   = 3305;

constexpr int kIdSetTimeEdit    = 3401;
constexpr int kIdSetSaveTime    = 3402;
constexpr int kIdSetOff         = 3403;
constexpr int kIdSetThemeLoad   = 3404;
constexpr int kIdSetThemeSave   = 3405;
constexpr int kIdSetThemeDefault = 3406;
constexpr int kIdSetImport      = 3407;
constexpr int kIdSetExport      = 3408;
constexpr int kIdSetClose       = 3409;
constexpr int kIdSetCheckNow    = 3410;
constexpr int kIdSetUpdateAuto  = 3411;

const wchar_t* kConfirmClass = L"DRBC_Confirm";
const wchar_t* kLanguageClass = L"DRBC_Language";
const wchar_t* kAboutClass = L"DRBC_About";
const wchar_t* kBackupsClass = L"DRBC_Backups";
const wchar_t* kSettingsClass = L"DRBC_Settings";

// ---------------------------------------------------------------------------
// Window plumbing
// ---------------------------------------------------------------------------
HWND CreateAppWindow(const wchar_t* className,
                     const wchar_t* title,
                     int dipWidth,
                     int dipHeight,
                     HWND owner,
                     void* state,
                     WNDPROC proc)
{
    HINSTANCE instance = GetModuleHandleW(nullptr);

    WNDCLASSEXW wc{};
    wc.cbSize        = sizeof(wc);
    wc.style         = CS_HREDRAW | CS_VREDRAW;
    wc.lpfnWndProc   = proc;
    wc.hInstance     = instance;
    wc.hCursor       = LoadCursorW(nullptr, IDC_ARROW);
    wc.hIcon         = LoadIconW(instance, MAKEINTRESOURCEW(IDI_APP));
    wc.hIconSm       = wc.hIcon;
    wc.lpszClassName = className;
    RegisterClassExW(&wc);

    const DWORD style   = WS_POPUP | WS_CAPTION | WS_SYSMENU | WS_CLIPCHILDREN;
    const DWORD exStyle = WS_EX_DLGMODALFRAME | WS_EX_CONTROLPARENT;

    HWND hwnd = CreateWindowExW(exStyle, className, title, style,
                                CW_USEDEFAULT, 0, dipWidth, dipHeight,
                                owner, nullptr, instance, state);
    if (!hwnd) return nullptr;

    // The window's own DPI is only reliable once it exists, and a freshly
    // created hidden window often still reports 96. Dialogs are centred on the
    // owner, so use the owner's monitor DPI when there is one.
    const int dpi = MonitorDpi(owner ? owner : hwnd);
    RECT rc{0, 0, MulDiv(dipWidth, dpi, 96), MulDiv(dipHeight, dpi, 96)};
    AdjustWindowRectDpi(rc, style, FALSE, exStyle, dpi);
    SetWindowPos(hwnd, nullptr, 0, 0, rc.right - rc.left, rc.bottom - rc.top,
                 SWP_NOMOVE | SWP_NOZORDER | SWP_NOACTIVATE);

    EnableDarkTitleBar(hwnd);
    SetWindowTextW(hwnd, title);
    RedrawWindow(hwnd, nullptr, nullptr, RDW_FRAME | RDW_INVALIDATE);
    CenterWindowOn(hwnd, owner);
    return hwnd;
}

void RunModal(HWND dialog, HWND owner)
{
    if (owner) EnableWindow(owner, FALSE);
    ShowWindow(dialog, SW_SHOW);
    SetForegroundWindow(dialog);

    MSG msg{};
    while (IsWindow(dialog) && GetMessageW(&msg, nullptr, 0, 0) > 0) {
        if (msg.message == WM_KEYDOWN && msg.wParam == VK_ESCAPE) {
            PostMessageW(dialog, WM_CLOSE, 0, 0);
            continue;
        }
        if (!IsDialogMessageW(dialog, &msg)) {
            TranslateMessage(&msg);
            DispatchMessageW(&msg);
        }
    }

    if (owner && IsWindow(owner)) {
        EnableWindow(owner, TRUE);
        SetForegroundWindow(owner);
    }
}

void PaintDialogBase(HWND hwnd, HDC dc, const wchar_t* title,
                     const std::wstring& subtitle, HFONT titleFont,
                     HFONT smallFont, int topMargin)
{
    RECT rc{};
    GetClientRect(hwnd, &rc);

    FillRectC(dc, rc, kBg);

    const int m = S(hwnd, 22);
    RECT accent{m, topMargin, m + S(hwnd, 4), topMargin + S(hwnd, 26)};
    FillRectC(dc, accent, kAccent);

    RECT tr{m + S(hwnd, 14), topMargin - S(hwnd, 3), rc.right - m, topMargin + S(hwnd, 30)};
    DrawWrapped(dc, title, tr, titleFont, kText,
                DT_LEFT | DT_SINGLELINE | DT_VCENTER | DT_NOPREFIX);

    if (!subtitle.empty()) {
        RECT sr{m + S(hwnd, 14), topMargin + S(hwnd, 26), rc.right - m,
                topMargin + S(hwnd, 50)};
        DrawWrapped(dc, subtitle, sr, smallFont, kMuted,
                    DT_LEFT | DT_SINGLELINE | DT_TOP | DT_NOPREFIX);
    }
}

// ---------------------------------------------------------------------------
// Confirmation dialog
// ---------------------------------------------------------------------------
struct ConfirmState {
    HWND hwnd = nullptr;
    HWND yes = nullptr;
    HWND no = nullptr;
    HFONT font = nullptr;
    HFONT fontTitle = nullptr;
    std::wstring message;
    bool danger = false;
    bool result = false;
    int dpi = 96;
};

void LayoutConfirm(ConfirmState* st)
{
    RECT rc{};
    GetClientRect(st->hwnd, &rc);
    const int m = S(st->hwnd, 22);
    const int bw = S(st->hwnd, 124);
    const int bh = S(st->hwnd, 36);
    const int gap = S(st->hwnd, 10);
    const int y = rc.bottom - m - bh;

    MoveWindow(st->no, rc.right - m - bw, y, bw, bh, TRUE);
    MoveWindow(st->yes, rc.right - m - bw - bw - gap, y, bw, bh, TRUE);
}

LRESULT CALLBACK ConfirmProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
    auto* st = reinterpret_cast<ConfirmState*>(GetWindowLongPtrW(hwnd, GWLP_USERDATA));

    switch (msg) {
    case WM_NCCREATE: {
        auto* cs = reinterpret_cast<CREATESTRUCTW*>(lParam);
        auto* state = reinterpret_cast<ConfirmState*>(cs->lpCreateParams);
        state->hwnd = hwnd;
        SetWindowLongPtrW(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(state));
        return TRUE;
    }
    case WM_CREATE: {
        st->dpi = DpiOf(hwnd);
        st->font = MakeFont(10, FW_NORMAL, st->dpi);
        st->fontTitle = MakeFont(12, FW_SEMIBOLD, st->dpi);

        st->no = CreateWindowExW(0, L"BUTTON", T().cancel,
                                 WS_CHILD | WS_VISIBLE | WS_TABSTOP,
                                 0, 0, 0, 0, hwnd,
                                 reinterpret_cast<HMENU>(static_cast<INT_PTR>(kIdNo)),
                                 GetModuleHandleW(nullptr), nullptr);
        st->yes = CreateWindowExW(0, L"BUTTON", T().yes,
                                  WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_DEFPUSHBUTTON,
                                  0, 0, 0, 0, hwnd,
                                  reinterpret_cast<HMENU>(static_cast<INT_PTR>(kIdYes)),
                                  GetModuleHandleW(nullptr), nullptr);

        SendMessageW(st->no, WM_SETFONT, reinterpret_cast<WPARAM>(st->font), TRUE);
        SendMessageW(st->yes, WM_SETFONT, reinterpret_cast<WPARAM>(st->font), TRUE);
        StyleButton(st->no, false, false);
        StyleButton(st->yes, !st->danger, st->danger);

        SetFocus(st->yes);
        LayoutConfirm(st);
        return 0;
    }
    case WM_SIZE:
        if (st) LayoutConfirm(st);
        return 0;
    case WM_ERASEBKGND:
        return 1;
    case WM_PAINT: {
        if (!st) break;
        BufferedPainter paint(hwnd);
        HDC dc = paint.dc();
        const RECT& rc = paint.client();
        const int m = S(hwnd, 24);
        const int icon = S(hwnd, 38);

        RECT circle{m, m, m + icon, m + icon};
        HBRUSH brush = CreateSolidBrush(st->danger ? kWarnBg : RGB(46, 42, 34));
        HPEN pen = CreatePen(PS_SOLID, S(hwnd, 2),
                             st->danger ? kWarnLine : RGB(126, 104, 58));
        HGDIOBJ oldBrush = SelectObject(dc, brush);
        HGDIOBJ oldPen = SelectObject(dc, pen);
        Ellipse(dc, circle.left, circle.top, circle.right, circle.bottom);
        SelectObject(dc, oldBrush);
        SelectObject(dc, oldPen);
        DeleteObject(brush);
        DeleteObject(pen);

        RECT mark = circle;
        DrawWrapped(dc, st->danger ? L"!" : L"?", mark, st->fontTitle,
                    st->danger ? kDanger : RGB(226, 192, 110),
                    DT_CENTER | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX);

        RECT tr{m + icon + S(hwnd, 18), m, rc.right - m,
                rc.bottom - m - S(hwnd, 46)};
        DrawWrapped(dc, st->message, tr, st->font, kText,
                    DT_LEFT | DT_TOP | DT_WORDBREAK | DT_NOPREFIX);
        return 0;
    }
    case WM_DRAWITEM:
        DrawOwnerButton(reinterpret_cast<const DRAWITEMSTRUCT*>(lParam));
        return TRUE;
    case WM_COMMAND:
        if (!st) break;
        switch (LOWORD(wParam)) {
        case kIdYes:
        case IDOK:
            st->result = true;
            DestroyWindow(hwnd);
            return 0;
        case kIdNo:
        case IDCANCEL:
            st->result = false;
            DestroyWindow(hwnd);
            return 0;
        default:
            break;
        }
        return 0;
    case WM_CLOSE:
        DestroyWindow(hwnd);
        return 0;
    case WM_NCDESTROY:
        if (st) {
            if (st->font) DeleteObject(st->font);
            if (st->fontTitle) DeleteObject(st->fontTitle);
            st->font = nullptr;
            st->fontTitle = nullptr;
            SetWindowLongPtrW(hwnd, GWLP_USERDATA, 0);
        }
        return 0;
    default:
        break;
    }

    return DefWindowProcW(hwnd, msg, wParam, lParam);
}

// ---------------------------------------------------------------------------
// Language picker
// ---------------------------------------------------------------------------
struct LanguageState {
    HWND hwnd = nullptr;
    HWND spanish = nullptr;
    HWND english = nullptr;
    HFONT font = nullptr;
    HFONT fontTitle = nullptr;
    HFONT fontSmall = nullptr;
    Lang result = Lang::ES;
    int dpi = 96;
};

void LayoutLanguage(LanguageState* st)
{
    RECT rc{};
    GetClientRect(st->hwnd, &rc);
    const int bw = S(st->hwnd, 168);
    const int bh = S(st->hwnd, 44);
    const int gap = S(st->hwnd, 14);
    const int total = bw * 2 + gap;
    const int x = (rc.right - total) / 2;
    const int y = rc.bottom - S(st->hwnd, 26) - bh;

    MoveWindow(st->spanish, x, y, bw, bh, TRUE);
    MoveWindow(st->english, x + bw + gap, y, bw, bh, TRUE);
}

LRESULT CALLBACK LanguageProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
    auto* st = reinterpret_cast<LanguageState*>(GetWindowLongPtrW(hwnd, GWLP_USERDATA));

    switch (msg) {
    case WM_NCCREATE: {
        auto* cs = reinterpret_cast<CREATESTRUCTW*>(lParam);
        auto* state = reinterpret_cast<LanguageState*>(cs->lpCreateParams);
        state->hwnd = hwnd;
        SetWindowLongPtrW(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(state));
        return TRUE;
    }
    case WM_CREATE: {
        st->dpi = DpiOf(hwnd);
        st->font = MakeFont(11, FW_NORMAL, st->dpi);
        st->fontTitle = MakeFont(15, FW_SEMIBOLD, st->dpi);
        st->fontSmall = MakeFont(9, FW_NORMAL, st->dpi);

        st->spanish = CreateWindowExW(0, L"BUTTON", T().btnSpanish,
                                      WS_CHILD | WS_VISIBLE | WS_TABSTOP,
                                      0, 0, 0, 0, hwnd,
                                      reinterpret_cast<HMENU>(static_cast<INT_PTR>(kIdLangEs)),
                                      GetModuleHandleW(nullptr), nullptr);
        st->english = CreateWindowExW(0, L"BUTTON", T().btnEnglish,
                                      WS_CHILD | WS_VISIBLE | WS_TABSTOP,
                                      0, 0, 0, 0, hwnd,
                                      reinterpret_cast<HMENU>(static_cast<INT_PTR>(kIdLangEn)),
                                      GetModuleHandleW(nullptr), nullptr);

        SendMessageW(st->spanish, WM_SETFONT, reinterpret_cast<WPARAM>(st->font), TRUE);
        SendMessageW(st->english, WM_SETFONT, reinterpret_cast<WPARAM>(st->font), TRUE);
        StyleButton(st->spanish, GetLang() == Lang::ES, false);
        StyleButton(st->english, GetLang() == Lang::EN, false);

        SetFocus(st->spanish);
        LayoutLanguage(st);
        return 0;
    }
    case WM_SIZE:
        if (st) LayoutLanguage(st);
        return 0;
    case WM_ERASEBKGND:
        return 1;
    case WM_PAINT: {
        if (!st) break;
        BufferedPainter paint(hwnd);
        HDC dc = paint.dc();
        const RECT& rc = paint.client();

        PaintDialogBase(hwnd, dc, T().langTitle, L"", st->fontTitle, st->fontSmall, S(hwnd, 24));

        RECT tr{S(hwnd, 26), S(hwnd, 70), rc.right - S(hwnd, 26), S(hwnd, 170)};
        DrawWrapped(dc, T().langPrompt, tr, st->font, kMuted,
                    DT_LEFT | DT_TOP | DT_WORDBREAK | DT_NOPREFIX);
        return 0;
    }
    case WM_DRAWITEM:
        DrawOwnerButton(reinterpret_cast<const DRAWITEMSTRUCT*>(lParam));
        return TRUE;
    case WM_COMMAND:
        if (!st) break;
        switch (LOWORD(wParam)) {
        case kIdLangEs:
        case kIdLangEn: {
            st->result = (LOWORD(wParam) == kIdLangEn) ? Lang::EN : Lang::ES;
            SetLang(st->result);
            SaveLang(st->result);
            DestroyWindow(hwnd);
            return 0;
        }
        default:
            break;
        }
        return 0;
    case WM_CLOSE:
        DestroyWindow(hwnd);
        return 0;
    case WM_NCDESTROY:
        if (st) {
            if (st->font) DeleteObject(st->font);
            if (st->fontTitle) DeleteObject(st->fontTitle);
            if (st->fontSmall) DeleteObject(st->fontSmall);
            st->font = st->fontTitle = st->fontSmall = nullptr;
            SetWindowLongPtrW(hwnd, GWLP_USERDATA, 0);
        }
        return 0;
    default:
        break;
    }

    return DefWindowProcW(hwnd, msg, wParam, lParam);
}

// ---------------------------------------------------------------------------
// Settings dialog (schedule, theme, config)
// ---------------------------------------------------------------------------
struct SettingsState {
    HWND hwnd = nullptr;
    HWND timeEdit = nullptr;
    HWND saveTime = nullptr;
    HWND off = nullptr;
    HWND themeLoad = nullptr;
    HWND themeSave = nullptr;
    HWND themeDefault = nullptr;
    HWND importConfig = nullptr;
    HWND exportConfig = nullptr;
    HWND checkNow = nullptr;
    HWND autoButton = nullptr;
    HWND close = nullptr;

    HFONT font = nullptr;
    HFONT fontTitle = nullptr;
    HFONT fontSmall = nullptr;

    std::wstring status;
    COLORREF statusColor = kMuted;

    bool updateAuto = true;
    bool checking = false;
    std::shared_ptr<UpdateCheck> check;
    std::wstring updateStatus;
    COLORREF updateStatusColor = kMuted;
    int updateRow = 0;          // filled by LayoutSettings, used when painting

    std::function<void()> onApplied;
};

void SetSettingsStatus(SettingsState* st, const std::wstring& text, COLORREF color)
{
    st->status = text;
    st->statusColor = color;
    if (st->hwnd) InvalidateRect(st->hwnd, nullptr, FALSE);
}

void RebuildSettingsFonts(SettingsState* st)
{
    if (st->font) { DeleteObject(st->font); st->font = nullptr; }
    if (st->fontTitle) { DeleteObject(st->fontTitle); st->fontTitle = nullptr; }
    if (st->fontSmall) { DeleteObject(st->fontSmall); st->fontSmall = nullptr; }

    const int dpi = DpiOf(st->hwnd);
    st->font      = MakeFont(10, FW_NORMAL, dpi);
    st->fontTitle = MakeFont(15, FW_SEMIBOLD, dpi);
    st->fontSmall = MakeFont(9, FW_NORMAL, dpi);

    HWND controls[] = {st->timeEdit, st->saveTime, st->off, st->themeLoad,
                       st->themeSave, st->themeDefault, st->importConfig,
                       st->exportConfig, st->checkNow, st->autoButton, st->close};
    for (HWND control : controls) {
        if (control) SendMessageW(control, WM_SETFONT,
                                  reinterpret_cast<WPARAM>(st->font), TRUE);
    }
}

void LayoutSettings(SettingsState* st)
{
    RECT rc{};
    GetClientRect(st->hwnd, &rc);

    const int m   = S(st->hwnd, 24);
    const int bh  = S(st->hwnd, 34);
    const int gap = S(st->hwnd, 10);

    const int editWidth  = S(st->hwnd, 92);
    const int wideButton = S(st->hwnd, 136);

    // Row 1: time + save + turn off.
    const int row1 = S(st->hwnd, 144);
    MoveWindow(st->timeEdit, m, row1, editWidth, bh, TRUE);
    MoveWindow(st->saveTime, m + editWidth + gap, row1, wideButton, bh, TRUE);
    MoveWindow(st->off, m + editWidth + gap * 2 + wideButton, row1, wideButton, bh, TRUE);

    // Row 2: theme buttons.
    const int row2 = S(st->hwnd, 258);
    const int themeButton = S(st->hwnd, 132);
    MoveWindow(st->themeLoad, m, row2, themeButton, bh, TRUE);
    MoveWindow(st->themeSave, m + themeButton + gap, row2, themeButton, bh, TRUE);
    MoveWindow(st->themeDefault, m + (themeButton + gap) * 2, row2, themeButton, bh, TRUE);

    // Row 3: config buttons.
    const int row3 = S(st->hwnd, 340);
    MoveWindow(st->importConfig, m, row3, themeButton, bh, TRUE);
    MoveWindow(st->exportConfig, m + themeButton + gap, row3, themeButton, bh, TRUE);

    // Row 4: updates, anchored above the Close button so the section always
    // stays inside the window.
    const int closeTop = rc.bottom - m - bh;
    const int row4 = closeTop - gap - bh;
    st->updateRow = row4;

    const int autoWidth = S(st->hwnd, 224);
    MoveWindow(st->checkNow, m, row4, themeButton, bh, TRUE);
    MoveWindow(st->autoButton, m + themeButton + gap, row4, autoWidth, bh, TRUE);

    const int closeWidth = S(st->hwnd, 130);
    MoveWindow(st->close, rc.right - m - closeWidth, closeTop,
               closeWidth, bh, TRUE);
}

void RefreshScheduleStatus(SettingsState* st)
{
    const sched::Status status = sched::query();

    if (status.state == sched::State::Scheduled) {
        const std::wstring time = status.time.empty()
                                      ? app::Load().scheduleTime
                                      : Widen(status.time);
        SetSettingsStatus(st, FormatText(T().scheduleOn, time.c_str()), kSuccess);
        if (status.time.size() == 5 && st->timeEdit)
            SetWindowTextW(st->timeEdit, time.c_str());
    } else if (status.state == sched::State::Error) {
        SetSettingsStatus(st, Widen(status.error), kDanger);
    } else {
        SetSettingsStatus(st, T().scheduleOff, kMuted);
    }
}

void ApplyThemeFromSettings(SettingsState* st)
{
    ApplyTheme(app::LoadTheme());
    RebuildSettingsFonts(st);
    LayoutSettings(st);
    if (st->onApplied) st->onApplied();
    InvalidateRect(st->hwnd, nullptr, TRUE);
    RedrawWindow(st->hwnd, nullptr, nullptr, RDW_FRAME | RDW_INVALIDATE);
}

// Shared open/save dialog for the two file types. `path` is the initial name
// for a save dialog and receives the picked file afterwards.
bool PickSettingsFile(HWND owner, bool save, const wchar_t* label,
                      const wchar_t* extension, std::wstring& path)
{
    wchar_t file[MAX_PATH * 2] = {};
    lstrcpynW(file, path.c_str(), static_cast<int>(std::size(file)));

    std::wstring filter;
    filter += label;
    filter.push_back(L'\0');
    filter += L"*.";
    filter += extension;
    filter.push_back(L'\0');
    filter += T().filterAll;
    filter.push_back(L'\0');
    filter += L"*.*";
    filter.push_back(L'\0');
    filter.push_back(L'\0');

    OPENFILENAMEW ofn{};
    ofn.lStructSize = sizeof(ofn);
    ofn.hwndOwner   = owner;
    ofn.lpstrFilter = filter.c_str();
    ofn.lpstrFile   = file;
    ofn.nMaxFile    = static_cast<DWORD>(std::size(file));
    ofn.lpstrDefExt = extension;
    ofn.Flags = OFN_EXPLORER | OFN_NOCHANGEDIR |
                (save ? OFN_OVERWRITEPROMPT
                      : OFN_FILEMUSTEXIST | OFN_PATHMUSTEXIST);

    const BOOL picked = save ? GetSaveFileNameW(&ofn) : GetOpenFileNameW(&ofn);
    if (!picked) return false;

    path = file;
    return true;
}

void HandleSaveTime(SettingsState* st)
{
    wchar_t buffer[16] = {};
    GetWindowTextW(st->timeEdit, buffer, 15);

    int hour = 0;
    int minute = 0;
    if (!cfg::parseClock(cfg::narrow(buffer), hour, minute)) {
        SetSettingsStatus(st, FormatText(T().scheduleError, L"HH:MM"), kDanger);
        return;
    }

    const std::string time = cfg::clockToText(hour, minute);
    std::string error;
    if (!sched::enable(time, error)) {
        SetSettingsStatus(st, FormatText(T().scheduleError, Widen(error).c_str()), kDanger);
        return;
    }

    app::SaveSchedule(true, Widen(time));
    RefreshScheduleStatus(st);
    SetSettingsStatus(st, FormatText(T().scheduleOn, Widen(time).c_str()), kSuccess);
}

void HandleTurnOff(SettingsState* st)
{
    std::string error;
    if (!sched::disable(error)) {
        SetSettingsStatus(st, FormatText(T().scheduleError, Widen(error).c_str()), kDanger);
        return;
    }
    app::SaveSchedule(false, app::Load().scheduleTime);
    SetSettingsStatus(st, T().scheduleOff, kMuted);
}

void HandleLoadTheme(SettingsState* st)
{
    std::wstring file;
    if (!PickSettingsFile(st->hwnd, false, T().filterTheme, L"bctheme", file))
        return;

    cfg::Theme theme;
    std::string error;
    if (!cfg::loadThemeFile(file, theme, error)) {
        SetSettingsStatus(st, Widen(error), kDanger);
        return;
    }

    app::SaveThemePath(file);
    ApplyThemeFromSettings(st);
    SetSettingsStatus(st, T().themeSaved, kSuccess);
}

void HandleSaveTheme(SettingsState* st)
{
    cfg::Theme theme = CurrentTheme();

    const std::wstring suggested = Widen(theme.name);
    std::wstring file = (suggested.empty() ? std::wstring(L"theme") : suggested) +
                        L".bctheme";
    if (!PickSettingsFile(st->hwnd, true, T().filterTheme, L"bctheme", file))
        return;

    theme.name = cfg::narrow(file.substr(file.find_last_of(L'\\') + 1));
    const size_t dot = theme.name.find_last_of('.');
    if (dot != std::string::npos) theme.name.erase(dot);

    std::string error;
    if (!cfg::saveThemeFile(file, theme, error)) {
        SetSettingsStatus(st, Widen(error), kDanger);
        return;
    }

    app::SaveThemePath(file);
    ApplyThemeFromSettings(st);
    SetSettingsStatus(st, T().themeSaved, kSuccess);
}

void HandleResetTheme(SettingsState* st)
{
    app::SaveThemePath(L"");
    ApplyThemeFromSettings(st);
    SetSettingsStatus(st, T().themeResetDone, kSuccess);
}

void HandleImportConfig(SettingsState* st)
{
    std::wstring file;
    if (!PickSettingsFile(st->hwnd, false, T().filterConfig, L"bcconfig", file))
        return;

    std::wstring message;
    if (!app::ImportConfig(file, message)) {
        SetSettingsStatus(st, message, kDanger);
        return;
    }

    ApplyThemeFromSettings(st);
    RefreshScheduleStatus(st);
    SetSettingsStatus(st, T().configImported, kSuccess);
}

void HandleExportConfig(SettingsState* st)
{
    std::wstring file = L"DRBackupCreator.bcconfig";
    if (!PickSettingsFile(st->hwnd, true, T().filterConfig, L"bcconfig", file))
        return;

    std::wstring message;
    if (!app::ExportConfig(file, message)) {
        SetSettingsStatus(st, message, kDanger);
        return;
    }
    SetSettingsStatus(st, T().configImported, kSuccess);
}

void UpdateAutoLabel(SettingsState* st)
{
    if (st->autoButton)
        SetWindowTextW(st->autoButton,
                       st->updateAuto ? T().updateAutoOn : T().updateAutoOff);
}

void SetUpdateStatus(SettingsState* st, const std::wstring& text, COLORREF color)
{
    st->updateStatus = text;
    st->updateStatusColor = color;
    if (st->hwnd) InvalidateRect(st->hwnd, nullptr, FALSE);
}

void StartCheckFromSettings(SettingsState* st)
{
    if (st->checking) return;

    st->checking = true;
    if (st->checkNow) EnableWindow(st->checkNow, FALSE);
    SetUpdateStatus(st, T().updateChecking, kMuted);
    st->check = StartUpdateCheck(st->hwnd);
}

void HandleUpdateCheckDone(SettingsState* st)
{
    st->checking = false;
    if (st->checkNow) EnableWindow(st->checkNow, TRUE);

    if (!st->check) return;

    bool ok = false;
    upd::ReleaseInfo info;
    std::string error;
    {
        std::lock_guard<std::mutex> lock(st->check->mutex);
        ok = st->check->ok;
        info = st->check->info;
        error = st->check->error;
    }
    st->check.reset();

    if (!ok) {
        SetUpdateStatus(st, FormatText(T().updateFailed, Widen(error).c_str()), kDanger);
        return;
    }

    if (!upd::IsNewer(info.version, DRBC_VERSION_A)) {
        SetUpdateStatus(st, FormatText(T().updateUpToDate, DRBC_VERSION_W), kSuccess);
        return;
    }

    SetUpdateStatus(st, FormatText(T().updateSubtitle, DRBC_VERSION_W,
                                   cfg::widen(info.version).c_str()), kSuccess);

    // The dialogs run their own modal loops, so this works from inside the
    // settings loop too.
    if (ShowUpdateDialog(st->hwnd, info, true) &&
        RunDownloadAndInstall(st->hwnd, info)) {
        // The installer needs the app gone to replace its files.
        HWND main = GetWindow(st->hwnd, GW_OWNER);
        DestroyWindow(st->hwnd);
        if (main) PostMessageW(main, WM_CLOSE, 0, 0);
    }
}

LRESULT CALLBACK SettingsProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
    auto* st = reinterpret_cast<SettingsState*>(GetWindowLongPtrW(hwnd, GWLP_USERDATA));

    switch (msg) {
    case WM_NCCREATE: {
        auto* cs = reinterpret_cast<CREATESTRUCTW*>(lParam);
        auto* state = reinterpret_cast<SettingsState*>(cs->lpCreateParams);
        state->hwnd = hwnd;
        SetWindowLongPtrW(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(state));
        return TRUE;
    }
    case WM_CREATE: {
        RebuildSettingsFonts(st);

        const app::Settings settings = app::Load();

        auto make = [&](const wchar_t* text, int id) {
            return CreateWindowExW(0, L"BUTTON", text,
                                   WS_CHILD | WS_VISIBLE | WS_TABSTOP,
                                   0, 0, 0, 0, hwnd,
                                   reinterpret_cast<HMENU>(static_cast<INT_PTR>(id)),
                                   GetModuleHandleW(nullptr), nullptr);
        };

        st->timeEdit = CreateWindowExW(WS_EX_CLIENTEDGE, L"EDIT", L"21:00",
                                       WS_CHILD | WS_VISIBLE | WS_TABSTOP |
                                           ES_AUTOHSCROLL | ES_CENTER,
                                       0, 0, 0, 0, hwnd,
                                       reinterpret_cast<HMENU>(static_cast<INT_PTR>(kIdSetTimeEdit)),
                                       GetModuleHandleW(nullptr), nullptr);
        SendMessageW(st->timeEdit, EM_SETLIMITTEXT, 5, 0);
        SendMessageW(st->timeEdit, EM_SETMARGINS, EC_LEFTMARGIN | EC_RIGHTMARGIN,
                     MAKELONG(S(hwnd, 6), S(hwnd, 6)));
        ApplyDarkEdit(st->timeEdit);

        st->saveTime = make(T().btnScheduleSave, kIdSetSaveTime);
        st->off = make(T().btnScheduleOff, kIdSetOff);
        st->themeLoad = make(T().btnThemeLoad, kIdSetThemeLoad);
        st->themeSave = make(T().btnThemeSave, kIdSetThemeSave);
        st->themeDefault = make(T().btnThemeReset, kIdSetThemeDefault);
        st->importConfig = make(T().btnConfigImport, kIdSetImport);
        st->exportConfig = make(T().btnConfigExport, kIdSetExport);
        st->checkNow = make(T().btnCheckNow, kIdSetCheckNow);
        st->autoButton = make(L"", kIdSetUpdateAuto);
        st->close = make(T().btnClose, kIdSetClose);

        st->updateAuto = settings.updateAuto;
        UpdateAutoLabel(st);

        StyleButton(st->saveTime, true, false);
        for (HWND button : {st->off, st->themeLoad, st->themeSave, st->themeDefault,
                            st->importConfig, st->exportConfig, st->checkNow,
                            st->autoButton, st->close}) {
            StyleButton(button, false, false);
        }

        RebuildSettingsFonts(st);
        LayoutSettings(st);

        SetWindowTextW(st->timeEdit, settings.scheduleTime.c_str());

        RefreshScheduleStatus(st);
        SetFocus(st->timeEdit);
        return 0;
    }
    case WM_SIZE:
        if (st) LayoutSettings(st);
        return 0;
    case WM_ERASEBKGND:
        return 1;
    case WM_PAINT: {
        if (!st) break;
        BufferedPainter paint(hwnd);
        HDC dc = paint.dc();
        const RECT& rc = paint.client();
        const int m = S(hwnd, 24);

        PaintDialogBase(hwnd, dc, T().settingsTitle, T().settingsSubtitle,
                        st->fontTitle, st->fontSmall, S(hwnd, 22));

        auto section = [&](const wchar_t* text, int y) {
            RECT r{m, S(hwnd, y), rc.right - m, S(hwnd, y + 22)};
            DrawWrapped(dc, text, r, st->font, kText,
                        DT_LEFT | DT_SINGLELINE | DT_VCENTER | DT_NOPREFIX);
        };
        auto line = [&](const std::wstring& text, int y, COLORREF color) {
            RECT r{m, S(hwnd, y), rc.right - m, S(hwnd, y + 22)};
            DrawWrapped(dc, text, r, st->fontSmall, color,
                        DT_LEFT | DT_SINGLELINE | DT_VCENTER | DT_NOPREFIX);
        };

        section(T().scheduleSection, 100);
        line(T().schedulePrompt, 120, kMuted);
        line(st->status, 186, st->statusColor);

        section(T().themeSection, 212);
        line(FormatText(T().themeCurrent, Widen(CurrentTheme().name).c_str()),
             232, kMuted);
        line(T().restartHint, 298, kMuted);

        section(T().configSection, 322);

        // Updates sits right above its buttons, wherever they ended up.
        if (st->updateRow > 0) {
            RECT sr{m, st->updateRow - S(hwnd, 48), rc.right - m,
                    st->updateRow - S(hwnd, 26)};
            DrawWrapped(dc, T().updateSection, sr, st->font, kText,
                        DT_LEFT | DT_SINGLELINE | DT_VCENTER | DT_NOPREFIX);

            RECT ur{m, st->updateRow - S(hwnd, 26), rc.right - m,
                    st->updateRow - S(hwnd, 4)};
            DrawWrapped(dc, st->updateStatus, ur, st->fontSmall,
                        st->updateStatusColor,
                        DT_LEFT | DT_SINGLELINE | DT_VCENTER | DT_NOPREFIX);
        }

        return 0;
    }
    case WM_COMMAND:
        if (!st) break;
        switch (LOWORD(wParam)) {
        case kIdSetSaveTime: HandleSaveTime(st); return 0;
        case kIdSetOff:      HandleTurnOff(st); return 0;
        case kIdSetThemeLoad: HandleLoadTheme(st); return 0;
        case kIdSetThemeSave: HandleSaveTheme(st); return 0;
        case kIdSetThemeDefault: HandleResetTheme(st); return 0;
        case kIdSetImport: HandleImportConfig(st); return 0;
        case kIdSetExport: HandleExportConfig(st); return 0;
        case kIdSetCheckNow: StartCheckFromSettings(st); return 0;
        case kIdSetUpdateAuto:
            st->updateAuto = !st->updateAuto;
            app::SaveUpdateAuto(st->updateAuto);
            UpdateAutoLabel(st);
            InvalidateRect(st->autoButton, nullptr, TRUE);
            return 0;
        case kIdSetClose:
            DestroyWindow(hwnd);
            return 0;
        default:
            break;
        }
        return 0;
    case ui::kUpdateCheckDone:
        if (st) HandleUpdateCheckDone(st);
        return 0;
    case WM_DRAWITEM:
        DrawOwnerButton(reinterpret_cast<const DRAWITEMSTRUCT*>(lParam));
        return TRUE;
    case WM_CTLCOLOREDIT: {
        HDC dc = reinterpret_cast<HDC>(wParam);
        SetTextColor(dc, kText);
        SetBkColor(dc, kInput);
        return reinterpret_cast<LRESULT>(InputBrush());
    }
    case WM_CLOSE:
        DestroyWindow(hwnd);
        return 0;
    case WM_NCDESTROY:
        if (st) {
            if (st->font) DeleteObject(st->font);
            if (st->fontTitle) DeleteObject(st->fontTitle);
            if (st->fontSmall) DeleteObject(st->fontSmall);
            st->font = st->fontTitle = st->fontSmall = nullptr;
            SetWindowLongPtrW(hwnd, GWLP_USERDATA, 0);
        }
        return 0;
    default:
        break;
    }

    return DefWindowProcW(hwnd, msg, wParam, lParam);
}

// ---------------------------------------------------------------------------
// About dialog
// ---------------------------------------------------------------------------
struct AboutState {
    HWND hwnd = nullptr;
    HWND github = nullptr;
    HWND website = nullptr;
    HWND close = nullptr;
    HFONT font = nullptr;
    HFONT fontTitle = nullptr;
    HFONT fontHuge = nullptr;
    int dpi = 96;
};

void LayoutAbout(AboutState* st)
{
    RECT rc{};
    GetClientRect(st->hwnd, &rc);
    const int m = S(st->hwnd, 22);
    const int bw = S(st->hwnd, 116);
    const int bh = S(st->hwnd, 36);
    const int gap = S(st->hwnd, 10);
    const int y = rc.bottom - m - bh;

    MoveWindow(st->github, m, y, bw, bh, TRUE);
    MoveWindow(st->website, m + bw + gap, y, bw + S(st->hwnd, 12), bh, TRUE);
    MoveWindow(st->close, rc.right - m - bw, y, bw, bh, TRUE);
}

LRESULT CALLBACK AboutProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
    auto* st = reinterpret_cast<AboutState*>(GetWindowLongPtrW(hwnd, GWLP_USERDATA));

    switch (msg) {
    case WM_NCCREATE: {
        auto* cs = reinterpret_cast<CREATESTRUCTW*>(lParam);
        auto* state = reinterpret_cast<AboutState*>(cs->lpCreateParams);
        state->hwnd = hwnd;
        SetWindowLongPtrW(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(state));
        return TRUE;
    }
    case WM_CREATE: {
        st->dpi = DpiOf(hwnd);
        st->font = MakeFont(10, FW_NORMAL, st->dpi);
        st->fontTitle = MakeFont(12, FW_SEMIBOLD, st->dpi);
        st->fontHuge = MakeFont(22, FW_SEMIBOLD, st->dpi);

        st->github = CreateWindowExW(0, L"BUTTON", T().btnGithub,
                                     WS_CHILD | WS_VISIBLE | WS_TABSTOP,
                                     0, 0, 0, 0, hwnd,
                                     reinterpret_cast<HMENU>(static_cast<INT_PTR>(kIdGithub)),
                                     GetModuleHandleW(nullptr), nullptr);
        st->website = CreateWindowExW(0, L"BUTTON", T().btnWebsite,
                                      WS_CHILD | WS_VISIBLE | WS_TABSTOP,
                                      0, 0, 0, 0, hwnd,
                                      reinterpret_cast<HMENU>(static_cast<INT_PTR>(kIdWebsite)),
                                      GetModuleHandleW(nullptr), nullptr);
        st->close = CreateWindowExW(0, L"BUTTON", T().btnClose,
                                    WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_DEFPUSHBUTTON,
                                    0, 0, 0, 0, hwnd,
                                    reinterpret_cast<HMENU>(static_cast<INT_PTR>(kIdAboutClose)),
                                    GetModuleHandleW(nullptr), nullptr);

        SendMessageW(st->github, WM_SETFONT, reinterpret_cast<WPARAM>(st->font), TRUE);
        SendMessageW(st->website, WM_SETFONT, reinterpret_cast<WPARAM>(st->font), TRUE);
        SendMessageW(st->close, WM_SETFONT, reinterpret_cast<WPARAM>(st->font), TRUE);
        StyleButton(st->github, false, false);
        StyleButton(st->website, false, false);
        StyleButton(st->close, true, false);

        SetFocus(st->close);
        LayoutAbout(st);
        return 0;
    }
    case WM_SIZE:
        if (st) LayoutAbout(st);
        return 0;
    case WM_ERASEBKGND:
        return 1;
    case WM_PAINT: {
        if (!st) break;
        BufferedPainter paint(hwnd);
        HDC dc = paint.dc();
        const RECT& rc = paint.client();
        const int m = S(hwnd, 24);

        RECT accent{m, S(hwnd, 20), m + S(hwnd, 5), S(hwnd, 52)};
        FillRectC(dc, accent, kAccent);

        RECT tr{m + S(hwnd, 16), S(hwnd, 16), rc.right - m, S(hwnd, 56)};
        DrawWrapped(dc, L"DRBackupCreator", tr, st->fontHuge, kText,
                    DT_LEFT | DT_SINGLELINE | DT_TOP | DT_NOPREFIX);

        RECT vr{m + S(hwnd, 16), S(hwnd, 62), rc.right - m, S(hwnd, 84)};
        DrawWrapped(dc, L"v" DRBC_VERSION_W L" · GPL-3.0", vr, st->font, kMuted,
                    DT_LEFT | DT_SINGLELINE | DT_TOP | DT_NOPREFIX);

        std::wstring body = FormatText(T().aboutBody, DRBC_VERSION_W);
        RECT br{m, S(hwnd, 100), rc.right - m, rc.bottom - S(hwnd, 76)};
        DrawWrapped(dc, body, br, st->font, kText,
                    DT_LEFT | DT_TOP | DT_WORDBREAK | DT_NOPREFIX);
        return 0;
    }
    case WM_DRAWITEM:
        DrawOwnerButton(reinterpret_cast<const DRAWITEMSTRUCT*>(lParam));
        return TRUE;
    case WM_COMMAND:
        if (!st) break;
        switch (LOWORD(wParam)) {
        case kIdGithub:
            ShellExecuteW(nullptr, L"open",
                          L"https://github.com/deltahelpers/DRBackupCreator",
                          nullptr, nullptr, SW_SHOWNORMAL);
            return 0;
        case kIdWebsite:
            ShellExecuteW(nullptr, L"open", L"https://deltahelpers.github.io",
                          nullptr, nullptr, SW_SHOWNORMAL);
            return 0;
        case kIdAboutClose:
        case IDOK:
        case IDCANCEL:
            DestroyWindow(hwnd);
            return 0;
        default:
            break;
        }
        return 0;
    case WM_CLOSE:
        DestroyWindow(hwnd);
        return 0;
    case WM_NCDESTROY:
        if (st) {
            if (st->font) DeleteObject(st->font);
            if (st->fontTitle) DeleteObject(st->fontTitle);
            if (st->fontHuge) DeleteObject(st->fontHuge);
            st->font = st->fontTitle = st->fontHuge = nullptr;
            SetWindowLongPtrW(hwnd, GWLP_USERDATA, 0);
        }
        return 0;
    default:
        break;
    }

    return DefWindowProcW(hwnd, msg, wParam, lParam);
}

// ---------------------------------------------------------------------------
// Backups browser
// ---------------------------------------------------------------------------
struct BackupsState {
    HWND hwnd = nullptr;
    HWND list = nullptr;
    HWND restore = nullptr;
    HWND open = nullptr;
    HWND remove = nullptr;
    HWND close = nullptr;
    HFONT font = nullptr;
    HFONT fontTitle = nullptr;
    HFONT fontSmall = nullptr;
    std::vector<BackupInfo> backups;
    fs::path saveDir;
    std::function<void()> onRestored;
    std::wstring status;
    COLORREF statusColor = kMuted;
    int dpi = 96;
};

bool RecyclePath(const fs::path& path)
{
    std::wstring from = path.wstring();
    from.push_back(L'\0');
    from.push_back(L'\0');

    SHFILEOPSTRUCTW op{};
    op.wFunc  = FO_DELETE;
    op.pFrom  = from.c_str();
    op.fFlags = FOF_ALLOWUNDO | FOF_NOCONFIRMATION | FOF_SILENT | FOF_NOERRORUI;

    const int result = SHFileOperationW(&op);
    return result == 0 && !op.fAnyOperationsAborted;
}

void LayoutBackups(BackupsState* st)
{
    RECT rc{};
    GetClientRect(st->hwnd, &rc);

    const int m = S(st->hwnd, 20);
    const int bw = S(st->hwnd, 136);
    const int bh = S(st->hwnd, 36);
    const int gap = S(st->hwnd, 10);

    const int buttonY = rc.bottom - m - bh;
    const int statusY = buttonY - S(st->hwnd, 30);
    const int listTop = S(st->hwnd, 78);
    const int listBottom = statusY - S(st->hwnd, 4);
    const int listWidth = rc.right - m * 2;

    MoveWindow(st->list, m, listTop, listWidth, listBottom - listTop, TRUE);

    MoveWindow(st->restore, m, buttonY, bw, bh, TRUE);
    MoveWindow(st->open, m + bw + gap, buttonY, bw + S(st->hwnd, 24), bh, TRUE);
    MoveWindow(st->remove, m + bw * 2 + gap * 2 + S(st->hwnd, 24), buttonY,
               S(st->hwnd, 116), bh, TRUE);
    MoveWindow(st->close, rc.right - m - S(st->hwnd, 110), buttonY,
               S(st->hwnd, 110), bh, TRUE);

    if (listWidth > 0) {
        int used = S(st->hwnd, 190) + S(st->hwnd, 70) + S(st->hwnd, 110);
        int folder = listWidth - used - S(st->hwnd, 28);
        if (folder < S(st->hwnd, 140)) folder = S(st->hwnd, 140);
        ListView_SetColumnWidth(st->list, 0, S(st->hwnd, 190));
        ListView_SetColumnWidth(st->list, 1, S(st->hwnd, 70));
        ListView_SetColumnWidth(st->list, 2, S(st->hwnd, 110));
        ListView_SetColumnWidth(st->list, 3, folder);
    }
}

void ReloadBackups(BackupsState* st)
{
    st->backups = listAllBackups();

    SendMessageW(st->list, WM_SETREDRAW, FALSE, 0);
    ListView_DeleteAllItems(st->list);

    for (size_t i = 0; i < st->backups.size(); ++i) {
        const BackupInfo& b = st->backups[i];

        std::wstring date   = TimestampText(b.timestamp);
        std::wstring count  = std::to_wstring(b.slotCount);
        std::wstring size   = SizeText(b.totalSize);
        std::wstring folder = b.path.filename().wstring();

        LVITEMW item{};
        item.mask  = LVIF_TEXT;
        item.iItem = static_cast<int>(i);
        item.pszText = date.data();
        int row = ListView_InsertItem(st->list, &item);

        ListView_SetItemText(st->list, row, 1, count.data());
        ListView_SetItemText(st->list, row, 2, size.data());
        ListView_SetItemText(st->list, row, 3, folder.data());
    }

    SendMessageW(st->list, WM_SETREDRAW, TRUE, 0);
    InvalidateRect(st->list, nullptr, TRUE);

    ShowWindow(st->list, st->backups.empty() ? SW_HIDE : SW_SHOW);

    InvalidateRect(st->hwnd, nullptr, FALSE);
}

void SetBackupStatus(BackupsState* st, const std::wstring& text, COLORREF color)
{
    st->status = text;
    st->statusColor = color;
    InvalidateRect(st->hwnd, nullptr, FALSE);
}

int SelectedBackup(BackupsState* st)
{
    return ListView_GetNextItem(st->list, -1, LVNI_SELECTED);
}

void RestoreSelected(BackupsState* st)
{
    const int selection = SelectedBackup(st);
    if (selection < 0) {
        SetBackupStatus(st, T().selectBackup, kDanger);
        return;
    }

    if (st->saveDir.empty()) {
        SetBackupStatus(st, T().noSaveDir, kDanger);
        return;
    }

    const BackupInfo& backup = st->backups[static_cast<size_t>(selection)];
    std::wstring date = TimestampText(backup.timestamp);

    if (!ConfirmDialog(st->hwnd, T().restoreTitle,
                       FormatText(T().restoreText, date.c_str()),
                       T().btnRestore, false))
        return;

    int restored = 0;
    if (restoreBackup(backup.path, st->saveDir, restored)) {
        SetBackupStatus(st, FormatText(T().restoreOk, restored), kSuccess);
        if (st->onRestored) st->onRestored();
    } else {
        SetBackupStatus(st, T().restoreErr, kDanger);
    }
}

void DeleteSelected(BackupsState* st)
{
    const int selection = SelectedBackup(st);
    if (selection < 0) {
        SetBackupStatus(st, T().selectBackup, kDanger);
        return;
    }

    const BackupInfo backup = st->backups[static_cast<size_t>(selection)];
    std::wstring date = TimestampText(backup.timestamp);

    if (!ConfirmDialog(st->hwnd, T().deleteTitle,
                       FormatText(T().deleteText, date.c_str()),
                       T().btnDelete, true))
        return;

    if (RecyclePath(backup.path)) {
        ReloadBackups(st);
        SetBackupStatus(st, T().deleteOk, kSuccess);
    } else {
        SetBackupStatus(st, T().deleteErr, kDanger);
    }
}

void OpenSelectedFolder(BackupsState* st)
{
    fs::path target;
    const int selection = SelectedBackup(st);
    if (selection >= 0) {
        target = st->backups[static_cast<size_t>(selection)].path;
    } else if (auto dir = getBackupDir(true)) {
        target = *dir;
    }

    if (target.empty()) {
        SetBackupStatus(st, T().openErr, kDanger);
        return;
    }

    HINSTANCE result = ShellExecuteW(nullptr, L"open", target.c_str(),
                                     nullptr, nullptr, SW_SHOWNORMAL);
    if (reinterpret_cast<INT_PTR>(result) <= 32)
        SetBackupStatus(st, T().openErr, kDanger);
}

LRESULT CALLBACK BackupsProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
    auto* st = reinterpret_cast<BackupsState*>(GetWindowLongPtrW(hwnd, GWLP_USERDATA));

    switch (msg) {
    case WM_NCCREATE: {
        auto* cs = reinterpret_cast<CREATESTRUCTW*>(lParam);
        auto* state = reinterpret_cast<BackupsState*>(cs->lpCreateParams);
        state->hwnd = hwnd;
        SetWindowLongPtrW(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(state));
        return TRUE;
    }
    case WM_CREATE: {
        st->dpi = DpiOf(hwnd);
        st->font = MakeFont(9, FW_NORMAL, st->dpi);
        st->fontTitle = MakeFont(15, FW_SEMIBOLD, st->dpi);
        st->fontSmall = MakeFont(9, FW_NORMAL, st->dpi);

        st->list = CreateWindowExW(0, WC_LISTVIEWW, L"",
                                   WS_CHILD | WS_VISIBLE | LVS_REPORT |
                                       LVS_SINGLESEL | LVS_SHOWSELALWAYS | WS_TABSTOP,
                                   0, 0, 0, 0, hwnd,
                                   reinterpret_cast<HMENU>(static_cast<INT_PTR>(kIdBackupList)),
                                   GetModuleHandleW(nullptr), nullptr);
        ListView_SetExtendedListViewStyle(
            st->list, LVS_EX_FULLROWSELECT | LVS_EX_DOUBLEBUFFER | LVS_EX_LABELTIP);
        ApplyDarkListView(st->list);

        st->restore = CreateWindowExW(0, L"BUTTON", T().btnRestore,
                                      WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_DEFPUSHBUTTON,
                                      0, 0, 0, 0, hwnd,
                                      reinterpret_cast<HMENU>(static_cast<INT_PTR>(kIdBackupRestore)),
                                      GetModuleHandleW(nullptr), nullptr);
        st->open = CreateWindowExW(0, L"BUTTON", T().openFolder,
                                   WS_CHILD | WS_VISIBLE | WS_TABSTOP,
                                   0, 0, 0, 0, hwnd,
                                   reinterpret_cast<HMENU>(static_cast<INT_PTR>(kIdBackupOpen)),
                                   GetModuleHandleW(nullptr), nullptr);
        st->remove = CreateWindowExW(0, L"BUTTON", T().btnDelete,
                                     WS_CHILD | WS_VISIBLE | WS_TABSTOP,
                                     0, 0, 0, 0, hwnd,
                                     reinterpret_cast<HMENU>(static_cast<INT_PTR>(kIdBackupDelete)),
                                     GetModuleHandleW(nullptr), nullptr);
        st->close = CreateWindowExW(0, L"BUTTON", T().btnClose,
                                    WS_CHILD | WS_VISIBLE | WS_TABSTOP,
                                    0, 0, 0, 0, hwnd,
                                    reinterpret_cast<HMENU>(static_cast<INT_PTR>(kIdBackupClose)),
                                    GetModuleHandleW(nullptr), nullptr);

        SendMessageW(st->list, WM_SETFONT, reinterpret_cast<WPARAM>(st->font), TRUE);
        SendMessageW(st->restore, WM_SETFONT, reinterpret_cast<WPARAM>(st->font), TRUE);
        SendMessageW(st->open, WM_SETFONT, reinterpret_cast<WPARAM>(st->font), TRUE);
        SendMessageW(st->remove, WM_SETFONT, reinterpret_cast<WPARAM>(st->font), TRUE);
        SendMessageW(st->close, WM_SETFONT, reinterpret_cast<WPARAM>(st->font), TRUE);

        StyleButton(st->restore, true, false);
        StyleButton(st->open, false, false);
        StyleButton(st->remove, false, true);
        StyleButton(st->close, false, false);

        LVCOLUMNW column{};
        column.mask = LVCF_TEXT | LVCF_WIDTH;

        const wchar_t* headers[4] = { T().colDate, T().colCount, T().colSize, T().colFolder };
        const int widths[4] = { 190, 70, 110, 380 };

        for (int i = 0; i < 4; ++i) {
            column.cx = S(hwnd, widths[i]);
            column.pszText = const_cast<wchar_t*>(headers[i]);
            SendMessageW(st->list, LVM_INSERTCOLUMNW, static_cast<WPARAM>(i),
                         reinterpret_cast<LPARAM>(&column));
        }

        ReloadBackups(st);
        LayoutBackups(st);
        SetFocus(st->list);
        return 0;
    }
    case WM_SIZE:
        if (st) LayoutBackups(st);
        return 0;
    case WM_ERASEBKGND:
        return 1;
    case WM_PAINT: {
        if (!st) break;
        BufferedPainter paint(hwnd);
        HDC dc = paint.dc();
        const RECT& rc = paint.client();

        const int backupCount = static_cast<int>(st->backups.size());
        PaintDialogBase(hwnd, dc, T().backupsTitle,
                        FormatText(backupCount == 1 ? T().backupsCountOne : T().backupsCount,
                                   backupCount),
                        st->fontTitle, st->fontSmall, S(hwnd, 20));

        RECT sr{S(hwnd, 22), rc.bottom - S(hwnd, 20) - S(hwnd, 36) - S(hwnd, 30),
                rc.right - S(hwnd, 22), rc.bottom - S(hwnd, 20) - S(hwnd, 36) - S(hwnd, 6)};
        DrawWrapped(dc, st->status, sr, st->fontSmall, st->statusColor,
                    DT_LEFT | DT_TOP | DT_SINGLELINE | DT_NOPREFIX);

        if (st->backups.empty()) {
            RECT empty{S(hwnd, 20), S(hwnd, 78), rc.right - S(hwnd, 20),
                       rc.bottom - S(hwnd, 20) - S(hwnd, 36) - S(hwnd, 34)};
            DrawTextCentered(dc, T().noBackups, empty, st->font, kMuted);
        }
        return 0;
    }
    case WM_DRAWITEM:
        DrawOwnerButton(reinterpret_cast<const DRAWITEMSTRUCT*>(lParam));
        return TRUE;
    case WM_COMMAND:
        if (!st) break;
        switch (LOWORD(wParam)) {
        case kIdBackupRestore:
        case IDOK:
            RestoreSelected(st);
            return 0;
        case kIdBackupOpen:
            OpenSelectedFolder(st);
            return 0;
        case kIdBackupDelete:
            DeleteSelected(st);
            return 0;
        case kIdBackupClose:
        case IDCANCEL:
            DestroyWindow(hwnd);
            return 0;
        default:
            break;
        }
        return 0;
    case WM_NOTIFY: {
        if (!st) break;
        {
            LRESULT handled = 0;
            if (HandleHeaderCustomDraw(st->list, lParam, &handled))
                return handled;
        }
        auto* header = reinterpret_cast<LPNMHDR>(lParam);
        if (header->idFrom != kIdBackupList) break;

        if (header->code == NM_DBLCLK) {
            RestoreSelected(st);
            return 0;
        }
        if (header->code == LVN_KEYDOWN) {
            auto* key = reinterpret_cast<NMLVKEYDOWN*>(lParam);
            if (key->wVKey == VK_RETURN) {
                RestoreSelected(st);
                return 0;
            }
            if (key->wVKey == VK_DELETE) {
                DeleteSelected(st);
                return 0;
            }
        }
        break;
    }
    case WM_CLOSE:
        DestroyWindow(hwnd);
        return 0;
    case WM_NCDESTROY:
        if (st) {
            if (st->font) DeleteObject(st->font);
            if (st->fontTitle) DeleteObject(st->fontTitle);
            if (st->fontSmall) DeleteObject(st->fontSmall);
            st->font = st->fontTitle = st->fontSmall = nullptr;
            SetWindowLongPtrW(hwnd, GWLP_USERDATA, 0);
        }
        return 0;
    default:
        break;
    }

    return DefWindowProcW(hwnd, msg, wParam, lParam);
}

// ---------------------------------------------------------------------------
// Update available
// ---------------------------------------------------------------------------
constexpr int kIdUpdateNotes   = 3501;
constexpr int kIdUpdateInstall = 3502;
constexpr int kIdUpdateLater   = 3503;
constexpr int kIdUpdateSkip    = 3504;

constexpr int kIdDownloadBar    = 3511;
constexpr int kIdDownloadCancel = 3512;

constexpr UINT kDownloadProgress = WM_APP + 72;
constexpr UINT kDownloadDone     = WM_APP + 73;

const wchar_t* kUpdateClass   = L"DRBC_Update";
const wchar_t* kDownloadClass = L"DRBC_Download";

struct UpdateState {
    HWND hwnd = nullptr;
    HWND notes = nullptr;
    HWND install = nullptr;
    HWND later = nullptr;
    HWND skip = nullptr;
    HFONT font = nullptr;
    HFONT fontTitle = nullptr;
    HFONT fontSmall = nullptr;
    const upd::ReleaseInfo* info = nullptr;
    bool allowSkip = true;
    bool accepted = false;
};

void LayoutUpdate(UpdateState* st)
{
    RECT rc{};
    GetClientRect(st->hwnd, &rc);
    const int m = S(st->hwnd, 22);
    const int gap = S(st->hwnd, 10);
    const int buttonHeight = S(st->hwnd, 36);
    const int buttonWidth = S(st->hwnd, 132);
    const int bottom = rc.bottom - m - buttonHeight;
    const int notesTop = S(st->hwnd, 96);

    MoveWindow(st->notes, m, notesTop, rc.right - m * 2,
               bottom - notesTop - gap, TRUE);

    int x = rc.right - m - buttonWidth;
    MoveWindow(st->install, x, bottom, buttonWidth, buttonHeight, TRUE);
    x -= gap + buttonWidth;
    MoveWindow(st->later, x, bottom, buttonWidth, buttonHeight, TRUE);
    if (st->skip) MoveWindow(st->skip, m, bottom, buttonWidth, buttonHeight, TRUE);
}

LRESULT CALLBACK UpdateProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
    auto* st = reinterpret_cast<UpdateState*>(GetWindowLongPtrW(hwnd, GWLP_USERDATA));

    switch (msg) {
    case WM_NCCREATE: {
        auto* cs = reinterpret_cast<CREATESTRUCTW*>(lParam);
        auto* state = reinterpret_cast<UpdateState*>(cs->lpCreateParams);
        state->hwnd = hwnd;
        SetWindowLongPtrW(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(state));
        return TRUE;
    }
    case WM_CREATE: {
        if (!st || !st->info) return -1;

        st->font = MakeFont(10, FW_NORMAL, DpiOf(hwnd));
        st->fontTitle = MakeFont(12, FW_SEMIBOLD, DpiOf(hwnd));
        st->fontSmall = MakeFont(9, FW_NORMAL, DpiOf(hwnd));

        std::wstring notes = cfg::widen(upd::NotesToPlainText(st->info->notes));
        if (notes.empty()) notes = T().updateNoNotes;

        st->notes = CreateWindowExW(WS_EX_CLIENTEDGE, L"EDIT", notes.c_str(),
                                    WS_CHILD | WS_VISIBLE | WS_TABSTOP | WS_VSCROLL |
                                    ES_MULTILINE | ES_READONLY | ES_AUTOVSCROLL,
                                    0, 0, 0, 0, hwnd,
                                    reinterpret_cast<HMENU>(static_cast<INT_PTR>(kIdUpdateNotes)),
                                    GetModuleHandleW(nullptr), nullptr);

        st->install = CreateWindowExW(0, L"BUTTON", T().btnInstallUpdate,
                                      WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_DEFPUSHBUTTON,
                                      0, 0, 0, 0, hwnd,
                                      reinterpret_cast<HMENU>(static_cast<INT_PTR>(kIdUpdateInstall)),
                                      GetModuleHandleW(nullptr), nullptr);
        st->later = CreateWindowExW(0, L"BUTTON", T().btnLater,
                                    WS_CHILD | WS_VISIBLE | WS_TABSTOP,
                                    0, 0, 0, 0, hwnd,
                                    reinterpret_cast<HMENU>(static_cast<INT_PTR>(kIdUpdateLater)),
                                    GetModuleHandleW(nullptr), nullptr);

        if (st->allowSkip) {
            st->skip = CreateWindowExW(0, L"BUTTON", T().btnSkipVersion,
                                       WS_CHILD | WS_VISIBLE | WS_TABSTOP,
                                       0, 0, 0, 0, hwnd,
                                       reinterpret_cast<HMENU>(static_cast<INT_PTR>(kIdUpdateSkip)),
                                       GetModuleHandleW(nullptr), nullptr);
        }

        SendMessageW(st->notes, WM_SETFONT, reinterpret_cast<WPARAM>(st->font), TRUE);
        SendMessageW(st->install, WM_SETFONT, reinterpret_cast<WPARAM>(st->font), TRUE);
        SendMessageW(st->later, WM_SETFONT, reinterpret_cast<WPARAM>(st->font), TRUE);
        if (st->skip) SendMessageW(st->skip, WM_SETFONT, reinterpret_cast<WPARAM>(st->font), TRUE);

        StyleButton(st->install, true, false);
        StyleButton(st->later, false, false);
        if (st->skip) StyleButton(st->skip, false, false);

        SetFocus(st->install);
        LayoutUpdate(st);
        return 0;
    }
    case WM_SIZE:
        if (st) LayoutUpdate(st);
        return 0;
    case WM_ERASEBKGND:
        return 1;
    case WM_PAINT: {
        if (!st || !st->info) break;

        std::wstring subtitle = FormatText(T().updateSubtitle, DRBC_VERSION_W,
                                           cfg::widen(st->info->version).c_str());

        BufferedPainter paint(hwnd);
        HDC dc = paint.dc();
        PaintDialogBase(hwnd, dc, T().updateTitle, subtitle, st->fontTitle, st->fontSmall,
                        S(hwnd, 22));
        return 0;
    }
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
        case kIdUpdateInstall:
            st->accepted = true;
            DestroyWindow(hwnd);
            return 0;
        case kIdUpdateSkip:
            if (st->info) app::SaveUpdateSkipped(cfg::widen(st->info->version));
            DestroyWindow(hwnd);
            return 0;
        case kIdUpdateLater:
        case IDCANCEL:
            DestroyWindow(hwnd);
            return 0;
        default:
            break;
        }
        return 0;
    case WM_CLOSE:
        DestroyWindow(hwnd);
        return 0;
    case WM_NCDESTROY:
        if (st) {
            if (st->font) DeleteObject(st->font);
            if (st->fontTitle) DeleteObject(st->fontTitle);
            if (st->fontSmall) DeleteObject(st->fontSmall);
            st->font = st->fontTitle = st->fontSmall = nullptr;
            SetWindowLongPtrW(hwnd, GWLP_USERDATA, 0);
        }
        return 0;
    default:
        break;
    }

    return DefWindowProcW(hwnd, msg, wParam, lParam);
}

// ---------------------------------------------------------------------------
// Download progress
// ---------------------------------------------------------------------------
struct DownloadState {
    HWND hwnd = nullptr;
    HWND bar = nullptr;
    HWND cancel = nullptr;
    HFONT font = nullptr;
    HFONT fontTitle = nullptr;
    HFONT fontSmall = nullptr;

    std::wstring version;
    std::filesystem::path dest;

    std::atomic<bool> cancelled{false};
    std::mutex mutex;
    int percent = 0;
    bool done = false;
    bool ok = false;
    std::string error;
    std::wstring message;      // shown instead of the percentage on failure
    bool started = false;      // the installer was launched
};

void LayoutDownload(DownloadState* st)
{
    RECT rc{};
    GetClientRect(st->hwnd, &rc);
    const int m = S(st->hwnd, 22);
    const int buttonHeight = S(st->hwnd, 36);
    const int buttonWidth = S(st->hwnd, 132);

    MoveWindow(st->bar, m, S(st->hwnd, 108), rc.right - m * 2, S(st->hwnd, 12), TRUE);
    MoveWindow(st->cancel, rc.right - m - buttonWidth, rc.bottom - m - buttonHeight,
               buttonWidth, buttonHeight, TRUE);
}

void StartDownload(const std::shared_ptr<DownloadState>& state, upd::ReleaseInfo info)
{
    const HWND target = state->hwnd;
    const std::filesystem::path dest = state->dest;

    std::thread([state, target, dest, info = std::move(info)]() {
        std::string error;
        const bool downloaded = upd::DownloadFile(
            info.installerUrl, dest,
            [target](int percent) {
                if (IsWindow(target))
                    PostMessageW(target, kDownloadProgress,
                                 static_cast<WPARAM>(percent), 0);
            },
            [state] { return state->cancelled.load(); },
            error);

        bool ok = downloaded;
        if (ok) ok = upd::VerifyDigest(dest, info.installerDigest, error);

        {
            std::lock_guard<std::mutex> lock(state->mutex);
            state->ok = ok;
            state->error = error;
        }
        if (IsWindow(target)) PostMessageW(target, kDownloadDone, 0, 0);
    }).detach();
}

LRESULT CALLBACK DownloadProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
    auto* st = reinterpret_cast<DownloadState*>(GetWindowLongPtrW(hwnd, GWLP_USERDATA));

    switch (msg) {
    case WM_NCCREATE: {
        auto* cs = reinterpret_cast<CREATESTRUCTW*>(lParam);
        auto* state = reinterpret_cast<DownloadState*>(cs->lpCreateParams);
        state->hwnd = hwnd;
        SetWindowLongPtrW(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(state));
        return TRUE;
    }
    case WM_CREATE: {
        if (!st) return -1;

        st->font = MakeFont(10, FW_NORMAL, DpiOf(hwnd));
        st->fontTitle = MakeFont(12, FW_SEMIBOLD, DpiOf(hwnd));
        st->fontSmall = MakeFont(9, FW_NORMAL, DpiOf(hwnd));

        st->bar = CreateWindowExW(0, PROGRESS_CLASSW, L"",
                                  WS_CHILD | WS_VISIBLE | PBS_SMOOTH,
                                  0, 0, 0, 0, hwnd,
                                  reinterpret_cast<HMENU>(static_cast<INT_PTR>(kIdDownloadBar)),
                                  GetModuleHandleW(nullptr), nullptr);
        st->cancel = CreateWindowExW(0, L"BUTTON", T().cancel,
                                     WS_CHILD | WS_VISIBLE | WS_TABSTOP,
                                     0, 0, 0, 0, hwnd,
                                     reinterpret_cast<HMENU>(static_cast<INT_PTR>(kIdDownloadCancel)),
                                     GetModuleHandleW(nullptr), nullptr);

        SendMessageW(st->cancel, WM_SETFONT, reinterpret_cast<WPARAM>(st->font), TRUE);
        StyleButton(st->cancel, false, false);

        SendMessageW(st->bar, PBM_SETBKCOLOR, 0, static_cast<LPARAM>(kInput));
        SendMessageW(st->bar, PBM_SETBARCOLOR, 0, static_cast<LPARAM>(kAccent));
        SendMessageW(st->bar, PBM_SETRANGE, 0, MAKELPARAM(0, 100));

        SetFocus(st->cancel);
        LayoutDownload(st);
        return 0;
    }
    case WM_SIZE:
        if (st) LayoutDownload(st);
        return 0;
    case WM_ERASEBKGND:
        return 1;
    case WM_PAINT: {
        if (!st) break;

        BufferedPainter paint(hwnd);
        HDC dc = paint.dc();
        RECT rc = paint.client();
        const int m = S(hwnd, 22);

        PaintDialogBase(hwnd, dc, T().downloadTitle,
                        FormatText(T().downloadText, st->version.c_str()),
                        st->fontTitle, st->fontSmall, m);

        {
            std::lock_guard<std::mutex> lock(st->mutex);
            RECT tr{m, S(hwnd, 132), rc.right - m, S(hwnd, 160)};
            if (!st->message.empty()) {
                DrawWrapped(dc, st->message, tr, st->font, kDanger,
                            DT_LEFT | DT_TOP | DT_WORDBREAK | DT_NOPREFIX);
            } else {
                wchar_t line[128];
                swprintf(line, 128, L"%d%%", st->percent < 0 ? 0 : st->percent);
                DrawWrapped(dc, line, tr, st->font, kMuted,
                            DT_LEFT | DT_TOP | DT_SINGLELINE | DT_NOPREFIX);
            }
        }
        return 0;
    }
    case WM_DRAWITEM:
        DrawOwnerButton(reinterpret_cast<const DRAWITEMSTRUCT*>(lParam));
        return TRUE;
    case kDownloadProgress:
        if (!st) return 0;
        {
            std::lock_guard<std::mutex> lock(st->mutex);
            st->percent = static_cast<int>(wParam);
        }
        SendMessageW(st->bar, PBM_SETPOS,
                     static_cast<WPARAM>(wParam < 0 ? 0 : wParam), 0);
        InvalidateRect(hwnd, nullptr, FALSE);
        return 0;
    case kDownloadDone:
        if (!st) return 0;

        if (st->cancelled.load()) {
            DestroyWindow(hwnd);
            return 0;
        }

        {
            std::lock_guard<std::mutex> lock(st->mutex);
            st->done = true;
            if (!st->ok)
                st->message = FormatText(T().downloadFailed,
                                         cfg::widen(st->error).c_str());
        }

        if (st->ok) {
            std::string error;
            if (upd::RunInstaller(st->dest, error)) {
                st->started = true;
                DestroyWindow(hwnd);
                return 0;
            }
            std::lock_guard<std::mutex> lock(st->mutex);
            st->message = FormatText(T().downloadFailed, cfg::widen(error).c_str());
        }

        SetWindowTextW(st->cancel, T().btnClose);
        EnableWindow(st->cancel, TRUE);
        InvalidateRect(hwnd, nullptr, FALSE);
        return 0;
    case WM_COMMAND:
        if (!st) break;
        if (LOWORD(wParam) == kIdDownloadCancel) {
            if (st->done) {
                DestroyWindow(hwnd);
            } else {
                st->cancelled.store(true);
                EnableWindow(st->cancel, FALSE);
            }
            return 0;
        }
        return 0;
    case WM_CLOSE:
        if (st && !st->done) {
            st->cancelled.store(true);
            EnableWindow(st->cancel, FALSE);
            return 0;
        }
        DestroyWindow(hwnd);
        return 0;
    case WM_NCDESTROY:
        if (st) {
            if (st->font) DeleteObject(st->font);
            if (st->fontTitle) DeleteObject(st->fontTitle);
            if (st->fontSmall) DeleteObject(st->fontSmall);
            st->font = st->fontTitle = st->fontSmall = nullptr;
            SetWindowLongPtrW(hwnd, GWLP_USERDATA, 0);
        }
        return 0;
    default:
        break;
    }

    return DefWindowProcW(hwnd, msg, wParam, lParam);
}

} // namespace

// ---------------------------------------------------------------------------
// Public dialog entry points
// ---------------------------------------------------------------------------
bool ConfirmDialog(HWND owner, const wchar_t* title,
                   const std::wstring& message, const wchar_t* action, bool danger)
{
    ConfirmState state;
    state.message = message;
    state.danger = danger;

    HWND hwnd = CreateAppWindow(kConfirmClass, title, 540, 260, owner, &state, ConfirmProc);
    if (!hwnd) return false;

    if (HWND yes = GetDlgItem(hwnd, kIdYes); yes && action && *action)
        SetWindowTextW(yes, action);

    RunModal(hwnd, owner);
    return state.result;
}

Lang PickLanguage(HWND owner)
{
    LanguageState state;
    state.result = GetLang();

    HWND hwnd = CreateAppWindow(kLanguageClass, T().langTitle, 500, 280, owner, &state,
                                LanguageProc);
    if (!hwnd) return state.result;

    RunModal(hwnd, owner);
    return state.result;
}

void ShowAboutDialog(HWND owner)
{
    AboutState state;
    HWND hwnd = CreateAppWindow(kAboutClass, T().aboutTitle, 640, 400, owner, &state,
                                AboutProc);
    if (!hwnd) return;

    RunModal(hwnd, owner);
}

void ShowBackupsDialog(HWND owner, const fs::path& saveDir,
                       std::function<void()> onRestored)
{
    BackupsState state;
    state.saveDir = saveDir;
    state.onRestored = std::move(onRestored);
    state.status = T().statusReady;
    state.statusColor = kMuted;

    HWND hwnd = CreateAppWindow(kBackupsClass, T().backupsTitle, 900, 560, owner, &state,
                                BackupsProc);
    if (!hwnd) return;

    RunModal(hwnd, owner);
}

void ShowSettingsDialog(HWND owner, std::function<void()> onApplied)
{
    SettingsState state;
    state.onApplied = std::move(onApplied);

    HWND hwnd = CreateAppWindow(kSettingsClass, T().settingsTitle, 560, 560,
                                owner, &state, SettingsProc);
    if (!hwnd) return;

    RunModal(hwnd, owner);
}

bool ShowUpdateDialog(HWND owner, const upd::ReleaseInfo& info, bool allowSkip)
{
    UpdateState state;
    state.info = &info;
    state.allowSkip = allowSkip;

    HWND hwnd = CreateAppWindow(kUpdateClass, T().updateTitle, 620, 540, owner,
                                &state, UpdateProc);
    if (!hwnd) return false;

    RunModal(hwnd, owner);
    return state.accepted;
}

bool RunDownloadAndInstall(HWND owner, const upd::ReleaseInfo& info)
{
    auto state = std::make_shared<DownloadState>();
    state->version = cfg::widen(info.version);
    state->dest = upd::UpdateDir(true) / L"drbcWindowsInstaller.exe";

    if (state->dest.filename().empty()) return false;

    HWND hwnd = CreateAppWindow(kDownloadClass, T().downloadTitle, 560, 300, owner,
                                state.get(), DownloadProc);
    if (!hwnd) return false;

    StartDownload(state, info);
    RunModal(hwnd, owner);
    return state->started;
}

} // namespace ui
