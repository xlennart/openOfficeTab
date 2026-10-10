#pragma once
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <ole2.h>
#include <ocidl.h>
#include <commctrl.h>
#include <shlobj.h>
#include <shobjidl.h>
#include <shellapi.h>
#include <string>
#include <vector>
#include <map>
#include <memory>
#include <algorithm>
#include <atomic>
#include <sstream>
#include <fstream>
#include <functional>
#include <stdexcept>

extern HINSTANCE module;
extern std::atomic<long> objects;
inline const CLSID CLSID_Addin = {0x65f17021,0x3b6b,0x4c5e,{0xb5,0x4b,0x51,0x28,0x06,0x43,0x17,0x10}};
inline const CLSID CLSID_Tabs = {0x865f57da,0x724b,0x4f9c,{0xa6,0xd6,0xac,0x20,0x65,0xc7,0x02,0x38}};
struct __declspec(uuid("B65AD801-ABAF-11D0-BB8B-00A0C90F2744")) IExtensibility : IDispatch {
    virtual HRESULT STDMETHODCALLTYPE OnConnection(IDispatch*,long,IDispatch*,SAFEARRAY**) = 0;
    virtual HRESULT STDMETHODCALLTYPE OnDisconnection(long,SAFEARRAY**) = 0;
    virtual HRESULT STDMETHODCALLTYPE OnAddInsUpdate(SAFEARRAY**) = 0;
    virtual HRESULT STDMETHODCALLTYPE OnStartupComplete(SAFEARRAY**) = 0;
    virtual HRESULT STDMETHODCALLTYPE OnBeginShutdown(SAFEARRAY**) = 0;
};
struct __declspec(uuid("000C033E-0000-0000-C000-000000000046")) IPaneConsumer : IDispatch {
    virtual HRESULT STDMETHODCALLTYPE CTPFactoryAvailable(IDispatch*) = 0;
};
struct __declspec(uuid("000C0396-0000-0000-C000-000000000046")) IRibbon : IDispatch {
    virtual HRESULT STDMETHODCALLTYPE GetCustomUI(BSTR,BSTR*) = 0;
};
class Session;
struct __declspec(uuid("6AD815EB-5317-4C5E-85A3-2832C8988BA1")) ITabBinding : IUnknown {
    virtual void STDMETHODCALLTYPE Bind(Session*,HWND) = 0;
    virtual HWND STDMETHODCALLTYPE NativeWindow() = 0;
    virtual HWND STDMETHODCALLTYPE OleWindow() = 0;
    virtual bool STDMETHODCALLTYPE Mount(HWND) = 0;
    virtual void STDMETHODCALLTYPE Unmount() = 0;
};

template<class T> class Ptr {
    T* p_ = nullptr;
public:
    Ptr() = default;
    explicit Ptr(T* p, bool add=true) : p_(p) { if(p_&&add)p_->AddRef(); }
    Ptr(const Ptr& p) : Ptr(p.p_) {}
    Ptr(Ptr&& p) noexcept : p_(p.p_) {p.p_=nullptr;}
    ~Ptr(){if(p_)p_->Release();}
    Ptr& operator=(Ptr p){std::swap(p_,p.p_);return *this;}
    T* get() const{return p_;} T* operator->()const{return p_;}
    explicit operator bool()const{return p_!=nullptr;}
    T** out(){reset();return &p_;} void reset(){if(p_)p_->Release();p_=nullptr;}
    template<class U> Ptr<U> as()const{Ptr<U> r;if(p_)p_->QueryInterface(__uuidof(U),(void**)r.out());return r;}
};
class Var {
public:
    VARIANT v;
    Var(){VariantInit(&v);} explicit Var(long x):Var(){v.vt=VT_I4;v.lVal=x;}
    explicit Var(bool x):Var(){v.vt=VT_BOOL;v.boolVal=x?VARIANT_TRUE:VARIANT_FALSE;}
    explicit Var(const std::wstring& x):Var(){v.vt=VT_BSTR;v.bstrVal=SysAllocStringLen(x.data(),(UINT)x.size());}
    explicit Var(IDispatch* x):Var(){v.vt=VT_DISPATCH;v.pdispVal=x;if(x)x->AddRef();}
    Var(const Var& x):Var(){VariantCopy(&v,const_cast<VARIANT*>(&x.v));}
    Var(Var&& x) noexcept:v(x.v){VariantInit(&x.v);}
    Var& operator=(Var x){std::swap(v,x.v);return *this;}
    ~Var(){VariantClear(&v);}
    static Var missing(){Var r;r.v.vt=VT_ERROR;r.v.scode=DISP_E_PARAMNOTFOUND;return r;}
    Ptr<IDispatch> disp()const{if(v.vt==VT_DISPATCH)return Ptr<IDispatch>(v.pdispVal);if(v.vt==VT_UNKNOWN&&v.punkVal)return Ptr<IUnknown>(v.punkVal).as<IDispatch>();return {};}
    long number(long fallback=0)const{VARIANT r;VariantInit(&r);if(FAILED(VariantChangeType(&r,const_cast<VARIANT*>(&v),0,VT_I4)))return fallback;long x=r.lVal;VariantClear(&r);return x;}
    bool boolean(bool fallback=false)const{if(v.vt==VT_EMPTY||v.vt==VT_ERROR)return fallback;return number(fallback?-1:0)!=0;}
    std::wstring str()const{if(v.vt!=VT_BSTR||!v.bstrVal)return L"";return std::wstring(v.bstrVal,SysStringLen(v.bstrVal));}
};
class ComError:public std::exception {public: HRESULT hr; std::wstring where; ComError(HRESULT h,std::wstring w):hr(h),where(std::move(w)){} const char* what()const noexcept override{return "Office COM call failed";}};
inline Var call(IDispatch* p,const wchar_t* name,WORD flags=DISPATCH_PROPERTYGET,std::vector<Var> args={}) {
    if(!p)throw ComError(E_POINTER,name);
    LPOLESTR n=const_cast<LPOLESTR>(name);DISPID id;HRESULT hr=p->GetIDsOfNames(IID_NULL,&n,1,LOCALE_USER_DEFAULT,&id);
    if(FAILED(hr))throw ComError(hr,name);
    std::reverse(args.begin(),args.end());std::vector<VARIANT> raw;for(auto& x:args)raw.push_back(x.v);
    DISPID put=DISPID_PROPERTYPUT;DISPPARAMS dp{raw.data(),nullptr,(UINT)raw.size(),0};
    if(flags&(DISPATCH_PROPERTYPUT|DISPATCH_PROPERTYPUTREF)){dp.rgdispidNamedArgs=&put;dp.cNamedArgs=1;}
    Var r;EXCEPINFO ex{};UINT bad=0;hr=p->Invoke(id,IID_NULL,LOCALE_USER_DEFAULT,flags,&dp,&r.v,&ex,&bad);
    if(ex.pfnDeferredFillIn)ex.pfnDeferredFillIn(&ex);
    SysFreeString(ex.bstrSource);SysFreeString(ex.bstrDescription);SysFreeString(ex.bstrHelpFile);
    if(FAILED(hr))throw ComError(hr,name);return r;
}
inline Var prop(IDispatch* p,const wchar_t* n){try{return call(p,n);}catch(...){return {};}}
inline void put(IDispatch* p,const wchar_t* n,Var x){call(p,n,DISPATCH_PROPERTYPUT,{std::move(x)});}
inline Ptr<IDispatch> item(IDispatch* p,long n){return call(p,L"Item",DISPATCH_PROPERTYGET|DISPATCH_METHOD,{Var(n)}).disp();}
inline std::wstring json(const std::wstring& s){std::wstring r=L"\"";for(wchar_t c:s){if(c==L'"'||c==L'\\'){r+=L'\\';r+=c;}else if(c<32){wchar_t b[8];swprintf_s(b,L"\\u%04x",c);r+=b;}else r+=c;}return r+L"\"";}
inline std::wstring dataDir(){wchar_t p[MAX_PATH];if(FAILED(SHGetFolderPathW(nullptr,CSIDL_LOCAL_APPDATA,nullptr,0,p)))return L"";std::wstring d=std::wstring(p)+L"\\openOfficetab";CreateDirectoryW(d.c_str(),nullptr);return d;}
inline void log(const wchar_t* event,HRESULT hr=S_OK){std::wstring path=dataDir()+L"\\runtime.log";HANDLE h=CreateFileW(path.c_str(),FILE_APPEND_DATA,FILE_SHARE_READ|FILE_SHARE_WRITE,nullptr,OPEN_ALWAYS,FILE_ATTRIBUTE_NORMAL,nullptr);if(h==INVALID_HANDLE_VALUE)return;SYSTEMTIME st;GetLocalTime(&st);wchar_t b[512];swprintf_s(b,L"%04d-%02d-%02d %02d:%02d:%02d pid=%lu %s hr=%08x\r\n",st.wYear,st.wMonth,st.wDay,st.wHour,st.wMinute,st.wSecond,GetCurrentProcessId(),event,(unsigned)hr);int len=WideCharToMultiByte(CP_UTF8,0,b,-1,nullptr,0,nullptr,nullptr);std::string u(len,0);WideCharToMultiByte(CP_UTF8,0,b,-1,u.data(),len,nullptr,nullptr);DWORD wrote;WriteFile(h,u.data(),len-1,&wrote,nullptr);CloseHandle(h);}
inline int dpi(HWND w){return w?(int)GetDpiForWindow(w):96;}

#define DISPATCH_STUBS \
HRESULT STDMETHODCALLTYPE GetTypeInfoCount(UINT* n) override {if(!n)return E_POINTER;*n=0;return S_OK;} \
HRESULT STDMETHODCALLTYPE GetTypeInfo(UINT,LCID,ITypeInfo**) override {return E_NOTIMPL;}
