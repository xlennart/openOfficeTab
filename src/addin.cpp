#include "tabs.h"

HINSTANCE module=nullptr;
std::atomic<long> objects{0};
namespace {
class Addin final:public IExtensibility,public IPaneConsumer,public IRibbon {
    ULONG refs=1;
    std::shared_ptr<Session> session;
    Ptr<IDispatch> pendingFactory,addinInstance;
public:
    Addin(){objects++;}~Addin(){if(session)session->stop();objects--;}
    HRESULT STDMETHODCALLTYPE QueryInterface(REFIID i,void** p)override{
        if(!p)return E_POINTER;*p=nullptr;
        if(i==IID_IUnknown||i==IID_IDispatch||i==__uuidof(IExtensibility))*p=static_cast<IExtensibility*>(this);
        else if(i==__uuidof(IPaneConsumer))*p=static_cast<IPaneConsumer*>(this);
        else if(i==__uuidof(IRibbon))*p=static_cast<IRibbon*>(this);
        if(!*p)return E_NOINTERFACE;AddRef();return S_OK;
    }
    ULONG STDMETHODCALLTYPE AddRef()override{return ++refs;}
    ULONG STDMETHODCALLTYPE Release()override{ULONG n=--refs;if(!n)delete this;return n;}
    DISPATCH_STUBS
    HRESULT STDMETHODCALLTYPE GetIDsOfNames(REFIID,LPOLESTR* names,UINT n,LCID,DISPID* ids)override{
        const wchar_t* api[]={L"Refresh",L"GetState",L"Activate",L"Close",L"Save",L"NewDocument",L"ToggleGroup",L"ToggleVisible",L"RestoreWindows",L"ShowSettings",L"SetLocked",L"Reorder",L"SaveAll",L"CloseOther",L"OpenDocument",L"SetDark",L"SetIndependent",L"JoinTabs",L"DetachDocument",L"NewIndependentDocument",L"NewTabGroup",L"MoveToGroup",L"SetTabWidth",L"GetDiagnostics",L"Recover",L"ShowDiagnostics",L"ExportDiagnostics",L"ShowWidth",L"ChooseTabGroup",L"DropTab",L"SetAppearance",L"SetViewWidth",L"SetBarColor",L"ShowColor"};
        if(!names||!ids)return E_POINTER;
        for(UINT i=0;i<n;i++){ids[i]=DISPID_UNKNOWN;for(size_t j=0;j<std::size(api);j++)if(!_wcsicmp(names[i],api[j]))ids[i]=(DISPID)j+1;if(ids[i]==DISPID_UNKNOWN)return DISP_E_UNKNOWNNAME;}return S_OK;
    }
    HRESULT STDMETHODCALLTYPE Invoke(DISPID id,REFIID,LCID,WORD,DISPPARAMS* dp,VARIANT* result,EXCEPINFO*,UINT*)override{
        if(result)VariantInit(result);auto s=session;if(!s||s->stopped)return E_UNEXPECTED;
        auto arg=[&](UINT n)->Var{if(!dp||n>=dp->cArgs)throw ComError(DISP_E_BADPARAMCOUNT,L"Arguments");Var v;VariantCopy(&v.v,&dp->rgvarg[dp->cArgs-n-1]);return v;};
        auto booleanResult=[&](bool b){if(result){result->vt=VT_BOOL;result->boolVal=b?VARIANT_TRUE:VARIANT_FALSE;}};
        try{
            switch(id){
                case 1:booleanResult(s->refresh());break;
                case 2:if(result){result->vt=VT_BSTR;result->bstrVal=SysAllocString(s->state().c_str());}break;
                case 3:s->activate((unsigned long long)arg(0).number());break;
                case 4:booleanResult(s->close((unsigned long long)arg(0).number()));break;
                case 5:booleanResult(s->save((unsigned long long)arg(0).number()));break;
                case 6:s->create();break;
                case 7:s->toggleGroup();break;
                case 8:s->toggleVisible();break;
                case 9:s->settings.group=false;s->restore();s->changed();break;
                case 10:{POINT p;GetCursorPos(&p);auto v=s->find(s->activeId);s->options(v?v->hwnd:nullptr,p);break;}
                case 11:{auto v=s->find((unsigned long long)arg(0).number());if(v){v->locked=arg(1).boolean();s->redraw();}break;}
                case 12:s->reorder((unsigned long long)arg(0).number(),(unsigned long long)arg(1).number());break;
                case 13:s->saveAll();break;
                case 14:{auto target=(unsigned long long)arg(0).number();auto selected=s->find(target);if(!selected)break;std::vector<unsigned long long> ids;for(auto& v:s->tabViews(selected->hwnd))if(v->id!=target)ids.push_back(v->id);s->batchClose(ids);break;}
                case 15:s->open();break;
                case 16:s->settings.dark=arg(0).boolean();s->changed();break;
                case 17:booleanResult(s->setIndependent((unsigned long long)arg(0).number(),arg(1).boolean()));break;
                case 18:booleanResult(s->setIndependent(s->activeId,false));break;
                case 19:booleanResult(s->setIndependent(s->activeId,true));break;
                case 20:s->create(true);break;
                case 21:booleanResult(s->newTabGroup((unsigned long long)arg(0).number()));break;
                case 22:booleanResult(s->moveToGroup((unsigned long long)arg(0).number(),(unsigned)arg(1).number()));break;
                case 23:booleanResult(s->setTabWidth(arg(0).number(),arg(1).number()));break;
                case 24:if(result){result->vt=VT_BSTR;result->bstrVal=SysAllocString(s->diagnostics().c_str());}break;
                case 25:booleanResult(s->recover());break;
                case 26:case 27:case 28:case 29:{auto v=s->find(s->activeId);HWND owner=v?v->hwnd:nullptr;if(id==26)s->showDiagnostics(owner);if(id==27)s->exportDiagnostics(owner);if(id==28)s->widthDialog(owner);if(id==29)s->chooseGroup(owner,s->activeId);break;}
                case 30:{POINT point{arg(1).number(),arg(2).number()};booleanResult(s->drop((unsigned long long)arg(0).number(),point));break;}
                case 31:booleanResult(s->setAppearance(arg(0).number(),arg(1).number()));break;
                case 32:booleanResult(s->setViewWidth((unsigned long long)arg(0).number(),arg(1).number()));break;
                case 33:{auto color=arg(0);if(color.v.vt!=VT_BSTR)throw ComError(DISP_E_TYPEMISMATCH,L"Color");booleanResult(s->setBarColor(color.str()));break;}
                case 34:{auto v=s->find(s->activeId);s->colorDialog(v?v->hwnd:nullptr);break;}
                default:return DISP_E_MEMBERNOTFOUND;
            }return S_OK;
        }catch(const ComError& e){s->report(e);return e.hr;}catch(...){log(L"addin.invoke.exception",E_FAIL);return E_FAIL;}
    }
    HRESULT STDMETHODCALLTYPE OnConnection(IDispatch* app,long,IDispatch* instance,SAFEARRAY**)override{
        try{session=std::make_shared<Session>(app);addinInstance=Ptr<IDispatch>(instance);if(instance)put(instance,L"Object",Var(static_cast<IExtensibility*>(this)));session->start();if(pendingFactory)session->bindFactory(pendingFactory.get());log(L"addin.connected");return S_OK;}
        catch(const ComError& e){log(L"addin.connect.failed",e.hr);session.reset();return e.hr;}
        catch(...){log(L"addin.connect.exception",E_FAIL);session.reset();return E_FAIL;}
    }
    HRESULT STDMETHODCALLTYPE OnDisconnection(long,SAFEARRAY**)override{if(session)session->stop();try{if(addinInstance)put(addinInstance.get(),L"Object",Var());}catch(...){}addinInstance.reset();pendingFactory.reset();return S_OK;}
    HRESULT STDMETHODCALLTYPE OnAddInsUpdate(SAFEARRAY**)override{return S_OK;}
    HRESULT STDMETHODCALLTYPE OnStartupComplete(SAFEARRAY**)override{if(session)session->defer();return S_OK;}
    HRESULT STDMETHODCALLTYPE OnBeginShutdown(SAFEARRAY**)override{if(session)session->stop();return S_OK;}
    HRESULT STDMETHODCALLTYPE CTPFactoryAvailable(IDispatch* f)override{pendingFactory=Ptr<IDispatch>(f);if(session)session->bindFactory(f);return S_OK;}
    HRESULT STDMETHODCALLTYPE GetCustomUI(BSTR,BSTR* xml)override{
        if(!xml)return E_POINTER;
        const wchar_t* markup=LR"(<customUI xmlns="http://schemas.microsoft.com/office/2009/07/customui"><ribbon><tabs><tab id="ootTab" label="openOfficetab">
<group id="ootDocs" label="文档标签"><button id="ootNew" label="新建文档" imageMso="FileNew" onAction="NewDocument"/><button id="ootNewIndependent" label="新建为独立窗口" onAction="NewIndependentDocument"/><button id="ootOpen" label="打开文档" imageMso="FileOpen" onAction="OpenDocument"/><button id="ootSave" label="保存本组全部标签" imageMso="FileSave" onAction="SaveAll"/></group>
<group id="ootWindows" label="标签与窗口"><button id="ootShow" label="显示 / 隐藏标签栏" onAction="ToggleVisible"/><button id="ootGroup" label="窗口跟随模式" onAction="ToggleGroup"/><button id="ootDetach" label="独立窗口显示" onAction="DetachDocument"/><button id="ootJoin" label="加入 / 移动到标签组" onAction="ChooseTabGroup"/><button id="ootRestore" label="恢复窗口排列" onAction="RestoreWindows"/></group>
<group id="ootOptions" label="设置与恢复"><button id="ootWidth" label="标签宽度" onAction="ShowWidth"/><button id="ootSettings" label="标签设置" imageMso="ControlProperties" onAction="ShowSettings"/><button id="ootRefresh" label="刷新标签" imageMso="RefreshAll" onAction="Refresh"/><button id="ootDiagnose" label="加载诊断" onAction="ShowDiagnostics"/><button id="ootExport" label="导出脱敏诊断" onAction="ExportDiagnostics"/><button id="ootRecover" label="恢复标签栏与窗口" onAction="Recover"/></group>
</tab></tabs></ribbon></customUI>)";
        *xml=SysAllocString(markup);return *xml?S_OK:E_OUTOFMEMORY;
    }
};
class Factory final:public IClassFactory {
    ULONG refs=1;CLSID clsid;
public:
    explicit Factory(CLSID c):clsid(c){objects++;}~Factory(){objects--;}
    HRESULT STDMETHODCALLTYPE QueryInterface(REFIID i,void** p)override{if(!p)return E_POINTER;*p=nullptr;if(i==IID_IUnknown||i==IID_IClassFactory){*p=static_cast<IClassFactory*>(this);AddRef();return S_OK;}return E_NOINTERFACE;}
    ULONG STDMETHODCALLTYPE AddRef()override{return ++refs;}ULONG STDMETHODCALLTYPE Release()override{ULONG n=--refs;if(!n)delete this;return n;}
    HRESULT STDMETHODCALLTYPE CreateInstance(IUnknown* outer,REFIID i,void** p)override{if(!p)return E_POINTER;*p=nullptr;if(outer)return CLASS_E_NOAGGREGATION;try{IUnknown* o=clsid==CLSID_Addin?static_cast<IExtensibility*>(new Addin()):static_cast<IDispatch*>(new Tabs());HRESULT hr=o->QueryInterface(i,p);o->Release();return hr;}catch(...){return E_OUTOFMEMORY;}}
    HRESULT STDMETHODCALLTYPE LockServer(BOOL lock)override{if(lock)objects++;else objects--;return S_OK;}
};
std::wstring guid(REFGUID g){wchar_t b[40];StringFromGUID2(g,b,40);return b;}
void regString(const std::wstring& path,const wchar_t* name,const std::wstring& value){HKEY h;LSTATUS e=RegCreateKeyExW(HKEY_CURRENT_USER,path.c_str(),0,nullptr,0,KEY_WRITE,nullptr,&h,nullptr);if(e!=ERROR_SUCCESS)throw ComError(HRESULT_FROM_WIN32(e),L"Registry");e=RegSetValueExW(h,name,0,REG_SZ,(const BYTE*)value.c_str(),(DWORD)((value.size()+1)*2));RegCloseKey(h);if(e!=ERROR_SUCCESS)throw ComError(HRESULT_FROM_WIN32(e),L"Registry");}
void regDword(const std::wstring& path,const wchar_t* name,DWORD value){HKEY h;LSTATUS e=RegCreateKeyExW(HKEY_CURRENT_USER,path.c_str(),0,nullptr,0,KEY_WRITE,nullptr,&h,nullptr);if(e!=ERROR_SUCCESS)throw ComError(HRESULT_FROM_WIN32(e),L"Registry");e=RegSetValueExW(h,name,0,REG_DWORD,(const BYTE*)&value,sizeof(value));RegCloseKey(h);if(e!=ERROR_SUCCESS)throw ComError(HRESULT_FROM_WIN32(e),L"Registry");}
void registerClass(REFCLSID id,const wchar_t* prog,const std::wstring& path,bool control){
    std::wstring cls=L"Software\\Classes\\CLSID\\"+guid(id);
    regString(cls,nullptr,prog);regString(cls+L"\\InprocServer32",nullptr,path);regString(cls+L"\\InprocServer32",L"ThreadingModel",L"Apartment");regString(cls+L"\\ProgID",nullptr,prog);
    regString(std::wstring(L"Software\\Classes\\")+prog,nullptr,prog);regString(std::wstring(L"Software\\Classes\\")+prog+L"\\CLSID",nullptr,guid(id));
    if(control){regString(cls+L"\\Control",nullptr,L"");regString(cls+L"\\MiscStatus\\1",nullptr,std::to_wstring(OLEMISC_SETCLIENTSITEFIRST|OLEMISC_INSIDEOUT|OLEMISC_ACTIVATEWHENVISIBLE|OLEMISC_RECOMPOSEONRESIZE|OLEMISC_NOUIACTIVATE));regString(cls+L"\\Implemented Categories\\{40FC6ED4-2438-11CF-A3DB-080036F12502}",nullptr,L"");regString(cls+L"\\Insertable",nullptr,L"");}
}
}
extern "C" HRESULT WINAPI DllGetClassObject(REFCLSID c,REFIID i,void** p){if(!p)return E_POINTER;*p=nullptr;if(c!=CLSID_Addin&&c!=CLSID_Tabs)return CLASS_E_CLASSNOTAVAILABLE;auto f=new(std::nothrow) Factory(c);if(!f)return E_OUTOFMEMORY;HRESULT hr=f->QueryInterface(i,p);f->Release();return hr;}
extern "C" HRESULT WINAPI DllCanUnloadNow(){return objects==0?S_OK:S_FALSE;}
extern "C" HRESULT WINAPI DllRegisterServer(){
    try{wchar_t file[32768];DWORD n=GetModuleFileNameW(module,file,32768);if(!n||n>=32768)return E_FAIL;registerClass(CLSID_Addin,L"openOfficetab.Connect",file,false);registerClass(CLSID_Tabs,L"openOfficetab.Tabs",file,true);
        for(const auto* host:{L"Word",L"Excel",L"PowerPoint"}){std::wstring path=std::wstring(L"Software\\Microsoft\\Office\\")+host+L"\\Addins\\openOfficetab.Connect";regString(path,L"FriendlyName",L"openOfficetab");regString(path,L"Description",L"永久免费原生文档标签 · Word / Excel / PowerPoint");regDword(path,L"LoadBehavior",3);regDword(path,L"CommandLineSafe",0);}return S_OK;
    }catch(const ComError& e){return e.hr;}catch(...){return E_FAIL;}
}
extern "C" HRESULT WINAPI DllUnregisterServer(){
    LSTATUS failure=ERROR_SUCCESS;auto del=[&](std::wstring path){LSTATUS e=RegDeleteTreeW(HKEY_CURRENT_USER,path.c_str());if(e!=ERROR_SUCCESS&&e!=ERROR_FILE_NOT_FOUND&&e!=ERROR_PATH_NOT_FOUND)failure=e;};
    for(const auto* host:{L"Word",L"Excel",L"PowerPoint"})del(std::wstring(L"Software\\Microsoft\\Office\\")+host+L"\\Addins\\openOfficetab.Connect");
    del(L"Software\\Classes\\openOfficetab.Connect");del(L"Software\\Classes\\openOfficetab.Tabs");del(L"Software\\Classes\\CLSID\\"+guid(CLSID_Addin));del(L"Software\\Classes\\CLSID\\"+guid(CLSID_Tabs));return HRESULT_FROM_WIN32(failure);
}
extern "C" HRESULT WINAPI DllInstall(BOOL install,LPCWSTR){return install?DllRegisterServer():DllUnregisterServer();}
BOOL WINAPI DllMain(HINSTANCE h,DWORD reason,LPVOID){if(reason==DLL_PROCESS_ATTACH){module=h;DisableThreadLibraryCalls(h);}return TRUE;}
