#include "../src/com.h"
#include <filesystem>
#include <cassert>
#include <iostream>
int main(){
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
