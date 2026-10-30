#pragma once
#include "model.h"
#include <gdiplus.h>
// Windowed, locally implemented ActiveX control. Office owns the task pane container.
class Tabs final:public IDispatch,public IOleObject,public IOleInPlaceObject,
    public IOleInPlaceActiveObject,public IOleControl,public IViewObject2,
    public IPersistStreamInit,public ITabBinding {
    ULONG refs=1;
    Ptr<IOleClientSite> site;
    Ptr<IOleInPlaceSite> inPlace;
    Ptr<IAdviseSink> advise;
    DWORD adviseAspect=0,adviseFlags=0;
    SIZEL extent{15000,1100};
    HWND hwnd=nullptr,hostWindow=nullptr;
    HWND compactFrame=nullptr,oleHwnd=nullptr,seamWindow=nullptr;
    Session* session=nullptr;
    bool activated=false,layingOut=false;
    std::vector<std::pair<unsigned long long,RECT>> hit;
    RECT addRect{},listRect{},settingsRect{};
    unsigned long long pressed=0,hover=0;
    POINT down{};
    bool dragging=false,pressedClose=false;
    unsigned long long resizing=0;
    int resizeStartWidth=0,resizeOriginalWidth=0;
    HWND resizePreviousFocus=nullptr;
    int offset=0;
    unsigned long long lastSelected=0;
    diagnostic::Gate paintDiagnosticGate;
    unsigned long long diagnosticSelected=0,diagnosticGroup=0,diagnosticHover=0;
    HWND tooltip=nullptr;
    std::wstring hint;
    ULONG_PTR graphicsToken=0;
public:
    Tabs(){objects++;Gdiplus::GdiplusStartupInput input;Gdiplus::GdiplusStartup(&graphicsToken,&input,nullptr);} ~Tabs(){destroy();if(graphicsToken)Gdiplus::GdiplusShutdown(graphicsToken);objects--;}
    void destroy();void paint(HDC dc,RECT rc);void click(UINT msg,WPARAM wp,LPARAM lp);
    unsigned long long at(POINT p)const;
    unsigned long long edgeAt(POINT p)const;
    void finishResize(bool cancel);
    static LRESULT CALLBACK proc(HWND,UINT,WPARAM,LPARAM);
    HRESULT activate(HWND parent,LPCRECT rc,bool focus);
    HRESULT STDMETHODCALLTYPE QueryInterface(REFIID,void**)override;
    ULONG STDMETHODCALLTYPE AddRef()override{return ++refs;}
    ULONG STDMETHODCALLTYPE Release()override{ULONG r=--refs;if(!r)delete this;return r;}
    DISPATCH_STUBS
    HRESULT STDMETHODCALLTYPE GetIDsOfNames(REFIID,LPOLESTR*,UINT,LCID,DISPID*)override{return DISP_E_UNKNOWNNAME;}
    HRESULT STDMETHODCALLTYPE Invoke(DISPID,REFIID,LCID,WORD,DISPPARAMS*,VARIANT*,EXCEPINFO*,UINT*)override{return DISP_E_MEMBERNOTFOUND;}
    void STDMETHODCALLTYPE Bind(Session* s,HWND h)override{session=s;hostWindow=h;if(NativeWindow())InvalidateRect(NativeWindow(),nullptr,FALSE);}
    HWND STDMETHODCALLTYPE NativeWindow()override{return hwnd?hwnd:oleHwnd;}
    HWND STDMETHODCALLTYPE OleWindow()override{return oleHwnd;}
    bool STDMETHODCALLTYPE Mount(HWND)override;
    void STDMETHODCALLTYPE Unmount()override;
    void layout();
    void layoutSeam();static LRESULT CALLBACK seamProc(HWND,UINT,WPARAM,LPARAM);
    HWND STDMETHODCALLTYPE SeamWindow()override{return seamWindow;}
    // IOleObject
    HRESULT STDMETHODCALLTYPE SetClientSite(IOleClientSite* s)override{site=Ptr<IOleClientSite>(s);return S_OK;}
    HRESULT STDMETHODCALLTYPE GetClientSite(IOleClientSite** s)override{if(!s)return E_POINTER;*s=site.get();if(*s)(*s)->AddRef();return S_OK;}
    HRESULT STDMETHODCALLTYPE SetHostNames(LPCOLESTR,LPCOLESTR)override{return S_OK;}
    HRESULT STDMETHODCALLTYPE Close(DWORD)override{InPlaceDeactivate();site.reset();return S_OK;}
    HRESULT STDMETHODCALLTYPE SetMoniker(DWORD,IMoniker*)override{return E_NOTIMPL;}
    HRESULT STDMETHODCALLTYPE GetMoniker(DWORD,DWORD,IMoniker**)override{return E_NOTIMPL;}
    HRESULT STDMETHODCALLTYPE InitFromData(IDataObject*,BOOL,DWORD)override{return E_NOTIMPL;}
    HRESULT STDMETHODCALLTYPE GetClipboardData(DWORD,IDataObject**)override{return E_NOTIMPL;}
    HRESULT STDMETHODCALLTYPE DoVerb(LONG v,LPMSG,IOleClientSite* s,LONG,HWND p,LPCRECT r)override;
    HRESULT STDMETHODCALLTYPE EnumVerbs(IEnumOLEVERB**)override{return OLE_S_USEREG;}
    HRESULT STDMETHODCALLTYPE Update()override{return S_OK;}
    HRESULT STDMETHODCALLTYPE IsUpToDate()override{return S_OK;}
    HRESULT STDMETHODCALLTYPE GetUserClassID(CLSID* p)override{if(!p)return E_POINTER;*p=CLSID_Tabs;return S_OK;}
    HRESULT STDMETHODCALLTYPE GetUserType(DWORD,LPOLESTR* p)override{if(!p)return E_POINTER;const wchar_t* s=i18n::text(session?session->language():0,L"openOfficeTab 标签栏");*p=(LPOLESTR)CoTaskMemAlloc((wcslen(s)+1)*2);if(!*p)return E_OUTOFMEMORY;wcscpy_s(*p,wcslen(s)+1,s);return S_OK;}
    HRESULT STDMETHODCALLTYPE SetExtent(DWORD a,SIZEL* s)override{if(a!=DVASPECT_CONTENT)return DV_E_DVASPECT;if(!s)return E_POINTER;extent=*s;return S_OK;}
    HRESULT STDMETHODCALLTYPE GetExtent(DWORD a,SIZEL* s)override{if(a!=DVASPECT_CONTENT)return DV_E_DVASPECT;if(!s)return E_POINTER;*s=extent;return S_OK;}
    HRESULT STDMETHODCALLTYPE Advise(IAdviseSink* s,DWORD* c)override{if(!c)return E_POINTER;advise=Ptr<IAdviseSink>(s);*c=1;return S_OK;}
    HRESULT STDMETHODCALLTYPE Unadvise(DWORD)override{advise.reset();return S_OK;}
    HRESULT STDMETHODCALLTYPE EnumAdvise(IEnumSTATDATA**)override{return E_NOTIMPL;}
    HRESULT STDMETHODCALLTYPE GetMiscStatus(DWORD,DWORD* p)override{if(!p)return E_POINTER;*p=OLEMISC_SETCLIENTSITEFIRST|OLEMISC_INSIDEOUT|OLEMISC_ACTIVATEWHENVISIBLE|OLEMISC_RECOMPOSEONRESIZE|OLEMISC_NOUIACTIVATE;return S_OK;}
    HRESULT STDMETHODCALLTYPE SetColorScheme(LOGPALETTE*)override{return S_OK;}
    // IOleWindow / IOleInPlaceObject / IOleInPlaceActiveObject
    HRESULT STDMETHODCALLTYPE GetWindow(HWND* p)override{if(!p)return E_POINTER;*p=oleHwnd;return *p?S_OK:E_FAIL;}
    HRESULT STDMETHODCALLTYPE ContextSensitiveHelp(BOOL)override{return E_NOTIMPL;}
    HRESULT STDMETHODCALLTYPE InPlaceDeactivate()override;
    HRESULT STDMETHODCALLTYPE UIDeactivate()override{return S_OK;}
    HRESULT STDMETHODCALLTYPE SetObjectRects(LPCRECT r,LPCRECT)override{if(!r)return E_POINTER;if(oleHwnd)SetWindowPos(oleHwnd,nullptr,r->left,r->top,r->right-r->left,r->bottom-r->top,SWP_NOZORDER|SWP_NOACTIVATE);if(compactFrame)layout();return S_OK;}
    HRESULT STDMETHODCALLTYPE ReactivateAndUndo()override{return E_NOTIMPL;}
    HRESULT STDMETHODCALLTYPE TranslateAccelerator(LPMSG)override;
    HRESULT STDMETHODCALLTYPE OnFrameWindowActivate(BOOL)override{return S_OK;}
    HRESULT STDMETHODCALLTYPE OnDocWindowActivate(BOOL)override{return S_OK;}
    HRESULT STDMETHODCALLTYPE ResizeBorder(LPCRECT,IOleInPlaceUIWindow*,BOOL)override{return S_OK;}
    HRESULT STDMETHODCALLTYPE EnableModeless(BOOL)override{return S_OK;}
    // IOleControl
    HRESULT STDMETHODCALLTYPE GetControlInfo(CONTROLINFO* p)override{if(!p)return E_POINTER;*p={sizeof(CONTROLINFO),nullptr,0,0};return S_OK;}
    HRESULT STDMETHODCALLTYPE OnMnemonic(LPMSG)override{return S_FALSE;}
    HRESULT STDMETHODCALLTYPE OnAmbientPropertyChange(DISPID)override{return S_OK;}
    HRESULT STDMETHODCALLTYPE FreezeEvents(BOOL)override{return S_OK;}
    // IViewObject2
    HRESULT STDMETHODCALLTYPE Draw(DWORD a,LONG,void*,DVTARGETDEVICE*,HDC,HDC d,LPCRECTL r,LPCRECTL,BOOL(CALLBACK*)(ULONG_PTR),ULONG_PTR)override{if(a!=DVASPECT_CONTENT)return DV_E_DVASPECT;if(d&&r){RECT rc{r->left,r->top,r->right,r->bottom};paint(d,rc);}return S_OK;}
    HRESULT STDMETHODCALLTYPE GetColorSet(DWORD,LONG,void*,DVTARGETDEVICE*,HDC,LOGPALETTE**)override{return E_NOTIMPL;}
    HRESULT STDMETHODCALLTYPE Freeze(DWORD,LONG,void*,DWORD*)override{return E_NOTIMPL;}
    HRESULT STDMETHODCALLTYPE Unfreeze(DWORD)override{return E_NOTIMPL;}
    HRESULT STDMETHODCALLTYPE SetAdvise(DWORD a,DWORD f,IAdviseSink* s)override{adviseAspect=a;adviseFlags=f;advise=Ptr<IAdviseSink>(s);return S_OK;}
    HRESULT STDMETHODCALLTYPE GetAdvise(DWORD* a,DWORD* f,IAdviseSink** s)override{if(a)*a=adviseAspect;if(f)*f=adviseFlags;if(s){*s=advise.get();if(*s)(*s)->AddRef();}return S_OK;}
    HRESULT STDMETHODCALLTYPE GetExtent(DWORD a,LONG,DVTARGETDEVICE*,LPSIZEL s)override{return GetExtent(a,s);}
    // IPersist / IPersistStreamInit: no documents or executable content are serialized.
    HRESULT STDMETHODCALLTYPE GetClassID(CLSID* p)override{return GetUserClassID(p);}
    HRESULT STDMETHODCALLTYPE IsDirty()override{return S_FALSE;}
    HRESULT STDMETHODCALLTYPE Load(IStream*)override{return S_OK;}
    HRESULT STDMETHODCALLTYPE Save(IStream*,BOOL)override{return S_OK;}
    HRESULT STDMETHODCALLTYPE GetSizeMax(ULARGE_INTEGER* p)override{if(!p)return E_POINTER;p->QuadPart=0;return S_OK;}
    HRESULT STDMETHODCALLTYPE InitNew()override{return S_OK;}
};
