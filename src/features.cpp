#include "model.h"
#include "layout.h"
#include "tab-color.h"
#include <commdlg.h>

namespace {
bool ownedWindow(HWND window){DWORD pid=0;GetWindowThreadProcessId(window,&pid);return window&&IsWindow(window)&&pid==GetCurrentProcessId();}
std::wstring hostLabel(Host host){return host==Host::Word?L"Word":host==Host::Excel?L"Excel":L"PowerPoint";}
struct WidthChoice {int width,mode;};
struct ColorChoice {COLORREF color;bool custom;};
INT_PTR CALLBACK colorProc(HWND dialog,UINT message,WPARAM wp,LPARAM lp){
    auto choice=(ColorChoice*)GetWindowLongPtrW(dialog,DWLP_USER);
    if(message==WM_INITDIALOG){
        choice=(ColorChoice*)lp;bool originalCustom=choice->custom;SetWindowLongPtrW(dialog,DWLP_USER,lp);SetWindowTextW(dialog,L"标签栏颜色");
        int scale=dpi(dialog);auto px=[&](int value){return MulDiv(value,scale,96);};RECT r{0,0,px(380),px(185)};
        AdjustWindowRectEx(&r,(DWORD)GetWindowLongPtrW(dialog,GWL_STYLE),FALSE,(DWORD)GetWindowLongPtrW(dialog,GWL_EXSTYLE));SetWindowPos(dialog,nullptr,0,0,r.right-r.left,r.bottom-r.top,SWP_NOMOVE|SWP_NOZORDER);
        auto control=[&](const wchar_t* cls,const wchar_t* text,DWORD style,int id,int x,int y,int width,int height){auto child=CreateWindowExW(0,cls,text,WS_CHILD|WS_VISIBLE|style,px(x),px(y),px(width),px(height),dialog,(HMENU)(INT_PTR)id,module,nullptr);SendMessageW(child,WM_SETFONT,(WPARAM)GetStockObject(DEFAULT_GUI_FONT),TRUE);return child;};
        control(L"STATIC",L"输入颜色（#RRGGBB），或使用调色板。",0,0,16,16,348,22);
        control(L"EDIT",tabcolor::format(choice->color).c_str(),WS_BORDER|WS_TABSTOP|ES_AUTOHSCROLL,101,16,48,160,26);SendDlgItemMessageW(dialog,101,EM_SETLIMITTEXT,7,0);
        control(L"BUTTON",L"调色板…",WS_TABSTOP,102,188,48,92,28);control(L"BUTTON",L"恢复默认",WS_TABSTOP,103,16,90,100,28);
        control(L"STATIC",L"深色模式和系统高对比度优先于自定义颜色。",0,0,16,120,348,22);
        control(L"BUTTON",L"确定",WS_TABSTOP|BS_DEFPUSHBUTTON,IDOK,188,151,82,28);control(L"BUTTON",L"取消",WS_TABSTOP,IDCANCEL,282,151,82,28);
        SetFocus(GetDlgItem(dialog,101));SendDlgItemMessageW(dialog,101,EM_SETSEL,0,-1);choice->custom=originalCustom;return FALSE;
    }
    if(message==WM_COMMAND&&choice){
        if(LOWORD(wp)==101&&HIWORD(wp)==EN_CHANGE)choice->custom=true;
        if(LOWORD(wp)==102){wchar_t text[32]{};GetDlgItemTextW(dialog,101,text,32);COLORREF color=choice->color;tabcolor::parse(text,color);
            COLORREF colors[16]{};CHOOSECOLORW picker{sizeof(picker)};picker.hwndOwner=dialog;picker.rgbResult=color;picker.lpCustColors=colors;picker.Flags=CC_FULLOPEN|CC_RGBINIT;
            if(ChooseColorW(&picker))SetDlgItemTextW(dialog,101,tabcolor::format(picker.rgbResult).c_str());return TRUE;}
        if(LOWORD(wp)==103){SetDlgItemTextW(dialog,101,L"#F0F0F0");choice->custom=false;return TRUE;}
        if(LOWORD(wp)==IDOK){wchar_t text[32]{};GetDlgItemTextW(dialog,101,text,32);COLORREF color;
            if(!tabcolor::parse(text,color)){MessageBoxW(dialog,L"请输入 6 位十六进制颜色，例如 #F0F0F0。",L"标签栏颜色",MB_OK|MB_ICONINFORMATION);return TRUE;}
            choice->color=color;EndDialog(dialog,IDOK);return TRUE;}
    }
    if(message==WM_CLOSE||(message==WM_COMMAND&&LOWORD(wp)==IDCANCEL)){EndDialog(dialog,IDCANCEL);return TRUE;}return FALSE;
}
INT_PTR CALLBACK widthProc(HWND dialog,UINT message,WPARAM wp,LPARAM lp){
    auto choice=(WidthChoice*)GetWindowLongPtrW(dialog,DWLP_USER);
    if(message==WM_INITDIALOG){
        SetWindowTextW(dialog,L"标签宽度");
        choice=(WidthChoice*)lp;SetWindowLongPtrW(dialog,DWLP_USER,lp);
        int scale=dpi(dialog);auto px=[&](int value){return MulDiv(value,scale,96);};
        RECT r{0,0,px(360),px(180)};AdjustWindowRectEx(&r,(DWORD)GetWindowLongPtrW(dialog,GWL_STYLE),FALSE,(DWORD)GetWindowLongPtrW(dialog,GWL_EXSTYLE));
        SetWindowPos(dialog,nullptr,0,0,r.right-r.left,r.bottom-r.top,SWP_NOMOVE|SWP_NOZORDER);
        auto control=[&](const wchar_t* cls,const wchar_t* text,DWORD style,int id,int x,int y,int w,int h){auto child=CreateWindowExW(0,cls,text,WS_CHILD|WS_VISIBLE|style,px(x),px(y),px(w),px(h),dialog,(HMENU)(INT_PTR)id,module,nullptr);SendMessageW(child,WM_SETFONT,(WPARAM)GetStockObject(DEFAULT_GUI_FONT),TRUE);return child;};
        control(L"STATIC",L"标签宽度（80–360，按缩放自动适配）",0,0,16,14,328,22);
        control(L"EDIT",std::to_wstring(choice->width).c_str(),WS_BORDER|WS_TABSTOP|ES_NUMBER|ES_AUTOHSCROLL,101,16,42,100,25);
        auto combo=control(L"COMBOBOX",L"",WS_TABSTOP|CBS_DROPDOWNLIST,102,16,84,328,100);
        for(auto text:{L"固定宽度",L"按标题长度（上述宽度为上限）",L"空间不足时压缩（上述宽度为上限）"})SendMessageW(combo,CB_ADDSTRING,0,(LPARAM)text);
        SendMessageW(combo,CB_SETCURSEL,choice->mode,0);
        control(L"BUTTON",L"确定",WS_TABSTOP|BS_DEFPUSHBUTTON,IDOK,174,138,78,28);
        control(L"BUTTON",L"取消",WS_TABSTOP,IDCANCEL,266,138,78,28);
        SetFocus(GetDlgItem(dialog,101));SendDlgItemMessageW(dialog,101,EM_SETSEL,0,-1);return FALSE;
    }
    if(message==WM_COMMAND&&LOWORD(wp)==IDOK){BOOL valid=FALSE;int width=(int)GetDlgItemInt(dialog,101,&valid,FALSE);int mode=(int)SendDlgItemMessageW(dialog,102,CB_GETCURSEL,0,0);
        if(!valid||!tablayout::validWidth(width,mode)){MessageBoxW(dialog,L"请输入 80 到 360 之间的整数。",L"标签宽度",MB_OK|MB_ICONINFORMATION);return TRUE;}
        choice->width=width;choice->mode=mode;EndDialog(dialog,IDOK);return TRUE;
    }
    if(message==WM_CLOSE||(message==WM_COMMAND&&LOWORD(wp)==IDCANCEL)){EndDialog(dialog,IDCANCEL);return TRUE;}return FALSE;
}
std::wstring hex(HRESULT hr){wchar_t text[16];swprintf_s(text,L"0x%08X",(unsigned)hr);return text;}
}
bool Session::setTabWidth(int width,int mode){
    if(!tablayout::validWidth(width,mode)||stopped)return false;settings.tabWidth=width;settings.widthMode=mode;
    for(auto& v:views)v->manualTabWidth=0;
    settings.save();redraw();return true;
}
bool Session::setAppearance(int accent,int style){
    if(!tablayout::validAppearance(accent,style)||stopped)return false;
    settings.accentPosition=accent;settings.barStyle=style;settings.save();redraw();return true;
}
bool Session::setBarColor(const std::wstring& value){
    if(stopped)return false;COLORREF color=tabcolor::defaultBackground();if(!value.empty()&&!tabcolor::parse(value,color))return false;
    settings.barColor=color;settings.customBarColor=!value.empty();settings.save();redraw();return true;
}
void Session::colorDialog(HWND owner){
    auto keep=shared_from_this();ColorChoice choice{settings.barColor,settings.customBarColor};
    struct Template {DLGTEMPLATE dialog;WORD menu,cls,title;} data{};data.dialog.style=WS_POPUP|WS_CAPTION|WS_SYSMENU|DS_MODALFRAME;data.dialog.cx=240;data.dialog.cy=125;
    if(DialogBoxIndirectParamW(module,&data.dialog,owner,colorProc,(LPARAM)&choice)==IDOK)setBarColor(choice.custom?tabcolor::format(choice.color):L"");
}
bool Session::setViewWidth(unsigned long long id,int width){
    auto v=find(id);if(!v||stopped||width<0||(width!=0&&!tablayout::validWidth(width,0)))return false;
    v->manualTabWidth=width;redraw();return true;
}
void Session::widthDialog(HWND owner){
    auto keep=shared_from_this();WidthChoice choice{settings.tabWidth,settings.widthMode};
    // DWORD-aligned DLGTEMPLATE with no resource-dependent controls.
    struct Template {DLGTEMPLATE dialog;WORD menu,cls,title;} data{};
    data.dialog.style=WS_POPUP|WS_CAPTION|WS_SYSMENU|DS_MODALFRAME;data.dialog.cx=220;data.dialog.cy=120;
    auto result=DialogBoxIndirectParamW(module,&data.dialog,owner,widthProc,(LPARAM)&choice);
    if(result==IDOK)setTabWidth(choice.width,choice.mode);
}
bool Session::newTabGroup(unsigned long long id,const POINT* position){
    auto keep=shared_from_this();auto v=find(id);if(!v||stopped||executing||refreshing||!ownedWindow(v->hwnd))return false;
    unsigned group=nextGroupId++;if(!moveToGroup(id,group))return false;
    v=find(id);if(!v)return false;
    RECT rect{};GetWindowRect(v->hwnd,&rect);MONITORINFO monitor{sizeof(monitor)};
    POINT point=position?*position:POINT{rect.left+40,rect.top+40};GetMonitorInfoW(MonitorFromPoint(point,MONITOR_DEFAULTTONEAREST),&monitor);
    int width=std::min(rect.right-rect.left,monitor.rcWork.right-monitor.rcWork.left),height=std::min(rect.bottom-rect.top,monitor.rcWork.bottom-monitor.rcWork.top);
    int left=std::clamp(point.x-40,monitor.rcWork.left,monitor.rcWork.right-width),top=std::clamp(point.y-24,monitor.rcWork.top,monitor.rcWork.bottom-height);
    if(IsZoomed(v->hwnd))ShowWindow(v->hwnd,SW_RESTORE);
    SetWindowPos(v->hwnd,nullptr,left,top,width,height,SWP_NOZORDER|SWP_NOACTIVATE);return true;
}
bool Session::drop(unsigned long long id,POINT screen){
    auto keep=shared_from_this();if(stopped||executing||refreshing)return false;auto source=find(id);if(!source)return false;
    HWND under=WindowFromPoint(screen),root=under?GetAncestor(under,GA_ROOT):nullptr;
    for(auto& pane:panes){auto target=find(pane.viewId);HWND bar=pane.control?pane.control->NativeWindow():nullptr;RECT bounds{};
        if(!target||target->independent||!bar||!IsWindowVisible(bar)||target->hwnd!=root||!GetWindowRect(bar,&bounds)||!PtInRect(&bounds,screen))continue;
        return moveToGroup(id,target->groupId);
    }
    return newTabGroup(id,&screen);
}
void Session::chooseGroup(HWND owner,unsigned long long id){
    auto keep=shared_from_this();auto source=find(id);if(!source)return;HMENU menu=CreatePopupMenu();std::map<unsigned,int> counts;
    for(auto& v:views)if(v->groupId)counts[v->groupId]++;
    std::vector<unsigned> groups;for(auto& pair:counts){std::wstring label=L"标签组 "+std::to_wstring(pair.first)+L"（"+std::to_wstring(pair.second)+L" 个窗口）";
        groups.push_back(pair.first);AppendMenuW(menu,MF_STRING|(source->groupId==pair.first?MF_CHECKED:0),(UINT)groups.size(),label.c_str());}
    AppendMenuW(menu,MF_SEPARATOR,0,nullptr);AppendMenuW(menu,MF_STRING,10000,L"新建标签组窗口");AppendMenuW(menu,MF_STRING,10001,L"独立窗口（隐藏标签栏）");
    if(counts.empty())AppendMenuW(menu,MF_STRING,10002,L"加入默认标签组");
    POINT point{};GetCursorPos(&point);UINT cmd=TrackPopupMenu(menu,TPM_RETURNCMD|TPM_NONOTIFY,point.x,point.y,0,owner,nullptr);DestroyMenu(menu);
    if(cmd&&cmd<=groups.size())moveToGroup(id,groups[cmd-1]);else if(cmd==10000)newTabGroup(id);else if(cmd==10001)setIndependent(id,true);else if(cmd==10002)moveToGroup(id,1);
}
std::wstring Session::diagnostics()const{
    std::wstring version=prop(app.get(),L"Version").str();DWORD registration=0,size=sizeof(registration);
    std::wstring key=L"Software\\Microsoft\\Office\\"+hostLabel(host)+L"\\Addins\\openOfficetab.Connect";
    LSTATUS status=RegGetValueW(HKEY_CURRENT_USER,key.c_str(),L"LoadBehavior",RRF_RT_REG_DWORD,nullptr,&registration,&size);
    size_t independent=0,mounted=0,visible=0;std::map<unsigned,int> groups;
    for(auto& v:views){if(v->independent)independent++;else groups[v->groupId]++;}
    for(auto& p:panes){HWND h=p.control?p.control->NativeWindow():nullptr;if(h&&IsWindow(h))mounted++;if(h&&IsWindowVisible(h))visible++;}
    return L"{\"version\":\"0.1.4\",\"host\":"+json(hostLabel(host))+L",\"officeVersion\":"+json(version)+L",\"architecture\":\""+(sizeof(void*)==8?std::wstring(L"x64"):std::wstring(L"x86"))+L"\",\"connected\":"+(stopped?L"false":L"true")+L",\"loadBehavior\":"+std::to_wstring(registration)+L",\"registrationReadable\":"+(status==ERROR_SUCCESS?L"true":L"false")+L",\"eventsConnected\":"+(cookie?L"true":L"false")+L",\"paneFactoryAvailable\":"+(factory?L"true":L"false")+L",\"viewCount\":"+std::to_wstring(views.size())+L",\"groupCount\":"+std::to_wstring(groups.size())+L",\"independentCount\":"+std::to_wstring(independent)+L",\"paneCount\":"+std::to_wstring(panes.size())+L",\"mountedCount\":"+std::to_wstring(mounted)+L",\"visiblePaneCount\":"+std::to_wstring(visible)+L",\"visibleSetting\":"+(settings.visible?L"true":L"false")+L",\"autoHide\":"+(settings.autoHide?L"true":L"false")+L",\"tabWidth\":"+std::to_wstring(settings.tabWidth)+L",\"widthMode\":"+std::to_wstring(settings.widthMode)+L",\"accentPosition\":"+std::to_wstring(settings.accentPosition)+L",\"barStyle\":"+std::to_wstring(settings.barStyle)+L",\"barColor\":"+json(tabcolor::format(settings.barColor))+L",\"customBarColor\":"+(settings.customBarColor?L"true":L"false")+L",\"closePending\":"+(pendingCloseId?L"true":L"false")+L",\"refreshFailures\":"+std::to_wstring(failures)+L",\"lastOperation\":"+json(lastOperation)+L",\"lastError\":"+json(hex(lastError))+L",\"privacy\":\"No document names, paths, content, account names or raw logs\"}";
}
void Session::showDiagnostics(HWND owner){
    auto keep=shared_from_this();std::wstring advice=L"openOfficetab 0.1.4\n\n";
    if(!factory)advice+=L"Office 尚未提供标签窗格工厂。请保存后重开 Office，或在安装器中检查和修复注册。\n";
    if(!settings.visible)advice+=L"标签栏当前被隐藏，可使用“恢复标签栏与窗口”。\n";
    if(settings.autoHide)advice+=L"已启用单标签自动隐藏。\n";
    if(failures)advice+=L"Office 调用最近被拒绝，可能正忙或有对话框；关闭对话框后重试。\n";
    if(views.empty())advice+=L"当前没有普通编辑视图；受保护视图请通过 Office 原提示进入编辑。\n";
    advice+=L"\n以下诊断不含文件名、路径、正文或账号：\n"+diagnostics();MessageBoxW(owner,advice.c_str(),L"加载诊断",MB_OK|MB_ICONINFORMATION);
}
void Session::exportDiagnostics(HWND owner){
    auto keep=shared_from_this();wchar_t file[MAX_PATH]=L"openOfficetab-diagnostics.json";OPENFILENAMEW request{sizeof(request)};
    request.hwndOwner=owner;request.lpstrFile=file;request.nMaxFile=MAX_PATH;request.lpstrFilter=L"JSON 诊断文件\0*.json\0\0";request.lpstrDefExt=L"json";request.Flags=OFN_OVERWRITEPROMPT|OFN_PATHMUSTEXIST|OFN_NOCHANGEDIR;
    if(!GetSaveFileNameW(&request))return;
    auto data=diagnostics();int length=WideCharToMultiByte(CP_UTF8,0,data.data(),(int)data.size(),nullptr,0,nullptr,nullptr);std::string bytes(length,0);WideCharToMultiByte(CP_UTF8,0,data.data(),(int)data.size(),bytes.data(),length,nullptr,nullptr);
    HANDLE output=CreateFileW(file,GENERIC_WRITE,0,nullptr,CREATE_ALWAYS,FILE_ATTRIBUTE_NORMAL,nullptr);DWORD written=0;
    bool ok=output!=INVALID_HANDLE_VALUE&&WriteFile(output,bytes.data(),(DWORD)bytes.size(),&written,nullptr)&&written==bytes.size();if(output!=INVALID_HANDLE_VALUE)CloseHandle(output);
    MessageBoxW(owner,ok?L"已导出脱敏诊断，不包含文档名称、路径、正文、账号或原始日志。":L"诊断文件写入失败，请选择可写目录。",L"导出诊断",MB_OK|(ok?MB_ICONINFORMATION:MB_ICONERROR));
}
