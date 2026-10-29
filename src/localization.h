#pragma once
#include <windows.h>
#include <string>
#include <string_view>

// 0 follows the Windows display language; 1 is Simplified Chinese; 2 is English.
// Every other Windows UI language falls back to English. Never translate file names.
namespace i18n {
inline bool valid(int mode){return mode>=0&&mode<=2;}
inline int resolve(int mode,LANGID system=GetUserDefaultUILanguage()){
    return mode==1||mode==2?mode:PRIMARYLANGID(system)==LANG_CHINESE?1:2;
}
struct Entry {const wchar_t* zh;const wchar_t* en;};
inline constexpr Entry entries[]={
    {L"永久免费原生文档标签 · Word / Excel / PowerPoint",L"Free native document tabs · Word / Excel / PowerPoint"},
    {L"标签栏颜色",L"Tab bar color"},
    {L"输入颜色（#RRGGBB），或使用调色板。",L"Enter #RRGGBB or choose a palette color."},
    {L"调色板…",L"Palette…"},
    {L"恢复默认",L"Reset"},
    {L"深色模式和系统高对比度优先于自定义颜色。",L"Dark mode and high contrast override custom colors."},
    {L"确定",L"OK"},{L"取消",L"Cancel"},
    {L"请输入 6 位十六进制颜色，例如 #F0F0F0。",L"Enter a six-digit hexadecimal color, such as #F0F0F0."},
    {L"标签宽度",L"Tab width"},
    {L"标签宽度（80–360，按缩放自动适配）",L"Tab width (80–360; scales with DPI)"},
    {L"固定宽度",L"Fixed width"},
    {L"按标题长度（上述宽度为上限）",L"Fit title (up to the width above)"},
    {L"空间不足时压缩（上述宽度为上限）",L"Shrink to fit (up to the width above)"},
    {L"请输入 80 到 360 之间的整数。",L"Enter a whole number from 80 to 360."},
    {L"标签组 ",L"Group "},{L"（",L" ("},{L" 个窗口）",L" windows)"},
    {L"新建标签组窗口",L"New tab group window"},
    {L"独立窗口（隐藏标签栏）",L"Independent window (hide tabs)"},
    {L"加入默认标签组",L"Join the default tab group"},
    {L"Office 尚未提供标签窗格工厂。请保存后重开 Office，或在安装器中检查和修复注册。\n",L"Office has not provided the tab pane factory. Save and restart Office, or check and repair registration in the installer.\n"},
    {L"标签栏当前被隐藏，可使用“恢复标签栏与窗口”。\n",L"The tab bar is hidden. Use Recover tabs and windows.\n"},
    {L"已启用单标签自动隐藏。\n",L"Auto-hide for a single tab is enabled.\n"},
    {L"Office 调用最近被拒绝，可能正忙或有对话框；关闭对话框后重试。\n",L"Office recently rejected a call. It may be busy or showing a dialog; dismiss the dialog and retry.\n"},
    {L"当前没有普通编辑视图；受保护视图请通过 Office 原提示进入编辑。\n",L"No normal editing views are open. Use Office's own prompt to leave Protected View.\n"},
    {L"\n以下诊断不含文件名、路径、正文或账号：\n",L"\nThe following diagnostics exclude file names, paths, content, and accounts:\n"},
    {L"加载诊断",L"Loading diagnostics"},
    {L"JSON 诊断文件\0*.json\0\0",L"JSON diagnostics\0*.json\0\0"},
    {L"已导出脱敏诊断，不包含文档名称、路径、正文、账号或原始日志。",L"Sanitized diagnostics exported. They exclude document names, paths, content, accounts, and raw logs."},
    {L"诊断文件写入失败，请选择可写目录。",L"Could not write the diagnostics file. Choose a writable folder."},
    {L"导出诊断",L"Export diagnostics"},
    {L"文件已移动、删除或当前无法访问。最近关闭记录仍然保留，可从菜单清空。",L"The file has moved, was deleted, or is unavailable. Its recently closed entry is kept; you can clear it from the menu."},
    {L"重新打开文档",L"Reopen document"},
    {L"暂无最近关闭的文件",L"No recently closed files"},
    {L"清空最近关闭记录",L"Clear recently closed files"},
    {L"切换到此文档",L"Switch to this document"},
    {L"在独立窗口中显示",L"Show in an independent window"},
    {L"移到新标签组窗口",L"Move to a new tab group window"},
    {L"移动到标签组…",L"Move to tab group…"},
    {L"新建为独立窗口",L"New independent document"},
    {L"保存",L"Save"},{L"另存为…",L"Save As…"},
    {L"保存本组全部标签",L"Save all tabs in this group"},
    {L"关闭",L"Close"},{L"关闭其他标签",L"Close other tabs"},
    {L"关闭左侧标签",L"Close tabs to the left"},{L"关闭右侧标签",L"Close tabs to the right"},
    {L"关闭全部标签",L"Close all tabs"},
    {L"锁定标签（防止从标签栏关闭）",L"Lock tab (prevent closing from the tab bar)"},
    {L"打开所在文件夹",L"Open containing folder"},
    {L"最近关闭…",L"Recently closed…"},
    {L"重置此文件的标签设置",L"Reset this file's tab settings"},
    {L"显示标签栏",L"Show tab bar"},{L"单任务栏模式",L"Single-taskbar mode"},
    {L"单标签时自动隐藏",L"Auto-hide when only one tab is open"},
    {L"双击标签关闭",L"Double-click a tab to close"},
    {L"标签置于顶部",L"Tabs at top"},{L"标签置于底部",L"Tabs at bottom"},
    {L"恢复窗口排列与任务栏",L"Restore window arrangement and taskbar entries"},
    {L"默认标签宽度…",L"Default tab width…"},
    {L"清除本应用已记住的文件设置",L"Clear remembered file settings for this application"},
    {L"加载诊断…",L"Loading diagnostics…"},{L"导出脱敏诊断…",L"Export sanitized diagnostics…"},
    {L"恢复标签栏与窗口",L"Recover tabs and windows"},
    {L"打开文档…",L"Open document…"},{L"打开日志与设置文件夹",L"Open logs and settings folder"},
    {L"关于 openOfficeTab",L"About openOfficeTab"},
    {L"融入应用背景（浅灰开口标签）",L"Blend with application (open-edge tabs)"},
    {L"独立标签栏（白色圆角面板）",L"Separate tab bar (rounded panel)"},
    {L"强调线：顶部",L"Accent line: top"},{L"强调线：底部",L"Accent line: bottom"},
    {L"强调线：无",L"Accent line: none"},{L"标签栏颜色…",L"Tab bar color…"},
    {L"恢复默认颜色",L"Restore default color"},
    {L"跟随 Office / 系统（默认）",L"Follow Office / Windows (default)"},
    {L"始终浅色",L"Always light"},{L"始终深色",L"Always dark"},
    {L"仅跟随系统",L"Follow Windows only"},{L"明暗主题",L"Theme"},
    {L"每次询问",L"Ask every time"},{L"只关闭当前文档",L"Close the current document"},
    {L"关闭本组全部文档",L"Close all documents in this group"},
    {L"点击窗口关闭按钮时",L"When clicking the window close button"},
    {L"标签外观",L"Tab appearance"},
    {L"openOfficeTab 0.2.2\n永久免费 · MIT 开源\nWord / Excel / PowerPoint\n\n浏览器圆角标签 · 原生 Office 文档与保存流程\n拖动排序，中键关闭，右键更多操作。\n\n标签锁定仅防止标签栏误关，不替代 Office 文档保护。",L"openOfficeTab 0.2.2\nFree and open source · MIT\nWord / Excel / PowerPoint\n\nBrowser-style tabs with Office's native documents and save flow.\nDrag to reorder, middle-click to close, right-click for more.\n\nTab locking prevents accidental tab-bar closes; it is not document protection."},
    {L"文档标签",L"Document tabs"},
    {L"openOfficeTab · + 新建 · ▾ 文档列表 · ⋯ 设置\n拖动排序，中键关闭，右键更多操作",L"openOfficeTab · + New · ▾ Document list · ⋯ Settings\nDrag to reorder, middle-click to close, right-click for more"},
    {L"openOfficeTab 标签栏",L"openOfficeTab tab bar"},
    {L"关闭文档窗口",L"Close document window"},
    {L"此窗口所在的标签组有 ",L"This tab group contains "},{L" 个文档。",L" documents."},
    {L"关闭当前文档，还是关闭本组全部文档？\n未保存的更改仍由 Office 询问；取消保存会停止后续关闭。",L"Close this document or the whole group?\nOffice will ask to save changes. Cancel stops remaining closes."},
    {L"记住我的选择（可在标签设置中更改）",L"Remember my choice (change it in Tab settings)"},
    {L"关闭当前",L"Close current"},{L"关闭本组全部",L"Close group"},
    {L"新建文档",L"New document"},{L"打开文档",L"Open document"},
    {L"标签与窗口",L"Tabs and windows"},{L"显示 / 隐藏标签栏",L"Show / hide tab bar"},
    {L"独立窗口显示",L"Independent window"},{L"加入 / 移动到标签组",L"Join / move to tab group"},
    {L"恢复窗口排列",L"Restore window arrangement"},{L"设置与恢复",L"Settings and recovery"},
    {L"标签设置",L"Tab settings"},{L"刷新标签",L"Refresh tabs"},{L"管理中心",L"Management center"},
    {L"导出脱敏诊断",L"Export sanitized diagnostics"},
    {L"语言 / Language",L"Language / 语言"},{L"跟随系统（默认）",L"Follow Windows (default)"}
};
inline const wchar_t* text(int language,const wchar_t* key){
    if(resolve(language)==1)return key;
    for(const auto& entry:entries)if(std::wstring_view(entry.zh)==key)return entry.en;
    return key;
}
inline std::wstring groupLabel(int language,unsigned id,size_t count){
    return std::wstring(text(language,L"标签组 "))+std::to_wstring(id)+text(language,L"（")+std::to_wstring(count)+text(language,L" 个窗口）");
}
inline std::wstring closeSummary(int language,size_t count){
    return std::wstring(text(language,L"此窗口所在的标签组有 "))+std::to_wstring(count)+text(language,L" 个文档。");
}
struct RibbonLabel {const wchar_t* id;const wchar_t* key;};
inline constexpr RibbonLabel ribbonLabels[]={
    {L"ootDocs",L"文档标签"},{L"ootNew",L"新建文档"},{L"ootNewIndependent",L"新建为独立窗口"},
    {L"ootOpen",L"打开文档"},{L"ootSave",L"保存本组全部标签"},{L"ootRecent",L"最近关闭…"},
    {L"ootWindows",L"标签与窗口"},{L"ootShow",L"显示 / 隐藏标签栏"},{L"ootGroup",L"单任务栏模式"},
    {L"ootDetach",L"独立窗口显示"},{L"ootJoin",L"加入 / 移动到标签组"},{L"ootRestore",L"恢复窗口排列"},
    {L"ootOptions",L"设置与恢复"},{L"ootCenter",L"管理中心"},{L"ootWidth",L"标签宽度"},{L"ootSettings",L"标签设置"},
    {L"ootRefresh",L"刷新标签"},{L"ootDiagnose",L"加载诊断"},{L"ootExport",L"导出脱敏诊断"},{L"ootRecover",L"恢复标签栏与窗口"}
};
inline const wchar_t* ribbonLabel(int language,const std::wstring& id){
    for(const auto& entry:ribbonLabels)if(id==entry.id)return text(language,entry.key);
    return L"openOfficeTab";
}
}
