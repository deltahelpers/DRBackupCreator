#include "dialogs.h"

#include "theme.h"
#include "resource.h"
#include "version.h"
#include "drbackup.h"

#include <commctrl.h>
#include <shellapi.h>

#include <cwchar>
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

const wchar_t* kConfirmClass = L"DRBC_Confirm";
const wchar_t* kLanguageClass = L"DRBC_Language";
const wchar_t* kAboutClass = L"DRBC_About";
const wchar_t* kBackupsClass = L"DRBC_Backups";

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

    // The window's own DPI is only reliable once it exists, so the final size
    // is computed here instead of with the owner's DPI.
    const int dpi = DpiOf(hwnd);
    RECT rc{0, 0, MulDiv(dipWidth, dpi, 96), MulDiv(dipHeight, dpi, 96)};
    AdjustWindowRectEx(&rc, style, FALSE, exStyle);
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

} // namespace ui
