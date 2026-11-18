#pragma once

#include <windows.h>
#include <commctrl.h>
#include <cstdint>
#include <string>

#include "config.h"

namespace ui {

// ---------------------------------------------------------------------------
// Palette
//
// Runtime values, not constexpr: a .bctheme file can replace them at startup.
// ApplyTheme() must run before the first window is created.
// ---------------------------------------------------------------------------
extern COLORREF kBg;
extern COLORREF kBgDeep;
extern COLORREF kPanel;
extern COLORREF kPanelAlt;
extern COLORREF kInput;
extern COLORREF kBorder;
extern COLORREF kBorderHot;
extern COLORREF kText;
extern COLORREF kMuted;
extern COLORREF kAccent;
extern COLORREF kAccentHot;
extern COLORREF kSuccess;
extern COLORREF kDanger;
extern COLORREF kSel;
extern COLORREF kWarnBg;
extern COLORREF kWarnLine;

void ApplyTheme(const cfg::Theme& theme);
const cfg::Theme& CurrentTheme();

// ---------------------------------------------------------------------------
// DPI helpers
// ---------------------------------------------------------------------------
int DpiOf(HWND hwnd);
int S(HWND hwnd, int value);

// DPI of the monitor the window is on (or would open on). Unlike DpiOf() this
// is correct for a freshly created, still hidden window.
int MonitorDpi(HWND hwnd);

// AdjustWindowRectEx, but with the window's own DPI instead of the system DPI.
void AdjustWindowRectDpi(RECT& rc, DWORD style, BOOL menu, DWORD exStyle, int dpi);

// ---------------------------------------------------------------------------
// Dark theming
// ---------------------------------------------------------------------------
void EnableDarkTitleBar(HWND hwnd);
void ApplyDarkListView(HWND listView);
void ApplyDarkEdit(HWND edit);
void ApplyDarkTree(HWND hwnd);

// Dark-custom-draws the header of a ListView. Call from WM_NOTIFY:
//   LRESULT result;
//   if (HandleHeaderCustomDraw(list, lParam, &result)) return result;
bool HandleHeaderCustomDraw(HWND listView, LPARAM lParam, LRESULT* result);

// ---------------------------------------------------------------------------
// Fonts / painting
// ---------------------------------------------------------------------------
HFONT MakeFont(int points, int weight, int dpi);
HBRUSH BgBrush();
HBRUSH InputBrush();

void FillRectC(HDC dc, const RECT& rc, COLORREF color);
void FillRoundRectC(HDC dc, const RECT& rc, int radius,
                    COLORREF fill, COLORREF border, int borderWidth = 1);
void DrawWrapped(HDC dc, const std::wstring& text, RECT rc, HFONT font,
                 COLORREF color, UINT flags);
void DrawTextCentered(HDC dc, const std::wstring& text, RECT rc, HFONT font,
                      COLORREF color);

// ---------------------------------------------------------------------------
// Owner-drawn buttons
// ---------------------------------------------------------------------------
// `slidersIcon` replaces the label with a three-slider "settings" glyph.
void StyleButton(HWND button, bool primary = false, bool danger = false,
                 COLORREF background = kBg, bool slidersIcon = false);
void DrawOwnerButton(const DRAWITEMSTRUCT* dis);

// ---------------------------------------------------------------------------
// Window helpers
// ---------------------------------------------------------------------------
void CenterWindowOn(HWND window, HWND owner);
void PaintWindowBackground(HWND hwnd, HDC dc);

// Double-buffered WM_PAINT helper.
class BufferedPainter {
public:
    explicit BufferedPainter(HWND hwnd);
    ~BufferedPainter();

    BufferedPainter(const BufferedPainter&) = delete;
    BufferedPainter& operator=(const BufferedPainter&) = delete;

    HDC dc() const { return mem_; }
    const RECT& client() const { return rc_; }

private:
    HWND          hwnd_   = nullptr;
    PAINTSTRUCT   ps_{};
    HDC           target_ = nullptr;
    HDC           mem_    = nullptr;
    HBITMAP       bmp_    = nullptr;
    HGDIOBJ       old_    = nullptr;
    RECT          rc_{};
};

// ---------------------------------------------------------------------------
// Text helpers
// ---------------------------------------------------------------------------
std::wstring Widen(const std::string& text);
std::wstring SizeText(uintmax_t bytes);
std::wstring TimestampText(const std::string& timestamp);
std::wstring FormatText(const wchar_t* fmt, ...);

} // namespace ui
