#pragma once

#include <windows.h>
#include <commctrl.h>
#include <string>

namespace ui {

// ---------------------------------------------------------------------------
// Palette
// ---------------------------------------------------------------------------
inline constexpr COLORREF kBg        = RGB(30, 30, 30);
inline constexpr COLORREF kBgDeep    = RGB(22, 22, 23);
inline constexpr COLORREF kPanel     = RGB(37, 37, 38);
inline constexpr COLORREF kPanelAlt  = RGB(43, 43, 45);
inline constexpr COLORREF kInput     = RGB(28, 28, 30);
inline constexpr COLORREF kBorder    = RGB(58, 58, 62);
inline constexpr COLORREF kBorderHot = RGB(92, 92, 98);
inline constexpr COLORREF kText      = RGB(232, 232, 234);
inline constexpr COLORREF kMuted     = RGB(150, 150, 156);
inline constexpr COLORREF kAccent    = RGB(228, 59, 68);
inline constexpr COLORREF kAccentHot = RGB(241, 82, 91);
inline constexpr COLORREF kSuccess   = RGB(152, 195, 121);
inline constexpr COLORREF kDanger    = RGB(230, 112, 120);
inline constexpr COLORREF kSel       = RGB(58, 42, 45);
inline constexpr COLORREF kWarnBg    = RGB(52, 30, 33);
inline constexpr COLORREF kWarnLine  = RGB(116, 48, 54);

// ---------------------------------------------------------------------------
// DPI helpers
// ---------------------------------------------------------------------------
int DpiOf(HWND hwnd);
int S(HWND hwnd, int value);

// ---------------------------------------------------------------------------
// Dark theming
// ---------------------------------------------------------------------------
void EnableDarkTitleBar(HWND hwnd);
void ApplyDarkListView(HWND listView);
void ApplyDarkEdit(HWND edit);
void ApplyDarkTree(HWND hwnd);

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
void StyleButton(HWND button, bool primary = false, bool danger = false,
                 COLORREF background = kBg);
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
