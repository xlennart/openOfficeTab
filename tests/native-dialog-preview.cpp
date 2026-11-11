// Hidden native windows only. Render the real dialog; never starts Office.
#include "../src/frame-close-dialog.h"
#include "../src/appearance-dialog.h"
#include "../src/tab-menu.h"
#include <gdiplus.h>
#include <cassert>
#include <iostream>
void saveBitmap(HBITMAP bitmap, const wchar_t* path) {
    Gdiplus::Bitmap image(bitmap,nullptr);CLSID png{0x557cf406,0x1a04,0x11d3,{0x9a,0x73,0x00,0x00,0xf8,0x1e,0xf3,0x2e}};
    assert(image.Save(path,&png,nullptr)==Gdiplus::Ok);
}
void capture(HWND window,const wchar_t* path){
    RECT r{};GetClientRect(window,&r);HDC screen=GetDC(nullptr),dc=CreateCompatibleDC(screen);
    HBITMAP bitmap=CreateCompatibleBitmap(screen,r.right,r.bottom);auto old=SelectObject(dc,bitmap);
    SendMessageW(window,WM_PRINTCLIENT,(WPARAM)dc,PRF_CLIENT);
    for(HWND child=GetWindow(window,GW_CHILD);child;child=GetWindow(child,GW_HWNDNEXT)){
        RECT bounds{};GetWindowRect(child,&bounds);MapWindowPoints(nullptr,window,(POINT*)&bounds,2);
        int saved=SaveDC(dc);SetViewportOrgEx(dc,bounds.left,bounds.top,nullptr);
        IntersectClipRect(dc,0,0,bounds.right-bounds.left,bounds.bottom-bounds.top);
        SendMessageW(child,WM_PRINT,(WPARAM)dc,PRF_CLIENT|PRF_ERASEBKGND);RestoreDC(dc,saved);
    }
    saveBitmap(bitmap, path);
    SelectObject(dc,old);DeleteObject(bitmap);DeleteDC(dc);ReleaseDC(nullptr,screen);
}
void showOffscreen(HWND dialog) {
    assert(dialog && !IsWindowVisible(dialog));
    // Standard controls print their content once their hierarchy is visible.
    SetWindowLongPtrW(dialog, GWL_EXSTYLE, GetWindowLongPtrW(dialog, GWL_EXSTYLE) | WS_EX_NOACTIVATE | WS_EX_TOOLWINDOW);
    SetWindowPos(dialog, nullptr, -30000, -30000, 0, 0, SWP_NOSIZE | SWP_NOZORDER | SWP_NOACTIVATE);
    ShowWindow(dialog, SW_SHOWNOACTIVATE);
    UpdateWindow(dialog);
}

void checkBounds(HWND dialog) {
    RECT client{};
    GetClientRect(dialog, &client);
    for (HWND window = GetWindow(dialog, GW_CHILD); window; window = GetWindow(window, GW_HWNDNEXT)) {
        RECT child{};
        GetWindowRect(window, &child);
        MapWindowPoints(nullptr, dialog, (POINT*)&child, 2);
        assert(child.left >= 0 && child.top >= 0 && child.right <= client.right && child.bottom <= client.bottom);
    }
}

void previewFrameClose(Host host, bool dark, int language, const wchar_t* path) {
    framecloseui::Choice choice{2, language, host, dark};
    framecloseui::Template data;
    HWND dialog = CreateDialogIndirectParamW(module, &data.dialog, nullptr, framecloseui::closeProc, (LPARAM)&choice);
    showOffscreen(dialog);
    checkBounds(dialog);
    auto defaultId = SendMessageW(dialog, DM_GETDEFID, 0, 0);
    assert(HIWORD(defaultId) == DC_HASDEFID && LOWORD(defaultId) == IDCANCEL);
    HWND remember = GetDlgItem(dialog, framecloseui::rememberId);
    assert(SendMessageW(remember, BM_GETCHECK, 0, 0) == BST_UNCHECKED);
    capture(dialog, path);
    SendMessageW(remember, BM_CLICK, 0, 0);
    assert(SendMessageW(remember, BM_GETCHECK, 0, 0) == BST_CHECKED);
    DestroyWindow(dialog);
}

void previewWidth(Host host, bool dark, int language, const wchar_t* path) {
    appearanceui::WidthChoice choice{120, 2, {language, host, dark}};
    appearanceui::Template data;
    HWND dialog = CreateDialogIndirectParamW(module, &data.dialog, nullptr, appearanceui::widthProc, (LPARAM)&choice);
    showOffscreen(dialog);
    checkBounds(dialog);
    assert(!GetDlgItem(dialog, appearanceui::subtitleId));
    wchar_t label[64]{};
    assert(GetDlgItemTextW(dialog, appearanceui::widthLabelId, label, (int)std::size(label)) > 0);
    assert(std::wstring(label) == (language == 1 ? L"默认宽度（80–360）" : L"Default width (80–360)"));
    assert(SendDlgItemMessageW(dialog, appearanceui::modeId, CB_GETCOUNT, 0, 0) == 3);
    assert(SendDlgItemMessageW(dialog, appearanceui::modeId, CB_GETCURSEL, 0, 0) == 2);
    capture(dialog, path);
    SetDlgItemInt(dialog, appearanceui::editId, 240, FALSE);
    SendDlgItemMessageW(dialog, appearanceui::modeId, CB_SETCURSEL, 1, 0);
    SendMessageW(dialog, WM_COMMAND, IDOK, 0);
    assert(choice.width == 240 && choice.mode == 1);
    DestroyWindow(dialog);
}

void previewColor(Host host, bool dark, int language, const wchar_t* path) {
    appearanceui::ColorChoice choice{RGB(218,234,251), false, {language, host, dark}};
    appearanceui::Template data;
    HWND dialog = CreateDialogIndirectParamW(module, &data.dialog, nullptr, appearanceui::colorProc, (LPARAM)&choice);
    showOffscreen(dialog);
    checkBounds(dialog);
    assert(!choice.custom);
    SetDlgItemTextW(dialog, appearanceui::editId, L"#DBEAFE");
    assert(choice.custom);
    capture(dialog, path);
    SendMessageW(dialog, WM_COMMAND, appearanceui::resetId, 0);
    assert(!choice.custom);
    SendMessageW(dialog, WM_COMMAND, IDOK, 0);
    assert(choice.color == tabcolor::defaultBackground());
    DestroyWindow(dialog);
}

void checkTabMenu() {
    HWND owner = CreateWindowExW(WS_EX_TOOLWINDOW, L"STATIC", L"", WS_POPUP, 0, 0, 1, 1,
        nullptr, nullptr, module, nullptr);
    assert(owner);
    {
        View view;
        view.path = L"C:\\Examples\\Report.docx";
        view.locked = view.readOnly = true;
        tabmenu::Menu menu(owner, Host::Word, false, 2);
        tabmenu::populate(menu, view, 2);
        HMENU root = menu.handle();
        assert(GetMenuItemCount(root) == 12);
        assert(GetMenuState(root, 2, MF_BYCOMMAND) & MF_DISABLED);
        assert(GetMenuState(root, 5, MF_BYCOMMAND) & MF_DISABLED);
        assert(GetMenuState(root, 10, MF_BYCOMMAND) & MF_CHECKED);
        assert(!(GetMenuState(root, 11, MF_BYCOMMAND) & MF_DISABLED));
        HMENU move = GetSubMenu(root, 5), close = GetSubMenu(root, 9), more = GetSubMenu(root, 11);
        assert(move && close && more);
        assert(GetMenuItemCount(move) == 3 && GetMenuItemCount(close) == 4 && GetMenuItemCount(more) == 5);
        assert(GetMenuItemID(move, 0) == 13 && GetMenuItemID(move, 2) == 15);
        assert(GetMenuItemID(close, 0) == 6 && GetMenuItemID(close, 3) == 9);
        assert(GetMenuItemID(more, 0) == 1 && GetMenuItemID(more, 4) == 19);

        wchar_t label[48]{};
        MENUITEMINFOW item{sizeof(item)};
        item.fMask = MIIM_FTYPE | MIIM_STRING | MIIM_DATA;
        item.dwTypeData = label;
        item.cch = (UINT)std::size(label);
        assert(GetMenuItemInfoW(root, 2, FALSE, &item));
        assert(std::wstring(label) == i18n::text(2, L"保存"));
        if (item.fType & MFT_OWNERDRAW) {
            MEASUREITEMSTRUCT measure{ODT_MENU, 0, 2, 0, 0, item.dwItemData};
            assert(SendMessageW(owner, WM_MEASUREITEM, 0, (LPARAM)&measure));
            assert(measure.itemWidth > 100 && measure.itemHeight >= 30);
            HDC screen = GetDC(owner), dc = CreateCompatibleDC(screen);
            HBITMAP bitmap = CreateCompatibleBitmap(screen, measure.itemWidth, measure.itemHeight);
            auto previous = SelectObject(dc, bitmap);
            DRAWITEMSTRUCT draw{ODT_MENU, 0, 2, ODA_DRAWENTIRE, ODS_DISABLED, (HWND)root, dc,
                {0, 0, (LONG)measure.itemWidth, (LONG)measure.itemHeight}, item.dwItemData};
            assert(SendMessageW(owner, WM_DRAWITEM, 0, (LPARAM)&draw));
            SelectObject(dc, previous);
            DeleteObject(bitmap);
            DeleteDC(dc);
            ReleaseDC(owner, screen);
        }
    }
    DestroyWindow(owner);
}
void captureMenu(HWND owner, HMENU popup, const dialogui::Theme& theme, UINT selected, const wchar_t* path) {
    struct Row { MENUITEMINFOW info; UINT height; };
    std::vector<Row> rows;
    UINT width = 0, height = 0;
    for (int index = 0; index < GetMenuItemCount(popup); ++index) {
        MENUITEMINFOW info{sizeof(info)};
        info.fMask = MIIM_ID | MIIM_FTYPE | MIIM_DATA | MIIM_STATE | MIIM_SUBMENU;
        assert(GetMenuItemInfoW(popup, index, TRUE, &info));
        UINT rowHeight = theme.px(9);
        if (!(info.fType & MFT_SEPARATOR)) {
            if (!(info.fType & MFT_OWNERDRAW)) return;
            MEASUREITEMSTRUCT measure{ODT_MENU, 0, info.wID, 0, 0, info.dwItemData};
            assert(SendMessageW(owner, WM_MEASUREITEM, 0, (LPARAM)&measure));
            width = std::max(width, measure.itemWidth);
            rowHeight = measure.itemHeight;
        }
        rows.push_back({info, rowHeight});
        height += rowHeight;
    }
    assert(width && height);
    HDC screen = GetDC(owner), dc = CreateCompatibleDC(screen);
    HBITMAP bitmap = CreateCompatibleBitmap(screen, width, height);
    auto old = SelectObject(dc, bitmap);
    LONG top = 0;
    for (const auto& row : rows) {
        RECT bounds{0, top, (LONG)width, top + (LONG)row.height};
        const auto& info = row.info;
        if (info.fType & MFT_SEPARATOR) {
            dialogui::fill(dc, bounds, theme.background);
            RECT line{theme.px(4), top + (LONG)row.height / 2, (LONG)width - theme.px(4),
                top + (LONG)row.height / 2 + 1};
            dialogui::fill(dc, line, theme.border);
        } else {
            UINT state = (info.fState & MFS_CHECKED ? ODS_CHECKED : 0U) |
                (info.fState & MFS_DISABLED ? ODS_DISABLED : 0U) | (info.wID == selected ? ODS_SELECTED : 0U);
            DRAWITEMSTRUCT draw{ODT_MENU, 0, info.wID, ODA_DRAWENTIRE, state,
                (HWND)popup, dc, bounds, info.dwItemData};
            assert(SendMessageW(owner, WM_DRAWITEM, 0, (LPARAM)&draw));
            // The popup window supplies its submenu arrows after WM_DRAWITEM.
            if (info.hSubMenu) {
                int x = (int)width - theme.px(14), y = top + (int)row.height / 2;
                auto pen = CreatePen(PS_SOLID, theme.px(1), info.wID == selected ? theme.accent : theme.muted);
                auto previous = SelectObject(dc, pen);
                MoveToEx(dc, x, y - theme.px(3), nullptr);
                LineTo(dc, x + theme.px(3), y);
                LineTo(dc, x, y + theme.px(3));
                SelectObject(dc, previous);
                DeleteObject(pen);
            }
        }
        top = bounds.bottom;
    }
    saveBitmap(bitmap, path);
    SelectObject(dc, old);
    DeleteObject(bitmap);
    DeleteDC(dc);
    ReleaseDC(owner, screen);
}

void previewRecentMenu(HWND owner, bool dark, const wchar_t* path) {
    {
        tabmenu::Menu empty(owner, Host::Word, dark, 1);
        tabmenu::populateRecent(empty, {}, 1);
        assert(GetMenuItemCount(empty.handle()) == 3);
        assert(GetMenuState(empty.handle(), tabmenu::clearRecentId, MF_BYCOMMAND) & MF_DISABLED);
    }
    tabmenu::Menu menu(owner, Host::Word, dark, 1);
    const std::vector<std::wstring> files{L"D:\\项目资料\\本周\\项目进度报告.docx",
        L"D:\\项目资料\\上周\\项目进度报告.docx", L"C:\\工作资料\\R&D\\会议纪要.docx",
        L"C:\\方案归档\\项目资料\\已提交的版本\\季度汇报\\会议用材料\\年度计划.docx"};
    tabmenu::populateRecent(menu, files, 1);
    HMENU root = menu.handle();
    assert(GetMenuItemCount(root) == (int)files.size() + 2);
    assert(!(GetMenuState(root, tabmenu::clearRecentId, MF_BYCOMMAND) & MF_DISABLED));

    dialogui::Theme theme;
    theme.initialize(owner, Host::Word, dark, 1);
    for (int index = 0; index < GetMenuItemCount(root); ++index) {
        wchar_t label[512]{};
        MENUITEMINFOW info{sizeof(info)};
        info.fMask = MIIM_ID | MIIM_FTYPE | MIIM_DATA | MIIM_STRING;
        info.dwTypeData = label;
        info.cch = (UINT)std::size(label);
        assert(GetMenuItemInfoW(root, index, TRUE, &info));
        if (index == (int)files.size()) {
            assert(info.fType & MFT_SEPARATOR);
            continue;
        }
        assert(info.wID == (index < (int)files.size() ? (UINT)index + 1 : tabmenu::clearRecentId));
        if (index == 2) assert(std::wstring(label).find(L"R&&D") != std::wstring::npos);
        if (info.fType & MFT_OWNERDRAW) {
            MEASUREITEMSTRUCT measure{ODT_MENU, 0, info.wID, 0, 0, info.dwItemData};
            assert(SendMessageW(owner, WM_MEASUREITEM, 0, (LPARAM)&measure));
            assert(measure.itemHeight == (UINT)theme.px(index < (int)files.size() ? 48 : 33));
            assert(measure.itemWidth <= (UINT)theme.px(420));
        }
    }
    captureMenu(owner, root, theme, 2, path);
}

void previewOptionsMenu(HWND owner, bool dark, const wchar_t* path) {
    Settings settings;
    settings.language = 1;
    settings.themeMode = dark ? 2 : 1;
    settings.frameCloseAction = 2;
    settings.autoHide = true;
    settings.position = 3;
    tabmenu::Menu menu(owner, Host::Word, dark, settings.language, tabmenu::optionsRowHeight);
    tabmenu::populateOptions(menu, settings, settings.language);

    HMENU root = menu.handle();
    const UINT order[] = {60, 1, 2, 4, 100, 101, 102, 5, 0, 6, 7, 8, 12, 17, 18, 0, 13, 14, 15, 16, 9, 10, 11};
    assert(GetMenuItemCount(root) == (int)std::size(order));
    for (int index = 0; index < GetMenuItemCount(root); ++index) {
        MENUITEMINFOW item{sizeof(item)};
        item.fMask = MIIM_ID | MIIM_FTYPE;
        assert(GetMenuItemInfoW(root, index, TRUE, &item));
        if (order[index]) assert(item.wID == order[index]);
        else assert(item.fType & MFT_SEPARATOR);
    }
    for (UINT id : {1U, 2U, 4U, 7U}) assert(GetMenuState(root, id, MF_BYCOMMAND) & MF_CHECKED);
    for (UINT id : {5U, 6U}) assert(!(GetMenuState(root, id, MF_BYCOMMAND) & MF_CHECKED));

    HMENU closing = GetSubMenu(root, 4), appearance = GetSubMenu(root, 5), languages = GetSubMenu(root, 6);
    assert(closing && appearance && languages);
    assert(GetMenuItemCount(closing) == 3 && GetMenuItemCount(appearance) == 11 && GetMenuItemCount(languages) == 3);
    HMENU themeMenu = GetSubMenu(appearance, 10);
    assert(themeMenu && GetMenuItemCount(themeMenu) == 4);
    assert(GetMenuItemID(themeMenu, 0) == 30 && GetMenuItemID(themeMenu, 3) == 33);
    assert(GetMenuState(closing, 42, MF_BYCOMMAND) & MF_CHECKED);
    assert(GetMenuState(appearance, 23, MF_BYCOMMAND) & MF_CHECKED);
    assert(GetMenuState(appearance, 21, MF_BYCOMMAND) & MF_CHECKED);
    assert(GetMenuState(languages, 51, MF_BYCOMMAND) & MF_CHECKED);
    assert(GetMenuState(themeMenu, 30 + settings.themeMode, MF_BYCOMMAND) & MF_CHECKED);

    dialogui::Theme theme;
    theme.initialize(owner, Host::Word, dark, settings.language);
    captureMenu(owner, root, theme, 101, path);
    if (dark) captureMenu(owner, appearance, theme, 103, L"artifacts/options-appearance-menu-dark.png");
}

void previewMenus() {
    HWND owner = CreateWindowExW(WS_EX_TOOLWINDOW, L"STATIC", L"", WS_POPUP, 0, 0, 1, 1,
        nullptr, nullptr, module, nullptr);
    assert(owner);
    previewRecentMenu(owner, false, L"artifacts/recent-menu-light.png");
    previewRecentMenu(owner, true, L"artifacts/recent-menu-dark.png");
    previewOptionsMenu(owner, false, L"artifacts/options-menu-light.png");
    previewOptionsMenu(owner, true, L"artifacts/options-menu-dark.png");
    DestroyWindow(owner);
}
int wmain(){
    SetProcessDpiAwarenessContext(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2);
    CoInitializeEx(nullptr,COINIT_APARTMENTTHREADED);module=GetModuleHandleW(nullptr);
    INITCOMMONCONTROLSEX controls{sizeof(controls),ICC_STANDARD_CLASSES};InitCommonControlsEx(&controls);
    Gdiplus::GdiplusStartupInput input;ULONG_PTR token=0;Gdiplus::GdiplusStartup(&token,&input,nullptr);
    {
        previewFrameClose(Host::Word, false, 1, L"artifacts/frame-close-dialog-light.png");
        previewFrameClose(Host::Excel, true, 1, L"artifacts/frame-close-dialog-dark.png");
        previewFrameClose(Host::PowerPoint, false, 2, L"artifacts/frame-close-dialog-en.png");
        previewWidth(Host::Word, false, 1, L"artifacts/tab-width-dialog-light.png");
        previewWidth(Host::Excel, true, 1, L"artifacts/tab-width-dialog-dark.png");
        previewWidth(Host::PowerPoint, false, 2, L"artifacts/tab-width-dialog-en.png");
        previewColor(Host::Word, false, 1, L"artifacts/tab-color-dialog-light.png");
        previewColor(Host::Excel, true, 1, L"artifacts/tab-color-dialog-dark.png");
        previewColor(Host::PowerPoint, false, 2, L"artifacts/tab-color-dialog-en.png");
        checkTabMenu();
        previewMenus();
    }
    Gdiplus::GdiplusShutdown(token);CoUninitialize();
    std::cout<<"Frame close renders: light/dark, Chinese/English, bounds, cancel default and checkbox passed.\n";
    std::cout<<"Appearance dialogs render in light/dark and Chinese/English. Width/mode submission and color reset passed.\n";
    std::cout<<"Tab menu labels, submenu command IDs, disabled/checked states and native draw messages passed.\n";
    std::cout<<"Recently closed menu: file command IDs, ampersands, bounded two-line rows and empty history passed.\n";
    std::cout<<"Options menu: original command order, nested submenus, checked preferences and native drawing passed.\n";
}
