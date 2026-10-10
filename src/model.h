#pragma once
#include "com.h"
enum class Host {Word,Excel,PowerPoint};
struct View {
    unsigned long long id=0;
    HWND hwnd=nullptr;
    Ptr<IDispatch> window,document;
    Ptr<IUnknown> identity;
    std::wstring title,path;
    bool saved=true,readOnly=false,locked=false,independent=false;
    WINDOWPLACEMENT original{sizeof(WINDOWPLACEMENT)},applied{sizeof(WINDOWPLACEMENT)};
    bool moved=false,taskbarRemoved=false;
};
class PaneChrome;
struct Pane {unsigned long long viewId=0;Ptr<IDispatch> office;Ptr<ITabBinding> control;std::shared_ptr<PaneChrome> chrome;unsigned initAttempts=0;long requestedHeight=0;};
struct Settings {
    bool visible=true,group=false,dark=false,autoHide=false,doubleClose=false,shortcuts=false;
    int position=1; // msoCTPDockPositionTop
    void load();void save()const;
};
class EventSink;
class Session : public std::enable_shared_from_this<Session> {
public:
    Ptr<IDispatch> app,factory;
    Host host=Host::Word;
    HWND timer=nullptr;
    Settings settings;
    std::vector<std::shared_ptr<View>> views;
    std::vector<Pane> panes;
    unsigned long long nextId=1,activeId=0,groupActiveId=0;
    Ptr<IConnectionPoint> connection;
    Ptr<EventSink> sink;
    DWORD cookie=0;
    Ptr<ITaskbarList> taskbar;
    bool refreshing=false,executing=false,creating=false,stopped=false,refreshPending=false;
    int failures=0;
    Session(IDispatch* application);
    ~Session();
    void start();void stop();void defer();bool refresh();
    void bindFactory(IDispatch* f);
    void event(DISPID id,DISPPARAMS* params);
    void syncPanes();void redraw();
    std::shared_ptr<View> find(unsigned long long id)const;
    std::vector<std::shared_ptr<View>> tabViews()const;
    unsigned long long activeTabId()const;
    bool showTabs(HWND root)const;
    bool setIndependent(unsigned long long id,bool independent);
    void activate(unsigned long long id);
    bool close(unsigned long long id);
    bool save(unsigned long long id,bool saveAs=false);
    void create();void open();
    void batchClose(std::vector<unsigned long long> ids);
    void saveAll();void reorder(unsigned long long from,unsigned long long to);
    void toggleGroup();void restore();void toggleVisible();void changed();
    void align(const std::shared_ptr<View>& target,const std::shared_ptr<View>& source);
    void updateTaskbar();void restoreView(View&);
    std::wstring state()const;
    void menu(HWND owner,unsigned long long id,POINT pt);
    void options(HWND owner,POINT pt);
    void report(const ComError& e);
};
class EventSink:public IDispatch {
    ULONG refs=1;Session* session;IID eventId;
public:
    EventSink(Session* s,IID i):session(s),eventId(i){objects++;}
    ~EventSink(){objects--;}
    void detach(){session=nullptr;}
    HRESULT STDMETHODCALLTYPE QueryInterface(REFIID i,void** p)override{if(!p)return E_POINTER;*p=nullptr;if(i==IID_IUnknown||i==IID_IDispatch||i==eventId){*p=static_cast<IDispatch*>(this);AddRef();return S_OK;}return E_NOINTERFACE;}
    ULONG STDMETHODCALLTYPE AddRef()override{return ++refs;}
    ULONG STDMETHODCALLTYPE Release()override{ULONG r=--refs;if(!r)delete this;return r;}
    DISPATCH_STUBS
    HRESULT STDMETHODCALLTYPE GetIDsOfNames(REFIID,LPOLESTR*,UINT,LCID,DISPID*)override{return E_NOTIMPL;}
    HRESULT STDMETHODCALLTYPE Invoke(DISPID id,REFIID,LCID,WORD,DISPPARAMS* params,VARIANT*,EXCEPINFO*,UINT*)override{if(session)try{session->event(id,params);}catch(...){log(L"event.rejected",E_FAIL);session->defer();}return S_OK;}
};
