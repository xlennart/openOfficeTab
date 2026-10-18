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
        const wchar_t* api[]={L"Refresh",L"GetState",L"Activate",L"Close",L"Save",L"NewDocument",L"ToggleGroup",L"ToggleVisible",L"RestoreWindows",L"ShowSettings",L"SetLocked",L"Reorder",L"SaveAll",L"CloseOther",L"OpenDocument",L"SetDark",L"SetIndependent",L"JoinTabs",L"DetachDocument",L"NewIndependentDocument",L"NewTabGroup",L"MoveToGroup",L"SetTabWidth",L"GetDiagnostics",L"Recover",L"ShowDiagnostics",L"ExportDiagnostics",L"ShowWidth",L"ChooseTabGroup",L"DropTab",L"SetAppearance",L"SetViewWidth",L"SetBarColor",L"ShowColor",L"GetRecentlyClosed",L"ReopenClosed",L"ClearRecentlyClosed",L"ResetFileSettings",L"ResetAllFileSettings",L"SyncSettings",L"ShowRecentlyClosed",L"SetThemeMode",L"SetFrameCloseAction",L"SetLanguage",L"GetLabel",L"RibbonLoaded"};
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
                case 9:s->restore();s->changed();break;
                case 10:{POINT p;GetCursorPos(&p);auto v=s->find(s->activeId);s->options(v?v->hwnd:nullptr,p);break;}
                case 11:{auto v=s->find((unsigned long long)arg(0).number());if(v){v->locked=arg(1).boolean();s->redraw();}break;}
                case 12:s->reorder((unsigned long long)arg(0).number(),(unsigned long long)arg(1).number());break;
                case 13:s->saveAll();break;
                case 14:{auto target=(unsigned long long)arg(0).number();auto selected=s->find(target);if(!selected)break;std::vector<unsigned long long> ids;for(auto& v:s->tabViews(selected->hwnd))if(v->id!=target)ids.push_back(v->id);s->batchClose(ids);break;}
                case 15:s->open();break;
                case 16:booleanResult(s->setThemeMode(arg(0).boolean()?2:1));break;
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
                case 35:if(result){std::wstring value=L"[";auto files=s->recentlyClosed();for(size_t i=0;i<files.size();i++){if(i)value+=L",";value+=json(files[i]);}value+=L"]";result->vt=VT_BSTR;result->bstrVal=SysAllocString(value.c_str());}break;
                case 36:{long index=arg(0).number(-1);booleanResult(index>=0&&s->reopenClosed((unsigned)index));break;}
                case 37:booleanResult(s->clearRecentlyClosed());break;
                case 38:booleanResult(s->resetFilePreferences((unsigned long long)arg(0).number()));break;
                case 39:booleanResult(s->resetAllFilePreferences());break;
                case 40:s->lastSettingsPoll=0;s->pollPersistence();booleanResult(s->refresh());break;
                case 41:{POINT point{};GetCursorPos(&point);auto view=s->find(s->activeId);s->recentMenu(view?view->hwnd:nullptr,point);break;}
                case 42:booleanResult(s->setThemeMode(arg(0).number()));break;
                case 43:booleanResult(s->setFrameCloseAction(arg(0).number()));break;
                case 44:booleanResult(s->setLanguage(arg(0).number()));break;
                case 45:{auto control=arg(0).disp();auto label=i18n::ribbonLabel(s->language(),prop(control.get(),L"Id").str());if(result){result->vt=VT_BSTR;result->bstrVal=SysAllocString(label);}break;}
                case 46:s->ribbon=arg(0).disp();s->lastLanguage=0;s->refreshLanguage();break;
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
        const wchar_t* markup=LR"(<customUI onLoad="RibbonLoaded" xmlns="http://schemas.microsoft.com/office/2009/07/customui"><ribbon><tabs><tab id="ootTab" label="openOfficetab">
<group id="ootDocs" getLabel="GetLabel"><button id="ootNew" getLabel="GetLabel" imageMso="FileNew" onAction="NewDocument"/><button id="ootNewIndependent" getLabel="GetLabel" onAction="NewIndependentDocument"/><button id="ootOpen" getLabel="GetLabel" imageMso="FileOpen" onAction="OpenDocument"/><button id="ootSave" getLabel="GetLabel" imageMso="FileSave" onAction="SaveAll"/><button id="ootRecent" getLabel="GetLabel" onAction="ShowRecentlyClosed"/></group>
<group id="ootWindows" getLabel="GetLabel"><button id="ootShow" getLabel="GetLabel" onAction="ToggleVisible"/><button id="ootGroup" getLabel="GetLabel" onAction="ToggleGroup"/><button id="ootDetach" getLabel="GetLabel" onAction="DetachDocument"/><button id="ootJoin" getLabel="GetLabel" onAction="ChooseTabGroup"/><button id="ootRestore" getLabel="GetLabel" onAction="RestoreWindows"/></group>
<group id="ootOptions" getLabel="GetLabel"><button id="ootWidth" getLabel="GetLabel" onAction="ShowWidth"/><button id="ootSettings" getLabel="GetLabel" imageMso="ControlProperties" onAction="ShowSettings"/><button id="ootRefresh" getLabel="GetLabel" imageMso="RefreshAll" onAction="Refresh"/><button id="ootDiagnose" getLabel="GetLabel" onAction="ShowDiagnostics"/><button id="ootExport" getLabel="GetLabel" onAction="ExportDiagnostics"/><button id="ootRecover" getLabel="GetLabel" onAction="Recover"/></group>
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
        for(const auto* host:{L"Word",L"Excel",L"PowerPoint"}){std::wstring path=std::wstring(L"Software\\Microsoft\\Office\\")+host+L"\\Addins\\openOfficetab.Connect";regString(path,L"FriendlyName",L"openOfficetab");regString(path,L"Description",i18n::text(0,L"永久免费原生文档标签 · Word / Excel / PowerPoint"));regDword(path,L"LoadBehavior",3);regDword(path,L"CommandLineSafe",0);}return S_OK;
    }catch(const ComError& e){return e.hr;}catch(...){return E_FAIL;}
}
extern "C" HRESULT WINAPI DllUnregisterServer(){
    LSTATUS failure=ERROR_SUCCESS;auto del=[&](std::wstring path){LSTATUS e=RegDeleteTreeW(HKEY_CURRENT_USER,path.c_str());if(e!=ERROR_SUCCESS&&e!=ERROR_FILE_NOT_FOUND&&e!=ERROR_PATH_NOT_FOUND)failure=e;};
    for(const auto* host:{L"Word",L"Excel",L"PowerPoint"})del(std::wstring(L"Software\\Microsoft\\Office\\")+host+L"\\Addins\\openOfficetab.Connect");
    del(L"Software\\Classes\\openOfficetab.Connect");del(L"Software\\Classes\\openOfficetab.Tabs");del(L"Software\\Classes\\CLSID\\"+guid(CLSID_Addin));del(L"Software\\Classes\\CLSID\\"+guid(CLSID_Tabs));return HRESULT_FROM_WIN32(failure);
}
extern "C" HRESULT WINAPI DllInstall(BOOL install,LPCWSTR){return install?DllRegisterServer():DllUnregisterServer();}
BOOL WINAPI DllMain(HINSTANCE h,DWORD reason,LPVOID){if(reason==DLL_PROCESS_ATTACH){module=h;DisableThreadLibraryCalls(h);}return TRUE;}
