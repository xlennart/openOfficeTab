#include "../src/tabs.h"
#include "../src/window-layout.h"
#include <cassert>
#include <iostream>

// No Office or screenshots. Model different document contents in two native
// roots, including an Office/native activation disagreement and reentrant restore.
class Dispatch final:public IDispatch {
    ULONG refs=1;std::map<DISPID,std::wstring> names;DISPID next=1;
public:
    std::map<std::wstring,Var> properties;
    std::vector<Ptr<IDispatch>> members;
    std::wstring rejected;
    std::function<void()> activate;
    unsigned queries=0,activations=0;
    HRESULT STDMETHODCALLTYPE QueryInterface(REFIID id,void** out)override{if(!out)return E_POINTER;*out=nullptr;if(id!=IID_IUnknown&&id!=IID_IDispatch)return E_NOINTERFACE;*out=this;AddRef();return S_OK;}
    ULONG STDMETHODCALLTYPE AddRef()override{return ++refs;}
    ULONG STDMETHODCALLTYPE Release()override{auto n=--refs;if(!n)delete this;return n;}
    DISPATCH_STUBS
    HRESULT STDMETHODCALLTYPE GetIDsOfNames(REFIID,LPOLESTR* ns,UINT,LCID,DISPID* ids)override{
        queries++;std::wstring name=*ns;
        if(!properties.count(name)&&name!=L"Item"&&name!=L"Activate")return DISP_E_UNKNOWNNAME;
        for(auto& entry:names)if(entry.second==name){*ids=entry.first;return S_OK;}
        *ids=next++;names[*ids]=name;return S_OK;
    }
    HRESULT STDMETHODCALLTYPE Invoke(DISPID id,REFIID,LCID,WORD,DISPPARAMS* args,VARIANT* result,EXCEPINFO*,UINT*)override{
        auto name=names[id];if(name==rejected)return DISP_E_EXCEPTION;
        if(name==L"Activate"){activations++;if(activate)activate();return S_OK;}
        if(!result)return E_POINTER;VariantInit(result);
        if(name==L"Item"){
            if(!args||!args->cArgs)return E_INVALIDARG;Var arg;VariantCopy(&arg.v,&args->rgvarg[0]);auto index=arg.number();
            if(index<1||static_cast<size_t>(index)>members.size())return DISP_E_BADINDEX;
            Var value(members[static_cast<size_t>(index)-1].get());return VariantCopy(result,&value.v);
        }
        auto found=properties.find(name);if(found==properties.end())return DISP_E_MEMBERNOTFOUND;
        return VariantCopy(result,&found->second.v);
    }
};
class Binding final:public ITabBinding {
    ULONG refs=1;HWND display;
public:
    explicit Binding(HWND h):display(h){}
    HRESULT STDMETHODCALLTYPE QueryInterface(REFIID id,void** out)override{if(!out)return E_POINTER;*out=nullptr;if(id!=IID_IUnknown&&id!=__uuidof(ITabBinding))return E_NOINTERFACE;*out=this;AddRef();return S_OK;}
    ULONG STDMETHODCALLTYPE AddRef()override{return ++refs;}
    ULONG STDMETHODCALLTYPE Release()override{auto n=--refs;if(!n)delete this;return n;}
    void STDMETHODCALLTYPE Bind(Session*,HWND)override{}
    HWND STDMETHODCALLTYPE NativeWindow()override{return display;}
    HWND STDMETHODCALLTYPE OleWindow()override{return nullptr;}
    bool STDMETHODCALLTYPE Mount(HWND)override{return true;}
    void STDMETHODCALLTYPE Unmount()override{}
    HWND STDMETHODCALLTYPE SeamWindow()override{return nullptr;}
};
Ptr<Dispatch> dispatch(){return Ptr<Dispatch>(new Dispatch(),false);}
std::shared_ptr<View> view(unsigned long long id,HWND root,IDispatch* window,IDispatch* document){
    auto v=std::make_shared<View>();v->id=id;v->hwnd=root;v->window=Ptr<IDispatch>(window);v->document=Ptr<IDispatch>(document);v->identity=v->window.as<IUnknown>();v->title=L"File "+std::to_wstring(id);GetWindowPlacement(root,&v->original);return v;
}
void test(Host host){
    // WS_VISIBLE is needed to observe invalidation. Keep these tool windows
    // offscreen and out of the taskbar; never automate a user's desktop window.
    HWND a=CreateWindowExW(WS_EX_TOOLWINDOW|WS_EX_NOACTIVATE,L"STATIC",L"content A",WS_POPUP|WS_VISIBLE,-30000,-30000,600,500,nullptr,nullptr,module,nullptr);
    HWND b=CreateWindowExW(WS_EX_TOOLWINDOW|WS_EX_NOACTIVATE,L"STATIC",L"content B",WS_POPUP|WS_VISIBLE,-30000,-30000,600,500,nullptr,nullptr,module,nullptr);
    HWND bar=CreateWindowExW(0,L"STATIC",L"",WS_CHILD|WS_VISIBLE,0,0,400,40,a,nullptr,module,nullptr);assert(a&&b&&bar);
    auto da=dispatch(),db=dispatch(),wa=dispatch(),wb=dispatch(),docs=dispatch(),windows=dispatch(),app=dispatch();
    for(auto doc:{da.get(),db.get()}){doc->properties[L"Path"]=Var(std::wstring());doc->properties[L"Saved"]=Var(true);doc->properties[L"ReadOnly"]=Var(false);}
    da->properties[L"Name"]=Var(std::wstring(L"File 1"));db->properties[L"Name"]=Var(std::wstring(L"File 2"));
    wa->properties[L"Hwnd"]=Var(static_cast<long>(reinterpret_cast<INT_PTR>(a)));wb->properties[L"Hwnd"]=Var(static_cast<long>(reinterpret_cast<INT_PTR>(b)));
    wa->properties[L"Document"]=wa->properties[L"Presentation"]=Var(da.get());wb->properties[L"Document"]=wb->properties[L"Presentation"]=Var(db.get());
    docs->members={Ptr<IDispatch>(da.get()),Ptr<IDispatch>(db.get())};docs->properties[L"Count"]=Var(2L);
    windows->members={Ptr<IDispatch>(wa.get()),Ptr<IDispatch>(wb.get())};windows->properties[L"Count"]=Var(2L);
    app->properties[L"Name"]=Var(std::wstring(host==Host::Word?L"Word":L"PowerPoint"));
    app->properties[L"Documents"]=app->properties[L"Presentations"]=Var(docs.get());app->properties[L"Windows"]=Var(windows.get());app->properties[L"ActiveWindow"]=Var(wa.get());
    wa->activate=[&]{app->properties[L"ActiveWindow"]=Var(wa.get());};wb->activate=[&]{app->properties[L"ActiveWindow"]=Var(wb.get());};
    wchar_t cwd[32768];GetCurrentDirectoryW(32768,cwd);
    auto s=std::make_shared<Session>(app.get(),std::wstring(cwd)+L"\\selection-state-"+std::to_wstring(GetCurrentProcessId()));
    s->settings=Settings{};s->settings.themeMode=1;s->settings.widthMode=0;s->views={view(1,a,wa.get(),da.get()),view(2,b,wb.get(),db.get())};s->nextId=3;
    SetActiveWindow(a);assert(s->refresh());
    Pane pane;pane.viewId=1;pane.control=Ptr<ITabBinding>(new Binding(bar),false);s->panes.push_back(std::move(pane));
    // An internally selected B must not be drawn over native A's content.
    s->setActiveView(2);assert(s->activeTabId(a)==2&&s->displayedTabId(a)==1&&s->displayedTabId(b)==2);
    Ptr<Tabs> tabs(new Tabs(),false);HDC dc=CreateCompatibleDC(nullptr);BITMAPINFO info{};info.bmiHeader.biSize=sizeof(BITMAPINFOHEADER);info.bmiHeader.biWidth=720;info.bmiHeader.biHeight=-40;info.bmiHeader.biPlanes=1;info.bmiHeader.biBitCount=32;info.bmiHeader.biCompression=BI_RGB;
    void* bits=nullptr;HBITMAP image=CreateDIBSection(dc,&info,DIB_RGB_COLORS,&bits,nullptr,0);auto old=SelectObject(dc,image);
    COLORREF accent=host==Host::Word?RGB(35,91,180):RGB(196,59,28);
    auto accents=[&](int left){int n=0;for(int y=0;y<10;y++)for(int x=left;x<left+50;x++)if(GetPixel(dc,x,y)==accent)n++;return n;};
    tabs->Bind(s.get(),a);tabs->paint(dc,RECT{0,0,720,40});GdiFlush();assert(accents(40)>30&&accents(160)==0);
    tabs->Bind(s.get(),b);tabs->paint(dc,RECT{0,0,720,40});GdiFlush();assert(accents(40)==0&&accents(160)>30);
    tabs->Bind(nullptr,nullptr);tabs.reset();SelectObject(dc,old);DeleteObject(image);DeleteDC(dc);
    // Restoring an unchanged slot must invalidate the previous painted state,
    // even when the next refresh reads exactly the already-committed A.
    ValidateRect(bar,nullptr);assert(!GetUpdateRect(bar,nullptr,FALSE));
    s->operatingRoot=a;s->rootLayoutChanged(a);s->operatingRoot=nullptr;
    assert(s->activeId==1&&GetUpdateRect(bar,nullptr,FALSE));assert(s->refresh()&&s->activeId==1);
    // Reproduce the issue's direction: COM points at B, native A still displays
    // A's content. Do not accept the COM id alone as a successful file switch.
    app->properties[L"ActiveWindow"]=Var(wb.get());SetActiveWindow(a);assert(s->refresh());
    assert(s->activeId==1&&s->activeWindowMatchesNative==0&&s->displayedTabId(a)==1);
    auto before=wa->activations;s->operatingRoot=a;s->rootLayoutChanged(a);s->operatingRoot=nullptr;
    assert(wa->activations==before+1&&prop(app.get(),L"ActiveWindow").disp().get()==wa.get()&&GetActiveWindow()==a);
    assert(s->refresh()&&s->activeWindowMatchesNative==1);
    // A real switch must move the native root and COM selection together.
    s->activate(2,a);assert(GetActiveWindow()==b&&prop(app.get(),L"ActiveWindow").disp().get()==wb.get()&&s->activeId==2);
    wchar_t content[32];GetWindowTextW(GetActiveWindow(),content,32);assert(std::wstring(content)==L"content B");
    // Office can restore the old root reentrantly during the native handover.
    SetActiveWindow(a);app->properties[L"ActiveWindow"]=Var(wa.get());s->refresh();
    struct Restore {HWND previous;int remaining;};Restore restore{a,2};
    auto restoreProc=+[](HWND w,UINT m,WPARAM p,LPARAM l,UINT_PTR,DWORD_PTR ref)->LRESULT{
        auto result=DefSubclassProc(w,m,p,l);auto r=reinterpret_cast<Restore*>(ref);
        if(m==WM_ACTIVATE&&LOWORD(p)!=WA_INACTIVE&&r->remaining>0){r->remaining--;SetActiveWindow(r->previous);}return result;
    };
    assert(SetWindowSubclass(b,restoreProc,137,reinterpret_cast<DWORD_PTR>(&restore)));
    s->activate(2,a);assert(restore.remaining==0&&GetActiveWindow()==b&&s->activeId==2&&prop(app.get(),L"ActiveWindow").disp().get()==wb.get());
    RemoveWindowSubclass(b,restoreProc,137);
    // Disabled modal roots must not be "selected" through COM behind a dialog.
    SetActiveWindow(a);app->properties[L"ActiveWindow"]=Var(wa.get());s->refresh();before=wb->activations;
    EnableWindow(b,FALSE);s->activate(2,a);assert(wb->activations==before&&s->activeId==1&&GetActiveWindow()==a);EnableWindow(b,TRUE);
    // Unknown active state is a rejected snapshot, including proposed removals.
    app->rejected=L"ActiveWindow";windows->members={Ptr<IDispatch>(wb.get())};windows->properties[L"Count"]=Var(1L);
    auto ids=s->views;assert(!s->refresh()&&s->activeId==1&&s->views.size()==ids.size()&&s->failures>0&&!s->lastActiveWindowQuerySucceeded);
    windows->members={Ptr<IDispatch>(wa.get()),Ptr<IDispatch>(wb.get())};windows->properties[L"Count"]=Var(2L);app->rejected.clear();
    wa->rejected=L"Hwnd";if(host==Host::Word)assert(!s->refresh()&&s->views.size()==2);wa->rejected.clear();assert(s->refresh());
    // Native activation queues work, without calling Office from its callback.
    s->timer=CreateWindowExW(0,L"STATIC",L"",0,0,0,0,0,HWND_MESSAGE,nullptr,module,nullptr);s->refreshPending=false;
    auto queries=app->queries+wa->queries+wb->queries;SendMessageW(a,WM_ACTIVATE,WA_ACTIVE,0);
    assert(s->refreshPending&&queries==app->queries+wa->queries+wb->queries);
    // No ordinary documents is a valid empty snapshot, not an active-query error.
    docs->properties[L"Count"]=Var(0L);windows->properties[L"Count"]=Var(0L);app->rejected=L"ActiveWindow";
    assert(s->refresh()&&s->views.empty()&&s->activeId==0&&s->failures==0);
    s->stop();s.reset();DestroyWindow(a);DestroyWindow(b);
}
int main(){CoInitializeEx(nullptr,COINIT_APARTMENTTHREADED);module=GetModuleHandleW(nullptr);test(Host::Word);test(Host::PowerPoint);CoUninitialize();std::cout<<"Word/PPT native content, COM selection, rendered highlight, unchanged-slot reconciliation, reentrant handover, modal guards and unknown-state snapshots passed.\n";}
