#include "../src/model.h"
#include <cassert>
#include <iostream>
#include <set>

extern "C" HRESULT WINAPI DllGetClassObject(REFCLSID,REFIID,void**);
class LanguageDispatch final:public IDispatch {
    ULONG refs=1;
public:
    int invalidations=0;
    HRESULT STDMETHODCALLTYPE QueryInterface(REFIID id,void** out)override{if(!out)return E_POINTER;*out=nullptr;if(id!=IID_IUnknown&&id!=IID_IDispatch)return E_NOINTERFACE;*out=this;AddRef();return S_OK;}
    ULONG STDMETHODCALLTYPE AddRef()override{return ++refs;}ULONG STDMETHODCALLTYPE Release()override{auto n=--refs;if(!n)delete this;return n;}
    DISPATCH_STUBS
    HRESULT STDMETHODCALLTYPE GetIDsOfNames(REFIID,LPOLESTR* names,UINT,LCID,DISPID* ids)override{if(!wcscmp(*names,L"Name")){*ids=1;return S_OK;}if(!wcscmp(*names,L"Invalidate")){*ids=2;return S_OK;}return DISP_E_UNKNOWNNAME;}
    HRESULT STDMETHODCALLTYPE Invoke(DISPID id,REFIID,LCID,WORD,DISPPARAMS*,VARIANT* result,EXCEPINFO*,UINT*)override{
        if(id==1){if(result){VariantInit(result);result->vt=VT_BSTR;result->bstrVal=SysAllocString(L"Word");}return S_OK;}
        if(id==2){invalidations++;return S_OK;}return DISP_E_MEMBERNOTFOUND;
    }
};
int wmain(){
    assert(SUCCEEDED(CoInitializeEx(nullptr,COINIT_APARTMENTTHREADED)));
    assert(i18n::resolve(0,0x0804)==1&&i18n::resolve(0,0x0404)==1&&i18n::resolve(0,0x0c04)==1);
    assert(i18n::resolve(0,0x0409)==2&&i18n::resolve(0,0x0809)==2&&i18n::resolve(0,0x0407)==2);
    assert(i18n::resolve(1,0x0409)==1&&i18n::resolve(2,0x0804)==2&&!i18n::valid(3)&&!i18n::valid(-1));
    std::set<std::wstring> keys;
    for(const auto& entry:i18n::entries){assert(keys.insert(entry.zh).second);assert(*entry.zh&&*entry.en);assert(!wcscmp(i18n::text(1,entry.zh),entry.zh));assert(!wcscmp(i18n::text(2,entry.zh),entry.en));}
    assert(!wcscmp(i18n::text(2,L"Unknown product name"),L"Unknown product name"));
    assert(i18n::groupLabel(2,3,12)==L"Group 3 (12 windows)");
    assert(i18n::closeSummary(2,2)==L"This tab group contains 2 documents.");
    assert(i18n::groupLabel(1,3,12)==L"标签组 3（12 个窗口）");
    for(const auto& entry:i18n::ribbonLabels){assert(keys.count(entry.key));assert(wcscmp(i18n::ribbonLabel(2,entry.id),entry.key));}
    Ptr<IClassFactory> factory;assert(SUCCEEDED(DllGetClassObject(CLSID_Addin,IID_IClassFactory,(void**)factory.out())));
    Ptr<IRibbon> ribbon;assert(SUCCEEDED(factory->CreateInstance(nullptr,__uuidof(IRibbon),(void**)ribbon.out())));
    BSTR xml=nullptr;assert(SUCCEEDED(ribbon->GetCustomUI(nullptr,&xml)));std::wstring markup(xml);SysFreeString(xml);
    assert(markup.find(L"onLoad=\"RibbonLoaded\"")!=std::wstring::npos);
    for(const auto& entry:i18n::ribbonLabels){auto control=L"id=\""+std::wstring(entry.id)+L"\" getLabel=\"GetLabel\"";assert(markup.find(control)!=std::wstring::npos);}
    auto api=ribbon.as<IDispatch>();for(auto entry:{std::pair<const wchar_t*,DISPID>{L"SetLanguage",44},{L"GetLabel",45},{L"RibbonLoaded",46}}){LPOLESTR name=const_cast<LPOLESTR>(entry.first);DISPID id=0;assert(SUCCEEDED(api->GetIDsOfNames(IID_NULL,&name,1,0,&id))&&id==entry.second);}
    wchar_t current[32768];GetCurrentDirectoryW(32768,current);std::wstring root=std::wstring(current)+L"\\localization-test-"+std::to_wstring(GetCurrentProcessId());CreateDirectoryW(root.c_str(),nullptr);
    Settings a,b;a.root=b.root=root;assert(a.load()&&b.load()&&a.language==0);
    a.language=2;assert(a.save());b.tabWidth=177;assert(b.save());assert(b.synchronize()&&b.language==2);
    Settings reopened;reopened.root=root;assert(reopened.load()&&reopened.language==2&&reopened.tabWidth==177);
    storage::Store store(root);assert(store.update(L"settings.ini",[](storage::Ini& ini){ini[L"openOfficetab"][L"language"]=L"99";}));assert(reopened.load()&&reopened.language==0);
    Ptr<LanguageDispatch> app(new LanguageDispatch(),false),ui(new LanguageDispatch(),false);
    auto session=std::make_shared<Session>(app.get(),root);session->ribbon=Ptr<IDispatch>(ui.get());
    auto view=std::make_shared<View>();view->id=8;view->title=L"授课计划.xlsx";view->manualTabWidth=211;session->views.push_back(view);
    assert(session->setLanguage(2)&&session->language()==2&&ui->invalidations==1);
    assert(!wcscmp(session->tr(L"保存"),L"Save")&&session->views[0]==view&&view->title==L"授课计划.xlsx"&&view->manualTabWidth==211);
    assert(session->setLanguage(1)&&session->language()==1&&ui->invalidations==2);
    assert(!session->setLanguage(3)&&session->settings.language==1&&ui->invalidations==2);
    Settings remote;remote.root=root;assert(remote.load());remote.language=2;assert(remote.save());session->lastSettingsPoll=0;session->pollPersistence();assert(session->language()==2&&ui->invalidations==3);
    assert(session->setLanguage(0)&&session->settings.language==0);
    session.reset();assert(reopened.load()&&reopened.language==0);DeleteFileW(store.path(L"settings.ini").c_str());assert(RemoveDirectoryW(root.c_str()));
    ribbon.reset();api.reset();factory.reset();app.reset();ui.reset();CoUninitialize();
    std::cout<<"Language fallback, translation coverage, Ribbon labels/API, live invalidation, persistence and stale-writer merge passed.\n";
}
