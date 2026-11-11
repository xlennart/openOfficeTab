// Real Win32 files and OPC packages, with Office represented by IDispatch.
#include "../src/model.h"
#include <msopc.h>
#include <urlmon.h>
#include <cassert>
#include <filesystem>
#include <iostream>

#pragma comment(lib, "urlmon.lib")

class Dispatch final : public IDispatch {
    ULONG refs=1;
    std::map<std::wstring,std::pair<DISPID,std::function<Var(DISPPARAMS*)>>> members;
public:
    std::wstring reject;
    void add(const wchar_t* name,std::function<Var(DISPPARAMS*)> f){members[name]={(DISPID)members.size()+1,std::move(f)};}
    HRESULT STDMETHODCALLTYPE QueryInterface(REFIID id,void** out)override{if(!out)return E_POINTER;*out=nullptr;if(id!=IID_IUnknown&&id!=IID_IDispatch)return E_NOINTERFACE;*out=this;AddRef();return S_OK;}
    ULONG STDMETHODCALLTYPE AddRef()override{return ++refs;}
    ULONG STDMETHODCALLTYPE Release()override{auto n=--refs;if(!n)delete this;return n;}
    DISPATCH_STUBS
    HRESULT STDMETHODCALLTYPE GetIDsOfNames(REFIID,LPOLESTR* names,UINT,LCID,DISPID* ids)override{auto it=members.find(*names);if(it==members.end())return DISP_E_UNKNOWNNAME;*ids=it->second.first;return S_OK;}
    HRESULT STDMETHODCALLTYPE Invoke(DISPID id,REFIID,LCID,WORD,DISPPARAMS* args,VARIANT* result,EXCEPINFO*,UINT*)override{
        for(auto& entry:members)if(entry.second.first==id){
            if(reject==entry.first)return DISP_E_EXCEPTION;
            auto value=entry.second.second(args);if(result)VariantCopy(result,&value.v);return S_OK;
        }
        return DISP_E_MEMBERNOTFOUND;
    }
};

Ptr<Dispatch> dispatch(){return Ptr<Dispatch>(new Dispatch(),false);}
bool exists(const std::wstring& path){return std::filesystem::exists(path);}
void writeFile(const std::wstring& path,const std::string& contents){
    std::ofstream stream(std::filesystem::path(path),std::ios::binary | std::ios::trunc);
    stream << contents;
    assert(stream.good());
}
std::string readFile(const std::wstring& path){
    std::ifstream stream(std::filesystem::path(path),std::ios::binary);
    assert(stream.good());
    return std::string(std::istreambuf_iterator<char>(stream),std::istreambuf_iterator<char>());
}

void writePresentation(const std::wstring& path,bool strict,bool mismatched=false) {
    Ptr<IOpcFactory> factory;
    Ptr<IOpcPackage> package;
    Ptr<IOpcPartSet> parts;
    Ptr<IOpcPartUri> name;
    Ptr<IOpcPart> part;
    Ptr<IOpcRelationshipSet> relationships;
    Ptr<IOpcRelationship> relation;
    Ptr<IUri> target;
    Ptr<IStream> content,archive;
    assert(SUCCEEDED(CoCreateInstance(__uuidof(OpcFactory),nullptr,CLSCTX_INPROC_SERVER,IID_PPV_ARGS(factory.out()))));
    assert(SUCCEEDED(factory->CreatePackage(package.out())));
    assert(SUCCEEDED(package->GetPartSet(parts.out())));
    assert(SUCCEEDED(factory->CreatePartUri(L"/ppt/presentation.xml",name.out())));
    assert(SUCCEEDED(parts->CreatePart(name.get(),
        L"application/vnd.openxmlformats-officedocument.presentationml.presentation.main+xml",
        OPC_COMPRESSION_NORMAL,part.out())));
    assert(SUCCEEDED(part->GetContentStream(content.out())));
    const std::string xmlns=(strict!=mismatched) ? "http://purl.oclc.org/ooxml/presentationml/main" :
        "http://schemas.openxmlformats.org/presentationml/2006/main";
    const auto xml="<p:presentation xmlns:p=\"" + xmlns + "\"/>";
    ULONG written=0;
    assert(SUCCEEDED(content->Write(xml.data(),static_cast<ULONG>(xml.size()),&written))&&written==xml.size());
    assert(SUCCEEDED(package->GetRelationshipSet(relationships.out())));
    assert(SUCCEEDED(CreateUri(L"ppt/presentation.xml",Uri_CREATE_ALLOW_RELATIVE,0,target.out())));
    assert(SUCCEEDED(relationships->CreateRelationship(L"rId1",strict ?
        L"http://purl.oclc.org/ooxml/officeDocument/relationships/officeDocument" :
        L"http://schemas.openxmlformats.org/officeDocument/2006/relationships/officeDocument",
        target.get(),OPC_URI_TARGET_MODE_INTERNAL,relation.out())));
    assert(SUCCEEDED(factory->CreateStreamOnFile(path.c_str(),OPC_STREAM_IO_WRITE,nullptr,FILE_ATTRIBUTE_NORMAL,archive.out())));
    assert(SUCCEEDED(factory->WritePackageToStream(package.get(),OPC_WRITE_DEFAULT,archive.get())));
}

void run(Host host, const wchar_t* suffix, int format, int sequence){
    const auto root=std::filesystem::current_path().wstring()+L"\\rename-session-"+
        std::to_wstring(GetCurrentProcessId())+L"-"+std::to_wstring(sequence);
    assert(CreateDirectoryW(root.c_str(),nullptr));
    const auto file=[&](const wchar_t* stem){return root+L"\\"+stem+suffix;};
    const auto original=file(L"original");
    if(format&&format!=4)writePresentation(original,format==2,format==3);
    else writeFile(original,"original contents");
    const auto originalBytes=readFile(original);
    std::wstring path=original;
    bool open=true,saved=false,readOnly=false,cancelSave=false,cancelClose=false;
    bool collisionOnSave=false,collisionOnClose=false,replaceOnClose=false;
    long windowCount=1;
    int saves=0,closes=0,opens=0;
    auto app=dispatch(),document=dispatch(),window=dispatch(),docs=dispatch(),windows=dispatch();
    HWND native=CreateWindowExW(0,L"STATIC",L"",WS_OVERLAPPEDWINDOW,0,0,800,500,nullptr,nullptr,module,nullptr);
    assert(native);
    app->add(L"Name",[&](DISPPARAMS*){return Var(std::wstring(host==Host::Word?L"Word":host==Host::Excel?L"Excel":L"PowerPoint"));});
    app->add(L"Ready",[](DISPPARAMS*){return Var(true);});
    app->add(host==Host::Word?L"Documents":host==Host::Excel?L"Workbooks":L"Presentations",[&](DISPPARAMS*){return Var(docs.get());});
    app->add(L"Windows",[&](DISPPARAMS*){return Var(windows.get());});
    app->add(L"ActiveWindow",[&](DISPPARAMS*){return Var(window.get());});
    docs->add(L"Count",[&](DISPPARAMS*){return Var(open?1L:0L);});
    docs->add(L"Item",[&](DISPPARAMS*){assert(open);return Var(document.get());});
    docs->add(L"Open",[&](DISPPARAMS* args){
        assert(args->cArgs==1&&args->rgvarg[0].vt==VT_BSTR&&!open);
        path=args->rgvarg[0].bstrVal;
        assert(exists(path));
        ++opens;open=true;return Var(document.get());
    });
    windows->add(L"Count",[&](DISPPARAMS*){return Var(open?windowCount:0L);});
    windows->add(L"Item",[&](DISPPARAMS*){return Var(window.get());});
    window->add(L"Hwnd",[&](DISPPARAMS*){return Var((long)(DWORD)(UINT_PTR)native);});
    window->add(host==Host::PowerPoint?L"Presentation":L"Document",[&](DISPPARAMS*){return Var(document.get());});
    window->add(L"Visible",[](DISPPARAMS*){return Var(true);});
    window->add(L"Activate",[](DISPPARAMS*){return Var();});
    document->add(L"Windows",[&](DISPPARAMS*){return Var(windows.get());});
    document->add(L"Name",[&](DISPPARAMS*){return Var(documentrename::leaf(path));});
    document->add(L"Path",[&](DISPPARAMS*){return Var(root);});
    document->add(L"FullName",[&](DISPPARAMS*){return Var(path);});
    document->add(L"Saved",[&](DISPPARAMS*){return Var(saved);});
    document->add(L"ReadOnly",[&](DISPPARAMS*){return Var(readOnly);});
    document->add(L"Save",[&](DISPPARAMS* args){
        assert(args->cArgs==0&&open&&path==original);
        ++saves;
        if(collisionOnSave)writeFile(file(L"appeared-during-save"),"collision during save");
        if(!cancelSave)saved=true;
        return Var();
    });
    document->add(L"Close",[&](DISPPARAMS* args){
        assert(open&&saved);
        if(host==Host::Word)assert(args->cArgs==1&&args->rgvarg[0].vt==VT_I4&&args->rgvarg[0].lVal==0);
        if(host==Host::Excel)assert(args->cArgs==1&&args->rgvarg[0].vt==VT_BOOL&&!args->rgvarg[0].boolVal);
        if(host==Host::PowerPoint)assert(args->cArgs==0);
        ++closes;
        if(cancelClose)return Var();
        open=false;
        if(collisionOnClose)writeFile(file(L"appeared-after-close"),"collision after close");
        if(replaceOnClose){
            assert(MoveFileW(original.c_str(),file(L"outside-backup").c_str()));
            writeFile(original,"replacement original path");
        }
        return Var();
    });
    auto session=std::make_shared<Session>(app.get(),root);session->settings.group=false;
    auto view=std::make_shared<View>();
    view->id=1;view->hwnd=native;view->document=Ptr<IDispatch>(document.get());view->window=Ptr<IDispatch>(window.get());
    view->identity=window.as<IUnknown>();view->path=original;view->manualTabWidth=196;view->locked=true;
    session->views.push_back(view);session->activeId=1;assert(session->refresh());
    using documentrename::Result;
    if(format==3||format==4){
        assert(session->renameFile(1,L"rejected")==Result::failed&&saves==0&&closes==0&&open);
        assert(readFile(original)==originalBytes&&!exists(file(L"rejected")));
    }else{
        assert(session->renameFile(1,L"original")==Result::renamed&&saves==0&&!saved);
        assert(session->renameFile(1,L"ORIGINAL")==Result::failed&&saves==0);
        assert(session->renameFile(1,L"../escape")==Result::failed&&saves==0);
        readOnly=true;
        assert(session->renameFile(1,L"new")==Result::failed&&saves==0);
        readOnly=false;
        windowCount=2;
        assert(session->renameFile(1,L"new")==Result::failed&&saves==0&&closes==0);
        windowCount=1;
        writeFile(file(L"existing"),"existing target");
        assert(session->renameFile(1,L"existing")==Result::failed&&saves==0&&closes==0);
        assert(readFile(file(L"existing"))=="existing target");
        document->reject=L"Save";
        assert(session->renameFile(1,L"new")==Result::failed&&open&&closes==0);
        document->reject.clear();
        cancelSave=true;
        assert(session->renameFile(1,L"new")==Result::failed&&open&&closes==0);
        cancelSave=false;
        cancelClose=true;
        assert(session->renameFile(1,L"new")==Result::failed&&open&&closes==1&&opens==0);
        cancelClose=false;
        assert(readFile(original)==originalBytes&&!exists(file(L"new")));

        saved=false;
        collisionOnSave=true;
        assert(session->renameFile(1,L"appeared-during-save")==Result::failed&&open&&opens==1&&path==original);
        assert(readFile(original)==originalBytes&&readFile(file(L"appeared-during-save"))=="collision during save");
        collisionOnSave=false;
        collisionOnClose=true;
        assert(session->renameFile(1,L"appeared-after-close")==Result::failed&&open&&opens==2&&path==original);
        assert(readFile(original)==originalBytes&&readFile(file(L"appeared-after-close"))=="collision after close");
        collisionOnClose=false;

        if(sequence==0){
            replaceOnClose=true;
            assert(session->renameFile(1,L"renamed")==Result::failed&&!open&&opens==2);
            assert(!exists(file(L"renamed"))&&readFile(original)=="replacement original path");
            assert(readFile(file(L"outside-backup"))==originalBytes);
        }else if(sequence==1){
            docs->reject=L"Open";
            assert(session->renameFile(1,L"renamed")==Result::reopenFailed&&!open);
            assert(!exists(original)&&readFile(file(L"renamed"))==originalBytes);
        }else{
            assert(session->renameFile(1,L"renamed")==Result::renamed&&open&&path==file(L"renamed"));
            assert(!exists(original)&&readFile(path)==originalBytes);
            assert(session->views.size()==1&&session->views.front()->locked&&session->views.front()->manualTabWidth==196);
            storage::FilePreference pref;
            assert(storage::Store(root).preference(host==Host::PowerPoint?L"PowerPoint":host==Host::Excel?L"Excel":L"Word",path,pref)&&pref.width==196);
            assert(session->recentlyClosed().empty());
        }
    }
    session.reset();
    DestroyWindow(native);
    for(auto name:{L"original",L"renamed",L"existing",L"appeared-during-save",L"appeared-after-close",L"outside-backup"})
        DeleteFileW(file(name).c_str());
    for(auto name:{L"settings.ini",L"documents.ini"})DeleteFileW((root+L"\\"+name).c_str());
    assert(RemoveDirectoryW(root.c_str()));
}

int main(){
    assert(SUCCEEDED(CoInitializeEx(nullptr,COINIT_APARTMENTTHREADED)));
    run(Host::Word,L".docm",0,0);
    run(Host::Excel,L".xlsm",0,1);
    run(Host::Word,L".docx",0,2);
    run(Host::Excel,L".xlsx",0,3);
    run(Host::PowerPoint,L".pptm",0,4);
    run(Host::PowerPoint,L".pptx",1,5);
    run(Host::PowerPoint,L".PPTX",2,6);
    run(Host::PowerPoint,L".pptx",3,7);
    run(Host::PowerPoint,L".pptx",4,8);
    CoUninitialize();
    std::cout<<"Native no-overwrite rename, Office cancellation/reopen, file identity, and PowerPoint package formats passed.\n";
}
