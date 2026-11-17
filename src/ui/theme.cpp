#include "theme.h"
#include "drbackup.h"

#include <dwmapi.h>
#include <uxtheme.h>
#include <cwchar>
#include <cstdarg>
#include <vector>

namespace ui {

namespace {

struct ButtonData {
    bool primary = false;
    bool danger  = false;
    bool hot     = false;
    bool pressed = false;
    COLORREF background = kBg;
    WNDPROC oldProc = nullptr;
};

LRESULT CALLBACK ButtonWndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
    auto* data = reinterpret_cast<ButtonData*>(GetWindowLongPtrW(hwnd, GWLP_USERDATA));

    switch (msg) {
    case WM_MOUSEMOVE:
        if (data && !data->hot) {
            TRACKMOUSEEVENT tme{};
            tme.cbSize    = sizeof(tme);
            tme.dwFlags   = TME_LEAVE;
            tme.hwndTrack = hwnd;
            TrackMouseEvent(&tme);
            data->hot = true;
            InvalidateRect(hwnd, nullptr, FALSE);
        }
        break;
    case WM_MOUSELEAVE:
        if (data) {
            data->hot = false;
            data->pressed = false;
        }
        InvalidateRect(hwnd, nullptr, FALSE);
        break;
    case WM_LBUTTONDOWN:
        if (data) data->pressed = true;
        InvalidateRect(hwnd, nullptr, FALSE);
        break;
    case WM_LBUTTONUP:
        if (data) data->pressed = false;
        InvalidateRect(hwnd, nullptr, FALSE);
        break;
    case WM_CAPTURECHANGED:
        if (data) data->pressed = false;
        InvalidateRect(hwnd, nullptr, FALSE);
        break;
    case WM_ERASEBKGND: {
        // Erase with the colour behind the button so the rounded corners
        // don't show the default light button face.
        HDC dc = reinterpret_cast<HDC>(wParam);
        RECT rc{};
        GetClientRect(hwnd, &rc);
        FillRectC(dc, rc, data ? data->background : kBg);
        return 1;
    }
    case WM_ENABLE:
    case WM_SETFOCUS:
    case WM_KILLFOCUS:
        InvalidateRect(hwnd, nullptr, FALSE);
        break;
    case WM_NCDESTROY: {
        WNDPROC oldProc = data ? data->oldProc : nullptr;
        SetWindowLongPtrW(hwnd, GWLP_USERDATA, 0);
        delete data;
        LRESULT result = oldProc ? CallWindowProcW(oldProc, hwnd, msg, wParam, lParam)
                                 : DefWindowProcW(hwnd, msg, wParam, lParam);
        return result;
    }
    default:
        break;
    }

    if (data && data->oldProc)
        return CallWindowProcW(data->oldProc, hwnd, msg, wParam, lParam);
    return DefWindowProcW(hwnd, msg, wParam, lParam);
}

using DwmSetWindowAttributeFn = HRESULT(WINAPI*)(HWND, DWORD, LPCVOID, DWORD);
using GetDpiForWindowFn       = UINT(WINAPI*)(HWND);

GetDpiForWindowFn getDpiForWindowFn()
{
    static GetDpiForWindowFn fn = []() -> GetDpiForWindowFn {
        HMODULE user32 = GetModuleHandleW(L"user32.dll");
        if (!user32) return nullptr;
        return reinterpret_cast<GetDpiForWindowFn>(
            GetProcAddress(user32, "GetDpiForWindow"));
    }();
    return fn;
}

} // namespace

// ---------------------------------------------------------------------------
// DPI
// ---------------------------------------------------------------------------
int DpiOf(HWND hwnd)
{
    if (auto fn = getDpiForWindowFn()) {
        if (hwnd) {
            UINT dpi = fn(hwnd);
            if (dpi) return static_cast<int>(dpi);
        }
    }
    HDC dc = GetDC(hwnd);
    int dpi = dc ? GetDeviceCaps(dc, LOGPIXELSX) : 96;
    if (dc) ReleaseDC(hwnd, dc);
    return dpi > 0 ? dpi : 96;
}

int S(HWND hwnd, int value)
{
    return MulDiv(value, DpiOf(hwnd), 96);
}

// ---------------------------------------------------------------------------
// Dark theming
// ---------------------------------------------------------------------------
void EnableDarkTitleBar(HWND hwnd)
{
    static DwmSetWindowAttributeFn fn = []() -> DwmSetWindowAttributeFn {
        HMODULE dwm = LoadLibraryW(L"dwmapi.dll");
        if (!dwm) return nullptr;
        return reinterpret_cast<DwmSetWindowAttributeFn>(
            GetProcAddress(dwm, "DwmSetWindowAttribute"));
    }();

    if (!fn) return;

    BOOL enabled = TRUE;
    // 20 = DWMWA_USE_IMMERSIVE_DARK_MODE (Win10 20H1+), 19 = older builds.
    if (FAILED(fn(hwnd, 20, &enabled, sizeof(enabled))))
        fn(hwnd, 19, &enabled, sizeof(enabled));
}

void ApplyDarkListView(HWND listView)
{
    if (!listView) return;

    SetWindowTheme(listView, L"DarkMode_Explorer", nullptr);
    ListView_SetBkColor(listView, kPanel);
    ListView_SetTextBkColor(listView, kPanel);
    ListView_SetTextColor(listView, kText);

    HWND header = ListView_GetHeader(listView);
    if (header) SetWindowTheme(header, L"DarkMode_ItemsView", nullptr);
}

void ApplyDarkEdit(HWND edit)
{
    if (!edit) return;
    SetWindowTheme(edit, L"DarkMode_Explorer", nullptr);
}

void ApplyDarkTree(HWND hwnd)
{
    if (!hwnd) return;
    SetWindowTheme(hwnd, L"DarkMode_Explorer", nullptr);
}

// ---------------------------------------------------------------------------
// Fonts / painting
// ---------------------------------------------------------------------------
HFONT MakeFont(int points, int weight, int dpi)
{
    LOGFONTW lf{};
    lf.lfHeight  = -MulDiv(points, dpi, 72);
    lf.lfWeight  = weight;
    lf.lfCharSet = DEFAULT_CHARSET;
    lf.lfQuality = CLEARTYPE_QUALITY;
    lstrcpynW(lf.lfFaceName, L"Segoe UI", LF_FACESIZE);
    return CreateFontIndirectW(&lf);
}

HBRUSH BgBrush()
{
    static HBRUSH brush = CreateSolidBrush(kBg);
    return brush;
}

HBRUSH InputBrush()
{
    static HBRUSH brush = CreateSolidBrush(kInput);
    return brush;
}

void FillRectC(HDC dc, const RECT& rc, COLORREF color)
{
    HBRUSH brush = CreateSolidBrush(color);
    FillRect(dc, &rc, brush);
    DeleteObject(brush);
}

void FillRoundRectC(HDC dc, const RECT& rc, int radius,
                    COLORREF fill, COLORREF border, int borderWidth)
{
    HBRUSH brush = CreateSolidBrush(fill);
    HPEN   pen   = CreatePen(PS_SOLID, borderWidth, border);
    HGDIOBJ oldBrush = SelectObject(dc, brush);
    HGDIOBJ oldPen   = SelectObject(dc, pen);

    if (radius > 0)
        RoundRect(dc, rc.left, rc.top, rc.right, rc.bottom, radius * 2, radius * 2);
    else
        Rectangle(dc, rc.left, rc.top, rc.right, rc.bottom);

    SelectObject(dc, oldBrush);
    SelectObject(dc, oldPen);
    DeleteObject(brush);
    DeleteObject(pen);
}

void DrawWrapped(HDC dc, const std::wstring& text, RECT rc, HFONT font,
                 COLORREF color, UINT flags)
{
    HGDIOBJ oldFont = font ? SelectObject(dc, font) : nullptr;
    int oldBk = SetBkMode(dc, TRANSPARENT);
    COLORREF oldColor = SetTextColor(dc, color);

    DrawTextW(dc, text.c_str(), static_cast<int>(text.size()), &rc, flags);

    SetTextColor(dc, oldColor);
    SetBkMode(dc, oldBk);
    if (oldFont) SelectObject(dc, oldFont);
}

void DrawTextCentered(HDC dc, const std::wstring& text, RECT rc, HFONT font,
                      COLORREF color)
{
    HGDIOBJ oldFont = font ? SelectObject(dc, font) : nullptr;
    int oldBk = SetBkMode(dc, TRANSPARENT);
    COLORREF oldColor = SetTextColor(dc, color);

    RECT calc = rc;
    calc.bottom = 0;
    DrawTextW(dc, text.c_str(), static_cast<int>(text.size()), &calc,
              DT_CENTER | DT_WORDBREAK | DT_CALCRECT | DT_NOPREFIX);

    const int textHeight = calc.bottom;
    RECT dest = rc;
    const int offset = ((rc.bottom - rc.top) - textHeight) / 2;
    if (offset > 0) dest.top += offset;

    DrawTextW(dc, text.c_str(), static_cast<int>(text.size()), &dest,
              DT_CENTER | DT_WORDBREAK | DT_NOPREFIX);

    SetTextColor(dc, oldColor);
    SetBkMode(dc, oldBk);
    if (oldFont) SelectObject(dc, oldFont);
}

// ---------------------------------------------------------------------------
// Owner-drawn buttons
// ---------------------------------------------------------------------------
void StyleButton(HWND button, bool primary, bool danger, COLORREF background)
{
    if (!button) return;

    LONG_PTR style = GetWindowLongPtrW(button, GWL_STYLE);
    SetWindowLongPtrW(button, GWL_STYLE, style | BS_OWNERDRAW);
    SetWindowTheme(button, L"", L"");

    auto* data = new ButtonData();
    data->primary = primary;
    data->danger = danger;
    data->background = background;
    SetWindowLongPtrW(button, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(data));
    data->oldProc = reinterpret_cast<WNDPROC>(
        SetWindowLongPtrW(button, GWLP_WNDPROC,
                          reinterpret_cast<LONG_PTR>(ButtonWndProc)));
}

void DrawOwnerButton(const DRAWITEMSTRUCT* dis)
{
    if (!dis || !dis->hwndItem) return;

    ButtonData fallback;
    auto* stored = reinterpret_cast<ButtonData*>(
        GetWindowLongPtrW(dis->hwndItem, GWLP_USERDATA));
    auto* data = stored ? stored : &fallback;

    const bool disabled = (dis->itemState & ODS_DISABLED) != 0;
    const bool pressed  = (dis->itemState & ODS_SELECTED) != 0 || data->pressed;
    const bool focused  = GetFocus() == dis->hwndItem;

    COLORREF fill   = kPanelAlt;
    COLORREF border = kBorder;
    COLORREF text   = kText;

    if (disabled) {
        fill   = RGB(34, 34, 36);
        border = RGB(48, 48, 51);
        text   = RGB(105, 105, 110);
    } else if (data->primary) {
        fill   = pressed ? RGB(188, 40, 48) : (data->hot ? kAccentHot : kAccent);
        border = fill;
        text   = RGB(255, 255, 255);
    } else if (data->danger) {
        fill   = pressed ? RGB(92, 35, 39) : (data->hot ? RGB(78, 37, 41) : RGB(55, 32, 35));
        border = kDanger;
        text   = kDanger;
    } else {
        fill   = pressed ? RGB(51, 51, 55) : (data->hot ? RGB(58, 58, 62) : kPanelAlt);
        border = data->hot ? kBorderHot : kBorder;
        text   = kText;
    }

    RECT rc = dis->rcItem;
    FillRoundRectC(dis->hDC, rc, S(dis->hwndItem, 6), fill, border);

    wchar_t buffer[256] = {};
    GetWindowTextW(dis->hwndItem, buffer, 255);

    HFONT font = reinterpret_cast<HFONT>(SendMessageW(dis->hwndItem, WM_GETFONT, 0, 0));
    HGDIOBJ oldFont = font ? SelectObject(dis->hDC, font) : nullptr;
    SetBkMode(dis->hDC, TRANSPARENT);
    SetTextColor(dis->hDC, text);

    RECT textRc = rc;
    DrawTextW(dis->hDC, buffer, -1, &textRc,
              DT_CENTER | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX);

    if (oldFont) SelectObject(dis->hDC, oldFont);

    if (focused && !disabled) {
        RECT focus = rc;
        InflateRect(&focus, -S(dis->hwndItem, 4), -S(dis->hwndItem, 4));
        DrawFocusRect(dis->hDC, &focus);
    }
}

// ---------------------------------------------------------------------------
// Window helpers
// ---------------------------------------------------------------------------
void CenterWindowOn(HWND window, HWND owner)
{
    RECT rc{};
    GetWindowRect(window, &rc);
    const int width  = rc.right - rc.left;
    const int height = rc.bottom - rc.top;

    RECT area{};
    HMONITOR monitor = nullptr;
    if (owner && IsWindowVisible(owner)) {
        monitor = MonitorFromWindow(owner, MONITOR_DEFAULTTONEAREST);
        GetWindowRect(owner, &area);
    } else {
        monitor = MonitorFromPoint(POINT{0, 0}, MONITOR_DEFAULTTOPRIMARY);
        MONITORINFO mi{};
        mi.cbSize = sizeof(mi);
        GetMonitorInfoW(monitor, &mi);
        area = mi.rcWork;
    }

    if (owner && IsWindowVisible(owner)) {
        area.left = area.left + 0;
        // Center over the owner window.
        int x = area.left + ((area.right - area.left) - width) / 2;
        int y = area.top + ((area.bottom - area.top) - height) / 2;
        SetWindowPos(window, nullptr, x, y, 0, 0, SWP_NOSIZE | SWP_NOZORDER | SWP_NOACTIVATE);
        return;
    }

    int x = area.left + ((area.right - area.left) - width) / 2;
    int y = area.top + ((area.bottom - area.top) - height) / 2;
    SetWindowPos(window, nullptr, x, y, 0, 0, SWP_NOSIZE | SWP_NOZORDER | SWP_NOACTIVATE);
}

void PaintWindowBackground(HWND hwnd, HDC dc)
{
    RECT rc{};
    GetClientRect(hwnd, &rc);
    FillRectC(dc, rc, kBg);
}

BufferedPainter::BufferedPainter(HWND hwnd)
    : hwnd_(hwnd)
{
    target_ = BeginPaint(hwnd_, &ps_);
    GetClientRect(hwnd_, &rc_);

    const int w = rc_.right > 0 ? rc_.right : 1;
    const int h = rc_.bottom > 0 ? rc_.bottom : 1;

    mem_ = CreateCompatibleDC(target_);
    bmp_ = CreateCompatibleBitmap(target_, w, h);
    old_ = SelectObject(mem_, bmp_);

    FillRectC(mem_, rc_, kBg);
}

BufferedPainter::~BufferedPainter()
{
    BitBlt(target_, 0, 0, rc_.right, rc_.bottom, mem_, 0, 0, SRCCOPY);
    SelectObject(mem_, old_);
    DeleteObject(bmp_);
    DeleteDC(mem_);
    EndPaint(hwnd_, &ps_);
}

// ---------------------------------------------------------------------------
// Text helpers
// ---------------------------------------------------------------------------
std::wstring Widen(const std::string& text)
{
    if (text.empty()) return {};

    int needed = MultiByteToWideChar(CP_UTF8, 0, text.c_str(),
                                     static_cast<int>(text.size()), nullptr, 0);
    if (needed <= 0) return {};

    std::wstring out(static_cast<size_t>(needed), L'\0');
    MultiByteToWideChar(CP_UTF8, 0, text.c_str(), static_cast<int>(text.size()),
                        out.data(), needed);
    return out;
}

std::wstring SizeText(uintmax_t bytes)
{
    return Widen(formatFileSize(bytes));
}

std::wstring TimestampText(const std::string& timestamp)
{
    std::wstring ts = Widen(timestamp);
    if (ts.size() < 19) return ts;

    std::wstring out;
    out.reserve(ts.size() + 2);
    out.append(ts, 0, 10);            // YYYY-MM-DD
    out.push_back(L' ');
    out.append(ts, 11, 2);            // HH
    out.push_back(L':');
    out.append(ts, 14, 2);            // MM
    out.push_back(L':');
    out.append(ts, 17, 2);            // SS

    if (ts.size() > 19) {
        out.push_back(L' ');
        out.append(ts, 20, std::wstring::npos);
    }
    return out;
}

std::wstring FormatText(const wchar_t* fmt, ...)
{
    wchar_t buffer[2048];
    va_list args;
    va_start(args, fmt);
    int written = _vsnwprintf(buffer, 2047, fmt, args);
    va_end(args);

    if (written < 0) {
        buffer[2047] = L'\0';
        return buffer;
    }
    return std::wstring(buffer, static_cast<size_t>(written));
}

} // namespace ui
