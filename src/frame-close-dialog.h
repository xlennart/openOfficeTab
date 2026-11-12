#pragma once
#include "dialog-theme.h"

namespace framecloseui {
constexpr int rememberId = 101, titleId = 110, summaryId = 111, questionId = 112, saveNoteId = 113;
constexpr int currentId = 201, groupId = 202;
constexpr int width = 500, height = 278, footerTop = 216;

struct Choice {
    size_t count;
    int language;
    Host host;
    bool dark;
    bool remember = false;
    dialogui::Theme theme{};
};

struct alignas(DWORD) Template {
    DLGTEMPLATE dialog{};
    WORD menu = 0, cls = 0, title = 0;
    Template() {
        dialog.style = WS_POPUP | WS_CAPTION | WS_SYSMENU | WS_CLIPCHILDREN | DS_MODALFRAME;
        dialog.cx = 300;
        dialog.cy = 160;
    }
};

inline void layout(HWND dialog, Choice& choice) {
    auto& theme = choice.theme;
    theme.initialize(dialog, choice.host, choice.dark, choice.language);
    theme.applyFonts(dialog, titleId);
    theme.place(dialog, titleId, 80, 23, 396, 30);
    theme.place(dialog, summaryId, 80, 57, 396, 22);
    theme.place(dialog, questionId, 24, 101, 452, 22);
    theme.place(dialog, saveNoteId, 24, 130, 452, 42);
    theme.place(dialog, rememberId, 24, 177, 452, 26);
    theme.place(dialog, currentId, 24, 230, 130, 34);
    theme.place(dialog, groupId, 164, 230, 160, 34);
    theme.place(dialog, IDCANCEL, 360, 230, 116, 34);
    InvalidateRect(dialog, nullptr, TRUE);
}

inline void paintIcon(HDC dc, const dialogui::Theme& theme) {
    dialogui::rounded(dc, theme.rect(24, 24, 40, 48), theme.footer, theme.border, theme.px(8));
    dialogui::rounded(dc, theme.rect(35, 39, 17, 15), theme.footer, theme.accent, theme.px(4));
    dialogui::rounded(dc, theme.rect(39, 43, 17, 15), theme.footer, theme.accent, theme.px(4));
    auto pen = CreatePen(PS_SOLID, theme.px(1), theme.accent);
    auto oldPen = SelectObject(dc, pen);
    MoveToEx(dc, theme.px(45), theme.px(48), nullptr);
    LineTo(dc, theme.px(51), theme.px(54));
    MoveToEx(dc, theme.px(50), theme.px(48), nullptr);
    LineTo(dc, theme.px(44), theme.px(54));
    SelectObject(dc, oldPen);
    DeleteObject(pen);
}

inline INT_PTR CALLBACK closeProc(HWND dialog, UINT message, WPARAM wp, LPARAM lp) {
    auto choice = (Choice*)GetWindowLongPtrW(dialog, DWLP_USER);
    if (message == WM_INITDIALOG) {
        choice = (Choice*)lp;
        SetWindowLongPtrW(dialog, DWLP_USER, lp);
        SetWindowTextW(dialog, L"openOfficeTab");
        auto tr = [&](const wchar_t* text) { return i18n::text(choice->language, text); };
        auto control = [&](const wchar_t* type, const wchar_t* text, DWORD style, int id) {
            return CreateWindowExW(0, type, text, WS_CHILD | WS_VISIBLE | style, 0, 0, 0, 0,
                dialog, (HMENU)(INT_PTR)id, module, nullptr);
        };
        control(L"STATIC", tr(L"关闭文档窗口"), SS_NOPREFIX, titleId);
        auto summary = i18n::closeSummary(choice->language, choice->count);
        control(L"STATIC", summary.c_str(), SS_NOPREFIX, summaryId);
        control(L"STATIC", tr(L"关闭当前文档，还是关闭本组全部文档？"), SS_NOPREFIX, questionId);
        control(L"STATIC", tr(L"未保存的更改仍由 Office 询问；取消保存会停止后续关闭。"), SS_NOPREFIX, saveNoteId);
        control(L"BUTTON", tr(L"记住我的选择（可在标签设置中更改）"), BS_AUTOCHECKBOX | WS_TABSTOP, rememberId);
        auto button = [&](const wchar_t* label, int id) {
            HWND child = control(L"BUTTON", tr(label), BS_OWNERDRAW | WS_TABSTOP, id);
            SetWindowSubclass(child, dialogui::controlProc, 1, 0);
        };
        button(L"关闭当前", currentId);
        button(L"关闭本组全部", groupId);
        button(L"取消", IDCANCEL);
        layout(dialog, *choice);
        choice->theme.center(dialog, width, height);
        choice->theme.caption(dialog);
        SendMessageW(dialog, DM_SETDEFID, IDCANCEL, 0);
        SetFocus(GetDlgItem(dialog, IDCANCEL));
        return FALSE;
    }
    if (!choice) return FALSE;
    auto& theme = choice->theme;
    switch (message) {
    case WM_ERASEBKGND:
        return TRUE;
    case WM_CTLCOLORSTATIC:
    case WM_CTLCOLORBTN: {
        int id = GetDlgCtrlID((HWND)lp);
        return theme.textColors((HDC)wp, id == summaryId || id == saveNoteId);
    }
    case WM_PAINT:
    case WM_PRINTCLIENT: {
        PAINTSTRUCT paint{};
        HDC dc = message == WM_PAINT ? BeginPaint(dialog, &paint) : (HDC)wp;
        theme.paintSurface(dialog, dc, footerTop);
        paintIcon(dc, theme);
        if (message == WM_PAINT) EndPaint(dialog, &paint);
        return TRUE;
    }
    case WM_DRAWITEM:
        if (wp == currentId || wp == groupId || wp == IDCANCEL) {
            theme.paintButton(*(DRAWITEMSTRUCT*)lp, wp == IDCANCEL);
            return TRUE;
        }
        break;
    case WM_DPICHANGED: {
        auto bounds = (RECT*)lp;
        SetWindowPos(dialog, nullptr, bounds->left, bounds->top, bounds->right - bounds->left,
            bounds->bottom - bounds->top, SWP_NOZORDER | SWP_NOACTIVATE);
        layout(dialog, *choice);
        return TRUE;
    }
    case WM_COMMAND: {
        int id = LOWORD(wp);
        if (id == currentId || id == groupId) {
            choice->remember = IsDlgButtonChecked(dialog, rememberId) == BST_CHECKED;
            EndDialog(dialog, id == currentId ? 1 : 2);
            return TRUE;
        }
        if (id != IDCANCEL) break;
        [[fallthrough]];
    }
    case WM_CLOSE:
        EndDialog(dialog, 0);
        return TRUE;
    }
    return FALSE;
}

inline FrameCloseChoice prompt(HWND owner, size_t count, int language, Host host, bool dark) {
    Choice choice{count, language, host, dark};
    Template layout;
    auto result = DialogBoxIndirectParamW(module, &layout.dialog, owner, closeProc, (LPARAM)&choice);
    return {(result == 1 || result == 2) ? (int)result : 0, choice.remember};
}
}
