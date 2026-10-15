#include "../src/model.h"
#include "../src/tab-color.h"
#include <cassert>
#include <iostream>

class Dispatch final:public IDispatch {
    ULONG refs=1;
    std::map<std::wstring,std::pair<DISPID,std::function<Var(DISPPARAMS*)>>> members;
public:
    void add(const wchar_t* name,std::function<Var(DISPPARAMS*)> f){members[name]={(DISPID)members.size()+1,std::move(f)};}
    HRESULT STDMETHODCALLTYPE QueryInterface(REFIID id,void** out)override{if(!out)return E_POINTER;*out=nullptr;if(id!=IID_IUnknown&&id!=IID_IDispatch)return E_NOINTERFACE;*out=this;AddRef();return S_OK;}
    ULONG STDMETHODCALLTYPE AddRef()override{return ++refs;}ULONG STDMETHODCALLTYPE Release()override{auto n=--refs;if(!n)delete this;return n;}
    DISPATCH_STUBS
    HRESULT STDMETHODCALLTYPE GetIDsOfNames(REFIID,LPOLESTR* names,UINT,LCID,DISPID* ids)override{auto it=members.find(*names);if(it==members.end())return DISP_E_UNKNOWNNAME;*ids=it->second.first;return S_OK;}
    HRESULT STDMETHODCALLTYPE Invoke(DISPID id,REFIID,LCID,WORD,DISPPARAMS* args,VARIANT* result,EXCEPINFO*,UINT*)override{for(auto& entry:members)if(entry.second.first==id){auto value=entry.second.second(args);if(result)VariantCopy(result,&value.v);return S_OK;}return DISP_E_MEMBERNOTFOUND;}
};
Ptr<Dispatch> dispatch(){return Ptr<Dispatch>(new Dispatch(),false);}

int wmain(int argc,wchar_t** argv){
    if(argc==4){storage::Store store(argv[2]);for(int i=0;i<60;i++){bool ok=false;for(int retry=0;retry<1000&&!ok;retry++){ok=store.update(L"race.ini",[&](storage::Ini& ini){ini[L"writer"][argv[3]]=std::to_wstring(i);});if(!ok)Sleep(1);}if(!ok)return 2;}return 0;}
    CoInitializeEx(nullptr,COINIT_APARTMENTTHREADED);
    wchar_t current[32768];GetCurrentDirectoryW(32768,current);std::wstring root=std::wstring(current)+L"\\persistence-test-"+std::to_wstring(GetCurrentProcessId());CreateDirectoryW(root.c_str(),nullptr);storage::Store store(root);
    Settings a,b;a.root=b.root=root;assert(a.load()&&b.load());
    a.barStyle=0;assert(a.save());b.barColor=RGB(18,171,205);b.customBarColor=true;assert(b.save());
    Settings merged;merged.root=root;assert(merged.load()&&merged.barStyle==0&&merged.barColor==RGB(18,171,205));
    assert(a.synchronize()&&a.barColor==RGB(18,171,205));assert(b.synchronize()&&b.barStyle==0);
    a.tabWidth=180;assert(a.save());b.dark=true;assert(b.save());assert(merged.synchronize()&&merged.tabWidth==180&&merged.dark);
    auto file=L"C:\\Public\\报告=[第一版];.docx";
    assert(storage::decode(storage::encode(file))==file);assert(storage::decode(L"00G0").empty());
    assert(store.savePreference(L"Word",file,{222,true}));storage::FilePreference pref;
    assert(store.preference(L"Word",file,pref)&&pref.width==222&&pref.independent);
    assert(store.preference(L"Word",L"c:\\public\\报告=[第一版];.DOCX",pref));
    assert(!store.preference(L"Excel",file,pref)&&!store.savePreference(L"Word",file,{79,false}));
    assert(!store.savePreference(L"Word",L"https://example.com/file.docx",{120,false}));
    assert(store.resetPreference(L"Word",file)&&!store.preference(L"Word",file,pref));
    for(int i=0;i<25;i++)assert(store.closed(L"Word",L"C:\\Public\\file"+std::to_wstring(i)+L".docx"));
    assert(store.recent(L"Word").size()==20&&store.recent(L"Excel").empty());
    assert(store.closed(L"Word",L"c:\\public\\FILE24.docx")&&store.recent(L"Word").size()==20);
    assert(!store.closed(L"Word",L"Document1")&&!store.closed(L"Word",L"https://example.com/doc"));assert(store.clearRecent(L"Word")&&store.recent(L"Word").empty());
    wchar_t executable[32768];GetModuleFileNameW(nullptr,executable,32768);PROCESS_INFORMATION processes[2]{};
    for(int i=0;i<2;i++){std::wstring command=L"\""+std::wstring(executable)+L"\" child \""+root+L"\" "+std::to_wstring(i);STARTUPINFOW startup{sizeof(startup)};assert(CreateProcessW(nullptr,command.data(),nullptr,nullptr,FALSE,CREATE_NO_WINDOW,nullptr,nullptr,&startup,&processes[i]));}
    for(auto& process:processes){assert(WaitForSingleObject(process.hProcess,15000)==WAIT_OBJECT_0);DWORD code=1;GetExitCodeProcess(process.hProcess,&code);assert(code==0);CloseHandle(process.hThread);CloseHandle(process.hProcess);}
    auto race=store.read(L"race.ini");assert(race[L"writer"][L"0"]==L"59"&&race[L"writer"][L"1"]==L"59");
    auto app=dispatch(),document=dispatch(),window=dispatch(),documents=dispatch(),windows=dispatch(),protectedViews=dispatch();
    bool open=true,visible=true,cancel=true,quit=false;
    HWND native=CreateWindowExW(0,L"STATIC",L"",WS_OVERLAPPEDWINDOW,10,10,800,500,nullptr,nullptr,module,nullptr);
    assert(native);
    app->add(L"Name",[](DISPPARAMS*){return Var(std::wstring(L"Word"));});app->add(L"Documents",[&](DISPPARAMS*){return Var(documents.get());});app->add(L"Windows",[&](DISPPARAMS*){return Var(windows.get());});app->add(L"ActiveWindow",[&](DISPPARAMS*){return open&&visible?Var(window.get()):Var();});app->add(L"ProtectedViewWindows",[&](DISPPARAMS*){return Var(protectedViews.get());});app->add(L"Quit",[&](DISPPARAMS*){quit=true;return Var();});
    documents->add(L"Count",[&](DISPPARAMS*){return Var(open?1L:0L);});documents->add(L"Item",[&](DISPPARAMS*){return Var(document.get());});
    windows->add(L"Count",[&](DISPPARAMS*){return Var(open&&visible?1L:0L);});windows->add(L"Item",[&](DISPPARAMS*){return Var(window.get());});protectedViews->add(L"Count",[](DISPPARAMS*){return Var(0L);});
    window->add(L"Hwnd",[&](DISPPARAMS*){return Var((long)(DWORD)(UINT_PTR)native);});window->add(L"Document",[&](DISPPARAMS*){return Var(document.get());});window->add(L"Caption",[](DISPPARAMS*){return Var(std::wstring(L"Example"));});window->add(L"Close",[&](DISPPARAMS*){if(!cancel)open=false;return Var();});
    document->add(L"Name",[](DISPPARAMS*){return Var(std::wstring(L"Example.docx"));});document->add(L"Path",[](DISPPARAMS*){return Var(std::wstring(L"C:\\Public"));});document->add(L"FullName",[&](DISPPARAMS*){return Var(std::wstring(file));});document->add(L"Saved",[](DISPPARAMS*){return Var(true);});document->add(L"ReadOnly",[](DISPPARAMS*){return Var(false);});
    auto session=std::make_shared<Session>(app.get(),root);assert(session->refresh()&&session->views.size()==1);auto id=session->views[0]->id;
    assert(session->setViewWidth(id,222));assert(session->setTabWidth(120,0)&&session->find(id)->manualTabWidth==222);
    assert(!session->close(id)&&session->recentlyClosed().empty()&&session->find(id)); // cancelled close
    visible=false;assert(session->refresh()&&session->views.empty()&&session->recentlyClosed().empty()); // hidden view, document still open
    visible=true;assert(session->refresh());id=session->views[0]->id;assert(session->views[0]->manualTabWidth==222);
    cancel=false;assert(session->close(id)&&session->recentlyClosed().size()==1&&quit); // confirmed last document
    View firstSave;firstSave.path=L"C:\\Public\\new-name.docx";firstSave.manualTabWidth=176;firstSave.independent=true;session->initializePreference(firstSave,false,L"");
    View reopened;reopened.path=firstSave.path;session->initializePreference(reopened,true,L"");assert(reopened.manualTabWidth==176&&reopened.independent&&reopened.groupId==0);
    session.reset();DestroyWindow(native);
    for(auto name:{L"settings.ini",L"documents.ini",L"race.ini"})DeleteFileW(store.path(name).c_str());RemoveDirectoryW(root.c_str());CoUninitialize();
    std::cout<<"Merged settings, two-process writes, bounded recent files, cancel/hidden close, per-file preferences and first-save inheritance passed.\n";
}
