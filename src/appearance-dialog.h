#pragma once
#include "dialog-theme.h"
#include "layout.h"
#include "tab-color.h"
#include <commdlg.h>

namespace appearanceui {
constexpr int editId = 101, modeId = 102, paletteId = 103, resetId = 104;
constexpr int titleId = 110, subtitleId = 111, widthLabelId = 112, modeLabelId = 113, noteId = 114;
constexpr int width = 480, height = 328, footerTop = 264;

struct alignas(DWORD) Template {
    DLGTEMPLATE dialog{};
    WORD menu = 0, cls = 0, title = 0;
    Template() {
        dialog.style = WS_POPUP | WS_CAPTION | WS_SYSMENU | WS_CLIPCHILDREN | DS_MODALFRAME;
        dialog.cx = 290;
        dialog.cy = 175;
    }
};

struct Dialog {
    int language;
    Host host;
    bool dark;
    dialogui::Theme theme{};

    const wchar_t* tr(const wchar_t* text) const { return i18n::text(language, text); }
    HWND control(HWND dialog, const wchar_t* type, const wchar_t* text, DWORD style, int id) const {
        return CreateWindowExW(0, type, text, WS_CHILD | WS_VISIBLE | style, 0, 0, 0, 0,
            dialog, (HMENU)(INT_PTR)id, module, nullptr);
    }
    void button(HWND dialog, const wchar_t* label, int id) const {
        HWND child = control(dialog, L"BUTTON", tr(label), BS_OWNERDRAW | WS_TABSTOP, id);
        SetWindowSubclass(child, dialogui::controlProc, 1, 0);
    }
    void heading(HWND dialog, const wchar_t* title, const wchar_t* subtitle) const {
        SetWindowTextW(dialog, L"openOfficeTab");
        control(dialog, L"STATIC", tr(title), SS_NOPREFIX, titleId);
        control(dialog, L"STATIC", tr(subtitle), SS_NOPREFIX, subtitleId);
    }
    void footer(HWND dialog) const {
        button(dialog, L"取消", IDCANCEL);
        button(dialog, L"确定", IDOK);
    }
    void layout(HWND dialog) {
        theme.initialize(dialog, host, dark, language);
        theme.applyFonts(dialog, titleId);
        theme.place(dialog, titleId, 80, 23, 376, 30);
        theme.place(dialog, subtitleId, 80, 57, 376, 36);
        theme.place(dialog, IDCANCEL, 256, 278, 92, 34);
        theme.place(dialog, IDOK, 360, 278, 96, 34);
    }
    void focusEdit(HWND dialog) const {
        theme.center(dialog, width, height);
        theme.caption(dialog);
        SendMessageW(dialog, DM_SETDEFID, IDOK, 0);
        SetFocus(GetDlgItem(dialog, editId));
        SendDlgItemMessageW(dialog, editId, EM_SETSEL, 0, -1);
    }
    void paint(HWND dialog, HDC dc, const RECT& fieldBounds) const {
        theme.paintSurface(dialog, dc, footerTop);
        bool focused = GetFocus() == GetDlgItem(dialog, editId);
        dialogui::rounded(dc, fieldBounds, theme.field, focused ? theme.accent : theme.border,
            theme.px(12), theme.px(focused ? 2 : 1));
        dialogui::rounded(dc, theme.rect(24, 24, 40, 48), theme.footer, theme.border, theme.px(8));
    }
    INT_PTR standardMessage(HWND dialog, UINT message, WPARAM wp, LPARAM lp) const {
        switch (message) {
        case WM_ERASEBKGND:
            return TRUE;
        case WM_CTLCOLORSTATIC:
        case WM_CTLCOLOREDIT:
        case WM_CTLCOLORLISTBOX: {
            int id = GetDlgCtrlID((HWND)lp);
            return theme.textColors((HDC)wp, id == subtitleId || id == noteId,
                id == editId || message == WM_CTLCOLORLISTBOX);
        }
        case WM_DRAWITEM:
            if (wp == IDOK || wp == IDCANCEL || wp == paletteId || wp == resetId) {
                theme.paintButton(*(DRAWITEMSTRUCT*)lp, wp == IDOK, wp == IDOK || wp == IDCANCEL);
                return TRUE;
            }
            break;
        case WM_COMMAND:
            if (LOWORD(wp) != IDCANCEL) break;
            [[fallthrough]];
        case WM_CLOSE:
            EndDialog(dialog, IDCANCEL);
            return TRUE;
        }
        return FALSE;
    }
};

struct WidthChoice {
    int width, mode;
    Dialog ui;
};

struct ColorChoice {
    COLORREF color;
    bool custom;
    Dialog ui;
};

inline void resize(HWND dialog, const RECT& bounds) {
    SetWindowPos(dialog, nullptr, bounds.left, bounds.top, bounds.right - bounds.left,
        bounds.bottom - bounds.top, SWP_NOZORDER | SWP_NOACTIVATE);
}

inline void widthLayout(HWND dialog, Dialog& ui) {
    ui.layout(dialog);
    auto& theme = ui.theme;
    theme.place(dialog, widthLabelId, 24, 101, 432, 22);
    theme.place(dialog, editId, 38, 136, 404, 24);
    theme.place(dialog, modeLabelId, 24, 183, 432, 22);
    SendDlgItemMessageW(dialog, modeId, CB_SETITEMHEIGHT, (WPARAM)-1, theme.px(36));
    SendDlgItemMessageW(dialog, modeId, CB_SETITEMHEIGHT, 0, theme.px(28));
    theme.place(dialog, modeId, 24, 208, 432, 130);
    InvalidateRect(dialog, nullptr, TRUE);
}

inline void drawMode(const DRAWITEMSTRUCT& item, const dialogui::Theme& theme) {
    bool selected = (item.itemState & ODS_SELECTED) && !(item.itemState & ODS_COMBOBOXEDIT);
    dialogui::fill(item.hDC, item.rcItem, selected ? theme.accent : theme.field);
    if (item.itemID == (UINT)-1) return;
    wchar_t text[128]{};
    // The list contains only the three localized mode labels created below.
    if (SendMessageW(item.hwndItem, CB_GETLBTEXTLEN, item.itemID, 0) >= (LRESULT)std::size(text)) return;
    if (SendMessageW(item.hwndItem, CB_GETLBTEXT, item.itemID, (LPARAM)text) == CB_ERR) return;
    auto font = SelectObject(item.hDC, theme.body);
    SetBkMode(item.hDC, TRANSPARENT);
    COLORREF selectedText = theme.contrast ? GetSysColor(COLOR_HIGHLIGHTTEXT) :
        (theme.dark ? RGB(20,26,34) : RGB(255,255,255));
    SetTextColor(item.hDC, selected ? selectedText : theme.foreground);
    RECT bounds = item.rcItem;
    bounds.left += theme.px(10);
    DrawTextW(item.hDC, text, -1, &bounds, DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS | DT_NOPREFIX);
    SelectObject(item.hDC, font);
    if ((item.itemState & ODS_FOCUS) && !(item.itemState & ODS_NOFOCUSRECT)) DrawFocusRect(item.hDC, &item.rcItem);
}

// Keep the native combo and its popup for input and accessibility. Paint the
// closed field so the legacy button and frame do not clash with dark mode.
inline LRESULT CALLBACK modeProc(HWND combo, UINT message, WPARAM wp, LPARAM lp, UINT_PTR, DWORD_PTR data) {
    const auto& theme = *(const dialogui::Theme*)data;
    if (message == WM_PAINT || message == WM_PRINTCLIENT) {
        PAINTSTRUCT paint{};
        HDC dc = message == WM_PAINT ? BeginPaint(combo, &paint) : (HDC)wp;
        RECT bounds{};
        GetClientRect(combo, &bounds);
        dialogui::fill(dc, bounds, theme.background);
        bool focused = GetFocus() == combo;
        dialogui::rounded(dc, bounds, theme.field, focused ? theme.accent : theme.border,
            theme.px(12), theme.px(focused ? 2 : 1));

        DRAWITEMSTRUCT item{};
        item.itemID = (UINT)SendMessageW(combo, CB_GETCURSEL, 0, 0);
        item.itemState = ODS_COMBOBOXEDIT;
        item.hwndItem = combo;
        item.hDC = dc;
        item.rcItem = {theme.px(2), theme.px(3), bounds.right - theme.px(38), bounds.bottom - theme.px(3)};
        drawMode(item, theme);

        int x = bounds.right - theme.px(21), y = bounds.bottom / 2;
        auto pen = CreatePen(PS_SOLID, theme.px(2), focused ? theme.accent : theme.foreground);
        auto oldPen = SelectObject(dc, pen);
        MoveToEx(dc, x - theme.px(4), y - theme.px(2), nullptr);
        LineTo(dc, x, y + theme.px(2));
        LineTo(dc, x + theme.px(4), y - theme.px(2));
        SelectObject(dc, oldPen);
        DeleteObject(pen);
        if (message == WM_PAINT) EndPaint(combo, &paint);
        return 0;
    }
    if (message == WM_NCDESTROY) RemoveWindowSubclass(combo, modeProc, 1);
    auto result = DefSubclassProc(combo, message, wp, lp);
    if (message == WM_SETFOCUS || message == WM_KILLFOCUS || message == CB_SETCURSEL) {
        InvalidateRect(combo, nullptr, FALSE);
    }
    return result;
}

inline INT_PTR CALLBACK widthProc(HWND dialog, UINT message, WPARAM wp, LPARAM lp) {
    auto choice = (WidthChoice*)GetWindowLongPtrW(dialog, DWLP_USER);
    if (message == WM_INITDIALOG) {
        choice = (WidthChoice*)lp;
        SetWindowLongPtrW(dialog, DWLP_USER, lp);
        auto& ui = choice->ui;
        ui.heading(dialog, L"标签宽度", L"标签宽度（80–360，按缩放自动适配）");
        ui.control(dialog, L"STATIC", ui.tr(L"默认宽度"), SS_NOPREFIX, widthLabelId);
        HWND edit = ui.control(dialog, L"EDIT", std::to_wstring(choice->width).c_str(),
            WS_TABSTOP | ES_NUMBER | ES_AUTOHSCROLL, editId);
        SetWindowSubclass(edit, dialogui::controlProc, 1, 0);
        ui.control(dialog, L"STATIC", ui.tr(L"宽度模式"), SS_NOPREFIX, modeLabelId);
        HWND combo = ui.control(dialog, L"COMBOBOX", L"", WS_TABSTOP | CBS_DROPDOWNLIST |
            CBS_OWNERDRAWFIXED | CBS_HASSTRINGS, modeId);
        for (auto text : {L"固定宽度", L"按标题长度（上述宽度为上限）", L"空间不足时压缩（上述宽度为上限）"}) {
            SendMessageW(combo, CB_ADDSTRING, 0, (LPARAM)ui.tr(text));
        }
        SendMessageW(combo, CB_SETCURSEL, choice->mode, 0);
        ui.footer(dialog);
        widthLayout(dialog, ui);
        SetWindowSubclass(combo, modeProc, 1, (DWORD_PTR)&ui.theme);
        ui.focusEdit(dialog);
        return FALSE;
    }
    if (!choice) return FALSE;
    auto& ui = choice->ui;
    if (message == WM_COMMAND && LOWORD(wp) == modeId && HIWORD(wp) == CBN_SELCHANGE) {
        InvalidateRect(GetDlgItem(dialog, modeId), nullptr, FALSE);
        return TRUE;
    }
    if (message == WM_DRAWITEM && wp == modeId) {
        drawMode(*(DRAWITEMSTRUCT*)lp, ui.theme);
        return TRUE;
    }
    if (message == WM_PAINT || message == WM_PRINTCLIENT) {
        PAINTSTRUCT paint{};
        HDC dc = message == WM_PAINT ? BeginPaint(dialog, &paint) : (HDC)wp;
        auto& theme = ui.theme;
        ui.paint(dialog, dc, theme.rect(24, 126, 432, 44));
        dialogui::rounded(dc, theme.rect(34, 42, 20, 12), theme.footer, theme.accent, theme.px(4));
        for (int x : {39, 44, 49}) dialogui::fill(dc, theme.rect(x, 43, 1, 5), theme.accent);
        if (message == WM_PAINT) EndPaint(dialog, &paint);
        return TRUE;
    }
    if (message == WM_DPICHANGED) {
        resize(dialog, *(RECT*)lp);
        widthLayout(dialog, ui);
        return TRUE;
    }
    if (message == WM_COMMAND && LOWORD(wp) == IDOK) {
        BOOL valid = FALSE;
        int value = (int)GetDlgItemInt(dialog, editId, &valid, FALSE);
        int mode = (int)SendDlgItemMessageW(dialog, modeId, CB_GETCURSEL, 0, 0);
        if (!valid || !tablayout::validWidth(value, mode)) {
            MessageBoxW(dialog, ui.tr(L"请输入 80 到 360 之间的整数。"), ui.tr(L"标签宽度"), MB_OK | MB_ICONINFORMATION);
            return TRUE;
        }
        choice->width = value;
        choice->mode = mode;
        EndDialog(dialog, IDOK);
        return TRUE;
    }
    return ui.standardMessage(dialog, message, wp, lp);
}

inline void colorLayout(HWND dialog, Dialog& ui) {
    ui.layout(dialog);
    auto& theme = ui.theme;
    theme.place(dialog, editId, 38, 125, 204, 24);
    theme.place(dialog, paletteId, 336, 120, 120, 34);
    theme.place(dialog, resetId, 24, 175, 120, 34);
    theme.place(dialog, noteId, 24, 222, 432, 38);
    InvalidateRect(dialog, nullptr, TRUE);
}

inline bool inputColor(HWND dialog, COLORREF& value) {
    wchar_t text[32]{};
    GetDlgItemTextW(dialog, editId, text, (int)std::size(text));
    return tabcolor::parse(text, value);
}

inline INT_PTR CALLBACK colorProc(HWND dialog, UINT message, WPARAM wp, LPARAM lp) {
    auto choice = (ColorChoice*)GetWindowLongPtrW(dialog, DWLP_USER);
    if (message == WM_INITDIALOG) {
        choice = (ColorChoice*)lp;
        SetWindowLongPtrW(dialog, DWLP_USER, lp);
        auto& ui = choice->ui;
        bool custom = choice->custom;
        ui.heading(dialog, L"标签栏颜色", L"输入颜色（#RRGGBB），或使用调色板。");
        HWND edit = ui.control(dialog, L"EDIT", tabcolor::format(choice->color).c_str(),
            WS_TABSTOP | ES_AUTOHSCROLL, editId);
        SendMessageW(edit, EM_SETLIMITTEXT, 7, 0);
        SetWindowSubclass(edit, dialogui::controlProc, 1, 0);
        ui.button(dialog, L"调色板…", paletteId);
        ui.button(dialog, L"恢复默认", resetId);
        ui.control(dialog, L"STATIC", ui.tr(L"深色模式和系统高对比度优先于自定义颜色。"), SS_NOPREFIX, noteId);
        ui.footer(dialog);
        colorLayout(dialog, ui);
        ui.focusEdit(dialog);
        choice->custom = custom;
        return FALSE;
    }
    if (!choice) return FALSE;
    auto& ui = choice->ui;
    if (message == WM_PAINT || message == WM_PRINTCLIENT) {
        PAINTSTRUCT paint{};
        HDC dc = message == WM_PAINT ? BeginPaint(dialog, &paint) : (HDC)wp;
        auto& theme = ui.theme;
        ui.paint(dialog, dc, theme.rect(24, 115, 236, 44));
        dialogui::rounded(dc, theme.rect(34, 38, 20, 20), theme.accent, theme.accent, theme.px(6));
        COLORREF swatch = theme.field;
        inputColor(dialog, swatch);
        dialogui::rounded(dc, theme.rect(276, 115, 44, 44), swatch, theme.border, theme.px(12));
        if (message == WM_PAINT) EndPaint(dialog, &paint);
        return TRUE;
    }
    if (message == WM_DPICHANGED) {
        resize(dialog, *(RECT*)lp);
        colorLayout(dialog, ui);
        return TRUE;
    }
    if (message == WM_COMMAND) {
        switch (LOWORD(wp)) {
        case editId:
            if (HIWORD(wp) == EN_CHANGE) {
                choice->custom = true;
                InvalidateRect(dialog, nullptr, FALSE);
            }
            break;
        case paletteId: {
            COLORREF color = choice->color, colors[16]{};
            inputColor(dialog, color);
            CHOOSECOLORW picker{sizeof(picker)};
            picker.hwndOwner = dialog;
            picker.rgbResult = color;
            picker.lpCustColors = colors;
            picker.Flags = CC_FULLOPEN | CC_RGBINIT;
            if (ChooseColorW(&picker)) SetDlgItemTextW(dialog, editId, tabcolor::format(picker.rgbResult).c_str());
            return TRUE;
        }
        case resetId:
            SetDlgItemTextW(dialog, editId, tabcolor::format(tabcolor::defaultBackground()).c_str());
            choice->custom = false;
            return TRUE;
        case IDOK:
            if (!inputColor(dialog, choice->color)) {
                MessageBoxW(dialog, ui.tr(L"请输入 6 位十六进制颜色，例如 #F0F0F0。"), ui.tr(L"标签栏颜色"), MB_OK | MB_ICONINFORMATION);
                return TRUE;
            }
            EndDialog(dialog, IDOK);
            return TRUE;
        }
    }
    return ui.standardMessage(dialog, message, wp, lp);
}
}
