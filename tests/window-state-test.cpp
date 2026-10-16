#include "../src/tabs.h"
#include "../src/window-layout.h"
#include <cassert>
#include <iostream>

// An OLE host records UI takeover calls. No Office or visible desktop UI.
class Site final:public IOleClientSite,public IOleInPlaceSite {
    ULONG refs=1;
public:
    HWND parent=nullptr;unsigned uiActivations=0,placeActivations=0;
    HRESULT STDMETHODCALLTYPE QueryInterface(REFIID id,void** out)override{if(!out)return E_POINTER;*out=nullptr;if(id==IID_IUnknown||id==IID_IOleClientSite)*out=static_cast<IOleClientSite*>(this);else if(id==IID_IOleInPlaceSite||id==IID_IOleWindow)*out=static_cast<IOleInPlaceSite*>(this);if(!*out)return E_NOINTERFACE;AddRef();return S_OK;}
    ULONG STDMETHODCALLTYPE AddRef()override{return ++refs;}
    ULONG STDMETHODCALLTYPE Release()override{auto n=--refs;if(!n)delete this;return n;}
    HRESULT STDMETHODCALLTYPE SaveObject()override{return S_OK;}
    HRESULT STDMETHODCALLTYPE GetMoniker(DWORD,DWORD,IMoniker**)override{return E_NOTIMPL;}
    HRESULT STDMETHODCALLTYPE GetContainer(IOleContainer** p)override{if(p)*p=nullptr;return E_NOINTERFACE;}
    HRESULT STDMETHODCALLTYPE ShowObject()override{return S_OK;}
    HRESULT STDMETHODCALLTYPE OnShowWindow(BOOL)override{return S_OK;}
    HRESULT STDMETHODCALLTYPE RequestNewObjectLayout()override{return E_NOTIMPL;}
    HRESULT STDMETHODCALLTYPE GetWindow(HWND* p)override{if(!p)return E_POINTER;*p=parent;return S_OK;}
    HRESULT STDMETHODCALLTYPE ContextSensitiveHelp(BOOL)override{return E_NOTIMPL;}
    HRESULT STDMETHODCALLTYPE CanInPlaceActivate()override{return S_OK;}
    HRESULT STDMETHODCALLTYPE OnInPlaceActivate()override{placeActivations++;return S_OK;}
    HRESULT STDMETHODCALLTYPE OnUIActivate()override{uiActivations++;return S_OK;}
    HRESULT STDMETHODCALLTYPE GetWindowContext(IOleInPlaceFrame** f,IOleInPlaceUIWindow** d,LPRECT pos,LPRECT clip,LPOLEINPLACEFRAMEINFO info)override{*f=nullptr;*d=nullptr;*pos=*clip=RECT{0,0,600,40};info->hwndFrame=parent;return S_OK;}
    HRESULT STDMETHODCALLTYPE Scroll(SIZE)override{return E_NOTIMPL;}
    HRESULT STDMETHODCALLTYPE OnUIDeactivate(BOOL)override{return S_OK;}
    HRESULT STDMETHODCALLTYPE OnInPlaceDeactivate()override{return S_OK;}
    HRESULT STDMETHODCALLTYPE DiscardUndoState()override{return S_OK;}
    HRESULT STDMETHODCALLTYPE DeactivateAndUndo()override{return E_NOTIMPL;}
    HRESULT STDMETHODCALLTYPE OnPosRectChange(LPCRECT)override{return S_OK;}
};
class Application final:public IDispatch {
    ULONG refs=1;
public:
    HWND activationWindow=nullptr;RECT activationBounds{};
    HRESULT STDMETHODCALLTYPE QueryInterface(REFIID id,void** out)override{if(!out)return E_POINTER;*out=nullptr;if(id!=IID_IUnknown&&id!=IID_IDispatch)return E_NOINTERFACE;*out=this;AddRef();return S_OK;}
    ULONG STDMETHODCALLTYPE AddRef()override{return ++refs;}
    ULONG STDMETHODCALLTYPE Release()override{auto n=--refs;if(!n)delete this;return n;}
    DISPATCH_STUBS
    HRESULT STDMETHODCALLTYPE GetIDsOfNames(REFIID,LPOLESTR* names,UINT,LCID,DISPID* ids)override{if(!_wcsicmp(*names,L"Activate")){*ids=2;return S_OK;}if(_wcsicmp(*names,L"Name"))return DISP_E_UNKNOWNNAME;*ids=1;return S_OK;}
    HRESULT STDMETHODCALLTYPE Invoke(DISPID id,REFIID,LCID,WORD,DISPPARAMS*,VARIANT* result,EXCEPINFO*,UINT*)override{if(id==2){if(activationWindow)SetWindowPos(activationWindow,nullptr,activationBounds.left,activationBounds.top,activationBounds.right-activationBounds.left,activationBounds.bottom-activationBounds.top,SWP_NOACTIVATE|SWP_NOZORDER);return S_OK;}VariantInit(result);result->vt=VT_BSTR;result->bstrVal=SysAllocString(L"Word");return S_OK;}
};
HWND window(const wchar_t* cls,HWND parent,int x,int y,int width,int height){return CreateWindowExW(0,cls,L"",parent?WS_CHILD:WS_OVERLAPPEDWINDOW,x,y,width,height,parent,nullptr,module,nullptr);}
class Taskbar final:public ITaskbarList {
    ULONG refs=1;
public:
    std::set<HWND> entries;std::vector<std::pair<bool,HWND>> operations;HWND failAdd=nullptr;
    HRESULT STDMETHODCALLTYPE QueryInterface(REFIID id,void** out)override{if(!out)return E_POINTER;*out=nullptr;if(id!=IID_IUnknown&&id!=IID_ITaskbarList)return E_NOINTERFACE;*out=this;AddRef();return S_OK;}
    ULONG STDMETHODCALLTYPE AddRef()override{return ++refs;}ULONG STDMETHODCALLTYPE Release()override{auto n=--refs;if(!n)delete this;return n;}
    HRESULT STDMETHODCALLTYPE HrInit()override{return S_OK;}
    HRESULT STDMETHODCALLTYPE AddTab(HWND h)override{operations.push_back({true,h});if(h==failAdd)return E_FAIL;entries.insert(h);return S_OK;}
    HRESULT STDMETHODCALLTYPE DeleteTab(HWND h)override{operations.push_back({false,h});entries.erase(h);return S_OK;}
    HRESULT STDMETHODCALLTYPE ActivateTab(HWND)override{return S_OK;}
    HRESULT STDMETHODCALLTYPE SetActiveAlt(HWND)override{return S_OK;}
};
int main(){
    CoInitializeEx(nullptr,COINIT_APARTMENTTHREADED);
    RECT reserved{14,358,2866,438},ribbonBounds{14,0,2866,358},edge{};
    assert(windowlayout::seamBounds(reserved,ribbonBounds,192,edge)&&edge.top==352&&edge.bottom==358);
    assert(!windowlayout::seamBounds(reserved,RECT{14,0,2866,100},192,edge));
    assert(!windowlayout::seamBounds(reserved,RECT{14,0,100,358},192,edge));
    assert(!windowlayout::seamBounds(reserved,RECT{14,0,2866,340},192,edge));
    windowlayout::PhysicalCoordinates context;
    HWND root=window(L"STATIC",nullptr,100,100,900,700),other=window(L"STATIC",nullptr,900,100,500,600);
    assert(root&&other);
    WNDCLASSW cls{};cls.hInstance=module;cls.lpfnWndProc=DefWindowProcW;cls.lpszClassName=L"MsoCommandBar";RegisterClassW(&cls);
    HWND strip=window(cls.lpszClassName,root,0,170,800,40),ribbon=window(cls.lpszClassName,root,0,0,800,MulDiv(180,dpi(root),96));
    HWND ole=window(L"STATIC",strip,0,0,800,30),wrongOle=window(L"STATIC",ribbon,0,0,800,30);
    assert(windowlayout::reserveFrame(strip,ole,root));
    assert(!windowlayout::reserveFrame(ribbon,wrongOle,root));
    assert(!windowlayout::reserveFrame(strip,ole,other));
    assert(!windowlayout::reserveFrame(strip,wrongOle,root));
    Ptr<Site> site(new Site(),false);site->parent=strip;
    Ptr<Tabs> tabs(new Tabs(),false);RECT pos{0,0,800,40};
    for(auto verb:{OLEIVERB_SHOW,OLEIVERB_INPLACEACTIVATE,OLEIVERB_UIACTIVATE,OLEIVERB_PRIMARY}){
        assert(SUCCEEDED(tabs->DoVerb(verb,nullptr,site.get(),0,strip,&pos)));
        assert(site->placeActivations==1&&site->uiActivations==0);
    }
    DWORD status=0;assert(SUCCEEDED(tabs->GetMiscStatus(DVASPECT_CONTENT,&status))&&(status&OLEMISC_NOUIACTIVATE));
    tabs->Bind(nullptr,root);assert(tabs->Mount(strip));assert(!tabs->Mount(ribbon));
    tabs->Unmount();tabs->Close(0);tabs.reset();site.reset();
    Ptr<IDispatch> app(new Application(),false);auto session=std::make_shared<Session>(app.get());session->settings=Settings{};session->settings.group=false;
    auto source=std::make_shared<View>(),target=std::make_shared<View>();source->id=1;source->hwnd=root;target->id=2;target->hwnd=other;
    session->views={source,target};session->activeId=1;
    WINDOWPLACEMENT old{sizeof(old)};RECT oldRect{};assert(windowlayout::capture(other,old,oldRect));
    session->align(target,source);RECT sourceRect{},actual{};GetWindowRect(root,&sourceRect);GetWindowRect(other,&actual);
    assert(EqualRect(&sourceRect,&actual)&&target->moved&&!session->settings.group);
    session->restoreView(*target);GetWindowRect(other,&actual);assert(EqualRect(&oldRect,&actual));
    session->align(target,source);SetWindowPos(other,nullptr,700,200,450,350,SWP_NOACTIVATE|SWP_NOZORDER);GetWindowRect(other,&oldRect);
    session->restoreView(*target);GetWindowRect(other,&actual);assert(EqualRect(&oldRect,&actual)); // keep manual changes
    target->independent=true;session->align(target,source);GetWindowRect(other,&actual);assert(EqualRect(&oldRect,&actual));
    target->independent=false;target->groupId=2;session->align(target,source);GetWindowRect(other,&actual);assert(EqualRect(&oldRect,&actual));
    target->groupId=1;EnableWindow(root,FALSE);session->align(target,source);GetWindowRect(other,&actual);assert(EqualRect(&oldRect,&actual));EnableWindow(root,TRUE);
    for(auto host:{Host::Word,Host::Excel,Host::PowerPoint}){session->host=host;SetWindowPos(other,nullptr,700,200,450,350,SWP_NOACTIVATE|SWP_NOZORDER);session->align(target,source);GetWindowRect(other,&actual);assert(EqualRect(&sourceRect,&actual)&&!session->settings.group);}
    WINDOWPLACEMENT stale{sizeof(stale)};RECT captured{};assert(windowlayout::capture(root,stale,captured));stale.rcNormalPosition={10,10,200,200};
    assert(windowlayout::apply(other,stale,captured));GetWindowRect(other,&actual);assert(EqualRect(&sourceRect,&actual));
    session->host=Host::Word;session->settings.group=false;session->activeId=target->id;
    Ptr<Application> targetWindow(new Application(),false);targetWindow->activationWindow=other;targetWindow->activationBounds={0,0,1400,900};target->window=Ptr<IDispatch>(targetWindow.get());
    session->activate(target->id,root);GetWindowRect(other,&actual);assert(EqualRect(&sourceRect,&actual)); // clicked owner wins, even if host restores target during Activate
    Ptr<Taskbar> taskbar(new Taskbar(),false);session->taskbar=Ptr<ITaskbarList>(taskbar.get());
    session->settings.group=true;session->groupActives[1]=source->id;taskbar->entries={root,other};session->updateTaskbar();
    assert(taskbar->entries==std::set<HWND>{root}&&target->taskbarRemoved);
    assert(taskbar->operations.front()==std::make_pair(true,root)); // add selected before deleting siblings
    taskbar->entries.insert(other);session->updateTaskbar();assert(taskbar->entries==std::set<HWND>{root}); // Office re-registration
    taskbar->entries.clear();session->updateTaskbar();assert(taskbar->entries==std::set<HWND>{root}); // Explorer recreation
    session->groupActives[1]=target->id;session->updateTaskbar();assert(taskbar->entries==std::set<HWND>{other}&&source->taskbarRemoved&&!target->taskbarRemoved);
    session->settings.visible=false;session->updateTaskbar();assert(taskbar->entries.size()==2); // hidden bar retains access
    session->settings.visible=true;target->independent=true;target->groupId=0;session->updateTaskbar();assert(taskbar->entries.size()==2); // independent windows remain separate
    target->independent=false;target->groupId=2;session->groupActives[2]=target->id;session->updateTaskbar();assert(taskbar->entries.size()==2); // one entry per group
    target->groupId=1;session->groupActives[1]=source->id;taskbar->failAdd=root;session->updateTaskbar();assert(taskbar->entries.size()==2); // shell failure must not strand documents
    taskbar->failAdd=nullptr;session->updateTaskbar();assert(taskbar->entries.size()==1);
    session->settings.group=false;session->updateTaskbar();assert(taskbar->entries.size()==2&&!target->taskbarRemoved);
    session->settings.group=true;session->groupActives[1]=source->id;session->syncWindowObservers();
    assert(session->rootObservers.size()==2);
    session->groupActives[1]=target->id;session->activeId=target->id;
    SendMessageW(root,WM_SYSCOMMAND,SC_RESTORE,0);session->processWindowLayout();assert(session->activeTabId(root)==source->id);

    SetWindowPos(root,nullptr,180,190,620,420,SWP_NOACTIVATE|SWP_NOZORDER);
    session->processWindowLayout();
    GetWindowRect(root,&sourceRect);GetWindowRect(other,&actual);assert(EqualRect(&sourceRect,&actual));
    ShowWindow(root,SW_SHOWMINNOACTIVE);
    assert(session->queuedLayoutRoot==root&&session->queuedLayoutCommand&&session->queuedLayoutMinimize);
    // Word can activate a sibling during a custom-caption minimize before
    // idle synchronization. Its normal-size notification must not win.
    session->groupActives[1]=target->id;session->activeId=target->id;session->requestWindowLayout(other,false);
    assert(session->queuedLayoutRoot==root&&session->queuedLayoutMinimize);
    session->processWindowLayout();assert(IsIconic(root)&&IsIconic(other));
    ShowWindow(root,SW_SHOWNOACTIVATE);session->processWindowLayout();assert(!IsIconic(root)&&!IsIconic(other));
    GetWindowRect(root,&sourceRect);GetWindowRect(other,&actual);assert(EqualRect(&sourceRect,&actual));
    ShowWindow(root,SW_SHOWMAXIMIZED);session->processWindowLayout();assert(IsZoomed(root)&&IsZoomed(other));
    ShowWindow(root,SW_SHOWNOACTIVATE);session->processWindowLayout();assert(!IsZoomed(root)&&!IsZoomed(other));
    // Reproduce the *modal* move/size loop, not just a single SetWindowPos.
    // Office/shell COM and sibling layouts must wait until it has unwound.
    for(auto host:{Host::Word,Host::Excel,Host::PowerPoint}){
        session->host=host;session->groupActives[1]=source->id;session->activeId=source->id;
        GetWindowRect(other,&oldRect);taskbar->operations.clear();
        auto peerStyle=GetWindowLongPtrW(other,GWL_EXSTYLE);auto peerVisible=IsWindowVisible(other);
        SendMessageW(root,WM_ENTERSIZEMOVE,0,0);
        COLORREF maskColor=0;BYTE maskAlpha=255;DWORD maskFlags=0;
        assert(session->windowMasks.size()==1&&session->maskSource==root);
        assert(GetLayeredWindowAttributes(other,&maskColor,&maskAlpha,&maskFlags)&&maskAlpha==0&&maskFlags==LWA_ALPHA);
        assert(IsWindowVisible(other)==peerVisible&&!(GetWindowLongPtrW(root,GWL_EXSTYLE)&WS_EX_LAYERED));
        for(int i=0;i<120;i++){
            SetWindowPos(root,nullptr,180+i,190,620+i%30,420+i%20,SWP_NOACTIVATE|SWP_NOZORDER);
            session->processWindowLayout();assert(!session->refresh());
            GetWindowRect(other,&actual);assert(EqualRect(&oldRect,&actual));
        }
        assert(taskbar->operations.empty()&&session->windowFrameBusy());
        SendMessageW(root,WM_EXITSIZEMOVE,0,0);session->processWindowLayout();
        assert(!session->windowFrameBusy()&&!session->queuedLayoutRoot);
        assert(session->windowMasks.empty()&&!session->maskSource&&GetWindowLongPtrW(other,GWL_EXSTYLE)==peerStyle);
        GetWindowRect(root,&sourceRect);GetWindowRect(other,&actual);assert(EqualRect(&sourceRect,&actual));
        // The old commanded/unchanged branch generated a new REFRESH on
        // every pass. Stable geometry now schedules no further work.
        session->refreshPending=false;session->operatingRoot=root;
        for(int i=0;i<120;i++)session->rootLayoutChanged(root);
        assert(!session->refreshPending&&taskbar->operations.empty());session->operatingRoot=nullptr;
        // Nested Office callbacks must not reset an outer coordination guard.
        session->coordinatingWindows=true;session->syncWindowObservers();assert(session->coordinatingWindows);session->coordinatingWindows=false;
        SendMessageW(root,WM_ENTERSIZEMOVE,0,0);assert(session->windowMasks.size()==1);
        SetWindowPos(root,nullptr,1200,700,800,600,SWP_NOACTIVATE|SWP_NOZORDER);
        SendMessageW(root,WM_CANCELMODE,0,0);session->processWindowLayout();
        assert(!session->windowFrameBusy()&&!session->operatingRoot&&session->windowMasks.empty()&&GetWindowLongPtrW(other,GWL_EXSTYLE)==peerStyle);
        GetWindowRect(root,&sourceRect);GetWindowRect(other,&actual);assert(EqualRect(&sourceRect,&actual));
    }
    // Preserve a pre-existing layered window's exact color/alpha policy.
    auto peerStyle=GetWindowLongPtrW(other,GWL_EXSTYLE);
    SetWindowLongPtrW(other,GWL_EXSTYLE,peerStyle|WS_EX_LAYERED);
    assert(SetLayeredWindowAttributes(other,RGB(12,34,56),93,LWA_ALPHA|LWA_COLORKEY));
    SendMessageW(root,WM_ENTERSIZEMOVE,0,0);assert(session->windowMasks.size()==1);
    SendMessageW(root,WM_EXITSIZEMOVE,0,0);session->processWindowLayout();
    COLORREF restoredColor=0;BYTE restoredAlpha=0;DWORD restoredFlags=0;
    assert(GetLayeredWindowAttributes(other,&restoredColor,&restoredAlpha,&restoredFlags)&&restoredColor==RGB(12,34,56)&&restoredAlpha==93&&restoredFlags==(LWA_ALPHA|LWA_COLORKEY));
    SetWindowLongPtrW(other,GWL_EXSTYLE,peerStyle);
    // An interrupted transaction without capture recovers after inactivity.
    SendMessageW(root,WM_ENTERSIZEMOVE,0,0);assert(session->windowMasks.size()==1);
    session->maskActivity=GetTickCount64()-4000;session->processWindowLayout();
    assert(!session->windowFrameBusy()&&session->windowMasks.empty()&&GetWindowLongPtrW(other,GWL_EXSTYLE)==peerStyle);
    target->independent=true;target->groupId=0;GetWindowRect(other,&oldRect);
    SendMessageW(root,WM_ENTERSIZEMOVE,0,0);assert(session->windowMasks.empty());SendMessageW(root,WM_EXITSIZEMOVE,0,0);session->processWindowLayout();
    SetWindowPos(root,nullptr,220,230,580,400,SWP_NOACTIVATE|SWP_NOZORDER);session->processWindowLayout();GetWindowRect(other,&actual);assert(EqualRect(&oldRect,&actual));
    target->independent=false;target->groupId=2;
    SendMessageW(root,WM_ENTERSIZEMOVE,0,0);assert(session->windowMasks.empty());SendMessageW(root,WM_EXITSIZEMOVE,0,0);session->processWindowLayout();
    SetWindowPos(root,nullptr,240,250,600,410,SWP_NOACTIVATE|SWP_NOZORDER);session->processWindowLayout();GetWindowRect(other,&actual);assert(EqualRect(&oldRect,&actual));
    target->groupId=1;session->settings.group=false;
    SetWindowPos(root,nullptr,260,270,610,420,SWP_NOACTIVATE|SWP_NOZORDER);session->processWindowLayout();GetWindowRect(other,&actual);assert(EqualRect(&oldRect,&actual));
    session->settings.group=true;SendMessageW(root,WM_ENTERSIZEMOVE,0,0);assert(session->windowMasks.size()==1);
    session->detachWindowObservers();assert(session->rootObservers.empty()&&session->windowMasks.empty()&&GetWindowLongPtrW(other,GWL_EXSTYLE)==peerStyle);
    session.reset();app.reset();DestroyWindow(other);DestroyWindow(root);CoUninitialize();
    std::cout<<"Tab slot geometry, restoration, independent/group boundaries, pane guards and OLE UI ownership passed.\n";
}
