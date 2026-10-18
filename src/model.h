#pragma once
#include "com.h"
#include "storage.h"
#include "localization.h"
enum class Host {Word,Excel,PowerPoint};
struct View {
    unsigned long long id=0;
    HWND hwnd=nullptr;
    Ptr<IDispatch> window,document;
    Ptr<IUnknown> identity;
    std::wstring title,path;
    bool saved=true,readOnly=false,locked=false,independent=false;
    unsigned groupId=1,lastGroupId=1;
    int manualTabWidth=0; // DIP, remembered by saved file; 0 uses the shared width policy.
    WINDOWPLACEMENT original{sizeof(WINDOWPLACEMENT)},applied{sizeof(WINDOWPLACEMENT)};
    RECT originalBounds{},appliedBounds{};
    bool moved=false,taskbarRemoved=false;
};
class PaneChrome;
class RootObserver;
class WindowMask;
struct GroupSlot {WINDOWPLACEMENT placement{sizeof(WINDOWPLACEMENT)};RECT bounds{};bool minimized=false;};
struct FrameCloseChoice {int action=0;bool remember=false;}; // 0 cancel, 1 current, 2 group
struct Pane {unsigned long long viewId=0;Ptr<IDispatch> office;Ptr<ITabBinding> control;std::shared_ptr<PaneChrome> chrome;unsigned initAttempts=0;long requestedHeight=0;};
struct Settings {
    bool visible=true,group=true,dark=false,autoHide=false,doubleClose=false,shortcuts=false;
    int themeMode=0,frameCloseAction=0,language=0; // language: Windows / Chinese / English
    int position=1; // msoCTPDockPositionTop
    int tabWidth=120,widthMode=2; // DIP; fixed, title, shrink
    int accentPosition=1,barStyle=1; // 0 bottom / 1 top / 2 none; 0 panel / 1 blended
    COLORREF barColor=RGB(240,240,240);
    bool customBarColor=false;
    std::wstring root;
    mutable storage::Section baseline;
    mutable bool savePending=false;
    bool load();bool save()const;bool synchronize();
};
class EventSink;
class Session : public std::enable_shared_from_this<Session> {
public:
    Ptr<IDispatch> app,factory;
    Ptr<IDispatch> ribbon;
    Host host=Host::Word;
    HWND timer=nullptr;
    Settings settings;
    std::wstring storageRoot;
    unsigned long long settingsStamp=0;
    ULONGLONG lastSettingsPoll=0;
    std::map<std::wstring,storage::FilePreference> pendingPreferences;
    std::vector<std::wstring> pendingRecent;
    std::vector<std::shared_ptr<View>> views;
    std::vector<Pane> panes;
    unsigned long long nextId=1,activeId=0,groupActiveId=0;
    unsigned nextGroupId=2,newDocumentGroup=1;
    std::map<unsigned,unsigned long long> groupActives;
    bool pendingNewDocument=false;
    ULONGLONG pendingNewSince=0;
    unsigned long long pendingCloseId=0;
    bool pendingCloseAccepted=false;
    ULONGLONG pendingCloseUntil=0;
    std::vector<unsigned long long> pendingBatch;
    HRESULT lastError=S_OK;
    std::wstring lastOperation=L"startup";
    Ptr<IConnectionPoint> connection;
    Ptr<EventSink> sink;
    DWORD cookie=0;
    Ptr<ITaskbarList> taskbar;
    std::map<HWND,std::shared_ptr<RootObserver>> rootObservers;
    std::map<unsigned,GroupSlot> groupSlots;
    bool coordinatingWindows=false,frameDialogActive=false,frameBatch=false;
    HWND pendingFrameClose=nullptr,operatingRoot=nullptr;
    HWND sizingRoot=nullptr,queuedLayoutRoot=nullptr;
    unsigned nativeFrameDepth=0;
    bool queuedLayoutCommand=false,queuedLayoutMinimize=false;
    std::map<HWND,std::shared_ptr<WindowMask>> windowMasks;
    HWND maskSource=nullptr;
    ULONGLONG maskActivity=0;
    int officeTheme=-1;bool systemDark=false;ULONGLONG lastThemePoll=0;
    int lastLanguage=0;
    std::function<FrameCloseChoice(HWND,size_t)> frameClosePrompt;
    bool refreshing=false,executing=false,creating=false,stopped=false,refreshPending=false;
    int failures=0;
    Session(IDispatch* application,const std::wstring& root=L"");
    ~Session();
    void start();void stop();void defer();bool refresh();
    void bindFactory(IDispatch* f);
    void event(DISPID id,DISPPARAMS* params);
    void syncPanes();void redraw();
    std::shared_ptr<View> find(unsigned long long id)const;
    unsigned groupForWindow(HWND root)const;
    unsigned currentGroup()const;
    std::vector<std::shared_ptr<View>> tabViews(HWND root=nullptr)const;
    unsigned long long activeTabId(HWND root=nullptr)const;
    bool showTabs(HWND root)const;
    bool setIndependent(unsigned long long id,bool independent);
    bool moveToGroup(unsigned long long id,unsigned group);
    bool newTabGroup(unsigned long long id,const POINT* position=nullptr);
    bool drop(unsigned long long id,POINT screen);
    void chooseGroup(HWND owner,unsigned long long id);
    void activate(unsigned long long id,HWND owner=nullptr);
    bool close(unsigned long long id,bool fromFrame=false);
    bool save(unsigned long long id,bool saveAs=false);
    void create(bool independent=false,HWND owner=nullptr);void open();void quitIfEmpty();void finishClose();void nativeClose();
    void batchClose(std::vector<unsigned long long> ids,bool fromFrame=false);
    void saveAll(HWND owner=nullptr);void reorder(unsigned long long from,unsigned long long to);
    void toggleGroup();void restore();void toggleVisible();void changed();
    void align(const std::shared_ptr<View>& target,const std::shared_ptr<View>& source);
    void updateTaskbar();void restoreView(View&);
    void syncWindowObservers();void detachWindowObservers();void rootLayoutChanged(HWND root);
    void requestWindowLayout(HWND root,bool commanded=false,bool minimize=false);void processWindowLayout();
    void beginWindowTransition(HWND root);void restoreWindowMasks();
    bool windowFrameBusy()const{return nativeFrameDepth!=0||sizingRoot!=nullptr;}
    bool queueFrameClose(HWND root);void processFrameClose();
    bool darkMode()const;COLORREF surroundColor()const;bool pollTheme(bool force=false);bool setThemeMode(int mode);bool setFrameCloseAction(int action);
    int language()const{return i18n::resolve(settings.language);}
    const wchar_t* tr(const wchar_t* key)const{return i18n::text(language(),key);}
    bool setLanguage(int mode);void refreshLanguage();
    std::wstring state()const;
    std::wstring diagnostics()const;
    bool recover();bool setTabWidth(int width,int mode);
    bool setAppearance(int accent,int style);
    bool setBarColor(const std::wstring& color);void colorDialog(HWND owner);
    bool setViewWidth(unsigned long long id,int width);
    void widthDialog(HWND owner);void showDiagnostics(HWND owner);void exportDiagnostics(HWND owner);
    void menu(HWND owner,unsigned long long id,POINT pt);
    void options(HWND owner,POINT pt);
    void report(const ComError& e);
    void persistView(const View& view);void initializePreference(View& view,bool isNew,const std::wstring& oldPath);
    void pollPersistence();void flushPersistence(unsigned milliseconds=0);bool resetFilePreferences(unsigned long long id);bool resetAllFilePreferences();
    std::vector<std::wstring> recentlyClosed()const;bool reopenClosed(unsigned index);bool reopenPath(const std::wstring& path);bool clearRecentlyClosed();
    void rememberClosed(const std::wstring& path);void recentMenu(HWND owner,POINT point);
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
