#pragma once
#include "dialog-theme.h"

namespace tabmenu {
enum class Glyph { none, save, saveAs, folder, external, move, lock, close, stack, more, check };

inline void drawGlyph(HDC dc, Glyph glyph, int x, int y, const dialogui::Theme& theme, COLORREF color) {
    if (glyph == Glyph::none) return;
    auto pen = CreatePen(PS_SOLID, theme.px(1), color);
    auto previousPen = SelectObject(dc, pen);
    auto line = [&](std::initializer_list<POINT> points) {
        auto point = points.begin();
        MoveToEx(dc, x + theme.px(point->x), y + theme.px(point->y), nullptr);
        for (++point; point != points.end(); ++point) {
            LineTo(dc, x + theme.px(point->x), y + theme.px(point->y));
        }
    };
    switch (glyph) {
    case Glyph::save:
        line({{2,2},{11,2},{14,5},{14,14},{2,14},{2,2}});
        line({{5,2},{5,6},{11,6},{11,2}});
        line({{5,14},{5,9},{11,9},{11,14}});
        break;
    case Glyph::saveAs:
        line({{3,2},{9,2},{13,6},{13,14},{3,14},{3,2}});
        line({{9,2},{9,6},{13,6}});
        line({{8,8},{8,12}}); line({{6,10},{11,10}});
        break;
    case Glyph::folder:
        line({{2,13},{2,4},{6,4},{8,6},{14,6},{14,7}});
        line({{2,13},{5,8},{15,8},{13,13},{2,13}});
        break;
    case Glyph::external:
        line({{7,4},{3,4},{3,14},{13,14},{13,9}});
        line({{9,2},{15,2},{15,8}}); line({{15,2},{7,10}});
        break;
    case Glyph::move:
        line({{2,3},{14,3},{14,13},{2,13},{2,3}});
        line({{2,6},{14,6}}); line({{7,6},{7,13}});
        break;
    case Glyph::lock:
        line({{3,7},{13,7},{13,14},{3,14},{3,7}});
        line({{5,7},{5,4},{7,2},{9,2},{11,4},{11,7}}); line({{8,10},{8,12}});
        break;
    case Glyph::close:
        line({{4,4},{13,13}}); line({{12,4},{3,13}});
        break;
    case Glyph::stack:
        line({{2,5},{8,2},{14,5},{8,8},{2,5}});
        line({{2,8},{8,11},{14,8}}); line({{2,11},{8,14},{14,11}});
        break;
    case Glyph::more:
        line({{3,8},{5,8}}); line({{7,8},{9,8}}); line({{11,8},{13,8}});
        break;
    case Glyph::check:
        line({{3,8},{7,12},{14,4}});
        break;
    case Glyph::none:
        break;
    }
    SelectObject(dc, previousPen);
    DeleteObject(pen);
}

struct Item {
    std::wstring label;
    Glyph glyph;
    bool dangerIcon = false;
    bool dangerText = false;
};

inline void drawItem(const DRAWITEMSTRUCT& draw, const Item& item, const dialogui::Theme& theme) {
    HDC dc = draw.hDC;
    int saved = SaveDC(dc);
    if (!saved) return;
    const RECT& row = draw.rcItem;
    IntersectClipRect(dc, row.left, row.top, row.right, row.bottom);
    dialogui::fill(dc, row, theme.background);
    bool disabled = (draw.itemState & (ODS_DISABLED | ODS_GRAYED)) != 0;
    bool highlighted = (draw.itemState & ODS_SELECTED) && !disabled;
    if (highlighted) {
        auto tint = [](int base, int accent) { return (base * 86 + accent * 14) / 100; };
        COLORREF highlight = RGB(tint(GetRValue(theme.background), GetRValue(theme.accent)),
            tint(GetGValue(theme.background), GetGValue(theme.accent)),
            tint(GetBValue(theme.background), GetBValue(theme.accent)));
        RECT inset = row;
        InflateRect(&inset, -theme.px(4), -theme.px(1));
        dialogui::rounded(dc, inset, highlight, highlight, theme.px(12));
    }
    COLORREF danger = theme.dark ? RGB(244,151,156) : RGB(173,60,68);
    COLORREF icon = disabled ? theme.muted : item.dangerIcon ? danger : highlighted ? theme.accent : theme.muted;
    Glyph glyph = (draw.itemState & ODS_CHECKED) ? Glyph::check : item.glyph;
    drawGlyph(dc, glyph, row.left + theme.px(12), row.top + (row.bottom - row.top - theme.px(16)) / 2, theme, icon);

    SelectObject(dc, theme.body);
    SetBkMode(dc, TRANSPARENT);
    SetTextColor(dc, disabled ? theme.muted : item.dangerText ? danger : highlighted ? theme.accent : theme.foreground);
    RECT text = row;
    text.left += theme.px(38);
    text.right -= theme.px(20);
    DrawTextW(dc, item.label.c_str(), -1, &text, DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS | DT_NOPREFIX);
    RestoreDC(dc, saved);
}

class Menu {
    HWND owner;
    HMENU root;
    dialogui::Theme theme;
    bool themed = false;
    std::vector<std::unique_ptr<Item>> items;

    Item* findItem(ULONG_PTR data) const {
        for (const auto& item : items) {
            if ((ULONG_PTR)item.get() == data) return item.get();
        }
        return nullptr;
    }

    static LRESULT CALLBACK ownerProc(HWND window, UINT message, WPARAM wp, LPARAM lp, UINT_PTR, DWORD_PTR data) {
        auto& menu = *(Menu*)data;
        if (message == WM_MEASUREITEM && wp == 0) {
            auto& measure = *(MEASUREITEMSTRUCT*)lp;
            if (measure.CtlType == ODT_MENU) {
                if (auto item = menu.findItem(measure.itemData)) {
                    auto dc = GetDC(window);
                    auto previousFont = SelectObject(dc, menu.theme.body);
                    SIZE label{};
                    GetTextExtentPoint32W(dc, item->label.c_str(), (int)item->label.size(), &label);
                    SelectObject(dc, previousFont);
                    ReleaseDC(window, dc);
                    measure.itemWidth = std::max(menu.theme.px(218), (int)label.cx + menu.theme.px(64));
                    measure.itemHeight = menu.theme.px(33);
                    return TRUE;
                }
            }
        }
        if (message == WM_DRAWITEM && wp == 0) {
            auto& draw = *(DRAWITEMSTRUCT*)lp;
            if (draw.CtlType == ODT_MENU) {
                if (auto item = menu.findItem(draw.itemData)) {
                    drawItem(draw, *item, menu.theme);
                    return TRUE;
                }
            }
        }
        return DefSubclassProc(window, message, wp, lp);
    }

    void setBackground(HMENU popup) const {
        if (!themed || !popup) return;
        MENUINFO info{sizeof(info)};
        info.fMask = MIM_BACKGROUND | MIM_STYLE;
        info.hbrBack = theme.backgroundBrush;
        info.dwStyle = MNS_NOCHECK;
        SetMenuInfo(popup, &info);
    }

    bool insert(HMENU popup, UINT id, const wchar_t* label, Glyph glyph, bool enabled, bool checked,
        bool dangerIcon, bool dangerText, HMENU branch = nullptr) {
        if (!popup) return false;
        MENUITEMINFOW info{sizeof(info)};
        info.fMask = MIIM_ID | MIIM_FTYPE | MIIM_STRING | MIIM_STATE;
        info.wID = id;
        info.fType = themed ? MFT_OWNERDRAW : MFT_STRING;
        info.fState = (enabled ? MFS_ENABLED : MFS_DISABLED) | (checked ? MFS_CHECKED : 0);
        info.dwTypeData = const_cast<wchar_t*>(label);
        if (branch) {
            info.fMask |= MIIM_SUBMENU;
            info.hSubMenu = branch;
        }
        if (themed) {
            items.push_back(std::make_unique<Item>(Item{label, glyph, dangerIcon, dangerText}));
            info.fMask |= MIIM_DATA;
            info.dwItemData = (ULONG_PTR)items.back().get();
        }
        return InsertMenuItemW(popup, (UINT)GetMenuItemCount(popup), TRUE, &info) != FALSE;
    }

public:
    Menu(HWND ownerWindow, Host host, bool dark, int language)
        : owner(ownerWindow), root(CreatePopupMenu()) {
        theme.initialize(owner, host, dark, language);
        // High contrast and hosts that reject subclassing use system drawing.
        themed = root && !theme.contrast && SetWindowSubclass(owner, ownerProc, (UINT_PTR)this, (DWORD_PTR)this);
        setBackground(root);
    }
    Menu(const Menu&) = delete;
    Menu& operator=(const Menu&) = delete;
    ~Menu() {
        if (themed) RemoveWindowSubclass(owner, ownerProc, (UINT_PTR)this);
        if (root) DestroyMenu(root);
    }

    HMENU handle() const { return root; }
    void add(HMENU popup, UINT id, const wchar_t* label, Glyph glyph = Glyph::none, bool enabled = true,
        bool checked = false, bool dangerIcon = false, bool dangerText = false) {
        insert(popup, id, label, glyph, enabled, checked, dangerIcon, dangerText);
    }
    void separator() const { if (root) AppendMenuW(root, MF_SEPARATOR, 0, nullptr); }
    HMENU submenu(UINT id, const wchar_t* label, Glyph glyph, bool dangerIcon = false) {
        if (!root) return nullptr;
        HMENU branch = CreatePopupMenu();
        if (!branch) return nullptr;
        if (!insert(root, id, label, glyph, true, false, dangerIcon, false, branch)) {
            DestroyMenu(branch);
            return nullptr;
        }
        setBackground(branch);
        return branch;
    }
    UINT track(POINT screen) const {
        if (!root) return 0;
        UINT flags = TPM_RETURNCMD;
        if (!themed) flags |= TPM_NONOTIFY;
        return TrackPopupMenu(root, flags, screen.x, screen.y, 0, owner, nullptr);
    }
};

inline void populate(Menu& menu, const View& view, int language) {
    auto tr = [=](const wchar_t* key) { return i18n::text(language, key); };
    HMENU root = menu.handle();
    menu.add(root, 2, tr(L"保存"), Glyph::save, !view.readOnly);
    menu.add(root, 3, tr(L"另存为…"), Glyph::saveAs, !view.readOnly);
    menu.add(root, 11, tr(L"打开所在文件夹"), Glyph::folder, !view.path.empty() && view.path.find(L"://") == std::wstring::npos);
    menu.separator();
    menu.add(root, 12, tr(L"在独立窗口中显示"), Glyph::external);
    HMENU move = menu.submenu(100, tr(L"移动标签"), Glyph::move);
    menu.add(move, 13, tr(L"移到新标签组窗口"));
    menu.add(move, 14, tr(L"移动到标签组…"));
    menu.add(move, 15, tr(L"新建为独立窗口"));
    menu.add(root, 10, tr(L"锁定标签"), Glyph::lock, true, view.locked);
    menu.separator();
    menu.add(root, 5, tr(L"关闭当前标签"), Glyph::close, !view.locked, false, true);
    HMENU close = menu.submenu(101, tr(L"批量关闭"), Glyph::stack, true);
    menu.add(close, 6, tr(L"关闭其他标签"));
    menu.add(close, 7, tr(L"关闭左侧标签"));
    menu.add(close, 8, tr(L"关闭右侧标签"));
    menu.add(close, 9, tr(L"关闭全部标签"), Glyph::none, true, false, false, true);
    menu.separator();
    HMENU more = menu.submenu(102, tr(L"更多操作"), Glyph::more);
    menu.add(more, 1, tr(L"切换到此文档"));
    menu.add(more, 4, tr(L"保存本组全部标签"));
    menu.add(more, 17, tr(L"最近关闭…"));
    menu.add(more, 18, tr(L"重置此文件的标签设置"), Glyph::none, storage::localFile(view.path));
    menu.add(more, 19, tr(L"管理中心"));
}
}
