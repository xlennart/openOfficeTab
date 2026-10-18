#include "model.h"
#include "layout.h"

namespace {
std::wstring name(Host host){return host==Host::Word?L"Word":host==Host::Excel?L"Excel":L"PowerPoint";}
}
void Session::persistView(const View& view){
    if(!storage::localFile(view.path))return;
    storage::FilePreference pref{view.manualTabWidth,view.independent};
    auto key=storage::normalize(view.path);for(auto it=pendingPreferences.begin();it!=pendingPreferences.end();)if(storage::normalize(it->first)==key)it=pendingPreferences.erase(it);else ++it;
    if(!storage::Store(storageRoot).savePreference(name(host),view.path,pref))pendingPreferences[view.path]=pref;
}
void Session::initializePreference(View& view,bool isNew,const std::wstring& oldPath){
    if(!storage::localFile(view.path))return;
    if(isNew){storage::FilePreference pref;if(storage::Store(storageRoot).preference(name(host),view.path,pref)){
        view.manualTabWidth=pref.width;view.independent=pref.independent;
        if(pref.independent){if(view.groupId)view.lastGroupId=view.groupId;view.groupId=0;}
        else if(!view.groupId){view.groupId=view.lastGroupId?view.lastGroupId:1;}
    }}else if(storage::normalize(oldPath)!=storage::normalize(view.path))persistView(view); // first save / Save As
}
void Session::flushPersistence(unsigned milliseconds){
    storage::Store store(storageRoot);
    auto until=GetTickCount64()+milliseconds;
    do{
        for(auto it=pendingPreferences.begin();it!=pendingPreferences.end();)if(store.savePreference(name(host),it->first,it->second))it=pendingPreferences.erase(it);else break;
        for(auto it=pendingRecent.begin();it!=pendingRecent.end();)if(store.closed(name(host),*it))it=pendingRecent.erase(it);else break;
        if(settings.savePending)settings.save();
        if((pendingPreferences.empty()&&pendingRecent.empty()&&!settings.savePending)||GetTickCount64()>=until)break;
        Sleep(1);
    }while(milliseconds);
}
void Session::pollPersistence(){
    pollTheme();
    auto view=find(activeId);if(executing||(view&&IsWindow(view->hwnd)&&!IsWindowEnabled(view->hwnd)))return;
    storage::Store store(storageRoot);flushPersistence();
    auto now=GetTickCount64();
    // Read a small atomic snapshot, rather than relying only on filesystem
    // timestamp resolution. Two quick writes can have the same timestamp.
    if(settings.savePending||now-lastSettingsPoll>=250){
        lastSettingsPoll=now;bool wasGrouped=settings.group;bool changed=settings.synchronize();
        if(!settings.savePending)settingsStamp=store.stamp(L"settings.ini");
        if(changed){if(wasGrouped&&!settings.group)restore();updateTaskbar();redraw();}
    }
    refreshLanguage();
}
bool Session::resetFilePreferences(unsigned long long id){
    auto view=find(id);if(!view||stopped||executing||refreshing||!storage::localFile(view->path))return false;
    if(!storage::Store(storageRoot).resetPreference(name(host),view->path))return false;
    pendingPreferences.erase(view->path);view->manualTabWidth=0;
    if(view->independent&&!moveToGroup(id,view->lastGroupId?view->lastGroupId:1))return false;
    // moveToGroup persists membership; erase again to leave a true default.
    storage::Store(storageRoot).resetPreference(name(host),view->path);pendingPreferences.erase(view->path);redraw();return true;
}
bool Session::resetAllFilePreferences(){
    if(stopped||executing||refreshing)return false;
    if(!storage::Store(storageRoot).resetPreference(name(host)))return false;
    pendingPreferences.clear();for(auto& view:views)view->manualTabWidth=0;
    // Current independent documents keep their window membership. Reopening
    // reads the reset defaults; resetting widths never closes/moves files.
    redraw();return true;
}
std::vector<std::wstring> Session::recentlyClosed()const{return storage::Store(storageRoot).recent(name(host));}
void Session::rememberClosed(const std::wstring& file){
    if(!storage::localFile(file))return;
    if(!storage::Store(storageRoot).closed(name(host),file)){
        auto key=storage::normalize(file);pendingRecent.erase(std::remove_if(pendingRecent.begin(),pendingRecent.end(),[&](auto& old){return storage::normalize(old)==key;}),pendingRecent.end());pendingRecent.push_back(file);
    }
}
bool Session::clearRecentlyClosed(){if(stopped)return false;if(!storage::Store(storageRoot).clearRecent(name(host)))return false;pendingRecent.clear();return true;}
bool Session::reopenPath(const std::wstring& file){
    auto keep=shared_from_this();if(stopped||executing||refreshing||!storage::localFile(file))return false;
    auto key=storage::normalize(file);for(auto& view:views)if(storage::normalize(view->path)==key){activate(view->id);return true;}
    DWORD attributes=GetFileAttributesW(file.c_str());if(attributes==INVALID_FILE_ATTRIBUTES||(attributes&FILE_ATTRIBUTE_DIRECTORY)){
        auto active=find(activeId);MessageBoxW(active?active->hwnd:nullptr,tr(L"文件已移动、删除或当前无法访问。最近关闭记录仍然保留，可从菜单清空。"),tr(L"重新打开文档"),MB_OK|MB_ICONINFORMATION);return false;}
    try{
        executing=true;creating=true;pendingNewDocument=true;pendingNewSince=GetTickCount64();newDocumentGroup=currentGroup()?currentGroup():1;
        auto docs=call(app.get(),host==Host::Word?L"Documents":host==Host::Excel?L"Workbooks":L"Presentations").disp();
        call(docs.get(),L"Open",DISPATCH_METHOD,{Var(file)});creating=false;executing=false;
    }catch(const ComError& e){creating=false;executing=false;pendingNewDocument=false;report(e);return false;}
    catch(...){creating=false;executing=false;pendingNewDocument=false;throw;}
    return refresh();
}
bool Session::reopenClosed(unsigned index){auto files=recentlyClosed();return index<files.size()&&reopenPath(files[index]);}
void Session::recentMenu(HWND owner,POINT point){
    auto keep=shared_from_this();auto files=recentlyClosed();HMENU menu=CreatePopupMenu();
    for(size_t i=0;i<files.size();i++){auto slash=files[i].find_last_of(L"\\/");std::wstring label=slash==std::wstring::npos?files[i]:files[i].substr(slash+1)+L"  —  "+files[i].substr(0,slash);if(label.size()>220)label=label.substr(0,217)+L"…";
        std::wstring escaped;for(auto c:label){escaped+=c;if(c==L'&')escaped+=c;}AppendMenuW(menu,MF_STRING,(UINT)i+1,escaped.c_str());}
    if(files.empty())AppendMenuW(menu,MF_STRING|MF_GRAYED,0,tr(L"暂无最近关闭的文件"));
    AppendMenuW(menu,MF_SEPARATOR,0,nullptr);AppendMenuW(menu,MF_STRING|(files.empty()?MF_GRAYED:0),100,tr(L"清空最近关闭记录"));
    UINT cmd=TrackPopupMenu(menu,TPM_RETURNCMD|TPM_NONOTIFY,point.x,point.y,0,owner,nullptr);DestroyMenu(menu);
    if(cmd&&cmd<=files.size())reopenPath(files[cmd-1]);else if(cmd==100)clearRecentlyClosed();
}
