#include "model.h"
#include "layout.h"
#include "window-layout.h"
#include "tab-color.h"

// Compact only the task pane reached through our own ActiveX ancestry. Office
// draws the title inside its client area. A separate root child renders in the
// reserved strip, while only our own task-pane frame is hidden.
class PaneChrome {
    HWND frame=nullptr;
    Ptr<ITabBinding> control;
    static LRESULT CALLBACK proc(HWND w,UINT m,WPARAM a,LPARAM b,UINT_PTR,DWORD_PTR data){
        auto self=(PaneChrome*)data;
        auto keep=self->control;
        // Office may show its reserve frame while rebuilding a document view.
        // Keep that frame hidden before the show is committed, rather than
        // showing its title chrome and immediately hiding it again.
        if(m==WM_WINDOWPOSCHANGING&&keep&&keep->NativeWindow()!=keep->OleWindow()&&windowlayout::reserveFrame(w,keep->OleWindow(),GetAncestor(keep->NativeWindow(),GA_ROOT))){
            auto pos=(WINDOWPOS*)b;
            if(pos->flags&SWP_SHOWWINDOW)pos->flags=(pos->flags&~SWP_SHOWWINDOW)|SWP_HIDEWINDOW;
        }
        if(m==WM_NCDESTROY){RemoveWindowSubclass(w,proc,110);self->frame=nullptr;if(keep)keep->Mount(nullptr);}
        else if((m==WM_SIZE||m==WM_WINDOWPOSCHANGED)&&keep)keep->Mount(w);
        return DefSubclassProc(w,m,a,b);
    }
public:
    ~PaneChrome(){detach();}
    bool mounted()const{return frame&&IsWindow(frame);}
    HWND window()const{return frame;}
    bool attach(ITabBinding* binding){
        HWND h=binding->OleWindow();bool foundContent=false;
        for(int i=0;h&&i<12;i++,h=GetParent(h)){
            wchar_t cls[128];GetClassNameW(h,cls,128);
            if(!_wcsicmp(cls,L"MsoWorkPane"))foundContent=true;
            if(foundContent&&!_wcsicmp(cls,L"MsoCommandBar")){
                if(!windowlayout::reserveFrame(h,binding->OleWindow(),GetAncestor(binding->NativeWindow(),GA_ROOT))){log(L"chrome.frameRejected");return false;}
                if(!SetWindowSubclass(h,proc,110,(DWORD_PTR)this)){log((L"chrome.subclassFailed.current="+std::to_wstring(GetCurrentThreadId())+L".frame="+std::to_wstring(GetWindowThreadProcessId(h,nullptr))+L".control="+std::to_wstring(GetWindowThreadProcessId(binding->NativeWindow(),nullptr))).c_str(),HRESULT_FROM_WIN32(GetLastError()));return false;}
                control=Ptr<ITabBinding>(binding);frame=h;
                if(!control->Mount(h)){log(L"chrome.mountFailed");detach();return false;}
                log(L"chrome.compactMounted");return true;
            }
        }return false;
    }
    void layout(){if(frame&&control)control->Mount(frame);}
    void detach(){if(frame&&IsWindow(frame)){RemoveWindowSubclass(frame,proc,110);if(control)control->Unmount();}frame=nullptr;control.reset();}
};

namespace {
constexpr UINT REFRESH=WM_APP+110;
constexpr UINT NATIVE_CLOSE=WM_APP+111;
constexpr UINT FRAME_CLOSE=WM_APP+112;
LRESULT CALLBACK sessionProc(HWND w,UINT m,WPARAM a,LPARAM b){
    auto s=(Session*)GetWindowLongPtrW(w,GWLP_USERDATA);
    if(m==WM_NCCREATE){s=(Session*)((CREATESTRUCTW*)b)->lpCreateParams;SetWindowLongPtrW(w,GWLP_USERDATA,(LONG_PTR)s);}
    if(s&&m==FRAME_CLOSE){auto keep=s->shared_from_this();s->processFrameClose();return 0;}
    if(s&&m==NATIVE_CLOSE){auto keep=s->shared_from_this();s->nativeClose();return 0;}
    if(s&&m==WM_TIMER&&a==3){auto keep=s->shared_from_this();s->nativeClose();return 0;}
    if(s&&m==WM_TIMER&&a==4){try{auto keep=s->shared_from_this();s->processWindowLayout();}catch(...){log(L"session.layout.exception",E_FAIL);}return 0;}
    if(s&&(m==WM_TIMER||m==REFRESH)){if(m==REFRESH)s->refreshPending=false;if(m==WM_TIMER&&a==2)KillTimer(w,2);try{auto keep=s->shared_from_this();if(s->windowFrameBusy()||s->coordinatingWindows){SetTimer(w,2,80,nullptr);return 0;}if(s->refresh()){s->finishClose();}else if((s->executing||s->refreshing)&&m==WM_TIMER&&a==2)SetTimer(w,2,20,nullptr);}catch(...){log(L"session.refresh.exception",E_FAIL);}return 0;}
    return DefWindowProcW(w,m,a,b);
}
struct Flag {bool& value;explicit Flag(bool& x):value(x){value=true;}~Flag(){value=false;}};
bool valid(HWND h){DWORD pid=0;GetWindowThreadProcessId(h,&pid);return h&&IsWindow(h)&&pid==GetCurrentProcessId();}
std::wstring hostName(Host h){return h==Host::Word?L"Word":h==Host::Excel?L"Excel":L"PowerPoint";}
}
Session::Session(IDispatch* application,const std::wstring& root):app(application),storageRoot(root.empty()?dataDir():root){
    std::wstring name=call(app.get(),L"Name").str();
    if(name.find(L"Excel")!=std::wstring::npos)host=Host::Excel;
    else if(name.find(L"PowerPoint")!=std::wstring::npos)host=Host::PowerPoint;
    else if(name.find(L"Word")!=std::wstring::npos)host=Host::Word;
    else throw ComError(E_NOTIMPL,L"Unsupported Office host");
    settings.root=storageRoot;settings.load();pollTheme(true);settingsStamp=storage::Store(storageRoot).stamp(L"settings.ini");
}
Session::~Session(){stop();}
void Session::start(){
    if(timer||stopped)return;
    WNDCLASSEXW c{sizeof(c)};c.lpfnWndProc=sessionProc;c.hInstance=module;c.lpszClassName=L"openOfficetab.Session";RegisterClassExW(&c);
    timer=CreateWindowExW(0,c.lpszClassName,L"",0,0,0,0,0,HWND_MESSAGE,nullptr,module,this);
    if(!timer)throw ComError(HRESULT_FROM_WIN32(GetLastError()),L"SessionWindow");
    IID events;CLSIDFromString(host==Host::Word?L"{00020A01-0000-0000-C000-000000000046}":host==Host::Excel?L"{00024413-0000-0000-C000-000000000046}":L"{914934C2-5A91-11CF-8700-00AA0060263B}",&events);
    auto cp=app.as<IConnectionPointContainer>();
    if(cp&&SUCCEEDED(cp->FindConnectionPoint(events,connection.out()))){sink=Ptr<EventSink>(new EventSink(this,events),false);HRESULT hr=connection->Advise(sink.get(),&cookie);if(FAILED(hr)){connection.reset();sink.reset();log(L"events.advise.failed",hr);}else log(L"events.connected");}
    HRESULT hr=CoCreateInstance(CLSID_TaskbarList,nullptr,CLSCTX_INPROC_SERVER,IID_ITaskbarList,(void**)taskbar.out());if(SUCCEEDED(hr)&&FAILED(taskbar->HrInit()))taskbar.reset();
    SetTimer(timer,1,750,nullptr);defer();log(L"session.started");
}
void Session::stop(){
    if(stopped)return;stopped=true;detachWindowObservers();
    try{flushPersistence(200);}catch(...){log(L"state.shutdown.flushFailed",E_FAIL);}
    if(timer){KillTimer(timer,1);KillTimer(timer,2);DestroyWindow(timer);timer=nullptr;}
    if(sink)sink->detach();if(connection&&cookie)connection->Unadvise(cookie);cookie=0;connection.reset();sink.reset();
    restore();
    for(auto& p:panes){if(p.control)p.control->Bind(nullptr,nullptr);p.chrome.reset();if(p.control)p.control->Unmount();try{call(p.office.get(),L"Delete",DISPATCH_METHOD);}catch(...){}}
    panes.clear();views.clear();factory.reset();ribbon.reset();app.reset();taskbar.reset();log(L"session.stopped");
}
void Session::defer(){if(timer&&!stopped&&!refreshPending){refreshPending=true;if(!PostMessageW(timer,REFRESH,0,0))refreshPending=false;}}
void Session::bindFactory(IDispatch* f){factory=Ptr<IDispatch>(f);log(L"factory.available");defer();}
void Session::event(DISPID id,DISPPARAMS* params){
    if(!stopped&&params&&params->cArgs&&((host==Host::Word&&id==11&&params->cArgs>=2)||(host==Host::Excel&&id==2911&&params->cArgs>=2)||(host==Host::PowerPoint&&(id==2005||id==2031)))){
        UINT index=(host==Host::Word||host==Host::Excel)?1:0;Var document;VariantCopyInd(&document.v,&params->rgvarg[index]);auto object=document.disp();
        auto path=prop(object.get(),L"Path").str().empty()?L"":prop(object.get(),L"FullName").str();
        if(!path.empty())for(auto& view:views)if(object&&object.as<IUnknown>().get()==view->document.as<IUnknown>().get()){
            auto old=view->path;view->path=path;initializePreference(*view,false,old);}
    }
    if(host==Host::PowerPoint&&id==2031&&params&&params->cArgs){
        Var value;VariantCopyInd(&value.v,&params->rgvarg[0]);auto closing=value.disp();
        for(auto& view:views)if(closing&&closing.as<IUnknown>().get()==view->document.as<IUnknown>().get()){rememberClosed(view->path);break;}
    }
    if(host==Host::PowerPoint&&id==2004&&pendingCloseId&&params&&params->cArgs){
        Var value;VariantCopyInd(&value.v,&params->rgvarg[0]);auto closing=value.disp();auto target=find(pendingCloseId);
        if(target&&closing&&closing.as<IUnknown>().get()==target->document.as<IUnknown>().get())pendingCloseAccepted=true;
    }
    if(stopped)return;
    // Word reports WindowActivate with a complete window/document pair. Mount
    // that view directly in the same STA event, before waiting for an idle
    // full-collection refresh. NewDocument arrives too early for a window.
    if(host==Host::Word&&id==9)return;
    if(host==Host::Word&&id==10&&params&&params->cArgs>=2&&!coordinatingWindows&&!windowFrameBusy()&&!refreshing&&(!executing||creating)){
        Var w,d;VariantCopy(&w.v,&params->rgvarg[0]);VariantCopy(&d.v,&params->rgvarg[1]);
        auto window=w.disp(),document=d.disp();
        HWND root=(HWND)(UINT_PTR)(DWORD)prop(window.get(),L"Hwnd").number();
        if(window&&document&&valid(root)){
            Flag busy(refreshing);
            std::shared_ptr<View> view;
            for(auto& old:views)if(old->hwnd==root){view=old;break;}
            if(!view){
                view=std::make_shared<View>();view->id=nextId++;view->hwnd=root;view->identity=window.as<IUnknown>();GetWindowPlacement(root,&view->original);
                view->groupId=pendingNewDocument?newDocumentGroup:currentGroup();view->independent=view->groupId==0;view->lastGroupId=view->groupId?view->groupId:1;pendingNewDocument=false;
                view->window=window;view->document=document;view->title=call(document.get(),L"Name").str();
                view->saved=prop(document.get(),L"Saved").boolean(true);view->readOnly=prop(document.get(),L"ReadOnly").boolean();
                view->path=prop(document.get(),L"Path").str().empty()?L"":prop(document.get(),L"FullName").str();initializePreference(*view,true,L"");views.push_back(view);
            }
            activeId=view->id;if(!view->independent){groupActiveId=view->id;groupActives[view->groupId]=view->id;}syncPanes();syncWindowObservers();redraw();
        }
    }
    defer();
}
std::shared_ptr<View> Session::find(unsigned long long id)const{for(auto& v:views)if(v->id==id)return v;return {};}
unsigned Session::groupForWindow(HWND root)const{
    if(root)root=GetAncestor(root,GA_ROOT);for(auto& v:views)if(v->hwnd==root)return v->groupId;return currentGroup();
}
unsigned Session::currentGroup()const{auto v=find(activeId);return v?v->groupId:1;}
std::vector<std::shared_ptr<View>> Session::tabViews(HWND root)const{
    unsigned group=root?groupForWindow(root):0;
    std::vector<std::shared_ptr<View>> result;for(auto& v:views)if(!v->independent&&(!root||v->groupId==group))result.push_back(v);return result;
}
unsigned long long Session::activeTabId(HWND root)const{
    unsigned group=root?groupForWindow(root):currentGroup();
    auto it=groupActives.find(group);auto current=it==groupActives.end()?nullptr:find(it->second);
    if(current&&!current->independent&&current->groupId==group)return current->id;
    for(auto& v:views)if(!v->independent&&v->groupId==group)return v->id;return 0;
}
bool Session::showTabs(HWND root)const{
    for(auto& v:views)if(v->hwnd==root)return settings.visible&&!v->independent&&(!settings.autoHide||tabViews(root).size()>1);
    return false;
}
bool Session::setIndependent(unsigned long long id,bool independent){
    auto v=find(id);if(!v)return false;return moveToGroup(id,independent?0:v->lastGroupId);
}
bool Session::moveToGroup(unsigned long long id,unsigned group){
    auto keep=shared_from_this();if(stopped||executing||refreshing)return false;
    auto v=find(id);if(!v||!valid(v->hwnd))return false;
    if(v->groupId==group)return true;
    if(group>=nextGroupId)return false;
    restoreWindowMasks();
    std::shared_ptr<View> destination;
    for(auto& candidate:views)if(candidate->groupId==group&&group&&candidate->id!=id){destination=candidate;if(candidate->id==activeTabId(candidate->hwnd))break;}
    try{
        Flag busy(executing);
        restoreView(*v);if(v->groupId)v->lastGroupId=v->groupId;
        v->groupId=group;v->independent=group==0;
        if(group){groupActiveId=id;groupActives[group]=id;v->lastGroupId=group;}
        else if(groupActiveId==id)groupActiveId=0;
        if(destination&&valid(destination->hwnd)&&IsWindowEnabled(destination->hwnd)){
            WINDOWPLACEMENT desired{sizeof(desired)};RECT bounds{};
            if(windowlayout::capture(destination->hwnd,desired,bounds)&&!windowlayout::minimized(desired)&&windowlayout::capture(v->hwnd,v->original,v->originalBounds)){
                if(windowlayout::apply(v->hwnd,desired,bounds)){v->moved=true;windowlayout::capture(v->hwnd,v->applied,v->appliedBounds);}}
        }
        // Keep the original Office document, including unsaved edits. Only
        // membership, the reserved strip and grouping behavior are changed.
        persistView(*v);syncPanes();updateTaskbar();redraw();
        call(v->window.get(),L"Activate",DISPATCH_METHOD);
    }catch(const ComError& e){report(e);}
    refresh();auto actual=find(id);return actual&&actual->groupId==group;
}
bool Session::refresh(){
    if(stopped||refreshing||executing||coordinatingWindows||windowFrameBusy()||!app)return false;
    Flag busy(refreshing);
    try{
        // Enumeration is transactional: a rejected/partial Office call must not remove live tabs.
        if(host==Host::Excel&&!prop(app.get(),L"Ready").boolean(true))return false;
        pollPersistence();
        // A hidden workbook or another editing view is not a closed document.
        std::set<std::wstring> openPaths;
        auto documents=call(app.get(),host==Host::Word?L"Documents":host==Host::Excel?L"Workbooks":L"Presentations").disp();
        long documentCount=call(documents.get(),L"Count").number();
        for(long i=1;i<=documentCount;i++){auto document=item(documents.get(),i);if(!call(document.get(),L"Path").str().empty())openPaths.insert(storage::normalize(call(document.get(),L"FullName").str()));}
        auto handle=[&](IDispatch* window)->HWND{
            if(host!=Host::PowerPoint)return (HWND)(UINT_PTR)(DWORD)prop(window,L"Hwnd").number();
            auto identity=Ptr<IDispatch>(window).as<IUnknown>();
            for(auto& old:views)if(old->identity.get()==identity.get()&&valid(old->hwnd))return old->hwnd;
            return nullptr;
        };
        std::vector<std::shared_ptr<View>> discovered;
        auto append=[&](Ptr<IDispatch> win,Ptr<IDispatch> doc){
            HWND h=handle(win.get());
            if(!valid(h)&&host!=Host::PowerPoint)return;
            auto ident=win.as<IUnknown>();if(!ident)return;
            std::shared_ptr<View> v;
            // Excel may expose a fresh Window COM wrapper on every enumeration.
            // The process-local top-level HWND identifies a live Office view.
            for(auto& old:views)if((h&&old->hwnd==h)||(host==Host::PowerPoint&&old->identity.get()==ident.get())){v=std::make_shared<View>(*old);break;}
            bool isNew=!v;std::wstring oldPath=v?v->path:L"";
            if(!v){v=std::make_shared<View>();v->id=nextId++;v->hwnd=h;v->identity=ident;GetWindowPlacement(h,&v->original);
                if(pendingNewDocument&&GetTickCount64()-pendingNewSince>5000)pendingNewDocument=false;
                v->groupId=pendingNewDocument?newDocumentGroup:currentGroup();v->independent=v->groupId==0;v->lastGroupId=v->groupId?v->groupId:1;pendingNewDocument=false;}
            v->window=win;v->document=doc;v->title=call(doc.get(),L"Name").str();
            std::wstring caption=prop(win.get(),L"Caption").str();
            if(caption.find(L":")!=std::wstring::npos)v->title=caption;
            v->saved=call(doc.get(),L"Saved").boolean(true);v->readOnly=prop(doc.get(),L"ReadOnly").boolean();
            v->path=prop(doc.get(),L"Path").str().empty()?L"":prop(doc.get(),L"FullName").str();
            initializePreference(*v,isNew,oldPath);
            if(std::none_of(discovered.begin(),discovered.end(),[&](auto& x){return (h&&x->hwnd==h)||x->identity.get()==ident.get();}))discovered.push_back(v);
        };
        if(host==Host::Excel){
            auto books=call(app.get(),L"Workbooks").disp();long n=call(books.get(),L"Count").number();
            for(long i=1;i<=n;i++){auto doc=item(books.get(),i);auto windows=call(doc.get(),L"Windows").disp();long k=call(windows.get(),L"Count").number();for(long j=1;j<=k;j++){auto win=item(windows.get(),j);if(call(win.get(),L"Visible").boolean())append(win,doc);}}
        }else{
            auto windows=call(app.get(),L"Windows").disp();long n=call(windows.get(),L"Count").number();
            for(long i=1;i<=n;i++){auto win=item(windows.get(),i);auto doc=call(win.get(),host==Host::Word?L"Document":L"Presentation").disp();if(doc)append(win,doc);}
        }
        // Preserve the displayed order, independent of Office's changing collection indexes.
        std::vector<std::shared_ptr<View>> ordered;
        for(auto& v:views){auto it=std::find_if(discovered.begin(),discovered.end(),[&](auto& x){return v->id==x->id;});if(it!=discovered.end())ordered.push_back(*it);else{if(!v->path.empty()&&!openPaths.count(storage::normalize(v->path)))rememberClosed(v->path);restoreView(*v);}}
        for(auto& v:discovered)if(std::none_of(ordered.begin(),ordered.end(),[&](auto& x){return x->id==v->id;}))ordered.push_back(v);
        bool visualChanged=views.size()!=ordered.size();
        for(size_t i=0;!visualChanged&&i<views.size();i++){
            auto& a=views[i];auto& b=ordered[i];
            visualChanged=a->id!=b->id||a->title!=b->title||a->saved!=b->saved||a->readOnly!=b->readOnly||a->locked!=b->locked||a->independent!=b->independent||a->groupId!=b->groupId;
        }
        views=std::move(ordered);
        auto active=prop(app.get(),L"ActiveWindow").disp();HWND activeWindow=handle(active.get());unsigned long long found=0;
        auto activeIdentity=active.as<IUnknown>();
        for(auto& v:views)if((activeWindow&&v->hwnd==activeWindow)||(host==Host::PowerPoint&&v->identity.get()==activeIdentity.get()))found=v->id;
        auto candidate=find(found);
        if(settings.group&&candidate&&!candidate->independent){
            auto members=tabViews(candidate->hwnd);bool allMinimized=!members.empty()&&std::all_of(members.begin(),members.end(),[](auto& v){return IsIconic(v->hwnd)!=FALSE;});
            auto previous=find(activeTabId(candidate->hwnd));if(allMinimized&&previous)found=previous->id;
        }
        visualChanged=visualChanged||activeId!=found;activeId=found;
        auto activeView=find(found);if(activeView&&!activeView->independent){groupActiveId=found;groupActives[activeView->groupId]=found;}
        syncPanes();syncWindowObservers();updateTaskbar();if(visualChanged)redraw();failures=0;return true;
    }catch(const ComError& e){lastError=e.hr;lastOperation=L"refresh.rejected";if(++failures==1||failures%80==0)log((L"refresh.rejected."+e.where).c_str(),e.hr);return false;}
    catch(...){log(L"refresh.exception",E_FAIL);return false;}
}
void Session::syncPanes(){
    if(!factory||stopped)return;
    for(auto it=panes.begin();it!=panes.end();){if(!find(it->viewId)){if(it->control)it->control->Bind(nullptr,nullptr);it->chrome.reset();if(it->control)it->control->Unmount();try{call(it->office.get(),L"Delete",DISPATCH_METHOD);}catch(...){}it=panes.erase(it);}else ++it;}
    for(auto& v:views){
        auto it=std::find_if(panes.begin(),panes.end(),[&](auto& p){return p.viewId==v->id;});
        if(it==panes.end()){
            try{
                Pane p;p.viewId=v->id;
                p.office=call(factory.get(),L"CreateCTP",DISPATCH_METHOD,{Var(std::wstring(L"openOfficetab.Tabs")),Var(std::wstring(L"  ")),Var(v->window.get())}).disp();
                if(!p.office)throw ComError(E_NOINTERFACE,L"CreateCTP");
                auto content=call(p.office.get(),L"ContentControl").disp();p.control=content.as<ITabBinding>();
                if(!p.control)throw ComError(E_NOINTERFACE,L"TabBinding");
                p.control->Bind(this,v->hwnd);put(p.office.get(),L"DockPosition",Var((long)settings.position));
                put(p.office.get(),L"DockPositionRestrict",Var(1L));
                p.requestedHeight=MulDiv(32,dpi(v->hwnd),96);put(p.office.get(),L"Height",Var(p.requestedHeight));
                panes.push_back(std::move(p));log(L"pane.created");it=panes.end()-1;
            }catch(const ComError& e){log(L"pane.create.failed",e.hr);continue;}
        }
        try{
            bool show=showTabs(v->hwnd);
            if(prop(it->office.get(),L"Visible").boolean()!=show)put(it->office.get(),L"Visible",Var(show));
            // PowerPoint does not expose DocumentWindow.Hwnd. CreateCTP binds
            // this exact COM window; its own OLE ancestry gives an unambiguous
            // root without caption matching or touching another add-in's pane.
            if(host==Host::PowerPoint&&!valid(v->hwnd)){
                HWND root=GetAncestor(it->control->OleWindow(),GA_ROOT);
                if(valid(root)){v->hwnd=root;GetWindowPlacement(root,&v->original);it->control->Bind(this,root);}
            }
            // Height includes Office's title chrome, and is in pixels. Measure the actual
            // ActiveX content after showing instead of assuming a fixed chrome height.
            HWND control=it->control->NativeWindow();
            if(it->chrome&&!it->chrome->mounted())it->chrome.reset();
            if(show&&control&&!it->chrome){auto chrome=std::make_shared<PaneChrome>();if(chrome->attach(it->control.get()))it->chrome=chrome;}
            // OLE activation and Office's parent hierarchy arrive asynchronously.
            // Retry briefly during initialization; do not wait for the idle timer.
            if(show&&!it->chrome&&it->initAttempts<50){it->initAttempts++;SetTimer(timer,2,20,nullptr);}
            else if(it->chrome)it->initAttempts=0;
            if(show&&control){
                long desired=MulDiv(32,dpi(v->hwnd),96);
                // Respect an Office minimum instead of repeatedly fighting it.
                if(it->requestedHeight!=desired){put(it->office.get(),L"Height",Var(desired));it->requestedHeight=desired;}
            }
            if(it->chrome)it->chrome->layout();
            HWND display=it->control->NativeWindow();if(display&&((GetWindowLongPtrW(display,GWL_STYLE)&WS_VISIBLE)!=0)!=show)ShowWindow(display,show?SW_SHOWNOACTIVATE:SW_HIDE);
        }catch(...){}
    }
}
void Session::redraw(){for(auto& p:panes)if(p.control){HWND h=p.control->NativeWindow();if(h)InvalidateRect(h,nullptr,FALSE);}}
void Session::report(const ComError& e){lastError=e.hr;lastOperation=L"command.failed";log(L"command.rejected",e.hr);defer();}
void Session::align(const std::shared_ptr<View>& target,const std::shared_ptr<View>& source){
    if(!target||!source||target->independent||source->independent||target->groupId!=source->groupId||source->hwnd==target->hwnd||!valid(source->hwnd)||!valid(target->hwnd))return;
    if(!IsWindowEnabled(source->hwnd)||!IsWindowEnabled(target->hwnd))return;
    WINDOWPLACEMENT p{sizeof(p)};RECT bounds{};if(!windowlayout::capture(source->hwnd,p,bounds)||windowlayout::minimized(p)||windowlayout::sameSlot(target->hwnd,p,bounds))return;
    if(!target->moved&&!windowlayout::capture(target->hwnd,target->original,target->originalBounds))return;
    if(windowlayout::apply(target->hwnd,p,bounds)){target->moved=true;windowlayout::capture(target->hwnd,target->applied,target->appliedBounds);}
}
void Session::activate(unsigned long long id,HWND owner){
    auto keep=shared_from_this();if(stopped||executing)return;auto target=find(id);if(!target)return;
    // Use the clicked bar's root, not the globally last active document: Windows
    // can expose another Office window while the previous pane is still visible.
    if(owner)owner=GetAncestor(owner,GA_ROOT);else owner=GetForegroundWindow();
    auto source=find(activeId);for(auto& v:views)if(v->hwnd==owner){source=v;break;}
    restoreWindowMasks();
    try{Flag busy(executing);align(target,source);call(target->window.get(),L"Activate",DISPATCH_METHOD);align(target,source);
        // PowerPoint can change ActiveWindow without raising its SDI root.
        // Never raise a disabled root behind an Office modal dialog.
        if(valid(target->hwnd)&&IsWindowEnabled(target->hwnd)){
            // In PowerPoint, DocumentWindow.Activate alone can leave its SDI
            // frame inactive when all frames already share identical bounds.
            if(GetWindowThreadProcessId(target->hwnd,nullptr)==GetCurrentThreadId())SetActiveWindow(target->hwnd);
            if(GetForegroundWindow()!=target->hwnd)SetForegroundWindow(target->hwnd);
        }
        // A deliberate tab selection supersedes an older, deferred restore
        // command from another frame in the same group.
        if(queuedLayoutRoot&&groupForWindow(queuedLayoutRoot)==target->groupId){queuedLayoutRoot=nullptr;queuedLayoutCommand=false;if(timer)KillTimer(timer,4);}
    }catch(const ComError& e){report(e);}
    refresh();auto current=find(id);if(current)rootLayoutChanged(current->hwnd);
}
bool Session::close(unsigned long long id,bool fromFrame){
    auto keep=shared_from_this();if(stopped||executing||pendingCloseId)return false;auto v=find(id);if(!v||(v->locked&&!fromFrame))return false;
    if(host==Host::PowerPoint){
        if(!valid(v->hwnd)||!timer)return false;
        pendingCloseId=id;pendingCloseAccepted=false;pendingCloseUntil=0;
        if(!PostMessageW(timer,NATIVE_CLOSE,0,0)){pendingCloseId=0;return false;}
        return true; // Accepted request; completion/cancellation occurs in the Office UI loop.
    }
    try{
        Flag busy(executing);
        if(host==Host::Word)call(v->window.get(),L"Close",DISPATCH_METHOD,{Var(-2L)}); // wdPromptToSaveChanges
        else if(host==Host::Excel)call(v->window.get(),L"Close",DISPATCH_METHOD);
    }catch(const ComError& e){pendingCloseId=0;pendingCloseAccepted=false;report(e);return false;}
    if(stopped)return true;
    if(!refresh()){defer();return pendingCloseAccepted;}
    if(find(id)){
        if(pendingCloseAccepted){defer();return true;}
        pendingCloseId=0;return false;
    }
    pendingCloseId=0;pendingCloseAccepted=false;quitIfEmpty();return true;
}
void Session::nativeClose(){
    if(stopped||!pendingCloseId)return;
    if(executing||refreshing){SetTimer(timer,3,20,nullptr);return;}
    KillTimer(timer,3);auto target=find(pendingCloseId);
    if(!target||!valid(target->hwnd)){refresh();finishClose();return;}
    try{
        Flag busy(executing);
        call(target->window.get(),L"Activate",DISPATCH_METHOD);
        SendMessageW(target->hwnd,WM_CLOSE,0,0);
    }catch(const ComError& e){report(e);pendingCloseId=0;pendingBatch.clear();frameBatch=false;return;}
    if(stopped)return;
    if(!refresh()){defer();return;}
    // PowerPoint may post the final close even from its UI loop. Keep the
    // cancellation observer alive until that native operation settles.
    pendingCloseUntil=GetTickCount64()+1000;
    finishClose();defer();
}
void Session::finishClose(){
    if(stopped||executing||refreshing||!pendingCloseId)return;
    if(find(pendingCloseId)){
        if(!pendingCloseAccepted&&pendingCloseUntil&&GetTickCount64()>=pendingCloseUntil){pendingCloseId=0;pendingBatch.clear();frameBatch=false;}
        return;
    }
    pendingCloseId=0;pendingCloseAccepted=false;
    bool fromFrame=frameBatch;frameBatch=false;auto remaining=std::move(pendingBatch);pendingBatch.clear();
    if(views.empty())quitIfEmpty();
    if(!stopped&&!remaining.empty())batchClose(std::move(remaining),fromFrame);
}
void Session::quitIfEmpty(){
    if(stopped||executing||!app||!views.empty())return;
    try{
        auto docs=call(app.get(),host==Host::Word?L"Documents":host==Host::Excel?L"Workbooks":L"Presentations").disp();
        if(call(docs.get(),L"Count").number()!=0)return;
        auto protectedViews=call(app.get(),L"ProtectedViewWindows").disp();
        if(call(protectedViews.get(),L"Count").number()!=0)return;
        auto application=app;Flag busy(executing);lastOperation=L"close.last.quit";lastError=S_OK;
        if(host==Host::Word)call(application.get(),L"Quit",DISPATCH_METHOD,{Var(-2L)});
        else call(application.get(),L"Quit",DISPATCH_METHOD);
    }catch(const ComError& e){report(e);}
}
bool Session::save(unsigned long long id,bool saveAs){
    auto keep=shared_from_this();if(stopped||executing)return false;auto v=find(id);if(!v||v->readOnly)return false;
    try{
        Flag busy(executing);call(v->window.get(),L"Activate",DISPATCH_METHOD);
        if(saveAs||v->path.empty()){
            auto bars=call(app.get(),L"CommandBars").disp();call(bars.get(),L"ExecuteMso",DISPATCH_METHOD,{Var(std::wstring(L"FileSaveAs"))});
        }else call(v->document.get(),L"Save",DISPATCH_METHOD);
    }catch(const ComError& e){report(e);return false;}
    if(!refresh())return false;auto actual=find(id);return actual&&actual->saved;
}
void Session::create(bool independent,HWND owner){
    auto keep=shared_from_this();if(stopped||executing)return;
    try{
        Flag busy(executing),newCommand(creating);
        pendingNewDocument=true;pendingNewSince=GetTickCount64();newDocumentGroup=independent?0:(owner?groupForWindow(owner):currentGroup());
        auto bars=call(app.get(),L"CommandBars").disp();
        call(bars.get(),L"ExecuteMso",DISPATCH_METHOD,{Var(std::wstring(L"FileNewDefault"))});
    }catch(const ComError& e){pendingNewDocument=false;report(e);}
    refresh();
}
void Session::open(){auto keep=shared_from_this();if(stopped||executing)return;try{Flag busy(executing);auto b=call(app.get(),L"CommandBars").disp();call(b.get(),L"ExecuteMso",DISPATCH_METHOD,{Var(std::wstring(L"FileOpen"))});}catch(const ComError& e){report(e);}refresh();}
void Session::batchClose(std::vector<unsigned long long> ids,bool fromFrame){
    auto keep=shared_from_this();if(pendingCloseId)return;frameBatch=fromFrame;
    for(size_t i=0;i<ids.size();i++){auto id=ids[i];auto v=find(id);if(!v||(v->locked&&!fromFrame)||v->independent)continue;
        if(!close(id,fromFrame)){frameBatch=false;return;}
        if(pendingCloseId){pendingBatch.assign(ids.begin()+i+1,ids.end());return;}
    }frameBatch=false;
}
void Session::saveAll(HWND owner){auto keep=shared_from_this();auto active=find(activeId);if(!owner&&active)owner=active->hwnd;std::vector<unsigned long long> ids;std::vector<IUnknown*> docs;for(auto& v:tabViews(owner)){auto i=v->document.as<IUnknown>();if(std::find(docs.begin(),docs.end(),i.get())==docs.end()){docs.push_back(i.get());if(!v->saved)ids.push_back(v->id);}}for(auto id:ids)if(!save(id))break;}
void Session::reorder(unsigned long long from,unsigned long long to){auto a=std::find_if(views.begin(),views.end(),[&](auto& v){return v->id==from;});auto b=std::find_if(views.begin(),views.end(),[&](auto& v){return v->id==to;});if(a==views.end()||b==views.end())return;auto v=*a;size_t index=(size_t)(b-views.begin());views.erase(a);views.insert(views.begin()+index,v);redraw();}
void Session::restoreView(View& v){
    if(valid(v.hwnd)){
        if(v.taskbarRemoved&&taskbar)taskbar->AddTab(v.hwnd);
        if(v.moved&&windowlayout::matches(v.hwnd,v.applied,v.appliedBounds))windowlayout::apply(v.hwnd,v.original,v.originalBounds);
    }v.taskbarRemoved=false;v.moved=false;
}
void Session::restore(){restoreWindowMasks();bool previous=coordinatingWindows;coordinatingWindows=true;groupSlots.clear();for(auto& v:views)restoreView(*v);coordinatingWindows=previous;log(L"windows.restored");}
void Session::updateTaskbar(){
    if(!taskbar)return;
    // Office can register a window again after activation, or after Explorer
    // recreates the taskbar. taskbarRemoved records our responsibility to
    // restore it, not proof of the shell's current state. Reconcile at idle.
    // Establish each group's recovery entry before removing any siblings.
    std::set<unsigned> ready;
    for(auto& v:views){
        if(!valid(v->hwnd))continue;
        bool grouped=settings.group&&showTabs(v->hwnd)&&!v->independent;
        if(grouped&&v->id==activeTabId(v->hwnd)){
            if(SUCCEEDED(taskbar->AddTab(v->hwnd))){v->taskbarRemoved=false;ready.insert(v->groupId);}
        }else if(!grouped&&v->taskbarRemoved&&SUCCEEDED(taskbar->AddTab(v->hwnd)))v->taskbarRemoved=false;
    }
    for(auto& v:views)if(valid(v->hwnd)&&ready.count(v->groupId)&&!v->independent&&v->id!=activeTabId(v->hwnd)){
        if(SUCCEEDED(taskbar->DeleteTab(v->hwnd)))v->taskbarRemoved=true;
    }
}
void Session::toggleGroup(){settings.synchronize();settings.group=!settings.group;if(!settings.group)restore();changed();}
void Session::toggleVisible(){settings.visible=!settings.visible;if(!settings.visible)restore();changed();}
void Session::changed(){settings.save();for(auto& p:panes)try{put(p.office.get(),L"DockPosition",Var((long)settings.position));}catch(...){}syncPanes();updateTaskbar();redraw();}
bool Session::recover(){
    auto keep=shared_from_this();if(stopped||executing||refreshing)return false;
    settings.visible=true;settings.autoHide=false;restore();
    // Rebuild only our own panes, retaining documents, membership and order.
    for(auto& p:panes){if(p.control)p.control->Bind(nullptr,nullptr);p.chrome.reset();if(p.control)p.control->Unmount();try{call(p.office.get(),L"Delete",DISPATCH_METHOD);}catch(const ComError& e){report(e);}}
    panes.clear();settings.save();bool ok=refresh();lastOperation=ok?L"recovery.completed":L"recovery.deferred";if(ok)lastError=S_OK;defer();return ok;
}
std::wstring Session::state()const{
    std::wstring r=L"{\"version\":\"0.1.7\",\"host\":"+json(hostName(host))+L",\"style\":"+json(settings.barStyle==1?L"open":L"browser")+L",\"activeId\":"+std::to_wstring(activeId)+L",\"activeTabId\":"+std::to_wstring(activeTabId())+L",\"tabCount\":"+std::to_wstring(tabViews().size())+L",\"visible\":"+(settings.visible?L"true":L"false")+L",\"group\":"+(settings.group?L"true":L"false")+L",\"panes\":"+std::to_wstring(panes.size())+L",\"views\":[";
    for(size_t i=0;i<views.size();i++){auto& v=views[i];if(i)r+=L",";r+=L"{\"id\":"+std::to_wstring(v->id)+L",\"hwnd\":"+std::to_wstring((UINT_PTR)v->hwnd)+L",\"title\":"+json(v->title)+L",\"groupId\":"+std::to_wstring(v->groupId)+L",\"saved\":"+(v->saved?L"true":L"false")+L",\"readOnly\":"+(v->readOnly?L"true":L"false")+L",\"locked\":"+(v->locked?L"true":L"false")+L",\"independent\":"+(v->independent?L"true":L"false")+L",\"taskbarRemoved\":"+(v->taskbarRemoved?L"true":L"false")+L",\"manualTabWidth\":"+std::to_wstring(v->manualTabWidth)+L",\"moved\":"+(v->moved?L"true":L"false")+L"}";}
    r+=L"],\"paneMetrics\":[";
    for(size_t i=0;i<panes.size();i++){
        if(i)r+=L",";auto& p=panes[i];HWND h=p.control?p.control->NativeWindow():nullptr;RECT rc{};if(h)GetWindowRect(h,&rc);
        r+=L"{\"viewId\":"+std::to_wstring(p.viewId)+L",\"controlHwnd\":"+std::to_wstring((UINT_PTR)h)+L",\"width\":"+std::to_wstring(rc.right-rc.left)+L",\"height\":"+std::to_wstring(rc.bottom-rc.top)+L",\"dpi\":"+std::to_wstring(dpi(h))+L",\"officeHeight\":"+std::to_wstring(prop(p.office.get(),L"Height").number())+L",\"ancestors\":[";
        for(int j=0;h&&j<10;j++){h=GetParent(h);if(!h)break;wchar_t cls[128];GetClassNameW(h,cls,128);GetWindowRect(h,&rc);RECT client{};GetClientRect(h,&client);if(j)r+=L",";r+=L"{\"class\":"+json(cls)+L",\"height\":"+std::to_wstring(rc.bottom-rc.top)+L",\"clientHeight\":"+std::to_wstring(client.bottom-client.top)+L",\"hwnd\":"+std::to_wstring((UINT_PTR)h)+L"}";}
        HWND frame=p.chrome?p.chrome->window():nullptr;RECT reserved{},display{};if(frame)GetWindowRect(frame,&reserved);HWND native=p.control?p.control->NativeWindow():nullptr;if(native)GetWindowRect(native,&display);
        r+=L"],\"displayLeft\":"+std::to_wstring(display.left)+L",\"displayTop\":"+std::to_wstring(display.top)+L",\"reserveFrameHwnd\":"+std::to_wstring((UINT_PTR)frame)+L",\"reserveFrameVisible\":"+(IsWindowVisible(frame)?L"true":L"false")+L",\"displayVisible\":"+(native&&(GetWindowLongPtrW(native,GWL_STYLE)&WS_VISIBLE)?L"true":L"false")+L",\"officeVisible\":"+(prop(p.office.get(),L"Visible").boolean()?L"true":L"false")+L",\"seamVisible\":"+(p.control&&IsWindowVisible(p.control->SeamWindow())?L"true":L"false")+L",\"seamHwnd\":"+std::to_wstring((UINT_PTR)(p.control?p.control->SeamWindow():nullptr))+L",\"matchesReservedRect\":"+(frame&&native&&EqualRect(&reserved,&display)?L"true":L"false")+L"}";
    }
    return r+L"],\"diagnostics\":"+diagnostics()+L"}";
}
void Session::menu(HWND owner,unsigned long long id,POINT pt){
    auto keep=shared_from_this();auto v=find(id);if(!v)return;
    HMENU m=CreatePopupMenu();auto add=[&](UINT n,const wchar_t* t,bool enabled=true,bool checked=false){AppendMenuW(m,MF_STRING|(enabled?0:MF_GRAYED)|(checked?MF_CHECKED:0),n,tr(t));};
    add(1,L"切换到此文档");add(12,L"在独立窗口中显示");add(13,L"移到新标签组窗口");add(14,L"移动到标签组…");add(15,L"新建为独立窗口");AppendMenuW(m,MF_SEPARATOR,0,nullptr);add(2,L"保存",!v->readOnly);add(3,L"另存为…",!v->readOnly);add(4,L"保存本组全部标签");AppendMenuW(m,MF_SEPARATOR,0,nullptr);
    add(5,L"关闭",!v->locked);add(6,L"关闭其他标签");add(7,L"关闭左侧标签");add(8,L"关闭右侧标签");add(9,L"关闭全部标签");AppendMenuW(m,MF_SEPARATOR,0,nullptr);
    add(10,L"锁定标签（防止从标签栏关闭）",true,v->locked);add(11,L"打开所在文件夹",!v->path.empty()&&v->path.find(L"://")==std::wstring::npos);
    AppendMenuW(m,MF_SEPARATOR,0,nullptr);add(17,L"最近关闭…");add(18,L"重置此文件的标签设置",storage::localFile(v->path));
    UINT command=TrackPopupMenu(m,TPM_RETURNCMD|TPM_NONOTIFY,pt.x,pt.y,0,owner,nullptr);DestroyMenu(m);
    if(command==1)activate(id);if(command==2)save(id);if(command==3)save(id,true);if(command==4)saveAll(v->hwnd);if(command==5)close(id);if(command==12)setIndependent(id,true);
    if(command>=6&&command<=9){std::vector<unsigned long long> ids;bool after=false;for(auto& x:tabViews(v->hwnd)){if(x->id==id){after=true;if(command==9)ids.push_back(x->id);continue;}if(command==6||command==9||(command==7&&!after)||(command==8&&after))ids.push_back(x->id);}batchClose(ids);}
    if(command==10){v->locked=!v->locked;redraw();}
    if(command==11){std::wstring parent=v->path.substr(0,v->path.find_last_of(L"\\/"));ShellExecuteW(owner,L"open",parent.c_str(),nullptr,nullptr,SW_SHOWNORMAL);}
    if(command==13)newTabGroup(id);if(command==14)chooseGroup(owner,id);if(command==15)create(true,owner);
    if(command==17)recentMenu(owner,pt);if(command==18)resetFilePreferences(id);
}
void Session::options(HWND owner,POINT pt){
    auto keep=shared_from_this();HMENU m=CreatePopupMenu();auto add=[&](UINT n,const wchar_t* t,bool check=false){AppendMenuW(m,MF_STRING|(check?MF_CHECKED:0),n,tr(t));};
    add(1,L"显示标签栏",settings.visible);add(2,L"单任务栏模式",settings.group);add(4,L"单标签时自动隐藏",settings.autoHide);add(5,L"双击标签关闭",settings.doubleClose);AppendMenuW(m,MF_SEPARATOR,0,nullptr);
    add(6,L"标签置于顶部",settings.position==1);add(7,L"标签置于底部",settings.position==3);add(8,L"恢复窗口排列与任务栏");add(12,L"默认标签宽度…");add(17,L"最近关闭…");add(18,L"清除本应用已记住的文件设置");AppendMenuW(m,MF_SEPARATOR,0,nullptr);add(13,L"加载诊断…");add(14,L"导出脱敏诊断…");add(15,L"恢复标签栏与窗口");add(16,L"新建为独立窗口");add(9,L"打开文档…");add(10,L"打开日志与设置文件夹");add(11,L"关于 openOfficetab");
    HMENU appearance=CreatePopupMenu();
    AppendMenuW(appearance,MF_STRING|(settings.barStyle==1?MF_CHECKED:0),23,tr(L"融入应用背景（浅灰开口标签）"));
    AppendMenuW(appearance,MF_STRING|(settings.barStyle==0?MF_CHECKED:0),24,tr(L"独立标签栏（白色圆角面板）"));
    AppendMenuW(appearance,MF_SEPARATOR,0,nullptr);
    AppendMenuW(appearance,MF_STRING|(settings.accentPosition==1?MF_CHECKED:0),21,tr(L"强调线：顶部"));
    AppendMenuW(appearance,MF_STRING|(settings.accentPosition==0?MF_CHECKED:0),20,tr(L"强调线：底部"));
    AppendMenuW(appearance,MF_STRING|(settings.accentPosition==2?MF_CHECKED:0),22,tr(L"强调线：无"));
    AppendMenuW(appearance,MF_SEPARATOR,0,nullptr);AppendMenuW(appearance,MF_STRING,25,tr(L"标签栏颜色…"));AppendMenuW(appearance,MF_STRING,26,tr(L"恢复默认颜色"));
    HMENU theme=CreatePopupMenu();
    const wchar_t* themes[]={L"跟随 Office / 系统（默认）",L"始终浅色",L"始终深色",L"仅跟随系统"};
    for(int i=0;i<4;i++)AppendMenuW(theme,MF_STRING|(settings.themeMode==i?MF_CHECKED:0),30+i,tr(themes[i]));
    AppendMenuW(appearance,MF_SEPARATOR,0,nullptr);AppendMenuW(appearance,MF_POPUP,(UINT_PTR)theme,tr(L"明暗主题"));
    HMENU closing=CreatePopupMenu();
    const wchar_t* actions[]={L"每次询问",L"只关闭当前文档",L"关闭本组全部文档"};
    for(int i=0;i<3;i++)AppendMenuW(closing,MF_STRING|(settings.frameCloseAction==i?MF_CHECKED:0),40+i,tr(actions[i]));
    InsertMenuW(m,3,MF_BYPOSITION|MF_POPUP,(UINT_PTR)closing,tr(L"点击窗口关闭按钮时"));
    InsertMenuW(m,4,MF_BYPOSITION|MF_POPUP,(UINT_PTR)appearance,tr(L"标签外观"));
    HMENU languages=CreatePopupMenu();
    const wchar_t* labels[]={tr(L"跟随系统（默认）"),L"简体中文",L"English"};
    for(int i=0;i<3;i++)AppendMenuW(languages,MF_STRING|(settings.language==i?MF_CHECKED:0),50+i,labels[i]);
    InsertMenuW(m,5,MF_BYPOSITION|MF_POPUP,(UINT_PTR)languages,tr(L"语言 / Language"));
    UINT cmd=TrackPopupMenu(m,TPM_RETURNCMD|TPM_NONOTIFY,pt.x,pt.y,0,owner,nullptr);DestroyMenu(m);
    if(cmd==1)toggleVisible();if(cmd==2)toggleGroup();if(cmd==4){settings.autoHide=!settings.autoHide;changed();}if(cmd==5){settings.doubleClose=!settings.doubleClose;changed();}
    if(cmd==6||cmd==7){settings.position=cmd==6?1:3;changed();}if(cmd==8){restore();changed();}if(cmd==9)open();if(cmd==10)ShellExecuteW(owner,L"open",dataDir().c_str(),nullptr,nullptr,SW_SHOWNORMAL);
    if(cmd==12)widthDialog(GetAncestor(owner,GA_ROOT));if(cmd==13)showDiagnostics(GetAncestor(owner,GA_ROOT));if(cmd==14)exportDiagnostics(GetAncestor(owner,GA_ROOT));if(cmd==15)recover();if(cmd==16)create(true,owner);
    if(cmd>=20&&cmd<=22)setAppearance((int)cmd-20,settings.barStyle);
    if(cmd==23||cmd==24)setAppearance(settings.accentPosition,cmd==23?1:0);
    if(cmd==25)colorDialog(GetAncestor(owner,GA_ROOT));if(cmd==26)setBarColor(L"");
    if(cmd>=30&&cmd<=33)setThemeMode((int)cmd-30);if(cmd>=40&&cmd<=42)setFrameCloseAction((int)cmd-40);
    if(cmd>=50&&cmd<=52)setLanguage((int)cmd-50);
    if(cmd==17)recentMenu(owner,pt);if(cmd==18)resetAllFilePreferences();
    if(cmd==11)MessageBoxW(owner,tr(L"openOfficetab 0.1.7\n永久免费 · MIT 开源\nWord / Excel / PowerPoint\n\n浏览器圆角标签 · 原生 Office 文档与保存流程\n拖动排序，中键关闭，右键更多操作。\n\n标签锁定仅防止标签栏误关，不替代 Office 文档保护。"),tr(L"关于 openOfficetab"),MB_OK|MB_ICONINFORMATION);
}
