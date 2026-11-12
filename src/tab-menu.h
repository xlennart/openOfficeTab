#pragma once
#include "dialog-theme.h"

namespace tabmenu {
enum class Glyph { none, save, saveAs, rename, folder, external, move, lock, close, stack, more, check, file, history, trash, sliders, info };

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
    case Glyph::rename:
        line({{3,10},{11,2},{14,5},{6,13},{2,14},{3,10}});
        line({{9,4},{12,7}});
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
    case Glyph::file:
        line({{3,2},{9,2},{13,6},{13,14},{3,14},{3,2}});
        line({{9,2},{9,6},{13,6}});
        line({{5,8},{11,8}}); line({{5,11},{11,11}});
        break;
    case Glyph::history:
        line({{2,7},{4,3},{9,2},{13,4},{15,8},{13,12},{9,14},{4,12}});
        line({{2,3},{2,7},{6,7}}); line({{8,5},{8,9},{11,10}});
        break;
    case Glyph::trash:
        line({{2,4},{14,4}}); line({{6,4},{6,2},{10,2},{10,4}});
        line({{4,4},{5,14},{11,14},{12,4}});
        line({{7,6},{7,12}}); line({{9,6},{9,12}});
        break;
    case Glyph::sliders:
        line({{2,4},{14,4}}); line({{6,2},{6,6}});
        line({{2,8},{14,8}}); line({{10,6},{10,10}});
        line({{2,12},{14,12}}); line({{5,10},{5,14}});
        break;
    case Glyph::info:
        line({{6,2},{10,2},{14,6},{14,10},{10,14},{6,14},{2,10},{2,6},{6,2}});
        line({{8,4},{8,6}}); line({{8,7},{8,12}});
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
    std::wstring detail;
};

inline void drawItem(const DRAWITEMSTRUCT& draw, const Item& item, const dialogui::Theme& theme, HFONT detailFont) {
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
    if (!item.detail.empty()) {
        text.top += theme.px(4);
        text.bottom = row.top + theme.px(24);
    }
    DrawTextW(dc, item.label.c_str(), -1, &text, DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS | DT_NOPREFIX);
    if (!item.detail.empty()) {
        if (detailFont) SelectObject(dc, detailFont);
        SetTextColor(dc, theme.muted);
        text.top = text.bottom;
        text.bottom = row.bottom - theme.px(4);
        DrawTextW(dc, item.detail.c_str(), -1, &text, DT_VCENTER | DT_SINGLELINE | DT_PATH_ELLIPSIS | DT_NOPREFIX);
    }
    RestoreDC(dc, saved);
}

class Menu {
    HWND owner;
    HMENU root;
    dialogui::Theme theme;
    const int rowHeight;
    HFONT detailFont = nullptr;
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
                    if (!item->detail.empty()) {
                        if (menu.detailFont) SelectObject(dc, menu.detailFont);
                        SIZE detail{};
                        GetTextExtentPoint32W(dc, item->detail.c_str(), (int)item->detail.size(), &detail);
                        label.cx = std::max(label.cx, detail.cx);
                    }
                    SelectObject(dc, previousFont);
                    ReleaseDC(window, dc);
                    int width = (int)label.cx + menu.theme.px(64);
                    measure.itemWidth = item->detail.empty() ? std::max(menu.theme.px(218), width)
                        : std::clamp(width, menu.theme.px(290), menu.theme.px(420));
                    measure.itemHeight = menu.theme.px(item->detail.empty() ? menu.rowHeight : 48);
                    return TRUE;
                }
            }
        }
        if (message == WM_DRAWITEM && wp == 0) {
            auto& draw = *(DRAWITEMSTRUCT*)lp;
            if (draw.CtlType == ODT_MENU) {
                if (auto item = menu.findItem(draw.itemData)) {
                    drawItem(draw, *item, menu.theme, menu.detailFont);
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
        bool dangerIcon, bool dangerText, HMENU branch = nullptr, const wchar_t* detail = L"") {
        if (!popup) return false;
        MENUITEMINFOW info{sizeof(info)};
        info.fMask = MIIM_ID | MIIM_FTYPE | MIIM_STRING | MIIM_STATE;
        info.wID = id;
        info.fType = themed ? MFT_OWNERDRAW : MFT_STRING;
        info.fState = (enabled ? MFS_ENABLED : MFS_DISABLED) | (checked ? MFS_CHECKED : 0);
        std::wstring caption = label;
        if (*detail) caption += std::wstring(L"  —  ") + detail;
        if (!themed && caption.size() > 220) caption = caption.substr(0, 217) + L"…";
        // Preserve literal ampersands in file names even in the system fallback.
        std::wstring escaped;
        for (auto ch : caption) {
            if (ch == L'&') escaped += L'&';
            escaped += ch;
        }
        info.dwTypeData = escaped.data();
        if (branch) {
            info.fMask |= MIIM_SUBMENU;
            info.hSubMenu = branch;
        }
        if (themed) {
            items.push_back(std::make_unique<Item>(Item{label, glyph, dangerIcon, dangerText, detail}));
            info.fMask |= MIIM_DATA;
            info.dwItemData = (ULONG_PTR)items.back().get();
        }
        return InsertMenuItemW(popup, (UINT)GetMenuItemCount(popup), TRUE, &info) != FALSE;
    }

public:
    Menu(HWND ownerWindow, Host host, bool dark, int language, int itemHeight = 33)
        : owner(ownerWindow), root(CreatePopupMenu()), rowHeight(itemHeight) {
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
        if (detailFont) DeleteObject(detailFont);
    }

    HMENU handle() const { return root; }
    void add(HMENU popup, UINT id, const wchar_t* label, Glyph glyph = Glyph::none, bool enabled = true,
        bool checked = false, bool dangerIcon = false, bool dangerText = false) {
        insert(popup, id, label, glyph, enabled, checked, dangerIcon, dangerText);
    }
    void addFile(UINT id, const std::wstring& file) {
        if (themed && !detailFont) {
            LOGFONTW font{};
            if (GetObjectW(theme.body, sizeof(font), &font)) {
                font.lfHeight = -theme.px(12);
                detailFont = CreateFontIndirectW(&font);
            }
        }
        auto slash = file.find_last_of(L"\\/");
        auto name = slash == std::wstring::npos ? file : file.substr(slash + 1);
        auto folder = slash == std::wstring::npos ? L"" : file.substr(0, slash == 2 && file[1] == L':' ? 3 : slash);
        insert(root, id, name.c_str(), Glyph::file, true, false, false, false, nullptr, folder.c_str());
    }
    void separator(HMENU popup) const { if (popup) AppendMenuW(popup, MF_SEPARATOR, 0, nullptr); }
    void separator() const { separator(root); }
    HMENU submenu(HMENU parent, UINT id, const wchar_t* label, Glyph glyph, bool dangerIcon = false) {
        if (!parent) return nullptr;
        HMENU branch = CreatePopupMenu();
        if (!branch) return nullptr;
        if (!insert(parent, id, label, glyph, true, false, dangerIcon, false, branch)) {
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
    menu.add(root, 20, tr(L"重命名文件…"), Glyph::rename, !view.readOnly && storage::localFile(view.path));
    menu.add(root, 11, tr(L"打开所在文件夹"), Glyph::folder, !view.path.empty() && view.path.find(L"://") == std::wstring::npos);
    menu.separator();
    menu.add(root, 12, tr(L"在独立窗口中显示"), Glyph::external);
    HMENU move = menu.submenu(root, 100, tr(L"移动标签"), Glyph::move);
    menu.add(move, 13, tr(L"移到新标签组窗口"));
    menu.add(move, 14, tr(L"移动到标签组…"));
    menu.add(move, 15, tr(L"新建为独立窗口"));
    menu.add(root, 10, tr(L"锁定标签"), Glyph::lock, true, view.locked);
    menu.separator();
    menu.add(root, 5, tr(L"关闭当前标签"), Glyph::close, !view.locked, false, true);
    HMENU close = menu.submenu(root, 101, tr(L"批量关闭"), Glyph::stack, true);
    menu.add(close, 6, tr(L"关闭其他标签"));
    menu.add(close, 7, tr(L"关闭左侧标签"));
    menu.add(close, 8, tr(L"关闭右侧标签"));
    menu.add(close, 9, tr(L"关闭全部标签"), Glyph::none, true, false, false, true);
    menu.separator();
    HMENU more = menu.submenu(root, 102, tr(L"更多操作"), Glyph::more);
    menu.add(more, 1, tr(L"切换到此文档"));
    menu.add(more, 4, tr(L"保存本组全部标签"));
    menu.add(more, 17, tr(L"最近关闭…"), Glyph::history);
    menu.add(more, 18, tr(L"重置此文件的标签设置"), Glyph::none, storage::localFile(view.path));
    menu.add(more, 19, tr(L"管理中心"));
}

constexpr int optionsRowHeight = 29;

inline void populateOptions(Menu& menu, const Settings& settings, int language) {
    auto tr = [=](const wchar_t* key) { return i18n::text(language, key); };
    auto add = [&](HMENU popup, UINT id, const wchar_t* label, Glyph glyph = Glyph::none, bool checked = false) {
        menu.add(popup, id, tr(label), glyph, true, checked);
    };
    HMENU root = menu.handle();
    add(root, 60, L"管理中心", Glyph::sliders);
    add(root, 1, L"显示标签栏", Glyph::move, settings.visible);
    add(root, 2, L"单任务栏模式", Glyph::stack, settings.group);
    add(root, 4, L"单标签时自动隐藏", Glyph::none, settings.autoHide);

    HMENU closing = menu.submenu(root, 100, tr(L"点击窗口关闭按钮时"), Glyph::close);
    const wchar_t* actions[] = {L"每次询问", L"只关闭当前文档", L"关闭本组全部文档"};
    for (int i = 0; i < 3; ++i) add(closing, 40 + i, actions[i], Glyph::none, settings.frameCloseAction == i);

    HMENU appearance = menu.submenu(root, 101, tr(L"标签外观"), Glyph::move);
    add(appearance, 23, L"融入应用背景（浅灰开口标签）", Glyph::none, settings.barStyle == 1);
    add(appearance, 24, L"独立标签栏（白色圆角面板）", Glyph::none, settings.barStyle == 0);
    menu.separator(appearance);
    add(appearance, 21, L"强调线：顶部", Glyph::none, settings.accentPosition == 1);
    add(appearance, 20, L"强调线：底部", Glyph::none, settings.accentPosition == 0);
    add(appearance, 22, L"强调线：无", Glyph::none, settings.accentPosition == 2);
    menu.separator(appearance);
    add(appearance, 25, L"标签栏颜色…");
    add(appearance, 26, L"恢复默认颜色");
    menu.separator(appearance);
    HMENU theme = menu.submenu(appearance, 103, tr(L"明暗主题"), Glyph::none);
    const wchar_t* themes[] = {L"跟随 Office / 系统（默认）", L"始终浅色", L"始终深色", L"仅跟随系统"};
    for (int i = 0; i < 4; ++i) add(theme, 30 + i, themes[i], Glyph::none, settings.themeMode == i);

    HMENU languages = menu.submenu(root, 102, tr(L"语言 / Language"), Glyph::none);
    add(languages, 50, L"跟随系统（默认）", Glyph::none, settings.language == 0);
    menu.add(languages, 51, L"简体中文", Glyph::none, true, settings.language == 1);
    menu.add(languages, 52, L"English", Glyph::none, true, settings.language == 2);
    add(root, 5, L"双击标签关闭", Glyph::none, settings.doubleClose);
    menu.separator();

    add(root, 6, L"标签置于顶部", Glyph::none, settings.position == 1);
    add(root, 7, L"标签置于底部", Glyph::none, settings.position == 3);
    add(root, 8, L"恢复窗口排列与任务栏", Glyph::stack);
    add(root, 12, L"默认标签宽度…", Glyph::sliders);
    add(root, 17, L"最近关闭…", Glyph::history);
    menu.add(root, 18, tr(L"清除本应用已记住的文件设置"), Glyph::trash, true, false, true);
    menu.separator();

    add(root, 13, L"加载诊断…", Glyph::info);
    add(root, 14, L"导出脱敏诊断…", Glyph::saveAs);
    add(root, 15, L"恢复标签栏与窗口", Glyph::move);
    add(root, 16, L"新建为独立窗口", Glyph::external);
    add(root, 9, L"打开文档…", Glyph::file);
    add(root, 10, L"打开日志与设置文件夹", Glyph::folder);
    add(root, 11, L"关于 openOfficeTab", Glyph::info);
}

constexpr UINT clearRecentId = 100;

inline void populateRecent(Menu& menu, const std::vector<std::wstring>& files, int language) {
    auto tr = [=](const wchar_t* key) { return i18n::text(language, key); };
    for (size_t index = 0; index < files.size(); ++index) menu.addFile((UINT)index + 1, files[index]);
    if (files.empty()) menu.add(menu.handle(), 0, tr(L"暂无最近关闭的文件"), Glyph::history, false);
    menu.separator();
    menu.add(menu.handle(), clearRecentId, tr(L"清空最近关闭记录"), Glyph::trash, !files.empty(), false, true, true);
}
}
