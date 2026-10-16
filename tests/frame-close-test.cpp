#include "../src/model.h"
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
int main(){
    CoInitializeEx(nullptr,COINIT_APARTMENTTHREADED);
    wchar_t cwd[32768];GetCurrentDirectoryW(32768,cwd);std::wstring root=std::wstring(cwd)+L"\\frame-close-test-"+std::to_wstring(GetCurrentProcessId());CreateDirectoryW(root.c_str(),nullptr);
    auto app=dispatch(),docs=dispatch(),windows=dispatch(),protectedViews=dispatch();
    bool opened[4]{true,true,true,true},cancel[4]{};int active=0,prompts=0;bool quit=false;std::vector<int> closed;
    std::vector<Ptr<Dispatch>> document,window;std::vector<HWND> native;
    auto count=[&]{long n=0;for(bool value:opened)if(value)n++;return n;};
    auto index=[&](DISPPARAMS* p){Var arg;VariantCopy(&arg.v,&p->rgvarg[0]);long desired=arg.number();for(int i=0;i<4;i++)if(opened[i]&&--desired==0)return i;return -1;};
    for(int i=0;i<4;i++){
        native.push_back(CreateWindowExW(0,L"STATIC",L"",WS_OVERLAPPEDWINDOW,30,30,800,500,nullptr,nullptr,module,nullptr));assert(native.back());
        document.push_back(dispatch());window.push_back(dispatch());
        document[i]->add(L"Name",[i](DISPPARAMS*){return Var(L"Test "+std::to_wstring(i));});
        document[i]->add(L"Path",[](DISPPARAMS*){return Var(std::wstring());});
        document[i]->add(L"Saved",[](DISPPARAMS*){return Var(true);});
        document[i]->add(L"ReadOnly",[](DISPPARAMS*){return Var(false);});
        window[i]->add(L"Hwnd",[&,i](DISPPARAMS*){return Var((long)(DWORD)(UINT_PTR)native[i]);});
        window[i]->add(L"Document",[&,i](DISPPARAMS*){return Var(document[i].get());});
        window[i]->add(L"Activate",[&,i](DISPPARAMS*){active=i;return Var();});
        window[i]->add(L"Close",[&,i](DISPPARAMS*){closed.push_back(i);if(!cancel[i])opened[i]=false;return Var();});
    }
    app->add(L"Name",[](DISPPARAMS*){return Var(std::wstring(L"Word"));});
    app->add(L"Documents",[&](DISPPARAMS*){return Var(docs.get());});app->add(L"Windows",[&](DISPPARAMS*){return Var(windows.get());});
    app->add(L"ProtectedViewWindows",[&](DISPPARAMS*){return Var(protectedViews.get());});
    app->add(L"ActiveWindow",[&](DISPPARAMS*){if(opened[active])return Var(window[active].get());for(int i=0;i<4;i++)if(opened[i])return Var(window[i].get());return Var();});
    app->add(L"Quit",[&](DISPPARAMS*){quit=true;return Var();});protectedViews->add(L"Count",[](DISPPARAMS*){return Var(0L);});
    docs->add(L"Count",[&](DISPPARAMS*){return Var(count());});windows->add(L"Count",[&](DISPPARAMS*){return Var(count());});
    docs->add(L"Item",[&](DISPPARAMS* p){return Var(document[index(p)].get());});windows->add(L"Item",[&](DISPPARAMS* p){return Var(window[index(p)].get());});
    auto s=std::make_shared<Session>(app.get(),root);assert(s->refresh()&&s->views.size()==4);
    s->settings.group=true;s->settings.frameCloseAction=0;s->settings.save();
    s->views[2]->independent=true;s->views[2]->groupId=0;s->views[3]->groupId=2;
    auto second=s->views[1]->id;s->find(second)->locked=true;
    assert(!s->close(second)&&closed.empty()); // tab lock remains effective
    s->timer=CreateWindowExW(0,L"STATIC",L"",0,0,0,0,0,HWND_MESSAGE,nullptr,module,nullptr);assert(s->timer);
    s->frameClosePrompt=[&](HWND,size_t n){assert(n==2);prompts++;return FrameCloseChoice{0,true};};
    assert(s->queueFrameClose(native[0]));s->processFrameClose();assert(prompts==1&&closed.empty()&&s->settings.frameCloseAction==0);
    s->pendingCloseId=second;assert(!s->queueFrameClose(native[1])&&s->queueFrameClose(native[0]));s->pendingCloseId=0; // allow native asynchronous completion only for the requested target
    s->frameClosePrompt=[&](HWND,size_t){prompts++;return FrameCloseChoice{1,true};};
    assert(s->queueFrameClose(native[0]));s->processFrameClose();assert(!opened[0]&&opened[1]&&opened[2]&&opened[3]&&!quit&&prompts==2);
    Settings reloaded;reloaded.root=root;assert(reloaded.load()&&reloaded.frameCloseAction==1);
    opened[0]=true;assert(s->refresh());s->settings.frameCloseAction=2;s->settings.save();
    cancel[1]=true;closed.clear();assert(s->queueFrameClose(native[0]));s->processFrameClose();
    assert(opened[0]&&opened[1]&&closed.size()==1&&closed[0]==1&&prompts==2&&!s->frameBatch); // save cancellation stops batch
    cancel[1]=false;closed.clear();assert(s->queueFrameClose(native[0]));s->processFrameClose();
    assert(!opened[0]&&!opened[1]&&opened[2]&&opened[3]&&!quit&&closed.size()==2&&prompts==2); // remembered group action, other groups untouched
    s->settings.group=false;assert(!s->queueFrameClose(native[3]));
    s->settings.group=true;for(auto& v:s->views){v->independent=false;v->groupId=1;}
    assert(s->queueFrameClose(native[2]));s->processFrameClose();assert(count()==0&&quit); // closing the last group exits the host
    s.reset();for(auto h:native)DestroyWindow(h);DeleteFileW((root+L"\\settings.ini").c_str());RemoveDirectoryW(root.c_str());CoUninitialize();
    std::cout<<"Frame close cancel/current/group, remembered choice, save cancellation, locked tabs and independent/group boundaries passed.\n";
}
