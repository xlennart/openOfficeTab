// No Office installation required: real Win32 files and mock Office IDispatch.
#include "../src/model.h"
#include <cassert>
#include <iostream>
class Dispatch final:public IDispatch {
    ULONG refs=1;
    std::map<std::wstring,std::pair<DISPID,std::function<Var(DISPPARAMS*)>>> members;
public:
    bool rejectSave=false;
    void add(const wchar_t* name,std::function<Var(DISPPARAMS*)> f){members[name]={(DISPID)members.size()+1,std::move(f)};}
    HRESULT STDMETHODCALLTYPE QueryInterface(REFIID id,void** out)override{if(!out)return E_POINTER;*out=nullptr;if(id!=IID_IUnknown&&id!=IID_IDispatch)return E_NOINTERFACE;*out=this;AddRef();return S_OK;}
    ULONG STDMETHODCALLTYPE AddRef()override{return ++refs;}ULONG STDMETHODCALLTYPE Release()override{auto n=--refs;if(!n)delete this;return n;}
    DISPATCH_STUBS
    HRESULT STDMETHODCALLTYPE GetIDsOfNames(REFIID,LPOLESTR* names,UINT,LCID,DISPID* ids)override{auto it=members.find(*names);if(it==members.end())return DISP_E_UNKNOWNNAME;*ids=it->second.first;return S_OK;}
    HRESULT STDMETHODCALLTYPE Invoke(DISPID id,REFIID,LCID,WORD,DISPPARAMS* args,VARIANT* result,EXCEPINFO*,UINT*)override{
        for(auto& entry:members)if(entry.second.first==id){
            if(rejectSave&&(entry.first==L"SaveAs"||entry.first==L"SaveAs2"))return DISP_E_EXCEPTION;
            auto value=entry.second.second(args);if(result)VariantCopy(result,&value.v);return S_OK;
        }
        return DISP_E_MEMBERNOTFOUND;
    }
};
Ptr<Dispatch> dispatch(){return Ptr<Dispatch>(new Dispatch(),false);}
bool exists(const std::wstring& path){return GetFileAttributesW(path.c_str())!=INVALID_FILE_ATTRIBUTES;}
void write(const std::wstring& path){
    HANDLE f=CreateFileW(path.c_str(),GENERIC_WRITE,0,nullptr,CREATE_NEW,FILE_ATTRIBUTE_NORMAL,nullptr);assert(f!=INVALID_HANDLE_VALUE);
    DWORD written=0;assert(WriteFile(f,"fixture",7,&written,nullptr)&&written==7);CloseHandle(f);
}
int main(){
    assert(SUCCEEDED(CoInitializeEx(nullptr,COINIT_APARTMENTTHREADED)));
    wchar_t cwd[32768];GetCurrentDirectoryW(32768,cwd);
    for(auto host:{Host::Word,Host::Excel,Host::PowerPoint}){
        auto root=std::wstring(cwd)+L"\\rename-session-"+std::to_wstring(GetCurrentProcessId())+L"-"+std::to_wstring((int)host);assert(CreateDirectoryW(root.c_str(),nullptr));
        auto suffix=host==Host::Word?L".docm":host==Host::Excel?L".xlsm":L".pptm";
        auto original=root+L"\\original"+suffix;write(original);
        std::wstring path=original;bool readOnly=false,saved=false,cancelSave=false,wrongPath=false;int saves=0;
        auto app=dispatch(),document=dispatch(),window=dispatch(),docs=dispatch(),windows=dispatch(),protectedViews=dispatch();
        HWND native=CreateWindowExW(0,L"STATIC",L"",WS_OVERLAPPEDWINDOW,0,0,800,500,nullptr,nullptr,module,nullptr);assert(native);
        app->add(L"Name",[&](DISPPARAMS*){return Var(std::wstring(host==Host::Word?L"Word":host==Host::Excel?L"Excel":L"PowerPoint"));});
        app->add(L"Ready",[](DISPPARAMS*){return Var(true);});
        app->add(host==Host::Word?L"Documents":host==Host::Excel?L"Workbooks":L"Presentations",[&](DISPPARAMS*){return Var(docs.get());});
        app->add(L"Windows",[&](DISPPARAMS*){return Var(windows.get());});
        app->add(L"ActiveWindow",[&](DISPPARAMS*){return Var(window.get());});
        app->add(L"ProtectedViewWindows",[&](DISPPARAMS*){return Var(protectedViews.get());});
        docs->add(L"Count",[](DISPPARAMS*){return Var(1L);});docs->add(L"Item",[&](DISPPARAMS*){return Var(document.get());});
        windows->add(L"Count",[](DISPPARAMS*){return Var(1L);});windows->add(L"Item",[&](DISPPARAMS*){return Var(window.get());});
        protectedViews->add(L"Count",[](DISPPARAMS*){return Var(0L);});
        window->add(L"Hwnd",[&](DISPPARAMS*){return Var((long)(DWORD)(UINT_PTR)native);});
        window->add(host==Host::PowerPoint?L"Presentation":L"Document",[&](DISPPARAMS*){return Var(document.get());});
        window->add(L"Visible",[](DISPPARAMS*){return Var(true);});window->add(L"Activate",[](DISPPARAMS*){return Var();});
        document->add(L"Windows",[&](DISPPARAMS*){return Var(windows.get());});
        document->add(L"Name",[&](DISPPARAMS*){return Var(documentrename::leaf(path));});
        document->add(L"Path",[&](DISPPARAMS*){return Var(root);});document->add(L"FullName",[&](DISPPARAMS*){return Var(path);});
        document->add(L"Saved",[&](DISPPARAMS*){return Var(saved);});document->add(L"ReadOnly",[&](DISPPARAMS*){return Var(readOnly);});
        document->add(host==Host::Word?L"SaveFormat":L"FileFormat",[](DISPPARAMS*){return Var(13L);});
        document->add(host==Host::Word?L"SaveAs2":L"SaveAs",[&](DISPPARAMS* args){
            assert(args->cArgs==2&&args->rgvarg[0].vt==VT_I4&&args->rgvarg[0].lVal==13);
            ++saves;if(cancelSave)return Var();
            auto destination=std::wstring(args->rgvarg[1].bstrVal);assert(CopyFileW(path.c_str(),destination.c_str(),TRUE));
            if(!wrongPath)path=destination;saved=true;return Var();
        });
        auto session=std::make_shared<Session>(app.get(),root);session->settings.group=false;
        auto v=std::make_shared<View>();v->id=1;v->hwnd=native;v->document=Ptr<IDispatch>(document.get());v->window=Ptr<IDispatch>(window.get());v->identity=window.as<IUnknown>();v->path=original;v->manualTabWidth=196;v->locked=true;
        session->views.push_back(v);session->activeId=1;assert(session->refresh());
        using documentrename::Result;
        assert(session->renameFile(1,L"original")==Result::renamed&&saves==0&&!saved);
        assert(session->renameFile(1,L"ORIGINAL")==Result::failed&&exists(original)&&saves==0);
        assert(session->renameFile(1,L"../escape")==Result::failed&&saves==0);
        auto collision=root+L"\\collision"+suffix;write(collision);
        assert(session->renameFile(1,L"collision")==Result::failed&&saves==0&&exists(collision));
        session->find(1)->readOnly=true;assert(session->renameFile(1,L"new")==Result::failed&&saves==0);session->find(1)->readOnly=false;
        readOnly=true;assert(session->renameFile(1,L"new")==Result::failed&&saves==0);readOnly=false;
        session->executing=true;assert(session->renameFile(1,L"new")==Result::failed);session->executing=false;
        document->rejectSave=true;assert(session->renameFile(1,L"new")==Result::failed&&exists(original));document->rejectSave=false;
        cancelSave=true;assert(session->renameFile(1,L"new")==Result::failed&&exists(original));cancelSave=false;
        wrongPath=true;assert(session->renameFile(1,L"unverified")==Result::failed&&exists(original));wrongPath=false;
        saved=false;
        assert(session->renameFile(1,L"renamed")==Result::renamed&&!exists(original)&&exists(path));
        assert(session->find(1)->path==path&&session->find(1)->saved&&session->find(1)->manualTabWidth==196&&session->find(1)->locked);
        storage::FilePreference pref;assert(storage::Store(root).preference(host==Host::Word?L"Word":host==Host::Excel?L"Excel":L"PowerPoint",path,pref)&&pref.width==196);
        // Simulate a second reader that forbids deleting the old file.
        auto previous=path;HANDLE reader=CreateFileW(previous.c_str(),GENERIC_READ,FILE_SHARE_READ|FILE_SHARE_WRITE,nullptr,OPEN_EXISTING,FILE_ATTRIBUTE_NORMAL,nullptr);assert(reader!=INVALID_HANDLE_VALUE);
        assert(session->renameFile(1,L"retained")==Result::originalRetained&&exists(previous)&&exists(path));CloseHandle(reader);
        assert(session->recentlyClosed().empty()); // Renaming is not closing.
        session.reset();DestroyWindow(native);
        for(auto name:{L"collision",L"unverified",L"renamed",L"retained"})DeleteFileW((root+L"\\"+name+suffix).c_str());
        for(auto name:{L"settings.ini",L"documents.ini"})DeleteFileW((root+L"\\"+name).c_str());
        assert(RemoveDirectoryW(root.c_str()));
    }
    CoUninitialize();std::cout<<"Three-host rename commands, real files, save rejection/cancel, collisions, retention and preference inheritance passed.\n";
}
