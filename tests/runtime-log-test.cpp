#include "../src/com.h"
#include "../src/diagnostic-trace.h"
#include <filesystem>
#include <cassert>
#include <iostream>
int main(){
    diagnostic::Gate gate;assert(gate.take(0,250));assert(!gate.take(100,250));assert(gate.take(250,250));assert(gate.drain()==1);
    long long parent=0;
    {diagnostic::Scope outer(diagnostic::Tab);parent=runtimelog::traceContext.id;
     {diagnostic::Scope child(diagnostic::Layout,parent,true,42);assert(runtimelog::traceContext.parent==parent);assert(runtimelog::traceContext.job==42);}
     assert(runtimelog::traceContext.id==parent);assert(runtimelog::traceContext.job==0);
    }assert(runtimelog::traceContext.id==0);
    bool propagated=false;try{diagnostic::Scope outer(diagnostic::Tab);diagnostic::perform(diagnostic::ComActivate,nullptr,[]()->bool{throw ComError(E_ACCESSDENIED,L"PRIVATE_TEXT");});}catch(const ComError& error){propagated=error.hr==E_ACCESSDENIED;}assert(propagated);assert(runtimelog::traceContext.id==0);
    auto trace=runtimelog::recorder().snapshot();assert(trace.find(L"window.api.error")!=std::wstring::npos);assert(trace.find(L"PRIVATE_TEXT")==std::wstring::npos);
    assert((runtimelog::officeBuild(L"16.0.20527")==std::make_pair(20527L,-1L)));
    assert((runtimelog::officeBuild(L"16.0.20527.20088")==std::make_pair(20527L,20088L)));
    assert((runtimelog::officeBuild(L"20527")==std::make_pair(20527L,-1L)));
    assert((runtimelog::officeBuild(L"",20527)==std::make_pair(20527L,-1L)));
    assert((runtimelog::officeBuild(L"PRIVATE_NAME")==std::make_pair(-1L,-1L)));

    wchar_t temp[MAX_PATH];GetTempPathW(MAX_PATH,temp);
    auto root=std::filesystem::path(temp)/(L"oot-log-test-"+std::to_wstring(GetCurrentProcessId()));std::filesystem::create_directories(root);
    {
        runtimelog::Recorder recorder;recorder.configure(root.wstring(),1);
        for(auto& file:std::filesystem::directory_iterator(root/L"logs"))assert(file.file_size()==0);
        recorder.emit(L"window.drag.begin",S_OK,{{"viewId",12},{"hwnd",128},{"x",-100}});
        // A hot callback queues a record without touching the file.
        for(auto& file:std::filesystem::directory_iterator(root/L"logs"))assert(file.file_size()==0);
        recorder.flush();assert(recorder.snapshot().find(L"window.drag.begin")!=std::wstring::npos);
        for(int i=0;i<70000;i++){
            recorder.emit(L"selection.observed",E_PENDING,{{"comId",1},{"nativeId",2},{"agreement",0}});
            if(i%200==0)recorder.flush();
        }
        recorder.flush();size_t files=0;for(auto& file:std::filesystem::directory_iterator(root/L"logs")){files++;assert(file.file_size()<=1024*1024);}assert(files<=4);
        assert(recorder.snapshot().size()<130*1024);
        for(int i=0;i<3000;i++)recorder.emit(L"office.event",S_OK,{{"dispid",1}});
        assert(recorder.lost()>0);recorder.flush();
        recorder.emit(L"session.stopped",S_OK,{});recorder.flush();assert(recorder.snapshot().find(L"session.stopped")!=std::wstring::npos);
    }
    std::filesystem::remove_all(root);std::cout<<"Runtime log buffering, bounds, rotation, overload recovery passed\n";
}
