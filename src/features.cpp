#include "model.h"
#include <wincrypt.h>

namespace {
// File identity at the module's loaded path, cached outside input callbacks.
// Only hex leaves this helper; paths and filesystem errors are not exported.
const std::wstring& moduleSha256(){
    static const std::wstring value=[]()->std::wstring{
        if(!module)return L"";std::vector<wchar_t> path(32768);DWORD length=GetModuleFileNameW(module,path.data(),(DWORD)path.size());
        if(!length||length>=path.size())return L"";
        HANDLE file=CreateFileW(path.data(),GENERIC_READ,FILE_SHARE_READ|FILE_SHARE_DELETE,nullptr,OPEN_EXISTING,FILE_ATTRIBUTE_NORMAL,nullptr);
        if(file==INVALID_HANDLE_VALUE)return L"";
        HCRYPTPROV provider=0;HCRYPTHASH hash=0;LARGE_INTEGER size{};std::wstring result;
        bool ok=GetFileSizeEx(file,&size)&&size.QuadPart>0&&size.QuadPart<=32*1024*1024&&CryptAcquireContextW(&provider,nullptr,nullptr,PROV_RSA_AES,CRYPT_VERIFYCONTEXT)&&CryptCreateHash(provider,CALG_SHA_256,0,0,&hash);
        BYTE bytes[32768];DWORD read=0;
        while(ok){if(!ReadFile(file,bytes,sizeof(bytes),&read,nullptr)){ok=false;break;}if(!read)break;if(!CryptHashData(hash,bytes,read,0)){ok=false;break;}}
        BYTE digest[32];DWORD digestSize=sizeof(digest);
        if(ok&&CryptGetHashParam(hash,HP_HASHVAL,digest,&digestSize,0)&&digestSize==sizeof(digest)){const wchar_t* hex=L"0123456789ABCDEF";for(auto byte:digest){result+=hex[byte>>4];result+=hex[byte&15];}}
        if(hash)CryptDestroyHash(hash);if(provider)CryptReleaseContext(provider,0);CloseHandle(file);return result;
    }();return value;
}
}
#include "appearance-dialog.h"
#include "rename-dialog.h"

namespace {
bool ownedWindow(HWND window){DWORD pid=0;GetWindowThreadProcessId(window,&pid);return window&&IsWindow(window)&&pid==GetCurrentProcessId();}
std::wstring hostLabel(Host host){return host==Host::Word?L"Word":host==Host::Excel?L"Excel":L"PowerPoint";}
using renameui::RenameChoice;
using renameui::renameProc;
std::wstring hex(HRESULT hr){wchar_t text[16];swprintf_s(text,L"0x%08X",(unsigned)hr);return text;}
}
bool Session::openManagement(HWND owner,const wchar_t* page){
    if(wcscmp(page,L"overview")&&wcscmp(page,L"appearance")&&wcscmp(page,L"repair")&&wcscmp(page,L"updates"))return false;
    wchar_t buffer[32768]{};DWORD size=sizeof(buffer);
    if(RegGetValueW(HKEY_CURRENT_USER,L"Software\\openOfficetab",L"CenterPath",RRF_RT_REG_SZ,nullptr,buffer,&size)!=ERROR_SUCCESS)return false;
    std::wstring path=buffer,prefix=dataDir()+L"\\app\\",suffix=L"\\openOfficetab.Center.exe";
    if(path.size()!=prefix.size()+12+suffix.size()||_wcsnicmp(path.c_str(),prefix.c_str(),prefix.size())||_wcsicmp(path.c_str()+prefix.size()+12,suffix.c_str()))return false;
    for(size_t i=prefix.size();i<prefix.size()+12;i++)if(!((path[i]>=L'0'&&path[i]<=L'9')||(path[i]>=L'a'&&path[i]<=L'f')))return false;
    DWORD attributes=GetFileAttributesW(path.c_str());if(attributes==INVALID_FILE_ATTRIBUTES||(attributes&FILE_ATTRIBUTE_DIRECTORY))return false;
    std::wstring args=L"--page "+std::wstring(page)+L" --host "+hostLabel(host);
    return (INT_PTR)ShellExecuteW(owner,L"open",path.c_str(),args.c_str(),nullptr,SW_SHOWNORMAL)>32;
}
bool Session::setLanguage(int mode){
    if(stopped||!i18n::valid(mode))return false;
    settings.synchronize();settings.language=mode;bool saved=settings.save();refreshLanguage();return saved;
}
void Session::refreshLanguage(){
    int current=language();if(current==lastLanguage)return;
    for(auto& pane:panes)if(pane.control){
        auto native=pane.control->NativeWindow();auto ole=pane.control->OleWindow();
        if(native&&IsWindow(native))SetWindowTextW(native,tr(L"文档标签"));
        if(ole&&ole!=native&&IsWindow(ole))SetWindowTextW(ole,tr(L"文档标签"));
    }
    // Invalidate cached labels; keep the existing Ribbon and document windows.
    bool invalidated=true;if(ribbon)try{call(ribbon.get(),L"Invalidate",DISPATCH_METHOD);}catch(...){invalidated=false;log(L"language.ribbon.retry");}
    lastLanguage=invalidated?current:0;redraw();
}
bool Session::setTabWidth(int width,int mode){
    if(!tablayout::validWidth(width,mode)||stopped)return false;settings.tabWidth=width;settings.widthMode=mode;
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
    auto keep = shared_from_this();
    appearanceui::ColorChoice choice{settings.barColor, settings.customBarColor, {language(), host, darkMode()}};
    appearanceui::Template data;
    if (DialogBoxIndirectParamW(module, &data.dialog, owner, appearanceui::colorProc, (LPARAM)&choice) == IDOK) {
        setBarColor(choice.custom ? tabcolor::format(choice.color) : L"");
    }
}
bool Session::setViewWidth(unsigned long long id,int width){
    auto v=find(id);if(!v||stopped||width<0||(width!=0&&!tablayout::validWidth(width,0)))return false;
    v->manualTabWidth=width;persistView(*v);redraw();return true;
}
void Session::renameDialog(HWND owner,unsigned long long id){
    auto keep=shared_from_this();auto v=find(id);
    if(stopped||executing||refreshing||!v||v->readOnly||!storage::localFile(v->path))return;
    RenameChoice choice{this,id,v->path,language()};
    struct Template {DLGTEMPLATE dialog;WORD menu,cls,title;} data{};
    data.dialog.style=WS_POPUP|WS_CAPTION|WS_SYSMENU|WS_CLIPCHILDREN|DS_MODALFRAME;data.dialog.cx=290;data.dialog.cy=125;
    if(DialogBoxIndirectParamW(module,&data.dialog,owner,renameProc,(LPARAM)&choice)!=IDOK)return;
    const auto result=renameFile(id,choice.name);
    if(result==documentrename::Result::renamed)return;
    auto active=find(activeId);
    HWND messageOwner=active&&IsWindow(active->hwnd)?active->hwnd:(IsWindow(owner)?owner:nullptr);
    const wchar_t* message=L"未完成重命名。请检查同名文件、文件格式或 Office 提示；多窗口文档和仅修改大小写暂不支持。";
    if(result==documentrename::Result::reopenFailed)
        message=L"文件已重命名。请从新路径手动打开文档。";
    if(result==documentrename::Result::originalRetained)
        message=L"已切换到新文件，原文件仍保留在原路径。";
    MessageBoxW(messageOwner,tr(message),tr(L"重命名文件"),MB_OK|MB_ICONINFORMATION);
}
void Session::widthDialog(HWND owner){
    auto keep = shared_from_this();
    appearanceui::WidthChoice choice{settings.tabWidth, settings.widthMode, {language(), host, darkMode()}};
    appearanceui::Template data;
    if (DialogBoxIndirectParamW(module, &data.dialog, owner, appearanceui::widthProc, (LPARAM)&choice) == IDOK) {
        setTabWidth(choice.width, choice.mode);
    }
}
bool Session::newTabGroup(unsigned long long id,const POINT* position){
    logEvent(L"group.new.request",{{"viewId",(long long)id}});
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
    logEvent(L"tab.drop.request",{{"viewId",(long long)id},{"x",screen.x},{"y",screen.y}});
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
    std::vector<unsigned> groups;for(auto& pair:counts){std::wstring label=i18n::groupLabel(language(),pair.first,pair.second);
        groups.push_back(pair.first);AppendMenuW(menu,MF_STRING|(source->groupId==pair.first?MF_CHECKED:0),(UINT)groups.size(),label.c_str());}
    AppendMenuW(menu,MF_SEPARATOR,0,nullptr);AppendMenuW(menu,MF_STRING,10000,tr(L"新建标签组窗口"));AppendMenuW(menu,MF_STRING,10001,tr(L"独立窗口（隐藏标签栏）"));
    if(counts.empty())AppendMenuW(menu,MF_STRING,10002,tr(L"加入默认标签组"));
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
    return L"{\"version\":\"0.2.3\",\"host\":"+json(hostLabel(host))+L",\"officeVersion\":"+json(version)+L",\"architecture\":\""+(sizeof(void*)==8?std::wstring(L"x64"):std::wstring(L"x86"))+L"\",\"connected\":"+(stopped?L"false":L"true")+L",\"loadBehavior\":"+std::to_wstring(registration)+L",\"registrationReadable\":"+(status==ERROR_SUCCESS?L"true":L"false")+L",\"eventsConnected\":"+(cookie?L"true":L"false")+L",\"paneFactoryAvailable\":"+(factory?L"true":L"false")+L",\"viewCount\":"+std::to_wstring(views.size())+L",\"groupCount\":"+std::to_wstring(groups.size())+L",\"independentCount\":"+std::to_wstring(independent)+L",\"paneCount\":"+std::to_wstring(panes.size())+L",\"mountedCount\":"+std::to_wstring(mounted)+L",\"visiblePaneCount\":"+std::to_wstring(visible)+L",\"visibleSetting\":"+(settings.visible?L"true":L"false")+L",\"autoHide\":"+(settings.autoHide?L"true":L"false")+L",\"tabWidth\":"+std::to_wstring(settings.tabWidth)+L",\"widthMode\":"+std::to_wstring(settings.widthMode)+L",\"accentPosition\":"+std::to_wstring(settings.accentPosition)+L",\"barStyle\":"+std::to_wstring(settings.barStyle)+L",\"barColor\":"+json(tabcolor::format(settings.barColor))+L",\"customBarColor\":"+(settings.customBarColor?L"true":L"false")+L",\"singleTaskbarSetting\":"+(settings.group?L"true":L"false")+L",\"languageSetting\":"+std::to_wstring(settings.language)+L",\"language\":"+json(language()==1?L"zh-CN":L"en")+L",\"themeMode\":"+std::to_wstring(settings.themeMode)+L",\"frameCloseAction\":"+std::to_wstring(settings.frameCloseAction)+L",\"observedRootCount\":"+std::to_wstring(rootObservers.size())+L",\"windowCoordination\":true,\"dark\":"+(darkMode()?L"true":L"false")+L",\"barBackground\":"+json(tabcolor::format((darkMode()&&settings.barStyle==1?surroundColor():tabcolor::background(darkMode(),settings.barStyle==1,settings.barColor,settings.customBarColor))))+L",\"surroundBackground\":"+json(tabcolor::format(surroundColor()))+L",\"settingsLiveSync\":true,\"settingsWritePending\":"+(settings.savePending?L"true":L"false")+L",\"recentlyClosedCount\":"+std::to_wstring(recentlyClosed().size())+L",\"closePending\":"+(pendingCloseId?L"true":L"false")+L",\"refreshFailures\":"+std::to_wstring(failures)+L",\"lastOperation\":"+json(lastOperation)+L",\"lastError\":"+json(hex(lastError))+L",\"inputTraceAvailable\":"+(inputTraceAvailable()?L"true":L"false")+L",\"moduleSha256\":"+json(moduleSha256())+L",\"moduleHashReadable\":"+(!moduleSha256().empty()?L"true":L"false")+L",\"logSchema\":1,\"droppedLogRecords\":"+std::to_wstring(runtimelog::recorder().lost())+L",\"privacy\":\"No document names, paths, content, account names or legacy raw logs\"}";
}
void Session::showDiagnostics(HWND owner){
    auto keep=shared_from_this();std::wstring advice=L"openOfficeTab 0.2.3\n\n";
    if(!factory)advice+=tr(L"Office 尚未提供标签窗格工厂。请保存后重开 Office，或在安装器中检查和修复注册。\n");
    if(!settings.visible)advice+=tr(L"标签栏当前被隐藏，可使用“恢复标签栏与窗口”。\n");
    if(settings.autoHide)advice+=tr(L"已启用单标签自动隐藏。\n");
    if(failures)advice+=tr(L"Office 调用最近被拒绝，可能正忙或有对话框；关闭对话框后重试。\n");
    if(views.empty())advice+=tr(L"当前没有普通编辑视图；受保护视图请通过 Office 原提示进入编辑。\n");
    advice+=std::wstring(tr(L"\n以下诊断不含文件名、路径、正文或账号：\n"))+diagnostics();MessageBoxW(owner,advice.c_str(),tr(L"加载诊断"),MB_OK|MB_ICONINFORMATION);
}
void Session::exportDiagnostics(HWND owner){
    auto keep=shared_from_this();wchar_t file[MAX_PATH]=L"openOfficetab-diagnostics.json";OPENFILENAMEW request{sizeof(request)};
    request.hwndOwner=owner;request.lpstrFile=file;request.nMaxFile=MAX_PATH;request.lpstrFilter=tr(L"JSON 诊断文件\0*.json\0\0");request.lpstrDefExt=L"json";request.Flags=OFN_OVERWRITEPROMPT|OFN_PATHMUSTEXIST|OFN_NOCHANGEDIR;
    if(!GetSaveFileNameW(&request))return;
    log(L"diagnostics.export");runtimelog::recorder().flush();
    auto data=L"{\"snapshot\":"+diagnostics()+L",\"runtimeTimeline\":"+runtimelog::recorder().snapshot()+L",\"droppedLogRecords\":"+std::to_wstring(runtimelog::recorder().lost())+L"}";int length=WideCharToMultiByte(CP_UTF8,0,data.data(),(int)data.size(),nullptr,0,nullptr,nullptr);std::string bytes(length,0);WideCharToMultiByte(CP_UTF8,0,data.data(),(int)data.size(),bytes.data(),length,nullptr,nullptr);
    HANDLE output=CreateFileW(file,GENERIC_WRITE,0,nullptr,CREATE_ALWAYS,FILE_ATTRIBUTE_NORMAL,nullptr);DWORD written=0;
    bool ok=output!=INVALID_HANDLE_VALUE&&WriteFile(output,bytes.data(),(DWORD)bytes.size(),&written,nullptr)&&written==bytes.size();if(output!=INVALID_HANDLE_VALUE)CloseHandle(output);
    MessageBoxW(owner,ok?tr(L"已导出脱敏诊断，不包含文档名称、路径、正文、账号或原始日志。"):tr(L"诊断文件写入失败，请选择可写目录。"),tr(L"导出诊断"),MB_OK|(ok?MB_ICONINFORMATION:MB_ICONERROR));
}
