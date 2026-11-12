#pragma once
#include "model.h"

namespace dialogui {
inline void fill(HDC dc, const RECT& bounds, COLORREF color) {
    auto brush = CreateSolidBrush(color);
    FillRect(dc, &bounds, brush);
    DeleteObject(brush);
}

inline void rounded(HDC dc, RECT bounds, COLORREF fillColor, COLORREF line, int radius, int thickness = 1) {
    auto brush = CreateSolidBrush(fillColor);
    auto pen = CreatePen(PS_SOLID, thickness, line);
    auto oldBrush = SelectObject(dc, brush);
    auto oldPen = SelectObject(dc, pen);
    RoundRect(dc, bounds.left, bounds.top, bounds.right, bounds.bottom, radius, radius);
    SelectObject(dc, oldPen);
    SelectObject(dc, oldBrush);
    DeleteObject(pen);
    DeleteObject(brush);
}

inline LRESULT CALLBACK controlProc(HWND window, UINT message, WPARAM wp, LPARAM lp, UINT_PTR, DWORD_PTR) {
    if (message == WM_SETFOCUS || message == WM_KILLFOCUS) {
        InvalidateRect(GetParent(window), nullptr, FALSE);
        InvalidateRect(window, nullptr, FALSE);
    }
    if (message == WM_MOUSEMOVE) {
        TRACKMOUSEEVENT track{sizeof(track), TME_LEAVE, window, 0};
        TrackMouseEvent(&track);
        InvalidateRect(window, nullptr, FALSE);
    }
    if (message == WM_MOUSELEAVE) InvalidateRect(window, nullptr, FALSE);
    if (message == WM_NCDESTROY) RemoveWindowSubclass(window, controlProc, 1);
    return DefSubclassProc(window, message, wp, lp);
}

struct Theme {
    int scale = 96;
    bool dark = false, contrast = false;
    COLORREF background = 0, foreground = 0, muted = 0, field = 0, border = 0, accent = 0, footer = 0;
    HFONT body = nullptr, title = nullptr;
    HBRUSH backgroundBrush = nullptr, fieldBrush = nullptr;

    Theme() = default;
    Theme(const Theme&) = delete;
    Theme& operator=(const Theme&) = delete;
    ~Theme() { clear(); }

    int px(int value) const { return MulDiv(value, scale, 96); }
    RECT rect(int x, int y, int width, int height) const {
        return {px(x), px(y), px(x + width), px(y + height)};
    }

    void initialize(HWND dialog, Host host, bool darkMode, int language) {
        clear();
        scale = dpi(dialog);
        HIGHCONTRASTW highContrast{sizeof(highContrast)};
        SystemParametersInfoW(SPI_GETHIGHCONTRAST, sizeof(highContrast), &highContrast, 0);
        contrast = (highContrast.dwFlags & HCF_HIGHCONTRASTON) != 0;
        dark = darkMode;
        background = dark ? RGB(32,32,32) : RGB(255,255,255);
        foreground = dark ? RGB(245,245,245) : RGB(32,35,40);
        muted = dark ? RGB(179,184,193) : RGB(100,108,120);
        field = dark ? RGB(43,43,43) : RGB(255,255,255);
        border = dark ? RGB(91,94,101) : RGB(204,210,219);
        footer = dark ? RGB(27,27,27) : RGB(247,248,250);
        switch (host) {
        case Host::Excel: accent = dark ? RGB(82,191,134) : RGB(16,124,65); break;
        case Host::PowerPoint: accent = dark ? RGB(245,142,113) : RGB(196,59,28); break;
        default: accent = dark ? RGB(103,163,247) : RGB(35,91,180); break;
        }
        if (contrast) {
            background = footer = field = GetSysColor(COLOR_WINDOW);
            foreground = muted = border = GetSysColor(COLOR_WINDOWTEXT);
            accent = GetSysColor(COLOR_HIGHLIGHT);
        }
        const auto face = i18n::resolve(language) == 1 ? L"Microsoft YaHei UI" : L"Segoe UI";
        auto font = [&](int size, int weight) {
            return CreateFontW(-px(size), 0, 0, 0, weight, FALSE, FALSE, FALSE,
                DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY, DEFAULT_PITCH, face);
        };
        body = font(14, FW_NORMAL);
        title = font(21, FW_SEMIBOLD);
        backgroundBrush = CreateSolidBrush(background);
        fieldBrush = CreateSolidBrush(field);
    }

    void applyFonts(HWND dialog, int titleId) const {
        for (HWND child = GetWindow(dialog, GW_CHILD); child; child = GetWindow(child, GW_HWNDNEXT)) {
            SendMessageW(child, WM_SETFONT, (WPARAM)(GetDlgCtrlID(child) == titleId ? title : body), TRUE);
        }
    }

    void place(HWND dialog, int id, int x, int y, int width, int height) const {
        MoveWindow(GetDlgItem(dialog, id), px(x), px(y), px(width), px(height), TRUE);
    }

    void center(HWND dialog, int width, int height) const {
        RECT frame = rect(0, 0, width, height);
        AdjustWindowRectEx(&frame, (DWORD)GetWindowLongPtrW(dialog, GWL_STYLE), FALSE,
            (DWORD)GetWindowLongPtrW(dialog, GWL_EXSTYLE));
        HWND owner = GetParent(dialog);
        MONITORINFO monitor{sizeof(monitor)};
        GetMonitorInfoW(MonitorFromWindow(owner ? owner : dialog, MONITOR_DEFAULTTONEAREST), &monitor);
        RECT parent = monitor.rcWork;
        if (owner) GetWindowRect(owner, &parent);
        width = frame.right - frame.left;
        height = frame.bottom - frame.top;
        int x = std::clamp(parent.left + (parent.right - parent.left - width) / 2,
            monitor.rcWork.left, std::max(monitor.rcWork.left, monitor.rcWork.right - width));
        int y = std::clamp(parent.top + (parent.bottom - parent.top - height) / 2,
            monitor.rcWork.top, std::max(monitor.rcWork.top, monitor.rcWork.bottom - height));
        SetWindowPos(dialog, nullptr, x, y, width, height, SWP_NOZORDER | SWP_NOACTIVATE);
    }

    void caption(HWND dialog) const {
        // Keep the standard caption on systems without dark caption support.
        auto dwm = LoadLibraryW(L"dwmapi.dll");
        if (!dwm) return;
        using SetAttribute = HRESULT(WINAPI*)(HWND, DWORD, LPCVOID, DWORD);
        auto set = (SetAttribute)GetProcAddress(dwm, "DwmSetWindowAttribute");
        BOOL darkCaption = dark && !contrast;
        if (set) set(dialog, 20, &darkCaption, sizeof(darkCaption));
        FreeLibrary(dwm);
    }

    INT_PTR textColors(HDC dc, bool secondary, bool inField = false) const {
        SetBkMode(dc, TRANSPARENT);
        SetTextColor(dc, secondary ? muted : foreground);
        SetBkColor(dc, inField ? field : background);
        return (INT_PTR)(inField ? fieldBrush : backgroundBrush);
    }

    void paintSurface(HWND dialog, HDC dc, int footerTop) const {
        RECT bounds{};
        GetClientRect(dialog, &bounds);
        FillRect(dc, &bounds, backgroundBrush);
        bounds.top = px(footerTop);
        fill(dc, bounds, footer);
    }

    void paintButton(const DRAWITEMSTRUCT& item, bool primary, bool inFooter = true) const {
        HDC dc = item.hDC;
        bool disabled = (item.itemState & ODS_DISABLED) != 0;
        bool pressed = (item.itemState & ODS_SELECTED) != 0;
        POINT cursor{};
        GetCursorPos(&cursor);
        ScreenToClient(item.hwndItem, &cursor);
        bool hover = PtInRect(&item.rcItem, cursor) != FALSE;
        COLORREF surface = primary ? accent : field;
        if (!primary && (hover || pressed)) surface = footer;
        if (primary && (hover || pressed) && !contrast) {
            int percent = pressed ? 20 : 10;
            auto tint = [&](int value) { return (value * (100 - percent) + (dark ? 255 : 0) * percent) / 100; };
            surface = RGB(tint(GetRValue(accent)), tint(GetGValue(accent)), tint(GetBValue(accent)));
        }
        if (disabled) surface = border;
        fill(dc, item.rcItem, inFooter ? footer : background);
        RECT bounds = item.rcItem;
        InflateRect(&bounds, -px(1), -px(1));
        rounded(dc, bounds, surface, primary ? surface : border, px(10));
        COLORREF primaryText = dark ? RGB(20,26,34) : RGB(255,255,255);
        if (contrast) primaryText = GetSysColor(COLOR_HIGHLIGHTTEXT);
        SetBkMode(dc, TRANSPARENT);
        SetTextColor(dc, disabled ? muted : primary ? primaryText : foreground);
        auto oldFont = SelectObject(dc, body);
        wchar_t text[80]{};
        GetWindowTextW(item.hwndItem, text, 80);
        DrawTextW(dc, text, -1, &bounds, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
        SelectObject(dc, oldFont);
        if ((item.itemState & ODS_FOCUS) && !(item.itemState & ODS_NOFOCUSRECT)) {
            InflateRect(&bounds, -px(4), -px(4));
            DrawFocusRect(dc, &bounds);
        }
    }

private:
    void clear() {
        if (body) DeleteObject(body);
        if (title) DeleteObject(title);
        if (backgroundBrush) DeleteObject(backgroundBrush);
        if (fieldBrush) DeleteObject(fieldBrush);
    }
};
}
